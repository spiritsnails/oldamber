
#include "main_menu.h"
#include "overworld.h"
#include "gbc_color.h"
#include "../data/font_data.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "../platform/display.h"
#include <string.h>

#define BOX_L   0
#define BOX_R  14
#define BOX_T   0

#define ITEM_COL   2
#define ARROW_COL  1
#define ITEM_ROW_FIRST  2
#define ITEM_ROW_STEP   2

#define CHAR_TERM  0x50
#define CHAR_SPACE 0x7F
#define CHAR_ARROW 0xED

static const uint8_t kStrContinue[] = {

    0x82,0x8E,0x8D,0x93,0x88,0x8D,0x94,0x84,
    CHAR_TERM
};
static const uint8_t kStrNewGame[] = {
    0x8D,0x84,0x96,0x7F,0x86,0x80,0x8C,0x84,
    CHAR_TERM
};

static const uint8_t kStrOption[] = {
    0x8E,0x8F,0x93,0x88,0x8E,0x8D,
    CHAR_TERM
};

static int gOpen      = 0;
static int gHasSave   = 0;
static int gNumItems  = 0;
static int gCursor    = 0;
static int gResult    = MAIN_MENU_PENDING;

static void mm_set(int col, int row, uint8_t tile) {
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2)] = tile;
}

static void mm_fill_row(int col, int row, int len, uint8_t tile) {
    for (int i = 0; i < len; i++)
        mm_set(col + i, row, tile);
}

static void mm_str(int col, int row, const uint8_t *s) {
    for (; *s != CHAR_TERM; s++, col++)
        mm_set(col, row, (uint8_t)Font_CharToTile(*s));
}

static void mm_clear_screen(void) {

    uint8_t blank = (uint8_t)Font_CharToTile(CHAR_SPACE);
    for (int i = 0; i < SCROLL_MAP_W * SCROLL_MAP_H; i++)
        gScrollTileMap[i] = blank;
}

static void draw_box(void) {
    int L = BOX_L, R = BOX_R, T = BOX_T;
    int B = ITEM_ROW_FIRST + (gNumItems - 1) * ITEM_ROW_STEP + 1;
    uint8_t blank = (uint8_t)Font_CharToTile(CHAR_SPACE);

    mm_set(L, T, (uint8_t)Font_CharToTile(0x79));
    for (int c = L+1; c < R; c++)
        mm_set(c, T, (uint8_t)Font_CharToTile(0x7A));
    mm_set(R, T, (uint8_t)Font_CharToTile(0x7B));

    for (int r = T+1; r < B; r++) {
        mm_set(L, r, (uint8_t)Font_CharToTile(0x7C));
        mm_fill_row(L+1, r, R-L-1, blank);
        mm_set(R, r, (uint8_t)Font_CharToTile(0x7C));
    }

    mm_set(L, B, (uint8_t)Font_CharToTile(0x7D));
    for (int c = L+1; c < R; c++)
        mm_set(c, B, (uint8_t)Font_CharToTile(0x7A));
    mm_set(R, B, (uint8_t)Font_CharToTile(0x7E));
}

static void draw_items(void) {
    int row = ITEM_ROW_FIRST;
    if (gHasSave) {
        mm_str(ITEM_COL, row, kStrContinue); row += ITEM_ROW_STEP;
    }
    mm_str(ITEM_COL, row, kStrNewGame); row += ITEM_ROW_STEP;
    mm_str(ITEM_COL, row, kStrOption);
}

static void set_cursor(int item, uint8_t tile) {
    mm_set(ARROW_COL, ITEM_ROW_FIRST + item * ITEM_ROW_STEP, tile);
}

static void mm_apply_palettes(void) {
    Display_SetPalette(0xE4, 0xD0, 0xE0);
    if (!GbcColor_IsEnabled()) return;

    static const uint16_t kGrey[4] = {
        0x7FFF,
        0x56B5,
        0x294A,
        0x0000,
    };
    Display_SetPositionAttrMode(0);
    for (int i = 0; i < 8; i++) Display_SetBGColorPalette(i, kGrey);

    Display_ClearAttrBoxes(0);
    Display_SetColorMode(1);
}

void MainMenu_Open(int has_save) {
    gHasSave  = has_save;

    gNumItems = has_save ? 3 : 2;
    gCursor   = 0;
    gResult   = MAIN_MENU_PENDING;
    gOpen     = 1;

    mm_apply_palettes();
    mm_clear_screen();
    draw_box();
    draw_items();
    set_cursor(0, (uint8_t)Font_CharToTile(CHAR_ARROW));
}

int MainMenu_IsOpen(void)    { return gOpen; }
int MainMenu_GetResult(void) { return gResult; }

void MainMenu_Tick(void) {
    if (!gOpen) return;

    if (hJoyPressed & PAD_UP) {
        set_cursor(gCursor, (uint8_t)Font_CharToTile(CHAR_SPACE));
        gCursor = (gCursor == 0) ? gNumItems - 1 : gCursor - 1;
        set_cursor(gCursor, (uint8_t)Font_CharToTile(CHAR_ARROW));
        return;
    }
    if (hJoyPressed & PAD_DOWN) {
        set_cursor(gCursor, (uint8_t)Font_CharToTile(CHAR_SPACE));
        gCursor = (gCursor == gNumItems - 1) ? 0 : gCursor + 1;
        set_cursor(gCursor, (uint8_t)Font_CharToTile(CHAR_ARROW));
        return;
    }
    if (hJoyPressed & (PAD_A | PAD_START)) {
        Audio_PlaySFX_PressAB();
        gOpen = 0;

        int item = gHasSave ? gCursor : gCursor + 1;
        if (item == 0)
            gResult = MAIN_MENU_CONTINUE;
        else if (item == 1)
            gResult = MAIN_MENU_NEW_GAME;
        else
            gResult = MAIN_MENU_OPTION;
        return;
    }

    if (hJoyPressed & PAD_B) {
        Audio_PlaySFX_PressAB();
        gOpen = 0;
        gResult = MAIN_MENU_BACK;
    }
}
