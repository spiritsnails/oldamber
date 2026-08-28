
#include "test_runner.h"
#include "../src/data/wild_data.h"
#include "../src/data/map_data.h"

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
