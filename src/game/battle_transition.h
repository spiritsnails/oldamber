
#pragma once
#include <stdint.h>

void BattleTransition_Start(int is_trainer, int enemy_level, int player_level);

int BattleTransition_Tick(void);

int BattleTransition_IsActive(void);

void BattleTransition_SetZoomMode(int on);
int  BattleTransition_GetZoomMode(void);
