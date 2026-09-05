
#include "../platform/hardware.h"
#include "../data/event_flag_names.h"
#include "session_log.h"
#include "debug_trace.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

uint8_t hJoyInput       = 0;
uint8_t hJoyHeld        = 0;
uint8_t hJoyPressed     = 0;
uint8_t hJoyReleased    = 0;
uint8_t wJoyIgnore      = 0;
uint8_t hRandomAdd      = 0;
uint8_t hRandomSub      = 0xFF;
uint8_t hFrameCounter   = 0;
uint8_t hVBlankOccurred = 0;
uint8_t hSCX            = 0;
uint8_t hSCY            = 0;
uint8_t hWY             = 0;
uint8_t hWX             = 7;
uint8_t hLoadedROMBank  = 1;
uint8_t hAutoBGTransferEnabled = 0;
uint8_t hTileAnimations = 0;
uint32_t hDividend      = 0;
uint8_t  hDivisor       = 0;
uint32_t hQuotient      = 0;
uint8_t  hMultiplicand[3] = {0};
uint8_t  hMultiplier    = 0;
uint8_t  hProduct[3]    = {0};
uint8_t  hTextID        = 0;
uint8_t  hItemAlreadyFound = 0;

uint8_t    wTileMap[TILEMAP_WIDTH * TILEMAP_HEIGHT]             = {0};
uint8_t    wSurroundingTiles[SURROUNDING_WIDTH * SURROUNDING_HEIGHT] = {0};
uint8_t    wOverworldMap[1300]                                  = {0};
uint8_t    wTileMapBackup[SCREEN_AREA]                          = {0};
uint8_t    wTileMapBackup2[SCREEN_AREA]                         = {0};
oam_entry_t wShadowOAM[MAX_SPRITES]                            = {0};
uint8_t     gWindowTileMap[SCREEN_HEIGHT][SCREEN_WIDTH]        = {0};

uint8_t  wTilesetBank               = 0;
uint16_t wTilesetBlocksPtr          = 0;
uint16_t wTilesetGfxPtr             = 0;
uint16_t wTilesetCollisionPtr       = 0;
uint8_t  wTilesetTalkingOverTiles[3]= {0xFF, 0xFF, 0xFF};
uint8_t  wGrassTile                 = 0xFF;
uint8_t  wCurMapTileset             = 0;

uint8_t  wCurMap                    = 0;
uint8_t  wLastMap                   = 0;
uint8_t  wLastBlackoutMap           = 0xFF;

uint8_t  wLastHealTownMap           = 0x00;

char     wLastHealTownName[24]      = "";
int16_t  wYCoord                    = 0;
int16_t  wXCoord                    = 0;
uint8_t  wYBlockCoord               = 0;
uint8_t  wXBlockCoord               = 0;
uint16_t wCurrentTileBlockMapViewPointer = 0;
uint16_t wMapViewVRAMPointer        = 0;
uint8_t  wCurMapHeight              = 0;
uint8_t  wCurMapWidth               = 0;
uint16_t wCurMapDataPtr             = 0;
uint16_t wCurMapTextPtr             = 0;
uint16_t wCurMapScriptPtr           = 0;
uint8_t  wCurMapConnections         = 0;
uint8_t  wNumberOfWarps             = 0;
uint8_t  wNumSigns                  = 0;
uint8_t  wNumSprites                = 0;
uint8_t  wDestinationWarpID         = 0xFF;
uint8_t  wMapBackgroundTile         = 0;
uint8_t  gMapPalOffset              = 0;

uint8_t wPlayerMovingDirection      = 0;
uint8_t wPlayerLastStopDirection    = 0;
uint8_t wPlayerDirection            = 0;
uint8_t wWalkBikeSurfState          = 0;
uint8_t wWalkCounter                = 0;
uint8_t wStepCounter                = 0;

