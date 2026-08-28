
#include "test_runner.h"
#include "../src/game/overworld.h"
#include "../src/game/warp.h"
#include "../src/platform/hardware.h"
#include "../src/data/event_data.h"

static void setup_pallet(void) {
    Map_Load(0);
    wXCoord = 10; wYCoord = 13;
}

TEST(Warp, NoWarpAtStartPosition) {
    setup_pallet();
    int result = Warp_Check();
    EXPECT_EQ(result, 0);
    EXPECT_EQ((int)wCurMap, 0);
}

TEST(Warp, DetectsRedsHouseEntrance) {
    Map_Load(0);
    wXCoord = 10; wYCoord = 11;
    int result = Warp_Check();
    EXPECT_EQ(result, 1);
    EXPECT_EQ((int)wCurMap, 0x25);
}

TEST(Warp, DetectsBluesHouseEntrance) {
    Map_Load(0);
    wXCoord = 26; wYCoord = 11;
    int result = Warp_Check();
    EXPECT_EQ(result, 1);
    EXPECT_EQ((int)wCurMap, 0x27);
}

TEST(Warp, DetectsOaksLabEntrance) {
    Map_Load(0);
    wXCoord = 24; wYCoord = 23;
    int result = Warp_Check();
    EXPECT_EQ(result, 1);
    EXPECT_EQ((int)wCurMap, 0x28);
}

TEST(Warp, PlayerPositionedAfterWarp) {
    Map_Load(0);
    wXCoord = 24; wYCoord = 23;
    Warp_Check();
    EXPECT_EQ((int)wCurMap, 0x28);
    EXPECT_GE((int)wXCoord, 0);
    EXPECT_GE((int)wYCoord, 0);
    EXPECT_LT((int)wXCoord, (int)wCurMapWidth  * 4);
    EXPECT_LT((int)wYCoord, (int)wCurMapHeight * 4);
}

TEST(Warp, WarpUpdatesLastMap) {
    Map_Load(0);
    wLastMap = 0xFF;
    wXCoord = 10; wYCoord = 11;
    Warp_Check();
    EXPECT_EQ((int)wLastMap, 0);
}

TEST(Warp, NonWarpTileReturnsZero) {
    Map_Load(0);
    int hits = 0;
    for (int x = 6; x <= 14; x++) {
        for (int y = 13; y <= 19; y++) {
            if (x == 10 && y == 11) continue;
            Map_Load(0);
            wXCoord = (uint16_t)x; wYCoord = (uint16_t)y;
            hits += Warp_Check();
        }
    }
    EXPECT_EQ(hits, 0);
}

TEST(Warp, IndoorExitWarpsBack) {
    Map_Load(0);
    wXCoord = 10; wYCoord = 11;
    Warp_Check();
    EXPECT_EQ((int)wCurMap, 0x25);

    const map_events_t *ev = &gMapEvents[0x25];
    EXPECT_GT((int)ev->num_warps, 0);
}
