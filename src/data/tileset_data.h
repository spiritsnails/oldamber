#pragma once

#include <stdint.h>

#define NUM_TILESETS 24

typedef struct {
    const uint8_t *blocks;
    uint16_t       num_blocks;
    const uint8_t *gfx;
    uint16_t       gfx_tiles;
    const uint8_t *coll_tiles;
    uint8_t        grass_tile;
    uint8_t        anim_type;

    uint8_t        counter[3];
} tileset_info_t;

extern tileset_info_t gTilesets[NUM_TILESETS];
