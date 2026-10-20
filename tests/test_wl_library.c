/*
 * WebLayoutLibTest: weblayout.library from LIBS:, as a program uses it.
 *
 *   WebLayoutLibTest
 *
 * Opens weblayout.library 1 (which opens openlayout.library 2), then runs
 * its calls on a small page laid out and drawn into an off-screen RastPort
 * with topaz 8: title, stylesheet and picture resources, layout, drawing,
 * the link under a point, the resolver, plain text, and 50 pages made and
 * disposed of to see the memory come back. Prints WEBLAYOUTLIB PASS, or what
 * failed and returns 10.
 *
 * Built by scripts/build-weblayout-library.sh.
 * MIT, Copyright (c) 2026 AmigaChrome Team.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include <stdio.h>
#include <string.h>

#include "weblayout.h"
#include <proto/weblayout.h>

struct Library *WebLayoutBase = NULL;

static int failed;

#define CHECK(cond) do { if (!(cond)) { printf("WEBLAYOUTLIB FAIL line %d: %s\n", __LINE__, #cond); failed = 1; } } while (0)

static const char page[] =
    "<html><head><title>weblayout &amp; the library</title>"
    "<link rel=\"stylesheet\" href=\"style.css\"></head><body>"
    "<h1>Heading</h1>"
    "<p>A paragraph with <a href=\"../next.html\">a link</a> in it, and "
    "<b>bold</b> text: caf\xc3\xa9 \xe2\x80\x9cquoted\xe2\x80\x9d.</p>"
    "<p><img src=\"/pics/one.png\" width=\"32\" height=\"16\" alt=\"one\"></p>"
    "<ul><li>first</li><li>second</li></ul>"
    "</body></html>";

static const char sheet[] = "h1 { color: #800000; } p { padding-left: 4px; }";

static void check_resolve(CONST_STRPTR base, CONST_STRPTR href, const char *want)
{
    char out[200];
    BOOL ok;
    ok = WL_ResolveURL(base, href, (STRPTR)out, sizeof out);
    if (!ok || strcmp(out, want)) {
        printf("WEBLAYOUTLIB FAIL resolve %s + %s: %s, want %s\n",
               (const char *)base, (const char *)href, out, want);
        failed = 1;
    }
}

/* one page made, laid out, drawn and disposed of */
static BOOL one_page(struct WLView *view)
{
    WLDocument *doc;
    BOOL ok;
    doc = WL_NewDocument((CONST_STRPTR)page, (ULONG)strlen(page), WLKIND_HTML,
                         (CONST_STRPTR)"https://example.com/dir/page.html");
    if (!doc) return FALSE;
    ok = WL_AddStyleSheet(doc, (CONST_STRPTR)sheet, (ULONG)strlen(sheet));
    if (ok) ok = WL_Layout(doc, view);
    if (ok) WL_Draw(doc, view);
    WL_DisposeDocument(doc);
    return ok;
}

