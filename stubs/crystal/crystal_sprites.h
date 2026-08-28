
#pragma once
#include <stdint.h>

#define CRYSTAL_NUM_SPRITES 102
#define CRYSTAL_SPRITE_TILES 24
#define CRYSTAL_SPRITE_GFX_SIZE (CRYSTAL_SPRITE_TILES * 16)

extern const uint8_t gCrystalSpriteGfx[CRYSTAL_NUM_SPRITES][CRYSTAL_SPRITE_GFX_SIZE];

extern const uint8_t gCrystalSpriteTileCount[CRYSTAL_NUM_SPRITES];

extern const uint8_t gCrystalSpritePal[CRYSTAL_NUM_SPRITES];

extern const char *const gCrystalSpriteNames[CRYSTAL_NUM_SPRITES];

#define CRYSTAL_SPRITE_WALKING  1
#define CRYSTAL_SPRITE_STANDING 2
#define CRYSTAL_SPRITE_STILL    3
extern const uint8_t gCrystalSpriteType[CRYSTAL_NUM_SPRITES];
