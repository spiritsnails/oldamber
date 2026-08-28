#include "town_map.h"

#include "overworld.h"
#include "fly_anim.h"
#include "npc.h"
#include "player.h"
#include "../platform/display.h"
#include "../platform/audio.h"
#include "../data/player_sprite.h"
#include "../data/sprite_data.h"
#include "../data/town_map_data.h"
#include "../data/font_data.h"
#include "../platform/hardware.h"
#include <string.h>

static const char *kTownMapOrderNames[] = {
    "PALLET TOWN", "ROUTE 1", "VIRIDIAN CITY", "ROUTE 2", "VIRIDIAN FOREST",
    "DIGLETTS CAVE", "PEWTER CITY", "ROUTE 3", "MT.MOON", "ROUTE 4",
    "CERULEAN CITY", "ROUTE 24", "ROUTE 25", "SEA COTTAGE", "ROUTE 5",
    "ROUTE 6", "VERMILION CITY", "S.S.ANNE", "ROUTE 9", "ROCK TUNNEL",
    "ROUTE 10", "LAVENDER TOWN", "POKEMON TOWER", "ROUTE 8", "ROUTE 7",
    "CELADON CITY", "SAFFRON CITY", "ROUTE 11", "ROUTE 12", "ROUTE 13",
    "ROUTE 14", "ROUTE 15", "ROUTE 16", "ROUTE 17", "ROUTE 18",
    "FUCHSIA CITY", "SAFARI ZONE", "ROUTE 19", "SEAFOAM ISLANDS",
    "ROUTE 20", "CINNABAR ISLAND", "ROUTE 21", "ROUTE 22", "ROUTE 23",
    "VICTORY ROAD", "INDIGO PLATEAU", "POWER PLANT",
};
#define TM_NAME_COUNT ((int)(sizeof(kTownMapOrderNames) / sizeof(kTownMapOrderNames[0])))

enum {
    TM_PALLET = 0,
    TM_VIRIDIAN,
    TM_PEWTER,
    TM_CERULEAN,
    TM_LAVENDER,
    TM_VERMILION,
    TM_CELADON,
    TM_FUCHSIA,
    TM_CINNABAR,
    TM_INDIGO,
    TM_SAFFRON,
    TM_NUM_CITIES
};

static const uint8_t kFlyCityMapIds[TM_NUM_CITIES] = {
    0x00, 0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
};

static const char *kFlyCityNames[TM_NUM_CITIES] = {
    "PALLET TOWN", "VIRIDIAN CITY", "PEWTER CITY", "CERULEAN CITY",
    "LAVENDER TOWN", "VERMILION CITY", "CELADON CITY", "FUCHSIA CITY",
    "CINNABAR ISLAND", "INDIGO PLATEAU", "SAFFRON CITY",
};

#define TM_FIRST_ROUTE_MAP 0x0C

#define TM_NORMAL_CURSOR_OAM_BASE  4
#define TM_FLY_CURSOR_OAM_BASE     32
#define TM_PLAYER_OAM_BASE         36
#define TM_OAM_COUNT               40

#define TM_PLAYER_TILE_BASE  72
#define TM_CURSOR_TILE_BASE  76

#define BLINK_HALF_PERIOD 25

static int gTownMapOpen = 0;
static int gTownMapIdx = 0;
static int gTownMapFlyMode = 0;

#define TM_FADE_FRAMES 14
static int gTownMapOpenFade  = 0;
static int gTownMapCloseFade = 0;
static uint8_t gTownVisitedFlag[2] = {0};
static uint8_t gFlyLocationsList[TM_NUM_CITIES + 2] = {0};
static oam_entry_t sTownMapOAMBackup[TM_OAM_COUNT] = {0};
static uint8_t sTownMapAnimCounter = 0;

static void tm_close(void);

void TownMap_GetVisited(uint8_t out[2]) {
    out[0] = gTownVisitedFlag[0];
    out[1] = gTownVisitedFlag[1];
}

void TownMap_SetVisited(const uint8_t in[2]) {
    gTownVisitedFlag[0] = in[0];
    gTownVisitedFlag[1] = in[1];
}

void TownMap_MarkVisited(uint8_t map_id) {
    if (map_id >= TM_FIRST_ROUTE_MAP)
        return;
    gTownVisitedFlag[map_id >> 3] |= (uint8_t)(1u << (map_id & 7));
}

