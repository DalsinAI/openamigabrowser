/*
 * OpenBrowser Lite semantic HTML adapter.
 * Derived in approach from OpenMail's small MIT HTML-to-text tokenizer,
 * but emits an OpenLayout tree rather than flattening the document.
 * Copyright (c) 2026 Dalsin Limited. MIT.
 */
#include "ob_html_lite.h"
#include "ob_css_lite.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OBHL_MAX_DEPTH 64

struct sb {
    char *p;
    size_t n;
    size_t cap;
    int failed;
};

struct frame {
    char tag[16];
    ol_node *node;
    ol_style style;
    int pre;
    int ordered;
    int counter;
};

struct parser {
    ob_html_lite_result *result;
    struct frame stack[OBHL_MAX_DEPTH];
    int depth;
    int in_head;
    int pending_space;
    int have_text;
    int next_image_id;
    ob_css_sheet css;
};

static int sb_reserve(struct sb *b, size_t extra)
{
    size_t need, cap;
    char *p;
    if (b->failed) return 0;
    need = b->n + extra + 1;
    if (need <= b->cap) return 1;
    cap = b->cap ? b->cap : 64;
    while (cap < need) cap *= 2;
    p = (char *)realloc(b->p, cap);
    if (!p) {
        b->failed = 1;
        return 0;
    }
    b->p = p;
    b->cap = cap;
    return 1;
}

static int sb_add(struct sb *b, const char *s, size_t n)
{
    if (!sb_reserve(b, n)) return 0;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
    return 1;
}

static int sb_addc(struct sb *b, char c)
{
    return sb_add(b, &c, 1);
}

static int sb_utf8(struct sb *b, unsigned long c)
{
    char s[4];
    if (c < 0x80) return sb_addc(b, (char)c);
    if (c < 0x800) {
        s[0] = (char)(0xc0 | (c >> 6));
        s[1] = (char)(0x80 | (c & 63));
        return sb_add(b, s, 2);
    }
    if (c >= 0xd800 && c < 0xe000) c = 0xfffd;
    if (c < 0x10000) {
        s[0] = (char)(0xe0 | (c >> 12));
        s[1] = (char)(0x80 | ((c >> 6) & 63));
        s[2] = (char)(0x80 | (c & 63));
        return sb_add(b, s, 3);
    }
    if (c > 0x10ffff) c = 0xfffd;
    s[0] = (char)(0xf0 | (c >> 18));
    s[1] = (char)(0x80 | ((c >> 12) & 63));
    s[2] = (char)(0x80 | ((c >> 6) & 63));
    s[3] = (char)(0x80 | (c & 63));
    return sb_add(b, s, 4);
}

static void sb_free(struct sb *b)
{
    if (!b) return;
    free(b->p);
    memset(b, 0, sizeof(*b));
}

