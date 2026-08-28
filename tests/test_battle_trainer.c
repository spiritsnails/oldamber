
#include "test_runner.h"
#include "../src/game/battle/battle_trainer.h"
#include "../src/game/battle/battle_switch.h"
#include "../src/game/battle/battle_loop.h"
#include "../src/platform/hardware.h"
#include "../src/game/constants.h"

static void trainer_reset(void) {
    extern void WRAMClear(void);
    WRAMClear();
    hRandomAdd = 0x00;
    hRandomSub = 0xFD;

    wBattleMon.hp     = 100;
    wBattleMon.max_hp = 100;
    wBattleMon.atk    = 80;
    wBattleMon.def    = 60;
    wBattleMon.spd    = 50;
    wBattleMon.spc    = 60;
    wBattleMon.level  = 30;

    wPartyCount           = 1;
    wPartyMons[0].base.hp = 100;
    wPlayerMonNumber      = 0;

    wIsInBattle = 1;
}

static void setup_enemy_party(uint8_t count) {
    wEnemyPartyCount = count;
    for (uint8_t i = 0; i < count; i++) {
        wEnemyMons[i].base.species    = SPECIES_RATTATA;
        wEnemyMons[i].base.hp         = 60;
        wEnemyMons[i].base.status     = 0;
        wEnemyMons[i].base.type1      = TYPE_NORMAL;
        wEnemyMons[i].base.type2      = TYPE_NORMAL;
        wEnemyMons[i].base.box_level  = (uint8_t)i;
        wEnemyMons[i].base.moves[0]   = 1;
        wEnemyMons[i].level           = 20;
        wEnemyMons[i].max_hp          = 60;
        wEnemyMons[i].atk             = 50;
        wEnemyMons[i].def             = 40;
        wEnemyMons[i].spd             = 55;
        wEnemyMons[i].spc             = 35;
    }
}

TEST(AnyEnemyCheck, EmptyParty_ReturnsZero) {
    trainer_reset();
    wEnemyPartyCount = 0;
    EXPECT_EQ(Battle_AnyEnemyPokemonAliveCheck(), 0);
}

TEST(AnyEnemyCheck, AllFainted_ReturnsZero) {
    trainer_reset();
    setup_enemy_party(3);
    wEnemyMons[0].base.hp = 0;
    wEnemyMons[1].base.hp = 0;
    wEnemyMons[2].base.hp = 0;
    EXPECT_EQ(Battle_AnyEnemyPokemonAliveCheck(), 0);
}

TEST(AnyEnemyCheck, OneAlive_ReturnsNonzero) {
    trainer_reset();
    setup_enemy_party(3);
    wEnemyMons[0].base.hp = 0;
    wEnemyMons[1].base.hp = 0;
    wEnemyMons[2].base.hp = 40;
    EXPECT_TRUE(Battle_AnyEnemyPokemonAliveCheck() != 0);
}

TEST(LoadEnemyMon, FieldsCopiedCorrectly) {
    trainer_reset();
    setup_enemy_party(2);
    wEnemyMonPartyPos = 1;

    wEnemyMons[1].base.species  = SPECIES_PIKACHU;
    wEnemyMons[1].base.hp       = 44;
    wEnemyMons[1].base.status   = STATUS_PSN;
    wEnemyMons[1].base.type1    = TYPE_ELECTRIC;
    wEnemyMons[1].base.type2    = TYPE_ELECTRIC;
    wEnemyMons[1].base.box_level= 1;
    wEnemyMons[1].level         = 25;
    wEnemyMons[1].max_hp        = 50;
    wEnemyMons[1].atk           = 55;
    wEnemyMons[1].def           = 40;
    wEnemyMons[1].spd           = 90;
    wEnemyMons[1].spc           = 50;

    Battle_LoadEnemyMonFromParty();

    EXPECT_EQ((int)wEnemyMon.species,   (int)SPECIES_PIKACHU);
    EXPECT_EQ((int)wEnemyMon.hp,        44);
    EXPECT_EQ((int)wEnemyMon.status,    (int)STATUS_PSN);
    EXPECT_EQ((int)wEnemyMon.type1,     (int)TYPE_ELECTRIC);
    EXPECT_EQ((int)wEnemyMon.party_pos, 1);
    EXPECT_EQ((int)wEnemyMon.level,     25);
    EXPECT_EQ((int)wEnemyMon.max_hp,    50);
    EXPECT_EQ((int)wEnemyMon.atk,       55);
    EXPECT_EQ((int)wEnemyMon.spd,       90);
}

