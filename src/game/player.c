
#include "player.h"
#include "assetpack_bind.h"
#include "elite_four_scripts.h"
#include "overworld.h"
#include "amberscript_core.h"
#include "amberscript_tilemod.h"
#include "../data/map_data.h"
#include "warp.h"
#include "npc.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../game/constants.h"
#include "../data/player_sprite.h"
#include "../data/bike_sprite.h"
#include "../platform/audio.h"
#include "bicycle.h"
#include "pallet_scripts.h"
#include "../data/seel_sprite.h"
#include "oakslab_scripts.h"
#include "viridian_mart_scripts.h"
#include "route24_scripts.h"
#include "blues_house_scripts.h"
#include "bills_house_scripts.h"
#include "seafoam_scripts.h"
#include "trainer_sight.h"
#include "gate_scripts.h"
#include "field_moves.h"
#include "crystal_grass_rustle.h"
#include <stdio.h>
#include "../platform/debug_log.h"

#define PLAYER_TILE_BASE   64
#define WALK_FRAMES         8
#define BIKE_WALK_FRAMES    4

#define SHADOW_TILE_IDX   0xFB
#define SHADOW_OAM_BASE   68
#define SHADOW_Y_OFFSET     8

static const uint8_t kShadowTile[16] = {
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x07, 0x07,
    0x1F, 0x1F,
    0x3F, 0x3F,
    0x7F, 0x7F,
};

static int gLedgeStep = 0;
static int gLedgeDX   = 0;
static int gLedgeDY   = 0;

static int gArcFrame  = 0;

static int gInputIgnoreFrames = 0;

int gNoClip = 0;

int gScriptedMovement = 0;

void Player_IgnoreInputFrames(int n) {
    gInputIgnoreFrames = n;
}

static const int kLedgeArc[16] = {
    -4, -6, -8,-10,-11,-12,-12,-12,
   -11,-10, -9, -8, -6, -4,  0,  0
};

typedef struct { int facing; uint8_t cur_tile; uint8_t ledge_tile; } ledge_entry_t;
static const ledge_entry_t kLedgeTiles[] = {
    { 0, 0x2C, 0x37 },
    { 0, 0x39, 0x36 },
    { 0, 0x39, 0x37 },
    { 2, 0x2C, 0x27 },
    { 2, 0x39, 0x27 },
    { 3, 0x2C, 0x0D },
    { 3, 0x2C, 0x1D },
    { 3, 0x39, 0x0D },
};
#define NUM_LEDGE_ENTRIES ((int)(sizeof(kLedgeTiles)/sizeof(kLedgeTiles[0])))

static int is_ledge_jump(int nx, int ny) {

    if (AmberScript_IsEnabled() && wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST) {
        int dirs;
        if (!AmberScript_GetLedgeOverrideAt((int)wXCoord * 2, (int)wYCoord * 2 + 1, &dirs)) return 0;
        int face_bit = (gPlayerFacing == 0) ? PKS_FACE_DOWN
                     : (gPlayerFacing == 1) ? PKS_FACE_UP
                     : (gPlayerFacing == 2) ? PKS_FACE_LEFT
                     : PKS_FACE_RIGHT;
        return (dirs & face_bit) ? 1 : 0;
    }

    if (wCurMapTileset != 0) return 0;
    uint8_t cur  = Map_GetGameTile((int)wXCoord, (int)wYCoord);
    uint8_t next = Map_GetGameTile(nx, ny);
    for (int i = 0; i < NUM_LEDGE_ENTRIES; i++) {
        if (kLedgeTiles[i].facing    == gPlayerFacing &&
            kLedgeTiles[i].cur_tile  == cur           &&
            kLedgeTiles[i].ledge_tile == next) return 1;
    }
    return 0;
}

static int is_surf_tileset(void) {
    switch (wCurMapTileset) {
    case TILESET_OVERWORLD:
    case TILESET_FOREST:
    case TILESET_DOJO:
    case TILESET_GYM:
    case TILESET_SHIP:
    case TILESET_SHIP_PORT:
    case TILESET_CAVERN:
    case TILESET_FACILITY:
    case TILESET_PLATEAU:
        return 1;
    default:
        return 0;
    }
}

static int is_connection_tileset(void) {
    return wCurMapTileset == TILESET_OVERWORLD || wCurMapTileset == TILESET_PLATEAU;
}

static int is_surf_water_or_shore_tile(uint8_t tile) {
    if (!is_surf_tileset()) return 0;
    if (wCurMapTileset == TILESET_SHIP_PORT) {
        return tile == 0x14;
    }
    return tile == 0x14 || tile == 0x32 || tile == 0x48;
}

static int is_surf_pair_blocked(uint8_t a, uint8_t b) {
    static const struct { uint8_t ts, t1, t2; } kPairs[] = {
        { TILESET_FOREST, 0x14, 0x2E },
        { TILESET_FOREST, 0x48, 0x2E },
        { TILESET_CAVERN,  0x14, 0x05 },
    };
    for (int i = 0; i < (int)(sizeof(kPairs) / sizeof(kPairs[0])); i++) {
        if (kPairs[i].ts != wCurMapTileset) continue;
        if ((kPairs[i].t1 == a && kPairs[i].t2 == b) ||
            (kPairs[i].t1 == b && kPairs[i].t2 == a))
            return 1;
    }
    return 0;
}

