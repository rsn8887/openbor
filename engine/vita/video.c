/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * Licensed under a BSD-style license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2017 OpenBOR Team
 */

#include <stdlib.h>
#include <string.h>
#include <vita2d.h>
#include <source/gamelib/types.h>
#include <source/savedata.h>
#include "globals.h"
#include "video.h"

#include "shader.h"

bool vita2d_inited = false;
static vita2d_shader *vita2d_shaders[4];
static unsigned char vitaPalette[4 * 256];
static int vitaBrightness = 0;

static void setPalette(void);

void video_init(void) {

    vita2d_init();
    vita2d_set_vblank_wait(1);
    vita2d_texture_set_alloc_memblock_type(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW);

    memset(vitaPalette, 0, sizeof(vitaPalette));

    // texture
    vita2d_shaders[0] = vita2d_create_shader((SceGxmProgram *) texture_v, (SceGxmProgram *) texture_f);
    // lcd3x
    vita2d_shaders[1] = vita2d_create_shader((SceGxmProgram *) lcd3x_v, (SceGxmProgram *) lcd3x_f);
    // sharp + scanlines
    vita2d_shaders[2] = vita2d_create_shader((SceGxmProgram *) sharp_bilinear_v, (SceGxmProgram *) sharp_bilinear_f);
    // sharp
    vita2d_shaders[3] = vita2d_create_shader((SceGxmProgram *) sharp_bilinear_simple_v,
                                             (SceGxmProgram *) sharp_bilinear_simple_f);

    vita2d_inited = true;
}

void video_exit(void) {

    if (vita2d_inited) {
        // wait for the GPU to finish rendering so we can free everything
        vita2d_fini();

        for (int i = 0; i < 4; i++) {
            vita2d_free_shader(vita2d_shaders[i]);
        }
    }
}

int video_set_mode(s_videomodes videomodes) //(int width, int height, int bytes_per_pixel)
{
    printf("video_set_mode: %i x %i | scale: %f x %f | bpp: %i\n",
           videomodes.hRes, videomodes.vRes,
           videomodes.hScale, videomodes.vScale, videomodes.pixel);

    for (int i = 0; i < 10; i++) {
        vita2d_start_drawing();
        vita2d_clear_screen();
        vita2d_end_drawing();
        vita2d_wait_rendering_done();
        vita2d_swap_buffers();
    }

    return 1;
}

int video_copy_screen(s_screen *screen) {

    unsigned int texWidth = vita2d_texture_get_width(screen->texture),
            texHeight = vita2d_texture_get_height(screen->texture);

    // determine scale factor and on-screen dimensions
    float scaleFactor = 960.0f / texWidth;
    if (544.0 / texHeight < scaleFactor) scaleFactor = 544.0f / texHeight;

    // determine offsets
    float xOffset = (960.0f - texWidth * scaleFactor) / 2.0f;
    float yOffset = (544.0f - texHeight * scaleFactor) / 2.0f;

    // set filtering mode
    SceGxmTextureFilter filter = savedata.hwfilter ?
                                 SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;
    vita2d_texture_set_filters(screen->texture, filter, filter);

    vita2d_start_drawing();

    if (vitaBrightness < 0) {
        vita2d_draw_texture_tint_scale(screen->texture, xOffset, yOffset, scaleFactor, scaleFactor,
                                       (unsigned int) RGBA8(255, 255, 255, 255 + vitaBrightness));
    } else {
        vita2d_draw_texture_with_shader(vita2d_shaders[savedata.shader],
                                        screen->texture, xOffset, yOffset, scaleFactor, scaleFactor);
    }

    vita2d_end_drawing();
    vita2d_wait_rendering_done();
    vita2d_swap_buffers();

    return 1;
}

void video_set_color_correction(int gamma, int brightness) {
    // TODO gamma
    vitaBrightness = (brightness < -255) ? -255 : brightness;
}

static void setPalette(void) {
    // TODO setPalette
}

void vga_setpalette(unsigned char *pal) {

    printf("vga_setpalette\n");
    if (memcmp(pal, vitaPalette, (size_t) PAL_BYTES) != 0) {
        memcpy(vitaPalette, pal, (size_t) PAL_BYTES);
        setPalette();
    }
}

// no-op because this function is a useless DOS artifact
void vga_vwait(void) {
}

// not sure if this is useful for anything
void video_clearscreen(void) {
}
