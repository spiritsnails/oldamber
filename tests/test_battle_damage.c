
#include "test_runner.h"
#include "../src/game/battle/battle.h"
#include "../src/platform/hardware.h"
#include "../src/game/constants.h"

static void battle_reset(void) {
    extern void WRAMClear(void);
    WRAMClear();
    hRandomAdd = 0;
    hRandomSub = 0;
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
}

static void seed_crit_hit(void)  { hRandomAdd = 0xFB; hRandomSub = 0x03; }
static void seed_crit_miss(void) { hRandomAdd = 0x7B; hRandomSub = 0x82; }
static void seed_r30(void)       { hRandomAdd = 0xBE; hRandomSub = 0x03; }
static void seed_acc_hit(void)   { hRandomAdd = 0xFB; hRandomSub = 0x03; }
static void seed_acc_miss(void)  { hRandomAdd = 0x00; hRandomSub = 0xFF; }

TEST(CalcDamage, BasicFormula_Lv5) {
    battle_reset();
    wDamage = 0;
    Battle_CalcDamage(10, 10, 40, 5);
    EXPECT_EQ((int)wDamage, 5);
}

TEST(CalcDamage, BasicFormula_Lv100) {
    battle_reset();
    wDamage = 0;
    Battle_CalcDamage(100, 60, 90, 100);
    EXPECT_EQ((int)wDamage, 128);
}

TEST(CalcDamage, StatusMove_NoDamage) {
    battle_reset();
    wDamage = 0;
    int ret = Battle_CalcDamage(100, 100, 0, 50);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ((int)wDamage, 0);
}

TEST(CalcDamage, Cap_At_999) {
    battle_reset();
    wDamage = 0;

    Battle_CalcDamage(255, 1, 250, 100);
    EXPECT_EQ((int)wDamage, 999);
}

TEST(CalcDamage, MinDamageIsTwo) {
    battle_reset();
    wDamage = 0;
    Battle_CalcDamage(1, 255, 1, 1);
    EXPECT_GE((int)wDamage, MIN_NEUTRAL_DAMAGE);
}

TEST(CalcDamage, Explode_HalvesDefense) {
    battle_reset();
    wPlayerMoveEffect = EFFECT_EXPLODE;
    wDamage = 0;
    Battle_CalcDamage(200, 200, 250, 50);
    EXPECT_EQ((int)wDamage, 222);
}

TEST(CalcDamage, Accumulates_AcrossCalls) {
    battle_reset();
    wDamage = 0;
    Battle_CalcDamage(50, 50, 50, 50);
    EXPECT_EQ((int)wDamage, 24);
    Battle_CalcDamage(50, 50, 50, 50);
    EXPECT_EQ((int)wDamage, 48);
}

TEST(CriticalHit, Electrode_Crits) {
    battle_reset();
    wBattleMon.species = SPECIES_ELECTRODE;
    wPlayerMovePower   = 90;
    seed_crit_hit();
    Battle_CriticalHitTest();
    EXPECT_EQ((int)wCriticalHitOrOHKO, 1);
}

TEST(CriticalHit, Electrode_NoCrit) {
    battle_reset();
    wBattleMon.species = SPECIES_ELECTRODE;
    wPlayerMovePower   = 90;
    seed_crit_miss();
    Battle_CriticalHitTest();
    EXPECT_EQ((int)wCriticalHitOrOHKO, 0);
}

TEST(CriticalHit, StatusMove_NeverCrits) {
    battle_reset();
    wBattleMon.species = SPECIES_ELECTRODE;
    wPlayerMovePower   = 0;
    seed_crit_hit();
    Battle_CriticalHitTest();
    EXPECT_EQ((int)wCriticalHitOrOHKO, 0);
}

TEST(CriticalHit, FocusEnergyBug_WithoutFE_IsCrit) {
    battle_reset();
    wBattleMon.species   = SPECIES_ELECTRODE;
    wPlayerMovePower     = 90;
    wPlayerBattleStatus2 = 0;
    seed_r30();
    Battle_CriticalHitTest();
    EXPECT_EQ((int)wCriticalHitOrOHKO, 1);
}

