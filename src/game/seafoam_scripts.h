#pragma once
#include <stdint.h>

void SeafoamScripts_OnMapLoad(void);
void SeafoamScripts_Tick(void);

void SeafoamScripts_StepCheck(void);

void SeafoamScripts_ArmMoveObject(void);

void SeafoamScripts_ArticunoInteract(void);
int  SeafoamScripts_ConsumeArticunoBattle(void);
int  SeafoamScripts_ConsumeArticunoPostBattle(void);
void SeafoamScripts_OnArticunoBattleOutcome(uint8_t battle_outcome);
