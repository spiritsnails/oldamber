#pragma once

#include <stdint.h>
#include <stdio.h>
#include "../../platform/hardware.h"
#include "../../data/moves_data.h"
#include "../../data/base_stats.h"
#include "../pokemon.h"
#include "../gen2_species.h"

extern void (*gCombatLogSink)(const char *line);
#define BLOG(fmt, ...) do { \
    char _b[512]; \
    snprintf(_b, sizeof(_b), "[BATTLE] " fmt, ##__VA_ARGS__); \
    fprintf(stderr, "%s\n", _b); \
    if (gCombatLogSink) gCombatLogSink(_b); \
} while(0)
#define BMON_P()  Pokemon_GetName(Species_Dex(wBattleMon.species))
#define BMON_E()  Pokemon_GetName(Species_Dex(wEnemyMon.species))
#define BMOVE(id) ((unsigned)(id) < NUM_MOVE_DEFS && gMoveNames[(id)] ? gMoveNames[(id)] : "???")

extern const uint8_t kBattleStatModRatios[13][2];

int Battle_CalcDamage(uint8_t attack, uint8_t defense, uint8_t power, uint8_t level);

void Battle_CriticalHitTest(void);

void Battle_RandomizeDamage(void);

void Battle_MoveHitTest(void);

void Battle_CalculateModifiedStats(void);

void Battle_RecalculateStat(uint8_t stat_idx);

void Battle_ApplyBurnAndParalysisPenalties(void);

int Battle_GetDamageVarsForPlayerAttack(void);

int Battle_GetDamageVarsForEnemyAttack(void);

void Battle_AdjustDamageForMoveType(void);

void Battle_CalcHitChance(void);

typedef struct battle_hittrace_t {
    uint32_t seq;
    uint8_t enabled;
    uint8_t player_turn;
    uint8_t move_num;
    uint8_t move_effect;
    uint8_t base_acc;
    uint8_t scaled_acc;
    uint8_t roll;
    uint8_t missed;
    uint8_t reason;
} battle_hittrace_t;

enum {
    BHTR_HIT = 0,
    BHTR_MISS_DREAM_EATER = 1,
    BHTR_HIT_SWIFT = 2,
    BHTR_MISS_INVULNERABLE = 3,
    BHTR_MISS_MIST = 4,
    BHTR_HIT_XACCURACY = 5,
    BHTR_MISS_ACCURACY_ROLL = 6
};

void Battle_HitTraceEnable(uint8_t enable);
uint8_t Battle_HitTraceIsEnabled(void);
void Battle_HitTraceReset(void);
battle_hittrace_t Battle_GetLastHitTrace(void);
