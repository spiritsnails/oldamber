
#include "test_runner.h"
#include "../src/game/battle/battle_ai.h"
#include "../src/platform/hardware.h"
#include "../src/game/constants.h"

#define POUND         1
#define SWORDS_DANCE 14
#define VINE_WHIP    22
#define THUNDER_WAVE 86

static void ai_reset(void) {
    extern void WRAMClear(void);
    WRAMClear();
    wIsInBattle            = 2;
    wEnemyDisabledMove     = 0;
    wAILayer2Encouragement = 0;
    wBattleMon.status      = 0;
    wBattleMon.type1       = TYPE_NORMAL;
    wBattleMon.type2       = TYPE_NORMAL;
    wEnemyMon.moves[0] = POUND;
    wEnemyMon.moves[1] = POUND;
    wEnemyMon.moves[2] = POUND;
    wEnemyMon.moves[3] = POUND;
}

TEST(AI, EmptyClassReturnsOriginalMoves) {
    ai_reset();
    wTrainerClass = 1;
    wEnemyMon.moves[1] = VINE_WHIP;

    uint8_t buf[4] = {9, 9, 9, 9};
    const uint8_t *r = AI_EnemyTrainerChooseMoves(buf);

    EXPECT_TRUE(r == wEnemyMon.moves);
    EXPECT_EQ((int)r[0], POUND);
    EXPECT_EQ((int)r[1], VINE_WHIP);
    EXPECT_EQ((int)buf[0], 9);
}

TEST(AI, Mod3EncouragesSuperEffective) {
    ai_reset();
    wTrainerClass = 4;
    wBattleMon.type1 = TYPE_WATER;
    wBattleMon.type2 = TYPE_WATER;
    wEnemyMon.moves[1] = VINE_WHIP;

    uint8_t buf[4];
    const uint8_t *r = AI_EnemyTrainerChooseMoves(buf);

    EXPECT_TRUE(r == buf);
    EXPECT_EQ((int)r[0], 0);
    EXPECT_EQ((int)r[1], VINE_WHIP);
    EXPECT_EQ((int)r[2], 0);
    EXPECT_EQ((int)r[3], 0);
}

TEST(AI, Mod1DiscouragesStatusMoveWhenPlayerStatused) {
    ai_reset();
    wTrainerClass = 2;
    wBattleMon.status = STATUS_PAR;
    wEnemyMon.moves[1] = THUNDER_WAVE;

    uint8_t buf[4];
    const uint8_t *r = AI_EnemyTrainerChooseMoves(buf);

    EXPECT_TRUE(r == buf);
    EXPECT_EQ((int)r[0], POUND);
    EXPECT_EQ((int)r[1], 0);
    EXPECT_EQ((int)r[2], POUND);
    EXPECT_EQ((int)r[3], POUND);
}

TEST(AI, Mod1NoOpWhenPlayerHealthy) {
    ai_reset();
    wTrainerClass = 2;
    wBattleMon.status = 0;
    wEnemyMon.moves[1] = THUNDER_WAVE;

    uint8_t buf[4];
    const uint8_t *r = AI_EnemyTrainerChooseMoves(buf);

    EXPECT_EQ((int)r[0], POUND);
    EXPECT_EQ((int)r[1], THUNDER_WAVE);
    EXPECT_EQ((int)r[2], POUND);
    EXPECT_EQ((int)r[3], POUND);
}

TEST(AI, Mod2GatedOffWhenCounterZero) {
    ai_reset();
    wTrainerClass = 8;
    wAILayer2Encouragement = 0;
    wEnemyMon.moves[1] = SWORDS_DANCE;

    uint8_t buf[4];
    const uint8_t *r = AI_EnemyTrainerChooseMoves(buf);

    EXPECT_EQ((int)r[0], POUND);
    EXPECT_EQ((int)r[1], SWORDS_DANCE);
    EXPECT_EQ((int)r[2], POUND);
    EXPECT_EQ((int)r[3], POUND);
}

