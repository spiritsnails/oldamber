#pragma once

#include <stdint.h>

#include "assetpack_bind.h"

#define SLOTS_MAP_W 20
#define SLOTS_MAP_H 12

#define SLOTS_SYMBOL_BG_BASE 0x25u

#define SLOTS_TILE_7      0x02u
#define SLOTS_TILE_BAR    0x06u
#define SLOTS_TILE_CHERRY 0x0au
#define SLOTS_TILE_FISH   0x0eu
#define SLOTS_TILE_BIRD   0x12u
#define SLOTS_TILE_MOUSE  0x16u

#define SLOTS_REEL_LEN   36
#define SLOTS_REEL_WRAP  30

void SlotsGfx_Load(void);
