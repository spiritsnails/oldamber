
#include "gbc_color.h"
#include "constants.h"
#include "amberscript_core.h"
#include "amberscript_mapbank.h"
#include "data/gbc_palettes.h"
#include "crystal_color.h"
#include "../data/map_data.h"
#include "../platform/display.h"
#include "../platform/game_version.h"
#include "../platform/hardware.h"
#include <string.h>

static const uint16_t *auto_bg(void);
static const uint16_t *auto_obj0(void);

#define MAP_SAFFRON_CITY         0x0A
#define MAP_ROUTE_6              0x11
#define MAP_BILLS_HOUSE          0x58
#define MAP_CELADON_MART_1F      0x7A
#define MAP_CELADON_MART_ROOF    0x7E
#define MAP_INDIGO_PLATEAU_LOBBY 0xAE

#define RED_NUM_CITY_MAPS        11
#define RED_FIRST_INDOOR_MAP     0x25
#define MAP_CERULEAN_CAVE_2F     0xE2
#define MAP_CERULEAN_CAVE_1F     0xE4
#define MAP_LORELEIS_ROOM        0xF5
#define MAP_BRUNOS_ROOM          0xF6

static int s_enabled = 0;

void GbcColor_SetEnabled(int on) {
    s_enabled = on ? 1 : 0;
    if (!s_enabled) {
        Display_SetColorMode(0);

        Display_SetPositionAttrMode(0);
    }
}

int GbcColor_IsEnabled(void) { return s_enabled; }

void GbcColor_Disable(void) { Display_SetColorMode(0); }

static int s_sync_want = -2;
static int s_sync_map  = -2;

void GbcColor_MarkDirty(void) { s_sync_want = -2; s_sync_map = -2; }

static int gbc_logical_map_id(uint8_t real_id);

void GbcColor_Sync(int want_color, uint8_t map_id) {
    want_color = want_color ? 1 : 0;

    int key = gbc_logical_map_id(map_id);
    if (want_color == s_sync_want && key == s_sync_map) return;
    s_sync_want = want_color;
    s_sync_map  = key;
    if (want_color) GbcColor_ApplyForMap(map_id);
    else            GbcColor_Disable();
}

uint8_t GbcColor_AttrForTile(int tileset_id, int tile_index) {
    if ((unsigned)tileset_id >= GBC_NUM_TILESETS) return 0;
    if ((unsigned)tile_index >= GBC_TILESET_SIZE) return 0;
    return gGbcTilesetPalMap[tileset_id][tile_index];
}

int GbcColor_MapIdForName(const char *name) {
    if (!name || !*name) return -1;

    for (int i = 0; i < NUM_MAPS; i++)
        if (gMapTable[i].name && strcasecmp(gMapTable[i].name, name) == 0)
            return i;
    return -1;
}

uint8_t GbcColor_AttrForTileOnMap(int tileset_id, int tile_index, int map_id) {
    uint8_t attr = GbcColor_AttrForTile(tileset_id, tile_index);

    if (map_id == MAP_CELADON_MART_ROOF) {
        if (tile_index >= 0x4B && tile_index <= 0x4F) attr = PAL_BG_WATER;
    } else if (map_id == MAP_CELADON_MART_1F) {
        if (tile_index == 0x07 || tile_index == 0x08 ||
            tile_index == 0x17 || tile_index == 0x18) attr = PAL_BG_YELLOW;
    }
    return attr;
}

static void gbc_apply_roof(int map_id) {
    if (map_id < 0 || map_id >= GBC_NUM_ROOF_MAPS) return;

    if (map_id == MAP_ROUTE_6 && wYCoord < 2) map_id = MAP_SAFFRON_CITY;
    Display_SetBGColorEntry(PAL_BG_ROOF, 1, gGbcRoofPalettes[map_id][0]);
    Display_SetBGColorEntry(PAL_BG_ROOF, 2, gGbcRoofPalettes[map_id][1]);
}

