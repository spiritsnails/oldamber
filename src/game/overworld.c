
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "overworld.h"
#include "debug_cli.h"
#include "amberscript_core.h"
#include "amberscript_tilemod.h"
#include "ss_anne_depart.h"
#include "amberscript_mapbank.h"
#include "warp.h"
#include "town_map.h"
#include "field_moves.h"
#include "../data/map_data.h"
#include "../data/tileset_data.h"
#include "anim.h"
#include "gbc_color.h"
#include "../data/font_data.h"
#include "music.h"
#include "johto_music.h"
#include "map_music.h"
#include "../data/connection_data.h"
#include "../data/event_data.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../game/constants.h"

#if defined(__GNUC__)
__attribute__((weak))
#endif
void DebugCLI_ClearTileOverrides(void) {
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int DebugCLI_GetTileOverrideAt(int tx, int ty, uint8_t *tile_id) {
    (void)tx;
    (void)ty;
    (void)tile_id;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_GetTileOverrideAt(int tx, int ty, uint8_t *tile_id) {
    (void)tx;
    (void)ty;
    (void)tile_id;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
void AmberScript_ClearTileOverrides(uint8_t map_id) {
    (void)map_id;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_GetPassableOverrideAt(int tx, int ty, uint8_t *passable) {
    (void)tx;
    (void)ty;
    (void)passable;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_GetPassableOverrideAtForMap(uint8_t map_id, int tx, int ty, uint8_t *passable) {
    (void)map_id;
    (void)tx;
    (void)ty;
    (void)passable;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_GetSurfableOverrideAtForMap(uint8_t map_id, int tx, int ty, uint8_t *surfable) {
    (void)map_id;
    (void)tx;
    (void)ty;
    (void)surfable;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_GetTileOverrideAtForMap(uint8_t map_id, int tx, int ty, uint8_t *tile_id) {
    (void)map_id;
    (void)tx;
    (void)ty;
    (void)tile_id;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_MapBank_GetDimsForRealId(int real_id, int *width_blocks, int *height_blocks) {
    (void)real_id;
    (void)width_blocks;
    (void)height_blocks;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_MapBank_GetBorderTileForRealId(int real_id, int tx, int ty,
                                               uint8_t *tile_id, uint8_t *passable_out) {
    (void)real_id;
    (void)tx;
    (void)ty;
    (void)tile_id;
    (void)passable_out;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
void AmberScript_MapBank_TouchRealId(uint8_t real_id) {
    (void)real_id;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
const char *AmberScript_MapBank_NameForRealId(int real_id) {
    (void)real_id;
    return NULL;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_MapBank_GetMusicForRealId(int real_id, char *out_track, size_t out_cap) {
    (void)real_id;
    (void)out_track;
    (void)out_cap;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
void AmberScript_TileMod_Tick(void) {
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
void AmberScript_TileMod_ReapplyCurrentMapNow(void) {
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
void AmberScript_TileMod_ForceReapplyGfx(void) {
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
void AmberScript_TileMod_InvalidateSubtileCache(void) {
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
void AmberScript_TileMod_BumpCacheGeneration(void) {
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
void AmberScript_TileMod_PrewarmNeighbors(void) {
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_MapBank_EnsureResidentForRealId(uint8_t real_id) {
    (void)real_id;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_GetConnectionOverride(uint8_t cur_real_id, int direction,
                                      uint8_t *dest_real_id, int16_t *player_coord, int16_t *adjust) {
    (void)cur_real_id;
    (void)direction;
    (void)dest_real_id;
    (void)player_coord;
    (void)adjust;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_GetConnectionOverridePassive(uint8_t cur_real_id, int direction,
                                             uint8_t *dest_real_id, int16_t *player_coord, int16_t *adjust) {
    (void)cur_real_id;
    (void)direction;
    (void)dest_real_id;
    (void)player_coord;
    (void)adjust;
    return 0;
}

static const tileset_info_t *cur_tileset    = NULL;
static const map_info_t     *cur_map        = NULL;
static const map_connections_t *cur_conns   = NULL;
static int cur_map_w = 0;
static int cur_map_h = 0;

int gCamX = 0;
int gCamY = 0;

uint8_t gScrollTileMap[SCROLL_MAP_W * SCROLL_MAP_H];

int Map_ViewTilesW(void) { return Display_FrameWidth() / 8; }

int Map_CamHalfX(void) { return Map_ViewTilesW() / 2 - 2; }

int Map_UiColOfsRight(void) {
    if (Display_AuthoredFrame()) return 0;
    return Map_ViewTilesW() - SCREEN_WIDTH;
}

int Map_UiColOfs(void) {

    if (Display_AuthoredFrame()) return 0;
    return (Map_ViewTilesW() - SCREEN_WIDTH) / 2;
}
int Map_ScrollCols(void) { return Map_ViewTilesW() + 4; }

static int is_connection_tileset(uint8_t tileset_id) {
    return tileset_id == TILESET_OVERWORLD || tileset_id == TILESET_PLATEAU;
}

#define MAX_BLOCK_OVERRIDES 16
typedef struct { int8_t bx; int8_t by; uint8_t id; } block_override_t;
static block_override_t gBlockOverrides[MAX_BLOCK_OVERRIDES];
static int gNumBlockOverrides = 0;

void Map_SetBlock(int bx, int by, uint8_t block_id) {
    for (int i = 0; i < gNumBlockOverrides; i++) {
        if (gBlockOverrides[i].bx == (int8_t)bx && gBlockOverrides[i].by == (int8_t)by) {
            gBlockOverrides[i].id = block_id;
            return;
        }
    }
    if (gNumBlockOverrides < MAX_BLOCK_OVERRIDES)
        gBlockOverrides[gNumBlockOverrides++] = (block_override_t){(int8_t)bx, (int8_t)by, block_id};
}

static uint8_t get_block_id(int bx, int by) {
    for (int i = 0; i < gNumBlockOverrides; i++) {
        if (gBlockOverrides[i].bx == (int8_t)bx && gBlockOverrides[i].by == (int8_t)by)
            return gBlockOverrides[i].id;
    }
    if (!cur_map || !cur_map->blocks) return 0;
    if (bx < 0 || by < 0 || bx >= cur_map->width || by >= cur_map->height) {
        if (wCurMap < PKS_VIRTUAL_MAP_FIRST) return gMapEvents[wCurMap].border_block;
        return 0;
    }
    return cur_map->blocks[by * cur_map->width + bx];
}

static int clamp_cam(int player_tile, int half, int map_tiles, int screen_tiles) {
    (void)map_tiles; (void)screen_tiles;

    return player_tile - half;
}

void Map_UpdateCamera(void) {
    if (!cur_map) return;
    int map_w = cur_map->width  * 4;
    int map_h = cur_map->height * 4;

    gCamX = clamp_cam((int)wXCoord * 2, Map_CamHalfX(), map_w, Map_ViewTilesW());
    gCamY = clamp_cam((int)wYCoord * 2 + 1, 9, map_h, SCREEN_HEIGHT);
    Display_SetBlockIDCam(gCamX, gCamY);
}

int Map_GetBlockIdRaw(int bx, int by) {
    return (int)get_block_id(bx, by);
}

int Map_IsDarkMap(int real_id) {
    return (real_id == 0x52  ||
            real_id == 0xE8  ||
            AmberScript_MapBank_IsDarkForRealId(real_id)) ? 1 : 0;
}

void Map_ApplyDarknessForWarp(int prev_was_dark, uint8_t prev_offset, int dest_real_id) {
    if (!Map_IsDarkMap(dest_real_id)) gMapPalOffset = 0;
    else if (!prev_was_dark)          gMapPalOffset = 6;
    else                              gMapPalOffset = prev_offset;

}

int gOverworldCloseWhiteout = 0;
void Overworld_ArmCloseWhiteout(int frames) {
    gOverworldCloseWhiteout = frames;
}

void Map_ReloadGfx(void) {
    if (cur_map && cur_tileset && cur_tileset->gfx)
        Display_LoadTileset(cur_tileset->gfx, cur_tileset->gfx_tiles);

    if (AmberScript_IsEnabled())
        AmberScript_TileMod_ForceReapplyGfx();
}

void Map_RefreshVirtualAnim(void) {
    int anim_ts = cur_tileset ? (int)(cur_tileset - gTilesets) : 0;
    if (AmberScript_IsEnabled()) {
        int real_ts = AmberScript_MapBank_GetGbcTilesetForRealId(wCurMap);
        if (real_ts >= 0 && real_ts < NUM_TILESETS) anim_ts = real_ts;
    }
    if (anim_ts >= 0 && anim_ts < NUM_TILESETS)
        Anim_SetTileset(gTilesets[anim_ts].anim_type);
}

void Map_RefreshVirtualDims(void) {
    if (!AmberScript_IsEnabled()) return;
    {
        int vw, vh;
        if (AmberScript_MapBank_GetDimsForRealId(wCurMap, &vw, &vh)) {
            cur_map_w     = vw * 4;
            cur_map_h     = vh * 4;
            wCurMapWidth  = (uint8_t)vw;
            wCurMapHeight = (uint8_t)vh;
        }
    }
}

void AmberScript_PlayMapMusic(uint8_t map_id) {
    MapMusic_PlayForMap(map_id);
}

void Map_Load(uint8_t map_id) {
    if (map_id >= NUM_MAPS) return;
    gNumBlockOverrides = 0;
    DebugCLI_ClearTileOverrides();
    AmberScript_ClearTileOverrides(map_id);
    Warp_Reset();
    FieldMove_OnMapLoad();
    const map_info_t *m = &gMapTable[map_id];

    cur_map     = m;
    cur_tileset = &gTilesets[m->tileset_id];
    cur_map_w   = m->width  * 4;
    cur_map_h   = m->height * 4;
    cur_conns   = (map_id < NUM_MAP_CONNECTIONS) ? &gMapConnections[map_id] : NULL;

    wCurMap        = map_id;
    wCurMapWidth   = m->width;
    wCurMapHeight  = m->height;

    Map_RefreshVirtualDims();
    wCurMapTileset = m->tileset_id;
    wGrassTile     = cur_tileset->grass_tile;

    wTilesetTalkingOverTiles[0] = cur_tileset->counter[0];
    wTilesetTalkingOverTiles[1] = cur_tileset->counter[1];
    wTilesetTalkingOverTiles[2] = cur_tileset->counter[2];

    if (m->blocks) {
        Display_LoadTileset(cur_tileset->gfx, cur_tileset->gfx_tiles);

        if (AmberScript_IsEnabled())
            AmberScript_TileMod_InvalidateSubtileCache();
    }

    {
        int tm_real = Map_CurrentRealId();
        if (tm_real >= 0) TownMap_MarkVisited((uint8_t)tm_real);
    }
    Display_SetBlockIDQueryFn(Map_GetBlockIdRaw);

    Map_RefreshVirtualAnim();

    AmberScript_MapBank_EnsureResidentForRealId(map_id);
    AmberScript_TileMod_PrewarmNeighbors();

    AmberScript_PlayMapMusic(map_id);

    GbcColor_ApplyForMap(map_id);

    {
        int group = 0;
        int is_gen2 = Font_Gen2Enabled()
                   && AmberScript_MapBank_GetCrystalEnvForRealId(map_id, &group) >= 0;
        Font_SetStyle(is_gen2 ? FONT_STYLE_GEN2 : FONT_STYLE_GEN1);
    }

    GbcColor_MarkDirty();

    AmberScript_TileMod_ReapplyCurrentMapNow();
}

static int connected_tile(const map_conn_t *conn, int ctx, int cty, uint8_t *out_tile) {
    uint8_t dest = conn->dest_map;

    int dest_is_vmap = AmberScript_IsEnabled() &&
                       dest >= PKS_VIRTUAL_MAP_FIRST && dest <= PKS_VIRTUAL_MAP_LAST;
    if (!dest_is_vmap && (dest == 0xFF || dest >= NUM_MAPS)) return 0;
    const map_info_t     *cm = &gMapTable[dest];
    const tileset_info_t *ct = &gTilesets[cm->tileset_id];
    int cw = cm->width * 4,  ch = cm->height * 4;

    if (AmberScript_IsEnabled()) {
        int vw, vh;
        if (AmberScript_MapBank_GetDimsForRealId(dest, &vw, &vh)) {
            cw = vw * 4;
            ch = vh * 4;
        }
    }
    if (ctx < 0 || cty < 0 || ctx >= cw || cty >= ch)
        return 0;

    if (AmberScript_IsEnabled()) {
        uint8_t override_tile = 0;
        if (AmberScript_GetTileOverrideAtForMap(dest, ctx, cty, &override_tile)) {
            *out_tile = override_tile;
            return 1;
        }
    }
    if (!cm->blocks) { *out_tile = 0; return 1; }

    int bx = ctx >> 2, by = cty >> 2;
    int lx = ctx & 3,  ly = cty & 3;
    uint8_t bid = cm->blocks[by * cm->width + bx];
    *out_tile = ct->blocks[bid * 16 + ly * 4 + lx];
    return 1;
}

static int pks_resolve_conn(int direction, map_conn_t *out) {
    if (AmberScript_IsEnabled()) {
        uint8_t dest_real_id;
        int16_t player_coord, adjust;

        if (AmberScript_GetConnectionOverridePassive(wCurMap, direction, &dest_real_id, &player_coord, &adjust)) {
            out->dest_map = dest_real_id;
            out->player_coord = player_coord;
            out->adjust = adjust;
            return 1;
        }
    }
    if (cur_conns) {
        const map_conn_t *real = (direction == 0) ? &cur_conns->north
                                : (direction == 1) ? &cur_conns->south
                                : (direction == 2) ? &cur_conns->west
                                                    : &cur_conns->east;
        if (real->dest_map != 0xFF) { *out = *real; return 1; }
    }
    return 0;
}

#define CONN_PAD 12

static int conn_pad(void) {
    if (Map_ViewTilesW() <= SCREEN_WIDTH) return CONN_PAD;
    return Map_ViewTilesW() / 2 + 4;
}

#define CONN_CAND_MAX 2
static int resolve_connection_cells(int tx, int ty, map_conn_t *cs,
                                    int *conn_tx, int *conn_ty);

uint8_t Map_GetTile(int tx, int ty) {
    if (!cur_map || !cur_tileset) return 0;

    if (is_connection_tileset(wCurMapTileset)) {

        map_conn_t cs[CONN_CAND_MAX];
        int ctxs[CONN_CAND_MAX], ctys[CONN_CAND_MAX];
        int n = resolve_connection_cells(tx, ty, cs, ctxs, ctys), i;
        uint8_t conn_tile;
        for (i = 0; i < n; i++)
            if (connected_tile(&cs[i], ctxs[i], ctys[i], &conn_tile))
                return conn_tile;

    }

    if (tx < 0 || ty < 0 || tx >= cur_map_w || ty >= cur_map_h) {

        if (AmberScript_IsEnabled()) {
            uint8_t border_tile;
            if (AmberScript_MapBank_GetBorderTileForRealId(wCurMap, tx, ty, &border_tile, NULL))
                return border_tile;

            if (wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST)
                return 0;
        }

        return cur_tileset->blocks[(ty & 3) * 4 + (tx & 3)];
    }

    {
        uint8_t override_tile = 0;
        int got_override = AmberScript_IsEnabled()
            ? AmberScript_GetTileOverrideAt(tx, ty, &override_tile)
            : DebugCLI_GetTileOverrideAt(tx, ty, &override_tile);
        if (got_override)
            return override_tile;
    }

    int bx = tx >> 2,  by = ty >> 2;
    int lx = tx & 3,   ly = ty & 3;
    uint8_t bid = get_block_id(bx, by);
    return cur_tileset->blocks[bid * 16 + ly * 4 + lx];
}

uint8_t Map_GetGameTile(int gx, int gy) {
    return Map_GetTile(gx * 2, gy * 2 + 1);
}

uint8_t Map_GetBlockAt(int gx, int gy) {
    int tx = gx * 2;
    int ty = gy * 2 + 1;
    int bx = tx >> 2,  by = ty >> 2;
    return get_block_id(bx, by);
}

void Map_SetBlockAt(int gx, int gy, uint8_t block_id) {
    int bx = (gx * 2) >> 2;
    int by = (gy * 2 + 1) >> 2;
    Map_SetBlock(bx, by, block_id);
}

void Map_BuildView(void) {
    if (!cur_map || !cur_tileset) return;
    if (AmberScript_IsEnabled()) AmberScript_TileMod_BumpCacheGeneration();

    int ox = (int)wXCoord * 2 - 8;
    int oy = (int)wYCoord * 2 + 1 - 9;

    for (int sy = 0; sy < SCREEN_HEIGHT; sy++)
        for (int sx = 0; sx < SCREEN_WIDTH; sx++)
            wTileMap[sy * SCREEN_WIDTH + sx] = Map_GetTile(ox + sx, oy + sy);
}

static int gScrollViewReady    = 0;
static int gConnTransRemaining = 0;

void Map_ResetScrollState(void) {

    gScrollViewReady    = 0;
    gConnTransRemaining = 0;
}

static const map_info_t       *conn_save_map      = NULL;
static const tileset_info_t   *conn_save_tileset  = NULL;
static const map_connections_t *conn_save_conns   = NULL;
static int   conn_save_map_w = 0, conn_save_map_h = 0;
static uint8_t conn_save_tileset_id = 0;
static int   conn_cam_x = 0, conn_cam_y = 0;

#define CONN_WALK_FRAMES 8

static int s_suppress_scroll_rebuild = 0;

void Map_SuppressScrollRebuild(int suppress) {
    s_suppress_scroll_rebuild = suppress;
}

static int s_boot_screen_hold = 0;

void Map_HoldForBootScreen(int hold) { s_boot_screen_hold = hold ? 1 : 0; }
int  Map_IsHeldForBootScreen(void)   { return s_boot_screen_hold; }

void Map_BuildScrollView(void) {
    if (!cur_map || !cur_tileset || s_suppress_scroll_rebuild) return;
    if (s_boot_screen_hold) return;
    if (AmberScript_IsEnabled()) {
        AmberScript_TileMod_BumpCacheGeneration();

        AmberScript_MapBank_TouchRealId((uint8_t)wCurMap);
    }

    if (gScrollViewReady) {
        gScrollViewReady = 0;
        Map_UpdateCamera();
        return;
    }

    if (gConnTransRemaining > 0) {
        gConnTransRemaining--;

        const map_info_t       *sv_map      = cur_map;
        const tileset_info_t   *sv_tileset  = cur_tileset;
        const map_connections_t *sv_conns   = cur_conns;
        int    sv_map_w = cur_map_w, sv_map_h = cur_map_h;
        uint8_t sv_ts   = wCurMapTileset;

        cur_map         = conn_save_map;
        cur_tileset     = conn_save_tileset;
        cur_conns       = conn_save_conns;
        cur_map_w       = conn_save_map_w;
        cur_map_h       = conn_save_map_h;
        wCurMapTileset  = conn_save_tileset_id;

        int ox = conn_cam_x - 2;
        int oy = conn_cam_y - 2;
        for (int sy = 0; sy < SCROLL_MAP_H; sy++)
            for (int sx = 0, cols = Map_ScrollCols(); sx < cols; sx++)
                gScrollTileMap[sy * SCROLL_MAP_W + sx] = Map_GetTile(ox + sx, oy + sy);

        cur_map        = sv_map;
        cur_tileset    = sv_tileset;
        cur_conns      = sv_conns;
        cur_map_w      = sv_map_w;
        cur_map_h      = sv_map_h;
        wCurMapTileset = sv_ts;

        Map_UpdateCamera();
        return;
    }

    Map_UpdateCamera();

    int ox = gCamX - 2;
    int oy = gCamY - 2;

    for (int sy = 0; sy < SCROLL_MAP_H; sy++)
        for (int sx = 0, cols = Map_ScrollCols(); sx < cols; sx++)
            gScrollTileMap[sy * SCROLL_MAP_W + sx] = Map_GetTile(ox + sx, oy + sy);

    SSAnneDepart_PostBuildScrollView();
}

void Map_PreBuildScrollStep(int dx, int dy) {
    if (!cur_map || !cur_tileset) return;

    if (!cur_conns || wCurMapTileset != 0) return;
    if (dy < 0 && cur_conns->north.dest_map == 0xFF) return;
    if (dy > 0 && cur_conns->south.dest_map == 0xFF) return;
    if (dx < 0 && cur_conns->west.dest_map  == 0xFF) return;
    if (dx > 0 && cur_conns->east.dest_map  == 0xFF) return;

    conn_save_map        = cur_map;
    conn_save_tileset    = cur_tileset;
    conn_save_conns      = cur_conns;
    conn_save_map_w      = cur_map_w;
    conn_save_map_h      = cur_map_h;
    conn_save_tileset_id = wCurMapTileset;

    int new_gx = (int)wXCoord + dx;
    int new_gy = (int)wYCoord + dy;
    conn_cam_x = clamp_cam(new_gx * 2, Map_CamHalfX(), cur_map_w, Map_ViewTilesW());
    conn_cam_y = clamp_cam(new_gy * 2 + 1, 9, cur_map_h, SCREEN_HEIGHT);

    int ox = conn_cam_x - 2;
    int oy = conn_cam_y - 2;
    for (int sy = 0; sy < SCROLL_MAP_H; sy++)
        for (int sx = 0, cols = Map_ScrollCols(); sx < cols; sx++)
            gScrollTileMap[sy * SCROLL_MAP_W + sx] = Map_GetTile(ox + sx, oy + sy);

    gScrollViewReady    = 1;
    gConnTransRemaining = 0;
}

int Connection_Check(int dx, int dy) {
    const map_conn_t *conn = NULL;
    int is_north_south = 0;
    map_conn_t override_conn;

    if (AmberScript_IsEnabled()) {
        int direction = (dy < 0) ? 0 : (dy > 0) ? 1 : (dx < 0) ? 2 : (dx > 0) ? 3 : -1;
        uint8_t dest_real_id;
        int16_t player_coord, adjust;
        if (direction >= 0 &&
            AmberScript_GetConnectionOverride(wCurMap, direction, &dest_real_id, &player_coord, &adjust)) {
            override_conn.dest_map = dest_real_id;
            override_conn.player_coord = player_coord;
            override_conn.adjust = adjust;
            conn = &override_conn;
            is_north_south = (direction == 0 || direction == 1);
        }
    }

    if (!conn) return 0;

    if (!conn) return 0;

    if (is_north_south) {
        wXCoord = (int16_t)((int)wXCoord + conn->adjust / 2);
        wYCoord = (int16_t)(conn->player_coord / 2);
    } else {
        wYCoord = (int16_t)((int)wYCoord + conn->adjust / 2);
        wXCoord = (int16_t)(conn->player_coord / 2);
    }

    MapMusic_SetNextTransition(MAPMUSIC_FADE);
    Map_Load(conn->dest_map);
    return 1;
}

int Tile_IsPassable(uint8_t tile_id) {
    if (!cur_tileset) return 1;
    const uint8_t *p = cur_tileset->coll_tiles;
    while (*p != 0xFF) {
        if (*p++ == tile_id) return 1;
    }
    return 0;
}

static int resolve_connection_cells(int tx, int ty, map_conn_t *cs,
                                    int *conn_tx, int *conn_ty) {
    int n = 0;
    if (!is_connection_tileset(wCurMapTileset)) return 0;

    const int pad = conn_pad();
    if (tx < 0 && tx >= -pad &&
        ty >= -pad && ty < cur_map_h + pad && pks_resolve_conn(2, &cs[n])) {
        conn_tx[n] = cs[n].player_coord + tx + 2;
        conn_ty[n] = ty + cs[n].adjust; n++;
    } else if (tx >= cur_map_w && tx < cur_map_w + pad &&
               ty >= -pad && ty < cur_map_h + pad && pks_resolve_conn(3, &cs[n])) {
        conn_tx[n] = cs[n].player_coord + (tx - cur_map_w);
        conn_ty[n] = ty + cs[n].adjust; n++;
    }
    if (ty < 0 && ty >= -pad &&
        tx >= -pad && tx < cur_map_w + pad && pks_resolve_conn(0, &cs[n])) {
        conn_tx[n] = tx + cs[n].adjust;
        conn_ty[n] = cs[n].player_coord + ty + 1; n++;
    } else if (ty >= cur_map_h && ty < cur_map_h + pad &&
               tx >= -pad && tx < cur_map_w + pad && pks_resolve_conn(1, &cs[n])) {
        conn_tx[n] = tx + cs[n].adjust;
        conn_ty[n] = cs[n].player_coord - 1 + (ty - cur_map_h); n++;
    }
    return n;
}

int Map_IsTilePassableAt(int gx, int gy) {
    int tx = gx * 2, ty = gy * 2 + 1;

    {
        map_conn_t cs[CONN_CAND_MAX];
        int ctxs[CONN_CAND_MAX], ctys[CONN_CAND_MAX];
        int n = resolve_connection_cells(tx, ty, cs, ctxs, ctys), i;
        for (i = 0; i < n; i++) {
            uint8_t conn_tile;
            if (AmberScript_IsEnabled()) {
                uint8_t passable = 0;
                if (AmberScript_GetPassableOverrideAtForMap(cs[i].dest_map, ctxs[i], ctys[i], &passable))
                    return passable ? 1 : 0;
            }

            if (connected_tile(&cs[i], ctxs[i], ctys[i], &conn_tile))
                return Tile_IsPassable(conn_tile);
        }
    }

    if (AmberScript_IsEnabled()) {
        uint8_t passable = 0;
        if (AmberScript_GetPassableOverrideAt(tx, ty, &passable))
            return passable ? 1 : 0;
    }

    if ((tx < 0 || ty < 0 || tx >= cur_map_w || ty >= cur_map_h) && AmberScript_IsEnabled()) {
        uint8_t border_passable;
        if (AmberScript_MapBank_GetBorderTileForRealId(wCurMap, tx, ty, NULL, &border_passable))
            return border_passable ? 1 : 0;

        if (wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST)
            return 0;
    }
    return Tile_IsPassable(Map_GetGameTile(gx, gy));
}

static void mcr_norm(char *dst, size_t dst_sz, const char *src) {
    size_t n = 0, i;
    if (!dst || dst_sz == 0) return;
    dst[0] = '\0';
    if (!src) return;
    for (i = 0; src[i] && n + 1 < dst_sz; i++) {
        char c = (char)tolower((unsigned char)src[i]);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) dst[n++] = c;
    }
    dst[n] = '\0';
}

int Map_RealIdForName(const char *name) {
    char want[64], have[64];
    int i;
    if (!name || !name[0]) return -1;
    mcr_norm(want, sizeof(want), name);
    if (!want[0]) return -1;
    for (i = 0; i < PKS_REAL_MAP_COUNT && i < NUM_MAPS; i++) {
        if (!gMapTable[i].name) continue;

        if (gMapIsFillerId[i]) continue;
        mcr_norm(have, sizeof(have), gMapTable[i].name);
        if (strcmp(want, have) == 0) return i;
    }
    return -1;
}

int Map_CurrentRealId(void) {
    const char *nm;
    if (wCurMap < PKS_VIRTUAL_MAP_FIRST) return (int)wCurMap;
    nm = AmberScript_MapBank_NameForRealId((int)wCurMap);
    if (!nm) return -1;
    return Map_RealIdForName(nm);
}

int Map_GetSurfableOverrideAt(int gx, int gy, uint8_t *surfable) {
    int tx = gx * 2, ty = gy * 2 + 1;
    if (!surfable || !AmberScript_IsEnabled()) return 0;
    {
        map_conn_t cs[CONN_CAND_MAX];
        int ctxs[CONN_CAND_MAX], ctys[CONN_CAND_MAX];
        int n = resolve_connection_cells(tx, ty, cs, ctxs, ctys), i;
        for (i = 0; i < n; i++)
            if (AmberScript_GetSurfableOverrideAtForMap(cs[i].dest_map, ctxs[i], ctys[i], surfable))
                return 1;
    }
    return AmberScript_GetSurfableOverrideAt(tx, ty, surfable);
}

int Tile_IsPairBlocked(uint8_t a, uint8_t b) {
    static const struct { uint8_t ts, t1, t2; } kPairs[] = {

        { 17, 0x20, 0x05 },
        { 17, 0x41, 0x05 },
        { 17, 0x2A, 0x05 },
        { 17, 0x05, 0x21 },

        {  3, 0x30, 0x2E },
        {  3, 0x52, 0x2E },
        {  3, 0x55, 0x2E },
        {  3, 0x56, 0x2E },
        {  3, 0x20, 0x2E },
        {  3, 0x5E, 0x2E },
        {  3, 0x5F, 0x2E },
    };
    uint8_t ts = wCurMapTileset;
    for (int i = 0; i < (int)(sizeof(kPairs)/sizeof(kPairs[0])); i++) {
        if (kPairs[i].ts != ts) continue;
        if ((kPairs[i].t1 == a && kPairs[i].t2 == b) ||
            (kPairs[i].t1 == b && kPairs[i].t2 == a))
            return 1;
    }
    return 0;
}