uint8_t     wPartyCount             = 0;
party_mon_t wPartyMons[PARTY_LENGTH];
uint8_t     wPartyMonOT[PARTY_LENGTH][NAME_LENGTH];
uint8_t     wPartyMonNicks[PARTY_LENGTH][NAME_LENGTH];
uint8_t     wNumHoFTeams          = 0;
hall_of_fame_team_t wHallOfFameTeams[HOF_TEAM_CAPACITY];
uint8_t     wCurrentBoxNum          = 0;
uint8_t     wBoxCount[NUM_BOXES]    = {0};
uint8_t     wBoxSpecies[NUM_BOXES][BOX_CAPACITY + 1] = {{0xFF}};
box_mon_t   wBoxMons[NUM_BOXES][BOX_CAPACITY];
uint8_t     wBoxMonOT[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
uint8_t     wBoxMonNicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];

uint8_t     wDayCareInUse           = 0;
box_mon_t   wDayCareMon;
uint8_t     wDayCareMonOT[NAME_LENGTH];
uint8_t     wDayCareMonName[NAME_LENGTH];

uint8_t     wFirstLockTrashCanIndex  = 0;
uint8_t     wSecondLockTrashCanIndex = 0;
uint8_t     wNumBagItems            = 0;
uint8_t     wBagItems[BAG_ITEM_CAPACITY * 2 + 1];
uint8_t     wNumBoxItems            = 0;
uint8_t     wBoxItems[PC_ITEM_CAPACITY * 2 + 1] = { 0xFF };
uint8_t     wPokeBallAnimData       = 0;
uint8_t     wPlayerMoney[3]         = {0};
uint8_t     wPlayerCoins[2]         = {0};
uint32_t    wAmountMoneyWon         = 0;
uint8_t     wPlayerName[NAME_LENGTH]= {0x91,0x84,0x83,0x50};
uint8_t     wRivalName[NAME_LENGTH] = {0};
uint16_t    wPlayerID               = 0;
uint8_t     wObtainedBadges         = 0;
uint16_t    wCompletedInGameTradeFlags = 0;
uint8_t     wGymLeaderNo            = 0;

uint8_t wPokedexOwned[19]           = {0};
uint8_t wPokedexSeen[19]            = {0};

uint8_t wEventFlags[EVENT_FLAGS_BYTES] = {0};
uint8_t wHandAuthoredEventFlags[PKS_HANDAUTHORED_EVENT_BYTES] = {0};

uint16_t wPickedUpItems[248] = {0};

uint8_t wGrassRate = 0;
uint8_t wWaterRate = 0;
uint8_t wGrassMons[NUM_WILD_SLOTS * 2] = {0};
uint8_t wWaterMons[NUM_WILD_SLOTS * 2] = {0};

uint8_t  wPlayerMonNumber       = 0;
uint8_t  wCalculateWhoseStats   = 0;

uint16_t wPlayerMonUnmodifiedAttack  = 0;
uint16_t wPlayerMonUnmodifiedDefense = 0;
uint16_t wPlayerMonUnmodifiedSpeed   = 0;
uint16_t wPlayerMonUnmodifiedSpecial = 0;
uint16_t wEnemyMonUnmodifiedAttack   = 0;
uint16_t wEnemyMonUnmodifiedDefense  = 0;
uint16_t wEnemyMonUnmodifiedSpeed    = 0;
uint16_t wEnemyMonUnmodifiedSpecial  = 0;

uint8_t  wIsInBattle            = 0;
uint8_t  wBattleType            = 0;
uint8_t  wCurEnemyLevel         = 0;
uint8_t  wCurPartySpecies       = 0;
uint8_t  wEnemyMonSpecies       = 0;
uint8_t  wCapturedMonSpecies    = 0;
uint8_t  wEnemyMonActualCatchRate = 0;
uint8_t  wSafariBaitFactor      = 0;
uint8_t  wSafariEscapeFactor    = 0;
uint8_t  wNumSafariBalls        = 0;
uint16_t wSafariSteps           = 0;
uint8_t  wSafariZoneGateCurScript = 0;
uint8_t  wNextSafariZoneGateScript = 0;
uint8_t  wSafariZoneGameOver    = 0;
uint8_t  wTrainerClass          = 0;
uint8_t  wLoneAttackNo          = 0;
uint8_t  wRivalStarter          = 0;

