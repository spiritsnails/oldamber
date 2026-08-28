#pragma once

#include <stdint.h>

typedef enum {
    MOVE_SFX_CMD_DUTY_CYCLE = 0,
    MOVE_SFX_CMD_DUTY_CYCLE_PATTERN = 1,
    MOVE_SFX_CMD_PITCH_SWEEP = 2,
    MOVE_SFX_CMD_SQUARE_NOTE = 3,
    MOVE_SFX_CMD_NOISE_NOTE = 4,
    MOVE_SFX_CMD_SOUND_LOOP = 5,
    MOVE_SFX_CMD_SOUND_RET = 6,
} move_sfx_cmd_type_t;

typedef struct {
    uint8_t type;
    int16_t p0;
    int16_t p1;
    int16_t p2;
    int16_t p3;
} move_sfx_cmd_t;

typedef struct {
    uint8_t  hw_channel;
    uint16_t cmd_first;
    uint16_t cmd_count;
} sfx_channel_t;

typedef struct {
    uint8_t  bank;
    uint8_t  channel_count;
    uint16_t chan_first;
} sfx_def_t;
