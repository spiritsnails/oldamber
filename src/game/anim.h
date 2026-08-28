#pragma once
#include <stdint.h>

#define TILEANIM_NONE         0
#define TILEANIM_WATER        1
#define TILEANIM_WATER_FLOWER 2

void Anim_SetTileset(uint8_t anim_type);

void Anim_UpdateTiles(void);
