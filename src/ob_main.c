/*
 * OpenBrowser: the main window, GadTools (Phase 1, "OpenBrowser Lite").
 *
 *   [Back] [Reload] [ address ............................ ] [Go]
 *   the page, as wrapped text, links shown as "text [n]"
 *   [Links...]  the status line
 *
 * Pages come over HTTP/HTTPS (ob_http, on OpenMail's bsdsocket/AmiSSL
 * layer) and are shown as text by OpenMail's HTML-to-text converter. Click
 * a line to follow its first link, or pick one from Links....
 * Other programs open addresses on the ARexx port AMIGACHROME.BROWSER
 * with OPENURL <address> (the hook OpenMail uses).
 *
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <graphics/gfxbase.h>
#include <rexx/storage.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <clib/alib_protos.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oam_buf.h"
#include "oam_html.h"
#include "oam_net.h"
#include "oam_stack.h"
#include "oam_text.h"
#include "ob_http.h"
#include "lite/ob_html_lite.h"
#include "lite/ob_layout_view.h"

struct Library *GadToolsBase = NULL;

#define VERSION_TEXT "OpenBrowser 0.2 (7.10.2026)"
static const char version[] __attribute__((used)) = "$VER: " VERSION_TEXT;
#define PORT_NAME "AMIGACHROME.BROWSER"
#define HOME_PAGE "http://example.com/"
#define HISTORY_MAX 32

enum { GID_BACK = 1, GID_RELOAD, GID_URL, GID_GO, GID_SCROLL, GID_LINKS, GID_STATUS, GID_COUNT };
enum { M_ABOUT = 1, M_QUIT, M_OPEN, M_RELOAD, M_BACK, M_LINKS };

#define PAD 4
#define MARGIN 6

static struct NewMenu menus[] = {
    { NM_TITLE, "Project", NULL, 0, 0, NULL },
    { NM_ITEM, "Open address", "L", 0, 0, (APTR)M_OPEN },
    { NM_ITEM, "About...", NULL, 0, 0, (APTR)M_ABOUT },
    { NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
    { NM_ITEM, "Quit", "Q", 0, 0, (APTR)M_QUIT },
    { NM_TITLE, "Page", NULL, 0, 0, NULL },
    { NM_ITEM, "Back", "B", 0, 0, (APTR)M_BACK },
    { NM_ITEM, "Reload", "R", 0, 0, (APTR)M_RELOAD },
    { NM_ITEM, "Links...", "K", 0, 0, (APTR)M_LINKS },
    { NM_END, NULL, NULL, 0, 0, NULL }
};

/* ---- state -------------------------------------------------------------------- */

static struct Screen *scr;
static APTR vi;
static struct Window *win;
static struct Menu *menu;
static struct Gadget *glist, *gad[GID_COUNT];
static struct TextFont *font;
static struct DrawInfo *drawinfo;
static struct TextAttr font_attr, fixed_attr;
static int fh, cw, btn_h;
static WORD page_left, page_top, page_width, page_height;
static struct MsgPort *rexx_port;
static BOOL quit_now;
static char status[200] = "Ready.";
static char url_text[1280] = HOME_PAGE;

static char *history[HISTORY_MAX];
static int nhistory;

static ob_html_lite_result page_layout;
static BOOL page_layout_valid;
static ob_layout_view page_view;

/* rows are retained for the separate Links... chooser. */
struct rows {
    struct List list;
    struct Node *node;
    char **text;
    int count;
};
static char **links;          /* absolute addresses, links[0] is [1] */
static int nlinks;
static char page_url[1280];

/* ---- small helpers ------------------------------------------------------------ */

static void set_status(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(status, sizeof status, fmt, ap);
    va_end(ap);
    if (win && gad[GID_STATUS]) GT_SetGadgetAttrs(gad[GID_STATUS], win, NULL, GTTX_Text, (ULONG)status, TAG_DONE);
}

static void progress(const char *step)
{
    set_status("%s...", step);
}

static int label_w(const char *s)
{
    struct IntuiText it = { 1, 0, JAM1, 0, 0, &font_attr, (UBYTE *)s, NULL };
    return IntuiTextLength(&it);
}

static void rows_free(struct rows *r)
{
    int i;
    for (i = 0; i < r->count; i++) free(r->text[i]);
    free(r->text);
    free(r->node);
    memset(r, 0, sizeof *r);
    NewList(&r->list);
}

