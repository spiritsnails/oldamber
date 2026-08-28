
#include "coin_box.h"
#include "constants.h"
#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "../platform/hardware.h"
#include "../data/font_data.h"
#include <stdint.h>

#define CB_L 11
#define CB_T 0
#define CB_R 19
#define CB_B 6

#define CB_CONTENT_L 12
#define CB_CONTENT_R 18
#define CB_MONEY_LABEL_ROW 2
#define CB_MONEY_ROW        3
#define CB_COIN_LABEL_ROW  4
#define CB_COIN_ROW         5

#define GC_TL 0x79u
#define GC_H  0x7Au
#define GC_TR 0x7Bu
#define GC_V  0x7Cu
#define GC_BL 0x7Du
#define GC_BR 0x7Eu
#define GC_SP 0x7Fu
#define GC_YEN 0xF0u

static void cb_set(int col, int row, uint8_t code) {
    if (col < 0 || col >= SCREEN_WIDTH || row < 0 || row >= SCREEN_HEIGHT) return;
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = (uint8_t)Font_CharToTile(code);
}

static uint32_t cb_money(void) {
    return ((wPlayerMoney[0] >> 4) & 0xF) * 100000u + (wPlayerMoney[0] & 0xF) * 10000u
         + ((wPlayerMoney[1] >> 4) & 0xF) * 1000u   + (wPlayerMoney[1] & 0xF) * 100u
         + ((wPlayerMoney[2] >> 4) & 0xF) * 10u     + (wPlayerMoney[2] & 0xF);
}

static uint32_t cb_coins(void) {
    return ((wPlayerCoins[0] >> 4) & 0xF) * 1000u + (wPlayerCoins[0] & 0xF) * 100u
         + ((wPlayerCoins[1] >> 4) & 0xF) * 10u   + (wPlayerCoins[1] & 0xF);
}

static int s_active = 0;

static void cb_draw_all(void) {

    cb_set(CB_L, CB_T, GC_TL);
    cb_set(CB_R, CB_T, GC_TR);
    cb_set(CB_L, CB_B, GC_BL);
    cb_set(CB_R, CB_B, GC_BR);
    for (int c = CB_L + 1; c < CB_R; c++) { cb_set(c, CB_T, GC_H); cb_set(c, CB_B, GC_H); }
    for (int r = CB_T + 1; r < CB_B; r++) {
        cb_set(CB_L, r, GC_V);
        cb_set(CB_R, r, GC_V);
        for (int c = CB_L + 1; c < CB_R; c++) cb_set(c, r, GC_SP);
    }

    { static const uint8_t kMoney[] = { 0x8Cu,0x8Eu,0x8Du,0x84u,0x98u };
      for (int i = 0; i < 5; i++) cb_set(CB_CONTENT_L + i, CB_MONEY_LABEL_ROW, kMoney[i]); }

    {
        uint32_t v = cb_money();
        uint8_t d[6];
        for (int i = 5; i >= 0; i--) { d[i] = (uint8_t)(v % 10u); v /= 10u; }
        cb_set(CB_CONTENT_L, CB_MONEY_ROW, GC_YEN);
        for (int i = 0; i < 6; i++) cb_set(CB_CONTENT_L + 1 + i, CB_MONEY_ROW, (uint8_t)(0xF6u + d[i]));
    }

    { static const uint8_t kCoin[] = { 0x82u,0x8Eu,0x88u,0x8Du };
      for (int i = 0; i < 4; i++) cb_set(CB_CONTENT_L + i, CB_COIN_LABEL_ROW, kCoin[i]); }

    {
        uint32_t v = cb_coins();
        uint8_t d[4];
        for (int i = 3; i >= 0; i--) { d[i] = (uint8_t)(v % 10u); v /= 10u; }
        for (int i = 0; i < 4; i++) cb_set(CB_CONTENT_L + 3 + i, CB_COIN_ROW, (uint8_t)(0xF6u + d[i]));
    }

    NPC_HideOverUITiles();
    Player_HideIfOverUI();
}

void CoinBox_Draw(void) {
    s_active = 1;
    cb_draw_all();
}

void CoinBox_Refresh(void) {
    if (s_active) cb_draw_all();
}

int CoinBox_IsActive(void) { return s_active; }

void CoinBox_Clear(void) {
    s_active = 0;
    Map_BuildScrollView();
    NPC_BuildView(gScrollPxX, gScrollPxY);
}