static char *dup0(const char *s)
{
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static int cieq_n(const char *a, size_t n, const char *b)
{
    size_t i;
    if (strlen(b) != n) return 0;
    for (i = 0; i < n; ++i)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            return 0;
    return 1;
}

static void lower_name(char *out, size_t out_size, const char *s, size_t n)
{
    size_t i, m = n < out_size - 1 ? n : out_size - 1;
    for (i = 0; i < m; ++i) out[i] = (char)tolower((unsigned char)s[i]);
    out[m] = 0;
}

static const struct { const char *name; unsigned short code; } entities[] = {
    { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' },
    { "apos", '\'' }, { "nbsp", 0xa0 }, { "copy", 0xa9 }, { "reg", 0xae },
    { "trade", 0x2122 }, { "euro", 0x20ac }, { "pound", 0xa3 },
    { "mdash", 0x2014 }, { "ndash", 0x2013 }, { "hellip", 0x2026 },
    { "bull", 0x2022 }, { "lsquo", 0x2018 }, { "rsquo", 0x2019 },
    { "ldquo", 0x201c }, { "rdquo", 0x201d }
};

static size_t decode_entity(const char *s, const char *end, unsigned long *code)
{
    const char *p = s;
    size_t i, n;
    if (p < end && *p == '#') {
        int hex = p + 1 < end && (p[1] == 'x' || p[1] == 'X');
        unsigned long v = 0;
        p += hex ? 2 : 1;
        n = 0;
        while (p < end && n < 8 &&
               (hex ? isxdigit((unsigned char)*p) : isdigit((unsigned char)*p))) {
            if (isdigit((unsigned char)*p)) v = v * (hex ? 16 : 10) + (unsigned long)(*p - '0');
            else v = v * 16 + (unsigned long)(tolower((unsigned char)*p) - 'a' + 10);
            ++p;
            ++n;
        }
        if (!n) return 0;
        if (p < end && *p == ';') ++p;
        *code = v ? v : 0xfffd;
        return (size_t)(p - s);
    }
    for (i = 0; i < sizeof(entities) / sizeof(entities[0]); ++i) {
        n = strlen(entities[i].name);
        if ((size_t)(end - s) > n &&
            cieq_n(s, n, entities[i].name) && s[n] == ';') {
            *code = entities[i].code;
            return n + 1;
        }
    }
    return 0;
}

static int attribute(const char *tag, const char *end, const char *name,
                     char *out, size_t size)
{
    size_t nl = strlen(name);
    const char *p = tag;
    if (!size) return 0;
    out[0] = 0;
    while (p < end) {
        const char *k, *ke, *v, *ve;
        while (p < end && isspace((unsigned char)*p)) ++p;
        if (p >= end) break;
        k = p;
        while (p < end && (isalnum((unsigned char)*p) || *p == '-' || *p == '_')) ++p;
        ke = p;
        while (p < end && isspace((unsigned char)*p)) ++p;
        if (p >= end || *p != '=') {
            while (p < end && !isspace((unsigned char)*p)) ++p;
            continue;
        }
        ++p;
        while (p < end && isspace((unsigned char)*p)) ++p;
        v = p;
        if (p < end && (*p == '"' || *p == '\'')) {
            char q = *p++;
            v = p;
            while (p < end && *p != q) ++p;
            ve = p;
            if (p < end) ++p;
        } else {
            while (p < end && !isspace((unsigned char)*p) && *p != '>') ++p;
            ve = p;
        }
        if ((size_t)(ke - k) == nl && cieq_n(k, nl, name)) {
            size_t n = (size_t)(ve - v);
            if (n >= size) n = size - 1;
            memcpy(out, v, n);
            out[n] = 0;
            return 1;
        }
    }
    return 0;
}

static int token_ci(const char *list, const char *want)
{
    const char *p = list;
    size_t n = strlen(want);
    if (!list || !want) return 0;
    while (*p) {
        const char *s;
        while (*p && isspace((unsigned char)*p)) ++p;
        s = p;
        while (*p && !isspace((unsigned char)*p)) ++p;
        if ((size_t)(p - s) == n && cieq_n(s, n, want)) return 1;
    }
    return 0;
}

static int add_stylesheet_ref(ob_html_lite_result *result, const char *href)
{
    char **refs;
    char *copy;
    if (!result || !href || !*href || result->nstylesheets >= 8) return 1;
    copy = dup0(href);
    if (!copy) return 0;
    refs = (char **)realloc(result->stylesheet_href,
                            (size_t)(result->nstylesheets + 1) * sizeof(*refs));
    if (!refs) { free(copy); return 0; }
    result->stylesheet_href = refs;
    result->stylesheet_href[result->nstylesheets++] = copy;
    return 1;
}

static int numeric_attr(const char *tag, const char *end, const char *name, int fallback)
{
    char buf[32];
    char *ep;
    long v;
    if (!attribute(tag, end, name, buf, sizeof(buf))) return fallback;
    v = strtol(buf, &ep, 10);
    if (ep == buf || v <= 0 || v > 4096) return fallback;
    return (int)v;
}

static void apply_element_css(struct parser *p, const char *tag,
                              const char *attrs, const char *tag_end,
                              ol_style *style)
{
    char id[96], classes[256], inline_style[512];
    int have_id, have_classes, have_inline;
    if (!p || !style) return;
    have_id = attribute(attrs, tag_end, "id", id, sizeof(id));
    have_classes = attribute(attrs, tag_end, "class", classes, sizeof(classes));
    have_inline = attribute(attrs, tag_end, "style", inline_style, sizeof(inline_style));
    ob_css_apply(&p->css, tag,
                 have_id ? id : NULL,
                 have_classes ? classes : NULL,
                 have_inline ? inline_style : NULL, style);
}

static void inherited_inline(ol_style *out, const ol_style *parent)
{
    ol_style_init(out);
    out->display = OL_DISPLAY_INLINE;
    if (!parent) return;
    out->font_size = parent->font_size;
    out->line_height = parent->line_height;
    out->foreground = parent->foreground;
    out->text_flags = parent->text_flags;
    out->text_align = parent->text_align;
}

static void block_from_parent(ol_style *out, const ol_style *parent)
{
    inherited_inline(out, parent);
    out->display = OL_DISPLAY_BLOCK;
}

static const ol_style *current_style(struct parser *p)
{
    return p->depth > 0 ? &p->stack[p->depth - 1].style : NULL;
}

static ol_node *current_parent(struct parser *p)
{
    if (p->depth > 0 && p->stack[p->depth - 1].node)
        return p->stack[p->depth - 1].node;
    return ol_document_root(p->result->document);
}

static int current_pre(struct parser *p)
{
    return p->depth > 0 ? p->stack[p->depth - 1].pre : 0;
}

static int push_frame(struct parser *p, const char *tag, ol_node *node,
                      const ol_style *style, int pre, int ordered)
{
    struct frame *f;
    if (p->depth >= OBHL_MAX_DEPTH) return 0;
    f = &p->stack[p->depth++];
    memset(f, 0, sizeof(*f));
    {
        size_t n = strlen(tag);
        if (n >= sizeof(f->tag)) n = sizeof(f->tag) - 1;
        memcpy(f->tag, tag, n);
        f->tag[n] = 0;
    }
    f->node = node;
    f->style = *style;
    f->pre = pre;
    f->ordered = ordered;
    return 1;
}

static void pop_to_tag(struct parser *p, const char *tag)
{
    int i;
    for (i = p->depth - 1; i >= 0; --i) {
        if (!strcmp(p->stack[i].tag, tag)) {
            p->depth = i;
            return;
        }
    }
}

static void block_boundary(struct parser *p)
{
    p->pending_space = 0;
    p->have_text = 0;
}

static int append_text_node(struct parser *p, ol_node *parent,
                            const ol_style *style, const char *text, size_t len)
{
    char *copy;
    ol_node *node;
    if (!len) return 1;
    copy = (char *)malloc(len + 1);
    if (!copy) return 0;
    memcpy(copy, text, len);
    copy[len] = 0;
    node = ol_node_append(p->result->document, parent, OL_ROLE_TEXT, copy);
    free(copy);
    if (!node) return 0;
    ol_node_set_style(node, style);
    return 1;
}

static int emit_text(struct parser *p, const char *s, const char *end)
{
    struct sb b;
    ol_style style;
    int pre = current_pre(p);
    memset(&b, 0, sizeof(b));
    inherited_inline(&style, current_style(p));

    while (s < end) {
        unsigned long c;
        size_t n;
        if (*s == '&' && (n = decode_entity(s + 1, end, &c)) != 0) {
            s += 1 + n;
            if (!pre && (c == 0xa0 || c == ' ' || c == '\n' || c == '\r' || c == '\t')) {
                p->pending_space = 1;
                continue;
            }
            if (!pre && p->pending_space && p->have_text) {
                if (!sb_addc(&b, ' ')) break;
            }
            p->pending_space = 0;
            if (!sb_utf8(&b, c)) break;
            p->have_text = 1;
            continue;
        }

        if (!pre && isspace((unsigned char)*s)) {
            p->pending_space = 1;
            ++s;
            continue;
        }

        if (!pre && p->pending_space && p->have_text) {
            if (!sb_addc(&b, ' ')) break;
        }
        p->pending_space = 0;

        if (!sb_addc(&b, *s++)) break;
        p->have_text = 1;
    }

    if (b.failed) {
        sb_free(&b);
        return 0;
    }
    if (b.n && !append_text_node(p, current_parent(p), &style, b.p, b.n)) {
        sb_free(&b);
        return 0;
    }
    sb_free(&b);
    return 1;
}

static int decode_title(const char *s, const char *end, char **title)
{
    struct sb b;
    int pending = 0, have = 0;
    memset(&b, 0, sizeof(b));
    while (s < end) {
        unsigned long c;
        size_t n;
        if (*s == '&' && (n = decode_entity(s + 1, end, &c)) != 0) {
            s += 1 + n;
            if (c == 0xa0 || c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                pending = 1;
                continue;
            }
            if (pending && have && !sb_addc(&b, ' ')) break;
            pending = 0;
            if (!sb_utf8(&b, c)) break;
            have = 1;
        } else if (isspace((unsigned char)*s)) {
            pending = 1;
            ++s;
        } else {
            if (pending && have && !sb_addc(&b, ' ')) break;
            pending = 0;
            if (!sb_addc(&b, *s++)) break;
            have = 1;
        }
    }
    if (b.failed) {
        sb_free(&b);
        return 0;
    }
    free(*title);
    *title = dup0(b.p ? b.p : "");
    sb_free(&b);
    return *title != NULL;
}

static int append_br(struct parser *p)
{
    ol_style s;
    inherited_inline(&s, current_style(p));
    p->pending_space = 0;
    return append_text_node(p, current_parent(p), &s, "\n", 1);
}

static int nearest_list_prefix(struct parser *p, char *out, size_t out_size)
{
    int i;
    for (i = p->depth - 1; i >= 0; --i) {
        if (!strcmp(p->stack[i].tag, "ol")) {
            ++p->stack[i].counter;
            snprintf(out, out_size, "%d. ", p->stack[i].counter);
            return 1;
        }
        if (!strcmp(p->stack[i].tag, "ul")) {
            strncpy(out, "- ", out_size);
            out[out_size - 1] = 0;
            return 1;
        }
    }
    strncpy(out, "- ", out_size);
    out[out_size - 1] = 0;
    return 1;
}

static int open_container(struct parser *p, const char *tag,
                          ol_role role, ol_style *style, int pre, int ordered)
{
    ol_node *node = ol_node_append(p->result->document, current_parent(p), role, NULL);
    if (!node) return 0;
    ol_node_set_style(node, style);
    if (!push_frame(p, tag, node, style, pre, ordered)) return 0;
    if (style->display == OL_DISPLAY_BLOCK) block_boundary(p);
    return 1;
}

static int handle_open(struct parser *p, const char *tag,
                       const char *attrs, const char *tag_end, int self_close)
{
    ol_style s;
    const ol_style *parent = current_style(p);

    if (!strcmp(tag, "html")) {
        inherited_inline(&s, parent);
        apply_element_css(p, tag, attrs, tag_end, &s);
        return push_frame(p, tag, current_parent(p), &s, current_pre(p), 0);
    }
    if (!strcmp(tag, "head")) {
        inherited_inline(&s, parent);
        p->in_head = 1;
        return push_frame(p, tag, current_parent(p), &s, current_pre(p), 0);
    }
    if (!strcmp(tag, "body")) {
        block_from_parent(&s, parent);
        s.padding_top = s.padding_right = s.padding_bottom = s.padding_left = OL_CSSPX(8);
        apply_element_css(p, tag, attrs, tag_end, &s);
        return open_container(p, tag, OL_ROLE_BLOCK, &s, 0, 0);
    }

    if (p->in_head && !strcmp(tag, "link")) {
        char rel[96], href[512];
        if (attribute(attrs, tag_end, "rel", rel, sizeof(rel)) &&
            token_ci(rel, "stylesheet") &&
            attribute(attrs, tag_end, "href", href, sizeof(href)))
            return add_stylesheet_ref(p->result, href);
        return 1;
    }
    if (p->in_head) return 1;

    if (!strcmp(tag, "br")) return append_br(p);

    if (!strcmp(tag, "img")) {
        char src[512], alt[256];
        int w = numeric_attr(attrs, tag_end, "width", 64);
        int h = numeric_attr(attrs, tag_end, "height", 48);
        ol_node *node;
        inherited_inline(&s, parent);
        apply_element_css(p, tag, attrs, tag_end, &s);
        node = ol_node_append(p->result->document, current_parent(p), OL_ROLE_IMAGE, NULL);
        if (!node) return 0;
        ol_node_set_style(node, &s);
        ol_node_set_image(node, p->next_image_id++, OL_CSSPX(w), OL_CSSPX(h));
        if (attribute(attrs, tag_end, "src", src, sizeof(src))) ol_node_set_value(node, src);
        if (attribute(attrs, tag_end, "alt", alt, sizeof(alt))) ol_node_set_name(node, alt);
        return 1;
    }

    if (!strcmp(tag, "hr")) {
        ol_node *node;
        block_from_parent(&s, parent);
        s.margin_top = s.margin_bottom = OL_CSSPX(6);
        apply_element_css(p, tag, attrs, tag_end, &s);
        node = ol_node_append(p->result->document, current_parent(p), OL_ROLE_PARAGRAPH,
                              "--------------------------------");
        if (!node) return 0;
        s.foreground = 0xff808080u;
        ol_node_set_style(node, &s);
        block_boundary(p);
        return 1;
    }

    if (!strcmp(tag, "p") || !strcmp(tag, "div") ||
        !strcmp(tag, "section") || !strcmp(tag, "article") ||
        !strcmp(tag, "header") || !strcmp(tag, "footer") ||
        !strcmp(tag, "main") || !strcmp(tag, "nav")) {
        block_from_parent(&s, parent);
        if (!strcmp(tag, "p")) s.margin_bottom = OL_CSSPX(8);
        apply_element_css(p, tag, attrs, tag_end, &s);
        return open_container(p, tag, OL_ROLE_PARAGRAPH, &s, 0, 0);
    }

    if (tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6' && tag[2] == 0) {
        static const int sizes[] = { 32, 26, 22, 20, 18, 16 };
        int level = tag[1] - '1';
        block_from_parent(&s, parent);
        s.font_size = OL_CSSPX(sizes[level]);
        s.line_height = OL_CSSPX(sizes[level] + 6);
        s.text_flags |= OL_TEXT_BOLD;
        s.margin_top = OL_CSSPX(level < 2 ? 12 : 8);
        s.margin_bottom = OL_CSSPX(8);
        apply_element_css(p, tag, attrs, tag_end, &s);
        return open_container(p, tag, OL_ROLE_HEADING, &s, 0, 0);
    }

    if (!strcmp(tag, "blockquote")) {
        block_from_parent(&s, parent);
        s.margin_left = s.margin_right = OL_CSSPX(20);
        s.margin_bottom = OL_CSSPX(8);
        apply_element_css(p, tag, attrs, tag_end, &s);
        return open_container(p, tag, OL_ROLE_BLOCK, &s, 0, 0);
    }

    if (!strcmp(tag, "pre")) {
        block_from_parent(&s, parent);
        s.text_flags |= OL_TEXT_MONO;
        s.padding_top = s.padding_right = s.padding_bottom = s.padding_left = OL_CSSPX(6);
        s.margin_bottom = OL_CSSPX(8);
        s.background = 0x10000000u;
        apply_element_css(p, tag, attrs, tag_end, &s);
        return open_container(p, tag, OL_ROLE_BLOCK, &s, 1, 0);
    }

    if (!strcmp(tag, "ul") || !strcmp(tag, "ol")) {
        block_from_parent(&s, parent);
        s.margin_left = OL_CSSPX(20);
        s.margin_bottom = OL_CSSPX(8);
        apply_element_css(p, tag, attrs, tag_end, &s);
        return open_container(p, tag, OL_ROLE_LIST, &s, 0, !strcmp(tag, "ol"));
    }

    if (!strcmp(tag, "li")) {
        char prefix[32];
        ol_node *node, *text;
        block_from_parent(&s, parent);
        s.margin_bottom = OL_CSSPX(2);
        apply_element_css(p, tag, attrs, tag_end, &s);
        node = ol_node_append(p->result->document, current_parent(p), OL_ROLE_LIST_ITEM, NULL);
        if (!node) return 0;
        ol_node_set_style(node, &s);
        nearest_list_prefix(p, prefix, sizeof(prefix));
        {
            ol_style is;
            inherited_inline(&is, &s);
            text = ol_node_append(p->result->document, node, OL_ROLE_TEXT, prefix);
            if (!text) return 0;
            ol_node_set_style(text, &is);
        }
        if (!push_frame(p, tag, node, &s, 0, 0)) return 0;
        block_boundary(p);
        p->have_text = 1;
        return 1;
    }

    if (!strcmp(tag, "a")) {
        char href[512];
        ol_node *node;
        inherited_inline(&s, parent);
        s.foreground = 0xff0000c0u;
        s.text_flags |= OL_TEXT_UNDERLINE;
        apply_element_css(p, tag, attrs, tag_end, &s);
        node = ol_node_append(p->result->document, current_parent(p), OL_ROLE_LINK, NULL);
        if (!node) return 0;
        ol_node_set_style(node, &s);
        if (attribute(attrs, tag_end, "href", href, sizeof(href))) {
            ol_node_set_href(node, href);
            ol_node_set_name(node, href);
        }
        if (self_close) return 1;
        return push_frame(p, tag, node, &s, current_pre(p), 0);
    }

    if (!strcmp(tag, "strong") || !strcmp(tag, "b") ||
        !strcmp(tag, "em") || !strcmp(tag, "i") ||
        !strcmp(tag, "u") || !strcmp(tag, "code") ||
        !strcmp(tag, "span")) {
        ol_node *node;
        inherited_inline(&s, parent);
        if (!strcmp(tag, "strong") || !strcmp(tag, "b")) s.text_flags |= OL_TEXT_BOLD;
        if (!strcmp(tag, "em") || !strcmp(tag, "i")) s.text_flags |= OL_TEXT_ITALIC;
        if (!strcmp(tag, "u")) s.text_flags |= OL_TEXT_UNDERLINE;
        if (!strcmp(tag, "code")) s.text_flags |= OL_TEXT_MONO;
        apply_element_css(p, tag, attrs, tag_end, &s);
        node = ol_node_append(p->result->document, current_parent(p), OL_ROLE_BLOCK, NULL);
        if (!node) return 0;
        ol_node_set_style(node, &s);
        if (self_close) return 1;
        return push_frame(p, tag, node, &s, current_pre(p), 0);
    }

    if (!strcmp(tag, "table") || !strcmp(tag, "tr") ||
        !strcmp(tag, "td") || !strcmp(tag, "th")) {
        ol_role role = !strcmp(tag, "table") ? OL_ROLE_TABLE :
                       !strcmp(tag, "tr") ? OL_ROLE_ROW : OL_ROLE_CELL;
        block_from_parent(&s, parent);
        if (!strcmp(tag, "table")) s.margin_bottom = OL_CSSPX(8);
        else if (!strcmp(tag, "td") || !strcmp(tag, "th"))
            s.padding_left = s.padding_right = OL_CSSPX(3);
        apply_element_css(p, tag, attrs, tag_end, &s);
        ++p->result->unsupported_tags; /* semantic preservation; grid layout comes later */
        return open_container(p, tag, role, &s, 0, 0);
    }

    ++p->result->unsupported_tags;
    inherited_inline(&s, parent);
    apply_element_css(p, tag, attrs, tag_end, &s);
    if (self_close) return 1;
    return push_frame(p, tag, current_parent(p), &s, current_pre(p), 0);
}

static void handle_close(struct parser *p, const char *tag)
{
    if (!strcmp(tag, "head")) p->in_head = 0;
    if (!strcmp(tag, "p") || !strcmp(tag, "div") ||
        !strcmp(tag, "section") || !strcmp(tag, "article") ||
        !strcmp(tag, "header") || !strcmp(tag, "footer") ||
        !strcmp(tag, "main") || !strcmp(tag, "nav") ||
        !strcmp(tag, "blockquote") || !strcmp(tag, "pre") ||
        !strcmp(tag, "ul") || !strcmp(tag, "ol") || !strcmp(tag, "li") ||
        !strcmp(tag, "table") || !strcmp(tag, "tr") ||
        !strcmp(tag, "td") || !strcmp(tag, "th") ||
        (tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6' && tag[2] == 0))
        block_boundary(p);
    pop_to_tag(p, tag);
}

static const char *find_close_tag(const char *s, const char *end, const char *tag)
{
    size_t n = strlen(tag);
    while (s + n + 3 <= end) {
        if (s[0] == '<' && s[1] == '/' && cieq_n(s + 2, n, tag)) {
            const char *p = s + 2 + n;
            while (p < end && isspace((unsigned char)*p)) ++p;
            if (p < end && *p == '>') return s;
        }
        ++s;
    }
    return NULL;
}

int ob_html_lite_parse_with_css(const char *html, size_t length,
                                const char *extra_css, size_t extra_css_length,
                                ob_html_lite_result *result)
{
    const char *s, *end;
    struct parser *p = NULL;
    ol_style base;

    if (!html || !result) return 0;
    memset(result, 0, sizeof(*result));
    result->document = ol_document_new();
    if (!result->document) return 0;
    result->title = dup0("");
    if (!result->title) {
        ob_html_lite_result_free(result);
        return 0;
    }

    /* Keep the parser's 64 semantic frames off the caller's C stack.  A
     * normal Amiga Shell command may have only a few KiB of stack; the old
     * local struct worked in the browser's 64 KiB worker stack but could
     * overflow small utilities before parsing even <p>x</p>. */
    p = (struct parser *)calloc(1, sizeof(*p));
    if (!p) {
        ob_html_lite_result_free(result);
        return 0;
    }
    p->result = result;
    p->next_image_id = 1;
    ob_css_init(&p->css);
    if (extra_css && extra_css_length &&
        !ob_css_add(&p->css, extra_css, extra_css_length)) {
        ob_css_free(&p->css);
        free(p);
        ob_html_lite_result_free(result);
        return 0;
    }
    ol_style_init(&base);
    base.display = OL_DISPLAY_BLOCK;
    if (!push_frame(p, "#document", ol_document_root(result->document), &base, 0, 0)) {
        ob_css_free(&p->css);
        free(p);
        ob_html_lite_result_free(result);
        return 0;
    }

    s = html;
    end = html + length;
    while (s < end) {
        const char *lt = memchr(s, '<', (size_t)(end - s));
        const char *gt, *name, *ne, *attrs;
        char tag[16];
        int closing, self_close = 0;

        if (!lt) {
            if (!p->in_head && !emit_text(p, s, end)) goto fail;
            break;
        }
        if (lt > s && !p->in_head && !emit_text(p, s, lt)) goto fail;

        if (lt + 3 < end && !strncmp(lt, "<!--", 4)) {
            const char *q = lt + 4;
            while (q + 2 < end && strncmp(q, "-->", 3)) ++q;
            s = q + 2 < end ? q + 3 : end;
            continue;
        }

        gt = memchr(lt, '>', (size_t)(end - lt));
        if (!gt) {
            if (!p->in_head && !emit_text(p, lt, end)) goto fail;
            break;
        }

        name = lt + 1;
        while (name < gt && isspace((unsigned char)*name)) ++name;
        closing = name < gt && *name == '/';
        if (closing) {
            ++name;
            while (name < gt && isspace((unsigned char)*name)) ++name;
        }
        ne = name;
        while (ne < gt && (isalnum((unsigned char)*ne) || *ne == '-' || *ne == '_')) ++ne;
        if (ne == name) {
            s = gt + 1;
            continue;
        }
        lower_name(tag, sizeof(tag), name, (size_t)(ne - name));
        attrs = ne;

        {
            const char *q = gt;
            while (q > lt && isspace((unsigned char)q[-1])) --q;
            if (q > lt && q[-1] == '/') self_close = 1;
        }

        if (!closing && !strcmp(tag, "title")) {
            const char *close = find_close_tag(gt + 1, end, "title");
            if (close) {
                if (!decode_title(gt + 1, close, &result->title)) goto fail;
                gt = memchr(close, '>', (size_t)(end - close));
                s = gt ? gt + 1 : end;
            } else {
                s = end;
            }
            continue;
        }

        if (!closing && (!strcmp(tag, "script") || !strcmp(tag, "style"))) {
            const char *close = find_close_tag(gt + 1, end, tag);
            if (!strcmp(tag, "script")) {
                result->saw_script = 1;
            } else {
                result->saw_style = 1;
                if (close && !ob_css_add(&p->css, gt + 1,
                                         (size_t)(close - (gt + 1))))
                    goto fail;
            }
            if (close) {
                gt = memchr(close, '>', (size_t)(end - close));
                s = gt ? gt + 1 : end;
            } else {
                s = end;
            }
            continue;
        }

        if (closing) handle_close(p, tag);
        else if (!handle_open(p, tag, attrs, gt, self_close)) goto fail;

        s = gt + 1;
    }

    ob_css_free(&p->css);
    free(p);
    return 1;

fail:
    ob_css_free(&p->css);
    free(p);
    ob_html_lite_result_free(result);
    return 0;
}

int ob_html_lite_parse(const char *html, size_t length,
                       ob_html_lite_result *result)
{
    return ob_html_lite_parse_with_css(html, length, NULL, 0, result);
}

void ob_html_lite_result_free(ob_html_lite_result *result)
{
    int i;
    if (!result) return;
    ol_document_free(result->document);
    free(result->title);
    for (i = 0; i < result->nstylesheets; ++i)
        free(result->stylesheet_href[i]);
    free(result->stylesheet_href);
    memset(result, 0, sizeof(*result));
}
