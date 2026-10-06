/*
 * OpenBrowser: the browser window. WebCore pages in a GadTools window:
 * Back, Forward, Reload and Stop, the address, the page, and a status line.
 *
 *   OpenBrowser [URL]
 *
 * The page is painted into a 32-bit buffer and copied into the window; on a
 * graphics card's screen as it is, on a native screen dithered to pens. One
 * task runs everything: it waits on the window, on WebCore's wake-up signal
 * and on a timer for WebCore's next timer, and between waits lets WebCore
 * do what is due.
 *
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <devices/inputevent.h>
#include <dos/dos.h>
#include <graphics/displayinfo.h>
#include <graphics/gfxmacros.h>
#include <graphics/modeid.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/icon.h>
#include <proto/keymap.h>
#include <proto/timer.h>

#include "ob_blit.h"
#include "ob_webview.h"
#include "ob_splash.h"

/* TLS sessions kept from the last run (ob_webview_load_tls_sessions). */
#define TLS_SESSIONS "PROGDIR:TLSSessions"
#include "oam_stack.h"

static const char version[] __attribute__((used)) = "$VER: OpenBrowser 0.4 (6.10.2026)";

#define HOME_PAGE "https://example.com/"

enum { GID_BACK = 1, GID_FORWARD, GID_RELOAD, GID_STOP, GID_URL, GID_STATUS, GID_COUNT };

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase, *KeymapBase, *IconBase;
struct Device *TimerBase;

static struct Screen *screen;
static struct Screen *ownScreen;   /* our true-colour screen, when Workbench has few colours */
static APTR visualInfo;
static struct Window *window;
static struct Gadget *gadgetList, *gadgets[GID_COUNT];
static struct TextAttr *screenFont;
static int fontHeight, toolbarHeight, statusHeight;

static OBWebView *view;
static int pageLeft, pageTop, pageWidth, pageHeight;
static unsigned char *pageBuffer;
static int pageStride;

static struct Task *mainTask;
static BYTE wakeSignal = -1;
static struct MsgPort *timerPort;
static struct timerequest *timerRequest;
static int timerPending;

static char urlText[2048] = HOME_PAGE;
static char statusText[256] = "";
static char titleText[256] = "OpenBrowser";
static int quitNow;
static int loading;

/* Page options (Settings menu; tool types JAVASCRIPT=NO, PICTURES=NO,
 * WEBFONTS=YES and LITE=YES). */
static int scriptsOn = 1, picturesOn = 1, webFontsOn = 0, liteOn = 0;
static struct Menu *menuStrip;
enum { MENU_RELOAD = 1, MENU_QUIT, MENU_SCRIPTS, MENU_PICTURES, MENU_WEBFONTS, MENU_LITE };

/* --- text: WebCore speaks UTF-8, Amiga gadgets Latin-1 ----------------------- */

static void utf8ToLatin1(const char *in, char *out, int size)
{
    const unsigned char *p = (const unsigned char *)in;
    int n = 0;
    while (*p && n < size - 1) {
        unsigned long c = *p++;
        if (c >= 0xc0 && (c < 0xe0) && (*p & 0xc0) == 0x80)
            c = ((c & 0x1f) << 6) | (*p++ & 0x3f);
        else if (c >= 0xe0) {
            while ((*p & 0xc0) == 0x80)
                p++;
            c = '?';
        }
        out[n++] = c > 0xff ? '?' : (char)c;
    }
    out[n] = 0;
}

static void latin1ToUTF8(const char *in, char *out, int size)
{
    const unsigned char *p = (const unsigned char *)in;
    int n = 0;
    while (*p && n < size - 2) {
        if (*p < 0x80)
            out[n++] = *p;
        else {
            out[n++] = 0xc0 | (*p >> 6);
            out[n++] = 0x80 | (*p & 0x3f);
        }
        p++;
    }
    out[n] = 0;
}

/* --- the window ---------------------------------------------------------- */

static void setStatus(const char *text)
{
    strncpy(statusText, text, sizeof statusText - 1);
    statusText[sizeof statusText - 1] = 0;
    if (window && gadgets[GID_STATUS])
        GT_SetGadgetAttrs(gadgets[GID_STATUS], window, NULL, GTTX_Text, (ULONG)statusText, TAG_DONE);
}

