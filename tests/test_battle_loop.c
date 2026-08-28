
#include "test_runner.h"
#include "../src/game/battle/battle.h"
#include "../src/game/battle/battle_core.h"
#include "../src/game/battle/battle_loop.h"
#include "../src/platform/hardware.h"
#include "../src/game/constants.h"
#include <stdio.h>

static void battle_reset(void) {
    extern void WRAMClear(void);
    WRAMClear();
    hRandomAdd = 0x00;
    hRandomSub = 0xFD;
    hWhoseTurn = 0;

    wBattleMon.atk = 100;  wBattleMon.def = 80;
    wBattleMon.spd = 50;   wBattleMon.spc = 70;
    wBattleMon.max_hp = 200; wBattleMon.hp = 200;
    wBattleMon.level = 50;

    wEnemyMon.atk = 100;  wEnemyMon.def = 80;
    wEnemyMon.spd = 50;   wEnemyMon.spc = 70;
    wEnemyMon.max_hp = 200; wEnemyMon.hp = 200;
    wEnemyMon.level = 50;

    wPlayerSelectedMove = 1;
    wPlayerMoveEffect   = EFFECT_NONE;
    wPlayerMovePower    = 40;
    wPlayerMoveType     = TYPE_NORMAL;
    wPlayerMoveAccuracy = 255;

    wEnemyMon.moves[0] = 1;
    wEnemyMon.moves[1] = 1;
    wEnemyMon.moves[2] = 1;
    wEnemyMon.moves[3] = 1;
    wEnemySelectedMove  = 1;
    wEnemyMoveEffect    = EFFECT_NONE;
    wEnemyMovePower     = 40;
    wEnemyMoveType      = TYPE_NORMAL;
    wEnemyMoveAccuracy  = 255;

    wDamageMultipliers  = DAMAGE_MULT_EFFECTIVE;

    wPlayerBattleStatus2 |= (1u << BSTAT2_USING_X_ACCURACY);
    wEnemyBattleStatus2  |= (1u << BSTAT2_USING_X_ACCURACY);

    wIsInBattle          = 1;
    wPartyCount          = 1;
    wPartyMons[0].base.hp = 200;
}

TEST(SelectEnemyMove, LockedRecharging_NoChange) {
    battle_reset();
    wEnemyBattleStatus2 |= (1u << BSTAT2_NEEDS_TO_RECHARGE);
    wEnemySelectedMove = 0x42;
    Battle_SelectEnemyMove();
    EXPECT_EQ((int)wEnemySelectedMove, 0x42);
}

TEST(SelectEnemyMove, LockedSleep_NoChange) {
    battle_reset();
    wEnemyMon.status = 2;
    wEnemySelectedMove = 0x42;
    Battle_SelectEnemyMove();
    EXPECT_EQ((int)wEnemySelectedMove, 0x42);
}

TEST(SelectEnemyMove, OnlyMoveDisabled_Struggle) {
    battle_reset();

    wEnemyMon.moves[0] = 5;
    wEnemyMon.moves[1] = 0;
    wEnemyMon.moves[2] = 0;
    wEnemyMon.moves[3] = 0;

    wEnemyDisabledMove = 0x11;
    Battle_SelectEnemyMove();
    EXPECT_EQ((int)wEnemySelectedMove, (int)MOVE_STRUGGLE);
}

TEST(SelectEnemyMove, EmptySlotSkipped_AlwaysPicksOnlyValidSlot) {
    battle_reset();

    wEnemyMon.moves[0] = 7;
    wEnemyMon.moves[1] = 0;
    wEnemyMon.moves[2] = 0;
    wEnemyMon.moves[3] = 0;
    wEnemyDisabledMove = 0;
    Battle_SelectEnemyMove();

    EXPECT_EQ((int)wEnemySelectedMove, 7);
    EXPECT_EQ((int)wEnemyMoveListIndex, 0);
}