static void rows_link(struct rows *r)
{
    int i;
    NewList(&r->list);
    for (i = 0; i < r->count; i++) {
        r->node[i].ln_Name = r->text[i];
        AddTail(&r->list, &r->node[i]);
    }
}

static void free_links(void)
{
    int i;
    for (i = 0; i < nlinks; i++) free(links[i]);
    free(links);
    links = NULL;
    nlinks = 0;
}

/* ---- gadgets ------------------------------------------------------------------ */

static struct Gadget *button(struct Gadget *g, struct NewGadget *ng, int id, const char *label, int x, int y, int w)
{
    ng->ng_LeftEdge = x; ng->ng_TopEdge = y; ng->ng_Width = w; ng->ng_Height = btn_h;
    ng->ng_GadgetText = (UBYTE *)label; ng->ng_GadgetID = id; ng->ng_Flags = PLACETEXT_IN;
    ng->ng_TextAttr = &font_attr;
    g = CreateGadget(BUTTON_KIND, g, ng, GT_Underscore, '_', TAG_DONE);
    gad[id] = g;
    return g;
}

static void make_gadgets(void)
{
    struct NewGadget ng;
    struct Gadget *g;
    int x, y, w, h, bw, ax, ay, aw, ah;

    memset(gad, 0, sizeof gad);
    glist = NULL;
    if (!(g = CreateContext(&glist))) return;
    memset(&ng, 0, sizeof ng);
    ng.ng_VisualInfo = vi;
    ax = win->BorderLeft + MARGIN;
    ay = win->BorderTop + MARGIN;
    aw = win->Width - win->BorderLeft - win->BorderRight - 2 * MARGIN;
    ah = win->Height - win->BorderTop - win->BorderBottom - 2 * MARGIN;

    x = ax;
    bw = label_w("Reload") + 16;
    g = button(g, &ng, GID_BACK, "_Back", x, ay, bw); x += bw + PAD;
    g = button(g, &ng, GID_RELOAD, "_Reload", x, ay, bw); x += bw + PAD;
    w = ax + aw - x - (label_w("Go") + 16) - PAD;
    ng.ng_LeftEdge = x; ng.ng_TopEdge = ay; ng.ng_Width = w; ng.ng_Height = btn_h;
    ng.ng_GadgetText = NULL; ng.ng_GadgetID = GID_URL; ng.ng_Flags = 0; ng.ng_TextAttr = &font_attr;
    g = CreateGadget(STRING_KIND, g, &ng, GTST_String, (ULONG)url_text, GTST_MaxChars, sizeof url_text - 1, TAG_DONE);
    gad[GID_URL] = g;
    g = button(g, &ng, GID_GO, "_Go", x + w + PAD, ay, label_w("Go") + 16);

    y = ay + btn_h + PAD;
    h = ah - 2 * (btn_h + PAD);
    page_left = ax;
    page_top = y;
    page_width = aw - 20 - PAD;
    page_height = h;
    if (page_width < 1) page_width = 1;
    if (page_height < 1) page_height = 1;
    ob_layout_view_set_rect(&page_view, page_left, page_top, page_width, page_height);
    if (page_layout_valid) ob_layout_view_set_document(&page_view, page_layout.document);

    ng.ng_LeftEdge = page_left + page_width + PAD; ng.ng_TopEdge = page_top;
    ng.ng_Width = 20; ng.ng_Height = page_height;
    ng.ng_GadgetText = NULL; ng.ng_GadgetID = GID_SCROLL; ng.ng_Flags = 0; ng.ng_TextAttr = &font_attr;
    {
        LONG total = ob_layout_view_content_px(&page_view);
        if (total < page_height) total = page_height;
        g = CreateGadget(SCROLLER_KIND, g, &ng,
                         GTSC_Top, (ULONG)page_view.scroll_px,
                         GTSC_Total, (ULONG)total,
                         GTSC_Visible, (ULONG)page_height,
                         GTSC_Arrows, 14, TAG_DONE);
    }
    gad[GID_SCROLL] = g;

    y = ay + ah - btn_h;
    bw = label_w("Links...") + 16;
    g = button(g, &ng, GID_LINKS, "Lin_ks...", ax, y, bw);
    ng.ng_LeftEdge = ax + bw + PAD; ng.ng_TopEdge = y; ng.ng_Width = aw - bw - PAD; ng.ng_Height = btn_h;
    ng.ng_GadgetText = NULL; ng.ng_GadgetID = GID_STATUS; ng.ng_Flags = 0; ng.ng_TextAttr = &font_attr;
    g = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)status, GTTX_Border, TRUE, TAG_DONE);
    gad[GID_STATUS] = g;
}