uint8_t  wFossilItem            = 0;
uint8_t  wFossilMon             = 0;

uint8_t  hWhoseTurn             = 0;

battle_mon_t wBattleMon         = {0};
battle_mon_t wEnemyMon          = {0};

uint8_t  wPlayerMoveNum         = 0;
uint8_t  wPlayerMoveEffect      = 0;
uint8_t  wPlayerMovePower       = 0;
uint8_t  wPlayerMoveType        = 0;
uint8_t  wPlayerMoveAccuracy    = 0;
uint8_t  wPlayerMoveMaxPP       = 0;

uint8_t  wMoveType              = 0;

uint8_t  wEnemyMoveNum          = 0;
uint8_t  wEnemyMoveEffect       = 0;
uint8_t  wEnemyMovePower        = 0;
uint8_t  wEnemyMoveType         = 0;
uint8_t  wEnemyMoveAccuracy     = 0;
uint8_t  wEnemyMoveMaxPP        = 0;

uint8_t  wPlayerBattleStatus1   = 0;
uint8_t  wPlayerBattleStatus2   = 0;
uint8_t  wPlayerBattleStatus3   = 0;
uint8_t  wEnemyBattleStatus1    = 0;
uint8_t  wEnemyBattleStatus2    = 0;
uint8_t  wEnemyBattleStatus3    = 0;

uint8_t  wPlayerConfusedCounter = 0;
uint8_t  wPlayerToxicCounter    = 0;
uint8_t  wPlayerDisabledMove    = 0;
uint8_t  wEnemyConfusedCounter  = 0;
uint8_t  wEnemyToxicCounter     = 0;
uint8_t  wEnemyDisabledMove     = 0;

uint8_t  wPlayerMonStatMods[NUM_STAT_MODS] = {7,7,7,7,7,7};
uint8_t  wEnemyMonStatMods[NUM_STAT_MODS]  = {7,7,7,7,7,7};
uint8_t  wCriticalHitOrOHKO     = 0;
uint16_t wDamage                = 0;
uint8_t  wDamageMultipliers     = 0;
uint8_t  wMoveMissed            = 0;

uint8_t  wPlayerMimicChoice     = 0;
uint8_t  wPlayerSelectedMove    = 0;
uint8_t  wEnemySelectedMove     = 0;
uint8_t  wRepelRemainingSteps   = 0;
uint8_t  wOptions               = 0;

uint8_t  wActionResultOrTookBattleTurn = 0;
uint8_t  wMonIsDisobedient      = 0;
uint8_t  wInHandlePlayerMonFainted = 0;
uint8_t  wPlayerUsedMove        = 0;
uint8_t  wEnemyUsedMove         = 0;
uint8_t  wLinkState             = 0;
uint8_t  wEscapedFromBattle     = 0;
uint8_t  wMoveDidntMiss         = 0;
uint8_t  wChargeMoveNum         = 0;
uint8_t  wPlayerNumAttacksLeft  = 0;
uint8_t  wEnemyNumAttacksLeft   = 0;
uint8_t  wPlayerNumHits         = 0;
uint8_t  wEnemyNumHits          = 0;
uint16_t wPlayerBideAccumulatedDamage = 0;
uint16_t wEnemyBideAccumulatedDamage  = 0;
uint8_t  wPlayerSubstituteHP    = 0;
uint8_t  wEnemySubstituteHP     = 0;
uint8_t  wPlayerMonMinimized    = 0;
uint8_t  wEnemyMonMinimized     = 0;
uint8_t  wPlayerDisabledMoveNumber = 0;
uint8_t  wEnemyDisabledMoveNumber  = 0;
uint8_t  wPlayerMoveListIndex   = 0;
uint8_t  wEnemyMoveListIndex    = 0;
uint8_t  wTotalPayDayMoney[3]   = {0,0,0};
uint16_t wTransformedEnemyMonOriginalDVs = 0;

