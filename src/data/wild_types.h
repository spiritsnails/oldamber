#pragma once

#include <stdint.h>

#include "game/types.h"

typedef struct PACKED {
    uint8_t level;
    uint8_t species;
} wild_slot_t;

typedef struct PACKED {
    uint8_t      rate;
    wild_slot_t  slots[10];
} wild_mons_t;
