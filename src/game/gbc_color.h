#pragma once
#include <stdint.h>

void GbcColor_SetEnabled(int on);
int  GbcColor_IsEnabled(void);

void GbcColor_ApplyForMap(uint8_t map_id);

void GbcColor_ApplyTileset(int tileset_id, int map_id, int owns_tile_slots);

uint8_t GbcColor_AttrForTileOnMap(int tileset_id, int tile_index, int map_id);

int GbcColor_MapIdForName(const char *name);

void GbcColor_Disable(void);

void GbcColor_Sync(int want_color, uint8_t map_id);

void GbcColor_MarkDirty(void);

uint8_t GbcColor_AttrForTile(int tileset_id, int tile_index);

void GbcColor_ApplySpritePalettes(int tileset_id, int map_id);

#define GBC_OBJ_PAL_ATK_BASE    0
#define GBC_OBJ_PAL_PLAYER_MON  8
#define GBC_OBJ_PAL_ENEMY_MON   9

#define GBC_OBJ_PAL_POKEBALL   10

#define GBC_OBJ_PAL_COUNT      16
#define GBC_OBJ_PAL_MASK     0x0F

void GbcColor_LoadAttackPalettes(void);

int GbcColor_AnimSpritePalette(int anim_tileset, uint8_t tile_id, int move_type);

int GbcColor_AnimTypeForMove(int animation_id, int move_type);

void GbcColor_SetPalBattle(int player_dex, int enemy_dex, int trainer_class,
                           int player_hp_color, int enemy_hp_color);

void GbcColor_SetBattleMonPalOverride(const uint16_t *player4,
                                      const uint16_t *enemy4);

void GbcColor_SetBattleSuperPalettes(const uint16_t (*table)[4], int count);

void GbcColor_SetPalBattleGen1(int player_dex, int enemy_dex, int trainer_class,
                               int player_hp_color, int enemy_hp_color);

void GbcColor_SetPalBattleBlack(void);

void GbcColor_SetPalPokemonWholeScreen(int dex);

void GbcColor_SetPalPokedex(int dex);

void GbcColor_SetPalTrainerCard(unsigned obtained_badges);

void GbcColor_SetPalStatusScreen(int dex, int hp_color);

void GbcColor_SetPalTradeGeneric(void);
void GbcColor_SetPalTradeMon(int dex);

void GbcColor_EndBattle(void);

uint8_t GbcColor_MonPalette(int dex);

#define GBC_MONPAL_ENHANCED 0
#define GBC_MONPAL_SGB      2
void GbcColor_SetMonPalStyle(int style);
int  GbcColor_MonPalStyleGet(void);

const uint16_t *GbcColor_SuperPalette(int pal_id);

const uint16_t *GbcColor_MonPaletteRGB(int dex);

#define GBC_OVERWORLD_DEFAULT      0
#define GBC_OVERWORLD_RED_SGB      1
#define GBC_OVERWORLD_RED_AUTOCOLOR 2
void GbcColor_SetOverworldStyle(int style);

void GbcColor_SetBattleAutoColor(int on);
int  GbcColor_BattleAutoColor(void);

void GbcColor_ApplyAutoColorAll(void);

void GbcColor_AutoColorMonPicPal(int obj_slot);

void GbcColor_AutoColorObjPal(int obj_slot, uint8_t obp);

void GbcColor_ApplyOverworldSpritePal(int obj_slot);
int  GbcColor_OverworldStyle(void);

uint8_t GbcColor_PalForSprite(int picture_id, int oam_slot);
