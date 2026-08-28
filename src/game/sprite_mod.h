#pragma once

#include <stdint.h>

int SpriteMod_LoadFrontFromFile(uint8_t species, const char *path);
const uint8_t *SpriteMod_GetFrontTile(uint8_t species, int tile_idx);
int SpriteMod_LoadBackFromFile(uint8_t species, const char *path);
const uint8_t *SpriteMod_GetBackTile(uint8_t species, int tile_idx);
