#pragma once
#include <stdint.h>

void VictoryRoadScripts_OnMapLoad(void);
void VictoryRoadScripts_Tick(void);
void VictoryRoadScripts_MoltresInteract(void);
int  VictoryRoadScripts_ConsumeMoltresBattle(void);
int  VictoryRoadScripts_ConsumeMoltresPostBattle(void);
void VictoryRoadScripts_OnMoltresBattleOutcome(uint8_t battle_outcome);
