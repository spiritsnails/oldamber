#pragma once
#include <stdint.h>

const uint8_t *AI_EnemyTrainerChooseMoves(uint8_t out_moves[4]);

typedef enum {
    AI_ACT_NONE = 0,
    AI_ACT_ITEM_HEAL,
    AI_ACT_FULL_HEAL,
    AI_ACT_X_STAT,
    AI_ACT_GUARD_SPEC,
    AI_ACT_SWITCH
} ai_action_kind_t;

typedef struct {
    ai_action_kind_t kind;
    uint8_t item_id;
    uint16_t hp_before;
    uint16_t hp_after;
} ai_action_t;

extern ai_action_t gLastAIAction;

int AI_TrainerAI(void);
