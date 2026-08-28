
#include "pokemart.h"
#include "rom_text.h"
#include "text.h"
#include "inventory.h"
#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "../data/font_data.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define CHAR_TERM   0x50u
#define CHAR_SPACE  0x7Fu
#define CHAR_CURSOR_TRANS 0xECu
#define CHAR_CURSOR_BLACK 0xEDu
#define CHAR_DOWN   0xEEu
#define CHAR_YEN    0xF0u
#define CHAR_TIMES  0xF1u

#define BC_TL   0x79u
#define BC_H    0x7Au
#define BC_TR   0x7Bu
#define BC_V    0x7Cu
#define BC_BL   0x7Du
#define BC_BR   0x7Eu

#define MENU_L          0
#define MENU_R         10
#define MENU_T          0
#define MENU_B          6
#define MENU_CURSOR_COL 1
#define MENU_TEXT_COL   2
#define MENU_ENTRY_ROW  1
#define MENU_ENTRY_DY   2

#define MONEY_L         11
#define MONEY_R         19
#define MONEY_T          0
#define MONEY_B          2
#define MONEY_LABEL_COL 13
#define MONEY_YEN_COL   12
#define MONEY_DIGIT_COL 13
#define MONEY_ROW        1

#define LIST_L              4
#define LIST_R             19
#define LIST_T              2
#define LIST_B             12
#define LIST_ITEM_TOP       4
#define LIST_CURSOR_COL     5
#define LIST_NAME_COL       6
#define LIST_NAME_W        12
#define LIST_PRICE_COL     11
#define MAX_LIST_VISIBLE    4
#define LIST_SCROLL_COL    18
#define LIST_SCROLL_ROW    11

#define QTY_L           7
#define QTY_R          19
#define QTY_T           9
#define QTY_B          11
#define QTY_ROW        10
#define QTY_TIMES_COL   8
#define QTY_YEN_COL    13
#define QTY_PRICE_COL  14

#define YESNO_L        14
#define YESNO_R        19
#define YESNO_T         7
#define YESNO_B        11
#define YESNO_YES_ROW   8
#define YESNO_NO_ROW   10
#define YESNO_CURSOR_COL 15
#define YESNO_TEXT_COL  16

#define smset(c, r, t) \
    gScrollTileMap[((r) + 2) * SCROLL_MAP_W + ((c) + 2) + Map_UiColOfs()] = (t)

static void sm_tile(int col, int row, uint8_t ch) {
    smset(col, row, (uint8_t)Font_CharToTile(ch));
}
static void sm_raw(int col, int row, uint8_t tile) {
    smset(col, row, tile);
}

typedef enum {
    MART_IDLE = 0,
    MART_MAIN,
    MART_BUY_LIST,
    MART_BUY_QTY,
    MART_BUY_CONFIRM,
    MART_BUY_YESNO,
    MART_BUY_AFTER,
    MART_SELL_LIST,
    MART_SELL_QTY,
    MART_SELL_CONFIRM,
    MART_SELL_YESNO,
    MART_SELL_AFTER,
    MART_LOOP,
    MART_DONE,
} mart_state_t;

static mart_state_t g_state       = MART_IDLE;
static const uint8_t *g_inv       = NULL;
static int  g_inv_count           = 0;
static int  g_list_cursor         = 0;
static int  g_list_scroll         = 0;
static int  g_main_cursor         = 0;
static int  g_yesno_cursor        = 0;
static int  g_qty                 = 1;
static uint8_t g_selected_item    = 0;
static int  g_needs_redraw        = 0;
static int  g_panel_drawn         = 0;
static int  g_sell_mode           = 0;
static int  g_buy_prompt_to_bg    = 0;
static int  g_scroll_blink_ctr    = 0;
static int  g_scroll_blink_on     = 1;
static int  g_main_outline_active = 0;
static int  g_yesno_open          = 0;
static uint8_t g_yesno_saved[(YESNO_B - YESNO_T + 1) * (YESNO_R - YESNO_L + 1)];

#define LIST_ARROW_ON_TICKS   32
#define LIST_ARROW_OFF_TICKS   8

static char g_text_buf[80];