player_surf_step_t Player_ClassifySurfStep(int nx, int ny) {
    uint8_t cur  = Map_GetGameTile((int)wXCoord, (int)wYCoord);
    uint8_t next = Map_GetGameTile(nx, ny);

    if (is_surf_pair_blocked(cur, next)) {
        return PLAYER_SURF_STEP_INVALID;
    }

    if (AmberScript_IsEnabled() &&
        AmberScript_IsPairBlockedAt((int)wXCoord * 2, (int)wYCoord * 2 + 1,
                                   nx * 2, ny * 2 + 1)) {
        return PLAYER_SURF_STEP_INVALID;
    }

    if (AmberScript_IsEnabled()) {
        uint8_t surfable = 0;
        if (Map_GetSurfableOverrideAt(nx, ny, &surfable) && surfable) {
            return PLAYER_SURF_STEP_WATER;
        }
    }

    if (is_surf_water_or_shore_tile(next)) {
        return PLAYER_SURF_STEP_WATER;
    }
    if (Map_IsTilePassableAt(nx, ny)) {
        return PLAYER_SURF_STEP_LAND;
    }
    return PLAYER_SURF_STEP_INVALID;
}

typedef struct { int8_t frame; uint8_t flip; } anim_entry_t;

static const anim_entry_t kAnimTable[4][4] = {
     { {0,0}, {3,0}, {0,0}, {3,1} },
     { {1,0}, {4,0}, {1,0}, {4,1} },
     { {2,0}, {5,0}, {2,0}, {5,0} },
     { {2,1}, {5,1}, {2,1}, {5,1} },
};

int gPlayerFacing = 0;
int gScrollPxX    = 0;
int gScrollPxY    = 0;

static int gWalkTimer        = 0;
static int gWalkStepPxMul    = 1;
int        gStepJustCompleted = 0;
static int gWalkDX           = 0;
static int gWalkDY           = 0;
static int gBgScrollDX       = 0;
static int gBgScrollDY       = 0;
static int gPlayerOffPxX     = 0;
static int gPlayerOffPxY     = 0;
static int gIntraAnimFrame   = 0;
static int gAnimFrameCounter = 0;
static int s_wall_anim_active = 0;

#define PLAYER_DIR_NONE   0
#define PLAYER_DIR_RIGHT  1
#define PLAYER_DIR_LEFT   2
#define PLAYER_DIR_DOWN   4
#define PLAYER_DIR_UP     8
#define TURN_HOLD_TICKS   1

static int s_check_180     = 1;
static int s_moving_dir    = PLAYER_DIR_NONE;
static int s_last_stop_dir = PLAYER_DIR_NONE;
static int s_turning       = 0;
static int s_turn_hold     = 0;
static const int8_t *s_sim_seq = 0;
static int s_sim_idx = -1;

static int s_sim_current_dir = -1;
static int s_spinner_spin_active = 0;
static int s_spinner_spin_phase = 0;

static int s_warp_spin_active = 0;
static int s_warp_spin_y      = 0;
static int s_hold_b_sprint_enabled = 0;
static int s_boulder_dust_timer = 0;
static int s_tried_push_boulder = 0;
static int s_boulder_dust_px = -1;
static int s_boulder_dust_py = -1;
static int s_boulder_dust_subframe = 0;
static int s_boulder_dust_facing = 0;
static int s_boulder_dust_palette_flip = 0;
static int s_boulder_dust_step_dx = 0;
static int s_boulder_dust_step_dy = 0;
static int s_boulder_dust_pending = 0;
static int s_boulder_dust_pending_idx = -1;
static int s_boulder_dust_pending_facing = 0;
static int s_pushed_boulder_pending = 0;
static uint8_t s_pushed_boulder_map = 0;
static int s_pushed_boulder_x = 0;
static int s_pushed_boulder_y = 0;

#define SPRITE_BOULDER_ID 0x3F
#define BOULDER_DUST_OAM_BASE 72
#define BOULDER_DUST_TILE_BASE 0xFC

static int spinner_next_facing(int facing) {
    switch (facing & 3) {
    case 0: return 2;
    case 2: return 1;
    case 1: return 3;
    default: return 0;
    }
}

static void update_shadow_oam(void) {
    if (gLedgeStep == 0 || gWalkTimer == 0) {
        wShadowOAM[SHADOW_OAM_BASE + 0].y = 0;
        wShadowOAM[SHADOW_OAM_BASE + 1].y = 0;
        wShadowOAM[SHADOW_OAM_BASE + 2].y = 0;
        wShadowOAM[SHADOW_OAM_BASE + 3].y = 0;
        return;
    }
    int sx = ((int)wXCoord * 2 - gCamX)     * TILE_PX + gPlayerOffPxX;
    int sy = ((int)wYCoord * 2 + 1 - gCamY - 1) * TILE_PX + gPlayerOffPxY + SHADOW_Y_OFFSET;

    wShadowOAM[SHADOW_OAM_BASE + 0].y     = (uint8_t)(sy     + OAM_Y_OFS);
    wShadowOAM[SHADOW_OAM_BASE + 0].x     = (sx     + OAM_X_OFS);
    wShadowOAM[SHADOW_OAM_BASE + 0].tile  = SHADOW_TILE_IDX;
    wShadowOAM[SHADOW_OAM_BASE + 0].flags = OAM_FLAG_PALETTE;

    wShadowOAM[SHADOW_OAM_BASE + 1].y     = (uint8_t)(sy     + OAM_Y_OFS);
    wShadowOAM[SHADOW_OAM_BASE + 1].x     = (sx + 8 + OAM_X_OFS);
    wShadowOAM[SHADOW_OAM_BASE + 1].tile  = SHADOW_TILE_IDX;
    wShadowOAM[SHADOW_OAM_BASE + 1].flags = OAM_FLAG_PALETTE | OAM_FLAG_FLIP_X;

    wShadowOAM[SHADOW_OAM_BASE + 2].y     = (uint8_t)(sy + 8 + OAM_Y_OFS);
    wShadowOAM[SHADOW_OAM_BASE + 2].x     = (sx     + OAM_X_OFS);
    wShadowOAM[SHADOW_OAM_BASE + 2].tile  = SHADOW_TILE_IDX;
    wShadowOAM[SHADOW_OAM_BASE + 2].flags = OAM_FLAG_PALETTE | OAM_FLAG_FLIP_Y;

    wShadowOAM[SHADOW_OAM_BASE + 3].y     = (uint8_t)(sy + 8 + OAM_Y_OFS);
    wShadowOAM[SHADOW_OAM_BASE + 3].x     = (sx + 8 + OAM_X_OFS);
    wShadowOAM[SHADOW_OAM_BASE + 3].tile  = SHADOW_TILE_IDX;
    wShadowOAM[SHADOW_OAM_BASE + 3].flags = OAM_FLAG_PALETTE | OAM_FLAG_FLIP_X | OAM_FLAG_FLIP_Y;
}

