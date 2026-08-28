
#include "escape_anim.h"
#include "player.h"
#include "warp.h"
#include "overworld.h"
#include "music.h"
#include "bicycle.h"
#include "town_map.h"
#include "constants.h"
#include "amberscript_mapbank.h"
#include "../data/map_data.h"
#include <string.h>
#include "../platform/audio.h"
#include "../platform/display.h"
#include "../platform/hardware.h"

#define EA_LEAVE_SPIN_START   16
#define EA_ENTER_SPIN_END      8

#define EA_RISE_DELTA_Y      (-16)
#define EA_RISE_STEPS          5
#define EA_SPIN_MOVE_DELAY     3
#define EA_PLAYER_OFFSCREEN_Y (EA_RISE_STEPS * EA_RISE_DELTA_Y)

#define EA_TRAILING_DELAY     10
#define EA_ENTER_DELAY3        3

#define EA_FADE_FRAMES 8
static const uint8_t kEaFadeOut[3][3] = {
    { 0x90, 0x80, 0x90 },
    { 0x40, 0x40, 0x40 },
    { 0x00, 0x00, 0x00 },
};
static const uint8_t kEaFadeIn[2][3] = {
    { 0x40, 0x40, 0x40 },
    { 0x90, 0x80, 0x90 },

};
#define EA_FADE_OUT_STEPS ((int)(sizeof(kEaFadeOut) / sizeof(kEaFadeOut[0])))
#define EA_FADE_IN_STEPS  ((int)(sizeof(kEaFadeIn)  / sizeof(kEaFadeIn[0])))

typedef enum {
    EA_IDLE = 0,
    EA_PREDELAY,

    EA_SPIN, EA_RISE, EA_DELAY, EA_FADE_OUT,

    EA_ENTER_DELAY, EA_FADE_IN, EA_DESCEND, EA_SETTLE
} ea_phase_t;

static ea_phase_t s_phase      = EA_IDLE;
static int        s_timer      = 0;
static int        s_spin_delay = 0;
static int        s_move_step  = 0;
static int        s_spin_y     = 0;
static int        s_fade_step  = 0;
static int        s_saved_facing = 0;
static uint8_t    s_dest_map   = 0;
static int        s_dest_x     = 0;
static int        s_dest_y     = 0;

void EscapeAnim_Start(uint8_t dest_map, int dest_x, int dest_y) {
    s_dest_map = dest_map;
    s_dest_x   = dest_x;
    s_dest_y   = dest_y;

    s_saved_facing = gPlayerFacing;

    Player_SetWarpSpin(1);
    Player_SetWarpSpinY(0);

    s_spin_delay = EA_LEAVE_SPIN_START;
    s_move_step  = 0;
    s_spin_y     = 0;
    s_timer      = 0;

    s_phase      = EA_PREDELAY;
}

int EscapeAnim_IsActive(void) {
    return s_phase != EA_IDLE;
}

void EscapeAnim_StartToLastHealTownAfter(int predelay) {

    int fx, fy;
    uint8_t town = wLastHealTownMap;

    if (wLastHealTownName[0] != '\0') {
        int real = Map_RealIdForName(wLastHealTownName);
        if (real >= 0) town = (uint8_t)real;
    }
    if (!TownMap_GetFlyDest(town, &fx, &fy)) {
        town = 0x00; fx = 5; fy = 6;
    }
    EscapeAnim_Start(town, fx, fy);

    s_timer = (predelay > 0) ? predelay : 0;
}

void EscapeAnim_StartToLastHealTown(void) {
    EscapeAnim_StartToLastHealTownAfter(0);
}

static int ea_cur_map_kanto_index(void) {

    return Map_CurrentRealId();
}

static uint8_t ea_cur_map_tileset(void) {
    int mi = ea_cur_map_kanto_index();
    return (mi >= 0) ? gMapTable[mi].tileset_id : wCurMapTileset;
}

int EscapeAnim_CanEscapeHere(void) {

    int mi = ea_cur_map_kanto_index();
    uint8_t ts = ea_cur_map_tileset();
    if (mi == 247) return 0;
    return ts == TILESET_FOREST   || ts == TILESET_CEMETERY ||
           ts == TILESET_CAVERN   || ts == TILESET_FACILITY ||
           ts == TILESET_INTERIOR;
}