static int tm_get_fly_destination(uint8_t map_id, int *x, int *y) {
    switch (map_id) {
    case 0x00: *x =  5; *y =  6; return 1;
    case 0x01: *x = 23; *y = 26; return 1;
    case 0x02: *x = 13; *y = 26; return 1;
    case 0x03: *x = 19; *y = 18; return 1;
    case 0x04: *x =  3; *y =  6; return 1;
    case 0x05: *x = 11; *y =  4; return 1;
    case 0x06: *x = 41; *y = 10; return 1;
    case 0x07: *x = 19; *y = 28; return 1;
    case 0x08: *x = 11; *y = 12; return 1;
    case 0x09: *x =  9; *y =  6; return 1;
    case 0x0A: *x =  9; *y = 30; return 1;
    default: return 0;
    }
}

int TownMap_GetFlyDest(uint8_t map_id, int *x, int *y) {
    int lx = 0, ly = 0;
    int ok = tm_get_fly_destination(map_id, &lx, &ly);
    if (ok) { *x = lx; *y = ly; }
    return ok;
}

static int tm_order_count(void) {
    return (gTownMapOrderCount < TM_NAME_COUNT) ? gTownMapOrderCount : TM_NAME_COUNT;
}

static void tm_set(int col, int row, uint8_t tile) {
    if (col < 0 || col >= SCREEN_WIDTH || row < 0 || row >= SCREEN_HEIGHT) return;
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
}

