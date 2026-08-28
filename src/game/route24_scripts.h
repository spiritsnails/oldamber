#pragma once
#include <stdint.h>

void Route24Scripts_OnMapLoad(void);

void Route24Scripts_StepCheck(void);

void Route24Scripts_Tick(void);

int  Route24Scripts_IsActive(void);

int  Route24Scripts_GetPendingBattle(uint8_t *class_out, uint8_t *no_out);

int  Route24Scripts_ConsumeRocketBattle(void);

void Route24Scripts_OnVictory(void);

void Route24Scripts_OnDefeat(void);