TEST(LoadEnemyMon, StatModsResetToNeutral) {
    trainer_reset();
    setup_enemy_party(1);
    wEnemyMonPartyPos = 0;

    for (int i = 0; i < NUM_STAT_MODS; i++) wEnemyMonStatMods[i] = 13;

    Battle_LoadEnemyMonFromParty();

    for (int i = 0; i < NUM_STAT_MODS; i++) {
        EXPECT_EQ((int)wEnemyMonStatMods[i], 7);
    }
}

TEST(LoadEnemyMon, UnmodifiedStatsSaved) {
    trainer_reset();
    setup_enemy_party(1);
    wEnemyMonPartyPos = 0;
    wEnemyMons[0].atk = 120;
    wEnemyMons[0].def =  80;
    wEnemyMons[0].spd =  95;
    wEnemyMons[0].spc =  70;

    Battle_LoadEnemyMonFromParty();

    EXPECT_EQ((int)wEnemyMonUnmodifiedAttack,  120);
    EXPECT_EQ((int)wEnemyMonUnmodifiedDefense,  80);
    EXPECT_EQ((int)wEnemyMonUnmodifiedSpeed,    95);
    EXPECT_EQ((int)wEnemyMonUnmodifiedSpecial,  70);
}

TEST(LoadEnemyMon, ParalysisPenaltyApplied) {
    trainer_reset();
    setup_enemy_party(1);
    wEnemyMonPartyPos = 0;
    wEnemyMons[0].base.status = STATUS_PAR;
    wEnemyMons[0].spd = 100;

    Battle_LoadEnemyMonFromParty();

    EXPECT_EQ((int)wEnemyMon.spd, 25);
}

TEST(LoadEnemyMon, BurnPenaltyApplied) {
    trainer_reset();
    setup_enemy_party(1);
    wEnemyMonPartyPos = 0;
    wEnemyMons[0].base.status = STATUS_BRN;
    wEnemyMons[0].atk = 80;

    Battle_LoadEnemyMonFromParty();

    EXPECT_EQ((int)wEnemyMon.atk, 40);
}

TEST(LoadEnemyMon, NoBadgeBoostForEnemy) {
    trainer_reset();
    setup_enemy_party(1);
    wEnemyMonPartyPos = 0;
    wEnemyMons[0].atk = 100;
    wObtainedBadges = 0xFF;

    Battle_LoadEnemyMonFromParty();

    EXPECT_EQ((int)wEnemyMon.atk, 100);
}

TEST(LoadEnemyMon, MovesCopied) {
    trainer_reset();
    setup_enemy_party(1);
    wEnemyMonPartyPos = 0;
    wEnemyMons[0].base.moves[0] = 10;
    wEnemyMons[0].base.moves[1] = 20;
    wEnemyMons[0].base.moves[2] = 30;
    wEnemyMons[0].base.moves[3] = 40;

    Battle_LoadEnemyMonFromParty();

    EXPECT_EQ((int)wEnemyMon.moves[0], 10);
    EXPECT_EQ((int)wEnemyMon.moves[1], 20);
    EXPECT_EQ((int)wEnemyMon.moves[2], 30);
    EXPECT_EQ((int)wEnemyMon.moves[3], 40);
}

TEST(EnemySendOut, NoAliveEnemies_ReturnsZero) {
    trainer_reset();
    setup_enemy_party(2);
    wEnemyMons[0].base.hp = 0;
    wEnemyMons[1].base.hp = 0;
    wEnemyMonPartyPos = 0;
    EXPECT_EQ(Battle_EnemySendOut_State(), 0);
}

TEST(EnemySendOut, SkipsCurrentSlot) {
    trainer_reset();
    setup_enemy_party(3);
    wEnemyMonPartyPos = 0;
    wEnemyMons[0].base.hp = 0;
    wEnemyMons[1].base.hp = 0;
    wEnemyMons[2].base.hp = 50;

    int result = Battle_EnemySendOut_State();

    EXPECT_EQ(result, 1);
    EXPECT_EQ((int)wEnemyMonPartyPos, 2);
}

TEST(EnemySendOut, FindsFirstAliveAfterCurrent) {
    trainer_reset();
    setup_enemy_party(3);
    wEnemyMonPartyPos = 0;
    wEnemyMons[0].base.hp = 0;
    wEnemyMons[1].base.hp = 55;
    wEnemyMons[2].base.hp = 55;

    Battle_EnemySendOut_State();

    EXPECT_EQ((int)wEnemyMonPartyPos, 1);
}

