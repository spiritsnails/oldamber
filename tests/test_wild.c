
#include "test_runner.h"
#include "../src/data/wild_data.h"
#include "../src/data/map_data.h"
#include "../src/game/missingno.h"
#include "../src/game/glitches.h"
#include "../src/game/gen2_species.h"
#include "../src/platform/hardware.h"
#include <string.h>

#define MAP_ROUTE_1     0x0C
#define MAP_PALLET_TOWN 0x00

TEST(Wild, PalletTownHasNoEncounters) {
    EXPECT_EQ((int)gWildGrass[MAP_PALLET_TOWN].rate, 0);
}

TEST(Wild, Route1HasEncounters) {

    EXPECT_GT((int)gWildGrass[MAP_ROUTE_1].rate, 0);
}

TEST(Wild, Route1HasTenSlots) {

    const wild_mons_t *w = &gWildGrass[MAP_ROUTE_1];
    if (w->rate == 0) return;

    int valid = 0;
    for (int i = 0; i < 10; i++) {
        if (w->slots[i].species > 0 && w->slots[i].level > 0)
            valid++;
    }
    EXPECT_EQ(valid, 10);
}

TEST(Wild, EncounterLevelsInRange) {

    for (int m = 0; m < NUM_MAPS; m++) {
        const wild_mons_t *w = &gWildGrass[m];
        if (!w->rate) continue;
        for (int i = 0; i < 10; i++) {
            EXPECT_GE((int)w->slots[i].level, 1);
            EXPECT_LT((int)w->slots[i].level, 101);
        }
    }
}

TEST(Wild, EncounterSpeciesInRange) {

    for (int m = 0; m < NUM_MAPS; m++) {
        const wild_mons_t *w = &gWildGrass[m];
        if (!w->rate) continue;
        for (int i = 0; i < 10; i++) {
            EXPECT_GE((int)w->slots[i].species, 1);
            EXPECT_LT((int)w->slots[i].species, 200);
        }
    }
}

TEST(Wild, MostMapsHaveNoEncounters) {

    int encounter_maps = 0;
    for (int m = 0; m < NUM_MAPS; m++)
        if (gWildGrass[m].rate > 0) encounter_maps++;

    EXPECT_GE(encounter_maps, 20);
    EXPECT_LT(encounter_maps, NUM_MAPS);
}

TEST(Wild, EncounterRateInRange) {
    for (int m = 0; m < NUM_MAPS; m++) {
        uint8_t rate = gWildGrass[m].rate;

        EXPECT_TRUE(rate == 0 || rate > 0);
        if (rate > 0) EXPECT_LT((int)rate, 256);
    }
}

TEST(Wild, CustomOddNameProducesLevel80M00) {
    uint8_t species = 0;
    uint8_t level = 0;
    base_stats_t bs;

    Glitches_SetEnabled(1);
    Glitches_SetMissingNoEnabled(1);
    memset(wPlayerName, 0, NAME_LENGTH);
    wPlayerName[0] = 0x80;
    wPlayerName[1] = 0x92;
    wPlayerName[2] = 0x87;
    wPlayerName[3] = 0x50;
    MissingNo_CaptureOldManName();

    EXPECT_TRUE(MissingNo_TryCinnabarEncounter(
        0x08, 19, 8, 1, 0, 60, &species, &level));
    EXPECT_EQ((int)species, SPECIES_M_00_INTERNAL);
    EXPECT_EQ((int)level, 80);
    EXPECT_TRUE(MissingNo_IsM00(species));
    EXPECT_TRUE(Species_GetBaseStats(species, &bs));
    EXPECT_EQ((int)bs.hp, 33);
    EXPECT_EQ((int)bs.atk, 137);
    EXPECT_EQ((int)bs.def, 0);
    EXPECT_EQ((int)bs.spd, 6);
    EXPECT_EQ((int)bs.spc, 29);
    EXPECT_EQ((int)bs.catch_rate, 3);
}

TEST(Wild, LaterCustomNameSlotProducesLevel0M00) {
    uint8_t species = 0;
    uint8_t level = 0xff;

    memset(wPlayerName, 0, NAME_LENGTH);
    wPlayerName[0] = 0x80;
    wPlayerName[1] = 0x92;
    wPlayerName[2] = 0x87;
    wPlayerName[3] = 0x50;
    MissingNo_CaptureOldManName();

    EXPECT_TRUE(MissingNo_TryCinnabarEncounter(
        0x08, 19, 8, 1, 0, 110, &species, &level));
    EXPECT_EQ((int)species, SPECIES_M_00_INTERNAL);
    EXPECT_EQ((int)level, 0);
}
