/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

#include <png.h>
#include "types.h"
#include "screen.h"

#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h"

s_screen *pngToScreen(const void *data) {

    int w, h, comp;

    // Ryujinx doesn't like original png decoding code..
    // https://github.com/gdkchan/Ryujinx
    unsigned char *png = stbi_load_from_memory(data, 1024 * 1024 * 4, &w, &h, &comp, 4);
    if (png) {
        printf("pngToScreen: %i x %i (comp=%i)\n", w, h, comp);
        s_screen *image = allocscreen(w, h, PIXEL_32);
        memcpy(image->data, png, (size_t) (w * h * 4));
        stbi_image_free(png);
        return image;
    }

    return NULL;
}

#ifdef SDL

SDL_Surface *pngToSurface(const void *data) {
    unsigned char *sp;
    char *dp;
    int width, height, linew;
    int h;
    SDL_Surface *ds = NULL;
    s_screen *src = pngToScreen(data);

    if (src == NULL) {
        return NULL;
    }
    assert(src->pixelformat == PIXEL_32);

    width = src->width;
    height = src->height;
    h = height;

    sp = (unsigned char *) src->data;
    ds = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, 0xff, 0xff00, 0xff0000, 0xff000000);
    dp = ds->pixels;

    linew = width * 4;

    do {
        memcpy(dp, sp, linew);
        sp += linew;
        dp += ds->pitch;
    } while (--h);

    freescreen(&src);

    return ds;
}

#endif
