
#pragma once
#include <stdint.h>

#define CRYSTAL_NUM_BG_PALETTES 42
#define CRYSTAL_PAL_MORN 0
#define CRYSTAL_PAL_DAY 8
#define CRYSTAL_PAL_NITE 16
#define CRYSTAL_PAL_DARK 24
#define CRYSTAL_PAL_INDOOR 32
#define CRYSTAL_PAL_WATER 40

#define CRYSTAL_PAL_BG_GRAY 0
#define CRYSTAL_PAL_BG_RED 1
#define CRYSTAL_PAL_BG_GREEN 2
#define CRYSTAL_PAL_BG_WATER 3
#define CRYSTAL_PAL_BG_YELLOW 4
#define CRYSTAL_PAL_BG_BROWN 5
#define CRYSTAL_PAL_BG_ROOF 6
#define CRYSTAL_PAL_BG_TEXT 7

#define CRYSTAL_NUM_ROOF_GROUPS 27
#define CRYSTAL_NUM_ENVIRONMENTS 8
#define CRYSTAL_TOD_MORN 0
#define CRYSTAL_TOD_DAY  1
#define CRYSTAL_TOD_NITE 2
#define CRYSTAL_TOD_DARK 3

extern const uint16_t gCrystalBGPalettes[CRYSTAL_NUM_BG_PALETTES][4];

extern const uint16_t gCrystalRoofPals[CRYSTAL_NUM_ROOF_GROUPS][4];

extern const uint16_t gCrystalObjectPals[4][8][4];

extern const uint8_t gCrystalEnvColors[CRYSTAL_NUM_ENVIRONMENTS][4][8];

extern const uint16_t gCrystalTextBGPal[4];
