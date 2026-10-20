#ifndef WL_IMAGE_DT_H
#define WL_IMAGE_DT_H

#include <exec/types.h>
#include <graphics/gfx.h>

struct RastPort;
struct Screen;

typedef struct {
    APTR object;
    struct BitMap *bitmap;
    PLANEPTR mask;
    UWORD source_width;
    UWORD source_height;
    UWORD width;
    UWORD height;
} wl_dt_image;

int wl_dt_open(void);
void wl_dt_close(void);
int wl_dt_image_load(wl_dt_image *image, const char *path,
                     struct Screen *screen, UWORD width, UWORD height);
void wl_dt_image_free(wl_dt_image *image);
int wl_dt_image_draw(const wl_dt_image *image, struct RastPort *rp,
                     WORD x, WORD y, UWORD width, UWORD height);

#endif