/* Fill state, build, AddGList: never set attributes on gadgets not attached. */
static void redo(void)
{
    if (glist) {
        RemoveGList(win, glist, -1);
        FreeGadgets(glist);
        glist = NULL;
    }
    EraseRect(win->RPort, win->BorderLeft, win->BorderTop, win->Width - win->BorderRight - 1, win->Height - win->BorderBottom - 1);
    make_gadgets();
    if (!glist) return;
    AddGList(win, glist, ~0, -1, NULL);
    RefreshGList(glist, win, NULL, -1);
    GT_RefreshWindow(win, NULL);
    ob_layout_view_draw(&page_view);
}

/* ---- pages -------------------------------------------------------------------- */

static void update_scroller(void)
{
    LONG total;
    if (!win || !gad[GID_SCROLL]) return;
    total = ob_layout_view_content_px(&page_view);
    if (total < page_height) total = page_height;
    GT_SetGadgetAttrs(gad[GID_SCROLL], win, NULL,
                      GTSC_Top, (ULONG)page_view.scroll_px,
                      GTSC_Total, (ULONG)total,
                      GTSC_Visible, (ULONG)page_height, TAG_DONE);
}

static void clear_layout(void)
{
    if (page_layout_valid) ob_html_lite_result_free(&page_layout);
    memset(&page_layout, 0, sizeof page_layout);
    page_layout_valid = FALSE;
    ob_layout_view_set_document(&page_view, NULL);
}

static int show_html(const char *utf8, size_t len)
{
    clear_layout();
    if (!ob_html_lite_parse(utf8, len, &page_layout)) return 0;
    page_layout_valid = TRUE;
    if (!ob_layout_view_set_document(&page_view, page_layout.document)) {
        clear_layout();
        return 0;
    }
    update_scroller();
    ob_layout_view_draw(&page_view);
    return 1;
}

static int show_text(const char *utf8)
{
    ol_node *root, *pre, *text;
    ol_style style;
    clear_layout();
    page_layout.document = ol_document_new();
    if (!page_layout.document) return 0;
    page_layout.title = strdup("");
    if (!page_layout.title) { clear_layout(); return 0; }
    root = ol_document_root(page_layout.document);
    pre = ol_node_append(page_layout.document, root, OL_ROLE_BLOCK, NULL);
    text = pre ? ol_node_append(page_layout.document, pre, OL_ROLE_TEXT, utf8 ? utf8 : "") : NULL;
    if (!pre || !text) { clear_layout(); return 0; }
    ol_style_init(&style);
    style.display = OL_DISPLAY_BLOCK;
    style.padding_top = style.padding_right = style.padding_bottom = style.padding_left = OL_CSSPX(8);
    ol_node_set_style(pre, &style);
    style.display = OL_DISPLAY_INLINE;
    style.text_flags = OL_TEXT_MONO;
    ol_node_set_style(text, &style);
    page_layout_valid = TRUE;
    if (!ob_layout_view_set_document(&page_view, page_layout.document)) { clear_layout(); return 0; }
    update_scroller();
    ob_layout_view_draw(&page_view);
    return 1;
}

/* links as OpenMail's converter gives them (one per line) made absolute */
static void take_links(const char *list)
{
    const char *p = list;
    free_links();
    while (p && *p) {
        const char *e = strchr(p, '\n');
        size_t n = e ? (size_t)(e - p) : strlen(p);
        char href[1280], absu[1280];
        char **nl = realloc(links, (nlinks + 1) * sizeof *links);
        if (!nl) break;
        links = nl;
        if (n >= sizeof href) n = sizeof href - 1;
        memcpy(href, p, n);
        href[n] = 0;
        ob_url_resolve(page_url, href, absu, sizeof absu);
        links[nlinks++] = strdup(absu);
        p = e ? e + 1 : p + n;
    }
}

