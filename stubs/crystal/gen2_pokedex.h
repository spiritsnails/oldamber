
#pragma once
#include <stdint.h>

#define GEN2_NUM_SPECIES 251

typedef struct {
    const char *name;
    uint8_t hp, atk, def, spd, sat, sdf;
    uint8_t type1, type2;
    uint8_t catch_rate, base_exp;
    uint8_t item1, item2;
    uint8_t gender, hatch_cycles;
    uint8_t pic_dimensions;
    uint8_t growth_rate;
    uint8_t egg1, egg2;
    uint8_t tmhm[8];
} gen2_base_stats_t;

extern const gen2_base_stats_t gGen2BaseStats[GEN2_NUM_SPECIES];
