
#include "npc.h"
#include "overworld.h"
#include "player.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../game/constants.h"
#include "../data/event_data.h"
#include "../data/sprite_data.h"
#include "amberscript_mapbank.h"
#include "amberscript_core.h"
#include "amberscript_tilemod.h"
#include "../data/map_data.h"
#include "debug_trace.h"
#include "gbc_color.h"
#include "crystal_sprites.h"
#include <string.h>
#include <stdio.h>

#define OAM_XFLIP  0x20

#define NPC_WALK_FRAMES_SCRIPTED   8
#define NPC_WALK_FRAMES_RANDOM    16
#define NPC_STEP_PX_SCRIPTED       2
#define NPC_STEP_PX_RANDOM         1

#define NPC_MOVE_DELAY_MASK 0x7F
#define NPC_MOVE_DELAY_ZERO 256

#define MAX_NPCS       16
#define NPC_TILE_BASE   0
#define NPC_OAM_BASE    4
#define SPRITE_BOULDER_ID 0x3F

static int      npc_count = 0;
static int      npc_last_interacted = -1;
static uint8_t  npc_sprite[MAX_NPCS];
static uint8_t  npc_x[MAX_NPCS];
static uint8_t  npc_y[MAX_NPCS];
static uint8_t  npc_facing[MAX_NPCS];

static uint8_t  npc_crystal_pal[MAX_NPCS];
static int      npc_src_idx[MAX_NPCS];
static uint8_t  npc_move_type[MAX_NPCS];
static int      npc_move_timer[MAX_NPCS];

extern uint8_t BattleRandom(void);

static int npc_next_move_delay(void) {
    int d = (int)(BattleRandom() & NPC_MOVE_DELAY_MASK);
    return d ? d : NPC_MOVE_DELAY_ZERO;
}
static int      npc_walk_frames[MAX_NPCS];
static int      npc_walk_total[MAX_NPCS];
static int      npc_step_px[MAX_NPCS];
static int      npc_px_off[MAX_NPCS];
static int      npc_py_off[MAX_NPCS];
static uint8_t  npc_hidden[MAX_NPCS];
static uint8_t  npc_debug_spawned[MAX_NPCS];

static int facing_tile_base(int facing) {
    if (facing == 1) return 4;
    if (facing >= 2) return 8;
    return 0;
}

static void reload_npc_tiles_ex(int i, int walking) {
    uint8_t base  = (uint8_t)(NPC_TILE_BASE + i * 4);
    int     sid   = npc_sprite[i] < NUM_SPRITES ? npc_sprite[i] : 0;
    int     tb    = facing_tile_base(npc_facing[i]) + (walking ? SPRITE_TILES / 2 : 0);
    const uint8_t *gfx = gSpriteGfx[sid];

    if (PKS_SPRITE_IS_CRYSTAL(npc_sprite[i])) {
        int ci = npc_sprite[i] - PKS_CRYSTAL_SPRITE_BASE;
        if (ci >= 0 && ci < CRYSTAL_NUM_SPRITES) gfx = gCrystalSpriteGfx[ci];
    }
    Display_LoadSpriteTile(base + 0, gfx + (tb + 0) * 16);
    Display_LoadSpriteTile(base + 1, gfx + (tb + 1) * 16);
    Display_LoadSpriteTile(base + 2, gfx + (tb + 2) * 16);
    Display_LoadSpriteTile(base + 3, gfx + (tb + 3) * 16);
}

static void reload_npc_tiles(int i) { reload_npc_tiles_ex(i, 0); }

static void apply_npc_oam_facing(int i) {
    uint8_t tile_base = (uint8_t)(NPC_TILE_BASE + i * 4);
    int     oam       = NPC_OAM_BASE + i * 4;
    uint8_t flg       = (npc_facing[i] == 3) ? OAM_XFLIP : 0;

    if (GbcColor_OverworldStyle() == GBC_OVERWORLD_RED_AUTOCOLOR) {

    } else if (npc_crystal_pal[i] > 0) {
        flg |= (uint8_t)((npc_crystal_pal[i] - 1) & 7);
    } else {
        flg |= GbcColor_PalForSprite(npc_sprite[i], i);
    }
    for (int s = 0; s < 4; s++) {
        wShadowOAM[oam + s].tile  = tile_base + s;
        wShadowOAM[oam + s].flags = flg;
    }
}

