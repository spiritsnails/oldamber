
#include "money_box.h"
#include "constants.h"
#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "../platform/hardware.h"
#include "../data/font_data.h"
#include <stdint.h>

#define MB_L 11
#define MB_T 0
#define MB_R 19
#define MB_B 2

#define GC_TL 0x79u
#define GC_H  0x7Au
#define GC_TR 0x7Bu
#define GC_V  0x7Cu
#define GC_BL 0x7Du
#define GC_BR 0x7Eu
#define GC_SP 0x7Fu
#define GC_YEN 0xF0u

static void mb_set(int col, int row, uint8_t code) {
    if (col < 0 || col >= SCREEN_WIDTH || row < 0 || row >= SCREEN_HEIGHT) return;

    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = (uint8_t)Font_CharToTile(code);
}

static uint32_t mb_money(void) {
    return ((wPlayerMoney[0] >> 4) & 0xF) * 100000u + (wPlayerMoney[0] & 0xF) * 10000u
         + ((wPlayerMoney[1] >> 4) & 0xF) * 1000u   + (wPlayerMoney[1] & 0xF) * 100u
         + ((wPlayerMoney[2] >> 4) & 0xF) * 10u     + (wPlayerMoney[2] & 0xF);
}

static int s_active = 0;

static void mb_draw_all(void) {

    mb_set(MB_L, MB_T, GC_TL);
    mb_set(MB_R, MB_T, GC_TR);
    mb_set(MB_L, MB_B, GC_BL);
    mb_set(MB_R, MB_B, GC_BR);
    for (int c = MB_L + 1; c < MB_R; c++) { mb_set(c, MB_T, GC_H); mb_set(c, MB_B, GC_H); }
    for (int r = MB_T + 1; r < MB_B; r++) {
        mb_set(MB_L, r, GC_V);
        mb_set(MB_R, r, GC_V);
        for (int c = MB_L + 1; c < MB_R; c++) mb_set(c, r, GC_SP);
    }

    { static const uint8_t kMoney[] = { 0x8Cu,0x8Eu,0x8Du,0x84u,0x98u };
      for (int i = 0; i < 5; i++) mb_set(13 + i, MB_T, kMoney[i]); }

    uint32_t v = mb_money();
    uint8_t d[6];
    for (int i = 5; i >= 0; i--) { d[i] = (uint8_t)(v % 10u); v /= 10u; }
    int nsig = 1;
    for (int i = 0; i < 5; i++) { if (d[i] != 0) { nsig = 6 - i; break; } }
    int start = 12 + (7 - (nsig + 1));
    for (int c = 12; c < start; c++) mb_set(c, 1, GC_SP);
    mb_set(start, 1, GC_YEN);
    for (int i = 0; i < nsig; i++) mb_set(start + 1 + i, 1, (uint8_t)(0xF6u + d[6 - nsig + i]));

    NPC_HideOverUITiles();
    Player_HideIfOverUI();
}

void MoneyBox_Draw(void) {
    s_active = 1;
    mb_draw_all();
}

void MoneyBox_Refresh(void) {
    if (s_active) mb_draw_all();
}

int MoneyBox_IsActive(void) { return s_active; }

void MoneyBox_Clear(void) {
    s_active = 0;

    Map_BuildScrollView();
    NPC_BuildView(gScrollPxX, gScrollPxY);
}