static void clear_boulder_dust_oam(void) {
    for (int i = 0; i < 4; i++) {
        wShadowOAM[BOULDER_DUST_OAM_BASE + i].y = 0;
        wShadowOAM[BOULDER_DUST_OAM_BASE + i].x = 0;
        wShadowOAM[BOULDER_DUST_OAM_BASE + i].tile = 0;
        wShadowOAM[BOULDER_DUST_OAM_BASE + i].flags = 0;
    }
}

static void update_boulder_dust_oam(void) {
    if (s_boulder_dust_timer <= 0 || s_boulder_dust_px < 0 || s_boulder_dust_py < 0) {
        clear_boulder_dust_oam();
        return;
    }
    uint8_t flg = s_boulder_dust_palette_flip ? OAM_FLAG_PALETTE : 0;

    wShadowOAM[BOULDER_DUST_OAM_BASE + 0] = (oam_entry_t){ (uint8_t)(s_boulder_dust_py),     (uint8_t)(s_boulder_dust_px),     BOULDER_DUST_TILE_BASE + 0, flg };
    wShadowOAM[BOULDER_DUST_OAM_BASE + 1] = (oam_entry_t){ (uint8_t)(s_boulder_dust_py),     (uint8_t)(s_boulder_dust_px + 8), BOULDER_DUST_TILE_BASE + 1, flg };
    wShadowOAM[BOULDER_DUST_OAM_BASE + 2] = (oam_entry_t){ (uint8_t)(s_boulder_dust_py + 8), (uint8_t)(s_boulder_dust_px),     BOULDER_DUST_TILE_BASE + 2, flg };
    wShadowOAM[BOULDER_DUST_OAM_BASE + 3] = (oam_entry_t){ (uint8_t)(s_boulder_dust_py + 8), (uint8_t)(s_boulder_dust_px + 8), BOULDER_DUST_TILE_BASE + 3, flg };
}

static void start_boulder_dust_from_player_oam(int facing) {

    static const int kSpawnOffX[4] = { 8, 8, -24, 40 };
    static const int kSpawnOffY[4] = { 52, -12, 20, 20 };

    static const int kStepDX[4] = { 0, 0, 1, -1 };
    static const int kStepDY[4] = { -1, 1, 0, 0 };

    int player_state_x = (int)wShadowOAM[0].x - OAM_X_OFS;
    int player_state_y = (int)wShadowOAM[0].y - OAM_Y_OFS;
    s_boulder_dust_px = player_state_x + kSpawnOffX[facing & 3];
    s_boulder_dust_py = player_state_y + kSpawnOffY[facing & 3];
    s_boulder_dust_step_dx = kStepDX[facing & 3];
    s_boulder_dust_step_dy = kStepDY[facing & 3];
    s_boulder_dust_facing = facing & 3;
    s_boulder_dust_subframe = 0;
    s_boulder_dust_palette_flip = 0;
}

static int s_fishing_pose = 0;
void Player_SetFishingPose(int on) { s_fishing_pose = on ? 1 : 0; }

static void load_player_frame(int frame_idx) {
    const uint8_t (*fr)[PLAYER_TILE_BYTES];
    if (s_fishing_pose) {

        fr = gPlayerGfx[frame_idx];
    } else if (wWalkBikeSurfState == 2) {
        fr = gSeelSpriteGfx[frame_idx];
    } else if (Bicycle_ShouldUseBikeSprite()) {
        fr = gBikePlayerGfx[frame_idx];
    } else {
        fr = gPlayerGfx[frame_idx];
    }
    Display_LoadSpriteTile(PLAYER_TILE_BASE + 0, fr[0]);
    Display_LoadSpriteTile(PLAYER_TILE_BASE + 1, fr[1]);
    if (s_fishing_pose) {

        int f = gPlayerFacing & 3;
        int p = (f == 0) ? 0 : (f == 1) ? 2 : 4;
        Display_LoadSpriteTile(PLAYER_TILE_BASE + 2, gFishingPoseTiles[p]);
        Display_LoadSpriteTile(PLAYER_TILE_BASE + 3, gFishingPoseTiles[p + 1]);
    } else {
        Display_LoadSpriteTile(PLAYER_TILE_BASE + 2, fr[2]);
        Display_LoadSpriteTile(PLAYER_TILE_BASE + 3, fr[3]);
    }
}

