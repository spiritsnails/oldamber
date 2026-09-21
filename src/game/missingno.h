#pragma once
#include <stdint.h>
#include "types.h"
#include "../data/wild_data.h"

#define SPECIES_M_00_INTERNAL 0xbf

int MissingNo_IsSpecies(uint8_t species);
int MissingNo_IsM00(uint8_t species);
int MissingNo_IsEnabledSpecies(uint8_t species);
const char *MissingNo_GetName(uint8_t species);
int MissingNo_GetBaseStats(uint8_t species, base_stats_t *out_bs);
int MissingNo_GetTypes(uint8_t species, uint8_t *out_type1, uint8_t *out_type2);
const uint8_t *MissingNo_GetFrontTile(uint8_t species, int tile);
const uint8_t *MissingNo_GetBackTile(uint8_t species, int tile);
void MissingNo_ApplyItemDuplication(void);
void MissingNo_ApplyHallOfFameCorruption(uint8_t species);
void MissingNo_CaptureOldManName(void);
void MissingNo_ObserveWildTable(uint8_t map_id, const wild_mons_t *table);
int MissingNo_TryCinnabarEncounter(uint8_t real_map, int x, int y, int surfing,
                                   uint8_t rate_roll, uint8_t slot_roll,
                                   uint8_t *out_species, uint8_t *out_level);
