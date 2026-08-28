
#include "warp.h"
#include "debug_cli.h"
#include "debug_trace.h"
#include "amberscript_core.h"
#include "amberscript_tilemod.h"
#include "amberscript_mapbank.h"
#include "overworld.h"
#include "player.h"
#include "npc.h"
#include "amberscript_scene.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "../platform/display.h"
#include "../data/event_data.h"
#include "../data/event_constants.h"
#include "../data/map_data.h"
#include "../game/constants.h"
#include "pallet_scripts.h"
#include "oakslab_scripts.h"
#include "viridian_mart_scripts.h"
#include "route24_scripts.h"
#include "blues_house_scripts.h"
#include "bills_house_scripts.h"
#include "route2gate_scripts.h"
#include "vermilion_gym_scripts.h"
#include "cinnabar_gym_scripts.h"
#include "rockethideout_b4f_scripts.h"
#include "rockethideout_scripts.h"
#include "game_corner_scripts.h"
#include "celadon_city_scripts.h"
#include "trainer_sight.h"
#include "gate_scripts.h"
#include "pokeflute.h"
#include <stdio.h>
#include <string.h>

#if defined(__GNUC__)
__attribute__((weak))
#endif
int DebugCLI_GetWarpOverrideAt(int x, int y, uint8_t *has_warp, uint8_t *dest_map, uint8_t *dest_warp_idx) {
    (void)x;
    (void)y;
    (void)has_warp;
    (void)dest_map;
    (void)dest_warp_idx;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
void DebugCLI_ClearTileOverrides(void) {
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_IsEnabled(void) {
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_GetWarpOverrideDestNameAt(int x, int y, char *out_name, size_t out_cap) {
    (void)x;
    (void)y;
    (void)out_name;
    (void)out_cap;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_GetWarpOverrideAt(int x, int y, uint8_t *has_warp, uint8_t *dest_map, uint8_t *dest_warp_idx) {
    (void)x;
    (void)y;
    (void)has_warp;
    (void)dest_map;
    (void)dest_warp_idx;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_MapBank_GetWarpSpotForRealId(int real_id, int spot_idx, int *x, int *y) {
    (void)real_id;
    (void)spot_idx;
    (void)x;
    (void)y;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_MapBank_GetWarpSpotForName(const char *name, int spot_idx, int *x, int *y) {
    (void)name;
    (void)spot_idx;
    (void)x;
    (void)y;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_MapBank_IsIndoorForRealId(int real_id) {
    (void)real_id;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_MapBank_GetNoDoorStepForRealId(int real_id) {
    (void)real_id;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_MapBank_IsWarpWalkIntoAt(int real_id, int x, int y) {
    (void)real_id;
    (void)x;
    (void)y;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_MapBank_GetWarpWalkIntoDirAt(int real_id, int x, int y) {
    (void)real_id;
    (void)x;
    (void)y;
    return 0;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
int AmberScript_MapBank_IsWarpStairAt(int real_id, int x, int y) {
    (void)real_id;
    (void)x;
    (void)y;
    return 0;
}

#define LAST_MAP 0xFF

#define TILESET_OVERWORLD  0
#define TILESET_PLATEAU   23

static const uint8_t kDoorTiles_Overworld[]   = {0x1B, 0x58, 0xFF};
static const uint8_t kDoorTiles_Mart[]        = {0x5E, 0xFF};
static const uint8_t kDoorTiles_Forest[]      = {0x3A, 0xFF};
static const uint8_t kDoorTiles_House[]       = {0x54, 0xFF};
static const uint8_t kDoorTiles_Museum[]      = {0x3B, 0xFF};
static const uint8_t kDoorTiles_Ship[]        = {0x1E, 0xFF};
static const uint8_t kDoorTiles_Lobby[]       = {0x1C, 0x38, 0x1A, 0xFF};
static const uint8_t kDoorTiles_Mansion[]     = {0x1A, 0x1C, 0x53, 0xFF};
static const uint8_t kDoorTiles_Lab[]         = {0x34, 0xFF};
static const uint8_t kDoorTiles_Facility[]    = {0x43, 0x58, 0x1B, 0xFF};
static const uint8_t kDoorTiles_Plateau[]     = {0x3B, 0x1B, 0xFF};
static const uint8_t kDoorTiles_Empty[]       = {0xFF};

static const uint8_t * const kDoorTilesByTileset[NUM_TILESETS] = {
     kDoorTiles_Overworld,
     kDoorTiles_Empty,
     kDoorTiles_Mart,
     kDoorTiles_Forest,
     kDoorTiles_Empty,
     kDoorTiles_Empty,
     kDoorTiles_Empty,
     kDoorTiles_Empty,
     kDoorTiles_House,
     kDoorTiles_Museum,
     kDoorTiles_Museum,
     kDoorTiles_Empty,
     kDoorTiles_Museum,
     kDoorTiles_Ship,
     kDoorTiles_Empty,
     kDoorTiles_Empty,
     kDoorTiles_Empty,
     kDoorTiles_Empty,
     kDoorTiles_Lobby,
     kDoorTiles_Mansion,
     kDoorTiles_Lab,
     kDoorTiles_Empty,
     kDoorTiles_Facility,
     kDoorTiles_Plateau,
};

int Warp_IsDoorTile(uint8_t tile) {
    if (wCurMapTileset >= NUM_TILESETS) return 0;
    const uint8_t *p = kDoorTilesByTileset[wCurMapTileset];
    for (; *p != 0xFF; p++) {
        if (*p == tile) return 1;
    }
    return 0;
}

static int is_door_tile(uint8_t tile) {
    if (wCurMapTileset >= NUM_TILESETS) return 0;
    const uint8_t *p = kDoorTilesByTileset[wCurMapTileset];
    for (; *p != 0xFF; p++) {
        if (*p == tile) return 1;
    }
    return 0;
}

static void fire_map_onload_callbacks(void) {
    PalletScripts_OnMapLoad();
    OaksLabScripts_OnMapLoad();
    ViridianMartScripts_OnMapLoad();
    Route24Scripts_OnMapLoad();
    BluesHouseScripts_OnMapLoad();
    BillsHouseScripts_OnMapLoad();
    Route2GateScripts_OnMapLoad();
    VermilionGymScripts_OnMapLoad();
    RocketHideoutB4FScripts_OnMapLoad();
    RocketHideoutScripts_OnMapLoad();
    GameCornerScripts_OnMapLoad();
    CeladonGiftScripts_OnMapLoad();
    CinnabarGymScripts_OnMapLoad();
    Trainer_LoadMap();
    Gate_LoadMap();
    PokeFlute_LoadMap();
}

static const uint8_t kWarpTiles_Overworld[]   = {0x1B, 0x58, 0xFF};
static const uint8_t kWarpTiles_RedsHouse[]   = {0x1A, 0x1C, 0xFF};
static const uint8_t kWarpTiles_Mart[]        = {0x5E, 0xFF};
static const uint8_t kWarpTiles_Forest[]      = {0x5A, 0x5C, 0x3A, 0xFF};
static const uint8_t kWarpTiles_Dojo[]        = {0x4A, 0xFF};
static const uint8_t kWarpTiles_House[]       = {0x54, 0x5C, 0x32, 0xFF};
static const uint8_t kWarpTiles_ForestGate[]  = {0x3B, 0x1A, 0x1C, 0xFF};
static const uint8_t kWarpTiles_Underground[] = {0x13, 0xFF};
static const uint8_t kWarpTiles_Ship[]        = {0x37, 0x39, 0x1E, 0x4A, 0xFF};
static const uint8_t kWarpTiles_Cemetery[]    = {0x1B, 0x13, 0xFF};
static const uint8_t kWarpTiles_Interior[]    = {0x15, 0x55, 0x04, 0xFF};
static const uint8_t kWarpTiles_Cavern[]      = {0x18, 0x1A, 0x22, 0xFF};
static const uint8_t kWarpTiles_Lobby[]       = {0x1A, 0x1C, 0x38, 0xFF};
static const uint8_t kWarpTiles_Mansion[]     = {0x1A, 0x1C, 0x53, 0xFF};
static const uint8_t kWarpTiles_Lab[]         = {0x34, 0xFF};
static const uint8_t kWarpTiles_Facility[]    = {0x43, 0x58, 0x20, 0x1B, 0x13, 0xFF};
static const uint8_t kWarpTiles_Plateau[]     = {0x1B, 0x3B, 0xFF};
static const uint8_t kWarpTiles_Empty[]       = {0xFF};

static const uint8_t * const kWarpTilesByTileset[NUM_TILESETS] = {
     kWarpTiles_Overworld,
     kWarpTiles_RedsHouse,
     kWarpTiles_Mart,
     kWarpTiles_Forest,
     kWarpTiles_RedsHouse,
     kWarpTiles_Dojo,
     kWarpTiles_Mart,
     kWarpTiles_Dojo,
     kWarpTiles_House,
     kWarpTiles_ForestGate,
     kWarpTiles_ForestGate,
     kWarpTiles_Underground,
     kWarpTiles_ForestGate,
     kWarpTiles_Ship,
     kWarpTiles_Empty,
     kWarpTiles_Cemetery,
     kWarpTiles_Interior,
     kWarpTiles_Cavern,
     kWarpTiles_Lobby,
     kWarpTiles_Mansion,
     kWarpTiles_Lab,
     kWarpTiles_Empty,
     kWarpTiles_Facility,
     kWarpTiles_Plateau,
};

static int is_warp_trigger_tile(uint8_t tile) {
    if (wCurMapTileset >= NUM_TILESETS) return 1;
    const uint8_t *p = kWarpTilesByTileset[wCurMapTileset];
    for (; *p != 0xFF; p++) {
        if (*p == tile) return 1;
    }
    return 0;
}

static int is_silph_co_floor_map(uint8_t map_id) {
    switch (map_id) {
        case 0xCF:
        case 0xD0:
        case 0xD1:
        case 0xD2:
        case 0xD3:
        case 0xD4:
        case 0xD5:
        case 0xE9:
        case 0xEA:
        case 0xEB:
            return 1;
        default:
            return 0;
    }
}

static int is_facing_edge_of_map(void) {
    switch (gPlayerFacing & 3) {
        case 0: return (int)wYCoord == (int)wCurMapHeight * 2 - 1;
        case 1: return (int)wYCoord == 0;
        case 2: return (int)wXCoord == 0;
        case 3: return (int)wXCoord == (int)wCurMapWidth  * 2 - 1;
    }
    return 0;
}

static int is_position_at_any_map_edge(void) {
    return (int)wYCoord == 0 || (int)wYCoord == (int)wCurMapHeight * 2 - 1 ||
           (int)wXCoord == 0 || (int)wXCoord == (int)wCurMapWidth  * 2 - 1;
}

static int gWarpJustHappened = 0;

static int gWarpDoorStep = 0;

static int gWarpDoorStepFaceUp = 0;

static int     gWarpPending     = 0;
static uint8_t gPendingDestMap  = 0;
static uint8_t gPendingDestIdx  = 0;

static char    gPendingDestMapName[32] = {0};
static int     gPendingExactPos = 0;
static int     gPendingX        = 0;
static int     gPendingY        = 0;

static int     gPendingElevatorLanding = 0;
static uint8_t gPendingFromMap  = 0;
static uint8_t gPendingFromWarp = 0;

static char    gPendingFromMapName[32] = {0};
static int     gPendingDungeonHole = 0;
static int     gPendingTeleportPad = 0;

static int     gSilphElevatorDestValid = 0;
static uint8_t gSilphElevatorDestMap   = 0;
static int     gSilphElevatorDestX     = 0;
static int     gSilphElevatorDestY     = 0;
static int     gSilphElevatorReturnValid = 0;
static uint8_t gSilphElevatorReturnMap   = 0;
static uint8_t gSilphElevatorReturnWarp  = 0;

static int     gVmapElevatorDestValid = 0;
static char    gVmapElevatorDestName[64] = {0};
static int     gVmapElevatorDestX = 0;
static int     gVmapElevatorDestY = 0;

static int rocket_elevator_landing(const char *floor_name, int *x_out, int *y_out) {
    if (!floor_name) return 0;
    if (!strcmp(floor_name, "RocketHideoutB1F")) { *x_out = 24; *y_out = 19; return 1; }
    if (!strcmp(floor_name, "RocketHideoutB2F")) { *x_out = 24; *y_out = 19; return 1; }
    if (!strcmp(floor_name, "RocketHideoutB4F")) { *x_out = 24; *y_out = 15; return 1; }
    return 0;
}

static int silph_elevator_landing(const char *floor_name, int *x_out, int *y_out) {
    if (!floor_name) return 0;
    if (!strcmp(floor_name, "SilphCo1F"))  { *x_out = 20; *y_out = 0; return 1; }
    if (!strcmp(floor_name, "SilphCo2F"))  { *x_out = 20; *y_out = 0; return 1; }
    if (!strcmp(floor_name, "SilphCo3F"))  { *x_out = 20; *y_out = 0; return 1; }
    if (!strcmp(floor_name, "SilphCo4F"))  { *x_out = 20; *y_out = 0; return 1; }
    if (!strcmp(floor_name, "SilphCo5F"))  { *x_out = 20; *y_out = 0; return 1; }
    if (!strcmp(floor_name, "SilphCo6F"))  { *x_out = 18; *y_out = 0; return 1; }
    if (!strcmp(floor_name, "SilphCo7F"))  { *x_out = 18; *y_out = 0; return 1; }
    if (!strcmp(floor_name, "SilphCo8F"))  { *x_out = 18; *y_out = 0; return 1; }
    if (!strcmp(floor_name, "SilphCo9F"))  { *x_out = 18; *y_out = 0; return 1; }
    if (!strcmp(floor_name, "SilphCo10F")) { *x_out = 12; *y_out = 0; return 1; }
    if (!strcmp(floor_name, "SilphCo11F")) { *x_out = 13; *y_out = 0; return 1; }
    return 0;
}

typedef struct {
    uint8_t src_map;
    uint8_t src_x;
    uint8_t src_y;
    uint8_t dst_map;
    uint8_t dst_x;
    uint8_t dst_y;
} dungeon_hole_warp_t;

static const dungeon_hole_warp_t kDungeonHoleWarps[] = {

    { 0xC0, 17,  6, 0x9F, 18,  7 },
    { 0xC0, 24,  6, 0x9F, 23,  7 },

    { 0x9F, 18,  6, 0xA0, 19,  7 },
    { 0x9F, 23,  6, 0xA0, 22,  7 },

    { 0xA0, 19,  6, 0xA1, 18,  7 },
    { 0xA0, 22,  6, 0xA1, 19,  7 },

    { 0xA1,  3, 16, 0xA2,  4, 14 },
    { 0xA1,  6, 16, 0xA2,  5, 14 },

    { 0xC6, 23, 15, 0xC2, 22, 16 },

    { 0xD7, 16, 14, 0xA5, 16, 14 },
    { 0xD7, 17, 14, 0xA5, 16, 14 },
    { 0xD7, 19, 14, 0xD6, 18, 14 },
};

int Warp_JustHappened(void) {
    int v = gWarpJustHappened;
    gWarpJustHappened = 0;
    return v;
}

int Warp_IsPending(void) {
    return gWarpPending;
}

int Warp_IsPendingDungeonHole(void) {
    return gWarpPending && gPendingDungeonHole;
}

int Warp_IsPendingTeleportPad(void) {
    return gWarpPending && gPendingTeleportPad;
}

void Warp_SetSilphElevatorDestination(uint8_t map_id, int tile_x, int tile_y) {
    gSilphElevatorDestValid = 1;
    gSilphElevatorDestMap   = map_id;
    gSilphElevatorDestX     = tile_x;
    gSilphElevatorDestY     = tile_y;
}

void Warp_SetVmapElevatorDestination(const char *vmap_name, int tile_x, int tile_y) {
    gVmapElevatorDestValid = 1;
    snprintf(gVmapElevatorDestName, sizeof(gVmapElevatorDestName), "%s", vmap_name ? vmap_name : "");
    gVmapElevatorDestX = tile_x;
    gVmapElevatorDestY = tile_y;
}

int Warp_HasDoorStep(void) {
    int v = gWarpDoorStep;
    gWarpDoorStep = 0;
    return v;
}

int Warp_ConsumeDoorStepFaceUp(void) {
    int v = gWarpDoorStepFaceUp;
    gWarpDoorStepFaceUp = 0;
    return v;
}

void Warp_Reset(void) {
    gPendingDungeonHole = 0;
    gPendingTeleportPad = 0;
    Player_IgnoreInputFrames(0);
}

static int is_outdoor_map(void) {

    return !AmberScript_MapBank_IsIndoorForRealId((int)wCurMap);
}

static int is_warp_tile_in_front_of_player(void) {
    int fx = (int)wXCoord;
    int fy = (int)wYCoord;
    switch (gPlayerFacing & 3) {
        case 0: fy += 1; break;
        case 1: fy -= 1; break;
        case 2: fx -= 1; break;
        case 3: fx += 1; break;
    }
    return is_warp_trigger_tile(Map_GetGameTile(fx, fy));
}

static int is_warp_event_enabled(const map_warp_t *w) {
    if (!w) return 0;
    if (wCurMap == 0x87 && w->x == 17 && w->y == 4) {
        return CheckEvent(EVENT_FOUND_ROCKET_HIDEOUT);
    }
    return 1;
}

static int s_forced_warp = 0;

void Warp_SetForced(int on) { s_forced_warp = on ? 1 : 0; }
int  Warp_IsForced(void)    { return s_forced_warp; }

static uint8_t joy_held_with_sim(void) {
    uint8_t held = hJoyHeld;
    switch (Player_GetSimulatedHeldDir()) {
        case 0: held |= PAD_DOWN;  break;
        case 1: held |= PAD_UP;    break;
        case 2: held |= PAD_LEFT;  break;
        case 3: held |= PAD_RIGHT; break;
        default: break;
    }
    return held;
}

static int get_warp_override_at(int x, int y, uint8_t *has_warp, uint8_t *dest_map, uint8_t *dest_warp_idx) {
    return AmberScript_IsEnabled()
        ? AmberScript_GetWarpOverrideAt(x, y, has_warp, dest_map, dest_warp_idx)
        : DebugCLI_GetWarpOverrideAt(x, y, has_warp, dest_map, dest_warp_idx);
}

static void cache_pending_warp_name(int x, int y) {
    if (!AmberScript_IsEnabled() ||
        !AmberScript_GetWarpOverrideDestNameAt(x, y, gPendingDestMapName, sizeof(gPendingDestMapName))) {
        gPendingDestMapName[0] = '\0';
    }
}

static void cache_pending_from_name(uint8_t from_map) {
    const char *n = AmberScript_IsEnabled() ? AmberScript_MapBank_NameForRealId(from_map) : NULL;
    snprintf(gPendingFromMapName, sizeof(gPendingFromMapName), "%s", n ? n : "");
}

int Warp_HasEventAt(int x, int y) {
    uint8_t has_debug_warp = 0;
    if (get_warp_override_at(x, y, &has_debug_warp, NULL, NULL))
        return has_debug_warp ? 1 : 0;

    return 0;
}

int Warp_CheckCollision(void) {
    if (wCurMap >= NUM_MAPS) return 0;
    if (is_position_at_any_map_edge() && !is_facing_edge_of_map()) return 0;

    const map_events_t *ev = &gMapEvents[wCurMap];
    {
        uint8_t has_debug_warp = 0, debug_dest_map = 0, debug_dest_idx = 0;
        if (get_warp_override_at((int)wXCoord, (int)wYCoord,
                                  &has_debug_warp, &debug_dest_map, &debug_dest_idx)) {
            if (!has_debug_warp) return 0;

            if (!AmberScript_MapBank_IsWarpStairAt((int)wCurMap, (int)wXCoord, (int)wYCoord)) {
                if (!AmberScript_MapBank_IsWarpWalkIntoAt((int)wCurMap, (int)wXCoord, (int)wYCoord))
                    return 0;
                int need = AmberScript_MapBank_GetWarpWalkIntoDirAt((int)wCurMap, (int)wXCoord, (int)wYCoord);
                uint8_t need_pad = 0;
                if (need & PKS_FACE_DOWN)  need_pad |= PAD_DOWN;
                if (need & PKS_FACE_UP)    need_pad |= PAD_UP;
                if (need & PKS_FACE_LEFT)  need_pad |= PAD_LEFT;
                if (need & PKS_FACE_RIGHT) need_pad |= PAD_RIGHT;
                uint8_t gate_pad = need_pad ? need_pad : (uint8_t)PAD_CTRL_PAD;
                if (!(joy_held_with_sim() & gate_pad)) return 0;
            }
            uint8_t from_map = wCurMap;
            int is_last_map = (debug_dest_map == LAST_MAP);
            uint8_t dest_map = is_last_map ? wLastMap : debug_dest_map;
            if (dest_map >= NUM_MAPS) return 0;
            if (is_outdoor_map()) wLastMap = wCurMap;
            Audio_PlaySFX_GoInside();
            gPendingDestMap  = dest_map;
            if (is_last_map) gPendingDestMapName[0] = '\0';
            else cache_pending_warp_name((int)wXCoord, (int)wYCoord);
            gPendingDestIdx  = debug_dest_idx;
            gPendingFromMap  = from_map;
            cache_pending_from_name(from_map);
            gPendingFromWarp = 0;
            gPendingExactPos = 0;
            gPendingDungeonHole = 0;
            gPendingTeleportPad = 0;
            gWarpPending      = 1;
            gWarpJustHappened = 1;
            printf("[warp_collision] debug tile map %d -> map %d (warp %d)\n",
                   wCurMap, dest_map, debug_dest_idx);
            return 1;
        }
    }
    if (!ev->warps || ev->num_warps == 0) return 0;

    for (int i = 0; i < ev->num_warps; i++) {
        const map_warp_t *w = &ev->warps[i];
        if (!is_warp_event_enabled(w)) continue;
        if (w->x != wXCoord || w->y != wYCoord) continue;

        uint8_t from_map = wCurMap;
        uint8_t dest_map = w->dest_map;
        if (dest_map == LAST_MAP) dest_map = wLastMap;
        if (dest_map >= NUM_MAPS) return 0;

        if (is_outdoor_map()) wLastMap = wCurMap;

        Audio_PlaySFX_GoInside();

        gPendingDestMap  = dest_map;
        gPendingDestMapName[0] = '\0';
        gPendingDestIdx  = w->dest_warp_idx;
        gPendingFromMap  = from_map;
        cache_pending_from_name(from_map);
        gPendingFromWarp = (uint8_t)i;
        gPendingExactPos = 0;
        gPendingDungeonHole = 0;
        gPendingTeleportPad = 0;
        gWarpPending      = 1;
        gWarpJustHappened = 1;
        printf("[warp_collision] map %d -> map %d (warp %d)\n",
               wCurMap, dest_map, w->dest_warp_idx);
        return 1;
    }
    return 0;
}

int Warp_CheckDungeonHole(void) {

    int dh_real = Map_CurrentRealId();
    if (dh_real < 0) return 0;
    for (int i = 0; i < (int)(sizeof(kDungeonHoleWarps) / sizeof(kDungeonHoleWarps[0])); i++) {
        const dungeon_hole_warp_t *h = &kDungeonHoleWarps[i];
        if ((int)h->src_map != dh_real) continue;
        if (h->src_x != (uint8_t)wXCoord || h->src_y != (uint8_t)wYCoord) continue;

        gPendingDestMap  = h->dst_map;

        if (gMapTable[h->dst_map].name[0])
            snprintf(gPendingDestMapName, sizeof(gPendingDestMapName), "%s",
                     gMapTable[h->dst_map].name);
        else
            gPendingDestMapName[0] = '\0';
        gPendingDestIdx  = 0;
        gPendingFromMap  = wCurMap;
        gPendingFromWarp = 0;
        gPendingExactPos = 1;
        gPendingX        = h->dst_x;
        gPendingY        = h->dst_y;
        gPendingDungeonHole = 1;
        gPendingTeleportPad = 0;
        gWarpPending      = 1;
        gWarpJustHappened = 1;
        return 1;
    }
    return 0;
}

void Warp_QueueTeleportPadVmap(const char *vmap_name, int tile_x, int tile_y) {
    gPendingDestMap  = (uint8_t)PKS_VIRTUAL_MAP_FIRST;
    snprintf(gPendingDestMapName, sizeof(gPendingDestMapName), "%s", vmap_name ? vmap_name : "");
    gPendingDestIdx  = 0;
    gPendingExactPos = 1;
    gPendingX        = tile_x;
    gPendingY        = tile_y;
    gPendingFromMap  = wCurMap;
    gPendingFromWarp = 0;
    gPendingDungeonHole = 0;
    gPendingTeleportPad = 1;
    gWarpPending      = 1;
    gWarpJustHappened = 1;
    printf("[warp] teleport pad -> vmap '%s' @ (%d,%d)\n", gPendingDestMapName, tile_x, tile_y);
}

static int warp_check_impl(int is_boundary_crossing_attempt) {
    const int warp_debug = 0;
    if (wCurMap >= NUM_MAPS) return 0;

    const map_events_t *ev = &gMapEvents[wCurMap];
    {
        uint8_t has_debug_warp = 0, debug_dest_map = 0, debug_dest_idx = 0;
        if (get_warp_override_at((int)wXCoord, (int)wYCoord,
                                  &has_debug_warp, &debug_dest_map, &debug_dest_idx)) {
            if (!has_debug_warp) return 0;
            uint8_t warp_tile = Map_GetGameTile((int)wXCoord, (int)wYCoord);

            Trace_Emit(TRACE_WARP,
                "\"path\":\"vmap\",\"map\":%d,\"x\":%d,\"y\":%d,\"bnd\":%d,"
                "\"indoor\":%d,\"edge\":%d,\"face_edge\":%d,\"held\":%d,"
                "\"nds\":%d,\"walk_into\":%d,\"tile\":%d",
                (int)wCurMap, (int)wXCoord, (int)wYCoord,
                is_boundary_crossing_attempt,
                AmberScript_MapBank_IsIndoorForRealId((int)wCurMap),
                is_position_at_any_map_edge(), is_facing_edge_of_map(),
                (int)(hJoyHeld & PAD_CTRL_PAD),
                AmberScript_MapBank_GetNoDoorStepForRealId((int)wCurMap),
                AmberScript_MapBank_IsWarpWalkIntoAt((int)wCurMap, (int)wXCoord, (int)wYCoord),
                warp_tile);

            if (AmberScript_MapBank_IsIndoorForRealId((int)wCurMap) &&
                is_position_at_any_map_edge() &&

                !AmberScript_MapBank_IsWarpStairAt((int)wCurMap, (int)wXCoord, (int)wYCoord) &&
                (!is_facing_edge_of_map() ||
                 (!is_boundary_crossing_attempt && !s_forced_warp &&
                  !(hJoyHeld & PAD_CTRL_PAD)))) {
                Trace_Emit(TRACE_WARP, "\"out\":\"suppressed\",\"gate\":\"indoor_exit_mat\"");
                return 0;
            }

            if (!is_boundary_crossing_attempt &&
                AmberScript_MapBank_GetNoDoorStepForRealId((int)wCurMap) &&
                is_position_at_any_map_edge()) {
                Trace_Emit(TRACE_WARP, "\"out\":\"suppressed\",\"gate\":\"no_door_step_edge\"");
                return 0;
            }

            if (!is_boundary_crossing_attempt &&
                AmberScript_MapBank_IsWarpWalkIntoAt((int)wCurMap, (int)wXCoord, (int)wYCoord)) {

                int need = AmberScript_MapBank_GetWarpWalkIntoDirAt((int)wCurMap, (int)wXCoord, (int)wYCoord);
                uint8_t need_pad = 0;
                if (need & PKS_FACE_DOWN)  need_pad |= PAD_DOWN;
                if (need & PKS_FACE_UP)    need_pad |= PAD_UP;
                if (need & PKS_FACE_LEFT)  need_pad |= PAD_LEFT;
                if (need & PKS_FACE_RIGHT) need_pad |= PAD_RIGHT;
                uint8_t gate_pad = need_pad ? need_pad : (uint8_t)PAD_CTRL_PAD;
                if (!s_forced_warp && !(joy_held_with_sim() & gate_pad)) {
                    Trace_Emit(TRACE_WARP, "\"out\":\"suppressed\",\"gate\":\"walk_into_dir\",\"need\":%d", need);
                    return 0;
                }
            }
            uint8_t from_map = wCurMap;
            int is_last_map = (debug_dest_map == LAST_MAP);
            uint8_t dest_map = is_last_map ? wLastMap : debug_dest_map;
            if (dest_map >= NUM_MAPS) {
                printf("[warp] debug dest_map 0x%02X out of range\n", dest_map);
                return 0;
            }
            if (is_outdoor_map()) wLastMap = wCurMap;
            if (warp_tile == 0x0B)
                Audio_PlaySFX_GoInside();
            else
                Audio_PlaySFX_GoOutside();
            gPendingDestMap  = dest_map;
            if (is_last_map) gPendingDestMapName[0] = '\0';
            else cache_pending_warp_name((int)wXCoord, (int)wYCoord);
            gPendingDestIdx  = debug_dest_idx;
            gPendingFromMap  = from_map;
            cache_pending_from_name(from_map);
            gPendingFromWarp = 0;
            gPendingExactPos = 0;
            gPendingDungeonHole = 0;

            {
                const char *cur_name = AmberScript_MapBank_NameForRealId((int)wCurMap);
                int is_celadon = cur_name && !strcmp(cur_name, "CeladonMartElevator") &&
                    ((int)wXCoord == 1 || (int)wXCoord == 2) && (int)wYCoord == 3;
                int is_rocket = cur_name && !strcmp(cur_name, "RocketHideoutElevator") &&
                    ((int)wXCoord == 2 || (int)wXCoord == 3) && (int)wYCoord == 1;

                int is_silph = cur_name && !strcmp(cur_name, "SilphCoElevator") &&
                    ((int)wXCoord == 1 || (int)wXCoord == 2) && (int)wYCoord == 3;
                if ((is_celadon || is_rocket || is_silph) && gVmapElevatorDestValid) {
                    gPendingExactPos = 1;
                    gPendingElevatorLanding = 1;
                    gPendingX = gVmapElevatorDestX;
                    gPendingY = gVmapElevatorDestY;
                    snprintf(gPendingDestMapName, sizeof(gPendingDestMapName), "%s", gVmapElevatorDestName);
                    gVmapElevatorDestValid = 0;
                    printf("[elev] exit uses selected floor -> vmap='%s' xy=(%d,%d)\n",
                           gPendingDestMapName, gPendingX, gPendingY);
                }
            }

            gPendingTeleportPad = 0;
            gWarpPending      = 1;
            gWarpJustHappened = 1;
            printf("[warp] debug tile map %d -> map %d (warp %d) queued\n",
                   wCurMap, dest_map, debug_dest_idx);
            Trace_Emit(TRACE_WARP, "\"out\":\"fire\",\"dest\":%d,\"idx\":%d",
                       dest_map, debug_dest_idx);
            return 1;
        }
    }
    if (!ev->warps || ev->num_warps == 0) return 0;

    if (warp_debug) {
        printf("[warp_check] map=%d pos=(%d,%d) facing=%d edge=%d warps=%d\n",
               wCurMap, wXCoord, wYCoord, gPlayerFacing,
               (int)((wCurMapHeight > 0) ? (wYCoord == (int16_t)(wCurMapHeight * 2 - 1)) : 0),
               ev->num_warps);
    }

    for (int i = 0; i < ev->num_warps; i++) {
        const map_warp_t *w = &ev->warps[i];
        if (!is_warp_event_enabled(w)) continue;
        if (warp_debug) {
            printf("[warp_check]   warp[%d]=(%d,%d) match=%d\n",
                   i, w->x, w->y, (w->x == wXCoord && w->y == wYCoord));
        }
        if (w->x != wXCoord || w->y != wYCoord) continue;

        uint8_t from_map = wCurMap;

        uint8_t warp_tile = Map_GetGameTile((int)w->x, (int)w->y);
        Trace_Emit(TRACE_WARP,
            "\"path\":\"real\",\"map\":%d,\"x\":%d,\"y\":%d,\"bnd\":%d,\"tile\":%d,"
            "\"trig\":%d,\"outdoor\":%d,\"face_edge\":%d,\"front\":%d,\"held\":%d",
            (int)wCurMap, (int)w->x, (int)w->y, is_boundary_crossing_attempt,
            warp_tile, is_warp_trigger_tile(warp_tile), is_outdoor_map(),
            is_facing_edge_of_map(), is_warp_tile_in_front_of_player(),
            (int)(hJoyHeld & PAD_CTRL_PAD));
        if (!is_warp_trigger_tile(warp_tile)) {
            if (is_outdoor_map()) {

                if (!is_warp_tile_in_front_of_player()) {
                    Trace_Emit(TRACE_WARP, "\"out\":\"suppressed\",\"gate\":\"not_facing_warp_tile\"");
                    continue;
                }
                if (!s_forced_warp && !(hJoyHeld & PAD_CTRL_PAD)) {
                    Trace_Emit(TRACE_WARP, "\"out\":\"suppressed\",\"gate\":\"outdoor_no_press\"");
                    continue;
                }
            } else if (!is_facing_edge_of_map() ||
                       (!s_forced_warp && !(hJoyHeld & PAD_CTRL_PAD))) {

                Trace_Emit(TRACE_WARP, "\"out\":\"suppressed\",\"gate\":\"indoor_edge_or_press\"");
                continue;
            }
        }

        uint8_t dest_map = w->dest_map;
        if (dest_map == LAST_MAP) {
            dest_map = wLastMap;
        }
        if (dest_map >= NUM_MAPS) {
            printf("[warp] dest_map 0x%02X out of range\n", dest_map);
            return 0;
        }

        uint8_t dest_idx = w->dest_warp_idx;
        int use_silph_elevator_override = 0;
        int use_silph_elevator_return = 0;
        if (wCurMap == 0xEC && (w->y == 3) && (w->x == 1 || w->x == 2) && gSilphElevatorDestValid) {
            dest_map = gSilphElevatorDestMap;
            use_silph_elevator_override = 1;
            printf("[elev] silph exit uses selected floor -> map=%u xy=(%d,%d)\n",
                   gSilphElevatorDestMap, gSilphElevatorDestX, gSilphElevatorDestY);
        } else if (wCurMap == 0xEC && (w->y == 3) && (w->x == 1 || w->x == 2) && gSilphElevatorReturnValid) {
            dest_map = gSilphElevatorReturnMap;
            dest_idx = gSilphElevatorReturnWarp;
            use_silph_elevator_return = 1;
            printf("[elev] silph exit uses stored return -> map=%u warp=%u\n",
                   gSilphElevatorReturnMap, gSilphElevatorReturnWarp);
        }

        int use_vmap_elevator_override = 0;

        if (is_outdoor_map()) {
            wLastMap = wCurMap;
        }

        if (warp_tile == 0x0B)
            Audio_PlaySFX_GoInside();
        else
            Audio_PlaySFX_GoOutside();

        gPendingDestMap  = dest_map;
        gPendingDestMapName[0] = '\0';
        gPendingDestIdx  = dest_idx;
        gPendingFromMap  = from_map;
        cache_pending_from_name(from_map);
        gPendingFromWarp = (uint8_t)i;
        gPendingDungeonHole = 0;
        gPendingTeleportPad = ((warp_tile == 0x20) ||
                               (is_silph_co_floor_map(wCurMap) && w->y != 0)) ? 1 : 0;
        if (use_vmap_elevator_override) {
            gPendingExactPos = 1;
            gPendingElevatorLanding = 1;
            gPendingX = gVmapElevatorDestX;
            gPendingY = gVmapElevatorDestY;
            snprintf(gPendingDestMapName, sizeof(gPendingDestMapName), "%s", gVmapElevatorDestName);
            gVmapElevatorDestValid = 0;
        } else if (use_silph_elevator_override) {
            gPendingExactPos = 1;
            gPendingElevatorLanding = 1;
            gPendingX = gSilphElevatorDestX;
            gPendingY = gSilphElevatorDestY;
            gSilphElevatorDestValid = 0;
        } else if (use_silph_elevator_return) {
            gPendingExactPos = 0;
            gPendingElevatorLanding = 1;
        } else {
            gPendingExactPos = 0;
        }
        gWarpPending      = 1;
        gWarpJustHappened = 1;
        printf("[warp] map %d -> map %d (warp %d) queued\n",
               from_map, dest_map, dest_idx);
        Trace_Emit(TRACE_WARP,
            "\"out\":\"fire\",\"dest\":%d,\"idx\":%d,\"pad\":%d,\"exact\":%d",
            dest_map, dest_idx, gPendingTeleportPad, gPendingExactPos);

        return 1;
    }
    return 0;
}

int Warp_Check(void) {
    return warp_check_impl(0);
}

int Warp_CheckAtMapBoundary(void) {
    return warp_check_impl(1);
}

void Warp_Execute(void) {
    if (!gWarpPending) return;
    gWarpPending = 0;
    gPendingDungeonHole = 0;
    uint8_t was_teleport_pad = gPendingTeleportPad;
    gPendingTeleportPad = 0;

    uint8_t dest_map = gPendingDestMap;
    uint8_t dest_idx = gPendingDestIdx;
    uint8_t from_map = gPendingFromMap;
    uint8_t from_warp = gPendingFromWarp;

    int     prev_was_dark   = Map_IsDarkMap((int)wCurMap);
    uint8_t prev_pal_offset = gMapPalOffset;

    if (gPendingDestMapName[0] && AmberScript_IsEnabled()) {
        int fresh_id = AmberScript_MapBank_EnsureResidentByName(gPendingDestMapName);
        if (fresh_id >= 0) dest_map = (uint8_t)fresh_id;
    }

    Map_Load(dest_map);

    if (dest_map == 0xEC && from_map != 0xEC) {
        gSilphElevatorReturnValid = 1;
        gSilphElevatorReturnMap = from_map;
        gSilphElevatorReturnWarp = from_warp;
        printf("[elev] silph set stored return from entry -> from_map=%u from_warp=%u\n",
               from_map, from_warp);
    }

    {
        const char *dest_name = AmberScript_MapBank_NameForRealId(dest_map);
        int from_is_elevator = !strcmp(gPendingFromMapName, "CeladonMartElevator") ||
                                !strcmp(gPendingFromMapName, "RocketHideoutElevator") ||
                                !strcmp(gPendingFromMapName, "SilphCoElevator");
        if (dest_name && !strcmp(dest_name, "CeladonMartElevator") &&
            gPendingFromMapName[0] && !from_is_elevator) {
            Warp_SetVmapElevatorDestination(gPendingFromMapName, 1, 1);
            printf("[elev] mart set stored return from entry -> from_vmap='%s'\n",
                   gPendingFromMapName);
        } else if (dest_name && !strcmp(dest_name, "RocketHideoutElevator") &&
                   gPendingFromMapName[0] && !from_is_elevator) {
            int lx, ly;
            if (rocket_elevator_landing(gPendingFromMapName, &lx, &ly)) {
                Warp_SetVmapElevatorDestination(gPendingFromMapName, lx, ly);
                printf("[elev] set stored return from entry -> from_vmap='%s' xy=(%d,%d)\n",
                       gPendingFromMapName, lx, ly);
            }
        } else if (dest_name && !strcmp(dest_name, "SilphCoElevator") &&
                   gPendingFromMapName[0] && !from_is_elevator) {

            int lx, ly;
            if (silph_elevator_landing(gPendingFromMapName, &lx, &ly)) {
                Warp_SetVmapElevatorDestination(gPendingFromMapName, lx, ly);
                printf("[elev] silph set stored return from entry -> from_vmap='%s' xy=(%d,%d)\n",
                       gPendingFromMapName, lx, ly);
            }
        }
    }

    Map_ApplyDarknessForWarp(prev_was_dark, prev_pal_offset, (int)dest_map);
    Display_LoadMapPalette();

    Player_IgnoreInputFrames(8);

    const map_events_t *dev = &gMapEvents[dest_map];
    int vspot_x, vspot_y;
    if (gPendingExactPos) {
        int max_x = (int)wCurMapWidth  * 2 - 1;
        int max_y = (int)wCurMapHeight * 2 - 1;
        int nx = gPendingX;
        int ny = gPendingY;
        if (nx > max_x) nx = max_x;
        if (ny > max_y) ny = max_y;
        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        Player_SetPos((int16_t)nx, (int16_t)ny);
        gPendingExactPos = 0;
    } else if (dest_map >= PKS_VIRTUAL_MAP_FIRST && dest_map <= PKS_VIRTUAL_MAP_LAST &&
               AmberScript_MapBank_GetWarpSpotForRealId(dest_map, dest_idx, &vspot_x, &vspot_y)) {

        int max_x = (int)wCurMapWidth  * 2 - 1;
        int max_y = (int)wCurMapHeight * 2 - 1;
        int nx = vspot_x;
        int ny = vspot_y;
        if (nx > max_x) nx = max_x;
        if (ny > max_y) ny = max_y;
        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        Player_SetPos((int16_t)nx, (int16_t)ny);
    } else if (dev->warps && dest_idx < dev->num_warps) {
        const map_warp_t *dw = &dev->warps[dest_idx];
        int max_x = (int)wCurMapWidth  * 2 - 1;
        int max_y = (int)wCurMapHeight * 2 - 1;
        int nx = (int)dw->x;
        int ny = (int)dw->y;
        if (nx > max_x) nx = max_x;
        if (ny > max_y) ny = max_y;
        Player_SetPos((int16_t)nx, (int16_t)ny);
    } else if (gPendingDestMapName[0] &&
               AmberScript_MapBank_GetWarpSpotForName(gPendingDestMapName, dest_idx,
                                                      &vspot_x, &vspot_y)) {

        int max_x = (int)wCurMapWidth  * 2 - 1;
        int max_y = (int)wCurMapHeight * 2 - 1;
        int nx = vspot_x, ny = vspot_y;
        if (nx > max_x) nx = max_x;
        if (ny > max_y) ny = max_y;
        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        Player_SetPos((int16_t)nx, (int16_t)ny);
    } else {

        Player_SetPos(
            (int16_t)(wCurMapWidth),
            (int16_t)(wCurMapHeight)
        );
    }

    if (was_teleport_pad)
        AmberScript_SceneTriggerSuppressAt((int)dest_map, (int)wXCoord, (int)wYCoord);

    if (gPendingElevatorLanding ||
        AmberScript_MapBank_IsWarpStairAt(dest_map, (int)wXCoord, (int)wYCoord)) {

        gWarpDoorStep = 1;
        if (gPendingElevatorLanding) {
            const char *ln = AmberScript_MapBank_NameForRealId(dest_map);
            gWarpDoorStepFaceUp = ln &&
                (!strcmp(ln, "RocketHideoutB1F") ||
                 !strcmp(ln, "RocketHideoutB2F") ||
                 !strcmp(ln, "RocketHideoutB4F"));
        }
    } else if (AmberScript_MapBank_IsIndoorForRealId(dest_map) ||
        AmberScript_MapBank_GetNoDoorStepForRealId(dest_map) ||
        AmberScript_MapBank_IsWarpWalkIntoAt(dest_map, (int)wXCoord, (int)wYCoord)) {

        gWarpDoorStep = 0;
        gWarpDoorStepFaceUp = 0;
    } else {
        uint8_t ov_has_warp = 0;
        int is_amberscript_warp_cell =
            get_warp_override_at((int)wXCoord, (int)wYCoord, &ov_has_warp, NULL, NULL) && ov_has_warp;
        gWarpDoorStep = is_amberscript_warp_cell ||
                        is_door_tile(Map_GetGameTile((int)wXCoord, (int)wYCoord));
        gWarpDoorStepFaceUp = 0;
    }
    gPendingElevatorLanding = 0;

    NPC_Load();
    fire_map_onload_callbacks();
    printf("[warp] executed -> map %d @ (%d,%d)\n",
           dest_map, wXCoord, wYCoord);
}

void Warp_ForceTeleport(uint8_t map_id, int tile_x, int tile_y) {

    extern int Game_WarpToRealMap(uint8_t real_id, int x, int y);
    if (!Game_WarpToRealMap(map_id, tile_x, tile_y)) {
        printf("[warp] teleport: no vmap for map %d -- direct load fallback\n",
               map_id);
        Map_Load(map_id);
        {
            int fx = tile_x, fy = tile_y;
            int mx = (int)wCurMapWidth * 2 - 1, my = (int)wCurMapHeight * 2 - 1;
            if (fx < 0) fx = (int)wCurMapWidth;
            if (fy < 0) fy = (int)wCurMapHeight;
            if (fx > mx) fx = mx;
            if (fy > my) fy = my;
            Player_SetPos((uint16_t)fx, (uint16_t)fy);
        }
        NPC_Load();
        fire_map_onload_callbacks();
    }

    gMapPalOffset = (map_id == 0x52  ||
                     map_id == 0xE8  ||
                     AmberScript_MapBank_IsDarkForRealId(map_id)) ? 6 : 0;
    Display_LoadMapPalette();

    Player_IgnoreInputFrames(8);
    printf("[warp] teleport -> map %d @ (%d,%d) [%s]\n", map_id,
           (int)wXCoord, (int)wYCoord,
           wCurMap >= PKS_VIRTUAL_MAP_FIRST ? "vmap" : "direct");
}

void Warp_QueueTeleport(uint8_t map_id, int tile_x, int tile_y) {
    gPendingDestMap  = map_id;
    gPendingDestMapName[0] = '\0';
    gPendingDestIdx  = 0;
    gPendingExactPos = 1;
    gPendingX        = tile_x;
    gPendingY        = tile_y;
    gPendingFromMap  = wCurMap;
    gPendingFromWarp = 0;
    gPendingDungeonHole = 0;
    gPendingTeleportPad = 0;
    gWarpPending      = 1;
    gWarpJustHappened = 1;
    printf("[warp] queued teleport -> map %d @ (%d,%d)\n", map_id, tile_x, tile_y);
}

void Warp_PlayMapChangeSound(void) {

    uint8_t t = Map_GetGameTile((int)wXCoord, (int)wYCoord);
    if (t == 0x0B) Audio_PlaySFX_GoInside();
    else           Audio_PlaySFX_GoOutside();
}

void Warp_QueueTeleportVmap(const char *vmap_name, int tile_x, int tile_y) {
    gPendingDestMap  = (uint8_t)PKS_VIRTUAL_MAP_FIRST;
    snprintf(gPendingDestMapName, sizeof(gPendingDestMapName), "%s", vmap_name ? vmap_name : "");
    gPendingDestIdx  = 0;
    gPendingExactPos = 1;
    gPendingX        = tile_x;
    gPendingY        = tile_y;
    gPendingFromMap  = wCurMap;
    gPendingFromWarp = 0;
    gPendingDungeonHole = 0;
    gPendingTeleportPad = 0;
    gWarpPending      = 1;
    gWarpJustHappened = 1;
    printf("[warp] queued teleport -> vmap '%s' @ (%d,%d)\n", gPendingDestMapName, tile_x, tile_y);
}