uint8_t  wFirstMonsNotOutYet    = 0;
uint8_t  wBattleResult          = 0;

uint8_t     wEnemyPartyCount      = 0;
party_mon_t wEnemyMons[PARTY_LENGTH];
uint8_t     wEnemyMonPartyPos     = 0;
uint8_t     wNumRunAttempts       = 0;
uint8_t     wForcePlayerToChooseMon = 0;
uint8_t     wAICount              = 0;
uint8_t     wAILayer2Encouragement = 0;
uint16_t    wLastSwitchInEnemyMonHP = 0;

uint8_t  wPartySpecies[PARTY_LENGTH + 1] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
uint8_t  wPartyGainExpFlags             = 0;
uint8_t  wPartyFoughtCurrentEnemyFlags  = 0;
uint8_t  wWhichPokemon                  = 0;
uint16_t wExpAmountGained               = 0;
uint8_t  wGainBoostedExp                = 0;
uint8_t  wBoostExpByExpAll              = 0;
uint8_t  wCurSpecies                    = 0;

uint8_t  wCanEvolveFlags    = 0;
uint8_t  wEvolutionOccurred = 0;
uint8_t  wEvoOldSpecies     = 0;
uint8_t  wEvoNewSpecies     = 0;
uint8_t  wForceEvolution    = 0;

uint8_t  wAudioROMBank          = 0;
uint16_t wMusicTempo            = 0;
uint16_t wSfxTempo              = 0;
uint8_t  wChannelCommandPointers[NUM_CHANNELS * 2]  = {0};
uint8_t  wChannelReturnAddresses[NUM_CHANNELS * 2]  = {0};
uint8_t  wChannelSoundIDs[NUM_CHANNELS]             = {0};
uint8_t  wChannelFlags1[NUM_CHANNELS]               = {0};
uint8_t  wChannelFlags2[NUM_CHANNELS]               = {0};
uint8_t  wChannelDutyCycles[NUM_CHANNELS]           = {0};
uint8_t  wChannelOctaves[NUM_CHANNELS]              = {0};
uint8_t  wChannelNoteDelayCounters[NUM_CHANNELS]    = {0};
uint8_t  wChannelNoteSpeeds[NUM_CHANNELS]           = {0};
uint8_t  wChannelVolumes[NUM_CHANNELS]              = {0};

#include <math.h>

uint16_t CalcStat(uint8_t base, uint8_t dv, uint16_t stat_exp, uint8_t level, int is_hp) {

    uint32_t ce = 0;
    do {
        ce++;
    } while (ce != 0xFF && ce * ce < (uint32_t)stat_exp);

    uint32_t s = (uint32_t)(base + dv) * 2 + (ce >> 2);
    s = s * level / 100 + (is_hp ? (level + 10) : 5);
    return (uint16_t)(s > 65535 ? 65535 : s);
}

uint16_t CalcDamage(uint8_t level, uint8_t bp, uint8_t attack, uint8_t defense) {
    if (!bp) return 0;
    uint32_t d = (uint32_t)level * 2 / 5 + 2;
    d = d * bp * attack / (defense ? defense : 1) / 50 + 2;
    if (d > 997) d = 997;
    return (uint16_t)d;
}

uint16_t ModifyStat(uint16_t base, uint8_t stage) {
    if (stage < 1)  stage = 1;
    if (stage > 13) stage = 13;
    return (uint32_t)base * STAT_MOD_NUM[stage-1] / STAT_MOD_DEN[stage-1];
}

uint8_t CalcCheckSum(const uint8_t *data, uint16_t len) {
    uint8_t sum = 0;
    while (len--) sum += *data++;
    return ~sum;
}

