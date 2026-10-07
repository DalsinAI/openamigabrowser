#include "ob_image_dt.h"

#include <intuition/screens.h>
#include <proto/intuition.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    struct Screen *screen;
    ob_dt_image image = {0};
    int ok;

    if (argc < 2) {
        puts("DTIMAGE usage: DTImageTest <picture>");
        return 10;
    }
    if (!ob_dt_open()) {
        puts("DTIMAGE FAIL datatypes.library");
        return 20;
    }
    screen = LockPubScreen(NULL);
    if (!screen) {
        puts("DTIMAGE FAIL public-screen");
        ob_dt_close();
        return 20;
    }

    ok = ob_dt_image_load(&image, argv[1], screen, 80, 60);
    if (ok)
        printf("DTIMAGE PASS source=%ux%u scaled=%ux%u mask=%s\n",
               image.source_width, image.source_height,
               image.width, image.height, image.mask ? "yes" : "no");
    else
        puts("DTIMAGE FAIL load");

    ob_dt_image_free(&image);
    UnlockPubScreen(NULL, screen);
    ob_dt_close();
    return ok ? 0 : 20;
}
