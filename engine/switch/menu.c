/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */
#include <unistd.h>
#include "SDL.h"
#include "sdlport.h"
#include "video.h"
#include "openbor.h"
#include "hankaku.h"

#include "pngdec.h"
#include "../resources/OpenBOR_Logo_480x272_png.h"
#include "../resources/OpenBOR_Menu_480x272_png.h"

#include <dirent.h>

extern s_videomodes videomodes;
extern s_screen *vscreen;
extern int nativeHeight;
extern int nativeWidth;
static s_screen *bgscreen;

#define RGB32(R, G, B) ((R) | ((G) << 8) | ((B) << 16))
#define RGB16(R, G, B) ((B&0xF8)<<8) | ((G&0xFC)<<3) | (R>>3)
#define RGB(R, G, B)   (bpp==16?RGB16(R,G,B):RGB32(R,G,B))

#define BLACK        RGB(  0,   0,   0)
#define WHITE        RGB(255, 255, 255)
#define RED            RGB(255,   0,   0)
#define    GREEN        RGB(  0, 255,   0)
#define BLUE        RGB(  0,   0, 255)
#define YELLOW        RGB(255, 255,   0)
#define PURPLE        RGB(255,   0, 255)
#define ORANGE        RGB(255, 128,   0)
#define GRAY        RGB(112, 128, 144)
#define LIGHT_GRAY  RGB(223, 223, 223)
#define DARK_RED    RGB(128,   0,   0)
#define DARK_GREEN    RGB(  0, 128,   0)
#define DARK_BLUE    RGB(  0,   0, 128)

static int bpp = 32;
static int isWide = 1;
static int isFull = 1;
static int dListTotal;
static int dListCurrentPosition;
static int dListScrollPosition;
static fileliststruct *filelist;
extern const s_drawmethod plainmethod;

typedef int (*ControlInput)();

static int ControlMenu();

static ControlInput pControl;

static int Control() {
    return pControl();
}

static void sortList() {
    int i, j;
    fileliststruct temp;
    if (dListTotal < 2) return;
    for (j = dListTotal - 1; j > 0; j--) {
        for (i = 0; i < j; i++) {
            if (stricmp(filelist[i].filename, filelist[i + 1].filename) > 0) {
                temp = filelist[i];
                filelist[i] = filelist[i + 1];
                filelist[i + 1] = temp;
            }
        }
    }
}

static int findPaks(void) {
    int i = 0;
    DIR *dp = NULL;
    struct dirent *ds;
    dp = opendir(paksDir);
    if (dp != NULL) {
        while ((ds = readdir(dp)) != NULL) {
            if (packfile_supported(ds->d_name)) {
                fileliststruct *copy = NULL;
                if (filelist == NULL) filelist = malloc(sizeof(fileliststruct));
                else {
                    copy = malloc(i * sizeof(fileliststruct));
                    memcpy(copy, filelist, i * sizeof(fileliststruct));
                    free(filelist);
                    filelist = malloc((i + 1) * sizeof(fileliststruct));
                    memcpy(filelist, copy, i * sizeof(fileliststruct));
                    free(copy);
                    copy = NULL;
                }
                memset(&filelist[i], 0, sizeof(fileliststruct));
                strncpy(filelist[i].filename, ds->d_name, strlen(ds->d_name));
                i++;
            }
        }
        closedir(dp);
    }
    return i;
}