static void npc_apply_visual_state(int i) {
    int walking = 0;
    if (npc_walk_frames[i] > 0 && npc_walk_total[i] > 0) {
        int total = npc_walk_total[i];
        int elapsed = total - npc_walk_frames[i];
        int divisor = total / 4;
        if (divisor <= 0) divisor = 1;
        int counter = (elapsed > 0) ? ((elapsed - 1) / divisor) : 0;
        walking = counter & 1;
    }
    if (npc_sprite[i] == SPRITE_BOULDER_ID) walking = 0;
    reload_npc_tiles_ex(i, walking);
    apply_npc_oam_facing(i);
}

void NPC_ReloadTiles(void) {
    for (int i = 0; i < npc_count; i++) {
        if (!npc_hidden[i]) {
            reload_npc_tiles(i);
            apply_npc_oam_facing(i);
        }
    }
}

void NPC_Load(void) {
    for (int i = 0; i < MAX_NPCS * 4; i++) {
        wShadowOAM[NPC_OAM_BASE + i].y = 0;
        wShadowOAM[NPC_OAM_BASE + i].x = 0;
    }
    for (int i = 0; i < MAX_NPCS; i++) { npc_hidden[i] = 0; npc_debug_spawned[i] = 0; npc_src_idx[i] = i; npc_crystal_pal[i] = 0; }

    for (int i = 0; i < MAX_NPCS; i++) {
        npc_walk_frames[i] = 0;
        npc_walk_total[i]  = 0;
        npc_step_px[i]     = 0;
        npc_px_off[i]      = 0;
        npc_py_off[i]      = 0;
    }
    Trace_Emit(TRACE_NPC, "\"ev\":\"load\",\"map\":%d,\"prev_count\":%d",
               (int)wCurMap, npc_count);
    npc_count = 0;

    if (wCurMap >= NUM_MAPS) return;

    const map_events_t *ev = AmberScript_GetMapEventsForFreshLoad(wCurMap);

    int n = 0;
    if (ev->npcs) {
        n = ev->num_npcs < MAX_NPCS ? ev->num_npcs : MAX_NPCS;
        for (int i = 0; i < n; i++) {
            const npc_event_t *npc = &ev->npcs[i];
            uint8_t facing = (uint8_t)(npc->facing & 3);
            if (ev->trainers) {
                for (int t = 0; t < ev->num_trainers; t++) {
                    if (ev->trainers[t].npc_idx == i) {
                        facing = (uint8_t)(ev->trainers[t].facing & 3);
                        break;
                    }
                }
            }
            npc_sprite[i]      = npc->sprite_id;
            npc_x[i]           = (uint8_t)npc->x;
            npc_y[i]           = (uint8_t)npc->y;
            npc_facing[i]      = facing;

            if (wCurMap >= PKS_VIRTUAL_MAP_FIRST) npc_src_idx[i] = (int)npc->src_idx;
            npc_crystal_pal[i] = npc->crystal_pal;

            if (npc->starts_hidden) npc_hidden[i] = 1;
            npc_move_type[i]   = npc->movement;
            npc_move_timer[i]  = 0;
            npc_walk_frames[i] = 0;
            npc_px_off[i]      = 0;
            npc_py_off[i]      = 0;
            reload_npc_tiles(i);
            apply_npc_oam_facing(i);
        }
    }

    if (ev->items) {
        int ni = ev->num_items < (MAX_NPCS - n) ? ev->num_items : (MAX_NPCS - n);
        for (int i = 0; i < ni; i++) {
            int slot = n + i;
            const item_event_t *it = &ev->items[i];

            uint16_t pks_flag_bit = (wCurMap >= PKS_VIRTUAL_MAP_FIRST) ? AmberScript_GetItemFlagBitAt(wCurMap, it->src_idx) : 0;
            if ((wCurMap < PKS_VIRTUAL_MAP_FIRST && (wPickedUpItems[wCurMap] & (1u << i))) ||
                (pks_flag_bit != 0 && CheckEvent(pks_flag_bit))) {

                npc_hidden[slot] = 1;
                continue;
            }
            npc_sprite[slot]      = 0x3D;
            npc_x[slot]           = (uint8_t)it->x;
            npc_y[slot]           = (uint8_t)it->y;
            npc_facing[slot]      = 0;
            npc_move_type[slot]   = 0;
            npc_move_timer[slot]  = 0;
            npc_walk_frames[slot] = 0;
            npc_px_off[slot]      = 0;
            npc_py_off[slot]      = 0;
            reload_npc_tiles(slot);
            apply_npc_oam_facing(slot);
        }
        n += ni;
    }

    npc_count = n;
}