void GbcColor_ApplyTileset(int tileset_id, int map_id, int owns_tile_slots) {
    if (!s_enabled) { Display_SetColorMode(0); return; }
    if ((unsigned)tileset_id >= GBC_NUM_TILESETS) { Display_SetColorMode(0); return; }

    Display_SetPositionAttrMode(0);

    for (int slot = 0; slot < 8; slot++) {
        uint8_t idx = gGbcMapPaletteSets[tileset_id][slot];
        Display_SetBGColorPalette(slot, gGbcMapPalettes[idx]);
    }

    Display_FillTileAttrs(GBC_TILESET_SIZE, 0x100 - GBC_TILESET_SIZE, PAL_BG_TEXT);

    Display_SetTileAttrs(0x00, gGbcTilesetPalMap[tileset_id], GBC_TILESET_SIZE);

    if (!owns_tile_slots) {

        if (tileset_id == TILESET_OVERWORLD || tileset_id == TILESET_PLATEAU)
            gbc_apply_roof(map_id);
        Display_SetColorMode(1);
        return;
    }

    if (map_id == MAP_CELADON_MART_ROOF) {

        for (int i = 0; i < 5; i++)
            Display_SetTileAttr((uint8_t)(0x4B + i), PAL_BG_WATER);
    } else if (map_id == MAP_CELADON_MART_1F) {

        Display_SetTileAttr(0x07, PAL_BG_YELLOW);
        Display_SetTileAttr(0x08, PAL_BG_YELLOW);
        Display_SetTileAttr(0x17, PAL_BG_YELLOW);
        Display_SetTileAttr(0x18, PAL_BG_YELLOW);
    }

    if (tileset_id == TILESET_OVERWORLD || tileset_id == TILESET_PLATEAU)
        gbc_apply_roof(map_id);

    Display_SetColorMode(1);
}

static int gbc_logical_map_id(uint8_t real_id) {
    const char *name;
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST)
        return real_id;
    if (!AmberScript_IsEnabled()) return -1;
    name = AmberScript_MapBank_NameForRealId(real_id);
    return GbcColor_MapIdForName(name);
}

static int s_overworld_style = GBC_OVERWORLD_RED_SGB;

static int s_battle_autocolor = 0;
static void gbc_apply_red_autocolor_all(void);

void GbcColor_ApplyAutoColorAll(void) {
    if (!s_enabled) return;
    gbc_apply_red_autocolor_all();
}

void GbcColor_AutoColorMonPicPal(int obj_slot) {
    uint8_t bgp;
    uint16_t objbg[4];
    if (!s_enabled || !s_battle_autocolor) return;
    bgp = Display_GetBGP();
    if (bgp == 0x00) bgp = 0xE4;
    for (int i = 0; i < 4; i++)
        objbg[i] = auto_bg()[(bgp >> (2 * i)) & 3];
    Display_SetOBJColorPalette(obj_slot, objbg);
}

void GbcColor_AutoColorObjPal(int obj_slot, uint8_t obp) {
    uint16_t objbg[4];
    if (!s_enabled) return;
    if (obp == 0x00) obp = 0xE4;
    for (int i = 0; i < 4; i++)
        objbg[i] = auto_bg()[(obp >> (2 * i)) & 3];
    Display_SetOBJColorPalette(obj_slot, objbg);
}

void GbcColor_ApplyOverworldSpritePal(int obj_slot) {
    uint16_t obj[4];
    if (!s_enabled) return;
    for (int i = 0; i < 4; i++)
        obj[i] = auto_obj0()[(0xD0 >> (2 * i)) & 3];
    Display_SetOBJColorPalette(obj_slot, obj);
}

void GbcColor_SetBattleAutoColor(int on) {
    on = on ? 1 : 0;
    if (on == s_battle_autocolor) return;
    s_battle_autocolor = on;
    GbcColor_MarkDirty();
}
int GbcColor_BattleAutoColor(void) { return s_battle_autocolor; }

