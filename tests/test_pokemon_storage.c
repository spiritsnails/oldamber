
#include "test_runner.h"
#include "../src/game/pokemon.h"
#include "../src/game/constants.h"
#include "../src/platform/hardware.h"
#include "../src/data/base_stats.h"

static void storage_reset(void) {
    extern void WRAMClear(void);
    WRAMClear();
    wCurrentBoxNum = 0;
}

static void write_exp(uint8_t out[3], uint32_t exp) {
    out[0] = (uint8_t)((exp >> 16) & 0xFF);
    out[1] = (uint8_t)((exp >> 8) & 0xFF);
    out[2] = (uint8_t)(exp & 0xFF);
}

TEST(PokemonStorage, DepositUsesPartyLevelWhenBaseLevelStale) {
    storage_reset();

    wPartyCount = 1;
    Pokemon_InitMon(&wPartyMons[0], SPECIES_PIDGEY, 3);
    wPartyMons[0].base.box_level = 0;
    wPartyMons[0].level = 3;
    wPartyMons[0].base.species = SPECIES_PIDGEY;

    EXPECT_TRUE(Pokemon_DepositPartyMonToBox(0));
    EXPECT_EQ((int)wBoxCount[0], 1);
    EXPECT_EQ((int)wBoxMons[0][0].box_level, 3);

    EXPECT_TRUE(Pokemon_WithdrawBoxMonToParty(0));
    EXPECT_EQ((int)wPartyCount, 1);
    EXPECT_EQ((int)wPartyMons[0].level, 3);
    EXPECT_EQ((int)wPartyMons[0].base.box_level, 3);
}

TEST(PokemonStorage, WithdrawRecoversLevelFromExpWhenBoxLevelZero) {
    storage_reset();

    uint8_t species = SPECIES_PIDGEY;
    uint8_t dex = gSpeciesToDex[species];
    uint8_t growth = gBaseStats[dex].growth_rate;
    uint32_t exp_l5 = CalcExpForLevel(growth, 5);

    wBoxCount[0] = 1;
    wBoxMons[0][0].species = species;
    wBoxMons[0][0].box_level = 0;
    wBoxMons[0][0].hp = 10;
    wBoxMons[0][0].type1 = gBaseStats[dex].type1;
    wBoxMons[0][0].type2 = gBaseStats[dex].type2;
    wBoxMons[0][0].catch_rate = gBaseStats[dex].catch_rate;
    write_exp(wBoxMons[0][0].exp, exp_l5);

    EXPECT_TRUE(Pokemon_WithdrawBoxMonToParty(0));
    EXPECT_EQ((int)wPartyCount, 1);
    EXPECT_EQ((int)wPartyMons[0].level, 5);
    EXPECT_EQ((int)wPartyMons[0].base.box_level, 5);
}