void NPC_HideSprite(int npc_slot_idx) {
    if (npc_slot_idx < 0 || npc_slot_idx >= MAX_NPCS) return;
    Trace_Emit(TRACE_NPC, "\"ev\":\"hide\",\"i\":%d,\"spr\":%d,\"x\":%d,\"y\":%d",
               npc_slot_idx, npc_sprite[npc_slot_idx],
               npc_x[npc_slot_idx], npc_y[npc_slot_idx]);
    npc_hidden[npc_slot_idx] = 1;
    int oam = NPC_OAM_BASE + npc_slot_idx * 4;
    for (int s = 0; s < 4; s++)
        wShadowOAM[oam + s].y = 0;
}

int NPC_IsHidden(int npc_slot_idx) {
    if (npc_slot_idx < 0 || npc_slot_idx >= MAX_NPCS) return 1;
    return npc_hidden[npc_slot_idx];
}

void NPC_ShowSprite(int npc_slot_idx) {
    if (npc_slot_idx < 0 || npc_slot_idx >= MAX_NPCS) return;
    Trace_Emit(TRACE_NPC, "\"ev\":\"show\",\"i\":%d,\"spr\":%d",
               npc_slot_idx, npc_sprite[npc_slot_idx]);
    npc_hidden[npc_slot_idx] = 0;
}

void NPC_DebugDump(void) {
    static const char *fn[4] = { "down", "up", "left", "right" };
    printf("[npcdump] count=%d wGrassTile=%d camX=%d camY=%d\n", npc_count, wGrassTile, gCamX, gCamY);
    for (int i = 0; i < npc_count; i++) {
        int oam = NPC_OAM_BASE + i * 4;
        printf("[npcdump]  i=%d sprite=%d cell=(%d,%d) facing=%s hidden=%d\n",
               i, npc_sprite[i], npc_x[i], npc_y[i], fn[npc_facing[i] & 3], npc_hidden[i]);
        for (int s = 0; s < 4; s++)
            printf("[npcdump]      oam%d: y=%3d x=%3d tile=%3d flags=0x%02X%s%s\n",
                   s, wShadowOAM[oam + s].y, wShadowOAM[oam + s].x, wShadowOAM[oam + s].tile,
                   wShadowOAM[oam + s].flags,
                   (wShadowOAM[oam + s].flags & OAM_XFLIP) ? " XFLIP" : "",
                   (wShadowOAM[oam + s].flags & OAM_FLAG_PRIORITY) ? " PRIO" : "");
    }
    fflush(stdout);
}

int NPC_GetMoveType(int npc_idx) {
    if (npc_idx < 0 || npc_idx >= MAX_NPCS) return -1;
    return npc_move_type[npc_idx];
}

void NPC_SetMoveType(int npc_idx, int move_type) {
    if (npc_idx < 0 || npc_idx >= MAX_NPCS) return;
    npc_move_type[npc_idx] = (uint8_t)move_type;

}

void NPC_HideAll(void) {
    for (int i = 0; i < MAX_NPCS; i++)
        NPC_HideSprite(i);
}