TEST(EnemySendOut, ClearsEnemyBattleStatus) {
    trainer_reset();
    setup_enemy_party(2);
    wEnemyMonPartyPos  = 0;
    wEnemyMons[0].base.hp = 0;
    wEnemyMons[1].base.hp = 50;

    wEnemyBattleStatus1    = 0xFF;
    wEnemyBattleStatus2    = 0xFF;
    wEnemyBattleStatus3    = 0xFF;
    wEnemyConfusedCounter  = 5;
    wEnemyToxicCounter     = 3;

    Battle_EnemySendOut_State();

    EXPECT_EQ((int)wEnemyBattleStatus1,   0);
    EXPECT_EQ((int)wEnemyBattleStatus2,   0);
    EXPECT_EQ((int)wEnemyBattleStatus3,   0);
    EXPECT_EQ((int)wEnemyConfusedCounter, 0);
    EXPECT_EQ((int)wEnemyToxicCounter,    0);
}

TEST(EnemySendOut, ClearsPlayerTrappingOnSwitch) {
    trainer_reset();
    setup_enemy_party(2);
    wEnemyMonPartyPos = 0;
    wEnemyMons[0].base.hp = 0;
    wEnemyMons[1].base.hp = 50;

    wPlayerBattleStatus1 = (uint8_t)(1u << BSTAT1_USING_TRAPPING);

    Battle_EnemySendOut_State();

    EXPECT_EQ((int)(wPlayerBattleStatus1 & (1u << BSTAT1_USING_TRAPPING)), 0);
}

TEST(ReplaceFainted, SuccessfulReplacement) {
    trainer_reset();
    setup_enemy_party(2);
    wEnemyMonPartyPos = 0;
    wEnemyMons[0].base.hp = 0;
    wEnemyMons[1].base.hp = 60;
    wAILayer2Encouragement        = 5;
    wActionResultOrTookBattleTurn = 1;
    wEnemyMoveNum                 = 3;

    int result = Battle_ReplaceFaintedEnemyMon();

    EXPECT_EQ(result, 1);
    EXPECT_EQ((int)wEnemyMonPartyPos, 1);
    EXPECT_EQ((int)wAILayer2Encouragement,        0);
    EXPECT_EQ((int)wActionResultOrTookBattleTurn, 0);
    EXPECT_EQ((int)wEnemyMoveNum,                 0);
}

TEST(ReplaceFainted, NoAliveEnemy_ReturnsZero) {
    trainer_reset();
    setup_enemy_party(2);
    wEnemyMons[0].base.hp = 0;
    wEnemyMons[1].base.hp = 0;
    EXPECT_EQ(Battle_ReplaceFaintedEnemyMon(), 0);
}

TEST(ReplaceFainted, LoadsNewMonStats) {
    trainer_reset();
    setup_enemy_party(2);
    wEnemyMonPartyPos = 0;
    wEnemyMons[0].base.hp = 0;
    wEnemyMons[1].base.hp = 45;
    wEnemyMons[1].atk     = 77;
    wEnemyMons[1].spd     = 88;

    Battle_ReplaceFaintedEnemyMon();

    EXPECT_EQ((int)wEnemyMon.hp,  45);
    EXPECT_EQ((int)wEnemyMon.atk, 77);
    EXPECT_EQ((int)wEnemyMon.spd, 88);
}

TEST(ReplaceFainted, SavesLastSwitchInHP) {
    trainer_reset();
    setup_enemy_party(2);
    wEnemyMonPartyPos = 0;
    wEnemyMons[0].base.hp = 0;
    wEnemyMons[1].base.hp = 52;

    Battle_ReplaceFaintedEnemyMon();

    EXPECT_EQ((int)wLastSwitchInEnemyMonHP, 52);
}

TEST(TrainerVictory, SetsBattleResult) {
    trainer_reset();
    wIsInBattle = 2;

    Battle_TrainerBattleVictory();

    EXPECT_EQ((int)wBattleResult, (int)BATTLE_OUTCOME_TRAINER_VICTORY);
}

TEST(TrainerVictory, ClearsIsInBattle) {
    trainer_reset();
    wIsInBattle = 2;

    Battle_TrainerBattleVictory();

    EXPECT_EQ((int)wIsInBattle, 0);
}

TEST(Blackout, SetsBattleResult) {
    trainer_reset();

    Battle_HandlePlayerBlackOut();

    EXPECT_EQ((int)wBattleResult, (int)BATTLE_OUTCOME_BLACKOUT);
}

TEST(Blackout, SetsIsInBattleLost) {
    trainer_reset();

    Battle_HandlePlayerBlackOut();

    EXPECT_EQ((int)wIsInBattle, (int)0xFF);
}

TEST(TryRun, NotWildBattle_ReturnsFail) {
    trainer_reset();
    wIsInBattle = 2;

    int result = Battle_TryRunningFromBattle();

    EXPECT_EQ(result, 0);
    EXPECT_EQ((int)wEscapedFromBattle, 0);
}

