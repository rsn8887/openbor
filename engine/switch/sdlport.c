/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

#include <switch.h>

#include "sdlport.h"
#include "ram.h"
#include "menu.h"
#include "nxnetprint.h"

int opengl = 0;

#define appExit exit
#undef exit

char packfile[128] = {"bor.pak"};
char paksDir[128] = {"Paks"};
char savesDir[128] = {"Saves"};
char logsDir[128] = {"Logs"};
char screenShotsDir[128] = {"ScreenShots"};

void borExit(int reset) {

    SDL_Delay(1000);
    appExit(0);
}

int main(int argc, char *argv[]) {

    //nx_net_init("192.168.0.13", 4444);
    //consoleDebugInit(debugDevice_SVC);
    //stdout = stderr;

    setSystemRam();

    initSDL();

    packfile_mode(0);

    dirExists(paksDir, 1);
    dirExists(savesDir, 1);
    dirExists(logsDir, 1);
    dirExists(screenShotsDir, 1);


    Menu();

    openborMain(argc, argv);

    borExit(0);

    //nx_net_exit();

    return 0;
}