static const uint16_t kItemPrices[86] = {
     0,
     0,
     1200,
     600,
     200,
     0,
     0,
     0,
     1000,
     0,
     0,
     100,
     250,
     250,
     200,
     200,
     3000,
     2500,
     1500,
     700,
     300,
     0, 0, 0, 0, 0, 0, 0, 0,
     550,
     350,
     0,
     2100,
     2100,
     2100,
     9800,
     9800,
     9800,
     9800,
     9800,
     4800,
     0, 0, 0, 0, 0,
     950,
     2100,
     0,
     10000,
     9800,
     1000,
     600,
     1500,
     4000,
     700,
     500,
     700,
     650,
     10,
     200,
     300,
     350,
     0, 0,
     500,
     550,
     350,
     350,
     0, 0, 0, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const uint8_t kInv_Viridian[]  = {0x04,0x0B,0x0F,0x0C,0};
static const uint8_t kInv_Pewter[]    = {0x04,0x14,0x1D,0x0B,0x0C,0x0E,0x0F,0};
static const uint8_t kInv_Cerulean[]  = {0x04,0x14,0x1E,0x0B,0x0C,0x0E,0x0F,0};
static const uint8_t kInv_Vermilion[] = {0x04,0x13,0x0D,0x0E,0x0F,0x1E,0};
static const uint8_t kInv_Lavender[]  = {0x03,0x13,0x35,0x1D,0x38,0x0B,0x0C,0x0D,0x0F,0};
static const uint8_t kInv_Cel2F1[]    = {0x03,0x13,0x35,0x38,0x0B,0x0C,0x0D,0x0E,0x0F,0};
static const uint8_t kInv_Cel2F2[]    = {0xE8,0xE9,0xCA,0xCF,0xED,0xC9,0xCD,0xDC,0xE2,0};
static const uint8_t kInv_Cel4F[]     = {0x33,0x20,0x21,0x22,0x2F,0};
static const uint8_t kInv_Cel5F1[]    = {0x2E,0x37,0x3A,0x41,0x42,0x43,0x44,0};
static const uint8_t kInv_Cel5F2[]    = {0x23,0x24,0x25,0x26,0x27,0};
static const uint8_t kInv_Fuchsia[]   = {0x02,0x03,0x13,0x35,0x34,0x38,0};
static const uint8_t kInv_Cinnabar[]  = {0x02,0x03,0x12,0x39,0x1D,0x34,0x35,0};
static const uint8_t kInv_Saffron[]   = {0x03,0x12,0x39,0x1D,0x34,0x35,0};
static const uint8_t kInv_Indigo[]    = {0x02,0x03,0x10,0x11,0x34,0x35,0x39,0};

static const uint8_t kStrCancel[] = {0x82,0x80,0x8D,0x82,0x84,0x8B,CHAR_TERM};
static const uint8_t kStrBuy[]    = {0x81,0x94,0x98,CHAR_TERM};
static const uint8_t kStrSell[]   = {0x92,0x84,0x8B,0x8B,CHAR_TERM};
static const uint8_t kStrQuit[]   = {0x90,0x94,0x88,0x93,CHAR_TERM};
static const uint8_t kStrMoney[]  = {0x8C,0x8E,0x8D,0x84,0x98,CHAR_TERM};

static uint32_t money_get(void) {
    return (uint32_t)(
        ((wPlayerMoney[0] >> 4) & 0xF) * 100000u +
        (wPlayerMoney[0] & 0xF)        * 10000u  +
        ((wPlayerMoney[1] >> 4) & 0xF) * 1000u   +
        (wPlayerMoney[1] & 0xF)        * 100u     +
        ((wPlayerMoney[2] >> 4) & 0xF) * 10u      +
        (wPlayerMoney[2] & 0xF)
    );
}

static void money_set(uint32_t v) {
    if (v > 999999u) v = 999999u;
    wPlayerMoney[0] = (uint8_t)(((v / 100000u) << 4) | ((v / 10000u) % 10u));
    wPlayerMoney[1] = (uint8_t)((((v / 1000u) % 10u) << 4) | ((v / 100u) % 10u));
    wPlayerMoney[2] = (uint8_t)((((v / 10u) % 10u) << 4) | (v % 10u));
}

static const uint8_t kTMPriceK[50] = {
    3, 2, 2, 1, 3, 4, 2, 4, 3, 4,
    2, 1, 4, 5, 5, 5, 3, 2, 3, 2,
    5, 5, 5, 2, 5, 4, 5, 2, 4, 1,
    2, 1, 1, 2, 4, 2, 2, 5, 2, 4,
    2, 2, 5, 2, 2, 4, 3, 4, 4, 2,
};

static uint16_t item_price(uint8_t id) {
    if (id >= 0xC9 && id <= 0xFA) return (uint16_t)kTMPriceK[id - 0xC9] * 1000u;
    if (id == 0 || id >= 86) return 0;
    return kItemPrices[id];
}

static int can_sell(uint8_t id) {
    if (id >= 0xC4) return 0;
    if (Inventory_IsKeyItem(id)) return 0;
    if (item_price(id) == 0) return 0;
    return 1;
}

static char pokered_char_to_ascii(uint8_t b) {
    if (b >= 0x80 && b <= 0x99) return (char)('A' + (b - 0x80));
    if (b >= 0xA0 && b <= 0xB9) return (char)('a' + (b - 0xA0));
    if (b >= 0xF6)               return (char)('0' + (b - 0xF6));
    if (b == 0x7F) return ' ';
    if (b == 0xE3) return '-';
    if (b == 0xE0) return '\'';
    if (b == 0xE8) return '.';
    if (b == 0xBA) return (char)0xE9;
    return ' ';
}

static void get_item_name_ascii(uint8_t item_id, char *out, int max_len) {
    const uint8_t *name = Inventory_GetName(item_id);
    int i = 0;
    for (; i < max_len - 1 && name[i] != CHAR_TERM; i++)
        out[i] = pokered_char_to_ascii(name[i]);
    out[i] = '\0';
}

static uint8_t ascii_tile(char c) {
    if (c >= 'A' && c <= 'Z') return (uint8_t)Font_CharToTile((unsigned char)(0x80u + (c - 'A')));
    if (c >= 'a' && c <= 'z') return (uint8_t)Font_CharToTile((unsigned char)(0xA0u + (c - 'a')));
    if (c >= '0' && c <= '9') return (uint8_t)Font_CharToTile((unsigned char)(0xF6u + (c - '0')));
    return (uint8_t)Font_CharToTile(CHAR_SPACE);
}

static void draw_pstr(int col, int row, const uint8_t *s) {
    for (; *s != CHAR_TERM; s++, col++)
        smset(col, row, (uint8_t)Font_CharToTile(*s));
}

static void draw_box(int box_l, int box_t, int box_r, int box_b) {
    sm_tile(box_l, box_t, BC_TL);
    for (int c = box_l + 1; c < box_r; c++) sm_tile(c, box_t, BC_H);
    sm_tile(box_r, box_t, BC_TR);
    for (int r = box_t + 1; r < box_b; r++) {
        sm_tile(box_l, r, BC_V);
        sm_tile(box_r, r, BC_V);
    }
    sm_tile(box_l, box_b, BC_BL);
    for (int c = box_l + 1; c < box_r; c++) sm_tile(c, box_b, BC_H);
    sm_tile(box_r, box_b, BC_BR);
}

static void clear_box_interior(int box_l, int box_t, int box_r, int box_b) {
    for (int r = box_t + 1; r < box_b; r++)
        for (int c = box_l + 1; c < box_r; c++)
            sm_tile(c, r, CHAR_SPACE);
}

static void draw_digits(int col, int row, uint32_t v, int width) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%*u", width, (unsigned)v);
    for (int i = 0; i < width; i++) {
        char ch = buf[i];
        uint8_t tile = (ch >= '0' && ch <= '9')
            ? (uint8_t)Font_CharToTile((unsigned char)(0xF6u + (ch - '0')))
            : (uint8_t)Font_CharToTile(CHAR_SPACE);
        smset(col + i, row, tile);
    }
}

