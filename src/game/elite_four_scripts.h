#pragma once
#include <stdint.h>

void EliteFourScripts_OnMapLoad(void);
void EliteFourScripts_Tick(void);
int EliteFourScripts_IsActive(void);

int EliteFourScripts_GetPendingBattle(uint8_t *class_out, uint8_t *no_out);
int EliteFourScripts_ConsumeBattle(void);
void EliteFourScripts_OnVictory(void);

void EliteFourScripts_OnRoomTrainerVictory(void);
void EliteFourScripts_OnDefeat(void);
int EliteFourScripts_BlocksMovementTo(int nx, int ny);

void EliteFourScripts_LoreleiInteract(void);
void EliteFourScripts_BrunoInteract(void);
void EliteFourScripts_AgathaInteract(void);
void EliteFourScripts_LanceInteract(void);

void EliteFourScripts_ResetRun(void);

void EliteFourScripts_ResetRunFull(void);
