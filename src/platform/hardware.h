#pragma once
#include <stdint.h>
#include "../game/types.h"
#include "../game/constants.h"

extern uint8_t hJoyInput;
extern uint8_t hJoyHeld;
extern uint8_t hJoyPressed;
extern uint8_t hJoyReleased;
extern uint8_t wJoyIgnore;

extern uint8_t hRandomAdd;
extern uint8_t hRandomSub;

extern uint8_t hFrameCounter;
extern uint8_t hVBlankOccurred;

extern uint8_t hSCX;
extern uint8_t hSCY;
extern uint8_t hWY;
extern uint8_t hWX;

extern uint8_t hLoadedROMBank;

extern uint8_t hAutoBGTransferEnabled;
extern uint8_t hTileAnimations;

extern uint32_t hDividend;
extern uint8_t  hDivisor;
extern uint32_t hQuotient;
extern uint8_t  hMultiplicand[3];
extern uint8_t  hMultiplier;
extern uint8_t  hProduct[3];

extern uint8_t hTextID;
extern uint8_t hItemAlreadyFound;

extern uint8_t wTileMap[TILEMAP_WIDTH * TILEMAP_HEIGHT];

extern uint8_t wSurroundingTiles[SURROUNDING_WIDTH * SURROUNDING_HEIGHT];

extern uint8_t wOverworldMap[1300];

extern uint8_t wTileMapBackup[SCREEN_AREA];
extern uint8_t wTileMapBackup2[SCREEN_AREA];

extern oam_entry_t wShadowOAM[MAX_SPRITES];

extern uint8_t gWindowTileMap[SCREEN_HEIGHT][SCREEN_WIDTH];

extern uint8_t  wTilesetBank;
extern uint16_t wTilesetBlocksPtr;
extern uint16_t wTilesetGfxPtr;
extern uint16_t wTilesetCollisionPtr;
extern uint8_t  wTilesetTalkingOverTiles[3];
extern uint8_t  wGrassTile;
extern uint8_t  wCurMapTileset;

extern uint8_t  wCurMap;
extern uint8_t  wLastMap;
extern uint8_t  wLastBlackoutMap;
extern uint8_t  wLastHealTownMap;
extern char     wLastHealTownName[24];
extern int16_t  wYCoord;
extern int16_t  wXCoord;
extern uint8_t  wYBlockCoord;
extern uint8_t  wXBlockCoord;
extern uint16_t wCurrentTileBlockMapViewPointer;
extern uint16_t wMapViewVRAMPointer;

extern uint8_t  wCurMapHeight;
extern uint8_t  wCurMapWidth;
extern uint16_t wCurMapDataPtr;
extern uint16_t wCurMapTextPtr;
extern uint16_t wCurMapScriptPtr;
extern uint8_t  wCurMapConnections;

extern uint8_t wNumberOfWarps;
extern uint8_t wNumSigns;
extern uint8_t wNumSprites;
extern uint8_t wDestinationWarpID;
extern uint8_t wMapBackgroundTile;
extern uint8_t gMapPalOffset;

extern uint8_t wPlayerMovingDirection;
extern uint8_t wPlayerLastStopDirection;
extern uint8_t wPlayerDirection;
extern uint8_t wWalkBikeSurfState;
extern uint8_t wWalkCounter;
extern uint8_t wStepCounter;

extern uint8_t    wPartyCount;
extern party_mon_t wPartyMons[PARTY_LENGTH];
extern uint8_t    wPartyMonOT[PARTY_LENGTH][NAME_LENGTH];
extern uint8_t    wPartyMonNicks[PARTY_LENGTH][NAME_LENGTH];
extern uint8_t    wNumHoFTeams;
extern hall_of_fame_team_t wHallOfFameTeams[HOF_TEAM_CAPACITY];
extern uint8_t    wCurrentBoxNum;
extern uint8_t    wBoxCount[NUM_BOXES];
extern uint8_t    wBoxSpecies[NUM_BOXES][BOX_CAPACITY + 1];
extern box_mon_t  wBoxMons[NUM_BOXES][BOX_CAPACITY];
extern uint8_t    wBoxMonOT[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
extern uint8_t    wBoxMonNicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];

