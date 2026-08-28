
#include "test_runner.h"
#include "../src/game/battle/battle.h"
#include "../src/game/battle/battle_effects.h"
#include "../src/platform/hardware.h"
#include "../src/game/constants.h"

static void battle_reset(void) {
    extern void WRAMClear(void);
    WRAMClear();
    hRandomAdd = 0;
    hRandomSub = 1;
    hWhoseTurn = 0;

    wPlayerMoveEffect   = EFFECT_NONE;
    wPlayerMovePower    = 80;
    wPlayerMoveType     = TYPE_NORMAL;
    wPlayerMoveAccuracy = 255;
    wPlayerMoveNum      = 1;

    wEnemyMoveEffect    = EFFECT_NONE;
    wEnemyMovePower     = 80;
    wEnemyMoveType      = TYPE_NORMAL;
    wEnemyMoveAccuracy  = 255;
    wEnemyMoveNum       = 1;

    wBattleMon.atk = 100;  wBattleMon.def = 80;
    wBattleMon.spd = 90;   wBattleMon.spc = 70;
    wBattleMon.max_hp = 200; wBattleMon.hp = 200;
    wBattleMon.level = 50;

    wEnemyMon.atk = 100;  wEnemyMon.def = 80;
    wEnemyMon.spd = 90;   wEnemyMon.spc = 70;
    wEnemyMon.max_hp = 200; wEnemyMon.hp = 200;
    wEnemyMon.level = 50;
}

static void seed_always_trigger(void) {

    hRandomAdd = 0x01;
    hRandomSub = 0x09;

}

static void seed_never_trigger(void) {

    hRandomAdd = 0x00;
    hRandomSub = 0xFD;
}

static void seed_sleep_bypass(void) {
    hRandomAdd = 0x02;
    hRandomSub = 0x00;
}

TEST(QuarterSpeed, PlayerTurn_EnemyParalyzed_QuartersEnemySpd) {
    battle_reset();
    hWhoseTurn = 0;
    wEnemyMon.status = STATUS_PAR;
    wEnemyMon.spd = 100;
    Battle_QuarterSpeedDueToParalysis();
    EXPECT_EQ((int)wEnemyMon.spd, 25);
}

TEST(QuarterSpeed, PlayerTurn_EnemyNotParalyzed_NoChange) {
    battle_reset();
    hWhoseTurn = 0;
    wEnemyMon.status = 0;
    wEnemyMon.spd = 100;
    Battle_QuarterSpeedDueToParalysis();
    EXPECT_EQ((int)wEnemyMon.spd, 100);
}

TEST(QuarterSpeed, EnemyTurn_PlayerParalyzed_QuartersPlayerSpd) {
    battle_reset();
    hWhoseTurn = 1;
    wBattleMon.status = STATUS_PAR;
    wBattleMon.spd = 80;
    Battle_QuarterSpeedDueToParalysis();
    EXPECT_EQ((int)wBattleMon.spd, 20);
}

TEST(QuarterSpeed, Min1_WhenSpdVeryLow) {
    battle_reset();
    hWhoseTurn = 0;
    wEnemyMon.status = STATUS_PAR;
    wEnemyMon.spd = 1;
    Battle_QuarterSpeedDueToParalysis();
    EXPECT_EQ((int)wEnemyMon.spd, 1);
}

TEST(HalveAtk, PlayerTurn_EnemyBurned_HalvesEnemyAtk) {
    battle_reset();
    hWhoseTurn = 0;
    wEnemyMon.status = STATUS_BRN;
    wEnemyMon.atk = 100;
    Battle_HalveAttackDueToBurn();
    EXPECT_EQ((int)wEnemyMon.atk, 50);
}

TEST(HalveAtk, PlayerTurn_EnemyNotBurned_NoChange) {
    battle_reset();
    hWhoseTurn = 0;
    wEnemyMon.status = 0;
    wEnemyMon.atk = 100;
    Battle_HalveAttackDueToBurn();
    EXPECT_EQ((int)wEnemyMon.atk, 100);
}