static void update_player_oam(void) {
    int anim_idx = (gWalkTimer > 0 || s_wall_anim_active) ? gAnimFrameCounter : 0;
    if (s_spinner_spin_active || s_warp_spin_active) {

        anim_idx = 0;
    }
    const anim_entry_t *e = &kAnimTable[gPlayerFacing & 3][anim_idx];
    load_player_frame(e->frame);

    uint8_t tl, tr, bl, br;
    uint8_t flags = e->flip ? OAM_FLAG_FLIP_X : 0;
    if (e->flip) {
        tl = PLAYER_TILE_BASE + 1;  tr = PLAYER_TILE_BASE + 0;
        bl = PLAYER_TILE_BASE + 3;  br = PLAYER_TILE_BASE + 2;
    } else {
        tl = PLAYER_TILE_BASE + 0;  tr = PLAYER_TILE_BASE + 1;
        bl = PLAYER_TILE_BASE + 2;  br = PLAYER_TILE_BASE + 3;
    }

    int sx = ((int)wXCoord * 2 - gCamX)     * TILE_PX + gPlayerOffPxX;

    int sy = ((int)wYCoord * 2 + 1 - gCamY - 1) * TILE_PX + gPlayerOffPxY - 4;

    if (gLedgeStep > 0 && gArcFrame < 16) {
        sy += kLedgeArc[gArcFrame];
    }

    if (s_warp_spin_active)
        sy += s_warp_spin_y;

    wShadowOAM[0].y = (uint8_t)(sy     + OAM_Y_OFS);
    wShadowOAM[0].x = (sx     + OAM_X_OFS);
    wShadowOAM[0].tile = tl;  wShadowOAM[0].flags = flags;

    wShadowOAM[1].y = (uint8_t)(sy     + OAM_Y_OFS);
    wShadowOAM[1].x = (sx + 8 + OAM_X_OFS);
    wShadowOAM[1].tile = tr;  wShadowOAM[1].flags = flags;

    if (AmberScript_IsEnabled() && wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST) {
        uint8_t is_grass = 0;
        if (AmberScript_GetGrassOverrideAt((int)wXCoord * 2, (int)wYCoord * 2 + 1, &is_grass) && is_grass)
            flags |= OAM_FLAG_PRIORITY;
    } else if (wGrassTile != 0xFF && Map_GetGameTile((int)wXCoord, (int)wYCoord) == wGrassTile) {
        flags |= OAM_FLAG_PRIORITY;
    }

    wShadowOAM[2].y = (uint8_t)(sy + 8 + OAM_Y_OFS);
    wShadowOAM[2].x = (sx     + OAM_X_OFS);
    wShadowOAM[2].tile = bl;  wShadowOAM[2].flags = flags;

    wShadowOAM[3].y = (uint8_t)(sy + 8 + OAM_Y_OFS);
    wShadowOAM[3].x = (sx + 8 + OAM_X_OFS);
    wShadowOAM[3].tile = br;  wShadowOAM[3].flags = flags;

    update_shadow_oam();
    update_boulder_dust_oam();

    CrystalGrassRustle_BuildOAM(sx, sy);
}

static void advance_anim_tick_once(void) {
    if (++gIntraAnimFrame >= 4) {
        gIntraAnimFrame = 0;
        gAnimFrameCounter = (gAnimFrameCounter + 1) & 3;
    }
}

static void advance_anim(void) {
    advance_anim_tick_once();
}

static void begin_step(int nx, int ny, int dx, int dy) {

    int on_ledge = (gLedgeStep > 0);
    int step_frames = (Bicycle_IsSpeedupActive() && !on_ledge) ? BIKE_WALK_FRAMES : WALK_FRAMES;
    if (!on_ledge && s_hold_b_sprint_enabled && !gScriptedMovement && (hJoyHeld & PAD_B)) {
        step_frames /= 2;
        if (step_frames < 1) step_frames = 1;
    }
    const int step_px_mul = WALK_FRAMES / step_frames;
    int old_cam_x = gCamX;
    int old_cam_y = gCamY;

    wXCoord = (int16_t)nx;
    wYCoord = (int16_t)ny;
    Map_UpdateCamera();

    int tdx = dx * 2;
    int tdy = dy * 2;

    gWalkDX     = tdx;
    gWalkDY     = tdy;
    gBgScrollDX = gCamX - old_cam_x;
    gBgScrollDY = gCamY - old_cam_y;

    gScrollPxX = gBgScrollDX * TILE_PX;
    gScrollPxY = gBgScrollDY * TILE_PX;

    gPlayerOffPxX = (gBgScrollDX - tdx) * TILE_PX;
    gPlayerOffPxY = (gBgScrollDY - tdy) * TILE_PX;

    gWalkTimer = step_frames;
    gWalkStepPxMul = step_px_mul;

    if (step_frames >= 2) {
        gScrollPxX    -= gBgScrollDX * step_px_mul;
        gScrollPxY    -= gBgScrollDY * step_px_mul;
        gPlayerOffPxX += (tdx - gBgScrollDX) * step_px_mul;
        gPlayerOffPxY += (tdy - gBgScrollDY) * step_px_mul;
        advance_anim();
        if (gLedgeStep > 0) gArcFrame++;
        gWalkTimer = step_frames - 1;
    }

    CrystalGrassRustle_OnStep(nx, ny, step_frames);
}

static void reset_boulder_push_flags(void) {
    s_tried_push_boulder = 0;
}

