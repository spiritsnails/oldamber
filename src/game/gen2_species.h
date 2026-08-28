
#pragma once
#include <stdint.h>
#include "types.h"

void Gen2Species_Init(void);

uint8_t Gen2Species_DexToInternal(uint8_t dex);

uint8_t Gen2Species_InternalToDex(uint8_t species);

uint8_t Gen2Species_AnyDexToInternal(uint8_t dex);

int Gen2Species_GetBaseStats(uint8_t species, base_stats_t *out_bs);

const char *Gen2Species_GetName(uint8_t species);

int Gen2Species_GetTypes(uint8_t species, uint8_t *out_t1, uint8_t *out_t2);

uint8_t Species_Dex(uint8_t species);

int Species_GetBaseStats(uint8_t species, base_stats_t *out_bs);
