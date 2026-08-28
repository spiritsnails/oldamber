#include "route1_scripts.h"
#include "rom_text.h"
#include "text.h"
#include "inventory.h"
#include "../platform/audio.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"

typedef enum {
    R1_IDLE = 0,
    R1_WAIT_SAMPLE_TEXT_CLOSE,
} Route1ScriptState;

static Route1ScriptState sState = R1_IDLE;

#define kSampleText (RomText("Route1Youngster1Text.MartSampleText"))

#define kGotPotionText (RomText("Route1Youngster1Text.GotPotionText"))

#define kAlsoBallsText (RomText("Route1Youngster1Text.AlsoGotPokeballsText"))

#define kNoRoomText (RomText("Route1Youngster1Text.NoRoomText"))

void Route1Youngster1_PotionScript(void) {
    if (sState != R1_IDLE) return;

    if (CheckEvent(EVENT_GOT_POTION_SAMPLE)) {
        Text_ShowASCII(kAlsoBallsText);
        return;
    }

    Text_ShowASCII(kSampleText);
    sState = R1_WAIT_SAMPLE_TEXT_CLOSE;
}

void Route1Scripts_Tick(void) {
    switch (sState) {
    case R1_IDLE:
        return;
    case R1_WAIT_SAMPLE_TEXT_CLOSE:
        if (Text_IsOpen()) return;
        if (Inventory_Add(ITEM_POTION, 1) != 0) {
            Text_ShowASCII(kNoRoomText);
            sState = R1_IDLE;
            return;
        }
        SetEvent(EVENT_GOT_POTION_SAMPLE);
        Audio_PlaySFX_GetItem1();
        Text_SetItemName(ITEM_POTION);
        Text_ShowASCII(kGotPotionText);
        sState = R1_IDLE;
        return;
    }
}