TEST(HalveAtk, EnemyTurn_PlayerBurned_HalvesPlayerAtk) {
    battle_reset();
    hWhoseTurn = 1;
    wBattleMon.status = STATUS_BRN;
    wBattleMon.atk = 60;
    Battle_HalveAttackDueToBurn();
    EXPECT_EQ((int)wBattleMon.atk, 30);
}

TEST(HalveAtk, Min1_WhenAtkVeryLow) {
    battle_reset();
    hWhoseTurn = 0;
    wEnemyMon.status = STATUS_BRN;
    wEnemyMon.atk = 1;
    Battle_HalveAttackDueToBurn();
    EXPECT_EQ((int)wEnemyMon.atk, 1);
}

TEST(SleepEffect, PlayerAttacks_SetsEnemySleepCounter) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_SLEEP;
    seed_sleep_bypass();
    wEnemyMon.status = 0;

    wEnemyBattleStatus2 = (1 << BSTAT2_NEEDS_TO_RECHARGE);
    Battle_JumpMoveEffect();
    EXPECT_TRUE(IS_ASLEEP(wEnemyMon.status));
    EXPECT_FALSE(wEnemyBattleStatus2 & (1 << BSTAT2_NEEDS_TO_RECHARGE));
}

TEST(SleepEffect, EnemyAlreadyAsleep_NoChange) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_SLEEP;
    wPlayerMoveAccuracy = 255;
    wEnemyMon.status = 3;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)(wEnemyMon.status & STATUS_SLP_MASK), 3);
}

TEST(SleepEffect, EnemyAlreadyStatused_NoChange) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_SLEEP;
    wPlayerMoveAccuracy = 255;
    wEnemyMon.status = STATUS_PAR;
    Battle_JumpMoveEffect();
    EXPECT_FALSE(IS_ASLEEP(wEnemyMon.status));
    EXPECT_TRUE(IS_PARALYZED(wEnemyMon.status));
}

TEST(SleepEffect, SleepCounter_NeverZero) {

    for (int i = 0; i < 20; i++) {
        battle_reset();
        hWhoseTurn = 0;
        wPlayerMoveEffect = EFFECT_SLEEP;

        hRandomAdd = (uint8_t)(i * 3 + 2);
        hRandomSub = (uint8_t)(i * 7 + 0);

        while ((hRandomSub & 7) == (hRandomAdd & 7)) hRandomSub++;
        wEnemyMon.status = 0;
        wEnemyBattleStatus2 = (1 << BSTAT2_NEEDS_TO_RECHARGE);
        Battle_JumpMoveEffect();
        if (IS_ASLEEP(wEnemyMon.status)) {
            uint8_t ctr = wEnemyMon.status & STATUS_SLP_MASK;
            EXPECT_GT((int)ctr, 0);
            EXPECT_LT((int)ctr, 8);
        }
    }
}

TEST(PoisonEffect, PlayerAttacks_SetsEnemyPoisoned) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_POISON;
    wPlayerMoveAccuracy = 255;
    wEnemyMon.status = 0;
    wEnemyMon.type1 = TYPE_NORMAL;
    wEnemyMon.type2 = TYPE_NORMAL;
    Battle_JumpMoveEffect();
    EXPECT_TRUE(IS_POISONED(wEnemyMon.status));
}

TEST(PoisonEffect, PoisonType_Blocked) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_POISON;
    wPlayerMoveAccuracy = 255;
    wEnemyMon.status = 0;
    wEnemyMon.type1 = TYPE_POISON;
    wEnemyMon.type2 = TYPE_POISON;
    Battle_JumpMoveEffect();
    EXPECT_FALSE(IS_POISONED(wEnemyMon.status));
}

TEST(PoisonEffect, AlreadyStatused_Blocked) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_POISON;
    wPlayerMoveAccuracy = 255;
    wEnemyMon.status = STATUS_PAR;
    wEnemyMon.type1 = TYPE_NORMAL;
    wEnemyMon.type2 = TYPE_NORMAL;
    Battle_JumpMoveEffect();
    EXPECT_FALSE(IS_POISONED(wEnemyMon.status));
}