TEST(CriticalHit, FocusEnergyBug_WithFE_NoCrit) {
    battle_reset();
    wBattleMon.species   = SPECIES_ELECTRODE;
    wPlayerMovePower     = 90;
    wPlayerBattleStatus2 = (uint8_t)(1 << BSTAT2_GETTING_PUMPED);

    seed_r30();
    Battle_CriticalHitTest();
    EXPECT_EQ((int)wCriticalHitOrOHKO, 0);
}

TEST(CriticalHit, HighCritMove_Slash) {
    battle_reset();
    wBattleMon.species = SPECIES_ELECTRODE;
    wPlayerMovePower   = 70;
    wPlayerMoveNum     = MOVE_SLASH;
    seed_r30();
    Battle_CriticalHitTest();
    EXPECT_EQ((int)wCriticalHitOrOHKO, 1);
}

TEST(CriticalHit, HighCritMove_KarateChop) {
    battle_reset();
    wBattleMon.species = SPECIES_ELECTRODE;
    wPlayerMovePower   = 50;
    wPlayerMoveNum     = MOVE_KARATE_CHOP;
    seed_r30();
    Battle_CriticalHitTest();
    EXPECT_EQ((int)wCriticalHitOrOHKO, 1);
}

TEST(CriticalHit, EnemyTurn) {
    battle_reset();
    hWhoseTurn         = 1;
    wEnemyMon.species  = SPECIES_ELECTRODE;
    wEnemyMovePower    = 90;
    seed_crit_hit();
    Battle_CriticalHitTest();
    EXPECT_EQ((int)wCriticalHitOrOHKO, 1);
}

TEST(RandomizeDamage, Zero_Unchanged) {
    battle_reset();
    wDamage = 0;
    Battle_RandomizeDamage();
    EXPECT_EQ((int)wDamage, 0);
}

TEST(RandomizeDamage, One_Unchanged) {
    battle_reset();
    wDamage = 1;
    Battle_RandomizeDamage();
    EXPECT_EQ((int)wDamage, 1);
}

TEST(RandomizeDamage, KnownSeed_100Damage) {
    battle_reset();
    wDamage    = 100;
    hRandomAdd = 0x00;
    hRandomSub = 0xFF;
    Battle_RandomizeDamage();
    EXPECT_EQ((int)wDamage, 98);
}

TEST(RandomizeDamage, AlwaysInRange) {
    battle_reset();
    hRandomAdd = 0xAA;
    hRandomSub = 0x55;
    int fails = 0;
    for (int i = 0; i < 20; i++) {
        wDamage = 200;
        Battle_RandomizeDamage();

        if ((int)wDamage < 170 || (int)wDamage > 200) fails++;
    }
    EXPECT_EQ(fails, 0);
}

TEST(CalcHitChance, NormalStages_Unchanged) {
    battle_reset();

    wPlayerMoveAccuracy = 200;
    Battle_CalcHitChance();
    EXPECT_EQ((int)wPlayerMoveAccuracy, 200);
}

TEST(CalcHitChance, FullAccuracy_NormalStages) {
    battle_reset();
    wPlayerMoveAccuracy = 255;
    Battle_CalcHitChance();
    EXPECT_EQ((int)wPlayerMoveAccuracy, 255);
}

TEST(CalcHitChance, DefenderEvasionUp1) {
    battle_reset();
    wPlayerMoveAccuracy          = 255;
    wPlayerMonStatMods[MOD_ACCURACY] = 7;
    wEnemyMonStatMods[MOD_EVASION]   = 8;
    Battle_CalcHitChance();
    EXPECT_EQ((int)wPlayerMoveAccuracy, 168);
}

TEST(CalcHitChance, AttackerAccuracyDown2) {
    battle_reset();
    wPlayerMoveAccuracy              = 200;
    wPlayerMonStatMods[MOD_ACCURACY] = 5;
    wEnemyMonStatMods[MOD_EVASION]   = 7;
    Battle_CalcHitChance();
    EXPECT_EQ((int)wPlayerMoveAccuracy, 100);
}

TEST(CalcHitChance, FloorAtOne) {
    battle_reset();
    wPlayerMoveAccuracy              = 1;
    wPlayerMonStatMods[MOD_ACCURACY] = 1;
    wEnemyMonStatMods[MOD_EVASION]   = 13;
    Battle_CalcHitChance();
    EXPECT_GE((int)wPlayerMoveAccuracy, 1);
}