void GbcColor_SetOverworldStyle(int style) {

    int new_style = (style == GBC_OVERWORLD_RED_SGB || style == GBC_OVERWORLD_RED_AUTOCOLOR)
                   ? style : GBC_OVERWORLD_RED_SGB;
    if (new_style == s_overworld_style) return;
    s_overworld_style = new_style;
    GbcColor_MarkDirty();

    if (new_style != GBC_OVERWORLD_DEFAULT)
        Display_SetColorCurve(GBC_CURVE_SAMEBOY_HW);
}

int GbcColor_OverworldStyle(void) { return s_overworld_style; }

static int red_sgb_town_or_route(uint8_t map) {
    return (map < RED_NUM_CITY_MAPS) ? (RSGB_PAL_PALLET + map) : RSGB_PAL_ROUTE;
}

static int red_sgb_palette_for_map(uint8_t map_id, int tileset_id) {
    if (tileset_id == TILESET_CEMETERY) return RSGB_PAL_GRAYMON;
    if (tileset_id == TILESET_CAVERN) return RSGB_PAL_CAVE;
    if (map_id < RED_FIRST_INDOOR_MAP) return red_sgb_town_or_route(map_id);
    if (map_id < MAP_CERULEAN_CAVE_2F) return red_sgb_town_or_route(wLastMap);
    if (map_id <= MAP_CERULEAN_CAVE_1F) return RSGB_PAL_CAVE;
    if (map_id == MAP_LORELEIS_ROOM) return RSGB_PAL_PALLET;
    if (map_id == MAP_BRUNOS_ROOM) return RSGB_PAL_CAVE;
    return red_sgb_town_or_route(wLastMap);
}

static void gbc_apply_red_sgb_overworld(uint8_t map_id, int tileset_id) {
    int pal = red_sgb_palette_for_map(map_id, tileset_id);
    Display_SetPositionAttrMode(0);
    Display_FillTileAttrs(0, 0x100, 0);

    for (int slot = 0; slot < 8; slot++)
        Display_SetBGColorPalette(slot, gGbcRedSgbPalettes[pal]);

    {
        uint8_t obp0 = Display_GetOBP0();
        uint16_t obj[4];
        if (obp0 == 0x00) obp0 = 0xD0;
        for (int i = 0; i < 4; i++)
            obj[i] = gGbcRedSgbPalettes[pal][(obp0 >> (2 * i)) & 3];
        for (int i = 0; i < GBC_NUM_SPRITE_PALETTES; i++)
            Display_SetOBJColorPalette(i, obj);
    }
    Display_SetColorMode(1);
}

static void gbc_apply_red_autocolor_all(void) {
    Display_SetPositionAttrMode(0);
    Display_FillTileAttrs(0, 0x100, 0);

    for (int slot = 0; slot < 8; slot++)
        Display_SetBGColorPalette(slot, auto_bg());

    {
        uint8_t obp0 = Display_GetOBP0();
        uint16_t obj0[4];

        if (obp0 == 0x00) obp0 = 0xD0;

        uint8_t obp1 = Display_GetOBP1();
        if (obp1 == 0x00) obp1 = 0xE0;
        for (int i = 0; i < 4; i++)
            obj0[i] = auto_obj0()[(obp0 >> (2 * i)) & 3];

        uint16_t obj1[4];
        for (int i = 0; i < 4; i++)
            obj1[i] = auto_bg()[(obp1 >> (2 * i)) & 3];
        Display_SetOBJColorPalette(0, obj0);
        Display_SetOBJColorPalette(1, obj1);

        for (int i = 2; i < GBC_OBJ_PAL_COUNT; i++)
            Display_SetOBJColorPalette(i, obj0);

        {
            uint8_t bgp = Display_GetBGP();
            uint16_t objbg[4];
            if (bgp == 0x00) bgp = 0xE4;
            for (int i = 0; i < 4; i++)
                objbg[i] = auto_bg()[(bgp >> (2 * i)) & 3];
            Display_SetOBJColorPalette(GBC_OBJ_PAL_PLAYER_MON, objbg);
            Display_SetOBJColorPalette(GBC_OBJ_PAL_ENEMY_MON,  objbg);
        }
    }
    Display_SetColorMode(1);
}

