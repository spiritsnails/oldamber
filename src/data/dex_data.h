#pragma once

#include <stdint.h>

typedef struct {
    const char *category;
    uint8_t     height_ft;
    uint8_t     height_in;
    uint16_t    weight;
    const char *description;
} dex_entry_t;

#define NUM_DEX_ENTRIES 152

extern dex_entry_t gDexEntries[NUM_DEX_ENTRIES];
void DexEntries_LoadFromPack(void);
