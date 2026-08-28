#include "wardens_house_scripts.h"
#include "rom_text.h"
#include "inventory.h"
#include "text.h"
#include "yesno.h"
#include "../platform/audio.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"

#define ITEM_GOLD_TEETH 0x40
#define ITEM_HM04 0xC7

typedef enum {
    WH_IDLE = 0,
    WH_WAIT_GIB1_CLOSE,
    WH_WAIT_YESNO,
    WH_WAIT_GIB2_CLOSE,
    WH_WAIT_GOLD_TEETH_CLOSE,
    WH_WAIT_THANKS_CLOSE,
    WH_WAIT_RECEIVED_CLOSE,
    WH_WAIT_NO_ROOM_CLOSE,
} wardens_house_state_t;

static wardens_house_state_t s_state = WH_IDLE;

#define kGibberish1Text (RomText("WardensHouseWardenText.Gibberish1Text"))

#define kGibberish2Text (RomText("WardensHouseWardenText.Gibberish2Text"))

#define kGibberish3Text (RomText("WardensHouseWardenText.Gibberish3Text"))

#define kGaveTheGoldTeethText (RomText("_WardensHouseWardenGaveTheGoldTeethText"))

#define kThanksText (RomText("WardensHouseWardenText.ThanksText"))

#define kReceivedHM04Text (RomText("WardensHouseWardenText.ReceivedHM04Text"))

#define kHM04ExplanationText (RomText("WardensHouseWardenText.HM04ExplanationText"))

#define kHM04NoRoomText (RomText("WardensHouseWardenText.HM04NoRoomText"))

void WardensHouseScripts_OnMapLoad(void) {
    s_state = WH_IDLE;
}

int WardensHouseScripts_IsActive(void) {
    return s_state != WH_IDLE;
}

void WardensHouseScripts_Tick(void) {
    switch (s_state) {
    case WH_IDLE:
        return;

    case WH_WAIT_GIB1_CLOSE:
        if (Text_IsOpen()) return;
        YesNo_Show("");
        s_state = WH_WAIT_YESNO;
        return;

    case WH_WAIT_YESNO:
        YesNo_Tick();
        if (YesNo_IsOpen()) return;
        if (YesNo_GetResult()) Text_ShowASCII(kGibberish2Text);
        else Text_ShowASCII(kGibberish3Text);
        s_state = WH_WAIT_GIB2_CLOSE;
        return;

    case WH_WAIT_GIB2_CLOSE:
        if (Text_IsOpen()) return;
        s_state = WH_IDLE;
        return;

    case WH_WAIT_GOLD_TEETH_CLOSE:
        if (Text_IsOpen()) return;
        Text_ShowASCII(kThanksText);
        s_state = WH_WAIT_THANKS_CLOSE;
        return;

    case WH_WAIT_THANKS_CLOSE:
        if (Text_IsOpen()) return;
        if (Inventory_Add(ITEM_HM04, 1) != 0) {
            Text_ShowASCII(kHM04NoRoomText);
            s_state = WH_WAIT_NO_ROOM_CLOSE;
            return;
        }
        SetEvent(EVENT_GOT_HM04);
        Audio_PlaySFX_GetItem1();
        Text_SetItemName(ITEM_HM04);
        Text_ShowASCII(kReceivedHM04Text);
        s_state = WH_WAIT_RECEIVED_CLOSE;
        return;

    case WH_WAIT_RECEIVED_CLOSE:
        if (Text_IsOpen()) return;
        s_state = WH_IDLE;
        return;

    case WH_WAIT_NO_ROOM_CLOSE:
        if (Text_IsOpen()) return;
        s_state = WH_IDLE;
        return;
    }
}
