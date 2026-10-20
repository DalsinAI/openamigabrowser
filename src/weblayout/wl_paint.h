#ifndef WL_PAINT_H
#define WL_PAINT_H

#include <exec/types.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include "wl_mem.h"
#include "wl_openlayout.h"

typedef int (*wl_image_draw_fn)(void *userdata, LONG image_id,
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
    wl_image_draw_fn image_draw;
    void *image_userdata;
    wl_pool *pool;                  /* the document's, for the text it converts */
} wl_view;

void wl_view_init(wl_view *view, struct RastPort *rp,
                         struct TextFont *font);
void wl_view_set_rect(wl_view *view, WORD left, WORD top,
                             WORD width, WORD height);
void wl_view_set_pens(wl_view *view, UWORD text_pen,
                             UWORD back_pen, UWORD link_pen, UWORD fill_pen);
void wl_view_set_image_drawer(wl_view *view,
                                     wl_image_draw_fn draw,
                                     void *userdata);
int wl_view_set_document(wl_view *view, ol_document *document);
LONG wl_view_content_px(const wl_view *view);
LONG wl_view_max_scroll(const wl_view *view);
void wl_view_scroll_to(wl_view *view, LONG scroll_px);
void wl_view_draw(wl_view *view);
/* the href (as written on the page) of the link at the mouse, or NULL */
const char *wl_view_hit_link(const wl_view *view, WORD mouse_x, WORD mouse_y);
const ol_node *wl_view_hit_action(const wl_view *view,
                                         WORD mouse_x, WORD mouse_y,
                                         ULONG action);

#endif
