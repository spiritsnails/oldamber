#pragma once

#include <stdint.h>

typedef struct {
    uint8_t        width;
    uint8_t        height;
    uint8_t        tileset_id;
    uint8_t        border_block;
    const uint8_t *blocks;
    const char    *name;
} map_info_t;

#define NUM_MAPS 256

#define PKS_REAL_MAP_COUNT 248

#define PKS_VIRTUAL_MAP_FIRST 248
#define PKS_VIRTUAL_MAP_LAST  255
#define PKS_VIRTUAL_MAP_COUNT (PKS_VIRTUAL_MAP_LAST - PKS_VIRTUAL_MAP_FIRST + 1)

#define PKS_VIRTUAL_MAP_W 32
#define PKS_VIRTUAL_MAP_H 20

extern map_info_t gMapTable[NUM_MAPS];

extern const uint8_t gMapIsFillerId[NUM_MAPS];