static void draw_item_name(int col, int row, uint8_t item_id, int width) {
    const uint8_t *name = Inventory_GetName(item_id);
    int drawn = 0;
    for (; drawn < width && name[drawn] != CHAR_TERM; drawn++)
        smset(col + drawn, row, (uint8_t)Font_CharToTile(name[drawn]));
    for (; drawn < width; drawn++)
        sm_tile(col + drawn, row, CHAR_SPACE);
}

static void draw_money_box(void) {
    draw_box(MONEY_L, MONEY_T, MONEY_R, MONEY_B);
    clear_box_interior(MONEY_L, MONEY_T, MONEY_R, MONEY_B);

    draw_pstr(MONEY_LABEL_COL, MONEY_T, kStrMoney);

    sm_tile(MONEY_YEN_COL, MONEY_ROW, CHAR_YEN);
    draw_digits(MONEY_DIGIT_COL, MONEY_ROW, money_get(), 6);
}

static void draw_main_menu(void) {
    draw_box(MENU_L, MENU_T, MENU_R, MENU_B);
    for (int r = MENU_T + 1; r < MENU_B; r++)
        for (int c = MENU_L + 1; c < MENU_R; c++)
            sm_tile(c, r, CHAR_SPACE);

    const uint8_t *labels[3] = { kStrBuy, kStrSell, kStrQuit };
    for (int i = 0; i < 3; i++) {
        int row = MENU_ENTRY_ROW + i * MENU_ENTRY_DY;
        if (i == g_main_cursor) {
            sm_raw(MENU_CURSOR_COL, row, g_main_outline_active ? CHAR_CURSOR_TRANS
                                                               : CHAR_CURSOR_BLACK);
        } else {
            sm_tile(MENU_CURSOR_COL, row, CHAR_SPACE);
        }
        draw_pstr(MENU_TEXT_COL, row, labels[i]);
    }
}

static void draw_price_asm_style(int col, int row, uint32_t value) {
    char six[7];
    snprintf(six, sizeof(six), "%06u", (unsigned)value);

    int out = col;
    int started = 0;
    for (int i = 0; i < 6; i++) {
        char d = six[i];
        if (!started && d == '0') {
            out++;
            continue;
        }
        if (!started) {
            sm_tile(out++, row, CHAR_YEN);
            started = 1;
        }
        smset(out++, row, (uint8_t)Font_CharToTile((unsigned char)(0xF6u + (d - '0'))));
    }
    if (!started) {
        sm_tile(out, row, CHAR_YEN);
        smset(out + 1, row, (uint8_t)Font_CharToTile((unsigned char)0xF6u));
    }
}

