
#include <stdint.h>
#include "dex_data.h"
#include "../platform/assetpack.h"

dex_entry_t gDexEntries[NUM_DEX_ENTRIES];

#define DEX_REC_SIZE 12
#define TEXT_NONE 0xFFFFFFFFu

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void DexEntries_LoadFromPack(void)
{
    const uint8_t *rec = (const uint8_t *)AssetPack_Require("gDexEntryRecords",
                                                            NULL);
    const char *text = (const char *)AssetPack_Require("gDexEntryText", NULL);

    for (int d = 1; d < NUM_DEX_ENTRIES; d++) {
        const uint8_t *r = rec + d * DEX_REC_SIZE;
        uint32_t cat = rd32(r + 4), desc = rd32(r + 8);
        gDexEntries[d].height_ft   = r[0];
        gDexEntries[d].height_in   = r[1];
        gDexEntries[d].weight      = (uint16_t)(r[2] | (r[3] << 8));
        gDexEntries[d].category    = (cat  == TEXT_NONE) ? 0 : text + cat;
        gDexEntries[d].description = (desc == TEXT_NONE) ? 0 : text + desc;
    }
}