TEST(CalcHitChance, EnemyTurn) {
    battle_reset();
    hWhoseTurn = 1;
    wEnemyMoveAccuracy               = 200;
    wEnemyMonStatMods[MOD_ACCURACY]  = 7;
    wPlayerMonStatMods[MOD_EVASION]  = 7;
    Battle_CalcHitChance();
    EXPECT_EQ((int)wEnemyMoveAccuracy, 200);
}

TEST(MoveHitTest, DreamEater_SleepingTarget_Hits) {
    battle_reset();
    wPlayerMoveEffect = EFFECT_DREAM_EATER;
    wPlayerMovePower  = 100;
    wEnemyMon.status  = 0x03;
    seed_acc_hit();
    wMoveMissed = 0;
    Battle_MoveHitTest();
    EXPECT_EQ((int)wMoveMissed, 0);
}

TEST(MoveHitTest, DreamEater_AwakeTarget_Misses) {
    battle_reset();
    wPlayerMoveEffect = EFFECT_DREAM_EATER;
    wPlayerMovePower  = 100;
    wEnemyMon.status  = 0;
    wMoveMissed = 0;
    Battle_MoveHitTest();
    EXPECT_EQ((int)wMoveMissed, 1);
    EXPECT_EQ((int)wDamage, 0);
}

TEST(MoveHitTest, Swift_AlwaysHits_EvenInvulnerable) {
    battle_reset();
    wPlayerMoveEffect   = EFFECT_SWIFT;
    wEnemyBattleStatus1 = (uint8_t)(1 << BSTAT1_INVULNERABLE);
    wMoveMissed = 0;
    Battle_MoveHitTest();
    EXPECT_EQ((int)wMoveMissed, 0);
}

TEST(MoveHitTest, Invulnerable_Target_Misses) {
    battle_reset();
    wPlayerMoveEffect   = EFFECT_NONE;
    wEnemyBattleStatus1 = (uint8_t)(1 << BSTAT1_INVULNERABLE);
    wMoveMissed = 0;
    Battle_MoveHitTest();
    EXPECT_EQ((int)wMoveMissed, 1);
}

TEST(MoveHitTest, Mist_BlocksStatLower) {
    battle_reset();
    wPlayerMoveEffect   = EFFECT_ATTACK_DOWN1;
    wEnemyBattleStatus2 = (uint8_t)(1 << BSTAT2_PROTECTED_BY_MIST);
    wMoveMissed = 0;
    Battle_MoveHitTest();
    EXPECT_EQ((int)wMoveMissed, 1);
}

TEST(MoveHitTest, Mist_DoesNotBlockDamage) {
    battle_reset();
    wPlayerMoveEffect   = EFFECT_NONE;
    wPlayerMoveAccuracy = 255;
    wEnemyBattleStatus2 = (uint8_t)(1 << BSTAT2_PROTECTED_BY_MIST);
    seed_acc_hit();
    wMoveMissed = 0;
    Battle_MoveHitTest();
    EXPECT_EQ((int)wMoveMissed, 0);
}

TEST(MoveHitTest, XAccuracy_AlwaysHits) {
    battle_reset();
    wPlayerMoveEffect    = EFFECT_NONE;
    wPlayerMoveAccuracy  = 1;
    wPlayerBattleStatus2 = (uint8_t)(1 << BSTAT2_USING_X_ACCURACY);
    wMoveMissed = 0;
    Battle_MoveHitTest();
    EXPECT_EQ((int)wMoveMissed, 0);
}

TEST(MoveHitTest, SubstituteBug_DrainPassesThrough) {
    battle_reset();
    wPlayerMoveEffect   = EFFECT_DRAIN_HP;
    wPlayerMoveAccuracy = 255;
    wEnemyBattleStatus2 = (uint8_t)(1 << BSTAT2_HAS_SUBSTITUTE);
    seed_acc_hit();
    wMoveMissed = 0;
    Battle_MoveHitTest();
    EXPECT_EQ((int)wMoveMissed, 0);
}

TEST(MoveHitTest, FullAccuracy_Hits) {
    battle_reset();
    wPlayerMoveEffect   = EFFECT_NONE;
    wPlayerMoveAccuracy = 255;
    seed_acc_hit();
    wMoveMissed = 0;
    Battle_MoveHitTest();
    EXPECT_EQ((int)wMoveMissed, 0);
}

