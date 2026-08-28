#pragma once

#include <stdint.h>
#include "../data/base_stats.h"

int SpeciesMod_ResolveSpeciesToken(const char *tok, uint8_t *out_species);
void SpeciesMod_DefineAlias(const char *name, uint8_t species_id);
void SpeciesMod_SetName(uint8_t species, const char *name);
const char *SpeciesMod_GetName(uint8_t species);

void SpeciesMod_SetBaseStats(uint8_t species, const base_stats_t *bs);
int SpeciesMod_GetBaseStats(uint8_t species, base_stats_t *out_bs);
void SpeciesMod_SetStartMoves(uint8_t species, uint8_t m1, uint8_t m2, uint8_t m3, uint8_t m4);
int SpeciesMod_GetLevelMoves(uint8_t species, uint8_t level, uint8_t out_moves[32], uint8_t *out_count);
void SpeciesMod_AddLevelMove(uint8_t species, uint8_t level, uint8_t move);

void SpeciesMod_BankSetEnabled(int enabled);
int SpeciesMod_BankIsEnabled(void);
int SpeciesMod_BankSave(void);
int SpeciesMod_BankLoad(void);
int SpeciesMod_BankClear(void);