static void history_push(const char *u)
{
    if (nhistory && !strcmp(history[nhistory - 1], u)) return;
    if (nhistory == HISTORY_MAX) {
        free(history[0]);
        memmove(history, history + 1, (HISTORY_MAX - 1) * sizeof *history);
        nhistory--;
    }
    history[nhistory++] = strdup(u);
}

static void set_url_gadget(const char *u)
{
    snprintf(url_text, sizeof url_text, "%s", u);
    if (win && gad[GID_URL]) GT_SetGadgetAttrs(gad[GID_URL], win, NULL, GTST_String, (ULONG)url_text, TAG_DONE);
}

static void open_url(const char *u, BOOL remember)
{
    ob_response resp;
    char err[200];
    oam_buf utf8, text, linklist;

    if (win) SetWindowPointer(win, WA_BusyPointer, TRUE, WA_PointerDelay, TRUE, TAG_DONE);
    set_status("Opening %s", u);
    if (!ob_http_get(u, &resp, err, sizeof err)) {
        set_status("Could not open the page: %s", err);
        ob_response_free(&resp);
        if (win) SetWindowPointer(win, TAG_DONE);
        return;
    }
    snprintf(page_url, sizeof page_url, "%s", resp.final_url);
    set_url_gadget(page_url);
    if (remember) history_push(page_url);

    oam_buf_init(&utf8);
    oam_buf_init(&text);
    oam_buf_init(&linklist);
    /* the body as UTF-8 first; servers that name no charset get Latin-1 */
    if (!resp.charset[0] || !strcmp(resp.charset, "utf-8") || !strcmp(resp.charset, "utf8"))
        oam_buf_add(&utf8, oam_buf_str(&resp.body), resp.body.len);
    else
        oam_charset_to_utf8(&utf8, resp.charset, oam_buf_str(&resp.body), resp.body.len);
    if (!resp.charset[0] && !oam_utf8_valid(oam_buf_str(&utf8), utf8.len)) {
        oam_buf_clear(&utf8);
        oam_charset_to_utf8(&utf8, "iso-8859-1", oam_buf_str(&resp.body), resp.body.len);
    }
    if (strstr(resp.content_type, "html") || !resp.content_type[0]) {
        /* Keep the old converter only for the separate Links... chooser. */
        oam_html_to_text(oam_buf_str(&utf8), utf8.len, &text, &linklist);
        take_links(oam_buf_str(&linklist));
        if (!show_html(oam_buf_str(&utf8), utf8.len))
            show_text("OpenBrowser could not lay out this page.");
    } else if (!strncmp(resp.content_type, "text/", 5)) {
        free_links();
        show_text(oam_buf_str(&utf8));
    } else {
        char message[256];
        free_links();
        snprintf(message, sizeof message,
                 "This page is %s (%lu bytes), which OpenBrowser cannot show yet.",
                 resp.content_type, (unsigned long)resp.body.len);
        show_text(message);
    }
    if (resp.status == 200)
        set_status("%s: %lu bytes, %d link%s.", resp.content_type[0] ? resp.content_type : "page",
                   (unsigned long)resp.body.len, nlinks, nlinks == 1 ? "" : "s");
    else
        set_status("The server answered %d.", resp.status);
    oam_buf_free(&utf8);
    oam_buf_free(&text);
    oam_buf_free(&linklist);
    ob_response_free(&resp);
    if (win) SetWindowPointer(win, TAG_DONE);
}

static void go_back(void)
{
    if (nhistory < 2) { set_status("There is no page to go back to."); return; }
    free(history[--nhistory]);
    open_url(history[nhistory - 1], FALSE);
}

