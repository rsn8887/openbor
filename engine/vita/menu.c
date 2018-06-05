/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * Licensed under a BSD-style license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2017 OpenBOR Team
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <psp2/ctrl.h>
#include <psp2/io/dirent.h>
#include <psp2/kernel/threadmgr.h>
#include <source/pnglib/savepng.h>
#include <source/gamelib/types.h>
#include "vitaport.h"
#include "video.h"
#include "types.h"
#include "utils.h"
#include "packfile.h"
#include "hankaku.h"
#include "stristr.h"
#include "screen.h"
#include "loadimg.h"
#include "timer.h"
#include "version.h"

#include "pngdec.h"
#include "../resources/OpenBOR_Logo_480x272_png.h"
#include "../resources/OpenBOR_Menu_480x272_Sony_png.h"

#define RGB32(R, G, B) ((B << 16) | ((G) << 8) | (R))
#define RGB(R, G, B)   RGB32(R,G,B)

#define BLACK        RGB(  0,   0,   0)
#define WHITE        RGB(255, 255, 255)
#define RED            RGB(255,   0,   0)
#define GRAY        RGB(112, 128, 144)
#define DARK_RED    RGB(128,   0,   0)

#define LOG_SCREEN_TOP 2
#define LOG_SCREEN_END 26

#define INPUT_DELAY (1000 * 150)

typedef struct {
    stringptr *buf;
    int *pos;
    int line;
    int rows;
    char ready;
} s_logfile;

static s_screen *Source = NULL;
static s_screen *Screen = NULL;

static int which_logfile = OPENBOR_LOG;
static unsigned int buttonsHeld = 0;
static unsigned int buttonsPressed = 0;
static fileliststruct *filelist;
static s_logfile logfile[2];

static int list_selection = 0;
static int list_count = 0;
static int list_max = 18;

typedef int (*ControlInput)();

static ControlInput pControl;

static int ControlMenu();

static int Control() {
    return pControl();
}

static void refreshInput() {
    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));
    sceCtrlPeekBufferPositive(0, &pad, 1);

    // read left analog stick
    if (pad.ly <= 0x30) pad.buttons |= SCE_CTRL_UP;
    if (pad.ly >= 0xC0) pad.buttons |= SCE_CTRL_DOWN;
    if (pad.lx <= 0x30) pad.buttons |= SCE_CTRL_LEFT;
    if (pad.lx >= 0xC0) pad.buttons |= SCE_CTRL_RIGHT;

    // update buttons pressed (not held)
    buttonsPressed = pad.buttons & ~buttonsHeld;
    buttonsHeld = pad.buttons;
}

static void getAllLogs() {
    int i, j, k;
    for (i = 0; i < 2; i++) {
        logfile[i].buf = readFromLogFile(i);
        if (logfile[i].buf != NULL) {
            logfile[i].pos = malloc(++logfile[i].rows * sizeof(int));
            if (logfile[i].pos == NULL) return;
            memset(logfile[i].pos, 0, logfile[i].rows * sizeof(int));

            for (k = 0, j = 0; j < logfile[i].buf->size; j++) {
                if (!k) {
                    logfile[i].pos[logfile[i].rows - 1] = j;
                    k = 1;
                }
                if (logfile[i].buf->ptr[j] == '\n') {
                    int *pos = malloc(++logfile[i].rows * sizeof(int));
                    if (pos == NULL) return;
                    memcpy(pos, logfile[i].pos, (logfile[i].rows - 1) * sizeof(int));
                    pos[logfile[i].rows - 1] = 0;
                    free(logfile[i].pos);
                    logfile[i].pos = NULL;
                    logfile[i].pos = malloc(logfile[i].rows * sizeof(int));
                    if (logfile[i].pos == NULL) return;
                    memcpy(logfile[i].pos, pos, logfile[i].rows * sizeof(int));
                    free(pos);
                    logfile[i].buf->ptr[j] = 0;
                    k = 0;
                }
                if (logfile[i].buf->ptr[j] == '\r') logfile[i].buf->ptr[j] = 0;
                if (logfile[i].rows > 0xFFFFFFFE) break;
            }
            logfile[i].ready = 1;
        }
    }
}

