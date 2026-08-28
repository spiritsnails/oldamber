#pragma once
#include <stdint.h>

void    BagMenu_Open(void);

void    BagMenu_ReopenAfterUse(void);
void    BagMenu_Tick(void);
int     BagMenu_IsOpen(void);

void    BagMenu_OpenBattle(void);
uint8_t BagMenu_GetSelected(void);

void    BagMenu_OpenBattleOldMan(void);

void BagMenu_PalTrace(const char *where);
