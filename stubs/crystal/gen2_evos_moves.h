
#pragma once
#include <stdint.h>

#define GEN2_EVOS_NUM_SPECIES 251

typedef struct {
    uint8_t method, param, cmp, target;
} gen2_evolution_t;

typedef struct {
    uint8_t level, move;
} gen2_level_move_t;

typedef struct {
    const gen2_evolution_t  *evos;
    const gen2_level_move_t *moves;
    uint8_t num_evos, num_moves;
} gen2_evos_moves_t;

extern const gen2_evos_moves_t gGen2EvosMoves[GEN2_EVOS_NUM_SPECIES];

int Gen2EvosMoves_MovesAtLevel(uint8_t dex, uint8_t level, uint8_t out_moves[4]);
