/*
 * obcore-view: load a page into an OpenBrowser view without a window, run
 * WebCore until the page has loaded, then paint the view into a 32-bit
 * buffer and save it as PNG. A test of the browser's page layer (frame
 * loader, chrome and editor clients, run loop, painting) from C.
 *
 *   obcore-view <file.html> [width height] [page.png]
 *   obcore-view -url <address> [width height] [page.png]
 *
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <cairo.h>
#include <proto/dos.h>

#include "ob_webview.h"
#include "oam_stack.h"

static const char version[] __attribute__((used)) = "$VER: obcore-view 0.1 (4.10.2026)";

static int loading = -1;   /* -1 not started yet, 1 loading, 0 done */
static int invalidations;
static long busyCycles, waits;  /* run loop turns with work waiting, and waits */
static time_t startTime;        /* for the seconds in OBVIEW_CREATED and OBVIEW_RAN */

static void onInvalidate(void *context, int x, int y, int w, int h)
{
    (void)context; (void)x; (void)y; (void)w; (void)h;
    invalidations++;
}

static void onTitle(void *context, const char *title)
{
    (void)context;
    printf("OBVIEW_TITLE %s\n", title);
}

static void onURL(void *context, const char *url)
{
    (void)context;
    printf("OBVIEW_URL %s\n", url);
}

static void onLoading(void *context, int isLoading, int percent)
{
    (void)context;
    if (isLoading != loading || !isLoading)
        printf("OBVIEW_LOADING %d %d%%\n", isLoading, percent);
    loading = isLoading;
}

static void onFailed(void *context, const char *url, const char *description)
{
    (void)context;
    printf("OBVIEW_FAILED %s: %s\n", url, description);
}

static void onAlert(void *context, const char *message)
{
    (void)context;
    printf("OBVIEW_ALERT %s\n", message);
}

static void onConsole(void *context, const char *message, int line, const char *source)
{
    (void)context;
    printf("OBVIEW_CONSOLE %s (%s:%d)\n", message, source, line);
}

static void onResource(void *context, const char *url, int started, const char *error)
{
    (void)context;
    printf("OBVIEW_%s %.200s%s%s\n", started ? "REQUEST" : "LOADED", url, error ? " FAILED: " : "", error ? error : "");
    fflush(stdout);
}

static char *readFile(const char *name)
{
    FILE *f = fopen(name, "rb");
    long size;
    char *text;
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    text = malloc(size + 1);
    if (text && fread(text, 1, size, f) == (size_t)size)
        text[size] = 0;
    else {
        free(text);
        text = NULL;
    }
    fclose(f);
    return text;
}

/* Runs WebCore's run loop for up to `seconds`, or until the page is done
 * and nothing is due within 50 ms, saying every 15 seconds how it goes. */
static void run(double seconds)
{
    time_t start = time(NULL), lastTick = start;
    while (difftime(time(NULL), start) < seconds) {
        double next;
        ob_webcore_cycle();
        next = ob_webcore_next_timer();
        if (loading == 0 && (next < 0 || next > 0.05))
            break;
        if (next != 0) {
            Delay(next < 0 || next > 0.2 ? 10 : (int)(next * 50) + 1);
            waits++;
        } else
            busyCycles++;
        if (difftime(time(NULL), lastTick) >= 15) {
            lastTick = time(NULL);
            printf("OBVIEW_TICK %lds loading=%d cycles=%ld waits=%ld\n", (long)(lastTick - start), loading, busyCycles, waits);
            fflush(stdout);
        }
    }
}

/* Intuition raw key codes of a US keyboard, for the input test. */
static int rawKeyFor(char c, int *shift)
{
    static const char *rows[] = { "1234567890-=", "qwertyuiop[]", "asdfghjkl;'", "zxcvbnm,./" };
    static const char *shifted[] = { "!@#$%^&*()_+", "QWERTYUIOP{}", "ASDFGHJKL:\"", "ZXCVBNM<>?" };
    static const int first[] = { 0x01, 0x10, 0x20, 0x31 };
    int row;
    *shift = 0;
    if (c == ' ')
        return 0x40;
    for (row = 0; row < 4; row++) {
        const char *at = strchr(rows[row], c);
        if (!at && (at = strchr(shifted[row], c)))
            *shift = OB_QUAL_SHIFT;
        if (at)
            return first[row] + (int)(at - (*shift ? shifted[row] : rows[row]));
    }
    return -1;
}

