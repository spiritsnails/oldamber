
#include "prize_list_choice.h"
#include "text.h"
#include "npc.h"
#include "pokemon.h"
#include "inventory.h"
#include "overworld.h"
#include "player.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "../data/font_data.h"
#include <string.h>
#include <stdio.h>

#define PL_TMIDX(r, c) (((r) + 2) * SCROLL_MAP_W + ((c) + 2) + Map_UiColOfs())
#define PL_TL 0x79u
#define PL_H  0x7Au
#define PL_TR 0x7Bu
#define PL_V  0x7Cu
#define PL_BL 0x7Du
#define PL_BR 0x7Eu
#define PL_SP 0x7Fu
#define PL_CUR 0xEDu
#define PL_YEN 0xF0u

#define PL_BOX_L 0
#define PL_BOX_T 2
#define PL_BOX_H 10
#define PL_BOX_W 18
#define PL_ARROW_COL 1
#define PL_ITEM_COL  2
#define PL_ITEM_ROW0 4
#define PL_ITEM_STEP 2
#define PL_PRICE_COL 13
#define PL_PRICE_ROW0 5
#define PL_NOTHANKS_ROW 10

#define PL_COIN_L 11
#define PL_COIN_T 0
#define PL_COIN_H 3
#define PL_COIN_W 9

typedef enum { PL_IDLE, PL_OPEN } pl_state_t;
static pl_state_t g_state = PL_IDLE;
static int      g_kind = PRIZE_KIND_MON;
static uint8_t  g_entries[3];
static uint16_t g_prices[3];
static int      g_cursor = 0;
static int      g_result = 0;

static void pl_set(int col, int row, uint8_t tile) {
    if (col < 0 || col >= SCREEN_WIDTH || row < 0 || row >= SCREEN_HEIGHT) return;
    gScrollTileMap[PL_TMIDX(row, col)] = tile;
}

static uint8_t pl_ct(char c) {
    if (c >= 'A' && c <= 'Z') return (uint8_t)Font_CharToTile((unsigned char)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return (uint8_t)Font_CharToTile((unsigned char)(0xA0 + (c - 'a')));
    if (c >= '0' && c <= '9') return (uint8_t)Font_CharToTile((unsigned char)(0xF6 + (c - '0')));
    return (uint8_t)Font_CharToTile(PL_SP);
}

static void pl_label(int col, int row, const char *s) {
    while (*s) pl_set(col++, row, pl_ct(*s++));
}

static void pl_box(int top, int left, int h, int w) {
    for (int r = 1; r < h - 1; r++) {
        for (int c = 1; c < w - 1; c++)
            pl_set(left + c, top + r, (uint8_t)Font_CharToTile(PL_SP));
        pl_set(left,         top + r, (uint8_t)Font_CharToTile(PL_V));
        pl_set(left + w - 1, top + r, (uint8_t)Font_CharToTile(PL_V));
    }
    pl_set(left,         top,         (uint8_t)Font_CharToTile(PL_TL));
    pl_set(left + w - 1, top,         (uint8_t)Font_CharToTile(PL_TR));
    pl_set(left,         top + h - 1, (uint8_t)Font_CharToTile(PL_BL));
    pl_set(left + w - 1, top + h - 1, (uint8_t)Font_CharToTile(PL_BR));
    for (int c = 1; c < w - 1; c++) {
        pl_set(left + c, top,         (uint8_t)Font_CharToTile(PL_H));
        pl_set(left + c, top + h - 1, (uint8_t)Font_CharToTile(PL_H));
    }
}

static void pl_entry_name(int i, char *out, size_t out_sz) {
    if (g_kind == PRIZE_KIND_ITEM)
        Inventory_DecodeASCII(g_entries[i], out, out_sz);
    else {
        const char *n = Pokemon_GetNameBySpecies(g_entries[i]);
        snprintf(out, out_sz, "%s", n ? n : "");
    }
}

static void pl_draw_number4(int col, int row, int value) {
    char buf[5];
    if (value < 0) value = 0;
    if (value > 9999) value = 9999;
    snprintf(buf, sizeof(buf), "%04d", value);
    for (int i = 0; i < 4; i++) pl_set(col + i, row, pl_ct(buf[i]));
}

static void pl_draw_coinbox(void) {
    pl_box(PL_COIN_T, PL_COIN_L, PL_COIN_H, PL_COIN_W);
    static const char kCoin[] = "COIN";
    pl_label(PL_COIN_L + 1, PL_COIN_T, kCoin);
    int coins = ((wPlayerCoins[0] >> 4) & 0xF) * 1000 + (wPlayerCoins[0] & 0xF) * 100
              + ((wPlayerCoins[1] >> 4) & 0xF) * 10  + (wPlayerCoins[1] & 0xF);
    pl_draw_number4(PL_PRICE_COL, PL_COIN_T + 1, coins);
}

static void pl_draw(void) {
    pl_box(PL_BOX_T, PL_BOX_L, PL_BOX_H, PL_BOX_W);
    for (int i = 0; i < 3; i++) {
        int row = PL_ITEM_ROW0 + i * PL_ITEM_STEP;
        char name[16];
        pl_entry_name(i, name, sizeof(name));
        pl_set(PL_ARROW_COL, row, (uint8_t)Font_CharToTile((g_cursor == i) ? PL_CUR : PL_SP));
        pl_label(PL_ITEM_COL, row, name);
        pl_draw_number4(PL_PRICE_COL, PL_PRICE_ROW0 + i * PL_ITEM_STEP, g_prices[i]);
    }
    {
        static const char kNoThanks[] = "NO THANKS";
        pl_set(PL_ARROW_COL, PL_NOTHANKS_ROW, (uint8_t)Font_CharToTile((g_cursor == 3) ? PL_CUR : PL_SP));
        pl_label(PL_ITEM_COL, PL_NOTHANKS_ROW, kNoThanks);
    }
    pl_draw_coinbox();
    NPC_HideOverUITiles();
    Player_HideIfOverUI();
}

static void pl_close(void) {
    g_state = PL_IDLE;
    Map_BuildScrollView();
    NPC_BuildView(gScrollPxX, gScrollPxY);
}

void PrizeListChoice_Open(int kind, const uint8_t entries[3], const uint16_t prices[3]) {
    g_kind = kind;
    memcpy(g_entries, entries, 3);
    memcpy(g_prices, prices, sizeof(uint16_t) * 3);
    g_cursor = 0;
    g_result = 0;

    g_state = PL_OPEN;
}

int PrizeListChoice_IsOpen(void) { return g_state != PL_IDLE; }

void PrizeListChoice_Tick(void) {
    if (g_state != PL_OPEN) return;
    if (hJoyPressed & PAD_UP)   g_cursor = (g_cursor + 4 - 1) % 4;
    if (hJoyPressed & PAD_DOWN) g_cursor = (g_cursor + 1) % 4;
    if (hJoyPressed & PAD_A) {
        Audio_PlaySFX_PressAB();
        g_result = (g_cursor == 3) ? 0 : (g_cursor + 1);
        pl_close();
        return;
    } else if (hJoyPressed & PAD_B) {
        Audio_PlaySFX_PressAB();
        g_result = 0;
        pl_close();
        return;
    }

    pl_draw();
}

int PrizeListChoice_GetResult(void) { return g_result; }