TEST(MoveHitTest, LowAccuracy_Misses) {
    battle_reset();
    wPlayerMoveEffect   = EFFECT_NONE;
    wPlayerMoveAccuracy = 1;
    seed_acc_miss();
    wMoveMissed = 0;
    Battle_MoveHitTest();
    EXPECT_EQ((int)wMoveMissed, 1);
}

TEST(MoveHitTest, OnMiss_ClearsTrappingFlag) {
    battle_reset();
    wPlayerMoveEffect    = EFFECT_NONE;
    wPlayerMoveAccuracy  = 1;
    wPlayerBattleStatus1 = (uint8_t)(1 << BSTAT1_USING_TRAPPING);
    seed_acc_miss();
    Battle_MoveHitTest();
    EXPECT_EQ((int)(wPlayerBattleStatus1 & (1 << BSTAT1_USING_TRAPPING)), 0);
}

TEST(AdjustDamageForMoveType, NoStab_NoTypeMatch_Unchanged) {
    battle_reset();

    wBattleMon.type1     = TYPE_FIRE;
    wBattleMon.type2     = TYPE_FIRE;
    wEnemyMon.type1      = TYPE_NORMAL;
    wEnemyMon.type2      = TYPE_NORMAL;
    wPlayerMoveType      = TYPE_NORMAL;
    wDamage              = 100;
    Battle_AdjustDamageForMoveType();
    EXPECT_EQ((int)wDamage, 100);
    EXPECT_EQ((int)(wDamageMultipliers & (1 << BIT_STAB_DAMAGE)), 0);
}

TEST(AdjustDamageForMoveType, StabOnly_NoTypeMatch) {
    battle_reset();
    wBattleMon.type1     = TYPE_GRASS;
    wBattleMon.type2     = TYPE_GRASS;
    wEnemyMon.type1      = TYPE_NORMAL;
    wEnemyMon.type2      = TYPE_NORMAL;
    wPlayerMoveType      = TYPE_GRASS;
    wDamage              = 100;
    Battle_AdjustDamageForMoveType();

    EXPECT_EQ((int)wDamage, 150);
    EXPECT_NE((int)(wDamageMultipliers & (1 << BIT_STAB_DAMAGE)), 0);
}

TEST(AdjustDamageForMoveType, SuperEffective_MonoType) {
    battle_reset();
    wBattleMon.type1     = TYPE_NORMAL;
    wBattleMon.type2     = TYPE_NORMAL;
    wEnemyMon.type1      = TYPE_FIRE;
    wEnemyMon.type2      = TYPE_FIRE;
    wPlayerMoveType      = TYPE_WATER;
    wDamage              = 100;
    Battle_AdjustDamageForMoveType();

    EXPECT_EQ((int)wDamage, 200);
}

TEST(AdjustDamageForMoveType, NotVeryEffective_MonoType) {
    battle_reset();
    wBattleMon.type1     = TYPE_NORMAL;
    wBattleMon.type2     = TYPE_NORMAL;
    wEnemyMon.type1      = TYPE_WATER;
    wEnemyMon.type2      = TYPE_WATER;
    wPlayerMoveType      = TYPE_FIRE;
    wDamage              = 100;
    Battle_AdjustDamageForMoveType();
    EXPECT_EQ((int)wDamage, 50);
}

TEST(AdjustDamageForMoveType, Immune_SetsMissFlag) {
    battle_reset();
    wBattleMon.type1     = TYPE_NORMAL;
    wBattleMon.type2     = TYPE_NORMAL;
    wEnemyMon.type1      = TYPE_GHOST;
    wEnemyMon.type2      = TYPE_GHOST;
    wPlayerMoveType      = TYPE_NORMAL;
    wDamage              = 100;
    wMoveMissed          = 0;
    Battle_AdjustDamageForMoveType();
    EXPECT_EQ((int)wDamage, 0);
    EXPECT_EQ((int)wMoveMissed, 1);
}

TEST(AdjustDamageForMoveType, DualType_4x) {
    battle_reset();
    wBattleMon.type1     = TYPE_NORMAL;
    wBattleMon.type2     = TYPE_NORMAL;
    wEnemyMon.type1      = TYPE_FIRE;
    wEnemyMon.type2      = TYPE_ROCK;
    wPlayerMoveType      = TYPE_WATER;
    wDamage              = 100;
    Battle_AdjustDamageForMoveType();

    EXPECT_EQ((int)wDamage, 400);
}

