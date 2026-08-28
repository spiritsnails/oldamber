#pragma once

#include <stdint.h>

#include "game/types.h"

typedef struct PACKED {
    uint16_t freq;
    uint16_t frames;
    uint8_t  duty;
    uint8_t  volume;
    uint8_t  env_byte;
    uint8_t  vib_delay;
    uint8_t  vib_rate;
    uint8_t  vib_depth;
    uint16_t slide_target;
    uint8_t  slide_frames;
} note_evt_t;

typedef struct PACKED {
    uint8_t nr43;
    uint8_t env_byte;
    uint8_t frames;
} drum_step_t;

typedef struct {
    const note_evt_t *notes;
    int count;
    int loop_start;
} ch_data_t;

typedef struct {
    const drum_step_t *steps;
    uint8_t count;
} drum_inst_t;

typedef struct {
    const ch_data_t *ch[4];
} song_t;