TEST(SelectEnemyMove, TrappedPlayer_SetsCannotMove) {
    battle_reset();
    wPlayerBattleStatus1 |= (1u << BSTAT1_USING_TRAPPING);
    wEnemySelectedMove = 0x42;
    Battle_SelectEnemyMove();
    EXPECT_EQ((int)wEnemySelectedMove, (int)CANNOT_MOVE);
}

TEST(CheckNumAttacksLeft, PlayerZero_ClearsTrapping) {
    battle_reset();
    wPlayerBattleStatus1 |= (1u << BSTAT1_USING_TRAPPING);
    wPlayerNumAttacksLeft = 0;
    Battle_CheckNumAttacksLeft();
    EXPECT_EQ((int)(wPlayerBattleStatus1 & (1u << BSTAT1_USING_TRAPPING)), 0);
}

TEST(CheckNumAttacksLeft, PlayerNonZero_KeepsTrapping) {
    battle_reset();
    wPlayerBattleStatus1 |= (1u << BSTAT1_USING_TRAPPING);
    wPlayerNumAttacksLeft = 2;
    Battle_CheckNumAttacksLeft();
    EXPECT_NE((int)(wPlayerBattleStatus1 & (1u << BSTAT1_USING_TRAPPING)), 0);
}

TEST(CheckNumAttacksLeft, EnemyZero_ClearsTrapping) {
    battle_reset();
    wEnemyBattleStatus1 |= (1u << BSTAT1_USING_TRAPPING);
    wEnemyNumAttacksLeft = 0;
    Battle_CheckNumAttacksLeft();
    EXPECT_EQ((int)(wEnemyBattleStatus1 & (1u << BSTAT1_USING_TRAPPING)), 0);
}

TEST(RunTurn, FasterPlayer_EnemyFainted) {
    battle_reset();

    wBattleMon.spd = 100;
    wEnemyMon.spd  = 10;
    wEnemyMon.hp   = 1;

    battle_result_t r = Battle_RunTurn();

    EXPECT_EQ((int)r, (int)BATTLE_RESULT_ENEMY_FAINTED);

    EXPECT_EQ((int)wBattleMon.hp, 200);
}

TEST(RunTurn, FasterEnemy_PlayerFainted) {
    battle_reset();

    wBattleMon.spd = 10;
    wEnemyMon.spd  = 100;
    wBattleMon.hp  = 1;

    battle_result_t r = Battle_RunTurn();

    EXPECT_EQ((int)r, (int)BATTLE_RESULT_PLAYER_FAINTED);

    EXPECT_EQ((int)wEnemyMon.hp, 200);
}

TEST(RunTurn, QuickAttack_PlayerFirst_OverridesSpeed) {
    battle_reset();

    wPlayerSelectedMove = MOVE_QUICK_ATTACK;
    wBattleMon.spd = 10;
    wEnemyMon.spd  = 100;
    wEnemyMon.hp   = 1;

    battle_result_t r = Battle_RunTurn();

    EXPECT_EQ((int)r, (int)BATTLE_RESULT_ENEMY_FAINTED);
    EXPECT_EQ((int)wBattleMon.hp, 200);
}

TEST(RunTurn, Counter_EnemyGoesFirst_PlayerFainted) {
    battle_reset();

    wPlayerSelectedMove = MOVE_COUNTER;
    wBattleMon.spd = 100;
    wEnemyMon.spd  = 10;
    wBattleMon.hp  = 1;

    battle_result_t r = Battle_RunTurn();

    EXPECT_EQ((int)r, (int)BATTLE_RESULT_PLAYER_FAINTED);
    EXPECT_EQ((int)wEnemyMon.hp, 200);
}

TEST(RunTurn, BothSurvive_ReturnsContinue) {
    battle_reset();

    wBattleMon.spd = 100;
    wEnemyMon.spd  = 10;

    battle_result_t r = Battle_RunTurn();

    EXPECT_EQ((int)r, (int)BATTLE_RESULT_CONTINUE);
    EXPECT_LT((int)wEnemyMon.hp,  200);
    EXPECT_LT((int)wBattleMon.hp, 200);
}

