#pragma once

#include <stdint.h>

int Battle_AnyPartyAlive(void);

int Battle_HasMonFainted(uint8_t slot);

void Battle_LoadBattleMonFromParty(void);

void Battle_SendOutMon_State(void);

void Battle_ReadPlayerMonCurHPAndStatus(void);

void Battle_SwitchPlayerMon(uint8_t new_slot);

void Battle_ChooseNextMon(uint8_t new_slot);

void Battle_ApplyBadgeStatBoosts(void);
