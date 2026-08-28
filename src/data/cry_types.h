#pragma once

#include <stdint.h>

#include "game/types.h"

typedef struct PACKED {
    uint8_t  len;
    uint8_t  vol;
    uint8_t  fade;
    uint16_t freq;
} cry_sq_note_t;

typedef struct PACKED {
    uint8_t len;
    uint8_t vol;
    uint8_t fade;
    uint8_t nr43;
} cry_noise_note_t;

typedef struct {
    uint8_t              duty_pattern;
    uint8_t              rotate_duty;
    uint8_t              n_notes;
    const cry_sq_note_t *notes;
} cry_sq_ch_t;

typedef struct {
    uint8_t                 n_notes;
    const cry_noise_note_t *notes;
} cry_noise_ch_t;

typedef struct {
    cry_sq_ch_t    ch5;
    cry_sq_ch_t    ch6;
    cry_noise_ch_t ch8;
} cry_def_t;

typedef struct PACKED {
    uint8_t base_cry;
    int8_t  pitch_mod;
    uint8_t tempo_mod;
} pokemon_cry_t;
