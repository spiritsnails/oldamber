#pragma once

#include <stdint.h>

#define SFX_INDEX_NONE      0xFFFFu
#define MOVE_SFX_DATA_COUNT 166

typedef struct {
    uint16_t sfx_index;
    uint8_t  pitch_mod;
    uint8_t  tempo_mod;
} move_sfx_data_t;