static void setURLGadget(const char *latin1)
{
    strncpy(urlText, latin1, sizeof urlText - 1);
    urlText[sizeof urlText - 1] = 0;
    if (window && gadgets[GID_URL])
        GT_SetGadgetAttrs(gadgets[GID_URL], window, NULL, GTST_String, (ULONG)urlText, TAG_DONE);
}

static void updateButtons(void)
{
    if (!window)
        return;
    GT_SetGadgetAttrs(gadgets[GID_BACK], window, NULL, GA_Disabled, !ob_webview_can_go_back(view), TAG_DONE);
    GT_SetGadgetAttrs(gadgets[GID_FORWARD], window, NULL, GA_Disabled, !ob_webview_can_go_forward(view), TAG_DONE);
    GT_SetGadgetAttrs(gadgets[GID_STOP], window, NULL, GA_Disabled, !loading, TAG_DONE);
}

static struct Gadget *makeButton(struct Gadget *previous, struct NewGadget *ng, int id, const char *label, int x, int width)
{
    ng->ng_LeftEdge = x;
    ng->ng_Width = width;
    ng->ng_GadgetText = (UBYTE *)label;
    ng->ng_GadgetID = id;
    ng->ng_Flags = PLACETEXT_IN;
    gadgets[id] = CreateGadget(BUTTON_KIND, previous, ng, TAG_DONE);
    return gadgets[id];
}

static int labelWidth(const char *text)
{
    struct IntuiText it = { 0, 0, 0, 0, 0, screenFont, (UBYTE *)text, NULL };
    return IntuiTextLength(&it) + 16;
}

/* Lays the gadgets out for the window's current size. */
static int makeGadgets(void)
{
    struct NewGadget ng;
    struct Gadget *g;
    int left = window->BorderLeft + 4, top = window->BorderTop + 3, x, buttonsRight;
    int inner = window->Width - window->BorderLeft - window->BorderRight;

    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = screenFont;
    ng.ng_VisualInfo = visualInfo;
    ng.ng_TopEdge = top;
    ng.ng_Height = fontHeight + 6;
    g = CreateContext(&gadgetList);
    x = left;
    g = makeButton(g, &ng, GID_BACK, "Back", x, labelWidth("Back"));
    x += labelWidth("Back") + 4;
    g = makeButton(g, &ng, GID_FORWARD, "Forward", x, labelWidth("Forward"));
    x += labelWidth("Forward") + 4;
    g = makeButton(g, &ng, GID_RELOAD, "Reload", x, labelWidth("Reload"));
    x += labelWidth("Reload") + 4;
    g = makeButton(g, &ng, GID_STOP, "Stop", x, labelWidth("Stop"));
    x += labelWidth("Stop") + 8;
    buttonsRight = x;

    ng.ng_LeftEdge = buttonsRight;
    ng.ng_Width = window->BorderLeft + inner - 4 - buttonsRight;
    ng.ng_GadgetText = NULL;
    ng.ng_GadgetID = GID_URL;
    gadgets[GID_URL] = g = CreateGadget(STRING_KIND, g, &ng, GTST_String, (ULONG)urlText, GTST_MaxChars, sizeof urlText - 1, TAG_DONE);

    toolbarHeight = ng.ng_Height + 6;
    statusHeight = fontHeight + 6;
    ng.ng_LeftEdge = window->BorderLeft;
    ng.ng_TopEdge = window->Height - window->BorderBottom - statusHeight;
    ng.ng_Width = inner;
    ng.ng_Height = statusHeight;
    ng.ng_GadgetID = GID_STATUS;
    gadgets[GID_STATUS] = g = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)statusText, GTTX_Border, TRUE, TAG_DONE);
    if (!g)
        return 0;

    pageLeft = window->BorderLeft;
    pageTop = window->BorderTop + toolbarHeight;
    pageWidth = inner;
    pageHeight = window->Height - window->BorderBottom - statusHeight - pageTop;
    if (pageHeight < 16)
        pageHeight = 16;

    AddGList(window, gadgetList, (UWORD)-1, (UWORD)-1, NULL);
    RefreshGList(gadgetList, window, NULL, (UWORD)-1);
    GT_RefreshWindow(window, NULL);
    return 1;
}

