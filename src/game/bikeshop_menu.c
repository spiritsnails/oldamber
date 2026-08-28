
#include "bikeshop_menu.h"
#include "text.h"
#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "constants.h"
#include "../data/font_data.h"
#include "../platform/hardware.h"
#include <string.h>
#include <stdio.h>

#define BOX_ROW   0
#define BOX_COL   0
#define BOX_W     17
#define BOX_H     6
#define ITEM_ROW1 2
#define ITEM_ROW2 4
#define ITEM_COL  2
#define CURSOR_COL 1
#define PRICE_ROW 3
#define PRICE_COL 8

#define POKE_SPACE     0x7F
#define POKE_CORNER_TL 0x79
#define POKE_HORIZ     0x7A
#define POKE_CORNER_TR 0x7B
#define POKE_VERT      0x7C
#define POKE_CORNER_BL 0x7D
#define POKE_CORNER_BR 0x7E
#define POKE_CURSOR    0xED
#define POKE_YEN       0xF0

typedef enum {
    BSM_IDLE = 0,
    BSM_DRAW,
    BSM_INPUT,
    BSM_DONE,
} bsm_state_t;

static bsm_state_t s_state = BSM_IDLE;
static int         s_cursor = 0;
static int         s_result = 0;
static char        s_item1[16];
static char        s_item2[16];
static char        s_price[16];

static int poke_char(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return Font_CharToTile((unsigned char)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return Font_CharToTile((unsigned char)(0xA0 + (c - 'a')));
    if (c >= '0' && c <= '9') return Font_CharToTile((unsigned char)(0xF6 + (c - '0')));
    return Font_CharToTile(POKE_SPACE);
}

#define TMIDX(r, c) (((r) + 2) * SCROLL_MAP_W + ((c) + 2) + Map_UiColOfs())

static void set_tile(int row, int col, int tile) {
    if (row < 0 || col < 0) return;
    gScrollTileMap[TMIDX(row, col)] = (uint8_t)tile;
}

static void put_str(int row, int col, const char *s) {
    for (; *s; s++, col++) set_tile(row, col, poke_char((unsigned char)*s));
}

static void draw_box(void) {
    set_tile(BOX_ROW, BOX_COL,             Font_CharToTile(POKE_CORNER_TL));
    for (int c = 1; c < BOX_W - 1; c++)
        set_tile(BOX_ROW, BOX_COL + c,     Font_CharToTile(POKE_HORIZ));
    set_tile(BOX_ROW, BOX_COL + BOX_W - 1, Font_CharToTile(POKE_CORNER_TR));

    for (int r = 1; r < BOX_H - 1; r++) {
        set_tile(BOX_ROW + r, BOX_COL,             Font_CharToTile(POKE_VERT));
        for (int c = 1; c < BOX_W - 1; c++)
            set_tile(BOX_ROW + r, BOX_COL + c,     Font_CharToTile(POKE_SPACE));
        set_tile(BOX_ROW + r, BOX_COL + BOX_W - 1, Font_CharToTile(POKE_VERT));
    }

    set_tile(BOX_ROW + BOX_H - 1, BOX_COL,             Font_CharToTile(POKE_CORNER_BL));
    for (int c = 1; c < BOX_W - 1; c++)
        set_tile(BOX_ROW + BOX_H - 1, BOX_COL + c,     Font_CharToTile(POKE_HORIZ));
    set_tile(BOX_ROW + BOX_H - 1, BOX_COL + BOX_W - 1, Font_CharToTile(POKE_CORNER_BR));

    put_str(ITEM_ROW1, ITEM_COL, s_item1);
    put_str(ITEM_ROW2, ITEM_COL, s_item2);
    set_tile(PRICE_ROW, PRICE_COL, POKE_YEN);
    put_str(PRICE_ROW, PRICE_COL + 1, s_price);

    set_tile(ITEM_ROW1, CURSOR_COL, s_cursor == 0 ? Font_CharToTile(POKE_CURSOR) : Font_CharToTile(POKE_SPACE));
    set_tile(ITEM_ROW2, CURSOR_COL, s_cursor == 1 ? Font_CharToTile(POKE_CURSOR) : Font_CharToTile(POKE_SPACE));
}

void BikeShopMenu_Show(const char *item1, const char *item2, int price) {
    snprintf(s_item1, sizeof(s_item1), "%s", item1);
    snprintf(s_item2, sizeof(s_item2), "%s", item2);
    snprintf(s_price, sizeof(s_price), "%d", price);
    s_cursor = 0;
    s_result = 0;
    s_state = BSM_DRAW;
}

int BikeShopMenu_IsOpen(void) { return s_state != BSM_IDLE; }
int BikeShopMenu_GetResult(void) { return s_result; }

void BikeShopMenu_PostRender(void) {
    if (s_state == BSM_DRAW || s_state == BSM_INPUT) {
        draw_box();

        NPC_HideOverUITiles();
        Player_HideIfOverUI();
    }
}

void BikeShopMenu_Tick(void) {
    switch (s_state) {
    case BSM_IDLE:
        break;
    case BSM_DRAW:
        draw_box();
        s_state = BSM_INPUT;
        break;
    case BSM_INPUT:
        if (hJoyPressed & PAD_UP)   s_cursor = 0;
        if (hJoyPressed & PAD_DOWN) s_cursor = 1;
        if (hJoyPressed & PAD_A) {
            s_result = (s_cursor == 0) ? 1 : 0;
            s_state = BSM_DONE;
        } else if (hJoyPressed & PAD_B) {
            s_result = 0;
            s_state = BSM_DONE;
        }
        break;
    case BSM_DONE:
        s_state = BSM_IDLE;
        break;
    }
}