extern uint8_t    wDayCareInUse;
extern box_mon_t  wDayCareMon;
extern uint8_t    wDayCareMonOT[NAME_LENGTH];
extern uint8_t    wDayCareMonName[NAME_LENGTH];

extern uint8_t    wFirstLockTrashCanIndex;
extern uint8_t    wSecondLockTrashCanIndex;

extern uint8_t    wNumBagItems;
extern uint8_t    wBagItems[BAG_ITEM_CAPACITY * 2 + 1];

extern uint8_t    wNumBoxItems;
extern uint8_t    wBoxItems[PC_ITEM_CAPACITY * 2 + 1];

extern uint8_t    wPokeBallAnimData;

extern uint8_t    wPlayerMoney[3];
extern uint8_t    wPlayerCoins[2];
extern uint32_t   wAmountMoneyWon;
extern uint8_t    wPlayerName[NAME_LENGTH];
extern uint8_t    wRivalName[NAME_LENGTH];
extern uint16_t   wPlayerID;
extern uint8_t    wObtainedBadges;

extern uint16_t   wCompletedInGameTradeFlags;
extern uint8_t    wGymLeaderNo;

extern uint8_t wPokedexOwned[19];
extern uint8_t wPokedexSeen[19];

#define PKS_EVENT_FLAGS_BASE 0xA00

#define PKS_EVENT_FLAGS_COUNT 1024

#define NUM_EVENTS          (PKS_EVENT_FLAGS_BASE + PKS_EVENT_FLAGS_COUNT)
#define EVENT_FLAGS_BYTES   ((NUM_EVENTS + 7) / 8)
extern uint8_t wEventFlags[EVENT_FLAGS_BYTES];

#define PKS_HANDAUTHORED_EVENT_COUNT 32
#define PKS_HANDAUTHORED_EVENT_BASE  NUM_EVENTS
#define PKS_HANDAUTHORED_EVENT_BYTES ((PKS_HANDAUTHORED_EVENT_COUNT + 7) / 8)
extern uint8_t wHandAuthoredEventFlags[PKS_HANDAUTHORED_EVENT_BYTES];
void Debug_LogEventFlagChange(uint16_t n, int new_value);

static inline int  CheckEvent(uint16_t n) {
    if (n >= PKS_HANDAUTHORED_EVENT_BASE) {
        uint16_t r = (uint16_t)(n - PKS_HANDAUTHORED_EVENT_BASE);
        return (wHandAuthoredEventFlags[r>>3] >> (r&7)) & 1;
    }
    return (wEventFlags[n>>3] >> (n&7)) & 1;
}
static inline void SetEvent  (uint16_t n) {
    uint8_t mask;
    uint8_t *byte;
    if (n >= PKS_HANDAUTHORED_EVENT_BASE) {
        uint16_t r = (uint16_t)(n - PKS_HANDAUTHORED_EVENT_BASE);
        mask = (uint8_t)(1u << (r & 7));
        byte = &wHandAuthoredEventFlags[r >> 3];
    } else {
        mask = (uint8_t)(1u << (n & 7));
        byte = &wEventFlags[n >> 3];
    }
    if (!(*byte & mask)) {
        *byte |= mask;
        Debug_LogEventFlagChange(n, 1);
    }
}
static inline void ClearEvent(uint16_t n) {
    uint8_t mask;
    uint8_t *byte;
    if (n >= PKS_HANDAUTHORED_EVENT_BASE) {
        uint16_t r = (uint16_t)(n - PKS_HANDAUTHORED_EVENT_BASE);
        mask = (uint8_t)(1u << (r & 7));
        byte = &wHandAuthoredEventFlags[r >> 3];
    } else {
        mask = (uint8_t)(1u << (n & 7));
        byte = &wEventFlags[n >> 3];
    }
    if (*byte & mask) {
        *byte &= (uint8_t)~mask;
        Debug_LogEventFlagChange(n, 0);
    }
}

extern uint16_t wPickedUpItems[248];

extern uint8_t wGrassRate;
extern uint8_t wWaterRate;
extern uint8_t wGrassMons[NUM_WILD_SLOTS * 2];
extern uint8_t wWaterMons[NUM_WILD_SLOTS * 2];