static void links_window(void)
{
    struct rows r;
    struct Gadget *lg = NULL, *g;
    struct NewGadget ng;
    struct Window *lw;
    int i, w = cw * 72, picked = -1;
    char **t;
    BOOL done = FALSE;

    if (!nlinks) { set_status("This page has no links."); return; }
    memset(&r, 0, sizeof r);
    NewList(&r.list);
    t = malloc(nlinks * sizeof *t);
    r.node = calloc(nlinks, sizeof *r.node);
    if (!t || !r.node) { free(t); free(r.node); return; }
    for (i = 0; i < nlinks; i++) {
        t[i] = malloc(strlen(links[i]) + 12);
        if (t[i]) sprintf(t[i], "[%d] %s", i + 1, links[i]);
    }
    r.text = t;
    r.count = nlinks;
    rows_link(&r);
    if (!(g = CreateContext(&lg))) { rows_free(&r); return; }
    memset(&ng, 0, sizeof ng);
    ng.ng_VisualInfo = vi;
    ng.ng_TextAttr = &fixed_attr;
    ng.ng_LeftEdge = scr->WBorLeft + PAD;
    ng.ng_TopEdge = scr->WBorTop + scr->Font->ta_YSize + 1 + PAD;
    ng.ng_Width = w;
    ng.ng_Height = (fixed_attr.ta_YSize + 1) * 12;
    g = CreateGadget(LISTVIEW_KIND, g, &ng, GTLV_Labels, (ULONG)&r.list, TAG_DONE);
    lw = g ? OpenWindowTags(NULL, WA_Title, (ULONG)"Links: pick one to open it", WA_PubScreen, (ULONG)scr,
                            WA_InnerWidth, w + 2 * PAD, WA_InnerHeight, ng.ng_Height + 2 * PAD,
                            WA_Left, win->LeftEdge + 40, WA_Top, win->TopEdge + 30,
                            WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
                            WA_Gadgets, (ULONG)lg, WA_IDCMP, IDCMP_CLOSEWINDOW | LISTVIEWIDCMP | IDCMP_REFRESHWINDOW,
                            TAG_DONE) : NULL;
    if (lw) {
        GT_RefreshWindow(lw, NULL);
        while (!done) {
            struct IntuiMessage *m;
            WaitPort(lw->UserPort);
            while ((m = GT_GetIMsg(lw->UserPort))) {
                ULONG cl = m->Class;
                UWORD code = m->Code;
                GT_ReplyIMsg(m);
                if (cl == IDCMP_CLOSEWINDOW) done = TRUE;
                else if (cl == IDCMP_GADGETUP) { picked = code; done = TRUE; }
                else if (cl == IDCMP_REFRESHWINDOW) { GT_BeginRefresh(lw); GT_EndRefresh(lw, TRUE); }
            }
        }
        CloseWindow(lw);
    }
    FreeGadgets(lg);
    rows_free(&r);
    if (picked >= 0 && picked < nlinks) {
        char target[1280];
        snprintf(target, sizeof target, "%s", links[picked]);
        open_url(target, TRUE);
    }
}

/* ---- window ------------------------------------------------------------------- */

static BOOL open_window(void)
{
    struct TextFont *fixed = ((struct GfxBase *)GfxBase)->DefaultFont;
    int w, h;
    if (!(scr = LockPubScreen(NULL))) return FALSE;
    if (!(vi = GetVisualInfo(scr, TAG_DONE))) return FALSE;
    drawinfo = GetScreenDrawInfo(scr);
    if (!drawinfo) return FALSE;
    font_attr = *scr->Font;
    if (!(font = OpenFont(&font_attr))) return FALSE;
    fixed_attr.ta_Name = fixed->tf_Message.mn_Node.ln_Name;
    fixed_attr.ta_YSize = fixed->tf_YSize;
    fixed_attr.ta_Style = fixed->tf_Style;
    fixed_attr.ta_Flags = fixed->tf_Flags;
    fh = font->tf_YSize;
    cw = fixed->tf_XSize;
    btn_h = fh + 6;
    if ((menu = CreateMenus(menus, TAG_DONE))) LayoutMenus(menu, vi, GTMN_NewLookMenus, TRUE, TAG_DONE);
    w = scr->Width * 9 / 10;
    h = scr->Height * 8 / 10;
    win = OpenWindowTags(NULL, WA_Title, (ULONG)"OpenBrowser", WA_ScreenTitle, (ULONG)VERSION_TEXT,
                         WA_PubScreen, (ULONG)scr, WA_Width, w, WA_Height, h,
                         WA_Left, (scr->Width - w) / 2, WA_Top, (scr->Height - h) / 2,
                         WA_MinWidth, cw * 40, WA_MinHeight, (fh + 6) * 8, WA_MaxWidth, ~0, WA_MaxHeight, ~0,
                         WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_SizeGadget, TRUE,
                         WA_SizeBBottom, TRUE, WA_Activate, TRUE, WA_SmartRefresh, TRUE, WA_NewLookMenus, TRUE,
                         WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_GADGETUP | IDCMP_GADGETDOWN | IDCMP_MOUSEMOVE |
                                   IDCMP_MOUSEBUTTONS | IDCMP_MENUPICK | IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW |
                                   SCROLLERIDCMP | BUTTONIDCMP | STRINGIDCMP | TEXTIDCMP,
                         TAG_DONE);
    if (!win) return FALSE;
    ob_layout_view_init(&page_view, win->RPort, font);
    ob_layout_view_set_pens(&page_view,
                            drawinfo->dri_Pens[TEXTPEN],
                            drawinfo->dri_Pens[BACKGROUNDPEN],
                            drawinfo->dri_Pens[FILLPEN],
                            drawinfo->dri_Pens[SHINEPEN]);
    if (menu) SetMenuStrip(win, menu);
    redo();
    return TRUE;
}

