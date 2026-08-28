
#include "slots_gfx.h"
#include "../platform/display.h"

void SlotsGfx_Load(void) {
    int i;

    for (i = 0; i < 24; i++)
        Display_LoadSpriteTile((uint8_t)i, kSlotsSymbolTiles[i]);

    for (i = 0; i < 37; i++)
        Display_LoadTile((uint8_t)i, kSlotsFrameTiles[i]);

    for (i = 0; i < 24; i++)
        Display_LoadTile((uint8_t)(SLOTS_SYMBOL_BG_BASE + i), kSlotsSymbolTiles[i]);
}
