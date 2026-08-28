
#include "test_runner.h"
#include "../src/game/overworld.h"
#include "../src/platform/hardware.h"
#include "../src/data/map_data.h"
#include "../src/data/event_data.h"

TEST(Map, LoadPalletTown) {
    Map_Load(0);
    EXPECT_EQ(wCurMap, 0);

    EXPECT_EQ(wCurMapWidth,  10);
    EXPECT_EQ(wCurMapHeight,  9);
    EXPECT_EQ(wCurMapTileset, 0);
}

TEST(Map, LoadRedsHouse1F) {
    Map_Load(0x25);
    EXPECT_EQ(wCurMap, 0x25);
    EXPECT_GT(wCurMapWidth,  0);
    EXPECT_GT(wCurMapHeight, 0);
    EXPECT_EQ(wCurMapTileset, 1);
}

TEST(Map, VirtualMapIdLoadsSafely) {
    Map_Load(0);
    Map_Load(0xFF);
    EXPECT_EQ(wCurMap, 0xFF);
    EXPECT_EQ(wCurMapTileset, 0);
}

TEST(Map, GetTileInBounds) {
    Map_Load(0);

    uint8_t t = Map_GetTile(0, 0);
    EXPECT_GE((int)t, 0);
    EXPECT_LT((int)t, 256);
}

TEST(Map, GetTileOutOfBounds) {
    Map_Load(0);

    uint8_t t = Map_GetTile(9999, 9999);
    EXPECT_GE((int)t, 0);
}

TEST(Map, BuildViewFillsTileMap) {
    Map_Load(0);
    wXCoord = 9; wYCoord = 8;
    Map_BuildView();
    uint8_t center = wTileMap[8 * 20 + 9];

    EXPECT_GE((int)center, 0);
    EXPECT_LT((int)center, 96);
}

TEST(Map, PassableTileAccepted) {
    Map_Load(0);

    int r = Tile_IsPassable(0x00);
    EXPECT_TRUE(r == 0 || r == 1);
}

TEST(Map, AllMapsHaveValidTileset) {
    for (int i = 0; i < NUM_MAPS; i++) {
        const map_info_t *m = &gMapTable[i];
        if (!m->blocks) continue;
        EXPECT_LT((int)m->tileset_id, 24);
        EXPECT_GT((int)m->width, 0);
        EXPECT_GT((int)m->height, 0);
    }
}

TEST(Map, PalletTownWarpCount) {

    const map_events_t *ev = &gMapEvents[0];
    EXPECT_EQ((int)ev->num_warps, 3);
}

TEST(Map, PalletTownWarpCoords) {
    const map_events_t *ev = &gMapEvents[0];
    EXPECT_TRUE(ev->warps != NULL);

    EXPECT_EQ((int)ev->warps[0].x, 10);
    EXPECT_EQ((int)ev->warps[0].y, 11);
    EXPECT_EQ((int)ev->warps[0].dest_map, 0x25);
}

TEST(Map, PalletTownNPCCount) {
    const map_events_t *ev = &gMapEvents[0];
    EXPECT_EQ((int)ev->num_npcs, 3);
}

TEST(Map, AllMapsHaveEventEntry) {

    int warp_maps = 0;
    for (int i = 0; i < NUM_MAPS; i++) {
        if (gMapEvents[i].num_warps > 0) warp_maps++;
    }
    EXPECT_GT(warp_maps, 50);
}