TEST(RunTurn, EnemyPoisoned_DiesAfterMoving) {
    battle_reset();

    wBattleMon.spd = 10;
    wEnemyMon.spd  = 100;
    wEnemyMon.status   = STATUS_PSN;
    wEnemyMon.max_hp   = 16;
    wEnemyMon.hp       = 1;

    battle_result_t r = Battle_RunTurn();

    EXPECT_EQ((int)r, (int)BATTLE_RESULT_ENEMY_FAINTED);
    EXPECT_EQ((int)wEnemyMon.hp, 0);
}

TEST(RunTurn, PlayerPoisoned_DiesAfterMoving) {
    battle_reset();

    wBattleMon.spd = 100;
    wEnemyMon.spd  = 10;
    wBattleMon.status  = STATUS_PSN;
    wBattleMon.max_hp  = 16;
    wBattleMon.hp      = 1;

    wEnemyMon.max_hp   = 200;
    wEnemyMon.hp       = 200;

    battle_result_t r = Battle_RunTurn();

    EXPECT_EQ((int)r, (int)BATTLE_RESULT_PLAYER_FAINTED);
    EXPECT_EQ((int)wBattleMon.hp, 0);
}

TEST(RunTurn, LeechSeed_SeededEnemyFaster_SappedAfterOwnMove) {
    battle_reset();

    wBattleMon.spd = 10;
    wEnemyMon.spd  = 100;
    wEnemyBattleStatus2 |= (1u << BSTAT2_SEEDED);
    wEnemyMon.max_hp = 16;
    wEnemyMon.hp     = 1;

    fprintf(stderr, "[TESTLOG] === SeededEnemyFaster: enemy moves first, sapped before player moves ===\n");
    battle_result_t r = Battle_RunTurn();
    fprintf(stderr, "[TESTLOG] result=%d enemyHP=%d playerHP=%d\n",
            (int)r, (int)wEnemyMon.hp, (int)wBattleMon.hp);

    EXPECT_EQ((int)r, (int)BATTLE_RESULT_ENEMY_FAINTED);
    EXPECT_EQ((int)wEnemyMon.hp, 0);
}

TEST(RunTurn, LeechSeed_SeededPlayerFaster_SapsSelfHealsEnemy) {
    battle_reset();

    wBattleMon.spd = 100;
    wEnemyMon.spd  = 10;
    wBattleMon.def = 250;
    wBattleMon.max_hp = 160; wBattleMon.hp = 160;
    wPlayerBattleStatus2 |= (1u << BSTAT2_SEEDED);
    wEnemyMon.max_hp = 200;  wEnemyMon.hp = 50;

    fprintf(stderr, "[TESTLOG] === SeededPlayerFaster: player moves, saps self (-10), heals enemy (+10) before enemy moves ===\n");
    battle_result_t r = Battle_RunTurn();
    fprintf(stderr, "[TESTLOG] result=%d playerHP=%d (start 160) enemyHP=%d (start 50)\n",
            (int)r, (int)wBattleMon.hp, (int)wEnemyMon.hp);

    EXPECT_EQ((int)r, (int)BATTLE_RESULT_CONTINUE);
    EXPECT_LT((int)wBattleMon.hp, 160);
}

TEST(RunTurn, PlayerAlreadyAtZeroHP_ImmediateFaint) {
    battle_reset();
    wBattleMon.hp = 0;

    battle_result_t r = Battle_RunTurn();

    EXPECT_EQ((int)r, (int)BATTLE_RESULT_PLAYER_FAINTED);
    EXPECT_EQ((int)wInHandlePlayerMonFainted, 1);
}

TEST(RunTurn, EnemyAlreadyAtZeroHP_ImmediateFaint) {
    battle_reset();
    wEnemyMon.hp = 0;

    battle_result_t r = Battle_RunTurn();

    EXPECT_EQ((int)r, (int)BATTLE_RESULT_ENEMY_FAINTED);
    EXPECT_EQ((int)wInHandlePlayerMonFainted, 0);
}
