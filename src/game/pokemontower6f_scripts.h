#pragma once
#include <stdint.h>

void PokemonTower6FScripts_OnMapLoad(void);

void PokemonTower6FScripts_StepCheck(void);

void PokemonTower6FScripts_StartGhostEncounter(void);
void PokemonTower6FScripts_Tick(void);
int  PokemonTower6FScripts_IsActive(void);

int  PokemonTower6FScripts_GetPendingBattle(uint8_t *species_out, uint8_t *level_out);
int  PokemonTower6FScripts_ConsumeBattle(void);
void PokemonTower6FScripts_OnBattleOutcome(uint8_t battle_outcome);
