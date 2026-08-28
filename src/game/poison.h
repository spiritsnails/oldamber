#pragma once

#include <stdint.h>

void Poison_StepCheck(void);

void Poison_Tick(void);

void Poison_DebugApply(int party_slot);

void Poison_StartBattleBlackout(void);

int  Poison_IsBlackingOut(void);