void GbcColor_ApplyForMap(uint8_t map_id) {
    int tileset;
    int is_vmap = (map_id >= PKS_VIRTUAL_MAP_FIRST && map_id <= PKS_VIRTUAL_MAP_LAST);
    int logical = gbc_logical_map_id(map_id);
    if (!s_enabled) { Display_SetColorMode(0); return; }

    if (is_vmap) {

        int group = 0;
        int env = AmberScript_MapBank_GetCrystalEnvForRealId(map_id, &group);
        if (env >= 0) {
            if (!CrystalColor_ApplyForEnv(env, group)) Display_SetColorMode(0);
            return;
        }
        tileset = AmberScript_MapBank_GetGbcTilesetForRealId(map_id);
    } else {

        tileset = (map_id < NUM_MAPS) ? gMapTable[map_id].tileset_id : -1;
    }

    if (tileset < 0) {

        Display_SetColorMode(0);
        return;
    }

    if (s_overworld_style == GBC_OVERWORLD_RED_AUTOCOLOR) {

        gbc_apply_red_autocolor_all();
        return;
    }
    if (s_overworld_style == GBC_OVERWORLD_RED_SGB && logical >= 0) {
        gbc_apply_red_sgb_overworld((uint8_t)logical, tileset);
        return;
    }

    GbcColor_ApplyTileset(tileset, logical, !is_vmap);
    GbcColor_ApplySpritePalettes(tileset, logical);
}

void GbcColor_ApplySpritePalettes(int tileset_id, int map_id) {
    const uint16_t (*pals)[4] = gGbcSpritePalettes;
    if (!s_enabled) return;
    if (tileset_id == TILESET_CAVERN) {
        pals = gGbcSpritePalettesNite;
    } else if (tileset_id == TILESET_POKECENTER || map_id == MAP_INDIGO_PLATEAU_LOBBY) {
        pals = gGbcSpritePalettesPokecenter;
    }
    for (int i = 0; i < GBC_NUM_SPRITE_PALETTES; i++)
        Display_SetOBJColorPalette(i, pals[i]);
}

#define PAL_EXP        0x0F
#define PAL_MEWMON     0x10
#define PAL_GREENBAR   0x1F
#define PAL_HERO       0xEB

#define PAL_BLACK      0x1E

uint8_t GbcColor_MonPalette(int dex) {
    if ((unsigned)dex >= GBC_NUM_MON_PALETTES) return PAL_MEWMON;
    return gGbcMonPalette[dex];
}

static uint8_t gbc_back_sprite_palette(int dex) {
    if (dex <= 0) return PAL_HERO;
    return GbcColor_MonPalette(dex);
}

static uint8_t gbc_front_sprite_palette(int dex, int trainer_class) {
    if (dex > 0) return GbcColor_MonPalette(dex);
    if ((unsigned)trainer_class < GBC_NUM_TRAINER_PALETTES)
        return gGbcTrainerPalette[trainer_class];
    return PAL_HERO;
}

static const uint16_t *s_pal_override[2];

void GbcColor_SetBattleMonPalOverride(const uint16_t *player4,
                                      const uint16_t *enemy4) {
    s_pal_override[0] = player4;
    s_pal_override[1] = enemy4;
}

static const uint16_t (*s_battle_super)[4];
static int s_battle_super_count;

void GbcColor_SetBattleSuperPalettes(const uint16_t (*table)[4], int count) {
    s_battle_super = (table && count > 0) ? table : 0;
    s_battle_super_count = (table && count > 0) ? count : 0;
}