static int try_push_boulder(int nx, int ny, int dx, int dy) {
    if (!FieldMove_IsStrengthActive()) return 0;
    if (s_boulder_dust_pending) return 0;
    if (s_boulder_dust_timer > 0) return 0;

    int boulder_idx = NPC_FindAtTile(nx, ny);
    if (boulder_idx < 0) {
        reset_boulder_push_flags();
        return 0;
    }
    if (NPC_GetSpriteId(boulder_idx) != SPRITE_BOULDER_ID || NPC_IsWalking(boulder_idx)) {
        reset_boulder_push_flags();
        return 0;
    }

    if (!s_tried_push_boulder) {
        s_tried_push_boulder = 1;
        return 0;
    }

    int bx = nx + dx;
    int by = ny + dy;
    int map_w = (int)wCurMapWidth * 2;
    int map_h = (int)wCurMapHeight * 2;

    if (bx < 0 || by < 0 || bx >= map_w || by >= map_h ||
        !Map_IsTilePassableAt(bx, by) ||
        Tile_IsPairBlocked(Map_GetGameTile(nx, ny), Map_GetGameTile(bx, by)) ||
        (AmberScript_IsEnabled() && AmberScript_IsPairBlockedAt(nx * 2, ny * 2 + 1, bx * 2, by * 2 + 1)) ||
        NPC_IsBlocked(bx, by)) {
        reset_boulder_push_flags();
        return 0;
    }

    NPC_DoScriptedStep(boulder_idx, gPlayerFacing & 3);

    {
        int pb_real = Map_CurrentRealId();
        s_pushed_boulder_map = (uint8_t)(pb_real >= 0 ? pb_real : (int)wCurMap);
    }
    s_pushed_boulder_x = bx;
    s_pushed_boulder_y = by;
    Audio_PlaySFX_PushBoulder();
    Display_LoadSpriteTile(BOULDER_DUST_TILE_BASE + 0, kBoulderSmokeTile);
    Display_LoadSpriteTile(BOULDER_DUST_TILE_BASE + 1, kBoulderSmokeTile);
    Display_LoadSpriteTile(BOULDER_DUST_TILE_BASE + 2, kBoulderSmokeTile);
    Display_LoadSpriteTile(BOULDER_DUST_TILE_BASE + 3, kBoulderSmokeTile);
    s_boulder_dust_pending = 1;
    s_boulder_dust_pending_idx = boulder_idx;
    s_boulder_dust_pending_facing = gPlayerFacing & 3;
    reset_boulder_push_flags();
    return 1;
}

int Player_ConsumePushedBoulderEvent(uint8_t *out_map, int *out_x, int *out_y) {
    if (!s_pushed_boulder_pending) return 0;
    s_pushed_boulder_pending = 0;
    if (out_map) *out_map = s_pushed_boulder_map;
    if (out_x) *out_x = s_pushed_boulder_x;
    if (out_y) *out_y = s_pushed_boulder_y;
    return 1;
}

void Player_ForceStepDown(void) {
    gPlayerFacing = 0;
    begin_step((int)wXCoord, (int)wYCoord + 1, 0, 1);
}

static int can_force_step_to(int nx, int ny) {
    if (EliteFourScripts_BlocksMovementTo(nx, ny)) return 0;
    if (!Map_IsTilePassableAt(nx, ny)) return 0;
    if (Tile_IsPairBlocked(Map_GetGameTile((int)wXCoord, (int)wYCoord),
                           Map_GetGameTile(nx, ny))) return 0;
    if (AmberScript_IsEnabled() &&
        AmberScript_IsPairBlockedAt((int)wXCoord * 2, (int)wYCoord * 2 + 1, nx * 2, ny * 2 + 1)) return 0;
    if (NPC_IsBlocked(nx, ny)) return 0;

    if (Warp_HasEventAt(nx, ny)) return 0;
    return 1;
}

void Player_ForceStepFromDoor(void) {

    int nx = (int)wXCoord;
    int ny = (int)wYCoord + 1;
    gPlayerFacing = 0;
    if (can_force_step_to(nx, ny))
        begin_step(nx, ny, 0, 1);
}

void Player_ForceStepFromDoorFacingUp(void) {
    int nx = (int)wXCoord;
    int ny = (int)wYCoord - 1;
    gPlayerFacing = 1;
    if (can_force_step_to(nx, ny))
        begin_step(nx, ny, 0, -1);
}

void Player_DoScriptedStep(int dir) {
    static const int ddx[4] = { 0,  0, -1,  1};
    static const int ddy[4] = { 1, -1,  0,  0};
    int dx = ddx[dir & 3];
    int dy = ddy[dir & 3];
    int nx = (int)wXCoord + dx;
    int ny = (int)wYCoord + dy;

    gPlayerFacing = dir;
    begin_step(nx, ny, dx, dy);
}

void Player_DoScriptedStepWithLedge(int dir) {
    static const int ddx[4] = { 0,  0, -1,  1};
    static const int ddy[4] = { 1, -1,  0,  0};
    int dx = ddx[dir & 3];
    int dy = ddy[dir & 3];
    int nx = (int)wXCoord + dx;
    int ny = (int)wYCoord + dy;

    gPlayerFacing = dir & 3;

    if (is_ledge_jump(nx, ny)) {
        gLedgeStep = 1;
        gLedgeDX   = dx;
        gLedgeDY   = dy;
        gArcFrame  = 0;
        Audio_PlaySFX_Ledge();
    }

    begin_step(nx, ny, dx, dy);
}

void Player_StartSimulatedMovement(const int8_t *seq, int last_idx) {
    s_sim_seq = seq;
    s_sim_idx = last_idx;
}

int Player_IsSimulatingMovement(void) {
    return (s_sim_seq != 0 && s_sim_idx >= 0);
}

int Player_GetSimulatedStepsRemaining(void) {
    return Player_IsSimulatingMovement() ? s_sim_idx : -1;
}

int Player_GetSimulatedHeldDir(void) {
    return s_sim_current_dir;
}

void Player_SetSpinnerSpin(int enabled) {
    s_spinner_spin_active = enabled ? 1 : 0;
    s_spinner_spin_phase = 0;
}