static void draw_list_row(int name_row, int entry_idx, int is_cursor, int show_price) {
    int price_row = name_row + 1;

    for (int c = LIST_L + 1; c < LIST_R; c++) {
        sm_tile(c, name_row,  CHAR_SPACE);
        sm_tile(c, price_row, CHAR_SPACE);
    }

    if (is_cursor) sm_raw(LIST_CURSOR_COL, name_row, CHAR_CURSOR_BLACK);
    else sm_tile(LIST_CURSOR_COL, name_row, CHAR_SPACE);

    int total_items = g_sell_mode ? (int)wNumBagItems : g_inv_count;
    if (entry_idx == total_items) {
        draw_pstr(LIST_NAME_COL, name_row, kStrCancel);
        return;
    }

    uint8_t item_id = g_sell_mode ? wBagItems[entry_idx * 2] : g_inv[entry_idx];
    draw_item_name(LIST_NAME_COL, name_row, item_id, LIST_NAME_W);

    if (show_price) {
        uint16_t price = item_price(item_id);
        if (g_sell_mode) price = (uint16_t)(price / 2u);
        draw_price_asm_style(LIST_PRICE_COL, price_row, price);
    }
}

static void draw_buy_list(void) {
    g_sell_mode = 0;
    draw_box(LIST_L, LIST_T, LIST_R, LIST_B);
    clear_box_interior(LIST_L, LIST_T, LIST_R, LIST_B);
    int total = g_inv_count + 1;
    for (int s = 0; s < MAX_LIST_VISIBLE; s++) {
        int idx = g_list_scroll + s;
        if (idx < total)
            draw_list_row(LIST_ITEM_TOP + s * 2, idx, (g_list_cursor == idx), 1);
    }

    if (g_list_scroll + MAX_LIST_VISIBLE < total) {
        if (g_scroll_blink_on) sm_raw(LIST_SCROLL_COL, LIST_SCROLL_ROW, CHAR_DOWN);
        else sm_tile(LIST_SCROLL_COL, LIST_SCROLL_ROW, CHAR_SPACE);
    }
}

static void draw_sell_list(void) {
    g_sell_mode = 1;
    draw_box(LIST_L, LIST_T, LIST_R, LIST_B);
    clear_box_interior(LIST_L, LIST_T, LIST_R, LIST_B);
    int total = (int)wNumBagItems + 1;
    for (int s = 0; s < MAX_LIST_VISIBLE; s++) {
        int idx = g_list_scroll + s;
        if (idx < total)
            draw_list_row(LIST_ITEM_TOP + s * 2, idx, (g_list_cursor == idx), 0);
    }
    if (g_list_scroll + MAX_LIST_VISIBLE < total) {
        if (g_scroll_blink_on) sm_raw(LIST_SCROLL_COL, LIST_SCROLL_ROW, CHAR_DOWN);
        else sm_tile(LIST_SCROLL_COL, LIST_SCROLL_ROW, CHAR_SPACE);
    }
}

static void draw_qty_box(void) {
    draw_box(QTY_L, QTY_T, QTY_R, QTY_B);
    clear_box_interior(QTY_L, QTY_T, QTY_R, QTY_B);

    sm_tile(QTY_TIMES_COL, QTY_ROW, CHAR_TIMES);
    {
        char qbuf[3];
        snprintf(qbuf, sizeof(qbuf), "%02u", (unsigned)g_qty);
        smset(QTY_TIMES_COL + 1, QTY_ROW,
              (uint8_t)Font_CharToTile((unsigned char)(0xF6u + (qbuf[0] - '0'))));
        smset(QTY_TIMES_COL + 2, QTY_ROW,
              (uint8_t)Font_CharToTile((unsigned char)(0xF6u + (qbuf[1] - '0'))));
    }

    uint32_t unit_price = g_sell_mode
        ? (uint32_t)(item_price(g_selected_item) / 2u)
        : (uint32_t)item_price(g_selected_item);
    uint32_t total = (uint32_t)g_qty * unit_price;

    draw_price_asm_style(QTY_PRICE_COL - 2, QTY_ROW, total);
}

static void clear_qty_box(void) {
    for (int r = QTY_T; r <= QTY_B; r++)
        for (int c = QTY_L; c <= QTY_R; c++)
            sm_tile(c, r, CHAR_SPACE);
}

