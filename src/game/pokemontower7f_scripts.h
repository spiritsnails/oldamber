#pragma once
#include <stdint.h>

void PokemonTower7FScripts_OnMapLoad(void);
void PokemonTower7FScripts_StepCheck(void);
void PokemonTower7FScripts_Tick(void);
int  PokemonTower7FScripts_IsActive(void);

int  PokemonTower7FScripts_GetPendingBattle(uint8_t *class_out, uint8_t *no_out);
int  PokemonTower7FScripts_ConsumeBattle(void);
void PokemonTower7FScripts_OnVictory(void);
void PokemonTower7FScripts_OnDefeat(void);

void PokemonTower7F_Rocket1Script(void);
void PokemonTower7F_Rocket2Script(void);
void PokemonTower7F_Rocket3Script(void);
