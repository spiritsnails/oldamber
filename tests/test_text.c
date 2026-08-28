
#include "test_runner.h"
#include "../src/data/font_data.h"
#include "../src/game/text.h"
#include "../src/platform/hardware.h"
#include "../src/game/constants.h"

TEST(Text, UppercaseAMapsToFontBase) {

    EXPECT_EQ(Font_CharToTile(0x80), 128);
}

TEST(Text, UppercaseZMappedCorrectly) {

    EXPECT_EQ(Font_CharToTile(0x99), 153);
}

TEST(Text, LowercaseAMappedCorrectly) {

    EXPECT_EQ(Font_CharToTile(0xA0), 160);
}

TEST(Text, LastFontChar) {

    EXPECT_EQ(Font_CharToTile(0xFF), 255);
}

TEST(Text, SpaceMapsToBlank) {

    EXPECT_EQ(Font_CharToTile(0x7F), BLANK_TILE_SLOT);
}

TEST(Text, BoxTopLeftCorner) {

    EXPECT_EQ(Font_CharToTile(0x79), 120);
}

TEST(Text, BoxHorizontalLine) {

    EXPECT_EQ(Font_CharToTile(0x7A), 121);
}

TEST(Text, BoxTopRightCorner) {

    EXPECT_EQ(Font_CharToTile(0x7B), 122);
}

TEST(Text, BoxVerticalLine) {

    EXPECT_EQ(Font_CharToTile(0x7C), 123);
}

TEST(Text, BoxBottomLeftCorner) {

    EXPECT_EQ(Font_CharToTile(0x7D), 124);
}

TEST(Text, BoxBottomRightCorner) {

    EXPECT_EQ(Font_CharToTile(0x7E), 125);
}

TEST(Text, ControlCharMapsToBlank) {

    EXPECT_EQ(Font_CharToTile(0x50), BLANK_TILE_SLOT);
    EXPECT_EQ(Font_CharToTile(0x4E), BLANK_TILE_SLOT);
    EXPECT_EQ(Font_CharToTile(0x00), BLANK_TILE_SLOT);
}

TEST(Text, FontTilesNonZero) {

    int nonzero = 0;
    for (int i = 0; i < 16; i++)
        if (gFontTiles[0][i]) nonzero = 1;
    EXPECT_TRUE(nonzero);
}

TEST(Text, BoxTilesNonZero) {

    int nonzero_corner = 0, nonzero_hline = 0;
    for (int i = 0; i < 16; i++) {
        if (gBoxTiles[0][i]) nonzero_corner = 1;
        if (gBoxTiles[1][i]) nonzero_hline  = 1;
    }
    EXPECT_TRUE(nonzero_corner);
    EXPECT_TRUE(nonzero_hline);
}

TEST(Text, DistinctFontGlyphs) {

    int same = 1;
    for (int i = 0; i < 16; i++)
        if (gFontTiles[0][i] != gFontTiles[1][i]) { same = 0; break; }
    EXPECT_FALSE(same);
}

TEST(Text, InitiallyClosed) {
    EXPECT_FALSE(Text_IsOpen());
}

TEST(Text, ShowBoxOpensDialog) {
    static const uint8_t hello[] = {0x87,0x84,0x8B,0x8B,0x8E,0x50};
    Text_ShowBox(hello);
    EXPECT_TRUE(Text_IsOpen());
    Text_Close();
}

TEST(Text, CloseEndsDialog) {
    static const uint8_t hi[] = {0x87,0x88,0x50};
    Text_ShowBox(hi);
    Text_Close();
    EXPECT_FALSE(Text_IsOpen());
}

TEST(Text, ShowBoxWritesToTileMap) {
    static const uint8_t msg[] = {0x80,0x50};
    Text_ShowBox(msg);

    EXPECT_EQ((int)wTileMap[12*20 + 0], 120);

    EXPECT_EQ((int)wTileMap[14*20 + 1], 128);
    Text_Close();
}

TEST(Text, TileSlotBoundsOK) {

    for (int c = 0; c <= 0xFF; c++) {
        int slot = Font_CharToTile((unsigned char)c);
        EXPECT_GE(slot, 0);
        EXPECT_LT(slot, 256);
    }
}