static void removeGadgets(void)
{
    if (gadgetList) {
        RemoveGList(window, gadgetList, (UWORD)-1);
        FreeGadgets(gadgetList);
        gadgetList = NULL;
    }
    memset(gadgets, 0, sizeof gadgets);
}

static int allocatePage(void)
{
    if (pageBuffer)
        FreeVec(pageBuffer);
    pageStride = pageWidth * 4;
    pageBuffer = AllocVec((ULONG)pageStride * pageHeight, MEMF_ANY | MEMF_CLEAR);
    return pageBuffer != NULL;
}

static void paintPage(int x, int y, int width, int height)
{
    if (!pageBuffer || width <= 0 || height <= 0)
        return;
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > pageWidth)
        width = pageWidth - x;
    if (y + height > pageHeight)
        height = pageHeight - y;
    if (width <= 0 || height <= 0)
        return;
    ob_webview_paint(view, pageBuffer, pageStride, x, y, width, height);
    ob_blit(window->RPort, pageLeft + x, pageTop + y, pageBuffer + y * pageStride + x * 4, pageStride, width, height);
}

static void paintDirty(void)
{
    int x, y, width, height;
    ob_webview_dirty(view, &x, &y, &width, &height);
    if (width > 0 && height > 0)
        paintPage(x, y, width, height);
}

static void relayout(void)
{
    removeGadgets();
    /* Clear the window inside the borders: the old gadgets' imagery. */
    SetAPen(window->RPort, 0);
    RectFill(window->RPort, window->BorderLeft, window->BorderTop, window->Width - window->BorderRight - 1,
        window->Height - window->BorderBottom - 1);
    if (!makeGadgets() || !allocatePage())
        return;
    ob_webview_resize(view, pageWidth, pageHeight);
    updateButtons();
    paintPage(0, 0, pageWidth, pageHeight);
}

/* --- WebCore's callbacks ---------------------------------------------------- */

static void onTitle(void *context, const char *title)
{
    (void)context;
    utf8ToLatin1(*title ? title : "OpenBrowser", titleText, sizeof titleText);
    if (window)
        SetWindowTitles(window, (UBYTE *)titleText, (UBYTE *)-1);
}

static void onURL(void *context, const char *url)
{
    char latin1[sizeof urlText];
    (void)context;
    utf8ToLatin1(url, latin1, sizeof latin1);
    setURLGadget(latin1);
    updateButtons();
}

static void onStatus(void *context, const char *text)
{
    char latin1[sizeof statusText];
    (void)context;
    utf8ToLatin1(text, latin1, sizeof latin1);
    setStatus(latin1);
}

static int loadPercent;

static void onLoading(void *context, int isLoading, int percent)
{
    char text[64];
    (void)context;
    loading = isLoading;
    loadPercent = percent;
    if (isLoading)
        snprintf(text, sizeof text, "Loading... %d%%", percent);
    else
        strcpy(text, "Done.");
    setStatus(text);
    updateButtons();
}

/* While a page loads, the status line says what is being fetched: on a 68k
 * one file can take seconds (a secure connection to a new site, about ten). */
static void onResource(void *context, const char *url, int started, const char *error)
{
    char text[sizeof statusText], latin1[sizeof statusText];
    const char *shown = url;
    (void)context;
    (void)error;
    if (!started || !loading || !url)
        return;
    if (!strncmp(shown, "https://", 8))
        shown += 8;
    else if (!strncmp(shown, "http://", 7))
        shown += 7;
    utf8ToLatin1(shown, latin1, sizeof latin1);
    snprintf(text, sizeof text, "Loading %d%%: %s", loadPercent, latin1);
    setStatus(text);
}

static void onFailed(void *context, const char *url, const char *description)
{
    char text[sizeof statusText], latin1[200];
    (void)context;
    (void)url;
    utf8ToLatin1(description, latin1, sizeof latin1);
    snprintf(text, sizeof text, "Could not load the page: %s", latin1);
    setStatus(text);
}

static LONG requester(const char *title, const char *text, const char *buttons)
{
    struct EasyStruct es = { sizeof(struct EasyStruct), 0, (UBYTE *)title, (UBYTE *)"%s", (UBYTE *)buttons };
    char latin1[1024];
    utf8ToLatin1(text, latin1, sizeof latin1);
    return EasyRequest(window, &es, NULL, (ULONG)latin1);
}

