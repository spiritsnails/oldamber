#pragma once

#include <stdint.h>

int Battle_AnyEnemyPokemonAliveCheck(void);

void Battle_LoadEnemyMonFromParty(void);

int Battle_EnemySendOut_State(void);

int Battle_ReplaceFaintedEnemyMon(void);

void Battle_TrainerBattleVictory(void);

void Battle_HandlePlayerBlackOut(void);

void Battle_HandlePlayerLossNoBlackOut(void);

int Battle_TryRunningFromBattle(void);
