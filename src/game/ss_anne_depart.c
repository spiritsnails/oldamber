
#include "ss_anne_depart.h"
#include "overworld.h"
#include "amberscript_tilemod.h"
#include "types.h"
#include "constants.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../platform/audio.h"
#include "assetpack_bind.h"
#include <stdio.h>
#include <string.h>

#define PRE_DELAY_FRAMES   120
#define POST_HORN_FRAMES   120

#define BAND_ROW             10
#define BAND_ROWS             6

#define COLUMNS               8
#define DRIFTS_PER_COLUMN    16
#define FRAMES_PER_DRIFT      8
#define PX_PER_COLUMN        16
#define TOTAL_SCROLL_PX     (COLUMNS * PX_PER_COLUMN)

#define WATER_TILE         0x14

#define SMOKE_X_INIT         88
#define SMOKE_X_STEP         16
#define SMOKE_Y             100
#define SMOKE_DRIFT_PX        2
#define SMOKE_TILE_FIRST   0xFC
#define SMOKE_MAX_PUFFS  COLUMNS

#define SMOKE_OAM_BASE       72

typedef enum {
    DEP_OFF = 0,
    DEP_PRE_DELAY,
    DEP_SCROLL,
    DEP_ERASE,
} dep_phase_t;

static dep_phase_t s_phase;
static int  s_timer;
static int  s_column;
static int  s_drift;
static int  s_frame;
static int  s_scroll_px;
static uint8_t s_smoke_x;
static int  s_puffs;

static uint8_t s_band[BAND_ROWS][SCREEN_WIDTH];

static uint8_t s_water[BAND_ROWS][SCREEN_WIDTH];

static uint8_t water_at(int r, int c) {
    return s_water[(r < 2) ? r + 2 : r][c];
}

static void smoke_load_tiles(void) {

    for (int i = 0; i < 4; i++)
        Display_LoadSpriteTile((uint8_t)(SMOKE_TILE_FIRST + i), kSmokeTileGfx);
}

static void smoke_clear(void) {
    for (int i = 0; i < SMOKE_MAX_PUFFS * 4; i++) {
        int s = SMOKE_OAM_BASE + i;
        if (s >= MAX_SPRITES) break;
        wShadowOAM[s].y = 0;
        wShadowOAM[s].x = 0;
        wShadowOAM[s].tile = 0;
        wShadowOAM[s].flags = 0;
    }
    s_puffs   = 0;
    s_smoke_x = SMOKE_X_INIT;
}

static void smoke_emit(void) {
    if (s_puffs >= SMOKE_MAX_PUFFS) return;
    s_smoke_x = (uint8_t)(s_smoke_x - SMOKE_X_STEP);

    int base = SMOKE_OAM_BASE + s_puffs * 4;

    static const int dx[4] = { 0, 8, 0, 8 };
    static const int dy[4] = { 0, 0, 8, 8 };
    for (int i = 0; i < 4; i++) {
        int s = base + i;
        if (s >= MAX_SPRITES) return;
        wShadowOAM[s].y     = (uint8_t)(SMOKE_Y + dy[i]);
        wShadowOAM[s].x     = (uint8_t)(s_smoke_x + dx[i]);
        wShadowOAM[s].tile  = (uint8_t)(SMOKE_TILE_FIRST + i);

        wShadowOAM[s].flags = OAM_FLAG_PALETTE;
    }
    s_puffs++;
}

static void smoke_drift(void) {
    for (int i = 0; i < s_puffs * 4; i++) {
        int s = SMOKE_OAM_BASE + i;
        if (s >= MAX_SPRITES) break;
        wShadowOAM[s].x = (uint8_t)(wShadowOAM[s].x + SMOKE_DRIFT_PX);
    }
}

static void erase_ship_blocks(void) {
    static const char *const kHull[4] = {
        "vdock_ship_h5", "vdock_ship_h6", "vdock_ship_h7", "vdock_ship_h8",
    };
    for (int i = 0; i < 4; i++)
        AmberScript_PlaceSwapBlock(kHull[i], "gone", 5 + i, 2);
}