TEST(AI, Mod2EncouragesStatMoveWhenCounterOne) {
    ai_reset();
    wTrainerClass = 8;
    wAILayer2Encouragement = 1;
    wEnemyMon.moves[1] = SWORDS_DANCE;

    uint8_t buf[4];
    const uint8_t *r = AI_EnemyTrainerChooseMoves(buf);

    EXPECT_TRUE(r == buf);
    EXPECT_EQ((int)r[0], 0);
    EXPECT_EQ((int)r[1], SWORDS_DANCE);
    EXPECT_EQ((int)r[2], 0);
    EXPECT_EQ((int)r[3], 0);
}

TEST(AI, DisabledMoveIsFilteredOut) {
    ai_reset();
    wTrainerClass = 2;
    wEnemyMon.moves[1] = VINE_WHIP;
    wEnemyDisabledMove = 0x30;

    uint8_t buf[4];
    const uint8_t *r = AI_EnemyTrainerChooseMoves(buf);

    EXPECT_TRUE(r == buf);
    EXPECT_EQ((int)r[0], POUND);
    EXPECT_EQ((int)r[1], VINE_WHIP);
    EXPECT_EQ((int)r[2], 0);
    EXPECT_EQ((int)r[3], POUND);
}

extern uint8_t (*gBattleRandomHook)(void);
static uint8_t s_ai_rand;
static uint8_t ai_rand_hook(void) { return s_ai_rand; }

#define CLS_YOUNGSTER  1
#define CLS_JUGGLER   21
#define CLS_BLACKBELT 24
#define CLS_BROCK     34
#define CLS_ERIKA     37
#define CLS_LANCE     47

static void trainer_ai_reset(uint8_t rand) {
    extern void WRAMClear(void);
    WRAMClear();
    gBattleRandomHook = ai_rand_hook;
    s_ai_rand = rand;
    wIsInBattle       = 2;
    wAICount          = 0xFF;
    wEnemyPartyCount  = 1;
    wEnemyMonPartyPos = 0;
    wEnemyMon.hp = 100; wEnemyMon.max_hp = 100;
    wEnemyMon.status = 0; wEnemyMon.party_pos = 0;
    wEnemyMons[0].base.hp = 100; wEnemyMons[0].base.status = 0;
}

static void trainer_ai_teardown(void) { gBattleRandomHook = 0; }

TEST(TrainerAI, ExhaustedBudgetIsNoOp) {
    trainer_ai_reset(0);
    wTrainerClass = CLS_ERIKA;
    wAICount = 0;
    wEnemyMon.hp = 1;
    EXPECT_EQ(AI_TrainerAI(), 0);
    EXPECT_EQ((int)wEnemyMon.hp, 1);
    trainer_ai_teardown();
}

TEST(TrainerAI, WildBattleIsNoOp) {
    trainer_ai_reset(0);
    wIsInBattle = 1;
    wTrainerClass = CLS_ERIKA;
    wEnemyMon.hp = 1;
    EXPECT_EQ(AI_TrainerAI(), 0);
    trainer_ai_teardown();
}

TEST(TrainerAI, GenericClassInitialisesBudgetButNoAction) {
    trainer_ai_reset(0);
    wTrainerClass = CLS_YOUNGSTER;
    EXPECT_EQ(AI_TrainerAI(), 0);
    EXPECT_EQ((int)wAICount, 3);
    trainer_ai_teardown();
}

TEST(TrainerAI, ErikaSuperPotionWhenLowHP) {
    trainer_ai_reset(0);
    wTrainerClass = CLS_ERIKA;
    wEnemyMon.hp = 5;
    EXPECT_EQ(AI_TrainerAI(), 1);
    EXPECT_EQ((int)wEnemyMon.hp, 55);
    EXPECT_EQ((int)wAICount, 0);
    trainer_ai_teardown();
}