static void close_window(void)
{
    if (win) { ClearMenuStrip(win); CloseWindow(win); win = NULL; }
    if (glist) { FreeGadgets(glist); glist = NULL; }
    if (menu) { FreeMenus(menu); menu = NULL; }
    if (font) { CloseFont(font); font = NULL; }
    if (drawinfo) { FreeScreenDrawInfo(scr, drawinfo); drawinfo = NULL; }
    if (vi) { FreeVisualInfo(vi); vi = NULL; }
    if (scr) { UnlockPubScreen(NULL, scr); scr = NULL; }
}

static void about(void)
{
    struct EasyStruct es = { sizeof(struct EasyStruct), 0, (UBYTE *)"OpenBrowser",
        (UBYTE *)VERSION_TEXT "\n\nA web browser for AmigaOS 3.2.\nMIT licence, Copyright (c) 2026 Dalsin Limited.", (UBYTE *)"OK" };
    EasyRequestArgs(win, &es, NULL, NULL);
}

static void go_to_typed(void)
{
    const char *typed = url_text;
    if (gad[GID_URL]) typed = (const char *)((struct StringInfo *)gad[GID_URL]->SpecialInfo)->Buffer;
    if (typed && *typed) {
        char target[1280];
        snprintf(target, sizeof target, "%s", typed);
        open_url(target, TRUE);
    }
}

static void menu_pick(UWORD code)
{
    while (code != MENUNULL) {
        struct MenuItem *item = ItemAddress(menu, code);
        if (!item) break;
        switch ((ULONG)GTMENUITEM_USERDATA(item)) {
        case M_ABOUT: about(); break;
        case M_QUIT: quit_now = TRUE; break;
        case M_OPEN: if (gad[GID_URL]) ActivateGadget(gad[GID_URL], win, NULL); break;
        case M_BACK: go_back(); break;
        case M_RELOAD: if (page_url[0]) open_url(page_url, FALSE); break;
        case M_LINKS: links_window(); break;
        }
        code = item->NextSelect;
    }
}

static void scroll_from_gadget(void)
{
    ULONG top = 0;
    if (!win || !gad[GID_SCROLL]) return;
    GT_GetGadgetAttrs(gad[GID_SCROLL], win, NULL, GTSC_Top, &top, TAG_DONE);
    ob_layout_view_scroll_to(&page_view, (LONG)top);
    ob_layout_view_draw(&page_view);
}

static void open_page_link_at(WORD x, WORD y)
{
    const ol_node *node = ob_layout_view_hit_action(&page_view, x, y, OL_ACTION_ACTIVATE);
    const char *href;
    char target[1280];
    if (!node) return;
    href = ol_node_href(node);
    if (!href || !*href) return;
    ob_url_resolve(page_url, href, target, sizeof target);
    open_url(target, TRUE);
}

static void window_events(void)
{
    struct IntuiMessage *m;
    while (win && (m = GT_GetIMsg(win->UserPort))) {
        ULONG cl = m->Class;
        UWORD code = m->Code;
        WORD mx = m->MouseX, my = m->MouseY;
        struct Gadget *g = (struct Gadget *)m->IAddress;
        GT_ReplyIMsg(m);
        switch (cl) {
        case IDCMP_CLOSEWINDOW: quit_now = TRUE; break;
        case IDCMP_MENUPICK: menu_pick(code); break;
        case IDCMP_NEWSIZE: redo(); break;
        case IDCMP_REFRESHWINDOW:
            GT_BeginRefresh(win); GT_EndRefresh(win, TRUE); ob_layout_view_draw(&page_view); break;
        case IDCMP_MOUSEBUTTONS:
            if (code == SELECTDOWN) open_page_link_at(mx, my);
            break;
        case IDCMP_MOUSEMOVE:
            if (g && g->GadgetID == GID_SCROLL) scroll_from_gadget();
            break;
        case IDCMP_GADGETDOWN:
        case IDCMP_GADGETUP:
            if (g && g->GadgetID == GID_SCROLL) { scroll_from_gadget(); break; }
            if (!g) break;
            switch (g->GadgetID) {
            case GID_BACK: go_back(); break;
            case GID_RELOAD: if (page_url[0]) open_url(page_url, FALSE); break;
            case GID_URL: case GID_GO: go_to_typed(); break;
            case GID_LINKS: links_window(); break;
            }
            break;
        }
    }
}