void NPC_HideOAM(void) {
    for (int i = 0; i < MAX_NPCS * 4; i++)
        wShadowOAM[NPC_OAM_BASE + i].y = 0;
}

void NPC_ShowAll(void) {
    for (int i = 0; i < MAX_NPCS; i++)
        npc_hidden[i] = 0;
}

#define MAP_TILESET_SIZE 96
void NPC_HideOverUITiles(void) {
    for (int i = 0; i < npc_count; i++) {
        if (npc_hidden[i]) continue;
        int nx = (int)npc_x[i] * 2     - gCamX;
        int ny = (int)npc_y[i] * 2 + 1 - gCamY;

        for (int dy = -1; dy <= 0; dy++) {
            for (int dx = 0; dx <= 1; dx++) {
                int c = nx + dx;
                int r = ny + dy;
                if (c < 0 || c >= Map_ViewTilesW() || r < 0 || r >= SCREEN_HEIGHT)
                    continue;
                if (gScrollTileMap[(r + 2) * SCROLL_MAP_W + (c + 2)] >= MAP_TILESET_SIZE) {
                    int oam = NPC_OAM_BASE + i * 4;
                    for (int s = 0; s < 4; s++)
                        wShadowOAM[oam + s].y = 0;
                    goto next_npc;
                }
            }
        }
        next_npc:;
    }
}

int NPC_SpriteCanFacePlayer(uint8_t sprite_id) {
    if (PKS_SPRITE_IS_CRYSTAL(sprite_id)) {
        int ci = (int)sprite_id - PKS_CRYSTAL_SPRITE_BASE;
        if (ci < 0 || ci >= CRYSTAL_NUM_SPRITES) return 0;
        return gCrystalSpriteType[ci] != CRYSTAL_SPRITE_STILL;
    }
    return sprite_id < 0x3D;
}

void NPC_FacePlayer(int i) {
    if (i < 0 || i >= npc_count) return;
    npc_last_interacted = i;

    int dx = (int)wXCoord - (int)npc_x[i];
    int dy = (int)wYCoord - (int)npc_y[i];
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    if (dx == 0 && dy == 0) {

    } else if (adx > ady) {
        npc_facing[i] = (dx > 0) ? 3 : 2;
    } else {
        npc_facing[i] = (dy > 0) ? 0 : 1;
    }
    reload_npc_tiles(i);
    apply_npc_oam_facing(i);
}

void NPC_SetFacing(int i, int facing) {
    if (i < 0 || i >= npc_count) return;
    npc_facing[i] = (uint8_t)(facing & 3);
    reload_npc_tiles(i);
    apply_npc_oam_facing(i);
}

int NPC_GetFacing(int i) {
    if (i < 0 || i >= npc_count) return 0;
    return (int)(npc_facing[i] & 3);
}

int NPC_GetLastInteracted(void) {
    return npc_last_interacted;
}

void NPC_GetScreenPos(int i, int *px, int *py) {
    if (i < 0 || i >= npc_count) { *px = *py = 0; return; }
    int oam = NPC_OAM_BASE + i * 4;

    int ref_slot = (npc_facing[i] == 3) ? 0 : 1;
    *py = (int)wShadowOAM[oam + 0].y - OAM_Y_OFS;
    *px = (int)wShadowOAM[oam + ref_slot].x - OAM_X_OFS;
}

void NPC_GetTilePos(int i, int *tx, int *ty) {
    if (i < 0 || i >= npc_count) { *tx = *ty = 0; return; }
    *tx = (int)npc_x[i];
    *ty = (int)npc_y[i];
}

int NPC_GetDeclIdx(int i) {
    if (i < 0 || i >= npc_count) return -1;
    return npc_src_idx[i];
}

void NPC_SetTilePos(int i, int tx, int ty) {
    if (i < 0 || i >= npc_count) return;
    npc_x[i]        = (uint8_t)tx;
    npc_y[i]        = (uint8_t)ty;
    npc_px_off[i]   = 0;
    npc_py_off[i]   = 0;
    npc_walk_frames[i] = 0;
}