void Debug_LogEventFlagChange(uint16_t n, int new_value) {
    SessionLog_EventFlagChange(n, new_value, EventFlagName(n));
    printf("[event] %s (#%u) -> %d map=%u pos=(%d,%d)\n",
           EventFlagName(n),
           (unsigned)n,
           new_value ? 1 : 0,
           (unsigned)wCurMap,
           (int)wXCoord,
           (int)wYCoord);
    Trace_Emit(TRACE_FLAG, "\"flag\":%u,\"name\":\"%s\",\"v\":%d,"
               "\"map\":%u,\"x\":%d,\"y\":%d",
               (unsigned)n, EventFlagName(n), new_value ? 1 : 0,
               (unsigned)wCurMap, (int)wXCoord, (int)wYCoord);
}

#define GB_CYCLES_PER_FRAME    70224ull
#define GB_CYCLES_PER_DIV_READ 20ull
static uint64_t s_div_cycles = 0;

static uint8_t read_div_approx(void) {
    uint8_t v = (uint8_t)((s_div_cycles >> 8) & 0xFFu);
    s_div_cycles += GB_CYCLES_PER_DIV_READ;
    return v;
}

uint64_t Random_GetDivCycles(void)          { return s_div_cycles; }
void     Random_SetDivCycles(uint64_t c)    { s_div_cycles = c; }

void Random_FrameTick(void) {
    s_div_cycles += GB_CYCLES_PER_FRAME;
    BattleRandom();
}

uint8_t (*gBattleRandomHook)(void) = 0;

void (*gBattleProbeHook)(const char *routine) = 0;

uint8_t BattleRandom(void) {
    if (gBattleRandomHook) return gBattleRandomHook();

    uint8_t div1 = read_div_approx();
    uint16_t add = (uint16_t)hRandomAdd + (uint16_t)div1;
    hRandomAdd = (uint8_t)add;
    uint8_t carry = (add > 0xFFu) ? 1u : 0u;

    uint8_t div2 = read_div_approx();
    uint16_t sub = (uint16_t)hRandomSub - (uint16_t)div2 - (uint16_t)carry;
    hRandomSub = (uint8_t)sub;

    return hRandomAdd;
}

