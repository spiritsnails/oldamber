#pragma once
#include <stdio.h>

int AmberScript_BattleDebug_TryHandle(const char *cmd, const char *verb, int n);

void AmberScript_BattleDebug_Tick(void);

int AmberScript_IsAutoWinEnabled(void);
void AmberScript_SetAutoWinEnabled(int enabled);

void AmberScript_BattleDebug_WriteStateExtra(FILE *fp);
