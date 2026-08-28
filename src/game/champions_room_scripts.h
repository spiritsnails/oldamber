#pragma once

#include <stdint.h>

void ChampionsRoomScripts_OnMapLoad(void);
void ChampionsRoomScripts_Tick(void);
int  ChampionsRoomScripts_IsActive(void);

int  ChampionsRoomScripts_GetPendingBattle(uint8_t *class_out, uint8_t *no_out);
int  ChampionsRoomScripts_ConsumeBattle(void);
void ChampionsRoomScripts_OnVictory(void);
void ChampionsRoomScripts_OnDefeat(void);

void ChampionsRoomScripts_RivalInteract(void);
void ChampionsRoomScripts_OakInteract(void);
