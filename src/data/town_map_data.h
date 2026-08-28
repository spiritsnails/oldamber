#pragma once

#include <stdint.h>

#define TOWN_MAP_WORLD_TILE_BASE 0x60
#define TOWN_MAP_WORLD_TILE_COUNT 16
#define TOWN_MAP_FIRST_INDOOR_MAP 0x25
#define TOWN_MAP_SCREEN_TILE_COUNT (20 * 18)

extern const int gTownMapOrderCount;

typedef struct {
    uint8_t group_end;
    uint8_t coords;
} town_map_internal_entry_t;

extern const int gTownMapInternalCoordsCount;

void TownMapData_LoadTiles(void);

#include "assetpack_bind.h"