static const uint16_t *auto_bg(void) {
    return (strcmp(GameVersion_Current(), "blue") == 0)
         ? gGbcBlueAutoColorBg : gGbcRedAutoColorBg;
}
static const uint16_t *auto_obj0(void) {
    return (strcmp(GameVersion_Current(), "blue") == 0)
         ? gGbcBlueAutoColorObj0 : gGbcRedAutoColorObj0;
}

static const uint16_t *battle_super(int pal_id) {
    if (s_battle_super) {

        if ((unsigned)pal_id >= (unsigned)s_battle_super_count) pal_id = PAL_MEWMON;
        return s_battle_super[pal_id];
    }

    if (strcmp(GameVersion_Current(), "red") != 0 &&
        gGbcRedSgbPalettes && (unsigned)pal_id < gGbcRedSgbPalettes_count)
        return gGbcRedSgbPalettes[pal_id];
    if ((unsigned)pal_id >= GBC_NUM_SUPER_PALETTES) pal_id = PAL_MEWMON;
    return gGbcSuperPalettes[pal_id];
}

const uint16_t *GbcColor_SuperPalette(int pal_id) {
    return battle_super(pal_id);
}

static int s_monpal_style = GBC_MONPAL_SGB;

void GbcColor_SetMonPalStyle(int style) {
    (void)style;
    s_monpal_style = GBC_MONPAL_SGB;
}
int GbcColor_MonPalStyleGet(void) { return s_monpal_style; }

static int rsgb_mon_pal_index(int dex) {
    int id = gGbcYellowMonPaletteId[dex];
    if (id < 0 || id >= GBC_NUM_RED_SGB_PALS) id = RSGB_PAL_MEWMON;
    return id;
}

const uint16_t *GbcColor_MonPaletteRGB(int dex) {
    if (dex > 0 && (unsigned)dex < GBC_NUM_MON_PALETTES) {
        if (s_monpal_style == GBC_MONPAL_SGB)
            return gGbcRedSgbPalettes[rsgb_mon_pal_index(dex)];
    }
    return battle_super(GbcColor_MonPalette(dex));
}

static void gbc_load_super(int slot, int pal_id) {
    Display_SetBGColorPalette(slot, battle_super(pal_id));
}

static void gbc_set_pal_whole_screen(int pal_id) {
    if (!s_enabled) { Display_SetColorMode(0); return; }

    if (s_battle_autocolor) {
        gbc_apply_red_autocolor_all();
        return;
    }

    for (int i = 0; i < 5; i++) gbc_load_super(i, pal_id);
    Display_SetOBJColorPalette(0, battle_super(pal_id));
    Display_ClearAttrBoxes(0);
    Display_SetColorMode(1);
}

void GbcColor_SetPalTradeGeneric(void) {
    gbc_set_pal_whole_screen(PAL_MEWMON);
}

void GbcColor_SetPalTradeMon(int dex) {
    gbc_set_pal_whole_screen(GbcColor_MonPalette(dex));
}

