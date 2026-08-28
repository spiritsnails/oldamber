#pragma once

#include <stdint.h>

#define NUM_SPRITES  73
#define SPRITE_TILES 24
#define SPRITE_GFX_SIZE (SPRITE_TILES * 16)

#include "assetpack_bind.h"

#define PKS_CRYSTAL_SPRITE_BASE 128
#define PKS_SPRITE_IS_CRYSTAL(id) ((id) >= PKS_CRYSTAL_SPRITE_BASE)