static void freeAllLogs() {
    int i;
    for (i = 0; i < 2; i++) {
        if (logfile[i].ready) {
            free(logfile[i].buf);
            logfile[i].buf = NULL;
            free(logfile[i].pos);
            logfile[i].pos = NULL;
        }
    }
}

static void sortList() {
    int i, j;
    fileliststruct temp;
    if (list_count < 2) return;
    for (j = list_count - 1; j > 0; j--) {
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
    SceUID dp = 0;
    SceIoDirent ds;

    dp = sceIoDopen(paksDir);

    while (sceIoDread(dp, &ds) > 0) {
        if (packfile_supported(ds.d_name)) {
            fileliststruct *copy = NULL;
            if (filelist == NULL) filelist = malloc(sizeof(fileliststruct));
            else {
                copy = malloc(i * sizeof(fileliststruct));
                memcpy(copy, filelist, i * sizeof(fileliststruct));
                free(filelist);
                filelist = malloc((i + 1) * sizeof(fileliststruct));
                memcpy(filelist, copy, i * sizeof(fileliststruct));
                free(copy);
            }
            memset(&filelist[i], 0, sizeof(fileliststruct));
            strncpy(filelist[i].filename, ds.d_name, strlen(ds.d_name));
            i++;
        }
    }
    sceIoDclose(dp);
    return i;
}

static void printText(int x, int y, int col, int backcol, int fill, const char *format, ...) {
    int x1, y1, i;
    uint32_t data;
    uint32_t *line32 = NULL;
    unsigned char *font;
    unsigned char ch = 0;
    char buf[128] = {""};

    va_list arglist;
    va_start(arglist, format);
    vsprintf(buf, format, arglist);
    va_end(arglist);

    for (i = 0; i < sizeof(buf); i++) {
        ch = (unsigned char) buf[i];
        // mapping
        if (ch < 0x20) ch = 0;
        else if (ch < 0x80) ch -= 0x20;
        else if (ch < 0xa0) ch = 0;
        else ch -= 0x40;
        font = (u8 *) &hankaku_font10[ch * 10];
        // draw
        line32 = (uint32_t *) Screen->data + x + y * Screen->width;

        for (y1 = 0; y1 < 10; y1++) {
            data = *font++;
            for (x1 = 0; x1 < 5; x1++) {
                if (data & 1) *line32 = (uint32_t) col;
                else if (fill) *line32 = (uint32_t) backcol;

                line32++;
                data = data >> 1;
            }
            line32 += Screen->width - 5;
        }
        x += 5;
    }
}

static int readFile(char *path, unsigned char *buffer, unsigned long len) {
    FILE *fd;

    if (!buffer) {
        printf("read_file: memory error");
        return -1;
    }

    fd = fopen(path, "rb");
    if (!fd) {
        printf("read_file: unable to open %s\n", path);
        return -1;
    }

    fread(buffer, len, 1, fd);
    fclose(fd);

    return 0;
}

static void writeFile(char *path, unsigned char *buffer, unsigned long len) {
    FILE *fd = fopen(path, "wb");
    if (!fd) {
        printf("Unable to open file %s\n", path);
        return;
    }

    fwrite(buffer, len, 1, fd);
    fclose(fd);
}

static s_screen *getPreview(char *filename) {
    s_screen *title = NULL;
    s_screen *scale = NULL;

    // try to load cached titles
    char title_path[512] = {""};
    char title_pal_path[512] = {""};
    unsigned long title_data_len;

    // Grab current path and filename
    getBasePath(packfile, filename, 1);

    snprintf(title_path, 512, "%s/%s.png", titlesDir, filename);
    snprintf(title_pal_path, 512, "%s/%s.pal", titlesDir, filename);
    printf("loading: %s\n", title_path);

    scale = allocscreen(160, 120, PIXEL_x8);
    title_data_len = (unsigned long) (160 * 120 * pixelbytes[scale->pixelformat]);
    if (readFile(title_path, scale->data, title_data_len) >= 0) {
        readFile(title_pal_path, scale->palette, (unsigned long) PAL_BYTES);
        strncpy(packfile, "Menu.xxx", 128);
        return scale;
    }

    printf("could not find cached title, loading from pak\n");
    // Create & Load & Scale Image
    if (!loadscreen("data/bgs/title", packfile, NULL, PIXEL_x8, &title)) {
        freescreen(&scale);
        return NULL;
    }

    scalescreen(scale, title);
    memcpy(scale->palette, title->palette, (size_t) PAL_BYTES);

    // ScreenShots within Menu will be saved as "Menu"
    strncpy(packfile, "Menu.xxx", 128);
    freescreen(&title);

    // cache title
    writeFile(title_path, scale->data, title_data_len);
    writeFile(title_pal_path, scale->palette, (size_t) PAL_BYTES);

    return scale;
}

static void getAllPreviews() {
    int i;
    for (i = 0; i < list_count; i++) {
        filelist[i].preview = getPreview(filelist[i].filename);
    }
}

static void freeAllPreviews() {
    int i;
    for (i = 0; i < list_count; i++) {
        if (filelist[i].preview != NULL)
            freescreen(&filelist[i].preview);
    }
}

static int ControlMenu() {

    int status = -1;

    refreshInput();

    switch (buttonsHeld) {

        case SCE_CTRL_UP:
            list_selection--;
            if (list_selection < 0) {
                list_selection = list_count - 1;
            }
            sceKernelDelayThread(INPUT_DELAY);
            break;

        case SCE_CTRL_DOWN:
            list_selection++;
            if (list_selection >= list_count) {
                list_selection = 0;
            }
            sceKernelDelayThread(INPUT_DELAY);
            break;

        case SCE_CTRL_LEFT:
            list_selection -= 5;
            if (list_selection < 0) {
                list_selection = 0;
            }
            sceKernelDelayThread(INPUT_DELAY);
            break;

        case SCE_CTRL_RIGHT:
            list_selection += 5;
            if (list_selection >= list_count) {
                list_selection = list_count - 1;
            }
            sceKernelDelayThread(INPUT_DELAY);
            break;

        case SCE_CTRL_CROSS:
            // Start Engine
            status = 1;
            sceKernelDelayThread(INPUT_DELAY);
            break;

        case SCE_CTRL_SQUARE:
            // Show Logs
            status = 3;
            sceKernelDelayThread(INPUT_DELAY);
            break;

        default:
            // No Update Needed
            status = 0;
            break;
    }

    return status;
}

static void initMenu(int type) {
    // Read Logo or Menu from Array.
    if (!type) Source = pngToScreen((void *) openbor_logo_480x272_png.data);
    else Source = pngToScreen((void *) openbor_menu_480x272_sony_png.data);

    Screen = allocscreen(480, 272, PIXEL_32);
}

static void termMenu() {
    freescreen(&Source);
    freescreen(&Screen);
}

static void drawMenu() {

    s_screen *Image = NULL;
    char listing[45] = {""}, *extension;
    int i = 0;
    int shift = 0;
    int colors = 0;

    copyscreen(Screen, Source);
    if (list_count < 1) printText(30, 33, RED, 0, 0, "No Games In Paks Folder!");

    for (i = 0; i < list_max; i++) {

        int index = ((list_selection / list_max) * list_max) + i;

        if (index < list_count) {

            shift = 0;
            colors = GRAY;
            listing[0] = '\0';
            strncat(listing, filelist[index].filename, sizeof(listing) - 1);
            extension = strrchr(listing, '.');
            if (extension) *extension = '\0';
            if (index == list_selection) {
                shift = 2;
                colors = RED;
                Image = filelist[index].preview;
                if (Image) putscreen(Screen, Image, 286, 32, NULL);
                else printText(288, 141, RED, 0, 0, "No Preview Available!");
            }
            printText(30 + shift, 33 + (11 * i), colors, 0, 0, "%s", listing);
        }
    }

    printText(26, 11, WHITE, 0, 0, "OpenBoR %s", VERSION);
    printText(392, 11, WHITE, 0, 0, __DATE__);
    printText(28, 251, WHITE, 0, 0, "View Logs");   // Square
    printText(150, 251, WHITE, 0, 0, "");            // Triangle
    printText(268, 251, WHITE, 0, 0, "");            // Circle
    printText(392, 251, WHITE, 0, 0, "Start Game");  // Cross
    printText(320, 175, BLACK, 0, 0, "www.chronocrash.com");
    printText(322, 185, BLACK, 0, 0, "www.senileteam.com");

#ifdef SPK_SUPPORTED
    printText(324,191, DARK_RED, 0, 0, "SecurePAK Edition");
#endif

    video_copy_screen(Screen);
}

static void drawLogs() {
    int i = which_logfile, j, k, l, done = 0;

    while (!done) {
        clearscreen(Screen);
        refreshInput();
        printText(410, 3, RED, 0, 0, "Quit : Circle");
        if (buttonsPressed & SCE_CTRL_CIRCLE) done = 1;

        if (logfile[i].ready) {
            printText(5, 3, RED, 0, 0, "OpenBorLog.txt");
            if (buttonsHeld & SCE_CTRL_UP) --logfile[i].line;
            if (buttonsHeld & SCE_CTRL_DOWN) ++logfile[i].line;
            if (buttonsHeld & SCE_CTRL_LEFT) logfile[i].line = 0;
            if (buttonsHeld & SCE_CTRL_RIGHT) logfile[i].line = logfile[i].rows - (LOG_SCREEN_END - LOG_SCREEN_TOP);
            if (logfile[i].line > logfile[i].rows - (LOG_SCREEN_END - LOG_SCREEN_TOP) - 1)
                logfile[i].line =
                        logfile[i].rows -
                        (LOG_SCREEN_END -
                         LOG_SCREEN_TOP) - 1;
            if (logfile[i].line < 0) logfile[i].line = 0;
            for (l = LOG_SCREEN_TOP, j = logfile[i].line; j < logfile[i].rows - 1; l++, j++) {
                if (l < LOG_SCREEN_END) {
                    char textpad[480] = {""};
                    for (k = 0; k < 480; k++) {
                        if (!logfile[i].buf->ptr[logfile[i].pos[j] + k]) break;
                        textpad[k] = logfile[i].buf->ptr[logfile[i].pos[j] + k];
                    }
                    if (logfile[i].rows > 0xFFFF)
                        printText(5, l * 10, WHITE, 0, 0, "0x%08x:  %s", j, textpad);
                    else
                        printText(5, l * 10, WHITE, 0, 0, "0x%04x:  %s", j, textpad);
                } else break;
            }
        } else if (i == SCRIPT_LOG) printText(5, 3, RED, 0, 0, "Log NOT Found: ScriptLog.txt");
        else printText(5, 3, RED, 0, 0, "Log NOT Found: OpenBorLog.txt");

        video_copy_screen(Screen);
    }
    drawMenu();
}

static void drawLogo() {
    initMenu(0);
    //copyscreen(Screen, Source);
    video_copy_screen(Source);
    unsigned int startTime = timer_gettick();

    // The logo displays for 2 seconds.  Let's put that time to good use.
    list_count = findPaks();
    getAllPreviews();
    sortList();
    getAllLogs();

    // Display logo for the rest of the 2 seconds
    while (timer_gettick() - startTime < 2000) {
        sceKernelDelayThread(1000);
    }
    termMenu();
}

static void setVideoMode() {
    s_videomodes videomodes;
    videomodes.hRes = 480;
    videomodes.vRes = 272;
    videomodes.pixel = 4;
    video_set_mode(videomodes);
}

void Menu() {

    int done = 0;
    int ctrl = 0;

    setVideoMode();
    drawLogo();

    //dListCurrentPosition = 0;
    if (list_count != 1) {

        initMenu(1);
        drawMenu();
        pControl = ControlMenu;

        while (!done) {
            ctrl = Control();
            switch (ctrl) {
                case 1:
                case 2:
                    done = 1;
                    break;

                case 3:
                    drawLogs();
                    break;

                case -1:
                    drawMenu();
                    break;

                default:
                    break;
            }
        }

        freeAllLogs();
        freeAllPreviews();
        termMenu();
        if (ctrl == 2) {
            if (filelist) {
                free(filelist);
                filelist = NULL;
            }
            borExit(0);
        }
    }

    getBasePath(packfile, filelist[list_selection].filename, 1);
    free(filelist);
}
