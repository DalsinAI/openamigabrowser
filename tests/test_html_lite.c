#include "ob_html_lite.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) {     printf("HTMLLITE FAIL line=%d expr=%s\n", __LINE__, #x); return 20; } } while (0)
#define MARK(s) do { printf("HTMLLITE stage=%s\n", (s)); fflush(stdout); } while (0)

static int probe_case(const char *name, const char *html)
{
    ob_html_lite_result r;
    printf("HTMLLITE probe=%s begin\n", name); fflush(stdout);
    if (!ob_html_lite_parse(html, strlen(html), &r)) {
        printf("HTMLLITE probe=%s parse-fail\n", name); fflush(stdout);
        return 0;
    }
    printf("HTMLLITE probe=%s parsed\n", name); fflush(stdout);
    ob_html_lite_result_free(&r);
    printf("HTMLLITE probe=%s freed\n", name); fflush(stdout);
    return 1;
}

static size_t count_type(const ol_document *doc, ol_op_type type)
{
    size_t i, n = 0;
    for (i = 0; i < ol_display_count(doc); ++i) {
        const ol_display_op *op = ol_display_get(doc, i);
        if (op && op->type == type) ++n;
    }
    return n;
}

static const ol_style *find_text_style(const ol_document *doc, const char *text)
{
    size_t i;
    for (i = 0; i < ol_display_count(doc); ++i) {
        const ol_display_op *op = ol_display_get(doc, i);
        const ol_node *node;
        const char *node_text;
        if (!op || op->type != OL_OP_TEXT) continue;
        node = ol_node_by_id(doc, op->u.text.node_id);
        node_text = node ? ol_node_text(node) : NULL;
        if (node_text && strstr(node_text, text)) return &op->u.text.style;
    }
    return NULL;
}

int main(void)
{
    static const char html[] =
        "<!doctype html><html><head>"
        "<title>OpenBrowser &amp; OpenLayout</title>"
        "<link rel=\"stylesheet\" href=\"/site.css\">"
        "<style>body{color:#202020} h1{color:#112233;font-size:28px}"
        ".lead{background:#eeeeee;padding:4px} #mainlink{color:red}"
        ".gone{display:none}</style></head><body>"
        "<h1>First light</h1>"
        "<p class=\"lead\">A tiny browser can keep <strong>semantics</strong> and "
        "<a id=\"mainlink\" href=\"https://example.com/\" style=\"color:#00aa00\">real links</a> without WebCore.</p>"
        "<p class=\"gone\">NOT SHOWN</p>"
        "<ul><li>Small</li><li>Fast</li></ul>"
        "<p><img src=\"logo.png\" alt=\"Logo\" width=\"80\" height=\"40\"> image.</p>"
        "<script>document.body.textContent='not executed';</script>"
        "</body></html>";
    ob_html_lite_result r;
    const ol_node *link;
    const ol_style *style;
    ol_rect box;
    ol_unit h1, h2;

    MARK("open-begin");
    CHECK(ob_openlayout_open());
    MARK("opened");
    CHECK(probe_case("p", "<p>x</p>"));
    CHECK(probe_case("link", "<p><a href=\"https://example.com/\">x</a></p>"));
    CHECK(probe_case("list", "<ul><li>one</li><li>two</li></ul>"));
    CHECK(probe_case("image", "<p><img src=\"x.png\" alt=\"x\" width=\"16\" height=\"16\"></p>"));
    {
        static const char ext_html[] = "<p class=\"ext\">external css</p>";
        static const char ext_css[] = ".ext{color:#abcdef;font-weight:bold}";
        ob_html_lite_result ext;
        CHECK(ob_html_lite_parse_with_css(ext_html, sizeof(ext_html) - 1,
                                          ext_css, sizeof(ext_css) - 1, &ext));
        CHECK(ol_layout(ext.document, OL_CSSPX(320), NULL, NULL));
        style = find_text_style(ext.document, "external css");
        CHECK(style != NULL);
        CHECK(style->foreground == 0xffabcdefu);
        CHECK(style->text_flags & OL_TEXT_BOLD);
        ob_html_lite_result_free(&ext);
    }
    CHECK(ob_html_lite_parse(html, sizeof(html) - 1, &r));
    MARK("parsed");
    CHECK(r.document != NULL);
    CHECK(r.title != NULL);
    CHECK(strcmp(r.title, "OpenBrowser & OpenLayout") == 0);
    CHECK(r.saw_script == 1);
    CHECK(r.saw_style == 1);
    CHECK(r.nstylesheets == 1);
    CHECK(!strcmp(r.stylesheet_href[0], "/site.css"));
    CHECK(r.unsupported_tags == 0);

    MARK("layout-wide-begin");
    CHECK(ol_layout(r.document, OL_CSSPX(360), NULL, NULL));
    MARK("layout-wide-done");
    h1 = ol_document_content_height(r.document);
    CHECK(h1 > 0);
    CHECK(count_type(r.document, OL_OP_TEXT) >= 8);
    CHECK(count_type(r.document, OL_OP_IMAGE) == 1);
    CHECK(count_type(r.document, OL_OP_FILL_RECT) >= 1);
    style = find_text_style(r.document, "First light");
    CHECK(style != NULL);
    CHECK(style->foreground == 0xff112233u);
    CHECK(style->font_size == OL_CSSPX(28));
    style = find_text_style(r.document, "semantics");
    CHECK(style != NULL);
    CHECK(style->text_flags & OL_TEXT_BOLD);
    style = find_text_style(r.document, "real links");
    CHECK(style != NULL);
    CHECK(style->foreground == 0xff00aa00u);
    CHECK(style->text_flags & OL_TEXT_UNDERLINE);
    CHECK(find_text_style(r.document, "NOT SHOWN") == NULL);

    link = ol_find_role_name(r.document, OL_ROLE_LINK, "https://example.com/");
    CHECK(link != NULL);
    CHECK(strcmp(ol_node_href(link), "https://example.com/") == 0);
    CHECK(ol_node_actions(link) & OL_ACTION_ACTIVATE);
    box = ol_node_bounds(link);
    CHECK(box.width > 0 && box.height > 0);
    CHECK(ol_node_id(ol_hit_action(r.document, box.x + 1, box.y + 1,
                                OL_ACTION_ACTIVATE)) == ol_node_id(link));

    MARK("semantic-checks-done");
    CHECK(ol_layout(r.document, OL_CSSPX(180), NULL, NULL));
    MARK("layout-narrow-done");
    h2 = ol_document_content_height(r.document);
    CHECK(h2 > h1);
    CHECK(ol_node_id(ol_find_role_name(r.document, OL_ROLE_LINK,
          "https://example.com/")) == ol_node_id(link));

    printf("HTMLLITE PASS title=\"%s\" wide_px=%ld narrow_px=%ld ops=%lu script=%d\n",
           r.title,
           (long)(h1 / OL_UNITS_PER_CSSPX),
           (long)(h2 / OL_UNITS_PER_CSSPX),
           (unsigned long)ol_display_count(r.document),
           r.saw_script);

    ob_html_lite_result_free(&r);
    ob_openlayout_close();
    return 0;
}