void Player_SetWarpSpin(int enabled) {
    s_warp_spin_active = enabled ? 1 : 0;
    if (!enabled) s_warp_spin_y = 0;
}

void Player_WarpSpinStep(void) {
    gPlayerFacing = spinner_next_facing(gPlayerFacing);
}

void Player_SetWarpSpinY(int dy) {
    s_warp_spin_y = dy;
}

void Player_SetPos(int16_t x, int16_t y) {
    wXCoord       = x;
    wYCoord       = y;
    gWalkTimer    = 0;
    gScrollPxX    = 0;
    gScrollPxY    = 0;
    gPlayerOffPxX = 0;
    gPlayerOffPxY = 0;
    gLedgeStep    = 0;
    gArcFrame     = 0;
    Map_UpdateCamera();
}

void Player_Init(uint8_t x, uint8_t y) {
    wXCoord           = x;
    wYCoord           = y;
    gWalkTimer        = 0;
    gScrollPxX        = 0;
    gScrollPxY        = 0;
    gPlayerOffPxX     = 0;
    gPlayerOffPxY     = 0;
    gIntraAnimFrame   = 0;
    gAnimFrameCounter = 0;
    gLedgeStep        = 0;
    gArcFrame         = 0;

    s_check_180       = 1;
    s_moving_dir      = PLAYER_DIR_NONE;
    s_last_stop_dir   = PLAYER_DIR_NONE;
    s_turning         = 0;
    s_turn_hold       = 0;
    Map_UpdateCamera();
    Display_LoadSpriteTile(SHADOW_TILE_IDX, kShadowTile);
    CrystalGrassRustle_LoadTile();
    CrystalGrassRustle_Reset();
    update_player_oam();
}

int Player_IsTurning(void) {
    return s_turning;
}

