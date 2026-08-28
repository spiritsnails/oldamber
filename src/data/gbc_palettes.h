
#pragma once
#include <stdint.h>

#include "assetpack_bind.h"

#define GBC_NUM_TILESETS        24
#define GBC_TILESET_SIZE        0x60
#define GBC_NUM_MAP_PALETTES    0x40
#define GBC_NUM_ROOF_MAPS       37
#define GBC_NUM_SPRITES         72
#define GBC_NUM_TYPES           27
#define GBC_NUM_SPRITE_PALETTES 8
#define GBC_ANIM_TILESET_PAL_MAP_SIZE 0x80
#define GBC_NUM_SUPER_PALETTES    236
#define GBC_NUM_MON_PALETTES      152
#define GBC_NUM_TRAINER_PALETTES  48
#define GBC_NUM_BADGE_PAL_MAP     0x60

#define PAL_BG_GRAY    0
#define PAL_BG_RED     1
#define PAL_BG_GREEN   2
#define PAL_BG_WATER   3
#define PAL_BG_YELLOW  4
#define PAL_BG_BROWN   5
#define PAL_BG_ROOF    6
#define PAL_BG_TEXT    7

#define GBC_ATTR_PRIORITY  0x80
#define GBC_ATTR_PAL(a)    ((a) & 0x07)

#define SPR_PAL_ORANGE  0
#define SPR_PAL_BLUE    1
#define SPR_PAL_GREEN   2
#define SPR_PAL_BROWN   3
#define SPR_PAL_PURPLE  4
#define SPR_PAL_EMOJI   5
#define SPR_PAL_TREE    6
#define SPR_PAL_ROCK    7
#define SPR_PAL_RANDOM  8

#define ATK_PAL_GREY    0
#define ATK_PAL_BLUE    1
#define ATK_PAL_RED     2
#define ATK_PAL_BROWN   3
#define ATK_PAL_YELLOW  4
#define ATK_PAL_GREEN   5
#define ATK_PAL_ICE     6
#define ATK_PAL_PURPLE  7

extern const uint8_t  gGbcTilesetPalMap[GBC_NUM_TILESETS][GBC_TILESET_SIZE];

extern const uint8_t  gGbcMapPaletteSets[GBC_NUM_TILESETS][8];
extern const uint16_t gGbcMapPalettes[GBC_NUM_MAP_PALETTES][4];

extern const uint16_t gGbcRoofPalettes[GBC_NUM_ROOF_MAPS][2];

extern const uint16_t gGbcSpritePalettes[GBC_NUM_SPRITE_PALETTES][4];

extern const uint16_t gGbcSpritePalettesNite[GBC_NUM_SPRITE_PALETTES][4];
extern const uint16_t gGbcSpritePalettesPokecenter[GBC_NUM_SPRITE_PALETTES][4];
extern const uint16_t gGbcAttackSpritePalettes[GBC_NUM_SPRITE_PALETTES][4];

extern const uint8_t  gGbcSpritePalAssignments[GBC_NUM_SPRITES];

extern const uint8_t  gGbcTypeColorTable[GBC_NUM_TYPES];

extern const uint8_t  gGbcAnimTilesetPalMap[2][GBC_ANIM_TILESET_PAL_MAP_SIZE];

extern const uint16_t gGbcSuperPalettes[GBC_NUM_SUPER_PALETTES][4];

extern const uint8_t  gGbcMonPalette[GBC_NUM_MON_PALETTES];

extern const uint8_t  gGbcTrainerPalette[GBC_NUM_TRAINER_PALETTES];

extern const uint8_t  gGbcBadgePalMap[GBC_NUM_BADGE_PAL_MAP];

#define YMON_MEWMON    0
#define YMON_BLUEMON   1
#define YMON_REDMON    2
#define YMON_CYANMON   3
#define YMON_PURPLEMON 4
#define YMON_BROWNMON  5
#define YMON_GREENMON  6
#define YMON_PINKMON   7
#define YMON_YELLOWMON 8
#define YMON_GRAYMON   9

#define GBC_NUM_RED_SGB_PALS 37

#define RSGB_PAL_ROUTE     0x00
#define RSGB_PAL_PALLET    0x01
#define RSGB_PAL_VIRIDIAN  0x02
#define RSGB_PAL_PEWTER    0x03
#define RSGB_PAL_CERULEAN  0x04
#define RSGB_PAL_LAVENDER  0x05
#define RSGB_PAL_VERMILION 0x06
#define RSGB_PAL_CELADON   0x07
#define RSGB_PAL_FUCHSIA   0x08
#define RSGB_PAL_CINNABAR  0x09
#define RSGB_PAL_INDIGO    0x0A
#define RSGB_PAL_SAFFRON   0x0B
#define RSGB_PAL_TOWNMAP   0x0C
#define RSGB_PAL_LOGO1     0x0D
#define RSGB_PAL_LOGO2     0x0E
#define RSGB_PAL_0F        0x0F
#define RSGB_PAL_MEWMON    0x10
#define RSGB_PAL_BLUEMON   0x11
#define RSGB_PAL_REDMON    0x12
#define RSGB_PAL_CYANMON   0x13
#define RSGB_PAL_PURPLEMON 0x14
#define RSGB_PAL_BROWNMON  0x15
#define RSGB_PAL_GREENMON  0x16
#define RSGB_PAL_PINKMON   0x17
#define RSGB_PAL_YELLOWMON 0x18
#define RSGB_PAL_GRAYMON   0x19
#define RSGB_PAL_SLOTS1    0x1A
#define RSGB_PAL_SLOTS2    0x1B
#define RSGB_PAL_SLOTS3    0x1C
#define RSGB_PAL_SLOTS4    0x1D
#define RSGB_PAL_BLACK     0x1E
#define RSGB_PAL_GREENBAR  0x1F
#define RSGB_PAL_YELLOWBAR 0x20
#define RSGB_PAL_REDBAR    0x21
#define RSGB_PAL_BADGE     0x22
#define RSGB_PAL_CAVE      0x23
#define RSGB_PAL_GAMEFREAK 0x24

extern const uint16_t gGbcRedAutoColorBg[4];
extern const uint16_t gGbcRedAutoColorObj0[4];

extern const uint16_t gGbcBlueAutoColorBg[4];
extern const uint16_t gGbcBlueAutoColorObj0[4];
