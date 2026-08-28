
#pragma once
#include <stdint.h>

#define CRYSTAL_FONT_TILES  128
#define CRYSTAL_FRAME_TILES 6
#define CRYSTAL_NUM_FRAMES  9

extern const uint8_t gCrystalFont[CRYSTAL_FONT_TILES][16];

extern const uint8_t gCrystalFrames[CRYSTAL_NUM_FRAMES][CRYSTAL_FRAME_TILES][16];

extern const uint8_t gCrystalTextboxSpace[16];

#define CRYSTAL_BATTLE_EXTRA_TILES 25

extern const uint8_t gCrystalBattleExtra[CRYSTAL_BATTLE_EXTRA_TILES][16];
