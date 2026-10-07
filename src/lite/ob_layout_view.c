/*
 * OpenBrowser lightweight OpenLayout viewport.
 * Copyright (c) 2026 Dalsin Limited. MIT.
 */
#include "ob_layout_view.h"

#include <graphics/gfxmacros.h>
#include <proto/graphics.h>
#include <stdlib.h>
#include <string.h>

#include "oam_text.h"

static int to_px(ol_unit u)
{
    if (u >= 0) return (int)((u + OL_UNITS_PER_CSSPX / 2) / OL_UNITS_PER_CSSPX);
    return -(int)((-u + OL_UNITS_PER_CSSPX / 2) / OL_UNITS_PER_CSSPX);
}

static char *slice_latin1(const char *text, size_t offset, size_t length)
{
    char *utf8, *latin1;
    if (!text) return NULL;
    utf8 = (char *)malloc(length + 1);
    if (!utf8) return NULL;
    memcpy(utf8, text + offset, length);
    utf8[length] = 0;
    latin1 = oam_utf8_to_latin1(utf8);
    free(utf8);
    return latin1;
}

static int measure_text(void *userdata, const char *utf8, size_t length,
                        const ol_style *style, ol_text_metrics *metrics)
{
    ob_layout_view *view = (ob_layout_view *)userdata;
    char *copy, *latin1;
    ULONG width;
    (void)style;
    if (!view || !view->rp || !view->font || !metrics) return 0;
    copy = (char *)malloc(length + 1);
    if (!copy) return 0;
    memcpy(copy, utf8, length);
    copy[length] = 0;
    latin1 = oam_utf8_to_latin1(copy);
    free(copy);
    if (!latin1) return 0;
    width = TextLength(view->rp, (STRPTR)latin1, (ULONG)strlen(latin1));
    free(latin1);
    metrics->width = OL_CSSPX((ol_unit)width);
    metrics->ascent = OL_CSSPX((ol_unit)view->font->tf_Baseline);
    metrics->descent = OL_CSSPX((ol_unit)(view->font->tf_YSize - view->font->tf_Baseline));
    return 1;
}

void ob_layout_view_init(ob_layout_view *view, struct RastPort *rp,
                         struct TextFont *font)
{
    memset(view, 0, sizeof(*view));
    view->rp = rp;
    view->font = font;
    view->width = view->height = 1;
}

void ob_layout_view_set_rect(ob_layout_view *view, WORD left, WORD top,
                             WORD width, WORD height)
{
    if (!view) return;
    view->left = left;
    view->top = top;
    view->width = width > 0 ? width : 1;
    view->height = height > 0 ? height : 1;
}

void ob_layout_view_set_pens(ob_layout_view *view, UWORD text_pen,
                             UWORD back_pen, UWORD link_pen, UWORD fill_pen)
{
    if (!view) return;
    view->text_pen = text_pen;
    view->back_pen = back_pen;
    view->link_pen = link_pen;
    view->fill_pen = fill_pen;
}

void ob_layout_view_set_image_drawer(ob_layout_view *view,
                                     ob_layout_image_draw_fn draw,
                                     void *userdata)
{
    if (!view) return;
    view->image_draw = draw;
    view->image_userdata = userdata;
}

int ob_layout_view_set_document(ob_layout_view *view, ol_document *document)
{
    if (!view) return 0;
    view->document = document;
    view->scroll_px = 0;
    if (!document) return 1;
    SetFont(view->rp, view->font);
    return ol_layout(document, OL_CSSPX(view->width), measure_text, view);
}

LONG ob_layout_view_content_px(const ob_layout_view *view)
{
    if (!view || !view->document) return 0;
    return (LONG)to_px(ol_document_content_height(view->document));
}

LONG ob_layout_view_max_scroll(const ob_layout_view *view)
{
    LONG content, max;
    if (!view) return 0;
    content = ob_layout_view_content_px(view);
    max = content - view->height;
    return max > 0 ? max : 0;
}

void ob_layout_view_scroll_to(ob_layout_view *view, LONG scroll_px)
{
    LONG max;
    if (!view) return;
    max = ob_layout_view_max_scroll(view);
    if (scroll_px < 0) scroll_px = 0;
    if (scroll_px > max) scroll_px = max;
    view->scroll_px = scroll_px;
}

static int visible_y(const ob_layout_view *view, int y, int h)
{
    int top = y - view->scroll_px;
    return top + h > 0 && top < view->height;
}

