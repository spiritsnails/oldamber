#include "saffron_city_scripts.h"
#include "rom_text.h"
#include "constants.h"
#include "inventory.h"
#include "npc.h"
#include "text.h"
#include "../platform/audio.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"

#define MAP_SAFFRON_CITY_A 0x0A
#define MAP_SAFFRON_CITY_B 0x0B
#define ITEM_TM01 (ITEM_HM01 + 5)
#define ITEM_TM29 (ITEM_TM01 + 28)

#define NPC_SAFFRON_ROCKET1 0
#define NPC_SAFFRON_ROCKET2 1
#define NPC_SAFFRON_ROCKET3 2
#define NPC_SAFFRON_ROCKET4 3
#define NPC_SAFFRON_ROCKET5 4
#define NPC_SAFFRON_ROCKET6 5
#define NPC_SAFFRON_ROCKET7 6
#define NPC_SAFFRON_SCIENTIST 7
#define NPC_SAFFRON_SILPH_WORKER_M 8
#define NPC_SAFFRON_SILPH_WORKER_F 9
#define NPC_SAFFRON_GENTLEMAN 10
#define NPC_SAFFRON_BIRD 11
#define NPC_SAFFRON_ROCKER 12
#define NPC_SAFFRON_ROCKET8 13
#define NPC_SAFFRON_ROCKET9 14

typedef enum {
    SC_IDLE = 0,
    SC_TM29_WAIT_PRETEXT_CLOSE,
} SaffronCityScriptState;

static SaffronCityScriptState sState = SC_IDLE;

#define kMrPsychicPretext (RomText("MrPsychicsHouseMrPsychicText.YouWantedThisText"))

#define kMrPsychicReceived (RomText("MrPsychicsHouseMrPsychicText.ReceivedTM29Text"))
#define kMrPsychicExplain (RomText("MrPsychicsHouseMrPsychicText.TM29ExplanationText"))
#define kMrPsychicNoRoom (RomText("MrPsychicsHouseMrPsychicText.TM29NoRoomText"))

void SaffronCityScripts_OnMapLoad(void) {
    if (wCurMap != MAP_SAFFRON_CITY_A && wCurMap != MAP_SAFFRON_CITY_B) return;

    if (CheckEvent(EVENT_BEAT_SILPH_CO_GIOVANNI)) {
        NPC_HideSprite(NPC_SAFFRON_ROCKET1);
        NPC_HideSprite(NPC_SAFFRON_ROCKET2);
        NPC_HideSprite(NPC_SAFFRON_ROCKET3);
        NPC_HideSprite(NPC_SAFFRON_ROCKET4);
        NPC_HideSprite(NPC_SAFFRON_ROCKET5);
        NPC_HideSprite(NPC_SAFFRON_ROCKET6);
        NPC_HideSprite(NPC_SAFFRON_ROCKET7);
        NPC_ShowSprite(NPC_SAFFRON_SCIENTIST);
        NPC_ShowSprite(NPC_SAFFRON_SILPH_WORKER_M);
        NPC_ShowSprite(NPC_SAFFRON_SILPH_WORKER_F);
        NPC_ShowSprite(NPC_SAFFRON_GENTLEMAN);
        NPC_ShowSprite(NPC_SAFFRON_BIRD);
        NPC_ShowSprite(NPC_SAFFRON_ROCKER);
    } else {
        NPC_ShowSprite(NPC_SAFFRON_ROCKET1);
        NPC_ShowSprite(NPC_SAFFRON_ROCKET2);
        NPC_ShowSprite(NPC_SAFFRON_ROCKET3);
        NPC_ShowSprite(NPC_SAFFRON_ROCKET4);
        NPC_ShowSprite(NPC_SAFFRON_ROCKET5);
        NPC_ShowSprite(NPC_SAFFRON_ROCKET6);
        NPC_ShowSprite(NPC_SAFFRON_ROCKET7);
        NPC_HideSprite(NPC_SAFFRON_SCIENTIST);
        NPC_HideSprite(NPC_SAFFRON_SILPH_WORKER_M);
        NPC_HideSprite(NPC_SAFFRON_SILPH_WORKER_F);
        NPC_HideSprite(NPC_SAFFRON_GENTLEMAN);
        NPC_HideSprite(NPC_SAFFRON_BIRD);
        NPC_HideSprite(NPC_SAFFRON_ROCKER);
    }

    if (CheckEvent(EVENT_RESCUED_MR_FUJI)) {
        NPC_HideSprite(NPC_SAFFRON_ROCKET8);
        NPC_ShowSprite(NPC_SAFFRON_ROCKET9);
    } else {
        NPC_ShowSprite(NPC_SAFFRON_ROCKET8);
        NPC_HideSprite(NPC_SAFFRON_ROCKET9);
    }
}

void SaffronCityScripts_Tick(void) {
    switch (sState) {
    case SC_IDLE:
        return;
    case SC_TM29_WAIT_PRETEXT_CLOSE:
        if (Text_IsOpen()) return;
        if (Inventory_Add(ITEM_TM29, 1) != 0) {
            Text_ShowASCII(kMrPsychicNoRoom);
            sState = SC_IDLE;
            return;
        }
        SetEvent(EVENT_GOT_TM29);
        Audio_PlaySFX_GetItem1();
        Text_SetItemName(ITEM_TM29);
        Text_ShowASCII(kMrPsychicReceived);
        sState = SC_IDLE;
        return;
    }
}

int SaffronCityScripts_IsActive(void) {
    return sState != SC_IDLE;
}
