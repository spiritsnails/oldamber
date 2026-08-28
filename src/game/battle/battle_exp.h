#pragma once

#include <stdint.h>

void Battle_GainExperience(void);

uint8_t Battle_CalcLevelFromExp(uint8_t growth_rate, uint32_t exp);

void Battle_LearnMoveFromLevelUp(uint8_t slot, uint8_t new_level);

extern int gDebugExpRate;

void Battle_AddExpDirect(uint8_t slot, uint32_t amount);

int Pokemon_ApplyRareCandy(uint8_t slot, uint8_t *new_level_out);

int Pokemon_ApplyEvoStone(uint8_t slot, uint8_t stone_item_id);

int Pokemon_CanEvolveWithStone(uint8_t slot, uint8_t stone_item_id);

int Pokemon_ApplyVitamin(uint8_t slot, uint8_t item_id);

const char *Pokemon_VitaminStatName(uint8_t item_id);

int Pokemon_ApplyPPRestore(uint8_t slot, int move_index, uint8_t item_id);

int Pokemon_ApplyPPUp(uint8_t slot, int move_index);

void Battle_EvolutionAfterBattle(void);

int Battle_CheckNextEvolution(uint8_t *slot_out, uint8_t *new_species_out);

void Battle_ApplyEvolution(uint8_t slot, uint8_t new_species);

void Battle_CancelEvolution(uint8_t slot);

typedef struct {
    int      valid;
    uint16_t atk, def, spd, spc;
} levelup_stats_t;

typedef enum {
    BEXP_EVENT_TEXT = 0,
    BEXP_EVENT_LEARN_MOVE = 1
} battleexp_event_type_t;

#define BEXP_ANIM_NONE   0
#define BEXP_ANIM_GAIN   1
#define BEXP_ANIM_LEVEL  2
#define BEXP_ANIM_SETTLE 3

typedef struct {
    battleexp_event_type_t type;
    char                   text[80];
    levelup_stats_t        stats;
    uint8_t                slot;
    uint8_t                move_id;
    uint8_t                exp_anim;
    uint8_t                exp_to_full;
} battleexp_event_t;

int BattleExp_TakeNextEvent(battleexp_event_t *out);

typedef struct {
    uint32_t seq;
    uint8_t  kind;
    uint8_t  slot;
    uint8_t  to_full;
} battleexp_barcue_t;

void BattleExp_GetBarCue(battleexp_barcue_t *out);

void Battle_AwardExpForFaintedEnemy(void);

void BattleExp_SetModernShare(int on);
int  BattleExp_ModernShare(void);
