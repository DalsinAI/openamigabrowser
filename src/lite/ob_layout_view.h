#ifndef OB_LAYOUT_VIEW_H
#define OB_LAYOUT_VIEW_H

#include <exec/types.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include "ob_openlayout_api.h"

typedef int (*ob_layout_image_draw_fn)(void *userdata, LONG image_id,
                                       struct RastPort *rp, WORD x, WORD y,
                                       UWORD width, UWORD height);

typedef struct {
    struct RastPort *rp;
    struct TextFont *font;
    ol_document *document;
    WORD left, top, width, height;
    LONG scroll_px;
    UWORD text_pen;
    UWORD back_pen;
    UWORD link_pen;
    UWORD fill_pen;
    ob_layout_image_draw_fn image_draw;
    void *image_userdata;
} ob_layout_view;

void ob_layout_view_init(ob_layout_view *view, struct RastPort *rp,
                         struct TextFont *font);
void ob_layout_view_set_rect(ob_layout_view *view, WORD left, WORD top,
                             WORD width, WORD height);
void ob_layout_view_set_pens(ob_layout_view *view, UWORD text_pen,
                             UWORD back_pen, UWORD link_pen, UWORD fill_pen);
void ob_layout_view_set_image_drawer(ob_layout_view *view,
                                     ob_layout_image_draw_fn draw,
                                     void *userdata);
int ob_layout_view_set_document(ob_layout_view *view, ol_document *document);
LONG ob_layout_view_content_px(const ob_layout_view *view);
LONG ob_layout_view_max_scroll(const ob_layout_view *view);
void ob_layout_view_scroll_to(ob_layout_view *view, LONG scroll_px);
void ob_layout_view_draw(ob_layout_view *view);
const ol_node *ob_layout_view_hit_action(const ob_layout_view *view,
                                         WORD mouse_x, WORD mouse_y,
                                         ULONG action);

#endif