static void printText(int x, int y, int col, int backcol, int fill, char *format, ...) {
    int x1, y1, i;
    u32 data;
    u16 *line16 = NULL;
    u32 *line32 = NULL;
    u8 *font;
    u8 ch = 0;
    char buf[128] = {""};
    int pitch = vscreen->width * bpp / 8;
    va_list arglist;
    va_start(arglist, format);
    vsprintf(buf, format, arglist);
    va_end(arglist);

    for (i = 0; i < sizeof(buf); i++) {
        ch = (u8) buf[i];
        // mapping
        if (ch < 0x20) ch = 0;
        else if (ch < 0x80) { ch -= 0x20; }
        else if (ch < 0xa0) { ch = 0; }
        else ch -= 0x40;
        font = (u8 *) &hankaku_font10[ch * 10];
        // draw
        if (bpp == 16) line16 = (u16 *) (vscreen->data + x * 2 + y * pitch);
        else line32 = (u32 *) (vscreen->data + x * 4 + y * pitch);

        for (y1 = 0; y1 < 10; y1++) {
            data = *font++;
            for (x1 = 0; x1 < 5; x1++) {
                if (data & 1) {
                    if (bpp == 16) *line16 = col;
                    else *line32 = col;
                } else if (fill) {
                    if (bpp == 16) *line16 = backcol;
                    else *line32 = backcol;
                }

                if (bpp == 16) line16++;
                else line32++;

                data = data >> 1;
            }
            if (bpp == 16) line16 += pitch / 2 - 5;
            else line32 += pitch / 4 - 5;
        }
        x += 5;
    }
}

static s_screen *getPreview(char *filename) {
    s_screen *title = NULL;
    s_screen *scale = NULL;
    // Grab current path and filename
    getBasePath(packfile, filename, 1);
    // Create & Load & Scale Image
    if (!loadscreen("data/bgs/title", packfile, NULL, PIXEL_x8, &title)) return NULL;
    scale = allocscreen(160, 120, PIXEL_x8);
    scalescreen(scale, title);
    memcpy(scale->palette, title->palette, PAL_BYTES);
    // ScreenShots within Menu will be saved as "Menu"
    strncpy(packfile, "Menu.xxx", 128);
    freescreen(&title);
    return scale;
}

static unsigned int buttonsHeld = 0;
static unsigned int buttonsPressed = 0;

static void refreshInput() {

    hidScanInput();

    // TODO
    /*
    // read left analog stick
    if (pad.ly <= 0x30) pad.buttons |= SCE_CTRL_UP;
    if (pad.ly >= 0xC0) pad.buttons |= SCE_CTRL_DOWN;
    if (pad.lx <= 0x30) pad.buttons |= SCE_CTRL_LEFT;
    if (pad.lx >= 0xC0) pad.buttons |= SCE_CTRL_RIGHT;
    */

    // update buttons pressed (not held)
    u64 key = hidKeysHeld(CONTROLLER_P1_AUTO);
    buttonsPressed = (unsigned int) (key & ~buttonsHeld);
    buttonsHeld = (unsigned int) key;
}

static int ControlMenu() {

    int status = -1;
    int dListMaxDisplay = 17;
    refreshInput();

    switch (buttonsPressed) {

        case KEY_DUP:
            dListScrollPosition--;
            if (dListScrollPosition < 0) {
                dListScrollPosition = 0;
                dListCurrentPosition--;
            }
            if (dListCurrentPosition < 0) dListCurrentPosition = 0;
            break;

        case KEY_DDOWN:
            dListCurrentPosition++;
            if (dListCurrentPosition > dListTotal - 1) dListCurrentPosition = dListTotal - 1;
            if (dListCurrentPosition > dListMaxDisplay) {
                if ((dListCurrentPosition + dListScrollPosition) < dListTotal) dListScrollPosition++;
                dListCurrentPosition = dListMaxDisplay;
            }
            break;

        case KEY_A:
            // Start Engine!
            status = 1;
            break;

        case KEY_MINUS:
            // Exit Engine!
            status = 2;
            break;

        default:
            // No Update Needed!
            status = 0;
            break;
    }
    return status;
}

static void initMenu(int type) {

    pixelformat = PIXEL_x8;

    savedata.fullscreen = isFull;
    video_stretch(savedata.stretch);
    videomodes.hRes = 480;
    videomodes.vRes = 272;
    videomodes.pixel = 4;

    vscreen = allocscreen(videomodes.hRes, videomodes.vRes, PIXEL_32);

    video_set_mode(videomodes);

    // Read Logo or Menu from Array.
    if (!type)
        bgscreen = pngToScreen((void *) openbor_logo_480x272_png.data);
    else
        bgscreen = pngToScreen((void *) openbor_menu_480x272_png.data);
}

