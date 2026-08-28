
#pragma once
#include <stdint.h>

#define CRYSTAL_MON_COUNT 252
#define CRYSTAL_PIC_TILES 49
#define CRYSTAL_PIC_POOL 18079
#define CRYSTAL_PIC_FRAMES 1047
#define CRYSTAL_PIC_ANIM 3270

typedef struct {
    uint16_t tile_base;
    uint16_t tile_count;
    uint16_t frame_base;
    uint16_t anim_base;
    uint16_t idle_base;
    uint8_t  frame_count;
    uint8_t  dim;
} crystal_pic_t;

extern const crystal_pic_t gCrystalMonPic[CRYSTAL_MON_COUNT];
extern const uint8_t gCrystalPicTiles[CRYSTAL_PIC_POOL][16];
extern const uint8_t gCrystalPicFrames[CRYSTAL_PIC_FRAMES][CRYSTAL_PIC_TILES];
extern const uint8_t gCrystalPicAnim[CRYSTAL_PIC_ANIM][2];
extern const uint8_t gCrystalMonBackPic[CRYSTAL_MON_COUNT][CRYSTAL_PIC_TILES][16];

extern const uint16_t gCrystalMonPalette[CRYSTAL_MON_COUNT][2];
extern const uint16_t gCrystalMonShinyPalette[CRYSTAL_MON_COUNT][2];
