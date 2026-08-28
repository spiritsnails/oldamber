
#include "blackboard.h"
#include "text.h"
#include "constants.h"
#include "npc.h"
#include "overworld.h"
#include "player.h"
#include "../platform/hardware.h"
#include "rom_text.h"
#include "../platform/audio.h"
#include "../data/font_data.h"
#include <stdint.h>
#include <stdio.h>

#define BB_TMIDX(r, c) (((r) + 2) * SCROLL_MAP_W + ((c) + 2) + Map_UiColOfs())

#define BC_TL 0x79u
#define BC_H  0x7Au
#define BC_TR 0x7Bu
#define BC_V  0x7Cu
#define BC_BL 0x7Du
#define BC_BR 0x7Eu
#define BC_SP 0x7Fu
#define BC_CUR 0xEDu

typedef enum { BB_IDLE, BB_INTRO, BB_MENU, BB_STATUS } bb_state_t;
static bb_state_t g_state = BB_IDLE;
static int g_col = 0;
static int g_row = 0;

static const char *const kStatus[5] = {
    "ViridianBlackboardSleepText",
    "ViridianBlackboardPoisonText",
    "ViridianBlackboardPrlzText",
    "ViridianBlackboardBurnText",
    "ViridianBlackboardFrozenText",
};

static const char *const kLeft[3]  = { "SLP", "PSN", "PAR" };
static const char *const kRight[3] = { "BRN", "FRZ", "QUIT" };

static void bb_set(int col, int row, uint8_t tile) {
    if (col < 0 || col >= SCREEN_WIDTH || row < 0 || row >= SCREEN_HEIGHT) return;
    gScrollTileMap[BB_TMIDX(row, col)] = tile;
}

