#ifndef OB_IMAGE_DT_H
#define OB_IMAGE_DT_H

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
} ob_dt_image;

int ob_dt_open(void);
void ob_dt_close(void);
int ob_dt_image_load(ob_dt_image *image, const char *path,
                     struct Screen *screen, UWORD width, UWORD height);
void ob_dt_image_free(ob_dt_image *image);
int ob_dt_image_draw(const ob_dt_image *image, struct RastPort *rp,
                     WORD x, WORD y, UWORD width, UWORD height);

#endif