int NPC_IsWalking(int i) {
    if (i < 0 || i >= npc_count) return 0;
    return npc_walk_frames[i] > 0;
}

int NPC_GetCount(void) { return npc_count; }

int NPC_DebugSpawn(uint8_t sprite_id, int tx, int ty, int facing, int move_type) {
    int i;
    if (npc_count < 0) npc_count = 0;
    if (npc_count >= MAX_NPCS) return -1;
    if (tx < 0 || ty < 0) return -1;

    i = npc_count++;
    npc_sprite[i]      = sprite_id;
    npc_x[i]           = (uint8_t)tx;
    npc_y[i]           = (uint8_t)ty;
    npc_facing[i]      = (uint8_t)(facing & 3);
    npc_move_type[i]   = (uint8_t)(move_type ? 1 : 0);
    npc_move_timer[i]  = 0;
    npc_walk_frames[i] = 0;
    npc_walk_total[i]  = 0;
    npc_step_px[i]     = 0;
    npc_px_off[i]      = 0;
    npc_py_off[i]      = 0;
    npc_hidden[i]      = 0;
    npc_debug_spawned[i] = 1;
    npc_src_idx[i]     = -1;
    reload_npc_tiles(i);
    apply_npc_oam_facing(i);
    Trace_Emit(TRACE_NPC, "\"ev\":\"spawn\",\"i\":%d,\"spr\":%d,\"x\":%d,\"y\":%d,"
               "\"cause\":\"debug_spawn\"", i, sprite_id, tx, ty);
    return i;
}

void NPC_DebugDespawn(int i) {
    if (i < 0 || i >= npc_count) return;
    npc_hidden[i] = 1;
    NPC_HideSprite(i);
    npc_walk_frames[i] = 0;
    npc_px_off[i] = 0;
    npc_py_off[i] = 0;

    while (npc_count > 0 && npc_hidden[npc_count - 1] && npc_debug_spawned[npc_count - 1]) {
        npc_count--;
        Trace_Emit(TRACE_NPC, "\"ev\":\"compact\",\"i\":%d,\"spr\":%d",
                   npc_count, npc_sprite[npc_count]);
    }
    Trace_Emit(TRACE_NPC, "\"ev\":\"despawn\",\"i\":%d,\"count\":%d,"
               "\"cause\":\"debug_despawn\"", i, npc_count);
}

void NPC_DoScriptedStepTimed(int i, int dir, int walk_frames, int step_px) {
    if (i < 0 || i >= npc_count) return;
    if (npc_walk_frames[i] > 0) return;
    if (walk_frames <= 0) walk_frames = NPC_WALK_FRAMES_SCRIPTED;
    if (step_px <= 0) step_px = NPC_STEP_PX_SCRIPTED;

    static const int8_t ddx[4] = { 0,  0, -1,  1};
    static const int8_t ddy[4] = { 1, -1,  0,  0};
    int ndx = ddx[dir & 3];
    int ndy = ddy[dir & 3];

    if (npc_sprite[i] != SPRITE_BOULDER_ID) {
        npc_facing[i] = (uint8_t)(dir & 3);
        reload_npc_tiles(i);
        apply_npc_oam_facing(i);
    } else {

        npc_facing[i] = 0;
        reload_npc_tiles(i);
        apply_npc_oam_facing(i);
    }

    npc_x[i] = (uint8_t)((int)npc_x[i] + ndx);
    npc_y[i] = (uint8_t)((int)npc_y[i] + ndy);

    npc_px_off[i]      = -ndx * 2 * TILE_PX;
    npc_py_off[i]      = -ndy * 2 * TILE_PX;
    npc_step_px[i]     = step_px;
    npc_walk_total[i]  = walk_frames;
    npc_walk_frames[i] = walk_frames;
}

void NPC_DoScriptedStep(int i, int dir) {
    NPC_DoScriptedStepTimed(i, dir, NPC_WALK_FRAMES_SCRIPTED, NPC_STEP_PX_SCRIPTED);
}