static void draw_text_op(ob_layout_view *view, const ol_display_op *op)
{
    const ol_node *node;
    const char *text;
    char *latin1;
    ULONG style = 0;
    int x, y, h;

    node = ol_node_by_id(view->document, op->u.text.node_id);
    text = node ? ol_node_text(node) : NULL;
    if (!text) return;

    x = to_px(op->u.text.bounds.x);
    y = to_px(op->u.text.baseline);
    h = to_px(op->u.text.bounds.height);
    if (!visible_y(view, to_px(op->u.text.bounds.y), h)) return;

    latin1 = slice_latin1(text, op->u.text.text_offset, op->u.text.text_length);
    if (!latin1) return;

    if (op->u.text.style.text_flags & OL_TEXT_BOLD) style |= FSF_BOLD;
    if (op->u.text.style.text_flags & OL_TEXT_ITALIC) style |= FSF_ITALIC;
    if (op->u.text.style.text_flags & OL_TEXT_UNDERLINE) style |= FSF_UNDERLINED;

    SetSoftStyle(view->rp, style, FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED);
    SetAPen(view->rp,
            node && ol_node_role(node) == OL_ROLE_LINK ? view->link_pen : view->text_pen);
    Move(view->rp, view->left + x, view->top + y - view->scroll_px);
    Text(view->rp, (STRPTR)latin1, (ULONG)strlen(latin1));
    free(latin1);
}

static void draw_fill_op(ob_layout_view *view, const ol_display_op *op)
{
    int x = to_px(op->u.fill_rect.bounds.x);
    int y = to_px(op->u.fill_rect.bounds.y);
    int w = to_px(op->u.fill_rect.bounds.width);
    int h = to_px(op->u.fill_rect.bounds.height);
    if (w <= 0 || h <= 0 || !visible_y(view, y, h)) return;
    SetAPen(view->rp, view->fill_pen);
    RectFill(view->rp, view->left + x, view->top + y - view->scroll_px,
             view->left + x + w - 1, view->top + y - view->scroll_px + h - 1);
}

static void draw_image_op(ob_layout_view *view, const ol_display_op *op)
{
    const ol_node *node = ol_node_by_id(view->document, op->u.image.node_id);
    const char *alt = node ? ol_node_name(node) : NULL;
    int x = to_px(op->u.image.bounds.x);
    int y = to_px(op->u.image.bounds.y);
    int w = to_px(op->u.image.bounds.width);
    int h = to_px(op->u.image.bounds.height);
    if (w <= 0 || h <= 0 || !visible_y(view, y, h)) return;

    if (view->image_draw &&
        view->image_draw(view->image_userdata, (LONG)op->u.image.image_id,
                         view->rp, (WORD)(view->left + x),
                         (WORD)(view->top + y - view->scroll_px),
                         (UWORD)w, (UWORD)h))
        return;

    SetAPen(view->rp, view->text_pen);
    Move(view->rp, view->left + x, view->top + y - view->scroll_px);
    Draw(view->rp, view->left + x + w - 1, view->top + y - view->scroll_px);
    Draw(view->rp, view->left + x + w - 1, view->top + y - view->scroll_px + h - 1);
    Draw(view->rp, view->left + x, view->top + y - view->scroll_px + h - 1);
    Draw(view->rp, view->left + x, view->top + y - view->scroll_px);

    if (alt && *alt) {
        char *latin1 = oam_utf8_to_latin1(alt);
        if (latin1) {
            Move(view->rp, view->left + x + 3,
                 view->top + y - view->scroll_px + view->font->tf_Baseline + 2);
            Text(view->rp, (STRPTR)latin1, (ULONG)strlen(latin1));
            free(latin1);
        }
    }
}

static void draw_rule_op(ob_layout_view *view, const ol_display_op *op)
{
    int y1 = to_px(op->u.rule.y1);
    int y2 = to_px(op->u.rule.y2);
    if (!visible_y(view, y1, y2 - y1 + 1)) return;
    SetAPen(view->rp, view->text_pen);
    Move(view->rp, view->left + to_px(op->u.rule.x1),
         view->top + y1 - view->scroll_px);
    Draw(view->rp, view->left + to_px(op->u.rule.x2),
         view->top + y2 - view->scroll_px);
}

void ob_layout_view_draw(ob_layout_view *view)
{
    size_t i;
    if (!view || !view->rp) return;

    SetAPen(view->rp, view->back_pen);
    RectFill(view->rp, view->left, view->top,
             view->left + view->width - 1, view->top + view->height - 1);

    if (!view->document) return;
    SetFont(view->rp, view->font);

    for (i = 0; i < ol_display_count(view->document); ++i) {
        const ol_display_op *op = ol_display_get(view->document, i);
        if (!op) continue;
        switch (op->type) {
        case OL_OP_TEXT: draw_text_op(view, op); break;
        case OL_OP_FILL_RECT: draw_fill_op(view, op); break;
        case OL_OP_IMAGE: draw_image_op(view, op); break;
        case OL_OP_RULE: draw_rule_op(view, op); break;
        default: break;
        }
    }
    SetSoftStyle(view->rp, 0, FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED);
}

const ol_node *ob_layout_view_hit_action(const ob_layout_view *view,
                                         WORD mouse_x, WORD mouse_y,
                                         ULONG action)
{
    ol_unit x, y;
    if (!view || !view->document) return NULL;
    if (mouse_x < view->left || mouse_y < view->top ||
        mouse_x >= view->left + view->width ||
        mouse_y >= view->top + view->height)
        return NULL;
    x = OL_CSSPX(mouse_x - view->left);
    y = OL_CSSPX(mouse_y - view->top + view->scroll_px);
    return ol_hit_action(view->document, x, y, action);
}