static void draw_yesno(void) {
    if (!g_yesno_open) {
        int i = 0;
        for (int r = YESNO_T; r <= YESNO_B; r++) {
            for (int c = YESNO_L; c <= YESNO_R; c++) {
                g_yesno_saved[i++] = gScrollTileMap[(r + 2) * SCROLL_MAP_W + (c + 2) + Map_UiColOfs()];
            }
        }
        g_yesno_open = 1;
    }
    draw_box(YESNO_L, YESNO_T, YESNO_R, YESNO_B);
    clear_box_interior(YESNO_L, YESNO_T, YESNO_R, YESNO_B);

    if (g_yesno_cursor == 0) sm_raw(YESNO_CURSOR_COL, YESNO_YES_ROW, CHAR_CURSOR_BLACK);
    else sm_tile(YESNO_CURSOR_COL, YESNO_YES_ROW, CHAR_SPACE);
    smset(YESNO_TEXT_COL,     YESNO_YES_ROW, ascii_tile('Y'));
    smset(YESNO_TEXT_COL + 1, YESNO_YES_ROW, ascii_tile('E'));
    smset(YESNO_TEXT_COL + 2, YESNO_YES_ROW, ascii_tile('S'));

    if (g_yesno_cursor == 1) sm_raw(YESNO_CURSOR_COL, YESNO_NO_ROW, CHAR_CURSOR_BLACK);
    else sm_tile(YESNO_CURSOR_COL, YESNO_NO_ROW, CHAR_SPACE);
    smset(YESNO_TEXT_COL,     YESNO_NO_ROW, ascii_tile('N'));
    smset(YESNO_TEXT_COL + 1, YESNO_NO_ROW, ascii_tile('O'));
    smset(YESNO_TEXT_COL + 2, YESNO_NO_ROW, ascii_tile(' '));
}

static void clear_yesno(void) {
    if (!g_yesno_open) return;
    int i = 0;
    for (int r = YESNO_T; r <= YESNO_B; r++) {
        for (int c = YESNO_L; c <= YESNO_R; c++) {
            gScrollTileMap[(r + 2) * SCROLL_MAP_W + (c + 2) + Map_UiColOfs()] = g_yesno_saved[i++];
        }
    }
    g_yesno_open = 0;
}

static void clear_panel(void) {

    for (int r = MENU_T; r <= MENU_B; r++)
        for (int c = MENU_L; c <= MENU_R; c++)
            sm_tile(c, r, CHAR_SPACE);

    for (int r = MONEY_T; r <= MONEY_B; r++)
        for (int c = MONEY_L; c <= MONEY_R; c++)
            sm_tile(c, r, CHAR_SPACE);

    for (int r = LIST_T; r <= LIST_B; r++)
        for (int c = LIST_L; c <= LIST_R; c++)
            sm_tile(c, r, CHAR_SPACE);
}

static void restore_map_and_sprites(void) {
    Map_BuildScrollView();
    NPC_BuildView(0, 0);
    Player_SyncOAM();
}

static void return_to_main_with_prompt(void) {
    restore_map_and_sprites();
    draw_money_box();
    draw_main_menu();
    NPC_HideOverUITiles();
    Player_HideIfOverUI();
    Text_KeepTilesOnClose();
    Text_SuppressCursorNext();
    wDoNotWaitForButtonPress = 1;
    Text_ShowASCII(RomText("_PokemartAnythingElseText"));
    g_state = MART_LOOP;
}

static void list_scroll_clamp(int total_entries) {
    int max_scroll = total_entries - MAX_LIST_VISIBLE;
    if (max_scroll < 0) max_scroll = 0;
    if (g_list_scroll > max_scroll) g_list_scroll = max_scroll;
    if (g_list_scroll < 0) g_list_scroll = 0;
}

static int list_cursor_up(int total_entries) {
    if (g_list_cursor <= 0) return 0;
    g_list_cursor--;
    if (g_list_cursor < g_list_scroll) g_list_scroll--;
    list_scroll_clamp(total_entries);
    return 1;
}

static int list_cursor_down(int total_entries) {
    if (g_list_cursor >= total_entries - 1) return 0;
    g_list_cursor++;
    if (g_list_cursor > g_list_scroll + MAX_LIST_VISIBLE - 1)
        g_list_scroll++;
    list_scroll_clamp(total_entries);
    return 1;
}

void Pokemart_SetInventory(const uint8_t *inv) {
    g_inv = inv;
    g_inv_count = 0;
    if (inv) {
        while (inv[g_inv_count] != 0) g_inv_count++;
    }
}

void Pokemart_Start(void) {
    g_state        = MART_MAIN;
    g_main_cursor  = 0;
    g_list_cursor  = 0;
    g_list_scroll  = 0;
    g_qty          = 1;
    g_panel_drawn  = 0;
    g_needs_redraw = 0;
    g_yesno_cursor = 0;
    g_buy_prompt_to_bg = 0;
    g_scroll_blink_ctr = 0;
    g_scroll_blink_on = 1;
    g_main_outline_active = 0;
    g_yesno_open = 0;

    NPC_BuildView(0, 0);

    Text_KeepTilesOnClose();
    Text_ShowASCII(RomText("_PokemartGreetingText"));
}