static void clear_ship_top_lower_cells(void) {
    for (int cx = 10; cx <= 17; cx++) {
        if (!AmberScript_TilePlaceCustom("vdock_ship_h5_gone_tl", cx, 3)) {
            printf("[ssanne] ship-top clear FAILED at cell (%d,3) -- "
                   "regenerate with tools/romimport/emit_kanto.py --all\n", cx);
            fflush(stdout);
            return;
        }
    }
}

static void band_capture(uint8_t dst[BAND_ROWS][SCREEN_WIDTH]) {
    for (int r = 0; r < BAND_ROWS; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            dst[r][c] = gScrollTileMap[(BAND_ROW + r + 2) * SCROLL_MAP_W + (c + 2) + Map_UiColOfs()];
}

void SSAnneDepart_Start(void) {
    s_phase      = DEP_PRE_DELAY;
    s_timer      = PRE_DELAY_FRAMES;
    s_column     = 0;
    s_drift      = 0;
    s_frame      = 0;
    s_scroll_px  = 0;
    smoke_clear();
    smoke_load_tiles();

    Display_SetOBP1(0x00);
    Display_SetBandXPx(-1, 0, 0);
    printf("[ssanne] departure: begin (%d frames pre-delay, %d px over %d frames)\n",
           PRE_DELAY_FRAMES, TOTAL_SCROLL_PX,
           COLUMNS * DRIFTS_PER_COLUMN * FRAMES_PER_DRIFT);
    fflush(stdout);
}

int SSAnneDepart_IsActive(void) { return s_phase != DEP_OFF; }

void SSAnneDepart_ApplyDeparted(void) {
    erase_ship_blocks();
    clear_ship_top_lower_cells();
}

void SSAnneDepart_Tick(void) {
    switch (s_phase) {
    case DEP_OFF:
        return;

    case DEP_PRE_DELAY:
        if (--s_timer > 0) return;

        band_capture(s_band);

        erase_ship_blocks();
        Map_BuildScrollView();
        band_capture(s_water);
        Audio_PlaySFX_SSAnneHorn();
        s_phase = DEP_SCROLL;
        return;

    case DEP_SCROLL:

        if (s_frame == 0) {
            if (s_drift == 0) smoke_emit();
            smoke_drift();
        }
        if (++s_frame < FRAMES_PER_DRIFT) return;

        s_frame = 0;
        s_scroll_px++;
        if (++s_drift < DRIFTS_PER_COLUMN) return;

        s_drift = 0;
        if (++s_column < COLUMNS) return;

        clear_ship_top_lower_cells();
        Display_SetBandXPx(-1, 0, 0);
        smoke_clear();
        Display_SetOBP1(0xE4);
        Map_BuildScrollView();
        Audio_PlaySFX_SSAnneHorn();
        s_timer = POST_HORN_FRAMES;
        s_phase = DEP_ERASE;
        return;

    case DEP_ERASE:
        if (--s_timer > 0) return;
        s_phase = DEP_OFF;
        printf("[ssanne] departure: done\n");
        fflush(stdout);
        return;
    }
}

void SSAnneDepart_PostBuildScrollView(void) {
    if (s_phase != DEP_SCROLL) return;

    int tile_shift = s_scroll_px / 8;
    int sub_px     = s_scroll_px % 8;

    for (int r = 0; r < BAND_ROWS; r++) {
        for (int c = -2; c < SCREEN_WIDTH + 2; c++) {
            int src = c + tile_shift;
            int cc  = (c < 0) ? 0 : (c >= SCREEN_WIDTH ? SCREEN_WIDTH - 1 : c);

            uint8_t t = (src >= 0 && src < SCREEN_WIDTH)
                        ? s_band[r][src]
                        : water_at(r, cc);
            gScrollTileMap[(BAND_ROW + r + 2) * SCROLL_MAP_W + (c + 2) + Map_UiColOfs()] = t;
        }
    }

    Display_SetBandXPx(BAND_ROW, BAND_ROWS, -sub_px);
}
