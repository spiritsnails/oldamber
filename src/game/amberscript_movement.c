
#include "amberscript_movement.h"
#include "amberscript_core.h"

#include "overworld.h"
#include "../platform/hardware.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define PKS_FRAMES_PER_TILE 20
#define PKS_PRESS 1
#define PKS_GAP   8

int AmberScript_Movement_TryHandle(const char *cmd, const char *verb, int n) {
    (void)cmd;

    if (strcmp(verb, "up") == 0) {
        AmberScript_SeqPush(PKS_BTN_UP, n * PKS_FRAMES_PER_TILE, 5);
    } else if (strcmp(verb, "down") == 0) {
        AmberScript_SeqPush(PKS_BTN_DOWN, n * PKS_FRAMES_PER_TILE, 5);
    } else if (strcmp(verb, "left") == 0) {
        AmberScript_SeqPush(PKS_BTN_LEFT, n * PKS_FRAMES_PER_TILE, 5);
    } else if (strcmp(verb, "right") == 0) {
        AmberScript_SeqPush(PKS_BTN_RIGHT, n * PKS_FRAMES_PER_TILE, 5);
    } else if (strcmp(verb, "a") == 0 || strcmp(verb, "interact") == 0) {
        for (int i = 0; i < n; i++) AmberScript_SeqPush(PKS_BTN_A, PKS_PRESS, PKS_GAP);
    } else if (strcmp(verb, "b") == 0 || strcmp(verb, "back") == 0) {
        AmberScript_SeqPush(PKS_BTN_B, PKS_PRESS, PKS_GAP);
    } else if (strcmp(verb, "start") == 0 || strcmp(verb, "menu") == 0) {
        AmberScript_SeqPush(PKS_BTN_START, PKS_PRESS, PKS_GAP);
    } else if (strcmp(verb, "select") == 0) {
        AmberScript_SeqPush(PKS_BTN_SELECT, PKS_PRESS, PKS_GAP);
    } else if (strcmp(verb, "wait") == 0) {
        AmberScript_SeqPush(0, n, 0);
    } else if (strcmp(verb, "state") == 0) {
        AmberScript_WriteState();
        return 1;
    } else if (strcmp(verb, "tile_info") == 0 || strcmp(verb, "tileinfo") == 0) {

        int px = (int)wXCoord, py = (int)wYCoord;
        static const struct { const char *label; int dx, dy; } dirs[] = {
            { "HERE ", 0,  0 },
            { "UP   ", 0, -1 },
            { "DOWN ", 0,  1 },
            { "LEFT ",-1,  0 },
            { "RIGHT", 1,  0 },
        };
        printf("[amberscript] tile_info player @ game(%d,%d)  tile(%d,%d)\n",
               px, py, px * 2, py * 2 + 1);
        for (int i = 0; i < 5; i++) {
            int gx = px + dirs[i].dx;
            int gy = py + dirs[i].dy;
            uint8_t tid = Map_GetGameTile(gx, gy);
            printf("  %s  game(%2d,%2d)  tile(%2d,%2d)  id=0x%02X  %s\n",
                   dirs[i].label, gx, gy, gx * 2, gy * 2 + 1, tid,
                   Tile_IsPassable(tid) ? "PASS" : "SOLID");
        }
        return 1;
    } else {
        return 0;
    }

    if (AmberScript_SeqPending()) {
        AmberScript_RequestDeferredWrite(20);
    } else {
        AmberScript_WriteState();
    }
    return 1;
}