static uint8_t bb_ct(char c) {
    if (c >= 'A' && c <= 'Z') return (uint8_t)Font_CharToTile((unsigned char)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return (uint8_t)Font_CharToTile((unsigned char)(0xA0 + (c - 'a')));
    if (c == '!') return (uint8_t)Font_CharToTile(0xE7u);
    if (c == '?') return (uint8_t)Font_CharToTile(0xE6u);
    return (uint8_t)Font_CharToTile(BC_SP);
}

static void bb_label(int col, int row, const char *s) {
    while (*s) bb_set(col++, row, bb_ct(*s++));
}

static void bb_box(int top, int left, int h, int w) {
    for (int r = 1; r < h - 1; r++) {
        for (int c = 1; c < w - 1; c++)
            bb_set(left + c, top + r, (uint8_t)Font_CharToTile(BC_SP));
        bb_set(left,         top + r, (uint8_t)Font_CharToTile(BC_V));
        bb_set(left + w - 1, top + r, (uint8_t)Font_CharToTile(BC_V));
    }
    bb_set(left,         top,         (uint8_t)Font_CharToTile(BC_TL));
    bb_set(left + w - 1, top,         (uint8_t)Font_CharToTile(BC_TR));
    bb_set(left,         top + h - 1, (uint8_t)Font_CharToTile(BC_BL));
    bb_set(left + w - 1, top + h - 1, (uint8_t)Font_CharToTile(BC_BR));
    for (int c = 1; c < w - 1; c++) {
        bb_set(left + c, top,         (uint8_t)Font_CharToTile(BC_H));
        bb_set(left + c, top + h - 1, (uint8_t)Font_CharToTile(BC_H));
    }
}

static void bb_draw_menu(void) {
    bb_box(0, 0, 8, 12);
    for (int i = 0; i < 3; i++) {
        int row = 2 + i * 2;
        bb_set(1, row, (uint8_t)Font_CharToTile((g_col == 0 && g_row == i) ? BC_CUR : BC_SP));
        bb_label(2, row, kLeft[i]);
        bb_set(6, row, (uint8_t)Font_CharToTile((g_col == 1 && g_row == i) ? BC_CUR : BC_SP));
        bb_label(7, row, kRight[i]);
    }

    NPC_HideOverUITiles();
    Player_HideIfOverUI();
}

static void bb_win_set(int col, int row, uint8_t tile) {
    if (col < 0 || col >= SCREEN_WIDTH || row < 0 || row >= SCREEN_HEIGHT) return;
    gWindowTileMap[row][col] = tile;
}

static void bb_win_label(int col, int row, const char *s) {
    while (*s) bb_win_set(col++, row, bb_ct(*s++));
}

static void bb_win_box(int top, int left, int h, int w) {
    for (int r = 1; r < h - 1; r++) {
        for (int c = 1; c < w - 1; c++)
            bb_win_set(left + c, top + r, (uint8_t)Font_CharToTile(BC_SP));
        bb_win_set(left,         top + r, (uint8_t)Font_CharToTile(BC_V));
        bb_win_set(left + w - 1, top + r, (uint8_t)Font_CharToTile(BC_V));
    }
    bb_win_set(left,         top,         (uint8_t)Font_CharToTile(BC_TL));
    bb_win_set(left + w - 1, top,         (uint8_t)Font_CharToTile(BC_TR));
    bb_win_set(left,         top + h - 1, (uint8_t)Font_CharToTile(BC_BL));
    bb_win_set(left + w - 1, top + h - 1, (uint8_t)Font_CharToTile(BC_BR));
    for (int c = 1; c < w - 1; c++) {
        bb_win_set(left + c, top,         (uint8_t)Font_CharToTile(BC_H));
        bb_win_set(left + c, top + h - 1, (uint8_t)Font_CharToTile(BC_H));
    }
}

#define BB_BOX_ROW   12
#define BB_TEXT_COL0  1
#define BB_TEXT_ROW1 14
#define BB_TEXT_ROW2 16
static void bb_draw_prompt(void) {
    bb_win_box(BB_BOX_ROW, 0, 6, SCREEN_WIDTH);
    bb_win_label(BB_TEXT_COL0, BB_TEXT_ROW1, "Which heading do");
    bb_win_label(BB_TEXT_COL0, BB_TEXT_ROW2, "you want to read?");
}

static void bb_draw_all(void) {
    bb_draw_menu();
    bb_draw_prompt();
}

static void bb_close(void) {
    g_state = BB_IDLE;
    hWY = SCREEN_HEIGHT_PX;
    Map_BuildScrollView();
    NPC_BuildView(gScrollPxX, gScrollPxY);
}

void Blackboard_Open(void) {
    g_state = BB_INTRO;
    g_col = 0;
    g_row = 0;

    Text_KeepTilesOnClose();
    Text_ShowASCII("The blackboard\ndescribes #MON\nSTATUS changes\nduring battles.");
}

int Blackboard_IsOpen(void) { return g_state != BB_IDLE; }

void Blackboard_Tick(void) {

    switch (g_state) {
    case BB_INTRO:
        if (Text_IsOpen()) break;
        bb_draw_all();
        g_state = BB_MENU;
        break;

    case BB_MENU:
        if (hJoyPressed & PAD_UP)    { g_row = (g_row + 2) % 3; bb_draw_menu(); }
        if (hJoyPressed & PAD_DOWN)  { g_row = (g_row + 1) % 3; bb_draw_menu(); }
        if (hJoyPressed & PAD_LEFT)  { g_col = 0;               bb_draw_menu(); }
        if (hJoyPressed & PAD_RIGHT) { g_col = 1;               bb_draw_menu(); }
        if (hJoyPressed & PAD_A) {
            int idx = g_row + (g_col ? 3 : 0);
            Audio_PlaySFX_PressAB();
            if (idx >= 5) {
                bb_close();
            } else {

                Text_KeepTilesOnClose();
                Text_ShowASCII(RomText(kStatus[idx]));
                g_state = BB_STATUS;
            }
        } else if (hJoyPressed & PAD_B) {
            Audio_PlaySFX_PressAB();
            bb_close();
        }
        break;

    case BB_STATUS:

        if (Text_IsOpen()) break;
        bb_draw_all();
        g_state = BB_MENU;
        break;

    default:
        break;
    }
}

typedef enum { LCH_IDLE, LCH_INTRO, LCH_MENU, LCH_INFO } lch_state_t;
static lch_state_t g_lch_state = LCH_IDLE;
static int g_lch_cursor = 0;

static const char *const kLchItems[4] = { "HOW TO LINK", "COLOSSEUM", "TRADE CENTER", "STOP READING" };

static const char *const kLchInfo[3] = {
    "LinkCableInfoText1",
    "LinkCableInfoText2",
    "LinkCableInfoText3",
};

static void lch_draw_menu(void) {
    bb_box(0, 0, 10, 15);
    for (int i = 0; i < 4; i++) {
        int row = 2 + i * 2;
        bb_set(1, row, (uint8_t)Font_CharToTile((g_lch_cursor == i) ? BC_CUR : BC_SP));
        bb_label(2, row, kLchItems[i]);
    }

    NPC_HideOverUITiles();
    Player_HideIfOverUI();
}

static void lch_draw_all(void) {
    lch_draw_menu();
    bb_draw_prompt();
}

static void lch_close(void) {
    g_lch_state = LCH_IDLE;
    hWY = SCREEN_HEIGHT_PX;
    Map_BuildScrollView();
    NPC_BuildView(gScrollPxX, gScrollPxY);
}

void LinkCableHelp_Open(void) {
    g_lch_state = LCH_INTRO;
    g_lch_cursor = 0;
    Text_KeepTilesOnClose();
    Text_ShowASCII(RomText("LinkCableHelpText1"));
}

int LinkCableHelp_IsOpen(void) { return g_lch_state != LCH_IDLE; }

void LinkCableHelp_Tick(void) {
    switch (g_lch_state) {
    case LCH_INTRO:

        if (Text_IsOpen()) break;
        lch_draw_all();
        g_lch_state = LCH_MENU;
        break;

    case LCH_MENU:
        if (hJoyPressed & PAD_UP)   { g_lch_cursor = (g_lch_cursor + 3) % 4; lch_draw_menu(); }
        if (hJoyPressed & PAD_DOWN) { g_lch_cursor = (g_lch_cursor + 1) % 4; lch_draw_menu(); }
        if (hJoyPressed & PAD_A) {
            Audio_PlaySFX_PressAB();
            if (g_lch_cursor == 3) {
                lch_close();
            } else {
                Text_KeepTilesOnClose();
                Text_ShowASCII(RomText(kLchInfo[g_lch_cursor]));
                g_lch_state = LCH_INFO;
            }
        } else if (hJoyPressed & PAD_B) {
            Audio_PlaySFX_PressAB();
            lch_close();
        }
        break;

    case LCH_INFO:

        if (Text_IsOpen()) break;
        lch_draw_all();
        g_lch_state = LCH_MENU;
        break;

    default:
        break;
    }
}