int Pokemart_IsActive(void) {
    return g_state != MART_IDLE;
}

void Pokemart_Tick(void) {
    if (++g_scroll_blink_ctr >= (g_scroll_blink_on ? LIST_ARROW_ON_TICKS
                                                   : LIST_ARROW_OFF_TICKS)) {
        g_scroll_blink_ctr = 0;
        g_scroll_blink_on ^= 1;
        if (g_state == MART_BUY_LIST || g_state == MART_SELL_LIST) {
            g_needs_redraw = 1;
        }
    }
    switch (g_state) {

    case MART_MAIN:
        if (!g_panel_drawn) {
            draw_money_box();
            draw_main_menu();

            NPC_HideOverUITiles();
            Player_HideIfOverUI();
            g_panel_drawn = 1;
        }
        if (hJoyPressed & PAD_UP) {
            if (g_main_cursor > 0) {
                sm_tile(MENU_CURSOR_COL, MENU_ENTRY_ROW + g_main_cursor * MENU_ENTRY_DY, CHAR_SPACE);
                g_main_cursor--;
                g_main_outline_active = 0;
                sm_raw(MENU_CURSOR_COL, MENU_ENTRY_ROW + g_main_cursor * MENU_ENTRY_DY, CHAR_CURSOR_BLACK);
            }
        }
        if (hJoyPressed & PAD_DOWN) {
            if (g_main_cursor < 2) {
                sm_tile(MENU_CURSOR_COL, MENU_ENTRY_ROW + g_main_cursor * MENU_ENTRY_DY, CHAR_SPACE);
                g_main_cursor++;
                g_main_outline_active = 0;
                sm_raw(MENU_CURSOR_COL, MENU_ENTRY_ROW + g_main_cursor * MENU_ENTRY_DY, CHAR_CURSOR_BLACK);
            }
        }
        if (hJoyPressed & PAD_A) {
            if (g_main_cursor == 0) {
                g_main_outline_active = 1;
                draw_main_menu();

                Text_KeepTilesOnClose();
                Text_SuppressCursorNext();
                Text_ShowASCII(RomText("_PokemartBuyingGreetingText"));
                g_state        = MART_BUY_LIST;
                g_buy_prompt_to_bg = 1;
                g_list_cursor  = 0;
                g_list_scroll  = 0;
                g_needs_redraw = 1;
            } else if (g_main_cursor == 1) {
                g_main_outline_active = 1;
                draw_main_menu();
                if (wNumBagItems == 0) {
                    Text_ShowASCII(RomText("_PokemartItemBagEmptyText"));
                    g_state = MART_LOOP;
                } else {
                    Text_ShowASCII(RomText("_PokemonSellingGreetingText"));
                    g_state        = MART_SELL_LIST;
                    g_list_cursor  = 0;
                    g_list_scroll  = 0;
                    g_needs_redraw = 1;
                }
            } else {
                Text_ShowASCII(RomText("_PokemartThankYouText"));
                g_state = MART_DONE;
            }
        }
        if (hJoyPressed & PAD_B) {
            Text_ShowASCII(RomText("_PokemartThankYouText"));
            g_state = MART_DONE;
        }
        break;

    case MART_BUY_LIST:
        if (g_buy_prompt_to_bg) {

            Text_BlitBoxToBGAndHideWindow();
            g_buy_prompt_to_bg = 0;
        }
        if (g_needs_redraw) {
            draw_money_box();
            draw_buy_list();
            g_needs_redraw = 0;
        }
        {
            int total = g_inv_count + 1;
            if (hJoyPressed & PAD_UP) {
                if (list_cursor_up(total)) g_needs_redraw = 1;
            }
            if (hJoyPressed & PAD_DOWN) {
                if (list_cursor_down(total)) g_needs_redraw = 1;
            }
            if (g_needs_redraw) {
                draw_buy_list();
                g_needs_redraw = 0;
            }
            if (hJoyPressed & PAD_A) {
                if (g_list_cursor == g_inv_count) {
                    return_to_main_with_prompt();
                } else {

                    sm_raw(LIST_CURSOR_COL, LIST_ITEM_TOP + (g_list_cursor - g_list_scroll) * 2, CHAR_CURSOR_TRANS);
                    g_selected_item = g_inv[g_list_cursor];
                    g_qty           = 1;
                    g_sell_mode     = 0;
                    draw_qty_box();
                    g_state = MART_BUY_QTY;
                }
            }
            if (hJoyPressed & PAD_B) {
                return_to_main_with_prompt();
            }
        }
        break;

    case MART_BUY_QTY:
        if (hJoyPressed & PAD_UP) {
            if (g_qty < 99) { g_qty++; draw_qty_box(); }
        }
        if (hJoyPressed & PAD_DOWN) {
            if (g_qty > 1) g_qty--;
            else g_qty = 99;
            draw_qty_box();
        }
        if (hJoyPressed & PAD_A) {
            uint32_t total = (uint32_t)g_qty * item_price(g_selected_item);
            char item_name[16];
            get_item_name_ascii(g_selected_item, item_name, sizeof(item_name));

            snprintf(g_text_buf, sizeof(g_text_buf), "%s?\nThat will be\n\xa5%u. OK?",
                     item_name, (unsigned)total);
            Text_KeepTilesOnClose();
            Text_SuppressCursorNext();
            wDoNotWaitForButtonPress = 1;
            Text_ShowASCII(g_text_buf);
            g_state = MART_BUY_CONFIRM;
        }
        if (hJoyPressed & PAD_B) {
            clear_qty_box();
            g_qty          = 1;
            g_needs_redraw = 1;
            g_state        = MART_BUY_LIST;
        }
        break;

    case MART_BUY_CONFIRM:
        g_yesno_cursor = 0;
        draw_yesno();
        g_state = MART_BUY_YESNO;
        break;

    case MART_BUY_YESNO:
        if (hJoyPressed & PAD_UP) {
            if (g_yesno_cursor > 0) { g_yesno_cursor = 0; draw_yesno(); }
        }
        if (hJoyPressed & PAD_DOWN) {
            if (g_yesno_cursor < 1) { g_yesno_cursor = 1; draw_yesno(); }
        }
        if (hJoyPressed & PAD_A) {
            if (g_yesno_cursor == 0) {
                clear_yesno();
                uint32_t total = (uint32_t)g_qty * item_price(g_selected_item);
                uint32_t money = money_get();
                if (money < total) {
                    wDoNotWaitForButtonPress = 0;
                    Text_ShowASCII(RomText("_PokemartNotEnoughMoneyText"));
                    g_state = MART_BUY_AFTER;
                } else if (Inventory_Add(g_selected_item, (uint8_t)g_qty) != 0) {
                    wDoNotWaitForButtonPress = 0;
                    Text_ShowASCII(RomText("_CantCarryMoreText"));
                    g_state = MART_BUY_AFTER;
                } else {
                    money_set(money - total);
                    Audio_PlaySFX_Purchase();
                    wDoNotWaitForButtonPress = 0;
                    Text_ShowASCII(RomText("_PokemartBoughtItemText"));
                    g_state = MART_BUY_AFTER;
                }
            } else {
                clear_yesno();
                hWY = SCREEN_HEIGHT_PX;
                g_needs_redraw = 1;
                g_state        = MART_BUY_LIST;
            }
        }
        if (hJoyPressed & PAD_B) {
            clear_yesno();
            hWY = SCREEN_HEIGHT_PX;
            g_needs_redraw = 1;
            g_state        = MART_BUY_LIST;
        }
        break;

    case MART_BUY_AFTER:
        g_qty          = 1;
        g_needs_redraw = 1;
        draw_money_box();
        g_state = MART_BUY_LIST;
        break;

    case MART_SELL_LIST:
        if (g_needs_redraw) {
            draw_money_box();
            draw_sell_list();
            g_needs_redraw = 0;
        }
        {
            int total = (int)wNumBagItems + 1;
            if (hJoyPressed & PAD_UP) {
                if (list_cursor_up(total)) g_needs_redraw = 1;
            }
            if (hJoyPressed & PAD_DOWN) {
                if (list_cursor_down(total)) g_needs_redraw = 1;
            }
            if (g_needs_redraw) {
                draw_sell_list();
                g_needs_redraw = 0;
            }
            if (hJoyPressed & PAD_A) {
                if (g_list_cursor == (int)wNumBagItems) {
                    return_to_main_with_prompt();
                } else {

                    sm_raw(LIST_CURSOR_COL, LIST_ITEM_TOP + (g_list_cursor - g_list_scroll) * 2, CHAR_CURSOR_TRANS);
                    g_selected_item = wBagItems[g_list_cursor * 2];
                    g_qty           = 1;
                    g_sell_mode     = 1;
                    draw_qty_box();
                    g_state = MART_SELL_QTY;
                }
            }
            if (hJoyPressed & PAD_B) {
                return_to_main_with_prompt();
            }
        }
        break;

    case MART_SELL_QTY:
        if (hJoyPressed & PAD_UP) {
            if (g_qty < 99) { g_qty++; draw_qty_box(); }
        }
        if (hJoyPressed & PAD_DOWN) {
            if (g_qty > 1) g_qty--;
            else g_qty = 99;
            draw_qty_box();
        }
        if (hJoyPressed & PAD_A) {
            uint32_t sell_per = item_price(g_selected_item) / 2;
            uint32_t sell_total = (uint32_t)g_qty * sell_per;
            snprintf(g_text_buf, sizeof(g_text_buf), "I can pay you\n\xa5%u for that.", (unsigned)sell_total);
            Text_KeepTilesOnClose();
            Text_SuppressCursorNext();
            wDoNotWaitForButtonPress = 1;
            Text_ShowASCII(g_text_buf);
            g_state = MART_SELL_CONFIRM;
        }
        if (hJoyPressed & PAD_B) {
            clear_qty_box();
            g_qty          = 1;
            g_needs_redraw = 1;
            g_state        = MART_SELL_LIST;
        }
        break;

    case MART_SELL_CONFIRM:
        if (!can_sell(g_selected_item)) {
            wDoNotWaitForButtonPress = 0;
            Text_ShowASCII(RomText("_PokemartUnsellableItemText"));
            g_state = MART_SELL_AFTER;
        } else {
            g_yesno_cursor = 0;
            draw_yesno();
            g_state = MART_SELL_YESNO;
        }
        break;

    case MART_SELL_YESNO:
        if (hJoyPressed & PAD_UP) {
            if (g_yesno_cursor > 0) { g_yesno_cursor = 0; draw_yesno(); }
        }
        if (hJoyPressed & PAD_DOWN) {
            if (g_yesno_cursor < 1) { g_yesno_cursor = 1; draw_yesno(); }
        }
        if (hJoyPressed & PAD_A) {
            if (g_yesno_cursor == 0) {
                clear_yesno();
                uint32_t sell_per   = item_price(g_selected_item) / 2;
                uint32_t sell_total = (uint32_t)g_qty * sell_per;
                Inventory_Remove(g_selected_item, (uint8_t)g_qty);
                money_set(money_get() + sell_total);
                Audio_PlaySFX_PressAB();
                wDoNotWaitForButtonPress = 0;
                Text_ShowASCII(RomText("_PokemartBoughtItemText"));
                g_state = MART_SELL_AFTER;
            } else {
                clear_yesno();
                hWY = SCREEN_HEIGHT_PX;
                g_needs_redraw = 1;
                g_state        = MART_SELL_LIST;
            }
        }
        if (hJoyPressed & PAD_B) {
            clear_yesno();
            hWY = SCREEN_HEIGHT_PX;
            g_needs_redraw = 1;
            g_state        = MART_SELL_LIST;
        }
        break;

    case MART_SELL_AFTER:
        g_qty          = 1;
        g_needs_redraw = 1;
        draw_money_box();
        if (g_list_cursor > (int)wNumBagItems)
            g_list_cursor = (int)wNumBagItems;
        list_scroll_clamp((int)wNumBagItems + 1);
        g_state = MART_SELL_LIST;
        break;

    case MART_LOOP:
        g_main_cursor = 0;
        g_main_outline_active = 0;
        g_panel_drawn = 1;
        g_state       = MART_MAIN;
        break;

    case MART_DONE:
        clear_panel();
        restore_map_and_sprites();
        g_state = MART_IDLE;
        break;

    case MART_IDLE:
    default:
        break;
    }

    if (Text_IsOpen()) {
        gWindowTileMap[16][18] = (uint8_t)Font_CharToTile(CHAR_SPACE);
    }

    if (g_state != MART_IDLE) {
        NPC_HideOverUITiles();
        Player_HideIfOverUI();
    }
}