static void onAlert(void *context, const char *message)
{
    (void)context;
    requester("The page says", message, "OK");
}

static int onConfirm(void *context, const char *message)
{
    (void)context;
    return requester("The page asks", message, "OK|Cancel") == 1;
}

/* --- timers and wake-ups -------------------------------------------------- */

static void wakeUp(void *context)
{
    (void)context;
    Signal(mainTask, 1UL << wakeSignal);
}

static void armTimer(double seconds)
{
    if (timerPending) {
        AbortIO((struct IORequest *)timerRequest);
        WaitIO((struct IORequest *)timerRequest);
        timerPending = 0;
    }
    if (seconds < 0)
        return;
    if (seconds < 0.005)
        seconds = 0.005;
    timerRequest->tr_node.io_Command = TR_ADDREQUEST;
    timerRequest->tr_time.tv_secs = (ULONG)seconds;
    timerRequest->tr_time.tv_micro = (ULONG)((seconds - (ULONG)seconds) * 1000000.0);
    SendIO((struct IORequest *)timerRequest);
    timerPending = 1;
}

/* --- input ------------------------------------------------------------------ */

static int qualifiers(UWORD q)
{
    int result = 0;
    if (q & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
        result |= OB_QUAL_SHIFT;
    if (q & IEQUALIFIER_CONTROL)
        result |= OB_QUAL_CONTROL;
    if (q & (IEQUALIFIER_LALT | IEQUALIFIER_RALT))
        result |= OB_QUAL_ALT;
    if (q & IEQUALIFIER_RCOMMAND)
        result |= OB_QUAL_AMIGA;
    if (q & IEQUALIFIER_CAPSLOCK)
        result |= OB_QUAL_CAPSLOCK;
    return result;
}

static int inPage(int x, int y)
{
    return x >= pageLeft && y >= pageTop && x < pageLeft + pageWidth && y < pageTop + pageHeight;
}

static ULONG lastClickSecs, lastClickMicros;
static int clickCount;

static void handleMouse(struct IntuiMessage *msg)
{
    int x = msg->MouseX, y = msg->MouseY, quals = qualifiers(msg->Qualifier);
    if (msg->Class == IDCMP_MOUSEMOVE) {
        if (inPage(x, y) || (msg->Qualifier & IEQUALIFIER_LEFTBUTTON))
            ob_webview_mouse(view, OB_MOUSE_MOVE, x - pageLeft, y - pageTop, OB_BUTTON_NONE, quals, 0);
        return;
    }
    switch (msg->Code) {
    case SELECTDOWN:
        if (!inPage(x, y))
            return;
        if (clickCount && DoubleClick(lastClickSecs, lastClickMicros, msg->Seconds, msg->Micros))
            clickCount++;
        else
            clickCount = 1;
        lastClickSecs = msg->Seconds;
        lastClickMicros = msg->Micros;
        ob_webview_mouse(view, OB_MOUSE_DOWN, x - pageLeft, y - pageTop, OB_BUTTON_LEFT, quals, clickCount);
        break;
    case SELECTUP:
        ob_webview_mouse(view, OB_MOUSE_UP, x - pageLeft, y - pageTop, OB_BUTTON_LEFT, quals, clickCount);
        break;
    case MIDDLEDOWN:
        if (inPage(x, y))
            ob_webview_mouse(view, OB_MOUSE_DOWN, x - pageLeft, y - pageTop, OB_BUTTON_MIDDLE, quals, 1);
        break;
    case MIDDLEUP:
        ob_webview_mouse(view, OB_MOUSE_UP, x - pageLeft, y - pageTop, OB_BUTTON_MIDDLE, quals, 1);
        break;
    }
}

#ifndef RAWKEY_WHEEL_UP
#define RAWKEY_WHEEL_UP 0x7a
#define RAWKEY_WHEEL_DOWN 0x7b
#endif

/* The message is still unreplied: MapRawKey() reads the dead-key state
 * that IAddress points to. */
static void handleKey(struct IntuiMessage *msg)
{
    UWORD code = msg->Code;
    int down = !(code & IECODE_UP_PREFIX), rawKey = code & 0x7f;
    char latin1[16], utf8[48];
    struct InputEvent ie;
    LONG n = 0;

    if (rawKey == RAWKEY_WHEEL_UP || rawKey == RAWKEY_WHEEL_DOWN) {
        if (down)
            ob_webview_wheel(view, msg->MouseX - pageLeft, msg->MouseY - pageTop, 0, rawKey == RAWKEY_WHEEL_UP ? 1 : -1,
                qualifiers(msg->Qualifier));
        return;
    }
    if (down) {
        memset(&ie, 0, sizeof ie);
        ie.ie_Class = IECLASS_RAWKEY;
        ie.ie_Code = code;
        ie.ie_Qualifier = msg->Qualifier;
        ie.ie_EventAddress = msg->IAddress ? *(APTR *)msg->IAddress : NULL;
        n = MapRawKey(&ie, latin1, sizeof latin1 - 1, NULL);
    }
    if (n < 0)
        n = 0;
    latin1[n] = 0;
    /* Control characters are keys WebCore names itself (Return, Tab...). */
    if (n == 1 && ((unsigned char)latin1[0] < 0x20 || latin1[0] == 0x7f))
        latin1[0] = 0;
    latin1ToUTF8(latin1, utf8, sizeof utf8);
    ob_webview_key(view, down, rawKey, utf8, qualifiers(msg->Qualifier));
}

/* --- the program ------------------------------------------------------------ */

static void loadTyped(void)
{
    char utf8[sizeof urlText * 2];
    struct StringInfo *si = (struct StringInfo *)gadgets[GID_URL]->SpecialInfo;
    latin1ToUTF8((const char *)si->Buffer, utf8, sizeof utf8);
    if (*utf8)
        ob_webview_load(view, utf8);
}

static void handleGadget(struct Gadget *gadget)
{
    switch (gadget->GadgetID) {
    case GID_BACK: ob_webview_back(view); break;
    case GID_FORWARD: ob_webview_forward(view); break;
    case GID_RELOAD: ob_webview_reload(view); break;
    case GID_STOP: ob_webview_stop(view); break;
    case GID_URL: loadTyped(); break;
    }
}

static void makeMenus(void)
{
    struct NewMenu menus[] = {
        { NM_TITLE, (STRPTR)"Project", NULL, 0, 0, NULL },
        { NM_ITEM, (STRPTR)"Reload", (STRPTR)"R", 0, 0, (APTR)MENU_RELOAD },
        { NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
        { NM_ITEM, (STRPTR)"Quit", (STRPTR)"Q", 0, 0, (APTR)MENU_QUIT },
        { NM_TITLE, (STRPTR)"Settings", NULL, 0, 0, NULL },
        { NM_ITEM, (STRPTR)"JavaScript", (STRPTR)"J", CHECKIT | MENUTOGGLE | (scriptsOn ? CHECKED : 0), 0, (APTR)MENU_SCRIPTS },
        { NM_ITEM, (STRPTR)"Pictures", (STRPTR)"P", CHECKIT | MENUTOGGLE | (picturesOn ? CHECKED : 0), 0, (APTR)MENU_PICTURES },
        { NM_ITEM, (STRPTR)"Web fonts", (STRPTR)"F", CHECKIT | MENUTOGGLE | (webFontsOn ? CHECKED : 0), 0, (APTR)MENU_WEBFONTS },
        { NM_ITEM, (STRPTR)"Lite (mobile pages)", (STRPTR)"L", CHECKIT | MENUTOGGLE | (liteOn ? CHECKED : 0), 0, (APTR)MENU_LITE },
        { NM_END, NULL, NULL, 0, 0, NULL }
    };
    menuStrip = CreateMenus(menus, TAG_DONE);
    if (menuStrip && LayoutMenus(menuStrip, visualInfo, GTMN_NewLookMenus, TRUE, TAG_DONE))
        SetMenuStrip(window, menuStrip);
}

static void handleMenu(UWORD code)
{
    while (code != MENUNULL) {
        struct MenuItem *item = ItemAddress(menuStrip, code);
        if (!item)
            break;
        switch ((ULONG)GTMENUITEM_USERDATA(item)) {
        case MENU_RELOAD:
            ob_webview_reload(view);
            break;
        case MENU_QUIT:
            quitNow = 1;
            break;
        case MENU_SCRIPTS:
            scriptsOn = (item->Flags & CHECKED) != 0;
            ob_webview_set_scripts(view, scriptsOn);
            setStatus(scriptsOn ? "JavaScript on, from the next page (Reload to apply)." : "JavaScript off, from the next page (Reload to apply).");
            break;
        case MENU_PICTURES:
            picturesOn = (item->Flags & CHECKED) != 0;
            ob_webview_set_pictures(view, picturesOn);
            setStatus(picturesOn ? "Pictures on, from the next page (Reload to apply)." : "Pictures off, from the next page (Reload to apply).");
            break;
        case MENU_WEBFONTS:
            webFontsOn = (item->Flags & CHECKED) != 0;
            ob_webview_set_web_fonts(view, webFontsOn);
            setStatus(webFontsOn ? "Web fonts on, from the next page (Reload to apply)." : "Web fonts off, from the next page (Reload to apply).");
            break;
        case MENU_LITE:
            liteOn = (item->Flags & CHECKED) != 0;
            ob_webview_set_lite(view, liteOn);
            setStatus(liteOn ? "Lite: sites send their mobile pages (Reload to apply)." : "Lite off (Reload to apply).");
            break;
        }
        code = item->NextSelect;
    }
}

/* Started from Workbench: the icon's tool types set the page options from
 * the start (JAVASCRIPT=NO, PICTURES=NO, WEBFONTS=YES, LITE=YES). */
static void readToolTypes(int argc, char **argv)
{
    struct WBStartup *startup = (struct WBStartup *)argv;
    struct DiskObject *icon;
    BPTR oldDir;
    STRPTR value;
    if (argc != 0 || !startup || !startup->sm_NumArgs)
        return;
    IconBase = OpenLibrary((CONST_STRPTR)"icon.library", 37);
    if (!IconBase)
        return;
    oldDir = CurrentDir(startup->sm_ArgList[0].wa_Lock);
    icon = GetDiskObject(startup->sm_ArgList[0].wa_Name);
    CurrentDir(oldDir);
    if (icon) {
        if ((value = FindToolType((CONST_STRPTR *)icon->do_ToolTypes, (CONST_STRPTR)"JAVASCRIPT")) && MatchToolValue(value, (CONST_STRPTR)"NO"))
            scriptsOn = 0;
        if ((value = FindToolType((CONST_STRPTR *)icon->do_ToolTypes, (CONST_STRPTR)"PICTURES")) && MatchToolValue(value, (CONST_STRPTR)"NO"))
            picturesOn = 0;
        if ((value = FindToolType((CONST_STRPTR *)icon->do_ToolTypes, (CONST_STRPTR)"WEBFONTS")) && MatchToolValue(value, (CONST_STRPTR)"YES"))
            webFontsOn = 1;
        if ((value = FindToolType((CONST_STRPTR *)icon->do_ToolTypes, (CONST_STRPTR)"LITE")) && MatchToolValue(value, (CONST_STRPTR)"YES"))
            liteOn = 1;
        FreeDiskObject(icon);
    }
    CloseLibrary(IconBase);
    IconBase = NULL;
}

static void handleWindow(void)
{
    struct IntuiMessage *msg;
    while (window && (msg = GT_GetIMsg(window->UserPort))) {
        ULONG class = msg->Class;
        struct IntuiMessage copy = *msg;
        if (class == IDCMP_RAWKEY)
            handleKey(msg);
        GT_ReplyIMsg(msg);
        switch (class) {
        case IDCMP_CLOSEWINDOW:
            quitNow = 1;
            break;
        case IDCMP_GADGETUP:
            handleGadget((struct Gadget *)copy.IAddress);
            break;
        case IDCMP_MENUPICK:
            handleMenu(copy.Code);
            break;
        case IDCMP_MOUSEMOVE:
        case IDCMP_MOUSEBUTTONS:
            handleMouse(&copy);
            break;
        case IDCMP_NEWSIZE:
            relayout();
            break;
        case IDCMP_REFRESHWINDOW:
            GT_BeginRefresh(window);
            GT_EndRefresh(window, TRUE);
            paintPage(0, 0, pageWidth, pageHeight);
            break;
        case IDCMP_ACTIVEWINDOW:
            ob_webview_focus(view, 1);
            break;
        case IDCMP_INACTIVEWINDOW:
            ob_webview_focus(view, 0);
            break;
        }
    }
}

static int openLibraries(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 40);
    GadToolsBase = OpenLibrary((CONST_STRPTR)"gadtools.library", 39);
    KeymapBase = OpenLibrary((CONST_STRPTR)"keymap.library", 37);
    if (!IntuitionBase || !GfxBase || !GadToolsBase || !KeymapBase)
        return 0;
    timerPort = CreateMsgPort();
    timerRequest = timerPort ? (struct timerequest *)CreateIORequest(timerPort, sizeof(struct timerequest)) : NULL;
    if (!timerRequest || OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)timerRequest, 0))
        return 0;
    TimerBase = timerRequest->tr_node.io_Device;
    wakeSignal = AllocSignal(-1);
    return wakeSignal >= 0;
}

