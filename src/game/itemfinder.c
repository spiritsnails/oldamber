
#include "itemfinder.h"
#include "rom_text.h"
#include "text.h"
#include "overworld.h"
#include "amberscript_core.h"
#include "amberscript_mapbank.h"
#include "../platform/audio.h"
#include "../platform/hardware.h"

static int hidden_item_near(void) {
    if (!AmberScript_IsEnabled()) return 0;

    const int px = (int)wXCoord, py = (int)wYCoord;
    const int y0 = (py >= 5) ? (py - 5) : 0;
    const int x0 = (px >= 5) ? (px - 5) : 0;

    for (int y = y0 + 1; y <= py + 4; y++) {
        if (y < 0) continue;
        for (int x = x0 + 1; x <= px + 5; x++) {
            uint8_t item = 0;
            uint16_t flag = 0;
            if (x < 0) continue;
            if (!AmberScript_GetHiddenItemAt(wCurMap, x, y, &item, &flag)) continue;
            if (flag && CheckEvent(flag)) continue;
            return 1;
        }
    }
    return 0;
}

#define IF_STEPS      8
#define IF_STEP_FRAMES 12

static int s_active;
static int s_step;
static int s_timer;

void Itemfinder_Use(void) {
    s_active = 0;
    s_step = 0;
    s_timer = 0;

    if (!hidden_item_near()) {
        Text_ShowASCII(RomText("ItemfinderFoundNothingText"));
        return;
    }
    s_active = 1;
}

int Itemfinder_IsActive(void) { return s_active; }

void Itemfinder_Tick(void) {
    if (!s_active) return;
    if (s_timer > 0) { s_timer--; return; }

    if (Audio_IsSFXPlaying()) return;

    if (s_step >= IF_STEPS) {
        s_active = 0;
        Text_ShowASCII(RomText("ItemfinderFoundItemText"));
        return;
    }
    if ((s_step & 1) == 0) Audio_PlaySFX_HealingMachine();
    else                   Audio_PlaySFX_Purchase();
    s_step++;
    s_timer = IF_STEP_FRAMES;
}