void ViridianMart_Start(void)   { Pokemart_SetInventory(kInv_Viridian);  Pokemart_Start(); }
void PewterMart_Start(void)     { Pokemart_SetInventory(kInv_Pewter);    Pokemart_Start(); }
void CeruleanMart_Start(void)   { Pokemart_SetInventory(kInv_Cerulean);  Pokemart_Start(); }
void VermilionMart_Start(void)  { Pokemart_SetInventory(kInv_Vermilion); Pokemart_Start(); }
void LavenderMart_Start(void)   { Pokemart_SetInventory(kInv_Lavender);  Pokemart_Start(); }
void Celadon2F1Mart_Start(void) { Pokemart_SetInventory(kInv_Cel2F1);    Pokemart_Start(); }
void Celadon2F2Mart_Start(void) { Pokemart_SetInventory(kInv_Cel2F2);    Pokemart_Start(); }
void Celadon4FMart_Start(void)  { Pokemart_SetInventory(kInv_Cel4F);     Pokemart_Start(); }
void Celadon5F1Mart_Start(void) { Pokemart_SetInventory(kInv_Cel5F1);    Pokemart_Start(); }
void Celadon5F2Mart_Start(void) { Pokemart_SetInventory(kInv_Cel5F2);    Pokemart_Start(); }
void FuchsiaMart_Start(void)    { Pokemart_SetInventory(kInv_Fuchsia);   Pokemart_Start(); }
void CinnabarMart_Start(void)   { Pokemart_SetInventory(kInv_Cinnabar);  Pokemart_Start(); }
void SaffronMart_Start(void)    { Pokemart_SetInventory(kInv_Saffron);   Pokemart_Start(); }
void IndigoMart_Start(void)     { Pokemart_SetInventory(kInv_Indigo);    Pokemart_Start(); }