TEST(TrainerAI, ErikaNoHealWhenHealthy) {
    trainer_ai_reset(0);
    wTrainerClass = CLS_ERIKA;
    wEnemyMon.hp = 100;
    EXPECT_EQ(AI_TrainerAI(), 0);
    EXPECT_EQ((int)wEnemyMon.hp, 100);
    EXPECT_EQ((int)wAICount, 1);
    trainer_ai_teardown();
}

TEST(TrainerAI, ErikaRollFailsNoHeal) {
    trainer_ai_reset(200);
    wTrainerClass = CLS_ERIKA;
    wEnemyMon.hp = 5;
    EXPECT_EQ(AI_TrainerAI(), 0);
    EXPECT_EQ((int)wEnemyMon.hp, 5);
    trainer_ai_teardown();
}

TEST(TrainerAI, LanceHyperPotionCapsAtMax) {
    trainer_ai_reset(0);
    wTrainerClass = CLS_LANCE;
    wEnemyMon.max_hp = 300; wEnemyMon.hp = 40;
    EXPECT_EQ(AI_TrainerAI(), 1);
    EXPECT_EQ((int)wEnemyMon.hp, 240);
    trainer_ai_teardown();
}

TEST(TrainerAI, BrockFullHealClearsStatus) {
    trainer_ai_reset(0);
    wTrainerClass = CLS_BROCK;
    wEnemyMon.status = STATUS_PAR;
    wEnemyMons[0].base.status = STATUS_PAR;
    EXPECT_EQ(AI_TrainerAI(), 1);
    EXPECT_EQ((int)wEnemyMon.status, 0);
    EXPECT_EQ((int)wEnemyMons[0].base.status, 0);
    EXPECT_EQ((int)wAICount, 4);
    trainer_ai_teardown();
}

TEST(TrainerAI, BrockNoActionWhenNoStatus) {
    trainer_ai_reset(0);
    wTrainerClass = CLS_BROCK;
    wEnemyMon.status = 0;
    EXPECT_EQ(AI_TrainerAI(), 0);
    trainer_ai_teardown();
}

TEST(TrainerAI, BlackbeltXAttackRaisesAtkStage) {
    trainer_ai_reset(0);
    wTrainerClass = CLS_BLACKBELT;
    for (int i = 0; i < NUM_STAT_MODS; i++) wEnemyMonStatMods[i] = 7;
    wEnemyMon.atk = 100; wEnemyMonUnmodifiedAttack = 100;
    EXPECT_EQ(AI_TrainerAI(), 1);
    EXPECT_EQ((int)wEnemyMonStatMods[0], 8);
    EXPECT_EQ((int)wAICount, 1);
    trainer_ai_teardown();
}

TEST(TrainerAI, JugglerSwitchesAndKeepsBudget) {
    trainer_ai_reset(0);
    wTrainerClass = CLS_JUGGLER;
    wEnemyPartyCount = 2;
    wEnemyMons[0].base.hp = 30; wEnemyMons[0].base.species = 1; wEnemyMons[0].level = 5;
    wEnemyMons[1].base.hp = 40; wEnemyMons[1].base.species = 2; wEnemyMons[1].level = 6;
    wEnemyMon.hp = 30; wEnemyMon.party_pos = 0;
    EXPECT_EQ(AI_TrainerAI(), 1);
    EXPECT_EQ((int)wEnemyMonPartyPos, 1);

    EXPECT_EQ((int)wAICount, 0xFF);
    trainer_ai_teardown();
}

TEST(TrainerAI, JugglerWontSwitchWithOneMon) {
    trainer_ai_reset(0);
    wTrainerClass = CLS_JUGGLER;
    wEnemyPartyCount = 1;
    wEnemyMons[0].base.hp = 30;
    EXPECT_EQ(AI_TrainerAI(), 0);
    EXPECT_EQ((int)wEnemyMonPartyPos, 0);
    trainer_ai_teardown();
}
