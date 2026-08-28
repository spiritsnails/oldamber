#include "town_map_data.h"

#include "../platform/display.h"

const int gTownMapInternalCoordsCount = 60;

const int gTownMapOrderCount = 47;

void TownMapData_LoadTiles(void) {
    for (int i = 0; i < TOWN_MAP_WORLD_TILE_COUNT; i++)
        Display_LoadTile((uint8_t)(TOWN_MAP_WORLD_TILE_BASE + i), gTownMapWorldTiles[i]);
}
