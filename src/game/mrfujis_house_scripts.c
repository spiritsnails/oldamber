#include "mrfujis_house_scripts.h"
#include "rom_text.h"
#include "text.h"
#include "npc.h"
#include "inventory.h"
#include "../platform/audio.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"

#define MAP_MR_FUJIS_HOUSE 0x95
#define NPC_MR_FUJI        4

#define kSuperNerdNotHere (RomText("MrFujisHouseSuperNerdText.MrFujiIsntHereText"))
#define kSuperNerdPraying (RomText("MrFujisHouseSuperNerdText.MrFujiHadBeenPrayingText"))

#define kLittleGirlHouse (RomText("MrFujisHouseLittleGirlText.ThisIsMrFujisHouseText"))
#define kLittleGirlHug (RomText("MrFujisHouseLittleGirlText.PokemonAreNiceToHugText"))

#define kFujiIntro (RomText("MrFujisHouseMrFujiText.IThinkThisMayHelpYourQuestText"))

#define kFujiReceived (RomText("MrFujisHouseMrFujiText.ReceivedPokeFluteText"))
#define kFujiNoRoom (RomText("MrFujisHouseMrFujiText.PokeFluteNoRoomText"))
#define kFujiAfter (RomText("MrFujisHouseMrFujiText.HasMyFluteHelpedYouText"))

void MrFujisHouseScripts_OnMapLoad(void) {
    if (wCurMap != MAP_MR_FUJIS_HOUSE) return;
    if (CheckEvent(EVENT_RESCUED_MR_FUJI)) NPC_ShowSprite(NPC_MR_FUJI);
    else NPC_HideSprite(NPC_MR_FUJI);
}

void MrFujisHouse_SuperNerdScript(void) {
    if (CheckEvent(EVENT_RESCUED_MR_FUJI)) Text_ShowASCII(kSuperNerdPraying);
    else Text_ShowASCII(kSuperNerdNotHere);
}

void MrFujisHouse_LittleGirlScript(void) {
    if (CheckEvent(EVENT_RESCUED_MR_FUJI)) Text_ShowASCII(kLittleGirlHug);
    else Text_ShowASCII(kLittleGirlHouse);
}