static int npc_blocked_except(int skip_idx, int nx, int ny) {
    for (int i = 0; i < npc_count; i++) {
        if (i == skip_idx) continue;
        if (npc_hidden[i]) continue;
        if (nx == (int)npc_x[i] && ny == (int)npc_y[i]) return 1;
    }
    return 0;
}

int NPC_IsBlocked(int nx, int ny) {
    return npc_blocked_except(-1, nx, ny);
}

int NPC_FindAtTile(int tx, int ty) {
    for (int i = 0; i < npc_count; i++) {
        if (npc_hidden[i]) continue;
        if ((int)npc_x[i] == tx && (int)npc_y[i] == ty) return i;
    }
    return -1;
}

int NPC_FindAtTileIncludingHidden(int tx, int ty) {
    for (int i = 0; i < npc_count; i++) {
        if ((int)npc_x[i] == tx && (int)npc_y[i] == ty) return i;
    }
    return -1;
}

int NPC_GetSpriteId(int i) {
    if (i < 0 || i >= npc_count) return -1;
    return (int)npc_sprite[i];
}

void NPC_Update(void) {

    static const int8_t ddx[4] = { 0,  0, -1,  1};
    static const int8_t ddy[4] = { 1, -1,  0,  0};

    for (int i = 0; i < npc_count; i++) {

        if (npc_walk_frames[i] > 0) {
            npc_walk_frames[i]--;
            int spx = npc_step_px[i];
            if (npc_px_off[i] > 0) npc_px_off[i] -= spx;
            if (npc_px_off[i] < 0) npc_px_off[i] += spx;
            if (npc_py_off[i] > 0) npc_py_off[i] -= spx;
            if (npc_py_off[i] < 0) npc_py_off[i] += spx;

            int total    = npc_walk_total[i];
            int elapsed  = total - npc_walk_frames[i];
            int divisor  = total / 4;
            int counter  = (elapsed - 1) / divisor;
            int walking  = (npc_walk_frames[i] > 0) ? (counter & 1) : 0;
            if (npc_sprite[i] == SPRITE_BOULDER_ID) walking = 0;
            reload_npc_tiles_ex(i, walking);

            apply_npc_oam_facing(i);
            continue;
        }
        npc_px_off[i] = 0;
        npc_py_off[i] = 0;

        if (npc_move_type[i] == 0) continue;

        if (npc_move_type[i] == 4) {
            if (npc_move_timer[i] > 0) { npc_move_timer[i]--; continue; }
            uint8_t rr = BattleRandom();

            int sid = npc_sprite[i] < NUM_SPRITES ? npc_sprite[i] : 0;

            int tilecount = gSpriteTileCount[sid];
            if (PKS_SPRITE_IS_CRYSTAL(npc_sprite[i])) {
                int ci = npc_sprite[i] - PKS_CRYSTAL_SPRITE_BASE;
                tilecount = (ci >= 0 && ci < CRYSTAL_NUM_SPRITES)
                          ? gCrystalSpriteTileCount[ci] : 0;
            }
            if (tilecount >= 12) {
                npc_facing[i] = (uint8_t)(rr >> 6);
                reload_npc_tiles(i);
                apply_npc_oam_facing(i);
            }
            npc_move_timer[i] = npc_next_move_delay();
            continue;
        }

        if (npc_move_timer[i] > 0) {
            npc_move_timer[i]--;
            continue;
        }

        static const uint8_t dir_free[4]       = {0, 1, 2, 3};
        static const uint8_t dir_left_right[4] = {2, 3, 2, 3};
        static const uint8_t dir_up_down[4]    = {0, 1, 1, 0};
        uint8_t r   = BattleRandom();
        int     q   = r >> 6;
        int     dir = npc_move_type[i] == 2 ? dir_left_right[q]
                    : npc_move_type[i] == 3 ? dir_up_down[q]
                                            : dir_free[q];
        int     ndx = ddx[dir];
        int     ndy = ddy[dir];
        int     nx  = (int)npc_x[i] + ndx;
        int     ny  = (int)npc_y[i] + ndy;

        int map_w = (int)wCurMapWidth  * 2;
        int map_h = (int)wCurMapHeight * 2;
        if (nx < 0 || ny < 0 || nx >= map_w || ny >= map_h) {
            npc_move_timer[i] = npc_next_move_delay();
            continue;
        }

        if (Map_IsTilePassableAt(nx, ny) &&
            !npc_blocked_except(i, nx, ny)           &&
            (nx != (int)wXCoord || ny != (int)wYCoord)) {

            npc_facing[i] = (uint8_t)dir;
            reload_npc_tiles(i);
            apply_npc_oam_facing(i);

            npc_x[i]           = (uint8_t)nx;
            npc_y[i]           = (uint8_t)ny;
            npc_px_off[i]      = -ndx * 2 * TILE_PX;
            npc_py_off[i]      = -ndy * 2 * TILE_PX;
            npc_step_px[i]     = NPC_STEP_PX_RANDOM;
            npc_walk_total[i]  = NPC_WALK_FRAMES_RANDOM;
            npc_walk_frames[i] = NPC_WALK_FRAMES_RANDOM;
        }

        npc_move_timer[i] = npc_next_move_delay();
    }
}