/* Clicks at (x, y) on the page, as the window does with the left button. */
static void click(OBWebView *view, int x, int y)
{
    ob_webview_mouse(view, OB_MOUSE_MOVE, x, y, OB_BUTTON_NONE, 0, 0);
    ob_webview_mouse(view, OB_MOUSE_DOWN, x, y, OB_BUTTON_LEFT, 0, 1);
    ob_webview_mouse(view, OB_MOUSE_UP, x, y, OB_BUTTON_LEFT, 0, 1);
}

/* Types text a key at a time, each key down and up, as the window does. */
static void typeText(OBWebView *view, const char *text)
{
    for (; *text; text++) {
        char one[2] = { *text, 0 };
        int shift, raw = rawKeyFor(*text, &shift);
        if (raw < 0)
            continue;
        ob_webview_key(view, 1, raw, one, shift);
        ob_webview_key(view, 0, raw, "", shift);
        ob_webcore_cycle();
    }
}

static int viewMain(int argc, char **argv)
{
    OBWebViewCallbacks callbacks = { 0 };
    OBWebView *view;
    int width = 800, height = 600, argi = 1, isURL = 0, inputTest = 0;
    double seconds = 120.0;
    const char *source, *png = NULL;
    unsigned char *pixels;
    char *html = NULL;

    if (argc > argi + 1 && !strcmp(argv[argi], "-wait")) {
        seconds = atof(argv[argi + 1]);
        argi += 2;
    }
    if (argc > argi && !strcmp(argv[argi], "-input")) {
        /* After the load: click at (30, 35), type an address, click at (30, 110). */
        inputTest = 1;
        argi++;
    }
    if (argc > argi && !strcmp(argv[argi], "-url")) {
        isURL = 1;
        argi++;
    }
    if (argc <= argi) {
        printf("usage: obcore-view [-wait seconds] [-input] [-url] <file.html|address> [width height] [page.png]\n");
        return 10;
    }
    source = argv[argi++];
    if (argc > argi + 1) {
        width = atoi(argv[argi]);
        height = atoi(argv[argi + 1]);
        argi += 2;
    }
    if (argc > argi)
        png = argv[argi];
    if (width < 100 || height < 100) {
        width = 800;
        height = 600;
    }
    if (!isURL && !(html = readFile(source))) {
        printf("OBVIEW_FAIL cannot read %s\n", source);
        return 20;
    }

    if (isURL ? !ob_webcore_init_with_network(":memory:") : !ob_webcore_init()) {
        printf("OBVIEW_FAIL init%s\n", isURL ? " (bsdsocket.library or AmiSSL)" : "");
        return 20;
    }
    callbacks.invalidate = onInvalidate;
    callbacks.title = onTitle;
    callbacks.url = onURL;
    callbacks.loading = onLoading;
    callbacks.failed = onFailed;
    callbacks.alert = onAlert;
    callbacks.console = onConsole;
    callbacks.resource = onResource;
    view = ob_webview_create(width, height, &callbacks);
    printf("OBVIEW_CREATED %dx%d after %lds\n", width, height, (long)(time(NULL) - startTime));
    fflush(stdout);

    if (isURL)
        ob_webview_load(view, source);
    else
        ob_webview_load_html(view, html, "file:///page.html");
    run(seconds);
    run(1.0); /* rendering updates and timers the load left behind */
    if (inputTest) {
        ob_webview_focus(view, 1);
        click(view, 30, 35);
        run(1.0);
        typeText(view, "someone@example.com");
        run(1.0);
        printf("OBVIEW_TYPED\n");
        click(view, 30, 110);
        run(3.0);
    }
    printf("OBVIEW_RAN loading=%d invalidations=%d cycles=%ld waits=%ld at %lds\n", loading, invalidations, busyCycles, waits,
        (long)(time(NULL) - startTime));
    fflush(stdout);

    pixels = calloc((size_t)width * height, 4);
    if (pixels) {
        ob_webview_paint(view, pixels, width * 4, 0, 0, width, height);
        printf("OBVIEW_PAINTED %dx%d\n", width, height);
        if (png) {
            cairo_surface_t *surface = cairo_image_surface_create_for_data(pixels, CAIRO_FORMAT_ARGB32, width, height, width * 4);
            cairo_status_t status = cairo_surface_write_to_png(surface, png);
            printf("OBVIEW_PNG %s %s\n", png, cairo_status_to_string(status));
            cairo_surface_destroy(surface);
        }
        free(pixels);
    }
    ob_webview_destroy(view);
    if (isURL)
        ob_webcore_shutdown();
    free(html);
    printf("OBVIEW_DONE\n");
    fflush(stdout);
    return 0;
}

int main(int argc, char **argv)
{
    startTime = time(NULL);
    return oam_run_with_stack(2 * 1024 * 1024, viewMain, argc, argv);
}
