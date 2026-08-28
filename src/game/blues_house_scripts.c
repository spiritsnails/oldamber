
#include "blues_house_scripts.h"
#include "rom_text.h"
#include "npc.h"
#include "text.h"
#include "inventory.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "../data/event_constants.h"

#define MAP_BLUES_HOUSE      0x27
#define DAISY_SITTING_NPC    0
#define DAISY_WALKING_NPC    1
#define TOWN_MAP_NPC         2

typedef enum {
    BHS_IDLE = 0,
    BHS_RIVAL_TEXT,
    BHS_OFFER_TEXT,
    BHS_BAG_FULL_TEXT,
    BHS_GOT_MAP_TEXT,
    BHS_USE_MAP_TEXT,
} blues_house_state_t;

static blues_house_state_t gState = BHS_IDLE;

#define kDaisyRivalAtLab (RomText("BluesHouseDaisyRivalAtLabText"))

#define kDaisyOfferMap (RomText("BluesHouseDaisyOfferMapText"))

static const char kGotMapText[] =
    "{PLAYER} got a\nTOWN MAP!";

#define kDaisyBagFull (RomText("BluesHouseDaisyBagFullText"))

#define kDaisyUseMap (RomText("BluesHouseDaisyUseMapText"))

void BluesHouseScripts_OnMapLoad(void) {
    if (wCurMap != MAP_BLUES_HOUSE) return;

    gState = BHS_IDLE;
    SetEvent(EVENT_ENTERED_BLUES_HOUSE);

    if (CheckEvent(EVENT_GOT_TOWN_MAP)) {
        SetEvent(EVENT_DAISY_WALKING);
        NPC_HideSprite(DAISY_SITTING_NPC);
        NPC_ShowSprite(DAISY_WALKING_NPC);
        NPC_HideSprite(TOWN_MAP_NPC);
        return;
    }

    NPC_ShowSprite(DAISY_SITTING_NPC);
    NPC_HideSprite(DAISY_WALKING_NPC);
    if (CheckEvent(EVENT_GOT_POKEDEX)) {
        NPC_ShowSprite(TOWN_MAP_NPC);
    } else {
        NPC_HideSprite(TOWN_MAP_NPC);
    }
}

int BluesHouseScripts_IsActive(void) {
    return gState != BHS_IDLE;
}

void BluesHouseScripts_Tick(void) {
    if (wCurMap != MAP_BLUES_HOUSE) return;

    switch (gState) {
    case BHS_IDLE:
        return;

    case BHS_RIVAL_TEXT:
        if (Text_IsOpen()) { Text_Update(); return; }
        gState = BHS_IDLE;
        return;

    case BHS_OFFER_TEXT:
        if (Text_IsOpen()) { Text_Update(); return; }
        if (Inventory_Add(ITEM_TOWN_MAP, 1) != 0) {
            Text_ShowASCII(kDaisyBagFull);
            gState = BHS_BAG_FULL_TEXT;
            return;
        }
        SetEvent(EVENT_GOT_TOWN_MAP);
        Audio_PlaySFX_GetKeyItem();
        NPC_HideSprite(TOWN_MAP_NPC);
        Text_ShowASCII(kGotMapText);
        gState = BHS_GOT_MAP_TEXT;
        return;

    case BHS_BAG_FULL_TEXT:
        if (Text_IsOpen()) { Text_Update(); return; }
        gState = BHS_IDLE;
        return;

    case BHS_GOT_MAP_TEXT:
        if (Text_IsOpen()) { Text_Update(); return; }
        gState = BHS_IDLE;
        return;

    case BHS_USE_MAP_TEXT:
        if (Text_IsOpen()) { Text_Update(); return; }
        gState = BHS_IDLE;
        return;
    }
}
