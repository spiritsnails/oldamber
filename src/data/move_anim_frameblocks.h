#pragma once
#include <stdint.h>

#define MOVE_ANIM_NUM_FRAMEBLOCKS 122

typedef struct {
    uint8_t y_offset;
    uint8_t x_offset;
    uint8_t y_offset_flip;
    uint8_t x_offset_flip;
    uint8_t tile_id;
    uint8_t flags;
} move_anim_frameblock_sprite_t;

typedef struct {
    uint8_t count;
    const move_anim_frameblock_sprite_t *sprites;
} move_anim_frameblock_def_t;