int main(void)
{
    struct TextAttr topaz = { (STRPTR)"topaz.font", 8, FS_NORMAL, FPF_ROMFONT };
    struct TextFont *font = NULL;
    struct BitMap *bm = NULL;
    struct RastPort rp;
    struct WLView view;
    struct WLResource res;
    WLDocument *doc;
    CONST_STRPTR title, href = NULL;
    ULONG n, inked = 0, before, after;
    LONG height;
    WORD x, y;
    int i;

    WebLayoutBase = OpenLibrary((CONST_STRPTR)WEBLAYOUT_NAME, WEBLAYOUT_VERSION);
    if (!WebLayoutBase) {
        printf("WEBLAYOUTLIB FAIL: no weblayout.library %d in LIBS: (or no openlayout.library 2)\n",
               WEBLAYOUT_VERSION);
        return 20;
    }
    printf("weblayout.library %u.%u: %s", (unsigned)WebLayoutBase->lib_Version,
           (unsigned)WebLayoutBase->lib_Revision,
           WebLayoutBase->lib_IdString ? (const char *)WebLayoutBase->lib_IdString : "\n");
    CHECK(WL_Open());

    check_resolve((CONST_STRPTR)"https://example.com/a/b.html", (CONST_STRPTR)"c.css", "https://example.com/a/c.css");
    check_resolve((CONST_STRPTR)"https://example.com/a/b.html", (CONST_STRPTR)"../up/x.png", "https://example.com/up/x.png");
    check_resolve((CONST_STRPTR)"example.com/a/", (CONST_STRPTR)"/root", "http://example.com/root");

    font = OpenFont(&topaz);
    bm = AllocBitMap(320, 400, 3, BMF_CLEAR, NULL);
    if (!font || !bm) {
        printf("WEBLAYOUTLIB FAIL: no topaz 8 or no memory for the bitmap\n");
        if (bm) FreeBitMap(bm);
        if (font) CloseFont(font);
        WL_Close();
        CloseLibrary(WebLayoutBase);
        return 20;
    }
    InitRastPort(&rp);
    rp.BitMap = bm;
    SetFont(&rp, font);

    memset(&view, 0, sizeof view);
    view.wv_Size = sizeof view;
    view.wv_RPort = &rp;
    view.wv_Font = font;
    view.wv_Left = 4;
    view.wv_Top = 4;
    view.wv_Width = 300;
    view.wv_Height = 380;
    view.wv_TextPen = 1;
    view.wv_BackPen = 0;
    view.wv_LinkPen = 3;
    view.wv_FillPen = 2;

    doc = WL_NewDocument((CONST_STRPTR)page, (ULONG)strlen(page), WLKIND_HTML,
                         (CONST_STRPTR)"https://example.com/dir/page.html");
    CHECK(doc != NULL);
    if (doc) {
        title = WL_GetTitle(doc);
        CHECK(!strcmp((const char *)title, "weblayout & the library"));
        n = WL_CountResources(doc, WLRES_STYLESHEET);
        CHECK(n == 1);
        res.wr_Size = sizeof res;
        CHECK(WL_GetResource(doc, WLRES_STYLESHEET, 0, &res));
        CHECK(res.wr_URL && !strcmp((const char *)res.wr_URL, "https://example.com/dir/style.css"));
        n = WL_CountResources(doc, WLRES_IMAGE);
        CHECK(n == 0);                                   /* pictures after layout */
        CHECK(WL_AddStyleSheet(doc, (CONST_STRPTR)sheet, (ULONG)strlen(sheet)));

        CHECK(WL_Layout(doc, &view));
        height = WL_ContentHeight(doc);
        CHECK(height > 40);
        n = WL_CountResources(doc, WLRES_IMAGE);
        CHECK(n == 1);
        res.wr_Size = sizeof res;
        CHECK(WL_GetResource(doc, WLRES_IMAGE, 0, &res));
        CHECK(res.wr_URL && !strcmp((const char *)res.wr_URL, "https://example.com/pics/one.png"));
        CHECK(res.wr_Width == 32 && res.wr_Height == 16);
        CHECK(!WL_SetImageFile(doc, res.wr_ID, (CONST_STRPTR)"NIL:", NULL));   /* no screen */

        WL_Draw(doc, &view);
        for (y = 0; y < 400; y += 2)
            for (x = 0; x < 320; x += 2)
                if (ReadPixel(&rp, x, y) != 0) inked++;
        CHECK(inked > 50);

        for (y = view.wv_Top; y < view.wv_Top + view.wv_Height && !href; y += 2)
            for (x = view.wv_Left; x < view.wv_Left + view.wv_Width && !href; x += 2)
                href = WL_HitLink(doc, &view, x, y);
        CHECK(href && !strcmp((const char *)href, "https://example.com/next.html"));
        CHECK(WL_HitLink(doc, &view, 0, 0) == NULL);     /* outside the page's area */
        printf("page: \"%s\", %ld px high, %lu pixels drawn, link %s\n",
               (const char *)title, (long)height, (unsigned long)inked,
               href ? (const char *)href : "(none)");
        WL_DisposeDocument(doc);
    }

    doc = WL_NewDocument((CONST_STRPTR)"plain\ntext", 10, WLKIND_TEXT, NULL);
    CHECK(doc != NULL);
    if (doc) {
        CHECK(WL_Layout(doc, &view));
        height = WL_ContentHeight(doc);
        CHECK(height > 0);
        CHECK(WL_CountResources(doc, WLRES_STYLESHEET) == 0);
        WL_Draw(doc, &view);
        WL_DisposeDocument(doc);
    }
    CHECK(WL_NewDocument((CONST_STRPTR)"x", 1, 99, NULL) == NULL);   /* no such kind */
    WL_Draw(NULL, &view);                                              /* clears the area */
    WL_DisposeDocument(NULL);

    /* the memory comes back: a page's pool goes with it */
    CHECK(one_page(&view));
    before = AvailMem(MEMF_ANY);
    for (i = 0; i < 50; i++)
        if (!one_page(&view)) { CHECK(!"page made"); break; }
    after = AvailMem(MEMF_ANY);
    printf("memory: %lu bytes free before 50 pages, %lu after\n",
           (unsigned long)before, (unsigned long)after);
    CHECK(after + 16384 >= before);

    FreeBitMap(bm);
    CloseFont(font);
    WL_Close();
    CloseLibrary(WebLayoutBase);
    printf("WEBLAYOUTLIB %s\n", failed ? "FAIL" : "PASS");
    return failed ? 10 : 0;
}