void Player_Update(void) {
    int has_forced_sim_dir = 0;
    int forced_sim_dir = 0;

    if (s_boulder_dust_pending) {
        if (s_boulder_dust_pending_idx >= 0 && NPC_IsWalking(s_boulder_dust_pending_idx)) {

            hJoyHeld = 0;
            hJoyPressed = 0;
            hJoyReleased = 0;
            update_player_oam();
            return;
        }
        if (s_boulder_dust_pending_idx < 0 || !NPC_IsWalking(s_boulder_dust_pending_idx)) {
            start_boulder_dust_from_player_oam(s_boulder_dust_pending_facing);
            s_boulder_dust_timer = 8;

            s_pushed_boulder_pending = 1;

            Audio_PlaySFX_Cut();
            s_boulder_dust_pending = 0;
            s_boulder_dust_pending_idx = -1;
        }
    }

    if (s_boulder_dust_timer > 0) {
        if (++s_boulder_dust_subframe >= 3) {
            s_boulder_dust_subframe = 0;
            s_boulder_dust_px += s_boulder_dust_step_dx;
            s_boulder_dust_py += s_boulder_dust_step_dy;
            s_boulder_dust_palette_flip ^= 1;
            s_boulder_dust_timer--;
        }
        if (s_boulder_dust_timer == 0) {

            hJoyHeld = 0;
            hJoyPressed = 0;
            hJoyReleased = 0;
            Audio_PlaySFX_Cut();
            s_boulder_dust_px = -1;
            s_boulder_dust_py = -1;
            s_boulder_dust_subframe = 0;
            s_boulder_dust_palette_flip = 0;
            s_boulder_dust_pending = 0;
            s_boulder_dust_pending_idx = -1;
        }

        if (s_boulder_dust_timer > 0) {
            hJoyHeld = 0;
            hJoyPressed = 0;
            hJoyReleased = 0;
            update_player_oam();
            return;
        }
    }

    if (gWalkTimer > 0) {
        s_turning = 0;
        if (s_spinner_spin_active) {

            if ((s_spinner_spin_phase++ & 1) == 0) {
                gPlayerFacing = spinner_next_facing(gPlayerFacing);
            }
        }

        gScrollPxX    -= gBgScrollDX * gWalkStepPxMul;
        gScrollPxY    -= gBgScrollDY * gWalkStepPxMul;

        gPlayerOffPxX += (gWalkDX - gBgScrollDX) * gWalkStepPxMul;
        gPlayerOffPxY += (gWalkDY - gBgScrollDY) * gWalkStepPxMul;
        if (!s_spinner_spin_active) {
            advance_anim();
        }

        CrystalGrassRustle_Tick();
        if (--gWalkTimer == 0) {

            gScrollPxX = gScrollPxY = gPlayerOffPxX = gPlayerOffPxY = 0;
            gStepJustCompleted = 1;
            if (gLedgeStep == 1) {

                gLedgeStep = 2;
                begin_step((int)wXCoord + gLedgeDX, (int)wYCoord + gLedgeDY,
                           gLedgeDX, gLedgeDY);
            } else {

                if (!Warp_CheckDungeonHole()) {
                    Warp_Check();
                }
                s_sim_current_dir = -1;
            }
        }

        if (gLedgeStep > 0) gArcFrame++;
        update_player_oam();

        if (gWalkTimer == 0 && gLedgeStep == 2) gLedgeStep = 0;
        return;
    }

    if (gScriptedMovement) {
        update_player_oam();
        return;
    }

    if (Player_IsSimulatingMovement()) {
        forced_sim_dir = (int)s_sim_seq[s_sim_idx--];
        has_forced_sim_dir = 1;
        s_sim_current_dir = forced_sim_dir & 3;
        if (wCurMap == 0xA1 || wCurMap == 0xA2) {
            printf("[seafoam sim] map=%u pos=(%d,%d) dir=%d idx=%d\n",
                   (unsigned)wCurMap, (int)wXCoord, (int)wYCoord, forced_sim_dir, s_sim_idx + 1);
        }
        if (s_sim_idx < 0) {
            s_sim_seq = 0;

            Player_IgnoreInputFrames(WALK_FRAMES + 2);
        }

        hJoyHeld = 0;
        hJoyPressed = 0;
        hJoyReleased = 0;
    }

    if (gInputIgnoreFrames > 0 && !has_forced_sim_dir) {
        static int last_printed = -1;
        if (gInputIgnoreFrames != last_printed) {
            printf("[input] suppressed, %d frames remaining\n", gInputIgnoreFrames);
            last_printed = gInputIgnoreFrames;
        }
        gInputIgnoreFrames--;
        update_player_oam();
        return;
    }

    if (s_turn_hold > 0) {
        s_turn_hold--;
        update_player_oam();
        return;
    }

    int dx = 0, dy = 0;
    int pressed_dir = PLAYER_DIR_NONE;
    if (has_forced_sim_dir) {
        switch (forced_sim_dir & 3) {
            case 3: dx =  1; gPlayerFacing = 3; pressed_dir = PLAYER_DIR_RIGHT; break;
            case 2: dx = -1; gPlayerFacing = 2; pressed_dir = PLAYER_DIR_LEFT;  break;
            case 0: dy =  1; gPlayerFacing = 0; pressed_dir = PLAYER_DIR_DOWN;  break;
            case 1: dy = -1; gPlayerFacing = 1; pressed_dir = PLAYER_DIR_UP;    break;
            default: break;
        }

    } else if      (hJoyHeld & PAD_DOWN)  { dy =  1; gPlayerFacing = 0; pressed_dir = PLAYER_DIR_DOWN;  }
    else if (hJoyHeld & PAD_UP)    { dy = -1; gPlayerFacing = 1; pressed_dir = PLAYER_DIR_UP;    }
    else if (hJoyHeld & PAD_LEFT)  { dx = -1; gPlayerFacing = 2; pressed_dir = PLAYER_DIR_LEFT;  }
    else if (hJoyHeld & PAD_RIGHT) { dx =  1; gPlayerFacing = 3; pressed_dir = PLAYER_DIR_RIGHT; }

    if (!dx && !dy) {

        s_turning   = 0;
        s_check_180 = 1;
        if (s_moving_dir != PLAYER_DIR_NONE) {
            s_last_stop_dir = s_moving_dir;
            s_moving_dir    = PLAYER_DIR_NONE;
        }

        gIntraAnimFrame = gAnimFrameCounter = s_wall_anim_active = 0;
        reset_boulder_push_flags();
        update_player_oam();
        return;
    }

    if (!has_forced_sim_dir && s_check_180 && pressed_dir != s_last_stop_dir) {
        s_check_180 = 0;
        s_moving_dir = pressed_dir;
        s_turning    = 1;
        s_turn_hold  = TURN_HOLD_TICKS;

        update_player_oam();
        return;
    }

    s_moving_dir = pressed_dir;
    s_turning    = 0;

    int nx = (int)wXCoord + dx;
    int ny = (int)wYCoord + dy;

    if (has_forced_sim_dir &&
        nx >= 0 && ny >= 0 &&
        nx < (int)wCurMapWidth * 2 &&
        ny < (int)wCurMapHeight * 2) {
        begin_step(nx, ny, dx, dy);
        update_player_oam();
        return;
    }

    if (nx < 0 || ny < 0 ||
        nx >= (int)wCurMapWidth  * 2 ||
        ny >= (int)wCurMapHeight * 2)
    {

        if (!gNoClip) {
            if (wWalkBikeSurfState == 2) {
                player_surf_step_t surf_step = Player_ClassifySurfStep(nx, ny);
                if (surf_step == PLAYER_SURF_STEP_INVALID && !has_forced_sim_dir) {
                    Audio_PlaySFX_Collision();
                    s_wall_anim_active = 1;
                    advance_anim();
                    update_player_oam();
                    return;
                }
                if (surf_step == PLAYER_SURF_STEP_LAND) {
                    wWalkBikeSurfState = 0;
                    Bicycle_PlayDefaultMusic();
                }
            } else if (is_connection_tileset() && !Map_IsTilePassableAt(nx, ny) &&
                       !Warp_HasEventAt((int)wXCoord, (int)wYCoord)) {

                Audio_PlaySFX_Collision();
                s_wall_anim_active = 1;
                advance_anim();
                update_player_oam();
                return;
            }
        }

        Map_PreBuildScrollStep(dx, dy);
        if (Connection_Check(dx, dy)) {

            Map_UpdateCamera();
            NPC_Load();
            PalletScripts_OnMapLoad();
            OaksLabScripts_OnMapLoad();
            ViridianMartScripts_OnMapLoad();
            Route24Scripts_OnMapLoad();
            BluesHouseScripts_OnMapLoad();
            BillsHouseScripts_OnMapLoad();
            SeafoamScripts_OnMapLoad();
            Trainer_LoadMap();
            Gate_LoadMap();
            gWalkDX       = dx * 2;
            gWalkDY       = dy * 2;
            gBgScrollDX   = dx * 2;
            gBgScrollDY   = dy * 2;
            gScrollPxX    = dx * 2 * TILE_PX;
            gScrollPxY    = dy * 2 * TILE_PX;
            gPlayerOffPxX = 0;
            gPlayerOffPxY = 0;
            gWalkTimer    = Bicycle_IsSpeedupActive() ? BIKE_WALK_FRAMES : WALK_FRAMES;
            gWalkStepPxMul = WALK_FRAMES / gWalkTimer;

            if (gWalkTimer >= 2) {
                gScrollPxX    -= gBgScrollDX * gWalkStepPxMul;
                gScrollPxY    -= gBgScrollDY * gWalkStepPxMul;
                gPlayerOffPxX += (gWalkDX - gBgScrollDX) * gWalkStepPxMul;
                gPlayerOffPxY += (gWalkDY - gBgScrollDY) * gWalkStepPxMul;
                advance_anim();
                gWalkTimer--;
            }
        } else {

            DBG_PRINTF("[oob] map=%d pos=(%d,%d) dir=(%d,%d) → Warp_CheckAtMapBoundary\n",
                   wCurMap, wXCoord, wYCoord, dx, dy);
            Warp_CheckAtMapBoundary();
        }
        update_player_oam();
        return;
    }

    if (!gNoClip && NPC_IsBlocked(nx, ny)) {
        if (try_push_boulder(nx, ny, dx, dy)) {

            update_player_oam();
            return;
        }
        update_player_oam();
        return;
    }

    if (is_ledge_jump(nx, ny)) {
        gLedgeStep = 1;
        gLedgeDX   = dx;
        gLedgeDY   = dy;
        gArcFrame  = 0;
        Audio_PlaySFX_Ledge();
        begin_step(nx, ny, dx, dy);
        update_player_oam();
        return;
    }

    if (wWalkBikeSurfState != 2 && !gNoClip) {
        int elite4_blocked = EliteFourScripts_BlocksMovementTo(nx, ny);
        int tile_blocked = !Map_IsTilePassableAt(nx, ny);
        if (!elite4_blocked && !tile_blocked) {

        } else if (!elite4_blocked && Warp_HasEventAt(nx, ny)) {

            begin_step(nx, ny, dx, dy);
            update_player_oam();
            return;
        } else {
            Audio_PlaySFX_Collision();
            s_wall_anim_active = 1;
            advance_anim();
            Warp_CheckCollision();
            update_player_oam();
            return;
        }
    }

    if (wWalkBikeSurfState == 2) {
        if (Warp_HasEventAt(nx, ny)) {
            begin_step(nx, ny, dx, dy);
            update_player_oam();
            return;
        }

        player_surf_step_t surf_step = Player_ClassifySurfStep(nx, ny);
        if (!gNoClip && surf_step == PLAYER_SURF_STEP_INVALID && !has_forced_sim_dir) {
            if (wCurMap == 0xA1 || wCurMap == 0xA2) {
                printf("[seafoam surf block] map=%u from=(%d,%d) to=(%d,%d) cur_tile=0x%02X next_tile=0x%02X dir=(%d,%d)\n",
                       (unsigned)wCurMap,
                       (int)wXCoord, (int)wYCoord, nx, ny,
                       (unsigned)Map_GetGameTile((int)wXCoord, (int)wYCoord),
                       (unsigned)Map_GetGameTile(nx, ny),
                       dx, dy);
            }
            Audio_PlaySFX_Collision();
            s_wall_anim_active = 1;
            advance_anim();
            update_player_oam();
            return;
        }

        if (surf_step == PLAYER_SURF_STEP_LAND) {
            wWalkBikeSurfState = 0;
            Bicycle_PlayDefaultMusic();
        }

        begin_step(nx, ny, dx, dy);
        update_player_oam();
        return;
    }

    if (!gNoClip &&
        (Tile_IsPairBlocked(Map_GetGameTile((int)wXCoord, (int)wYCoord), Map_GetGameTile(nx, ny)) ||
         (AmberScript_IsEnabled() &&
          AmberScript_IsPairBlockedAt((int)wXCoord * 2, (int)wYCoord * 2 + 1, nx * 2, ny * 2 + 1)))) {
        Audio_PlaySFX_Collision();
        s_wall_anim_active = 1;
        advance_anim();
        update_player_oam();
        return;
    }

    begin_step(nx, ny, dx, dy);
    update_player_oam();
}

