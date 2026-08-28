
#include "party_icon_data.h"
#include "../platform/display.h"

void PartyIcons_LoadTiles(void) {

    static const uint8_t kSlots[] = { 0, 2, 4, 6, 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 56, 57, 58, 59, 64, 66, 68, 70, 72, 73, 74, 75, 76, 78, 80, 82, 84, 86, 88, 90, 92, 94, 96, 98, 100, 102, 120, 121, 122, 123 };
    for (int i = 0; i < 52; i++)
        Display_LoadSpriteTile(kSlots[i], gIconTileGfx[kSlots[i]]);
}