extern uint8_t  wIsInBattle;
extern uint8_t  wBattleType;
extern uint8_t  wCurEnemyLevel;
extern uint8_t  wCurPartySpecies;
extern uint8_t  wEnemyMonSpecies;
extern uint8_t  wCapturedMonSpecies;
extern uint8_t  wEnemyMonActualCatchRate;
extern uint8_t  wSafariBaitFactor;
extern uint8_t  wSafariEscapeFactor;
extern uint8_t  wNumSafariBalls;
extern uint16_t wSafariSteps;
extern uint8_t  wSafariZoneGateCurScript;
extern uint8_t  wNextSafariZoneGateScript;
extern uint8_t  wSafariZoneGameOver;

extern uint8_t  wFossilItem;
extern uint8_t  wFossilMon;
extern uint8_t  wTrainerClass;

extern uint8_t  wLoneAttackNo;

extern uint8_t  wRivalStarter;

extern uint8_t  hWhoseTurn;

extern battle_mon_t wBattleMon;
extern battle_mon_t wEnemyMon;

extern uint8_t  wPlayerMoveNum;
extern uint8_t  wPlayerMoveEffect;
extern uint8_t  wPlayerMovePower;
extern uint8_t  wPlayerMoveType;
extern uint8_t  wPlayerMoveAccuracy;
extern uint8_t  wPlayerMoveMaxPP;

extern uint8_t  wEnemyMoveNum;
extern uint8_t  wEnemyMoveEffect;
extern uint8_t  wEnemyMovePower;
extern uint8_t  wEnemyMoveType;
extern uint8_t  wEnemyMoveAccuracy;
extern uint8_t  wEnemyMoveMaxPP;

extern uint8_t  wMoveType;

extern uint8_t  wPlayerBattleStatus1;
extern uint8_t  wPlayerBattleStatus2;
extern uint8_t  wPlayerBattleStatus3;
extern uint8_t  wEnemyBattleStatus1;
extern uint8_t  wEnemyBattleStatus2;
extern uint8_t  wEnemyBattleStatus3;

extern uint8_t  wPlayerConfusedCounter;
extern uint8_t  wPlayerToxicCounter;
extern uint8_t  wPlayerDisabledMove;
extern uint8_t  wEnemyConfusedCounter;
extern uint8_t  wEnemyToxicCounter;
extern uint8_t  wEnemyDisabledMove;

extern uint8_t  wPlayerMonStatMods[NUM_STAT_MODS];
extern uint8_t  wEnemyMonStatMods[NUM_STAT_MODS];

extern uint8_t  wPlayerMonNumber;

extern uint8_t  wCalculateWhoseStats;

extern uint16_t wPlayerMonUnmodifiedAttack;
extern uint16_t wPlayerMonUnmodifiedDefense;
extern uint16_t wPlayerMonUnmodifiedSpeed;
extern uint16_t wPlayerMonUnmodifiedSpecial;
extern uint16_t wEnemyMonUnmodifiedAttack;
extern uint16_t wEnemyMonUnmodifiedDefense;
extern uint16_t wEnemyMonUnmodifiedSpeed;
extern uint16_t wEnemyMonUnmodifiedSpecial;

extern uint16_t CalcStat(uint8_t base, uint8_t dv, uint16_t stat_exp,
                          uint8_t level, int is_hp);

extern uint8_t  wCriticalHitOrOHKO;
extern uint16_t wDamage;
extern uint8_t  wDamageMultipliers;
extern uint8_t  wMoveMissed;

extern uint8_t  wPlayerMimicChoice;
extern uint8_t  wPlayerSelectedMove;
extern uint8_t  wEnemySelectedMove;
extern uint8_t  wRepelRemainingSteps;
extern uint8_t  wOptions;

extern uint8_t  wActionResultOrTookBattleTurn;

extern uint8_t  wMonIsDisobedient;

extern uint8_t  wInHandlePlayerMonFainted;

extern uint8_t  wPlayerUsedMove;
extern uint8_t  wEnemyUsedMove;

extern uint8_t  wLinkState;