int EscapeAnim_IsOutsideMap(void) {

    uint8_t ts = ea_cur_map_tileset();
    return ts == TILESET_OVERWORLD || ts == TILESET_PLATEAU;
}

void EscapeAnim_Tick(void) {
    if (s_phase == EA_IDLE) return;
    if (s_timer > 0) { s_timer--; return; }

    switch (s_phase) {

    case EA_PREDELAY:

        Music_Stop();
        s_phase = EA_SPIN;
        s_timer = 0;
        break;

    case EA_SPIN:

        Player_WarpSpinStep();
        if ((s_spin_delay & 3) == 0)
            Audio_PlaySFX_TeleportExit2();
        s_spin_delay -= 1;
        if (s_spin_delay == 0) {
            Audio_PlaySFX_TeleportExit1();
            s_phase     = EA_RISE;
            s_move_step = 0;
            s_timer     = 0;
            break;
        }
        s_timer = s_spin_delay;
        break;

    case EA_RISE:

        Player_WarpSpinStep();
        s_spin_y += EA_RISE_DELTA_Y;
        Player_SetWarpSpinY(s_spin_y);
        if (++s_move_step >= EA_RISE_STEPS) {
            s_phase = EA_DELAY;
            s_timer = EA_TRAILING_DELAY;
            break;
        }
        s_timer = EA_SPIN_MOVE_DELAY;
        break;

    case EA_DELAY:
        s_phase     = EA_FADE_OUT;
        s_fade_step = 0;
        s_timer     = 0;
        break;

    case EA_FADE_OUT:
        if (s_fade_step < EA_FADE_OUT_STEPS) {
            Display_SetPalette(kEaFadeOut[s_fade_step][0],
                               kEaFadeOut[s_fade_step][1],
                               kEaFadeOut[s_fade_step][2]);
            s_fade_step++;
            s_timer = EA_FADE_FRAMES;
            break;
        }

        Warp_ForceTeleport(s_dest_map, s_dest_x, s_dest_y);
        Music_Stop();
        Display_SetPalette(0x00, 0x00, 0x00);

        gPlayerFacing  = 0;
        s_saved_facing = gPlayerFacing;

        Player_SetWarpSpinY(EA_PLAYER_OFFSCREEN_Y);
        s_spin_y = EA_PLAYER_OFFSCREEN_Y;
        s_phase  = EA_ENTER_DELAY;
        s_timer  = EA_ENTER_DELAY3;
        break;

    case EA_ENTER_DELAY:
        s_phase     = EA_FADE_IN;
        s_fade_step = 0;
        s_timer     = 0;
        break;

    case EA_FADE_IN:
        if (s_fade_step < EA_FADE_IN_STEPS) {
            Display_SetPalette(kEaFadeIn[s_fade_step][0],
                               kEaFadeIn[s_fade_step][1],
                               kEaFadeIn[s_fade_step][2]);
            s_fade_step++;
            s_timer = EA_FADE_FRAMES;
            break;
        }
        Display_LoadMapPalette();
        Audio_PlaySFX_TeleportEnter1();
        s_phase     = EA_DESCEND;
        s_move_step = 0;
        s_timer     = 0;
        break;

    case EA_DESCEND:

        Player_WarpSpinStep();
        s_spin_y -= EA_RISE_DELTA_Y;
        Player_SetWarpSpinY(s_spin_y);
        if (++s_move_step >= EA_RISE_STEPS) {
            Audio_PlaySFX_TeleportEnter2();
            s_spin_delay = 0;
            s_phase      = EA_SETTLE;
            s_timer      = 0;
            break;
        }
        s_timer = EA_SPIN_MOVE_DELAY;
        break;

    case EA_SETTLE:

        Player_WarpSpinStep();
        s_spin_delay += 1;
        if (s_spin_delay == EA_ENTER_SPIN_END) {
            Player_SetWarpSpin(0);
            gPlayerFacing = s_saved_facing;
            Bicycle_PlayDefaultMusic();
            s_phase = EA_IDLE;
            break;
        }
        s_timer = s_spin_delay;
        break;

    default:
        break;
    }
}