/* OPENURL <address>, OPENFILE <path>, QUIT on AMIGACHROME.BROWSER */
static void rexx_events(void)
{
    struct RexxMsg *rm;
    while ((rm = (struct RexxMsg *)GetMsg(rexx_port))) {
        const char *cmd = (const char *)rm->rm_Args[0];
        rm->rm_Result1 = 0;
        rm->rm_Result2 = 0;
        if (cmd && !strncasecmp(cmd, "OPENURL ", 8)) {
            char target[1280];
            snprintf(target, sizeof target, "%s", cmd + 8);
            if (win) WindowToFront(win);
            open_url(target, TRUE);
        } else if (cmd && !strncasecmp(cmd, "OPENFILE ", 9)) {
            set_status("Opening files comes later; asked for %s", cmd + 9);
            rm->rm_Result1 = 5;
        } else if (cmd && !strcasecmp(cmd, "QUIT")) {
            quit_now = TRUE;
        } else {
            rm->rm_Result1 = 10;
        }
        ReplyMsg((struct Message *)rm);
    }
}

static int browser_main(int argc, char **argv)
{
    char err[200];
    if (!ob_openlayout_open()) {
        PutStr((STRPTR)"OpenBrowser needs openlayout.library 2 or later\n");
        return 20;
    }
    if (!(GadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 39))) {
        PutStr((STRPTR)"OpenBrowser needs AmigaOS 3.0 or later (gadtools.library 39)\n");
        ob_openlayout_close();
        return 20;
    }
    Forbid();
    if (!FindPort((STRPTR)PORT_NAME) && (rexx_port = CreateMsgPort())) {
        rexx_port->mp_Node.ln_Name = (char *)PORT_NAME;
        rexx_port->mp_Node.ln_Pri = 0;
        AddPort(rexx_port);
    }
    Permit();
    if (!open_window()) {
        PutStr((STRPTR)"OpenBrowser: the window could not open\n");
        close_window();
        if (rexx_port) { RemPort(rexx_port); DeleteMsgPort(rexx_port); }
        CloseLibrary(GadToolsBase);
        ob_openlayout_close();
        return 20;
    }
    ob_http_progress = progress;
    if (!oam_net_init(err, sizeof err)) {
        set_status("No network: %s", err);
        show_text("OpenBrowser could not reach the network.\n\n"
                  "Check that a TCP/IP stack is running (bsdsocket.library) and that\n"
                  "AmiSSL 5 is installed for https:// pages.");
    } else {
        open_url(argc > 1 ? argv[1] : HOME_PAGE, TRUE);
    }

    while (!quit_now) {
        ULONG wsig = win ? 1UL << win->UserPort->mp_SigBit : 0;
        ULONG rsig = rexx_port ? 1UL << rexx_port->mp_SigBit : 0;
        ULONG got = Wait(wsig | rsig | SIGBREAKF_CTRL_C);
        if (got & SIGBREAKF_CTRL_C) quit_now = TRUE;
        if (got & rsig) rexx_events();
        if (got & wsig) window_events();
    }

    if (rexx_port) {
        Forbid();
        RemPort(rexx_port);
        Permit();
        rexx_events();                  /* answer anything that came in meanwhile */
        DeleteMsgPort(rexx_port);
    }
    clear_layout();
    close_window();
    free_links();
    while (nhistory) free(history[--nhistory]);
    oam_net_cleanup();
    CloseLibrary(GadToolsBase);
    ob_openlayout_close();
    return 0;
}

int main(int argc, char **argv)
{
    return oam_run_with_stack(65536, browser_main, argc, argv);
}