TEST(AdjustDamageForMoveType, DualType_CancelOut) {
    battle_reset();
    wBattleMon.type1     = TYPE_NORMAL;
    wBattleMon.type2     = TYPE_NORMAL;
    wEnemyMon.type1      = TYPE_GRASS;
    wEnemyMon.type2      = TYPE_ROCK;
    wPlayerMoveType      = TYPE_FIRE;
    wDamage              = 100;
    Battle_AdjustDamageForMoveType();

    EXPECT_EQ((int)wDamage, 100);
}

TEST(AdjustDamageForMoveType, Stab_Plus_4x) {
    battle_reset();
    wBattleMon.type1     = TYPE_WATER;
    wBattleMon.type2     = TYPE_WATER;
    wEnemyMon.type1      = TYPE_FIRE;
    wEnemyMon.type2      = TYPE_ROCK;
    wPlayerMoveType      = TYPE_WATER;
    wDamage              = 100;
    Battle_AdjustDamageForMoveType();
    EXPECT_EQ((int)wDamage, 600);
    EXPECT_NE((int)(wDamageMultipliers & (1 << BIT_STAB_DAMAGE)), 0);
}

TEST(AdjustDamageForMoveType, EnemyTurn) {
    battle_reset();
    hWhoseTurn           = 1;
    wEnemyMon.type1      = TYPE_WATER;
    wEnemyMon.type2      = TYPE_WATER;
    wBattleMon.type1     = TYPE_FIRE;
    wBattleMon.type2     = TYPE_FIRE;
    wEnemyMoveType       = TYPE_WATER;
    wDamage              = 100;
    Battle_AdjustDamageForMoveType();

    EXPECT_EQ((int)wDamage, 300);
}

TEST(AdjustDamageForMoveType, DamageMultipliers_Updated) {
    battle_reset();
    wBattleMon.type1     = TYPE_NORMAL;
    wBattleMon.type2     = TYPE_NORMAL;
    wEnemyMon.type1      = TYPE_GRASS;
    wEnemyMon.type2      = TYPE_ROCK;
    wPlayerMoveType      = TYPE_FIRE;
    wDamage              = 100;
    Battle_AdjustDamageForMoveType();

    EXPECT_EQ((int)(wDamageMultipliers & 0x7F), 5);
    EXPECT_EQ((int)(wDamageMultipliers & (1 << BIT_STAB_DAMAGE)), 0);
}

TEST(CalculateModifiedStats, NormalStage_NoChange) {
    battle_reset();
    wCalculateWhoseStats           = 0;
    wPlayerMonStatMods[MOD_ATTACK] = STAT_STAGE_NORMAL;
    wPlayerMonUnmodifiedAttack     = 100;
    Battle_CalculateModifiedStats();
    EXPECT_EQ((int)wBattleMon.atk, 100);
}

TEST(CalculateModifiedStats, StageBoost) {
    battle_reset();
    wCalculateWhoseStats           = 0;
    wPlayerMonStatMods[MOD_ATTACK] = 8;
    wPlayerMonUnmodifiedAttack     = 100;
    Battle_CalculateModifiedStats();
    EXPECT_EQ((int)wBattleMon.atk, 150);
}

TEST(CalculateModifiedStats, StageDrop) {
    battle_reset();
    wCalculateWhoseStats           = 0;
    wPlayerMonStatMods[MOD_ATTACK] = 6;
    wPlayerMonUnmodifiedAttack     = 100;
    Battle_CalculateModifiedStats();
    EXPECT_EQ((int)wBattleMon.atk, 66);
}

TEST(CalculateModifiedStats, Cap_At_999) {
    battle_reset();
    wCalculateWhoseStats           = 0;
    wPlayerMonStatMods[MOD_ATTACK] = 13;
    wPlayerMonUnmodifiedAttack     = 300;
    Battle_CalculateModifiedStats();
    EXPECT_EQ((int)wBattleMon.atk, 999);
}

TEST(CalculateModifiedStats, Floor_At_1) {
    battle_reset();
    wCalculateWhoseStats           = 0;
    wPlayerMonStatMods[MOD_ATTACK] = 1;
    wPlayerMonUnmodifiedAttack     = 3;
    Battle_CalculateModifiedStats();
    EXPECT_EQ((int)wBattleMon.atk, 1);
}