void GbcColor_SetPalBattle(int player_dex, int enemy_dex, int trainer_class,
                           int player_hp_color, int enemy_hp_color) {
    if (!s_enabled) { Display_SetColorMode(0); return; }

    if (s_battle_autocolor) {
        gbc_apply_red_autocolor_all();
        return;
    }
    if (player_hp_color < 0 || player_hp_color > 2) player_hp_color = 0;
    if (enemy_hp_color  < 0 || enemy_hp_color  > 2) enemy_hp_color  = 0;

    {
        int player_pal = gbc_back_sprite_palette(player_dex);
        int enemy_pal  = gbc_front_sprite_palette(enemy_dex, trainer_class);
        const uint16_t *player_rgb = s_pal_override[0]
                                   ? s_pal_override[0] : battle_super(player_pal);
        const uint16_t *enemy_rgb  = s_pal_override[1]
                                   ? s_pal_override[1] : battle_super(enemy_pal);

        Display_SetBGColorPalette(0, player_rgb);
        Display_SetBGColorPalette(1, enemy_rgb);
        gbc_load_super(2, PAL_GREENBAR + player_hp_color);
        gbc_load_super(3, PAL_GREENBAR + enemy_hp_color);
        gbc_load_super(4, PAL_EXP);

        Display_SetOBJColorPalette(GBC_OBJ_PAL_PLAYER_MON, player_rgb);
        Display_SetOBJColorPalette(GBC_OBJ_PAL_ENEMY_MON,  enemy_rgb);

        Display_SetOBJColorPalette(GBC_OBJ_PAL_POKEBALL,
                                   gGbcSpritePalettes[SPR_PAL_ORANGE]);
    }

    Display_ClearAttrBoxes(0);
    Display_FillAttrBox( 0,  0, 11, 4, 3);
    Display_FillAttrBox( 9,  7, 11, 4, 2);
    Display_FillAttrBox( 9, 11, 11, 1, 4);
    Display_FillAttrBox( 0,  4,  9, 8, 0);
    Display_FillAttrBox(11,  0,  9, 7, 1);
    Display_FillAttrBox( 0, 12, 20, 6, 0);

    Display_SetPositionAttrMode(1);
    Display_SetColorMode(1);
}

void GbcColor_SetPalBattleGen1(int player_dex, int enemy_dex, int trainer_class,
                               int player_hp_color, int enemy_hp_color) {
    if (!s_enabled) { Display_SetColorMode(0); return; }

    if (s_battle_autocolor) {
        gbc_apply_red_autocolor_all();
        return;
    }
    if (player_hp_color < 0 || player_hp_color > 2) player_hp_color = 0;
    if (enemy_hp_color  < 0 || enemy_hp_color  > 2) enemy_hp_color  = 0;

    {
        int player_pal = gbc_back_sprite_palette(player_dex);
        int enemy_pal  = gbc_front_sprite_palette(enemy_dex, trainer_class);
        const uint16_t *player_rgb = s_pal_override[0]
                                   ? s_pal_override[0] : battle_super(player_pal);
        const uint16_t *enemy_rgb  = s_pal_override[1]
                                   ? s_pal_override[1] : battle_super(enemy_pal);

        Display_SetBGColorPalette(0, player_rgb);
        Display_SetBGColorPalette(1, enemy_rgb);
        gbc_load_super(2, PAL_GREENBAR + player_hp_color);
        gbc_load_super(3, PAL_GREENBAR + enemy_hp_color);

        Display_SetOBJColorPalette(GBC_OBJ_PAL_PLAYER_MON, player_rgb);
        Display_SetOBJColorPalette(GBC_OBJ_PAL_ENEMY_MON,  enemy_rgb);
        Display_SetOBJColorPalette(GBC_OBJ_PAL_POKEBALL,
                                   gGbcSpritePalettes[SPR_PAL_ORANGE]);
    }

    Display_ClearAttrBoxes(0);
    Display_FillAttrBox( 1,  0, 10, 4, 3);
    Display_FillAttrBox(10,  7, 10, 4, 2);
    Display_FillAttrBox( 0,  4,  9, 8, 0);
    Display_FillAttrBox(11,  0,  9, 7, 1);
    Display_FillAttrBox( 0, 12, 20, 6, 0);

    Display_SetPositionAttrMode(1);
    Display_SetColorMode(1);
}

void GbcColor_SetPalPokemonWholeScreen(int dex) {
    const uint16_t *pal;
    if (!s_enabled) { Display_SetColorMode(0); return; }
    if (s_battle_autocolor) { gbc_apply_red_autocolor_all(); return; }
    pal = (dex > 0) ? GbcColor_MonPaletteRGB(dex)
                    : GbcColor_SuperPalette(RSGB_PAL_BLACK);
    if (!pal) return;
    Display_SetPositionAttrMode(0);
    for (int i = 0; i < 8; i++) Display_SetBGColorPalette(i, pal);
    Display_SetOBJColorPalette(0, pal);
    Display_SetOBJColorPalette(1, pal);
    Display_ClearAttrBoxes(0);
    Display_SetColorMode(1);
}

