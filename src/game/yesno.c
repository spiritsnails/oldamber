
#include "yesno.h"
#include "text.h"
#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "constants.h"
#include "money_box.h"
#include "../data/font_data.h"
#include "../platform/hardware.h"

#define BOX_ROW   7
#define BOX_COL  14
#define BOX_W     6
#define BOX_H     5
#define YES_ROW  (BOX_ROW + 1)
#define OPT_STEP  2
#define NO_ROW   (YES_ROW + OPT_STEP)

#define POKE_SPACE  0x7F
#define POKE_CORNER_TL 0x79
#define POKE_HORIZ     0x7A
#define POKE_CORNER_TR 0x7B
#define POKE_VERT      0x7C
#define POKE_CORNER_BL 0x7D
#define POKE_CORNER_BR 0x7E
#define POKE_CURSOR    0xED

typedef enum {
    YN_IDLE = 0,
    YN_TEXT,
    YN_DRAW,
    YN_INPUT,
    YN_DONE,
} yn_state_t;

static yn_state_t s_state  = YN_IDLE;
static int        s_cursor = 0;
static int        s_result = 0;
static int        s_saved_valid = 0;
static int        s_money_armed = 0;

static uint8_t s_saved[BOX_H * BOX_W];

static int s_over_menu = 0;

static int poke_char(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return Font_CharToTile((unsigned char)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return Font_CharToTile((unsigned char)(0xA0 + (c - 'a')));
    return Font_CharToTile(POKE_SPACE);
}

#define TMIDX(r, c) (((r) + 2) * SCROLL_MAP_W + ((c) + 2) + Map_UiColOfs())

static void set_tile(int row, int col, int tile) {
    gScrollTileMap[TMIDX(row, col)] = (uint8_t)tile;
}

static void save_tiles(void) {
    for (int r = 0; r < BOX_H; r++)
        for (int c = 0; c < BOX_W; c++)
            s_saved[r * BOX_W + c] = gScrollTileMap[TMIDX(BOX_ROW + r, BOX_COL + c)];
}

static void restore_tiles(void) {
    for (int r = 0; r < BOX_H; r++)
        for (int c = 0; c < BOX_W; c++)
            gScrollTileMap[TMIDX(BOX_ROW + r, BOX_COL + c)] = s_saved[r * BOX_W + c];
}

static void draw_option_row(int row, const char *label, int selected) {
    set_tile(row, BOX_COL,             Font_CharToTile(POKE_VERT));
    set_tile(row, BOX_COL + 1, selected ? Font_CharToTile(POKE_CURSOR)
                                        : Font_CharToTile(POKE_SPACE));
    int at_end = 0;
    for (int c = 2; c < BOX_W - 1; c++) {
        char ch = at_end ? '\0' : label[c - 2];
        if (!ch) at_end = 1;
        set_tile(row, BOX_COL + c, ch ? poke_char((unsigned char)ch)
                                      : Font_CharToTile(POKE_SPACE));
    }
    set_tile(row, BOX_COL + BOX_W - 1, Font_CharToTile(POKE_VERT));
}

static void draw_box(void) {

    set_tile(BOX_ROW, BOX_COL,             Font_CharToTile(POKE_CORNER_TL));
    for (int c = 1; c < BOX_W - 1; c++)
        set_tile(BOX_ROW, BOX_COL + c,     Font_CharToTile(POKE_HORIZ));
    set_tile(BOX_ROW, BOX_COL + BOX_W - 1, Font_CharToTile(POKE_CORNER_TR));

    set_tile(BOX_ROW + BOX_H - 1, BOX_COL,             Font_CharToTile(POKE_CORNER_BL));
    for (int c = 1; c < BOX_W - 1; c++)
        set_tile(BOX_ROW + BOX_H - 1, BOX_COL + c,     Font_CharToTile(POKE_HORIZ));
    set_tile(BOX_ROW + BOX_H - 1, BOX_COL + BOX_W - 1, Font_CharToTile(POKE_CORNER_BR));

    for (int r = BOX_ROW + 1; r < BOX_ROW + BOX_H - 1; r++)
        draw_option_row(r, "", 0);
    draw_option_row(YES_ROW, "YES", s_cursor == 0);
    draw_option_row(NO_ROW,  "NO",  s_cursor == 1);
}

void YesNo_Show(const char *prompt) {
    s_state  = YN_TEXT;
    s_cursor = 0;
    s_result = 0;
    s_saved_valid = 0;

    Text_KeepTilesOnClose();
    Text_ShowASCII(prompt);
}

int  YesNo_IsOpen(void)      { return s_state != YN_IDLE; }
int  YesNo_GetResult(void)   { return s_result; }
void YesNo_ArmMoneyBox(void) { s_money_armed = 1; }
void YesNo_Reset(void) {
    if (s_saved_valid) {
        restore_tiles();
        s_saved_valid = 0;
    }
    s_state = YN_IDLE;
    s_cursor = 0;
    s_result = 0;

    Text_CancelKeepTilesOnClose();
    Text_Close();
}
void YesNo_PostRender(void)  {
    int show_box = (s_state == YN_DRAW || s_state == YN_INPUT);
    if (show_box) {
        if (!s_saved_valid) {
            save_tiles();
            s_saved_valid = 1;
        }
        draw_box();

        if (!s_over_menu) {
            NPC_HideOverUITiles();
            Player_HideIfOverUI();
        }
    }
}

void YesNo_SetOverMenu(int on) { s_over_menu = on ? 1 : 0; }

void YesNo_Tick(void) {
    switch (s_state) {
        case YN_IDLE:
            break;

        case YN_TEXT:

            if (!Text_IsOpen()) {
                if (!s_saved_valid) {
                    save_tiles();
                    s_saved_valid = 1;
                }
                if (s_money_armed) {
                    s_money_armed = 0;
                    MoneyBox_Draw();
                }
                s_state = YN_DRAW;
            }
            break;

        case YN_DRAW:
            draw_box();
            s_state = YN_INPUT;
            break;

        case YN_INPUT:
            if (hJoyPressed & PAD_UP)   s_cursor = 0;
            if (hJoyPressed & PAD_DOWN) s_cursor = 1;
            draw_box();
            if (hJoyPressed & PAD_A) {
                s_result = (s_cursor == 0) ? 1 : 0;
                if (s_saved_valid) restore_tiles();
                s_state = YN_DONE;
            } else if (hJoyPressed & PAD_B) {
                s_result = 0;
                if (s_saved_valid) restore_tiles();
                s_state = YN_DONE;
            }
            break;

        case YN_DONE:
            s_saved_valid = 0;
            s_state = YN_IDLE;
            break;
    }
}