TEST(CalculateModifiedStats, EnemySide) {
    battle_reset();
    wCalculateWhoseStats           = 1;
    wEnemyMonStatMods[MOD_ATTACK]  = 8;
    wEnemyMonUnmodifiedAttack      = 200;
    Battle_CalculateModifiedStats();
    EXPECT_EQ((int)wEnemyMon.atk, 300);
}

TEST(ApplyBurnAndParalysisPenalties, PlayerSide_Paralysis) {
    battle_reset();
    hWhoseTurn        = 1;
    wBattleMon.status = STATUS_PAR;
    wBattleMon.spd    = 100;
    Battle_ApplyBurnAndParalysisPenalties();
    EXPECT_EQ((int)wBattleMon.spd, 25);
}

TEST(ApplyBurnAndParalysisPenalties, PlayerSide_Burn) {
    battle_reset();
    hWhoseTurn        = 1;
    wBattleMon.status = STATUS_BRN;
    wBattleMon.atk    = 100;
    Battle_ApplyBurnAndParalysisPenalties();
    EXPECT_EQ((int)wBattleMon.atk, 50);
}

TEST(ApplyBurnAndParalysisPenalties, EnemySide_Paralysis) {
    battle_reset();
    hWhoseTurn       = 0;
    wEnemyMon.status = STATUS_PAR;
    wEnemyMon.spd    = 100;
    Battle_ApplyBurnAndParalysisPenalties();
    EXPECT_EQ((int)wEnemyMon.spd, 25);
}

TEST(ApplyBurnAndParalysisPenalties, EnemySide_Burn) {
    battle_reset();
    hWhoseTurn       = 0;
    wEnemyMon.status = STATUS_BRN;
    wEnemyMon.atk    = 100;
    Battle_ApplyBurnAndParalysisPenalties();
    EXPECT_EQ((int)wEnemyMon.atk, 50);
}

TEST(ApplyBurnAndParalysisPenalties, MinOne_Paralysis) {
    battle_reset();
    hWhoseTurn        = 1;
    wBattleMon.status = STATUS_PAR;
    wBattleMon.spd    = 1;
    Battle_ApplyBurnAndParalysisPenalties();
    EXPECT_EQ((int)wBattleMon.spd, 1);
}

TEST(ApplyBurnAndParalysisPenalties, MinOne_Burn) {
    battle_reset();
    hWhoseTurn        = 1;
    wBattleMon.status = STATUS_BRN;
    wBattleMon.atk    = 1;
    Battle_ApplyBurnAndParalysisPenalties();
    EXPECT_EQ((int)wBattleMon.atk, 1);
}

TEST(ApplyBurnAndParalysisPenalties, NoStatus_NoChange) {
    battle_reset();
    hWhoseTurn        = 1;
    wBattleMon.status = 0;
    wBattleMon.atk    = 100;
    wBattleMon.spd    = 100;
    Battle_ApplyBurnAndParalysisPenalties();
    EXPECT_EQ((int)wBattleMon.atk, 100);
    EXPECT_EQ((int)wBattleMon.spd, 100);
}

TEST(GetDamageVarsForPlayerAttack, PowerZero_Returns0) {
    battle_reset();
    wPlayerMovePower = 0;
    int ret = Battle_GetDamageVarsForPlayerAttack();
    EXPECT_EQ(ret, 0);
    EXPECT_EQ((int)wDamage, 0);
}

TEST(GetDamageVarsForPlayerAttack, Physical_Normal) {
    battle_reset();
    wBattleMon.atk   = 100;
    wBattleMon.level = 100;
    wEnemyMon.def    = 60;
    wPlayerMovePower = 90;
    wPlayerMoveType  = TYPE_NORMAL;
    Battle_GetDamageVarsForPlayerAttack();
    EXPECT_EQ((int)wDamage, 128);
}

TEST(GetDamageVarsForPlayerAttack, Special_Normal) {
    battle_reset();
    wBattleMon.spc   = 100;
    wBattleMon.level = 100;
    wEnemyMon.spc    = 60;
    wPlayerMovePower = 90;
    wPlayerMoveType  = TYPE_FIRE;
    Battle_GetDamageVarsForPlayerAttack();
    EXPECT_EQ((int)wDamage, 128);
}

