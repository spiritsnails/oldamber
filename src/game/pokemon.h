#pragma once

#include <stdint.h>
#include "types.h"

void Pokemon_InitMon(party_mon_t *mon, uint8_t species, uint8_t level);

uint32_t CalcExpForLevel(uint8_t growth_rate, uint8_t level);

const char *Pokemon_GetName(uint8_t dex);
const char *Pokemon_GetNameBySpecies(uint8_t species);

void Pokemon_EncodeNameString(const char *src, uint8_t *dst);

void Pokemon_WriteMovesForLevel(uint8_t *moves, uint8_t *pp,
                                uint8_t species_id, uint8_t level);

void Pokemon_AddToParty(uint8_t species, uint8_t level);

int Pokemon_AddToBox(uint8_t species, uint8_t level);

int Pokemon_SendBattleMonToBox(const battle_mon_t *mon);

int Pokemon_DepositPartyMonToBox(int party_slot);

int Pokemon_WithdrawBoxMonToParty(int box_slot);

int Pokemon_ReleaseBoxMon(int box_slot);

void Pokemon_RemoveFromParty(int slot);

void Pokemon_HealParty(void);

uint8_t Pokemon_LevelFromExp(uint8_t species, const uint8_t exp[3]);

uint8_t Pokemon_DaycareCheckedLevel(uint8_t species, uint8_t exp[3]);

int Pokemon_DepositPartyMonToDaycare(int party_slot);

int Pokemon_WithdrawDaycareMonToParty(void);