static void closeLibraries(void)
{
    if (timerPending) {
        AbortIO((struct IORequest *)timerRequest);
        WaitIO((struct IORequest *)timerRequest);
    }
    if (TimerBase)
        CloseDevice((struct IORequest *)timerRequest);
    if (timerRequest)
        DeleteIORequest((struct IORequest *)timerRequest);
    if (timerPort)
        DeleteMsgPort(timerPort);
    if (wakeSignal >= 0)
        FreeSignal(wakeSignal);
    if (KeymapBase) CloseLibrary(KeymapBase);
    if (GadToolsBase) CloseLibrary(GadToolsBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
}

static int browserMain(int argc, char **argv)
{
    OBWebViewCallbacks callbacks;
    int network, width, height;

    mainTask = FindTask(NULL);
    readToolTypes(argc, argv);
    ob_splash("Starting WebKit and the network", 75);
    if (!openLibraries()) {
        printf("OpenBrowser needs AmigaOS 3.1 or newer.\n");
        closeLibraries();
        return 20;
    }
    network = ob_webcore_init_with_network("PROGDIR:Cookies.db");
    if (!network && !ob_webcore_init()) {
        closeLibraries();
        return 20;
    }
    if (network)
        ob_webview_load_tls_sessions(TLS_SESSIONS);
    ob_webcore_set_wakeup(wakeUp, NULL);
    ob_splash("Opening the window", 90);

    screen = LockPubScreen(NULL);
    if (screen && GetBitMapAttr(screen->RastPort.BitMap, BMA_DEPTH) <= 8) {
        /* A native Workbench: pages look much better on a graphics card's
         * screen, so open one of our own there when the system has one. */
        ULONG mode = BestModeID(BIDTAG_NominalWidth, 800, BIDTAG_NominalHeight, 600, BIDTAG_Depth, 24, TAG_DONE);
        struct DimensionInfo dims;
        if (mode != INVALID_ID && GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof dims, DTAG_DIMS, mode) && dims.MaxDepth >= 15) {
            ownScreen = OpenScreenTags(NULL, SA_DisplayID, mode, SA_Depth, dims.MaxDepth >= 24 ? 24 : dims.MaxDepth,
                SA_Width, 800, SA_Height, 600, SA_Title, (ULONG)"OpenBrowser", SA_PubName, (ULONG)"OPENBROWSER",
                SA_LikeWorkbench, TRUE, SA_SharePens, TRUE, TAG_DONE);
            if (ownScreen) {
                PubScreenStatus(ownScreen, 0);
                UnlockPubScreen(NULL, screen);
                screen = LockPubScreen((CONST_STRPTR)"OPENBROWSER");
            }
        }
    }
    visualInfo = screen ? GetVisualInfo(screen, TAG_DONE) : NULL;
    if (!visualInfo) {
        if (screen)
            UnlockPubScreen(NULL, screen);
        if (ownScreen)
            CloseScreen(ownScreen);
        closeLibraries();
        return 20;
    }
    screenFont = screen->Font;
    fontHeight = screenFont->ta_YSize;
    width = ownScreen ? screen->Width : screen->Width > 820 ? 800 : screen->Width - 20;
    height = ownScreen ? screen->Height - screen->BarHeight - 1 : screen->Height > 640 ? 600 : screen->Height - screen->BarHeight - 20;
    window = OpenWindowTags(NULL, WA_Title, (ULONG)titleText, WA_ScreenTitle, (ULONG)(version + 6),
        WA_PubScreen, (ULONG)screen, WA_Left, ownScreen ? 0 : 10, WA_Top, screen->BarHeight + (ownScreen ? 1 : 2),
        WA_Width, width, WA_Height, height,
        WA_MinWidth, 320, WA_MinHeight, 200, WA_MaxWidth, -1, WA_MaxHeight, -1,
        WA_Flags, WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_SIZEGADGET | WFLG_SIZEBBOTTOM
            | WFLG_ACTIVATE | WFLG_REPORTMOUSE | WFLG_SMART_REFRESH,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_GADGETUP | IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY
            | IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW | IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW | IDCMP_MENUPICK
            | BUTTONIDCMP | STRINGIDCMP,
        WA_NewLookMenus, TRUE,
        TAG_DONE);
    ob_blit_open(screen);
    UnlockPubScreen(NULL, screen);
    if (!window || !makeGadgets() || !allocatePage()) {
        printf("OpenBrowser could not open its window.\n");
        quitNow = 1;
    }
    ob_splash(NULL, OB_SPLASH_CLOSE);

    memset(&callbacks, 0, sizeof callbacks);
    callbacks.title = onTitle;
    callbacks.url = onURL;
    callbacks.status = onStatus;
    callbacks.loading = onLoading;
    callbacks.failed = onFailed;
    callbacks.alert = onAlert;
    callbacks.confirm = onConfirm;
    callbacks.resource = onResource;
    view = ob_webview_create(pageWidth > 0 ? pageWidth : 640, pageHeight > 0 ? pageHeight : 400, &callbacks);
    ob_webview_set_scripts(view, scriptsOn);
    ob_webview_set_pictures(view, picturesOn);
    ob_webview_set_web_fonts(view, webFontsOn);
    ob_webview_set_lite(view, liteOn);
    if (!quitNow) {
        makeMenus();
        updateButtons();
        setStatus(network ? "Ready." : "No network: bsdsocket.library or AmiSSL is missing.");
        ob_webview_load(view, argc > 1 ? argv[1] : HOME_PAGE);
    }

    while (!quitNow) {
        ULONG waitMask, got;
        double next;

        ob_webcore_cycle();
        paintDirty();
        next = ob_webcore_next_timer();
        if (next == 0) {
            /* More is due at once: take any input, then go round again. */
            got = SetSignal(0, (1UL << window->UserPort->mp_SigBit) | (1UL << wakeSignal));
            if (got & (1UL << window->UserPort->mp_SigBit))
                handleWindow();
            continue;
        }
        armTimer(next);
        waitMask = (1UL << window->UserPort->mp_SigBit) | (1UL << wakeSignal) | (1UL << timerPort->mp_SigBit) | SIGBREAKF_CTRL_C;
        got = Wait(waitMask);
        if (got & SIGBREAKF_CTRL_C)
            quitNow = 1;
        if (got & (1UL << timerPort->mp_SigBit)) {
            if (timerPending && CheckIO((struct IORequest *)timerRequest)) {
                WaitIO((struct IORequest *)timerRequest);
                timerPending = 0;
            }
        }
        if (got & (1UL << window->UserPort->mp_SigBit))
            handleWindow();
    }

    ob_webview_destroy(view);
    if (window) {
        if (menuStrip)
            ClearMenuStrip(window);
        removeGadgets();
        CloseWindow(window);
    }
    ob_blit_close();
    if (visualInfo)
        FreeVisualInfo(visualInfo);
    if (ownScreen) {
        while (!CloseScreen(ownScreen))
            Delay(50); /* a visitor window is still open on it */
    }
    if (menuStrip)
        FreeMenus(menuStrip);
    if (pageBuffer)
        FreeVec(pageBuffer);
    if (network) {
        ob_webview_save_tls_sessions(TLS_SESSIONS);
        ob_webcore_shutdown();
    } else
        ob_webcore_stop_threads();
    closeLibraries();
    return 0;
}

int main(int argc, char **argv)
{
    /* Started from Workbench (no arguments at all): WebKit's own messages
     * go to a log in T:, not to a console window over the page. */
    if (argc == 0)
        freopen("T:OpenBrowser.log", "w", stderr);
    /* WebCore and JavaScriptCore recurse deeply; give them 2 MB of stack. */
    return oam_run_with_stack(2 * 1024 * 1024, browserMain, argc, argv);
}