void GbcColor_SetPalPokedex(int dex) {
    const uint16_t *mon;
    if (!s_enabled) { Display_SetColorMode(0); return; }

    if (s_battle_autocolor) { gbc_apply_red_autocolor_all(); return; }

    mon = (dex > 0) ? GbcColor_MonPaletteRGB(dex)
                    : GbcColor_SuperPalette(RSGB_PAL_BROWNMON);
    if (!mon) return;

    Display_SetBGColorPalette(0, GbcColor_SuperPalette(RSGB_PAL_BROWNMON));
    Display_SetBGColorPalette(1, mon);

    Display_SetOBJColorPalette(0, mon);

    Display_ClearAttrBoxes(0);
    Display_FillAttrBox(1, 1, 8, 8, 1);

    Display_SetPositionAttrMode(1);
    Display_SetColorMode(1);
}

void GbcColor_SetPalTrainerCard(unsigned obtained_badges) {

    static const struct {
        unsigned char bit, pal, x1, y1, x2, y2;
    } kBadgeBoxes[] = {
        { 0, 0,  3, 12,  4, 13 },
        { 1, 1,  7, 12,  8, 13 },
        { 2, 3, 11, 12, 12, 13 },
        { 3, 2, 16, 11, 17, 12 },
        { 3, 1, 14, 13, 15, 14 },
        { 3, 3, 16, 13, 17, 14 },
        { 4, 2,  3, 15,  4, 16 },
        { 5, 3,  7, 15,  8, 16 },
        { 6, 2, 11, 15, 12, 16 },
        { 7, 1, 15, 15, 16, 16 },
    };
    unsigned i;

    if (!s_enabled) { Display_SetColorMode(0); return; }

    if (s_battle_autocolor) { gbc_apply_red_autocolor_all(); return; }

    Display_SetBGColorPalette(0, GbcColor_SuperPalette(RSGB_PAL_MEWMON));
    Display_SetBGColorPalette(1, GbcColor_SuperPalette(RSGB_PAL_BADGE));
    Display_SetBGColorPalette(2, GbcColor_SuperPalette(RSGB_PAL_REDMON));
    Display_SetBGColorPalette(3, GbcColor_SuperPalette(RSGB_PAL_YELLOWMON));

    Display_SetOBJColorPalette(0, GbcColor_SuperPalette(RSGB_PAL_MEWMON));

    Display_ClearAttrBoxes(0);
    for (i = 0; i < sizeof kBadgeBoxes / sizeof kBadgeBoxes[0]; i++) {
        if (!(obtained_badges & (1u << kBadgeBoxes[i].bit)))
            continue;

        Display_FillAttrBox(kBadgeBoxes[i].x1, kBadgeBoxes[i].y1,
                            kBadgeBoxes[i].x2 - kBadgeBoxes[i].x1 + 1,
                            kBadgeBoxes[i].y2 - kBadgeBoxes[i].y1 + 1,
                            kBadgeBoxes[i].pal);
    }

    Display_SetPositionAttrMode(1);
    Display_SetColorMode(1);
}

void GbcColor_SetPalStatusScreen(int dex, int hp_color) {
    const uint16_t *mon;
    if (!s_enabled) { Display_SetColorMode(0); return; }
    if (s_battle_autocolor) { gbc_apply_red_autocolor_all(); return; }
    if (hp_color < 0 || hp_color > 2) hp_color = 0;

    mon = (dex > 0) ? GbcColor_MonPaletteRGB(dex)
                    : GbcColor_SuperPalette(RSGB_PAL_BROWNMON);
    if (!mon) return;

    gbc_load_super(0, PAL_GREENBAR + hp_color);
    Display_SetBGColorPalette(1, mon);
    Display_SetOBJColorPalette(0, mon);

    Display_ClearAttrBoxes(0);
    Display_FillAttrBox(1, 0, 7, 7, 1);

    Display_SetPositionAttrMode(1);
    Display_SetColorMode(1);
}

