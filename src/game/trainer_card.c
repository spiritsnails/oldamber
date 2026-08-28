
#include "trainer_card.h"
#include "menu.h"
#include "overworld.h"
#include "constants.h"
#include "../data/font_data.h"
#include "../data/hof_player_sprites.h"
#include "../data/trainer_card_tiles.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../platform/audio.h"
#include "gbc_color.h"
#include <stdio.h>
#include <string.h>

extern uint8_t  gScrollTileMap[];
extern uint8_t  wPlayerName[];
extern uint8_t  wObtainedBadges;
extern uint8_t  wPlayerMoney[3];
extern unsigned long gPlayTimeFrames;

#define TC_PIC_BASE    25
#define TC_BADGE_BASE  74
#define TC_NUM_BASE    106
#define TC_CIRCLE_SLOT 114
#define TC_BOX_BASE    115
#define TC_COLON_SLOT  124

#define TC_BOXT(role)  ((uint8_t)(TC_BOX_BASE + (role)))

#define TC_WHITEOUT_FRAMES 14

#define TC_PAL_WHITE_R  0x00
#define TC_PAL_NORMAL_R 0xE4
#define TC_PAL_NORMAL_G 0xD0
#define TC_PAL_NORMAL_B 0xE0

static int s_open  = 0;
static int s_fade  = 0;
static int s_close = 0;

static void tc_set(int col, int row, uint8_t tile) {
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
}

static int tc_ascii_tile(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return Font_CharToTile((uint8_t)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return Font_CharToTile((uint8_t)(0xA0 + (c - 'a')));
    if (c >= '0' && c <= '9') return Font_CharToTile((uint8_t)(0xF6 + (c - '0')));
    if (c == '/')  return Font_CharToTile(0xF3);
    if (c == ' ')  return BLANK_TILE_SLOT;
    return BLANK_TILE_SLOT;
}
static void tc_str(int col, int row, const char *s) {
    for (; *s; s++, col++)
        tc_set(col, row, (uint8_t)tc_ascii_tile((unsigned char)*s));
}

static void tc_player_name(int col, int row) {
    for (int i = 0; i < NAME_LENGTH; i++) {
        uint8_t b = wPlayerName[i];
        if (b == 0x00 || b == 0x50) break;
        tc_set(col++, row, (uint8_t)Font_CharToTile(b));
    }
}

static uint32_t tc_money_value(void) {
    uint32_t v = 0;
    for (int i = 0; i < 3; i++)
        v = v * 100 + ((wPlayerMoney[i] >> 4) * 10) + (wPlayerMoney[i] & 0x0F);
    return v;
}

static void tc_money(int col, int row) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)tc_money_value());
    tc_set(col, row, (uint8_t)Font_CharToTile(0xF0));
    tc_str(col + 1, row, buf);
}

static void tc_time(int col, int row) {
    unsigned long secs = gPlayTimeFrames / 60;
    int hrs  = (int)(secs / 3600); if (hrs > 999) hrs = 999;
    int mins = (int)((secs / 60) % 60);
    char hb[8];
    int n = snprintf(hb, sizeof(hb), "%d", hrs);
    tc_str(col, row, hb);
    int cx = col + n;
    tc_set(cx,     row, (uint8_t)TC_COLON_SLOT);
    tc_set(cx + 1, row, (uint8_t)Font_CharToTile((uint8_t)(0xF6 + mins / 10)));
    tc_set(cx + 2, row, (uint8_t)Font_CharToTile((uint8_t)(0xF6 + mins % 10)));
}

static void tc_clear(void) {

    memset(gScrollTileMap, (uint8_t)BLANK_TILE_SLOT,
           (size_t)SCROLL_MAP_W * SCROLL_MAP_H);
}

static void tc_box(int L, int T, int R, int B) {
    tc_set(L, T, TC_BOXT(TC_BOX_UL));
    tc_set(R, T, TC_BOXT(TC_BOX_UR));
    tc_set(L, B, TC_BOXT(TC_BOX_LL));
    tc_set(R, B, TC_BOXT(TC_BOX_LR));
    for (int c = L + 1; c < R; c++) {
        tc_set(c, T, TC_BOXT(TC_BOX_TOP));
        tc_set(c, B, TC_BOXT(TC_BOX_BOTTOM));
    }
    for (int r = T + 1; r < B; r++) {
        tc_set(L, r, TC_BOXT(TC_BOX_LEFT));
        tc_set(R, r, TC_BOXT(TC_BOX_RIGHT));
    }
}

