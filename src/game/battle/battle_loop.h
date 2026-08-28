#pragma once

#include <stdint.h>

typedef enum {
    BATTLE_RESULT_CONTINUE       = 0,
    BATTLE_RESULT_PLAYER_FAINTED = 1,
    BATTLE_RESULT_ENEMY_FAINTED  = 2,
    BATTLE_RESULT_ESCAPED        = 3,
} battle_result_t;

#define BATTLE_OUTCOME_NONE             0
#define BATTLE_OUTCOME_WILD_VICTORY     1
#define BATTLE_OUTCOME_TRAINER_VICTORY  2
#define BATTLE_OUTCOME_BLACKOUT         3
#define BATTLE_OUTCOME_CAUGHT           4
#define BATTLE_OUTCOME_LOSS_NO_BLACKOUT 5

extern int gBattleNoBlackoutOnLoss;

battle_result_t Battle_TurnPrepare(void);

int Battle_TurnPlayerFirst(void);

void Battle_SelectEnemyMove(void);

void Battle_CheckNumAttacksLeft(void);

battle_result_t Battle_RunTurn(void);
