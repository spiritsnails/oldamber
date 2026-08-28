
#pragma once
#include <stdint.h>

#define CRYSTAL_NUM_EMOTES 12
#define CRYSTAL_EMOTE_MAX_BYTES 64

typedef struct {
    const char *name;
    uint8_t     tiles;
    uint8_t     dest_tile;
    uint16_t    bytes;
    const uint8_t *gfx;
} crystal_emote_t;

extern const crystal_emote_t gCrystalEmotes[CRYSTAL_NUM_EMOTES];

#define CRYSTAL_GRASS_RUSTLE_FRAMES 2
#define CRYSTAL_GRASS_RUSTLE_OBJS 2

#define CRYSTAL_GRASS_RUSTLE_FRAME_MASK 4
#define CRYSTAL_GRASS_RUSTLE_PAL 6

typedef struct { int8_t y, x; uint8_t attr, tile; } crystal_oam_t;

extern const uint8_t gCrystalGrassRustleGfx[16];
extern const crystal_oam_t gCrystalGrassRustleOam[CRYSTAL_GRASS_RUSTLE_FRAMES][CRYSTAL_GRASS_RUSTLE_OBJS];
