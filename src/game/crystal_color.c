
#include "crystal_color.h"
#include "../platform/display.h"
#include "crystal_palettes.h"
#include "data/gbc_palettes.h"
#include <time.h>

#define CRYSTAL_MORN_HOUR 4
#define CRYSTAL_DAY_HOUR  10
#define CRYSTAL_NITE_HOUR 18

int CrystalColor_TimeOfDay(void) {
    time_t now = time(NULL);
    struct tm lt;
    int hour;
#if defined(_WIN32)
    if (localtime_s(&lt, &now) != 0) return CRYSTAL_TOD_DAY;
#else
    if (!localtime_r(&now, &lt)) return CRYSTAL_TOD_DAY;
#endif
    hour = lt.tm_hour;
    if (hour < CRYSTAL_MORN_HOUR) return CRYSTAL_TOD_NITE;
    if (hour < CRYSTAL_DAY_HOUR)  return CRYSTAL_TOD_MORN;
    if (hour < CRYSTAL_NITE_HOUR) return CRYSTAL_TOD_DAY;
    return CRYSTAL_TOD_NITE;
}

int CrystalColor_ApplyForEnv(int environment, int map_group) {
    int tod, slot;
    if (environment < 0 || environment >= CRYSTAL_NUM_ENVIRONMENTS) return 0;
    tod = CrystalColor_TimeOfDay();

    Display_SetPositionAttrMode(0);

    Display_FillTileAttrs(GBC_TILESET_SIZE, 0x100 - GBC_TILESET_SIZE,
                          CRYSTAL_PAL_BG_TEXT);
    for (slot = 0; slot < 8; slot++) {
        uint8_t idx = gCrystalEnvColors[environment][tod][slot];
        if (idx >= CRYSTAL_NUM_BG_PALETTES) continue;
        for (int c = 0; c < 4; c++)
            Display_SetBGColorEntry(slot, c, gCrystalBGPalettes[idx][c]);
    }

    for (int c = 0; c < 4; c++)
        Display_SetBGColorEntry(CRYSTAL_PAL_BG_TEXT, c, gCrystalTextBGPal[c]);

    if ((environment == 1 || environment == 2) &&
        map_group >= 0 && map_group < CRYSTAL_NUM_ROOF_GROUPS) {
        int at = (tod == CRYSTAL_TOD_NITE) ? 2 : 0;
        Display_SetBGColorEntry(CRYSTAL_PAL_BG_ROOF, 1,
                                gCrystalRoofPals[map_group][at]);
        Display_SetBGColorEntry(CRYSTAL_PAL_BG_ROOF, 2,
                                gCrystalRoofPals[map_group][at + 1]);
    }

    for (int i = 0; i < 8; i++)
        Display_SetOBJColorPalette(i, gCrystalObjectPals[tod][i]);

    Display_SetColorMode(1);
    return 1;
}
