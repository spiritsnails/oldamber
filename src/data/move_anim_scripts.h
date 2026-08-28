#pragma once
#include <stdint.h>

#define MOVE_ANIM_FIRST_SE_ID 0xC0
#define MOVE_ANIM_NUM_ATTACK_ANIMS 203

typedef struct {
    uint8_t se_id;
    const char *asm_routine_name;
} move_anim_special_effect_ptr_t;

extern const move_anim_special_effect_ptr_t gMoveAnimSpecialEffectPointers[39];
extern const uint8_t gMoveAnimTilesetTileCounts[3];
extern const char *const gMoveAnimTilesetAsmLabels[3];