void NPC_BuildView(int scroll_px_x, int scroll_px_y) {
    int ox = gCamX;
    int oy = gCamY;

    for (int i = 0; i < npc_count; i++) {
        if (npc_hidden[i]) {

            int oam = NPC_OAM_BASE + i * 4;
            wShadowOAM[oam + 0].y = 0;
            wShadowOAM[oam + 1].y = 0;
            wShadowOAM[oam + 2].y = 0;
            wShadowOAM[oam + 3].y = 0;
            continue;
        }

        int nx = (int)npc_x[i] * 2     - ox;
        int ny = (int)npc_y[i] * 2 + 1 - oy;

        int px = nx       * TILE_PX + scroll_px_x + npc_px_off[i];
        int py = (ny - 1) * TILE_PX + scroll_px_y + npc_py_off[i] - 4;

        int oam = NPC_OAM_BASE + i * 4;

        if (px + 16 <= 0 || px >= Display_FrameWidth() ||
            py + 16 <= 0 || py >= SCREEN_HEIGHT_PX) {
            wShadowOAM[oam + 0].y = 0;
            wShadowOAM[oam + 1].y = 0;
            wShadowOAM[oam + 2].y = 0;
            wShadowOAM[oam + 3].y = 0;
            continue;
        }

        if (npc_facing[i] == 3) {

            wShadowOAM[oam + 0].y = (uint8_t)(py     + OAM_Y_OFS);
            wShadowOAM[oam + 0].x = (px + 8 + OAM_X_OFS);
            wShadowOAM[oam + 1].y = (uint8_t)(py     + OAM_Y_OFS);
            wShadowOAM[oam + 1].x = (px     + OAM_X_OFS);
            wShadowOAM[oam + 2].y = (uint8_t)(py + 8 + OAM_Y_OFS);
            wShadowOAM[oam + 2].x = (px + 8 + OAM_X_OFS);
            wShadowOAM[oam + 3].y = (uint8_t)(py + 8 + OAM_Y_OFS);
            wShadowOAM[oam + 3].x = (px     + OAM_X_OFS);
        } else {

            wShadowOAM[oam + 0].y = (uint8_t)(py     + OAM_Y_OFS);
            wShadowOAM[oam + 0].x = (px     + OAM_X_OFS);
            wShadowOAM[oam + 1].y = (uint8_t)(py     + OAM_Y_OFS);
            wShadowOAM[oam + 1].x = (px + 8 + OAM_X_OFS);
            wShadowOAM[oam + 2].y = (uint8_t)(py + 8 + OAM_Y_OFS);
            wShadowOAM[oam + 2].x = (px     + OAM_X_OFS);
            wShadowOAM[oam + 3].y = (uint8_t)(py + 8 + OAM_Y_OFS);
            wShadowOAM[oam + 3].x = (px + 8 + OAM_X_OFS);
        }

        int on_grass;
        if (AmberScript_IsEnabled() &&
            wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST) {
            uint8_t is_grass = 0;
            on_grass = AmberScript_GetGrassOverrideAt((int)npc_x[i] * 2, (int)npc_y[i] * 2 + 1, &is_grass) && is_grass;
        } else {
            on_grass = (wGrassTile != 0xFF &&
                        Map_GetGameTile((int)npc_x[i], (int)npc_y[i]) == wGrassTile);
        }
        if (on_grass) {
            wShadowOAM[oam + 2].flags |=  OAM_FLAG_PRIORITY;
            wShadowOAM[oam + 3].flags |=  OAM_FLAG_PRIORITY;
        } else {
            wShadowOAM[oam + 2].flags &= ~OAM_FLAG_PRIORITY;
            wShadowOAM[oam + 3].flags &= ~OAM_FLAG_PRIORITY;
        }
    }

    for (int i = npc_count; i < MAX_NPCS; i++) {
        int oam = NPC_OAM_BASE + i * 4;
        wShadowOAM[oam + 0].y = 0;
        wShadowOAM[oam + 1].y = 0;
        wShadowOAM[oam + 2].y = 0;
        wShadowOAM[oam + 3].y = 0;
    }
}