extern uint8_t  wEscapedFromBattle;

extern uint8_t  wMoveDidntMiss;

extern uint8_t  wChargeMoveNum;

extern uint8_t  wPlayerNumAttacksLeft;
extern uint8_t  wEnemyNumAttacksLeft;
extern uint8_t  wPlayerNumHits;
extern uint8_t  wEnemyNumHits;

extern uint16_t wPlayerBideAccumulatedDamage;
extern uint16_t wEnemyBideAccumulatedDamage;

extern uint8_t  wPlayerSubstituteHP;
extern uint8_t  wEnemySubstituteHP;

extern uint8_t  wPlayerMonMinimized;
extern uint8_t  wEnemyMonMinimized;

extern uint8_t  wPlayerDisabledMoveNumber;
extern uint8_t  wEnemyDisabledMoveNumber;

extern uint8_t  wPlayerMoveListIndex;
extern uint8_t  wEnemyMoveListIndex;

extern uint8_t  wTotalPayDayMoney[3];

extern uint16_t wTransformedEnemyMonOriginalDVs;

extern uint8_t  wFirstMonsNotOutYet;

extern uint8_t  wBattleResult;

extern uint8_t     wEnemyPartyCount;

extern party_mon_t wEnemyMons[PARTY_LENGTH];

extern uint8_t     wEnemyMonPartyPos;

extern uint8_t     wNumRunAttempts;

extern uint8_t     wForcePlayerToChooseMon;

extern uint8_t     wAICount;

extern uint8_t     wAILayer2Encouragement;

extern uint16_t    wLastSwitchInEnemyMonHP;

extern uint8_t  wPartySpecies[PARTY_LENGTH + 1];

extern uint8_t  wPartyGainExpFlags;

extern uint8_t  wPartyFoughtCurrentEnemyFlags;

extern uint8_t  wWhichPokemon;

extern uint16_t wExpAmountGained;

extern uint8_t  wGainBoostedExp;

extern uint8_t  wBoostExpByExpAll;

extern uint8_t  wCurSpecies;

extern uint8_t  wCanEvolveFlags;

extern uint8_t  wEvolutionOccurred;

extern uint8_t  wEvoOldSpecies;
extern uint8_t  wEvoNewSpecies;

extern uint8_t  wForceEvolution;

extern uint8_t  wAudioROMBank;
extern uint16_t wMusicTempo;
extern uint16_t wSfxTempo;
extern uint8_t  wChannelCommandPointers[NUM_CHANNELS * 2];
extern uint8_t  wChannelReturnAddresses[NUM_CHANNELS * 2];
extern uint8_t  wChannelSoundIDs[NUM_CHANNELS];
extern uint8_t  wChannelFlags1[NUM_CHANNELS];
extern uint8_t  wChannelFlags2[NUM_CHANNELS];
extern uint8_t  wChannelDutyCycles[NUM_CHANNELS];
extern uint8_t  wChannelOctaves[NUM_CHANNELS];
extern uint8_t  wChannelNoteDelayCounters[NUM_CHANNELS];
extern uint8_t  wChannelNoteSpeeds[NUM_CHANNELS];
extern uint8_t  wChannelVolumes[NUM_CHANNELS];

static inline uint32_t exp_to_u32(const uint8_t e[3]) {
    return ((uint32_t)e[0] << 16) | ((uint32_t)e[1] << 8) | e[2];
}
static inline void u32_to_exp(uint32_t v, uint8_t e[3]) {
    e[0] = (v >> 16) & 0xFF;
    e[1] = (v >>  8) & 0xFF;
    e[2] =  v        & 0xFF;
}

uint16_t CalcStat(uint8_t base, uint8_t dv, uint16_t stat_exp, uint8_t level, int is_hp);

uint16_t CalcDamage(uint8_t level, uint8_t bp, uint8_t attack, uint8_t defense);

uint16_t ModifyStat(uint16_t base, uint8_t stage);

uint8_t CalcCheckSum(const uint8_t *data, uint16_t len);

uint8_t BattleRandom(void);

void Random_FrameTick(void);

uint64_t Random_GetDivCycles(void);
void     Random_SetDivCycles(uint64_t cycles);