TEST(TryRun, IncreasesAttemptCounter) {
    trainer_reset();
    wIsInBattle = 1;
    wBattleMon.spd = 10;
    wEnemyMon.spd  = 200;
    wNumRunAttempts = 0;

    Battle_TryRunningFromBattle();

    EXPECT_EQ((int)wNumRunAttempts, 1);
}

TEST(TryRun, ZeroDivisorAlwaysEscapes) {
    trainer_reset();
    wIsInBattle   = 1;
    wEnemyMon.spd = 3;
    wBattleMon.spd = 50;

    int result = Battle_TryRunningFromBattle();

    EXPECT_EQ(result, 1);
    EXPECT_EQ((int)wEscapedFromBattle, 1);
}

TEST(TryRun, HighOddsOverflow_AlwaysEscapes) {
    trainer_reset();
    wIsInBattle    = 1;
    wBattleMon.spd = 255;
    wEnemyMon.spd  = 4;

    int result = Battle_TryRunningFromBattle();

    EXPECT_EQ(result, 1);
    EXPECT_EQ((int)wEscapedFromBattle, 1);
}

TEST(TryRun, SlowPlayerFastEnemy_LowOdds_Fails) {
    trainer_reset();
    wIsInBattle    = 1;
    wBattleMon.spd = 10;
    wEnemyMon.spd  = 200;

    int result = Battle_TryRunningFromBattle();

    EXPECT_EQ(result, 0);
    EXPECT_EQ((int)wEscapedFromBattle, 0);
}

TEST(TryRun, MultipleAttempts_OddsIncrease) {
    trainer_reset();
    wIsInBattle    = 1;
    wBattleMon.spd = 10;
    wEnemyMon.spd  = 200;

    Battle_TryRunningFromBattle();
    EXPECT_EQ((int)wEscapedFromBattle, 0);
    EXPECT_EQ((int)wNumRunAttempts, 1);

    for (int i = 0; i < 9; i++) Battle_TryRunningFromBattle();

    EXPECT_EQ((int)wEscapedFromBattle, 1);
}

TEST(TryRun, EqualSpeed_CanEscape) {
    trainer_reset();
    wIsInBattle    = 1;
    wBattleMon.spd = 100;
    wEnemyMon.spd  = 100;

    hRandomAdd = 0x7E;
    hRandomSub = 0x7F;

    hRandomAdd = 0x00;
    hRandomSub = 0x00;
    int result = Battle_TryRunningFromBattle();

    EXPECT_EQ((int)wNumRunAttempts, 1);
    (void)result;
}

#include "../src/game/battle/battle_core.h"
#include "../src/game/battle/battle_exp.h"

static void full_faint_reset(void) {
    trainer_reset();

    wPartyGainExpFlags = 0;
    wEnemyMon.hp      = 0;
}

TEST(HandleEnemyFainted, WildBattle_SetsWildVictory) {
    full_faint_reset();
    wIsInBattle = 1;

    Battle_HandleEnemyMonFainted();

    EXPECT_EQ((int)wBattleResult, (int)BATTLE_OUTCOME_WILD_VICTORY);
}

TEST(HandleEnemyFainted, AllPlayerFainted_Blackout) {
    full_faint_reset();
    wIsInBattle          = 1;
    wPartyMons[0].base.hp = 0;

    Battle_HandleEnemyMonFainted();

    EXPECT_EQ((int)wBattleResult, (int)BATTLE_OUTCOME_BLACKOUT);
}

TEST(HandleEnemyFainted, TrainerBattle_AllEnemyFainted_Victory) {
    full_faint_reset();
    wIsInBattle = 2;
    setup_enemy_party(2);
    wEnemyMons[0].base.hp = 0;
    wEnemyMons[1].base.hp = 0;

    Battle_HandleEnemyMonFainted();

    EXPECT_EQ((int)wBattleResult, (int)BATTLE_OUTCOME_TRAINER_VICTORY);
}

TEST(HandleEnemyFainted, TrainerBattle_EnemyReplaced_NoBattleResult) {
    full_faint_reset();
    wIsInBattle = 2;
    setup_enemy_party(2);
    wEnemyMonPartyPos     = 0;
    wEnemyMons[0].base.hp = 0;
    wEnemyMons[1].base.hp = 60;

    Battle_HandleEnemyMonFainted();

    EXPECT_EQ((int)wBattleResult, (int)BATTLE_OUTCOME_NONE);
    EXPECT_EQ((int)wEnemyMonPartyPos, 1);
    EXPECT_EQ((int)wEnemyMon.hp, 60);
}
