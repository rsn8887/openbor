/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

#ifndef SDLPORT_H
#define SDLPORT_H

#include <SDL.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include "globals.h"

#define stricmp  strcasecmp
#define strnicmp strncasecmp

#define SDL2 1
#define SKIP_CODE

//#define MEMTEST 1
extern int opengl;

void initSDL();
void borExit(int reset);
void openborMain(int argc, char** argv);

extern char packfile[128];
extern char paksDir[128];
extern char savesDir[128];
extern char logsDir[128];
extern char screenShotsDir[128];

#endif