TEST(GetDamageVarsForPlayerAttack, Reflect_DoublesEnemyDef) {
    battle_reset();
    wBattleMon.atk      = 100;
    wBattleMon.level    = 100;
    wEnemyMon.def       = 60;
    wPlayerMovePower    = 90;
    wPlayerMoveType     = TYPE_NORMAL;
    wEnemyBattleStatus3 = (1 << BSTAT3_HAS_REFLECT);
    Battle_GetDamageVarsForPlayerAttack();
    EXPECT_EQ((int)wDamage, 65);
}

TEST(GetDamageVarsForPlayerAttack, LightScreen_DoublesEnemySpc) {
    battle_reset();
    wBattleMon.spc      = 100;
    wBattleMon.level    = 100;
    wEnemyMon.spc       = 60;
    wPlayerMovePower    = 90;
    wPlayerMoveType     = TYPE_FIRE;
    wEnemyBattleStatus3 = (1 << BSTAT3_HAS_LIGHT_SCREEN);
    Battle_GetDamageVarsForPlayerAttack();
    EXPECT_EQ((int)wDamage, 65);
}

TEST(GetDamageVarsForPlayerAttack, Crit_BypassStats) {
    battle_reset();
    wCriticalHitOrOHKO    = 1;
    wPlayerMonNumber      = 0;
    wPartyMons[0].atk     = 100;
    wBattleMon.atk        = 50;
    wBattleMon.level      = 50;
    wEnemyMon.species     = SPECIES_BLASTOISE;
    wEnemyMon.dvs         = 0;
    wEnemyMon.level       = 50;
    wEnemyMon.def         = 200;
    wPlayerMovePower      = 80;
    wPlayerMoveType       = TYPE_NORMAL;
    Battle_GetDamageVarsForPlayerAttack();
    EXPECT_EQ((int)wDamage, 66);
}

TEST(GetDamageVarsForEnemyAttack, PowerZero_Returns0) {
    battle_reset();
    wEnemyMovePower = 0;
    int ret = Battle_GetDamageVarsForEnemyAttack();
    EXPECT_EQ(ret, 0);
    EXPECT_EQ((int)wDamage, 0);
}

TEST(GetDamageVarsForEnemyAttack, Physical_Normal) {
    battle_reset();
    wEnemyMon.atk   = 100;
    wEnemyMon.level = 100;
    wBattleMon.def  = 60;
    wEnemyMovePower = 90;
    wEnemyMoveType  = TYPE_NORMAL;
    Battle_GetDamageVarsForEnemyAttack();
    EXPECT_EQ((int)wDamage, 128);
}

TEST(GetDamageVarsForEnemyAttack, Special_Normal) {
    battle_reset();
    wEnemyMon.spc   = 100;
    wEnemyMon.level = 100;
    wBattleMon.spc  = 60;
    wEnemyMovePower = 90;
    wEnemyMoveType  = TYPE_FIRE;
    Battle_GetDamageVarsForEnemyAttack();
    EXPECT_EQ((int)wDamage, 128);
}

TEST(GetDamageVarsForEnemyAttack, Reflect_DoublesPlayerDef) {
    battle_reset();
    wEnemyMon.atk        = 100;
    wEnemyMon.level      = 100;
    wBattleMon.def       = 60;
    wEnemyMovePower      = 90;
    wEnemyMoveType       = TYPE_NORMAL;
    wPlayerBattleStatus3 = (1 << BSTAT3_HAS_REFLECT);
    Battle_GetDamageVarsForEnemyAttack();
    EXPECT_EQ((int)wDamage, 65);
}

TEST(GetDamageVarsForEnemyAttack, Crit_BypassStats) {
    battle_reset();
    wCriticalHitOrOHKO    = 1;
    wPlayerMonNumber      = 0;
    wEnemyMon.species     = SPECIES_BULBASAUR;
    wEnemyMon.dvs         = 0;
    wEnemyMon.level       = 50;
    wEnemyMon.atk         = 200;
    wPartyMons[0].def     = 100;
    wBattleMon.def        = 10;
    wEnemyMovePower       = 80;
    wEnemyMoveType        = TYPE_NORMAL;
    Battle_GetDamageVarsForEnemyAttack();
    EXPECT_EQ((int)wDamage, 38);
}