TEST(PoisonEffect, SubstituteBlocks) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_POISON;
    wPlayerMoveAccuracy = 255;
    wEnemyMon.status = 0;
    wEnemyMon.type1 = TYPE_NORMAL;
    wEnemyMon.type2 = TYPE_NORMAL;
    wEnemyBattleStatus2 = (1 << BSTAT2_HAS_SUBSTITUTE);
    Battle_JumpMoveEffect();
    EXPECT_FALSE(IS_POISONED(wEnemyMon.status));
}

TEST(PoisonSide, Side1_Triggers_WhenRngLow) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_POISON_SIDE1;
    wEnemyMon.status = 0;
    wEnemyMon.type1 = TYPE_NORMAL;
    wEnemyMon.type2 = TYPE_NORMAL;
    seed_always_trigger();
    Battle_JumpMoveEffect();
    EXPECT_TRUE(IS_POISONED(wEnemyMon.status));
}

TEST(PoisonSide, Side1_NoTrigger_WhenRngHigh) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_POISON_SIDE1;
    wEnemyMon.status = 0;
    wEnemyMon.type1 = TYPE_NORMAL;
    wEnemyMon.type2 = TYPE_NORMAL;
    seed_never_trigger();
    Battle_JumpMoveEffect();
    EXPECT_FALSE(IS_POISONED(wEnemyMon.status));
}

TEST(ToxicEffect, SetsBadlyPoisonedFlag) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect   = EFFECT_POISON;
    wPlayerMoveNum      = MOVE_TOXIC;
    wPlayerMoveAccuracy = 255;
    wEnemyMon.status    = 0;
    wEnemyMon.type1     = TYPE_NORMAL;
    wEnemyMon.type2     = TYPE_NORMAL;
    Battle_JumpMoveEffect();
    EXPECT_TRUE(IS_POISONED(wEnemyMon.status));
    EXPECT_TRUE(wEnemyBattleStatus3 & (1 << BSTAT3_BADLY_POISONED));
}

TEST(ToxicEffect, ToxicCounter_ResetToZero) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect   = EFFECT_POISON;
    wPlayerMoveNum      = MOVE_TOXIC;
    wPlayerMoveAccuracy = 255;
    wEnemyToxicCounter  = 5;
    wEnemyMon.status    = 0;
    wEnemyMon.type1     = TYPE_NORMAL;
    wEnemyMon.type2     = TYPE_NORMAL;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)wEnemyToxicCounter, 0);
}

TEST(ExplodeEffect, PlayerTurn_SetsAttackerHP_ToZero) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_EXPLODE;
    wBattleMon.hp = 200;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)wBattleMon.hp, 0);
}

TEST(ExplodeEffect, PlayerTurn_ClearsAttackerStatus) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_EXPLODE;
    wBattleMon.status = STATUS_BRN;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)wBattleMon.status, 0);
}

TEST(HazeEffect, ResetsAllStatMods_BothSides) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_HAZE;
    for (int i = 0; i < NUM_STAT_MODS; i++) {
        wPlayerMonStatMods[i] = 10;
        wEnemyMonStatMods[i]  = 4;
    }
    Battle_JumpMoveEffect();
    for (int i = 0; i < NUM_STAT_MODS; i++) {
        EXPECT_EQ((int)wPlayerMonStatMods[i], STAT_STAGE_NORMAL);
        EXPECT_EQ((int)wEnemyMonStatMods[i],  STAT_STAGE_NORMAL);
    }
}

TEST(HazeEffect, ClearsTargetStatus_NotAttackerStatus) {

    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_HAZE;
    wBattleMon.status = STATUS_PAR;
    wEnemyMon.status  = STATUS_BRN;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)wEnemyMon.status,  0);
    EXPECT_EQ((int)wBattleMon.status, STATUS_PAR);
}

TEST(MistEffect, SetsProtectedByMist_PlayerSide) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_MIST;
    wPlayerBattleStatus2 = 0;
    Battle_JumpMoveEffect();
    EXPECT_TRUE(wPlayerBattleStatus2 & (1 << BSTAT2_PROTECTED_BY_MIST));
}

