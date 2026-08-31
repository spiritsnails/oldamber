
#include "crystal_grass_rustle.h"

#include "crystal_emotes.h"
#include "amberscript_tilemod.h"
#include "amberscript_core.h"
#include "../data/map_data.h"
#include "constants.h"
#include "../platform/hardware.h"
#include "../platform/display.h"

#define RUSTLE_TILE_IDX   0xFA
#define RUSTLE_OAM_BASE   76

#define RUSTLE_TICKS_PER_ROM_FRAME 2

static int s_timer = 0;
static int s_frame_ctr = 0;

void CrystalGrassRustle_LoadTile(void) {
    Display_LoadSpriteTile(RUSTLE_TILE_IDX, gCrystalGrassRustleGfx);
}

void CrystalGrassRustle_Reset(void) {
    s_timer = 0;
    s_frame_ctr = 0;
}

void CrystalGrassRustle_OnStep(int gx, int gy, int frames) {
    uint8_t rustle = 0;

    if (!AmberScript_IsEnabled()) return;
    if (wCurMap < PKS_VIRTUAL_MAP_FIRST || wCurMap > PKS_VIRTUAL_MAP_LAST) return;

    if (!AmberScript_GetGrassRustleOverrideAt(gx * 2, gy * 2 + 1, &rustle))
        return;
    if (!rustle) return;

    s_timer = (frames > 1) ? frames - 1 : 1;
    s_frame_ctr = 0;
}

void CrystalGrassRustle_Tick(void) {
    if (s_timer <= 0) return;
    s_frame_ctr += RUSTLE_TICKS_PER_ROM_FRAME;
    s_timer--;
}

static void clear_oam(void) {
    for (int i = 0; i < CRYSTAL_GRASS_RUSTLE_OBJS; i++) {
        wShadowOAM[RUSTLE_OAM_BASE + i].y = 0;
        wShadowOAM[RUSTLE_OAM_BASE + i].x = 0;
        wShadowOAM[RUSTLE_OAM_BASE + i].tile = 0;
        wShadowOAM[RUSTLE_OAM_BASE + i].flags = 0;
    }
}

void CrystalGrassRustle_BuildOAM(int tracked_sx, int tracked_sy) {
    if (s_timer <= 0) {
        clear_oam();
        return;
    }

    int px = tracked_sx;
    int py = tracked_sy;

    if (px + 16 <= 0 || px >= Display_FrameWidth() ||
        py + 16 <= 0 || py >= SCREEN_HEIGHT_PX) {
        clear_oam();
        return;
    }

    int f = (s_frame_ctr & CRYSTAL_GRASS_RUSTLE_FRAME_MASK) ? 1 : 0;

    for (int i = 0; i < CRYSTAL_GRASS_RUSTLE_OBJS; i++) {
        const crystal_oam_t *o = &gCrystalGrassRustleOam[f][i];
        wShadowOAM[RUSTLE_OAM_BASE + i].y = (uint8_t)(py + o->y + OAM_Y_OFS);
        wShadowOAM[RUSTLE_OAM_BASE + i].x = (px + o->x + OAM_X_OFS);
        wShadowOAM[RUSTLE_OAM_BASE + i].tile = RUSTLE_TILE_IDX;

        wShadowOAM[RUSTLE_OAM_BASE + i].flags =
            (uint8_t)(((o->attr & OAM_FLAG_FLIP_X) ? OAM_FLAG_FLIP_X : 0)
                      | (CRYSTAL_GRASS_RUSTLE_PAL & 7));
    }
}
