/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * Licensed under a BSD-style license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2017 OpenBOR Team
 */

#include <string.h>
#include <switch.h>
#include "control.h"

#define    PAD_START    1
#define    MAX_PADS     4
#define    PAD_END      (18*MAX_PADS)

static int usejoy;
static int lastkey[MAX_PADS];

static const char *padnames[PAD_END + 1 + 1] = {
        "...",
#define CONTROLNAMES(x) \
    x" Up",             \
    x" Right",          \
    x" Down",           \
    x" Left",           \
    x" B",              \
    x" A",              \
    x" Y",                \
    x" X",                \
    x" L",              \
    x" R",              \
    x" Minus",          \
    x" Plus",            \
    x" ZL",             \
    x" ZR",             \
    x" SL",             \
    x" SR",
        CONTROLNAMES("Switch 1")
        CONTROLNAMES("Switch 2")
        CONTROLNAMES("Switch 3")
        CONTROLNAMES("Switch 4")
        "undefined"
};

static unsigned int getPad(int port);

static int flag_to_index(unsigned int flag) {
    int index = 0;
    unsigned int bit = 1;
    while (!((bit << index) & flag) && index < 31) ++index;
    return index;
}

void control_exit() {
    usejoy = 0;
}

void control_init(int joy_enable) {
    usejoy = joy_enable;
}

int control_usejoy(int enable) {
    usejoy = enable;
    return 0;
}

int control_getjoyenabled() {
    return usejoy;
}

int keyboard_getlastkey(void) {
    int i, ret = 0;
    for (i = 0; i < MAX_PADS; i++) {
        ret |= lastkey[i];
        lastkey[i] = 0;
    }
    return ret;
}

void control_setkey(s_playercontrols *pcontrols, unsigned int flag, int key) {
    if (!pcontrols) return;
    pcontrols->settings[flag_to_index(flag)] = key;
    pcontrols->keyflags = pcontrols->newkeyflags = 0;
}

// Scan input for newly-pressed keys.
// Return value:
// 0  = no key was pressed
// >0 = key code for pressed key
// <0 = error
int control_scankey() {

    static unsigned ready = 0;
    unsigned i, k = 0;

    for (i = 0; i < MAX_PADS; i++) {
        if (lastkey[i]) {
            k = 1 + i * 18 + flag_to_index((unsigned int) lastkey[i]);
            break;
        }
    }

    if (ready && k) {
        ready = 0;
        return k;
    }
    ready = (unsigned int) (!k);

    return 0;
}

char *control_getkeyname(unsigned keycode) {
    if (keycode >= PAD_START && keycode <= PAD_END) return (char *) padnames[keycode];
    return "...";
}

void control_update(s_playercontrols **playercontrols, int numplayers) {

    unsigned int k;
    unsigned int i;
    int player;
    int t;
    s_playercontrols *pcontrols;
    unsigned port[MAX_PADS];

    hidScanInput();

    for (i = 0; i < MAX_PADS; i++)
        port[i] = getPad(i);

    for (player = 0; player < numplayers; player++) {
        pcontrols = playercontrols[player];
        k = 0;
        for (i = 0; i < 32; i++) {
            t = pcontrols->settings[i];
            if (t >= PAD_START && t <= PAD_END) {
                int portnum = (t - 1) / 18;
                int shiftby = (t - 1) % 18;
                if (portnum >= 0 && portnum <= 3) {
                    if ((port[portnum] >> shiftby) & 1) k |= (1 << i);
                }
            }
        }
        pcontrols->kb_break = 0;
        pcontrols->newkeyflags = k & (~pcontrols->keyflags);
        pcontrols->keyflags = k;
    }
}

void control_rumble(int port, int msec) {
}

static unsigned int getPad(int port) {

    unsigned int btns = 0;

    if (port != 0) {
        lastkey[port] = 0;
        return 0;
    }

    // TODO
    if (control_getjoyenabled()) {
        //if (pad.ly >= 0xC0)              btns |= VITA_DPAD_DOWN;
        //if (pad.ly <= 0x30)              btns |= VITA_DPAD_UP;
        //if (pad.lx <= 0x30)              btns |= VITA_DPAD_LEFT;
        //if (pad.lx >= 0xC0)              btns |= VITA_DPAD_RIGHT;
    }

    u64 key = hidKeysHeld(port == 0 ? CONTROLLER_P1_AUTO : (HidControllerID) port);

    if (key & KEY_MINUS) btns |= SWITCH_SELECT;
    if (key & KEY_PLUS) btns |= SWITCH_START;
    if (key & KEY_DUP) btns |= SWITCH_DPAD_UP;
    if (key & KEY_DRIGHT) btns |= SWITCH_DPAD_RIGHT;
    if (key & KEY_DDOWN) btns |= SWITCH_DPAD_DOWN;
    if (key & KEY_DLEFT) btns |= SWITCH_DPAD_LEFT;
    if (key & KEY_L) btns |= SWITCH_LEFT_TRIGGER;
    if (key & KEY_R) btns |= SWITCH_RIGHT_TRIGGER;
    if (key & KEY_X) btns |= SWITCH_TRIANGLE;
    if (key & KEY_A) btns |= SWITCH_CIRCLE;
    if (key & KEY_B) btns |= SWITCH_CROSS;
    if (key & KEY_Y) btns |= SWITCH_SQUARE;
    if (key & KEY_ZL) btns |= SWITCH_L1;
    if (key & KEY_ZR) btns |= SWITCH_R1;
    if (key & KEY_SL) btns |= SWITCH_L3;
    if (key & KEY_SR) btns |= SWITCH_R3;

    lastkey[port] = btns;

    return btns;
}
