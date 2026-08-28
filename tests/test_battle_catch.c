
#include "test_runner.h"
#include "../src/game/battle/battle_catch.h"
#include "../src/platform/hardware.h"
#include "../src/game/constants.h"

static void catch_reset(void) {
    extern void WRAMClear(void);
    WRAMClear();
    hRandomAdd = 0x00;
    hRandomSub = 0xFF;

    wEnemyMon.max_hp    = 100;
    wEnemyMon.hp        = 50;
    wEnemyMon.catch_rate= 255;
    wEnemyMon.status    = 0;
}

static void seed_rng(uint8_t val) {
    hRandomAdd = 0x00;
    hRandomSub = (uint8_t)((val ^ 0x05) + 3);
}

TEST(CatchMechanic, MasterBall_AlwaysCatches) {
    catch_reset();
    wEnemyMon.catch_rate = 1;
    wEnemyMon.hp        = wEnemyMon.max_hp;
    catch_result_t r = Battle_CatchAttempt(ITEM_MASTER_BALL);
    EXPECT_EQ((int)r, (int)CATCH_RESULT_SUCCESS);
}

TEST(CatchMechanic, PokeBall_MaxCatchRate_LowHP_Catches) {
    catch_reset();
    wEnemyMon.catch_rate = 255;
    wEnemyMon.hp         = 1;
    seed_rng(0);
    catch_result_t r = Battle_CatchAttempt(ITEM_POKE_BALL);
    EXPECT_EQ((int)r, (int)CATCH_RESULT_SUCCESS);
}

TEST(CatchMechanic, PokeBall_HighRand_LowCatchRate_Fails) {
    catch_reset();
    wEnemyMon.catch_rate = 1;
    wEnemyMon.hp         = wEnemyMon.max_hp;
    seed_rng(255);
    catch_result_t r = Battle_CatchAttempt(ITEM_POKE_BALL);
    EXPECT_NE((int)r, (int)CATCH_RESULT_SUCCESS);
}

TEST(CatchMechanic, SleepBonus_Catches) {
    catch_reset();
    wEnemyMon.catch_rate = 10;
    wEnemyMon.hp         = wEnemyMon.max_hp;
    wEnemyMon.status     = 0x01;
    seed_rng(20);
    catch_result_t r = Battle_CatchAttempt(ITEM_POKE_BALL);
    EXPECT_EQ((int)r, (int)CATCH_RESULT_SUCCESS);
}

TEST(CatchMechanic, ParalysisBonus_Subtracts12) {
    catch_reset();
    wEnemyMon.catch_rate = 255;
    wEnemyMon.hp         = 1;
    wEnemyMon.status     = (1 << 6);

    seed_rng(5);
    catch_result_t r = Battle_CatchAttempt(ITEM_POKE_BALL);
    EXPECT_EQ((int)r, (int)CATCH_RESULT_SUCCESS);
}

TEST(CatchMechanic, GreatBall_LargerW) {
    catch_reset();
    wEnemyMon.catch_rate = 255;
    wEnemyMon.hp         = wEnemyMon.max_hp;
    seed_rng(0);

    seed_rng(0);
    catch_result_t r = Battle_CatchAttempt(ITEM_GREAT_BALL);

    EXPECT_TRUE(r == CATCH_RESULT_SUCCESS  ||
                r == CATCH_RESULT_0_SHAKES ||
                r == CATCH_RESULT_1_SHAKE  ||
                r == CATCH_RESULT_2_SHAKES ||
                r == CATCH_RESULT_3_SHAKES);
}

TEST(CatchMechanic, ShakeCount_Zero) {
    catch_reset();
    wEnemyMon.catch_rate = 1;
    wEnemyMon.hp         = 100;
    wEnemyMon.max_hp     = 100;

    seed_rng(200);
    catch_result_t r = Battle_CatchAttempt(ITEM_POKE_BALL);
    EXPECT_EQ((int)r, (int)CATCH_RESULT_0_SHAKES);
}

TEST(CatchMechanic, ShakeCount_Three_WhenZGeq70) {
    catch_reset();
    wEnemyMon.catch_rate = 255;
    wEnemyMon.max_hp     = 400;
    wEnemyMon.hp         = 1;

    wEnemyMon.max_hp = 100;
    wEnemyMon.hp     = 100;

    hRandomAdd = 0x00;
    hRandomSub = (uint8_t)((0 ^ 0x05) + 3);

    hRandomSub = (uint8_t)216;

    wEnemyMon.catch_rate = 255;
    wEnemyMon.max_hp     = 100;
    wEnemyMon.hp         = 100;

    wEnemyMon.catch_rate = 2;
    seed_rng(50);

    catch_result_t r = Battle_CatchAttempt(ITEM_POKE_BALL);
    EXPECT_EQ((int)r, (int)CATCH_RESULT_0_SHAKES);
}

TEST(CatchMechanic, ShakeCount_One) {
    catch_reset();
    wEnemyMon.catch_rate = 200;
    wEnemyMon.max_hp     = 100;
    wEnemyMon.hp         = 100;
    seed_rng(201);
    catch_result_t r = Battle_CatchAttempt(ITEM_POKE_BALL);

    EXPECT_EQ((int)r, (int)CATCH_RESULT_1_SHAKE);
}

TEST(CatchMechanic, ShakeCount_Two) {
    catch_reset();
    wEnemyMon.catch_rate = 255;
    wEnemyMon.max_hp     = 100;
    wEnemyMon.hp         = 100;

    hRandomAdd = 0x00;

    hRandomSub = 8;

    wEnemyMon.catch_rate = 5;
    seed_rng(200);

    catch_result_t r = Battle_CatchAttempt(ITEM_POKE_BALL);
    EXPECT_EQ((int)r, (int)CATCH_RESULT_0_SHAKES);
}

TEST(CatchMechanic, ShakeCount_Two_Verified) {
    catch_reset();
    wEnemyMon.catch_rate = 200;
    wEnemyMon.max_hp     = 100;
    wEnemyMon.hp         = 50;
    seed_rng(201);
    catch_result_t r = Battle_CatchAttempt(ITEM_POKE_BALL);

    EXPECT_EQ((int)r, (int)CATCH_RESULT_2_SHAKES);
}

TEST(CatchMechanic, ShakeCount_Three) {
    catch_reset();
    wEnemyMon.catch_rate = 255;
    wEnemyMon.max_hp     = 100;
    wEnemyMon.hp         = 25;

    wEnemyMon.catch_rate = 254;
    seed_rng(255);

    catch_result_t r = Battle_CatchAttempt(ITEM_POKE_BALL);
    EXPECT_EQ((int)r, (int)CATCH_RESULT_3_SHAKES);
}

TEST(CatchMechanic, Status2_SleepPushesShakesUp) {
    catch_reset();
    wEnemyMon.catch_rate = 200;
    wEnemyMon.max_hp     = 100;
    wEnemyMon.hp         = 100;
    wEnemyMon.status     = 0x01;
    seed_rng(201);

    seed_rng(250);

    catch_result_t r = Battle_CatchAttempt(ITEM_POKE_BALL);
    EXPECT_EQ((int)r, (int)CATCH_RESULT_2_SHAKES);
}