void GbcColor_SetPalBattleBlack(void) {
    if (!s_enabled) { Display_SetColorMode(0); return; }

    if (s_battle_autocolor) {
        gbc_apply_red_autocolor_all();
        return;
    }

    for (int i = 0; i < 5; i++) gbc_load_super(i, PAL_BLACK);
    Display_SetOBJColorPalette(GBC_OBJ_PAL_PLAYER_MON, battle_super(PAL_BLACK));
    Display_SetOBJColorPalette(GBC_OBJ_PAL_ENEMY_MON,  battle_super(PAL_BLACK));

    Display_ClearAttrBoxes(0);
    Display_FillAttrBox( 0,  0, 11, 4, 3);
    Display_FillAttrBox( 9,  7, 11, 4, 2);
    Display_FillAttrBox( 9, 11, 11, 1, 4);
    Display_FillAttrBox( 0,  4,  9, 8, 0);
    Display_FillAttrBox(11,  0,  9, 7, 1);
    Display_FillAttrBox( 0, 12, 20, 6, 0);

    Display_SetPositionAttrMode(1);
    Display_SetColorMode(1);
}

void GbcColor_EndBattle(void) {
    Display_SetPositionAttrMode(0);
}

void GbcColor_LoadAttackPalettes(void) {
    if (!s_enabled) return;

    for (int i = 0; i < GBC_NUM_SPRITE_PALETTES; i++)
        Display_SetOBJColorPalette(GBC_OBJ_PAL_ATK_BASE + i, gGbcAttackSpritePalettes[i]);
}

#define MOVE_ABSORB      71
#define MOVE_STUN_SPORE  78
#define MOVE_SOLARBEAM   76
#define MOVE_TRI_ATTACK 161
#define TYPE_GRASS     0x16
#define TYPE_ELECTRIC  0x17

int GbcColor_AnimSpritePalette(int anim_tileset, uint8_t tile_id, int move_type) {

    const uint8_t *map = gGbcAnimTilesetPalMap[anim_tileset == 1 ? 1 : 0];
    uint8_t v = map[tile_id & (GBC_ANIM_TILESET_PAL_MAP_SIZE - 1)];
    if (v < 8) return GBC_OBJ_PAL_ATK_BASE + v;
    if (v == 9) return -1;
    if (v == 8) {
        if ((unsigned)move_type >= GBC_NUM_TYPES) return GBC_OBJ_PAL_ATK_BASE;
        return GBC_OBJ_PAL_ATK_BASE + gGbcTypeColorTable[move_type];
    }

    return GBC_OBJ_PAL_ATK_BASE;
}

int GbcColor_AnimTypeForMove(int animation_id, int move_type) {
    if (animation_id == MOVE_ABSORB)     return TYPE_GRASS;
    if (animation_id == MOVE_STUN_SPORE) return TYPE_ELECTRIC;
    if (animation_id == MOVE_SOLARBEAM)  return TYPE_ELECTRIC;
    if (animation_id == MOVE_TRI_ATTACK) return TYPE_ELECTRIC;
    return move_type;
}

uint8_t GbcColor_PalForSprite(int picture_id, int oam_slot) {
    int idx = picture_id - 1;
    uint8_t pal;
    if ((unsigned)idx >= GBC_NUM_SPRITES) return SPR_PAL_ORANGE;
    pal = gGbcSpritePalAssignments[idx];
    if (pal != SPR_PAL_RANDOM) return pal;

    if (gbc_logical_map_id(wCurMap) == MAP_BILLS_HOUSE) return SPR_PAL_BROWN;

    return (uint8_t)((oam_slot + 1) & 3);
}