void NPC_StateCapture(npc_state_t *out) {
    if (!out) return;
    out->npc_count = npc_count;
    out->npc_last_interacted = npc_last_interacted;
    memcpy(out->npc_sprite, npc_sprite, sizeof(npc_sprite));
    memcpy(out->npc_x, npc_x, sizeof(npc_x));
    memcpy(out->npc_y, npc_y, sizeof(npc_y));
    memcpy(out->npc_facing, npc_facing, sizeof(npc_facing));
    memcpy(out->npc_move_type, npc_move_type, sizeof(npc_move_type));
    memcpy(out->npc_move_timer, npc_move_timer, sizeof(npc_move_timer));
    memcpy(out->npc_walk_frames, npc_walk_frames, sizeof(npc_walk_frames));
    memcpy(out->npc_walk_total, npc_walk_total, sizeof(npc_walk_total));
    memcpy(out->npc_step_px, npc_step_px, sizeof(npc_step_px));
    memcpy(out->npc_px_off, npc_px_off, sizeof(npc_px_off));
    memcpy(out->npc_py_off, npc_py_off, sizeof(npc_py_off));
    memcpy(out->npc_hidden, npc_hidden, sizeof(npc_hidden));
}

void NPC_StateRestore(const npc_state_t *in) {
    if (!in) return;
    npc_count = in->npc_count;
    if (npc_count < 0) npc_count = 0;
    if (npc_count > MAX_NPCS) npc_count = MAX_NPCS;
    npc_last_interacted = in->npc_last_interacted;
    memcpy(npc_sprite, in->npc_sprite, sizeof(npc_sprite));
    memcpy(npc_x, in->npc_x, sizeof(npc_x));
    memcpy(npc_y, in->npc_y, sizeof(npc_y));
    memcpy(npc_facing, in->npc_facing, sizeof(npc_facing));
    memcpy(npc_move_type, in->npc_move_type, sizeof(npc_move_type));
    memcpy(npc_move_timer, in->npc_move_timer, sizeof(npc_move_timer));
    memcpy(npc_walk_frames, in->npc_walk_frames, sizeof(npc_walk_frames));
    memcpy(npc_walk_total, in->npc_walk_total, sizeof(npc_walk_total));
    memcpy(npc_step_px, in->npc_step_px, sizeof(npc_step_px));
    memcpy(npc_px_off, in->npc_px_off, sizeof(npc_px_off));
    memcpy(npc_py_off, in->npc_py_off, sizeof(npc_py_off));
    memcpy(npc_hidden, in->npc_hidden, sizeof(npc_hidden));
    for (int i = 0; i < npc_count; i++) npc_apply_visual_state(i);
}
