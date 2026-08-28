#include "safari_zone_secret_house_scripts.h"
#include "rom_text.h"
#include "text.h"
#include "inventory.h"
#include "../platform/audio.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"
#include <stdio.h>

#define MAP_SAFARI_ZONE_SECRET_HOUSE 0xDE
#define ITEM_HM03 0xC6

typedef enum {
    SZH_IDLE = 0,
    SZH_WAIT_PRETEXT_CLOSE,
    SZH_WAIT_POSTTEXT_CLOSE,
} szh_state_t;

static szh_state_t s_state = SZH_IDLE;

#define kYouHaveWonText (RomText("SafariZoneSecretHouseFishingGuruText.YouHaveWonText"))

#define kHM03ExplanationText (RomText("SafariZoneSecretHouseFishingGuruText.HM03ExplanationText"))

#define kHM03NoRoomText (RomText("SafariZoneSecretHouseFishingGuruText.HM03NoRoomText"))

void SafariZoneSecretHouseScripts_OnMapLoad(void) {
    s_state = SZH_IDLE;
}

int SafariZoneSecretHouseScripts_IsActive(void) {
    return s_state != SZH_IDLE;
}

void SafariZoneSecretHouseScripts_Tick(void) {
    switch (s_state) {
    case SZH_IDLE:
        return;

    case SZH_WAIT_PRETEXT_CLOSE:
        if (Text_IsOpen()) return;
        if (Inventory_Add(ITEM_HM03, 1) != 0) {
            Text_ShowASCII(kHM03NoRoomText);
            s_state = SZH_WAIT_POSTTEXT_CLOSE;
            return;
        }
        SetEvent(EVENT_GOT_HM03);
        Text_SetItemName(ITEM_HM03);
        Audio_PlaySFX_GetKeyItem();
        Text_ShowASCII(RomText("_SafariZoneSecretHouseFishingGuruReceivedHM03Text"));
        s_state = SZH_WAIT_POSTTEXT_CLOSE;
        return;

    case SZH_WAIT_POSTTEXT_CLOSE:
        if (Text_IsOpen()) return;
        s_state = SZH_IDLE;
        return;
    }
}