TEST(MistEffect, SetsProtectedByMist_EnemySide) {
    battle_reset();
    hWhoseTurn = 1;
    wEnemyMoveEffect = EFFECT_MIST;
    wEnemyBattleStatus2 = 0;
    Battle_JumpMoveEffect();
    EXPECT_TRUE(wEnemyBattleStatus2 & (1 << BSTAT2_PROTECTED_BY_MIST));
}

TEST(FocusEnergy, SetsGettingPumped_PlayerSide) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_FOCUS_ENERGY;
    wPlayerBattleStatus2 = 0;
    Battle_JumpMoveEffect();
    EXPECT_TRUE(wPlayerBattleStatus2 & (1 << BSTAT2_GETTING_PUMPED));
}

TEST(FocusEnergy, SetsGettingPumped_EnemySide) {
    battle_reset();
    hWhoseTurn = 1;
    wEnemyMoveEffect = EFFECT_FOCUS_ENERGY;
    wEnemyBattleStatus2 = 0;
    Battle_JumpMoveEffect();
    EXPECT_TRUE(wEnemyBattleStatus2 & (1 << BSTAT2_GETTING_PUMPED));
}

TEST(SubstituteEffect, SetsHasSubstitute_WhenEnoughHP) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect    = EFFECT_SUBSTITUTE;
    wBattleMon.max_hp    = 200;
    wBattleMon.hp        = 150;
    wPlayerBattleStatus2 = 0;
    Battle_JumpMoveEffect();
    EXPECT_TRUE(wPlayerBattleStatus2 & (1 << BSTAT2_HAS_SUBSTITUTE));
}

TEST(SubstituteEffect, HPCost_DeductedFromAttacker) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_SUBSTITUTE;
    wBattleMon.max_hp = 200;
    wBattleMon.hp     = 150;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)wBattleMon.hp, 100);
}

TEST(SubstituteEffect, ExactlyZeroHP_StillSucceeds) {

    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect    = EFFECT_SUBSTITUTE;
    wBattleMon.max_hp    = 200;
    wBattleMon.hp        = 50;
    wPlayerBattleStatus2 = 0;
    Battle_JumpMoveEffect();
    EXPECT_TRUE(wPlayerBattleStatus2 & (1 << BSTAT2_HAS_SUBSTITUTE));
}

TEST(SubstituteEffect, Fails_WhenHPLessThanCost) {

    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect    = EFFECT_SUBSTITUTE;
    wBattleMon.max_hp    = 200;
    wBattleMon.hp        = 49;
    wPlayerBattleStatus2 = 0;
    Battle_JumpMoveEffect();
    EXPECT_FALSE(wPlayerBattleStatus2 & (1 << BSTAT2_HAS_SUBSTITUTE));
}

TEST(SubstituteEffect, SubstituteHP_SetToQuarterMaxHP) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect   = EFFECT_SUBSTITUTE;
    wBattleMon.max_hp   = 200;
    wBattleMon.hp       = 150;
    wPlayerSubstituteHP = 0;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)wPlayerSubstituteHP, 50);
}

TEST(BideEffect, SetsStoringEnergyFlag) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect    = EFFECT_BIDE;
    wPlayerBattleStatus1 = 0;
    Battle_JumpMoveEffect();
    EXPECT_TRUE(wPlayerBattleStatus1 & (1 << BSTAT1_STORING_ENERGY));
}

TEST(BideEffect, ZeroesMovEffects_BothSides) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_BIDE;
    wEnemyMoveEffect  = EFFECT_POISON;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)wPlayerMoveEffect, 0);
    EXPECT_EQ((int)wEnemyMoveEffect,  0);
}

TEST(RecoilEffect, AttackerTakesQuarterDamage) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_RECOIL;
    wDamage = 80;
    wBattleMon.hp = 100;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)wBattleMon.hp, 80);
}

TEST(RecoilEffect, RecoilMin1) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_RECOIL;
    wDamage = 3;
    wBattleMon.hp = 50;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)wBattleMon.hp, 49);
}