static void termMenu() {

    videomodes.hRes = videomodes.vRes = 0;
    video_set_mode(videomodes);

    if (bgscreen) freescreen(&bgscreen);
    if (vscreen) freescreen(&vscreen);
}

static void drawMenu() {

    char listing[45] = {""};
    int list = 0;
    int shift = 0;
    int colors = 0;
    s_screen *Image = NULL;

    putscreen(vscreen, bgscreen, 0, 0, NULL);
    if (dListTotal < 1) printText((isWide ? 30 : 8), (isWide ? 33 : 24), RED, 0, 0, "No Mods In Paks Folder!");
    for (list = 0; list < dListTotal; list++) {
        if (list < 18) {
            shift = 0;
            colors = GRAY;
            strncpy(listing, "", (isWide ? 44 : 28));
            if (strlen(filelist[list + dListScrollPosition].filename) - 4 < (isWide ? 44 : 28))
                strncpy(listing, filelist[list + dListScrollPosition].filename,
                        strlen(filelist[list + dListScrollPosition].filename) - 4);
            if (strlen(filelist[list + dListScrollPosition].filename) - 4 > (isWide ? 44 : 28))
                strncpy(listing, filelist[list + dListScrollPosition].filename, (isWide ? 44 : 28));
            if (list == dListCurrentPosition) {
                shift = 2;
                colors = RED;
                Image = getPreview(filelist[list + dListScrollPosition].filename);

            }
            printText((isWide ? 30 : 7) + shift, (isWide ? 33 : 22) + (11 * list), colors, 0, 0, "%s", listing);
        }
    }

    printText((isWide ? 26 : 5), (isWide ? 11 : 4), WHITE, 0, 0, "OpenBoR %s", VERSION);
    printText((isWide ? 392 : 261), (isWide ? 11 : 4), WHITE, 0, 0, __DATE__);
    printText((isWide ? 23 : 4), (isWide ? 251 : 226), WHITE, 0, 0, "(A): Start Game");
    printText((isWide ? 390 : 244), (isWide ? 251 : 226), WHITE, 0, 0, "(-): Quit");
    printText((isWide ? 320 : 188), (isWide ? 175 : 158), BLACK, 0, 0, "www.chronocrash.com");
    printText((isWide ? 322 : 190), (isWide ? 185 : 168), BLACK, 0, 0, "www.SenileTeam.com");

#ifdef SPK_SUPPORTED
    printText((isWide ? 324 : 192),(isWide ? 191 : 176), DARK_RED, 0, 0, "SecurePAK Edition");
#endif

    if (Image) {
        putscreen(vscreen, Image, isWide ? 286 : 155, isWide ? 32 : 21, NULL);
        freescreen(&Image);
    } else
        printText((isWide ? 288 : 157), (isWide ? 141 : 130), RED, 0, 0, "No Preview Available!");

    video_copy_screen(vscreen);
}

static void drawLogo() {
    if (savedata.logo) return;
    initMenu(0);
    video_copy_screen(bgscreen);
    SDL_Delay(3000);
    termMenu();
}

int Menu() {

    int done = 0, ret = 1;
    int ctrl = 0;

    loadsettings();
    drawLogo();

    dListCurrentPosition = 0;

    if ((dListTotal = findPaks()) != 1) {

        sortList();
        initMenu(1);
        drawMenu();

        pControl = ControlMenu;

        while (!done) {

            // TODO: used in Ryu.. emu
            // drawMenu();

            ctrl = Control();
            switch (ctrl) {
                case 1:
                    done = 1;
                    break;

                case 2:
                    done = 1;
                    ret = 0;
                    break;

                case -1:
                    drawMenu();
                    break;

                default:
                    break;
            }
        }

        termMenu();
    }

    getBasePath(packfile, filelist[dListCurrentPosition + dListScrollPosition].filename, 1);
    if (filelist) {
        free(filelist);
        filelist = NULL;
    }
    pixelformat = PIXEL_x8;

    // TODO: return continue / exit
    return ret;
}
