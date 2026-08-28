#pragma once

#include <stdint.h>

#include "game/types.h"

#define SUPER_ROD_SLOTS 4

typedef struct PACKED {

    uint8_t count;
    struct PACKED {
        uint8_t level;
        uint8_t species;
    } slots[SUPER_ROD_SLOTS];
} super_rod_group_t;