int Player_GetLedgeDir(uint8_t tile_id) {
    for (int i = 0; i < NUM_LEDGE_ENTRIES; i++) {
        if (kLedgeTiles[i].ledge_tile == tile_id)
            return kLedgeTiles[i].facing;
    }
    return -1;
}

void Player_GetFacingTile(int *out_x, int *out_y) {
    static const int ddx[4] = { 0,  0, -1, 1 };
    static const int ddy[4] = { 1, -1,  0, 0 };
    *out_x = (int)wXCoord + ddx[gPlayerFacing & 3];
    *out_y = (int)wYCoord + ddy[gPlayerFacing & 3];
}

int Player_IsLedgeJumping(void) { return gLedgeStep != 0; }

int Player_IsMoving(void) {
    return gWalkTimer > 0;
}

void Player_SetHoldBSprintEnabled(int enabled) {
    s_hold_b_sprint_enabled = enabled ? 1 : 0;
}

int Player_GetHoldBSprintEnabled(void) {
    return s_hold_b_sprint_enabled;
}

void Player_SyncOAM(void) {
    update_player_oam();
}

void Player_HideIfOverUI(void) {
    int nx = (int)wXCoord * 2 - gCamX;
    int ny = (int)wYCoord * 2 + 1 - gCamY;

    if (nx < 0 || nx >= Map_ViewTilesW() || ny < 0 || ny >= SCREEN_HEIGHT) return;
    if (gScrollTileMap[(ny + 2) * SCROLL_MAP_W + (nx + 2)] >= 96) {
        wShadowOAM[0].y = 0;
        wShadowOAM[1].y = 0;
        wShadowOAM[2].y = 0;
        wShadowOAM[3].y = 0;
    }
}