static void tc_vwall(int col, int top, int bot) {
    for (int r = top; r <= bot; r++)
        tc_set(col, r, TC_BOXT(TC_BOX_BG));
}

#define TC_PIC_ROW 1
static void tc_draw_pic(void) {
    for (int i = 0; i < 49; i++)
        Display_LoadTile((uint8_t)(TC_PIC_BASE + i), kHofRedFrontSprite[i]);

    for (int ty = 0; ty < 7; ty++)
        for (int tx = 1; tx <= 5; tx++)
            tc_set(14 + tx, TC_PIC_ROW + ty, (uint8_t)(TC_PIC_BASE + ty * 7 + tx));
}

static void tc_draw_badges(void) {
    static const int col_x[4] = { 2, 6, 10, 14 };

    for (int i = 0; i < TRAINER_CARD_NUM_BADGES; i++)
        Display_LoadTile((uint8_t)(TC_NUM_BASE + i), kBadgeNumberTiles[i]);

    for (int slot = 0; slot < TRAINER_CARD_NUM_BADGES; slot++) {
        int owned = (wObtainedBadges >> slot) & 1;
        const uint8_t (*gfx)[16] = owned ? kTrainerBadgeTiles[slot]
                                         : kTrainerFaceTiles[slot];
        int base = TC_BADGE_BASE + slot * 4;
        for (int t = 0; t < 4; t++)
            Display_LoadTile((uint8_t)(base + t), gfx[t]);

        int nx   = col_x[slot & 3];
        int ynum = (slot < 4) ? 11 : 14;
        int ygfx = ynum + 1;
        tc_set(nx,     ynum,     (uint8_t)(TC_NUM_BASE + slot));
        tc_set(nx + 1, ygfx,     (uint8_t)(base + 0));
        tc_set(nx + 2, ygfx,     (uint8_t)(base + 1));
        tc_set(nx + 1, ygfx + 1, (uint8_t)(base + 2));
        tc_set(nx + 2, ygfx + 1, (uint8_t)(base + 3));
    }
}

void TrainerCard_Open(void) {
    s_open  = 1;
    s_close = 0;

    for (int i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
    hWY = 144;

    for (int i = 0; i < 9; i++)
        Display_LoadTile((uint8_t)(TC_BOX_BASE + i), kTrainerInfoBoxTiles[i]);
    Display_LoadTile(TC_COLON_SLOT, kTimeColonTile);

    tc_clear();

    tc_draw_pic();
    tc_box(0, 0, 19, 7);
    tc_box(1, 10, 18, 17);
    tc_vwall(0, 10, 17);
    tc_vwall(19, 10, 17);

    tc_str(2, 2, "NAME/");  tc_player_name(7, 2);
    tc_str(2, 4, "MONEY/"); tc_money(8, 4);
    tc_str(2, 6, "TIME/");  tc_time(9, 6);

    Display_LoadTile(TC_CIRCLE_SLOT, kBadgeCircleTile);
    tc_set(6, 9, TC_CIRCLE_SLOT);
    tc_str(7, 9, "BADGES");
    tc_set(13, 9, TC_CIRCLE_SLOT);
    tc_draw_badges();

    GbcColor_SetPalTrainerCard(wObtainedBadges);

    Display_SetPalette(TC_PAL_WHITE_R, TC_PAL_WHITE_R, TC_PAL_WHITE_R);
    s_fade = TC_WHITEOUT_FRAMES;
}

int TrainerCard_IsOpen(void) { return s_open; }

void TrainerCard_Tick(void) {
    if (!s_open) return;

    if (s_fade > 0) {
        if (--s_fade == 0)
            Display_SetPalette(TC_PAL_NORMAL_R, TC_PAL_NORMAL_G, TC_PAL_NORMAL_B);
        return;
    }

    if (s_close > 0) {
        if (--s_close == 0) {
            s_open = 0;
            Menu_ResumeFromBag();

            GbcColor_MarkDirty();
            Display_LoadMapPalette();
        }
        return;
    }

    if (hJoyPressed & (PAD_A | PAD_B)) {
        Audio_PlaySFX_PressAB();
        Display_SetPalette(TC_PAL_WHITE_R, TC_PAL_WHITE_R, TC_PAL_WHITE_R);
        s_close = TC_WHITEOUT_FRAMES;
    }
}