static void tm_clear(void) {
    for (int r = 0; r < SCREEN_HEIGHT; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            tm_set(c, r, BLANK_TILE_SLOT);
}

static void tm_clear_row(int row) {
    for (int c = 0; c < SCREEN_WIDTH; c++)
        tm_set(c, row, BLANK_TILE_SLOT);
}

static uint8_t tm_char(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return (uint8_t)Font_CharToTile((uint8_t)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return (uint8_t)Font_CharToTile((uint8_t)(0xA0 + (c - 'a')));
    if (c >= '0' && c <= '9') return (uint8_t)Font_CharToTile((uint8_t)(0xF6 + (c - '0')));
    if (c == ' ') return (uint8_t)Font_CharToTile(0x7F);
    if (c == '.') return (uint8_t)Font_CharToTile(0xE8);
    if (c == '-') return (uint8_t)Font_CharToTile(0xE3);
    return (uint8_t)Font_CharToTile(0x7F);
}

static void tm_puts(int col, int row, const char *s) {
    while (*s && col < SCREEN_WIDTH) {
        tm_set(col++, row, tm_char((unsigned char)*s));
        s++;
    }
}

static void tm_draw_world_map(void) {
    int out = 0;
    for (int i = 0; gTownMapCompressedMap[i] != 0 && out < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        uint8_t packed = gTownMapCompressedMap[i];
        int run = packed & 0x0F;
        uint8_t tile = (uint8_t)(TOWN_MAP_WORLD_TILE_BASE + ((packed >> 4) & 0x0F));
        while (run-- > 0 && out < SCREEN_WIDTH * SCREEN_HEIGHT) {
            int row = out / SCREEN_WIDTH;
            int col = out % SCREEN_WIDTH;
            tm_set(col, row, tile);
            out++;
        }
    }
}

static void tm_load_selector_gfx(void) {
    const uint8_t *player_tiles = gPlayerGfx[0][0];
    const uint8_t *cursor_tiles = gTownMapCursorTiles[0];

    for (int i = 0; i < 4; i++)
        Display_LoadSpriteTile((uint8_t)(TM_PLAYER_TILE_BASE + i),
                               player_tiles + (i * 16));
    for (int i = 0; i < 4; i++)
        Display_LoadSpriteTile((uint8_t)(TM_CURSOR_TILE_BASE + i),
                               cursor_tiles + (i * 16));
}

static void tm_set_sprite_oam(int base, int tile_base, int map_x, int map_y) {

    int oy = 24 + (map_y * 8) - 3;
    int ox = 24 + (map_x * 8) - 4;

    wShadowOAM[base + 0].y = (uint8_t)oy;
    wShadowOAM[base + 0].x = (uint8_t)ox;
    wShadowOAM[base + 0].tile = (uint8_t)(tile_base + 0);
    wShadowOAM[base + 0].flags = 0;

    wShadowOAM[base + 1].y = (uint8_t)oy;
    wShadowOAM[base + 1].x = (uint8_t)(ox + 8);
    wShadowOAM[base + 1].tile = (uint8_t)(tile_base + 1);
    wShadowOAM[base + 1].flags = 0;

    wShadowOAM[base + 2].y = (uint8_t)(oy + 8);
    wShadowOAM[base + 2].x = (uint8_t)ox;
    wShadowOAM[base + 2].tile = (uint8_t)(tile_base + 2);
    wShadowOAM[base + 2].flags = 0;

    wShadowOAM[base + 3].y = (uint8_t)(oy + 8);
    wShadowOAM[base + 3].x = (uint8_t)(ox + 8);
    wShadowOAM[base + 3].tile = (uint8_t)(tile_base + 3);
    wShadowOAM[base + 3].flags = 0;
}

static uint8_t tm_get_map_coords_packed(uint8_t map_id) {
    if (map_id < TOWN_MAP_FIRST_INDOOR_MAP) {
        return gTownMapExternalCoords[map_id];
    }
    for (int i = 0; i < gTownMapInternalCoordsCount; i++) {
        if (map_id < gTownMapInternalCoords[i].group_end) {
            return gTownMapInternalCoords[i].coords;
        }
    }
    return gTownMapExternalCoords[0];
}

static void tm_get_map_coords(uint8_t map_id, int *x, int *y) {
    uint8_t packed = tm_get_map_coords_packed(map_id);
    *x = packed & 0x0F;
    *y = (packed >> 4) & 0x0F;
}

static void tm_build_fly_locations_list(void) {
    uint16_t visited = (uint16_t)gTownVisitedFlag[0] | ((uint16_t)gTownVisitedFlag[1] << 8);
    for (int i = 0; i < TM_NUM_CITIES; i++) {
        gFlyLocationsList[i] = (visited & 1u) ? kFlyCityMapIds[i] : 0xFE;
        visited >>= 1;
    }
    gFlyLocationsList[TM_NUM_CITIES] = 0xFF;
    gFlyLocationsList[TM_NUM_CITIES + 1] = 0x00;
}

static int tm_fly_has_any_visited(void) {
    for (int i = 0; i < TM_NUM_CITIES; i++) {
        if (gFlyLocationsList[i] != 0xFE) return 1;
    }
    return 0;
}

static void tm_copy_oam_to_backup(void) {
    memcpy(sTownMapOAMBackup, wShadowOAM, sizeof(sTownMapOAMBackup));
}

static void tm_restore_oam_from_backup(void) {
    memcpy(&wShadowOAM[0], &sTownMapOAMBackup[0],
           (TM_OAM_COUNT - 4) * sizeof(wShadowOAM[0]));
}

static void tm_hide_oam(void) {
    for (int i = 0; i < TM_OAM_COUNT - 4; i++)
        wShadowOAM[i].y = (uint8_t)(SCREEN_HEIGHT_PX + OAM_Y_OFS);
}

static void tm_town_map_sprite_blinking_animation(void) {
    sTownMapAnimCounter++;
    if (sTownMapAnimCounter == 25) {
        tm_hide_oam();
        return;
    }
    if (sTownMapAnimCounter == 50) {
        tm_restore_oam_from_backup();
        sTownMapAnimCounter = 0;
    }
}

static void tm_render(void) {
    int map_x, map_y;
    int cur_x, cur_y;
    int count = tm_order_count();
    if (count <= 0) return;
    if (gTownMapIdx >= count) gTownMapIdx = 0;
    if (gTownMapIdx < 0) gTownMapIdx = count - 1;

    if (gTownMapFlyMode) {
        if (!tm_fly_has_any_visited()) {
            tm_close();
            return;
        }
        if (gTownMapIdx >= TM_NUM_CITIES) gTownMapIdx = 0;
        if (gTownMapIdx < 0) gTownMapIdx = TM_NUM_CITIES - 1;
        uint8_t map_id = gFlyLocationsList[gTownMapIdx];
        tm_get_map_coords(map_id, &map_x, &map_y);
        tm_get_map_coords(Map_CurrentRealId(), &cur_x, &cur_y);
        tm_clear();
        tm_draw_world_map();
        tm_clear_row(0);
        tm_puts(1, 0, kFlyCityNames[gTownMapIdx < TM_NUM_CITIES ? gTownMapIdx : 0]);
        tm_set_sprite_oam(TM_PLAYER_OAM_BASE, TM_PLAYER_TILE_BASE, cur_x, cur_y);
        tm_set_sprite_oam(TM_FLY_CURSOR_OAM_BASE, TM_CURSOR_TILE_BASE, map_x, map_y);
        tm_copy_oam_to_backup();
        return;
    }

    tm_get_map_coords(gTownMapOrderMapIds[gTownMapIdx], &map_x, &map_y);

    tm_get_map_coords(Map_CurrentRealId(), &cur_x, &cur_y);
    tm_clear();
    tm_draw_world_map();
    tm_clear_row(0);
    tm_puts(1, 0, kTownMapOrderNames[gTownMapIdx]);

    tm_set_sprite_oam(TM_PLAYER_OAM_BASE, TM_PLAYER_TILE_BASE, cur_x, cur_y);
    tm_set_sprite_oam(TM_NORMAL_CURSOR_OAM_BASE, TM_CURSOR_TILE_BASE, map_x, map_y);
    tm_copy_oam_to_backup();
}

static void tm_close(void) {
    gTownMapOpen = 0;
    gTownMapFlyMode = 0;
    memset(wShadowOAM, 0, sizeof(wShadowOAM));
    hWY = SCREEN_HEIGHT_PX;
    Display_LoadMapPalette();
    Map_ReloadGfx();
    NPC_ReloadTiles();
    Player_SyncOAM();
    Font_Load();
    Map_BuildScrollView();
    NPC_BuildView(gScrollPxX, gScrollPxY);
}

void TownMap_Open(void) {
    gTownMapFlyMode = 0;
    gTownMapOpen = 1;
    gTownMapIdx = 0;
    sTownMapAnimCounter = 0;
    TownMapData_LoadTiles();
    memset(wShadowOAM, 0, sizeof(wShadowOAM));
    tm_load_selector_gfx();
    hWY = SCREEN_HEIGHT_PX;
    tm_render();

    Display_SetPalette(0x00, 0x00, 0x00);
    gTownMapOpenFade  = TM_FADE_FRAMES;
    gTownMapCloseFade = 0;
}

void TownMap_OpenFly(void) {
    gTownMapFlyMode = 1;
    gTownMapOpen = 1;
    gTownMapIdx = 0;
    sTownMapAnimCounter = 0;
    TownMapData_LoadTiles();
    memset(wShadowOAM, 0, sizeof(wShadowOAM));
    tm_load_selector_gfx();
    tm_build_fly_locations_list();
    for (int i = 0; i < 4; i++)
        Display_LoadSpriteTile((uint8_t)(TM_CURSOR_TILE_BASE + i),
                               gSpriteGfx[0x09] + (i * 16));
    hWY = SCREEN_HEIGHT_PX;
    tm_render();
}

int TownMap_IsOpen(void) {
    return gTownMapOpen;
}

void TownMap_Tick(void) {
    if (!gTownMapOpen) return;

    if (gTownMapOpenFade > 0) {
        if (--gTownMapOpenFade == 0) Display_SetPalette(0xE4, 0xD0, 0xE0);
        return;
    }
    if (gTownMapCloseFade > 0) {
        if (--gTownMapCloseFade == 0) tm_close();
        return;
    }
    int count = tm_order_count();
    if (count <= 0) return;
    if (gTownMapFlyMode && !tm_fly_has_any_visited()) {
        tm_close();
        return;
    }

    if (!gTownMapFlyMode)
        tm_town_map_sprite_blinking_animation();

    if (hJoyPressed & (PAD_A | PAD_B | PAD_UP | PAD_DOWN)) {
        if (gTownMapFlyMode && (hJoyPressed & PAD_A))
            Audio_PlaySFX_HealAilment();
        else
            Audio_PlaySFX_Tink();
    }

    if (hJoyPressed & PAD_UP) {
        if (gTownMapFlyMode) {
            int idx = gTownMapIdx;
            int guard = 0;
            do {
                idx++;
                if (idx >= TM_NUM_CITIES) idx = 0;
                guard++;
            } while (gFlyLocationsList[idx] == 0xFE && guard <= TM_NUM_CITIES);
            if (guard > TM_NUM_CITIES) {
                tm_close();
                return;
            }
            gTownMapIdx = idx;
        } else {
            gTownMapIdx++;
            if (gTownMapIdx >= count) gTownMapIdx = 0;
        }
        tm_render();
        return;
    }
    if (hJoyPressed & PAD_DOWN) {
        if (gTownMapFlyMode) {
            int idx = gTownMapIdx;
            int guard = 0;
            do {
                idx--;
                if (idx < 0) idx = TM_NUM_CITIES - 1;
                guard++;
            } while (gFlyLocationsList[idx] == 0xFE && guard <= TM_NUM_CITIES);
            if (guard > TM_NUM_CITIES) {
                tm_close();
                return;
            }
            gTownMapIdx = idx;
        } else {
            gTownMapIdx--;
            if (gTownMapIdx < 0) gTownMapIdx = count - 1;
        }
        tm_render();
        return;
    }
    if (hJoyPressed & (PAD_A | PAD_B)) {
        if (gTownMapFlyMode && (hJoyPressed & PAD_A)) {
            uint8_t map_id = gFlyLocationsList[gTownMapIdx];
            int tx, ty;
            if (tm_get_fly_destination(map_id, &tx, &ty)) {
                tm_close();
                FlyAnim_Start(map_id, tx, ty);
                return;
            }
        }
        if (!gTownMapFlyMode) {

            Display_SetPalette(0x00, 0x00, 0x00);
            gTownMapCloseFade = TM_FADE_FRAMES;
            return;
        }
        tm_close();
        return;
    }
}
