/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * Licensed under a BSD-style license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2017 OpenBOR Team
 */

#include <string.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>
#include <psp2/ctrl.h>
#include "vitaport.h"
#include "packfile.h"
#include "video.h"
#include "utils.h"
#include "ram.h"

// set the heap size so that we can use up to 240 MB of RAM instead of 32 MB
// note: not sure how you can get 240 MB of heap ?
unsigned int _newlib_heap_size_user = 192 * 1024 * 1024;

void Menu(); // defined in menu.c

char packfile[128] = {"ux0:/data/bor.pak"};
char paksDir[128];
char savesDir[128];
char logsDir[128];
char screenShotsDir[128];
char rootDir[128]; // note: this one ends with a slash
char titlesDir[128];

void borExit(int reset) {
    sceKernelExitProcess(0);
}

int main(int argc, char *argv[]) {

    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    video_init();

    packfile_mode(0);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    strcpy(rootDir, "ux0:/data/OpenBOR/");
    strcpy(paksDir, "ux0:/data/OpenBOR/Paks");
    strcpy(savesDir, "ux0:/data/OpenBOR/Saves");
    strcpy(logsDir, "ux0:/data/OpenBOR/Logs");
    strcpy(screenShotsDir, "ux0:/data/OpenBOR/ScreenShots");
    strcpy(titlesDir, "ux0:/data/OpenBOR/Titles");

    dirExists(rootDir, 1);
    dirExists(paksDir, 1);
    dirExists(savesDir, 1);
    dirExists(logsDir, 1);
    dirExists(screenShotsDir, 1);
    dirExists(titlesDir, 1);

    Menu();
    setSystemRam();
    openborMain(argc, argv);
    video_exit();
    borExit(0);

    return 0;
}