void WRAMClear(void) {
    memset(wTileMap, 0, sizeof(wTileMap));
    memset(wShadowOAM, 0, sizeof(wShadowOAM));
    memset(wEventFlags, 0, sizeof(wEventFlags));
    memset(wPickedUpItems, 0, sizeof(wPickedUpItems));
    wPartyCount     = 0;
    wNumHoFTeams    = 0;
    memset(wHallOfFameTeams, 0, sizeof(wHallOfFameTeams));
    wCurrentBoxNum  = 0;
    memset(wBoxCount, 0, sizeof(wBoxCount));
    memset(wBoxSpecies, 0xFF, sizeof(wBoxSpecies));
    memset(wBoxMons, 0, sizeof(wBoxMons));
    memset(wBoxMonOT, 0, sizeof(wBoxMonOT));
    memset(wBoxMonNicks, 0, sizeof(wBoxMonNicks));
    wNumBagItems    = 0;
    wPokeBallAnimData = 0;
    wObtainedBadges = 0;
    wCompletedInGameTradeFlags = 0;
    memset(wPokedexOwned, 0, sizeof(wPokedexOwned));
    memset(wPokedexSeen,  0, sizeof(wPokedexSeen));
    wPlayerMoney[0] = wPlayerMoney[1] = wPlayerMoney[2] = 0;
    memset(wPlayerMonStatMods, STAT_STAGE_NORMAL, sizeof(wPlayerMonStatMods));
    memset(wEnemyMonStatMods,  STAT_STAGE_NORMAL, sizeof(wEnemyMonStatMods));
    memset(&wBattleMon, 0, sizeof(wBattleMon));
    memset(&wEnemyMon,  0, sizeof(wEnemyMon));
    wPlayerBattleStatus1 = wPlayerBattleStatus2 = wPlayerBattleStatus3 = 0;
    wEnemyBattleStatus1  = wEnemyBattleStatus2  = wEnemyBattleStatus3  = 0;
    wPlayerMonNumber = 0;
    wCalculateWhoseStats = 0;
    wPlayerMonUnmodifiedAttack = wPlayerMonUnmodifiedDefense = 0;
    wPlayerMonUnmodifiedSpeed  = wPlayerMonUnmodifiedSpecial = 0;
    wEnemyMonUnmodifiedAttack  = wEnemyMonUnmodifiedDefense  = 0;
    wEnemyMonUnmodifiedSpeed   = wEnemyMonUnmodifiedSpecial  = 0;
    wMoveType = 0;
    wDamageMultipliers = 0;
    wLinkState = 0;
    wEscapedFromBattle = 0;
    wMoveDidntMiss = 0;
    wChargeMoveNum = 0;
    wPlayerNumAttacksLeft = wEnemyNumAttacksLeft = 0;
    wPlayerNumHits = wEnemyNumHits = 0;
    wPlayerBideAccumulatedDamage = wEnemyBideAccumulatedDamage = 0;
    wPlayerSubstituteHP = wEnemySubstituteHP = 0;
    wPlayerMonMinimized = wEnemyMonMinimized = 0;
    wPlayerDisabledMoveNumber = wEnemyDisabledMoveNumber = 0;
    wPlayerMoveListIndex = wEnemyMoveListIndex = 0;
    memset(wTotalPayDayMoney, 0, sizeof(wTotalPayDayMoney));
    wTransformedEnemyMonOriginalDVs = 0;
    wDamage = 0;
    wMoveMissed = 0;
    wCriticalHitOrOHKO = 0;
    wOptions = 0;
    wIsInBattle = 0;
    wBattleType = 0;
    wLoneAttackNo = 0;
    wRivalStarter = 0;
    wBattleResult = 0;
    wFirstMonsNotOutYet = 0;
    wActionResultOrTookBattleTurn = 0;
    wInHandlePlayerMonFainted = 0;
    wEscapedFromBattle = 0;
    wMonIsDisobedient = 0;
    wPlayerUsedMove = wEnemyUsedMove = 0;
    wSafariSteps = 0;
    wFossilItem = 0;
    wFossilMon = 0;
    wSafariZoneGateCurScript = 0;
    wNextSafariZoneGateScript = 0;
    wSafariZoneGameOver = 0;
    wPartyGainExpFlags = 0;
    wPartyFoughtCurrentEnemyFlags = 0;
    wCanEvolveFlags = wEvolutionOccurred = 0;
    wEvoOldSpecies = wEvoNewSpecies = wForceEvolution = 0;
    memset(wPartyMons, 0, sizeof(wPartyMons));
    memset(wPartySpecies, 0xFF, sizeof(wPartySpecies));

    wEnemyPartyCount = 0;
    memset(wEnemyMons, 0, sizeof(wEnemyMons));
    wEnemyMonPartyPos = 0;
    wNumRunAttempts = 0;
    wForcePlayerToChooseMon = 0;
    wAICount = 0;
    wAILayer2Encouragement = 0;
    wLastSwitchInEnemyMonHP = 0;

    wWalkBikeSurfState = 0;
    wPlayerCoins[0] = wPlayerCoins[1] = 0;
    wNumBoxItems = 0;
    memset(wBoxItems, 0, sizeof(wBoxItems));
    wBoxItems[0] = 0xFF;
    wDayCareInUse = 0;
    memset(wDayCareMonName, 0, sizeof(wDayCareMonName));
    memset(wDayCareMonOT,   0, sizeof(wDayCareMonOT));
    wFirstLockTrashCanIndex = wSecondLockTrashCanIndex = 0;
    memset(wHandAuthoredEventFlags, 0, sizeof(wHandAuthoredEventFlags));
    wNumSafariBalls = 0;
    wLastBlackoutMap = 0xFF;
    wLastHealTownMap = 0x00;
    memset(wLastHealTownName, 0, sizeof(wLastHealTownName));
}