TEST(LeechSeed, SetsSeededFlag_OnTarget) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect   = EFFECT_LEECH_SEED;
    wPlayerMoveAccuracy = 255;
    wEnemyBattleStatus2 = 0;
    wEnemyMon.type1     = TYPE_NORMAL;
    wEnemyMon.type2     = TYPE_NORMAL;
    Battle_JumpMoveEffect();
    EXPECT_TRUE(wEnemyBattleStatus2 & (1 << BSTAT2_SEEDED));
}

TEST(ChargeEffect, SetsChargingUpFlag_PlayerSide) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect    = EFFECT_CHARGE;
    wPlayerBattleStatus1 = 0;
    wChargeMoveNum       = 0;
    Battle_JumpMoveEffect();
    EXPECT_TRUE(wPlayerBattleStatus1 & (1 << BSTAT1_CHARGING_UP));
}

TEST(HyperBeam, SetsNeedsToRecharge_OnAttacker) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect    = EFFECT_HYPER_BEAM;
    wPlayerBattleStatus2 = 0;
    Battle_JumpMoveEffect();
    EXPECT_TRUE(wPlayerBattleStatus2 & (1 << BSTAT2_NEEDS_TO_RECHARGE));
}

TEST(RageEffect, SetsUsingRageFlag) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect    = EFFECT_RAGE;
    wPlayerBattleStatus2 = 0;
    Battle_JumpMoveEffect();
    EXPECT_TRUE(wPlayerBattleStatus2 & (1 << BSTAT2_USING_RAGE));
}

TEST(NullEffects, EFFECT_NONE_IsNoOp) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_NONE;
    wDamage = 42;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)wDamage, 42);
}

TEST(ParalyzeEffect, SetsParalyze_OnTarget) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect   = EFFECT_PARALYZE;
    wPlayerMoveAccuracy = 255;
    wEnemyMon.status    = 0;
    wEnemyMon.type1     = TYPE_NORMAL;
    wEnemyMon.type2     = TYPE_NORMAL;
    Battle_JumpMoveEffect();
    EXPECT_TRUE(IS_PARALYZED(wEnemyMon.status));
}

TEST(ParalyzeEffect, Blocked_WhenAlreadyStatused) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect   = EFFECT_PARALYZE;
    wPlayerMoveAccuracy = 255;
    wEnemyMon.status    = STATUS_PSN;
    wEnemyMon.type1     = TYPE_NORMAL;
    wEnemyMon.type2     = TYPE_NORMAL;
    Battle_JumpMoveEffect();
    EXPECT_FALSE(IS_PARALYZED(wEnemyMon.status));
}

TEST(HealEffect, HealsHalfMaxHP_WhenMaxHPHigh) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_HEAL;
    wBattleMon.max_hp = 300;
    wBattleMon.hp     = 50;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)wBattleMon.hp, 200);
}

TEST(HealEffect, Gen1Bug_MaxHPLessThan256_HealsFullHP) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_HEAL;
    wBattleMon.max_hp = 200;
    wBattleMon.hp     = 50;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)wBattleMon.hp, 200);
}

TEST(HealEffect, CapsAtMaxHP) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_HEAL;
    wBattleMon.max_hp = 300;
    wBattleMon.hp     = 280;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)wBattleMon.hp, 300);
}

TEST(HealEffect, FailsIfAtFullHP) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_HEAL;
    wBattleMon.max_hp = 300;
    wBattleMon.hp     = 300;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)wBattleMon.hp, 300);
}

TEST(HealEffect_Rest, RestHealsToFull) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_HEAL;
    wPlayerMoveNum    = MOVE_REST;
    wBattleMon.max_hp = 300;
    wBattleMon.hp     = 1;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)wBattleMon.hp, 300);
}

TEST(HealEffect_Rest, RestSetsAttackerAsleep_TwoTurns) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerMoveEffect = EFFECT_HEAL;
    wPlayerMoveNum    = MOVE_REST;
    wBattleMon.max_hp = 200;
    wBattleMon.hp     = 100;
    wBattleMon.status = 0;
    Battle_JumpMoveEffect();
    EXPECT_EQ((int)(wBattleMon.status & STATUS_SLP_MASK), 2);
}
