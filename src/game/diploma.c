
#include "diploma.h"
#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "constants.h"
#include "rom_text.h"
#include "../data/font_data.h"
#include "../data/trainer_card_tiles.h"
#include "../data/title_screen_data.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include <string.h>

#define DP_TL 0x78
#define DP_TH 0x79
#define DP_TR 0x7A
#define DP_VL 0x7B
#define DP_VR 0x77
#define DP_BL 0x7C
#define DP_BH 0x76
#define DP_BR 0x7D

#define DP_CIRCLE_SLOT 127

#define DP_WHITEOUT_FRAMES 14
#define DP_PAL_NORMAL_R 0xE4
#define DP_PAL_NORMAL_G 0xD0
#define DP_PAL_NORMAL_B 0xE0

#define DP_PLAYER_COLS  7
#define DP_PLAYER_ROWS  7
#define DP_PLAYER_OAM   0
#define DP_PLAYER_TILE  0

static int s_open  = 0;
static int s_fade  = 0;
static int s_close = 0;
static uint8_t s_saved[SCROLL_MAP_W * SCROLL_MAP_H];

static void dp_set(int col, int row, uint8_t tile) {
    if ((unsigned)col >= SCREEN_WIDTH || (unsigned)row >= SCREEN_HEIGHT) return;
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
}

static int dp_ascii_tile(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return Font_CharToTile((uint8_t)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return Font_CharToTile((uint8_t)(0xA0 + (c - 'a')));
    if (c >= '0' && c <= '9') return Font_CharToTile((uint8_t)(0xF6 + (c - '0')));
    if (c == '!')  return Font_CharToTile(0xE7);
    if (c == '.')  return Font_CharToTile(0xE8);
    if (c == ' ')  return BLANK_TILE_SLOT;
    return BLANK_TILE_SLOT;
}

static void dp_str(int col, int row, const char *s) {
    int x = col;
    if (!s) return;
    for (; *s; s++) {
        if (*s == '\n' || *s == '\f') { row++; x = col; continue; }
        if (*s == '#') {
            dp_set(x++, row, (uint8_t)Font_CharToTile(0xE1));
            dp_set(x++, row, (uint8_t)Font_CharToTile(0xE2));
            continue;
        }
        dp_set(x++, row, (uint8_t)dp_ascii_tile((unsigned char)*s));
    }
}

static void dp_title(int col, int row) {
    const char *s = RomText("DiplomaText");
    const char *word = "Diploma";
    (void)s;
    dp_set(col, row, DP_CIRCLE_SLOT);
    dp_str(col + 1, row, word);
    dp_set(col + 1 + (int)strlen(word), row, DP_CIRCLE_SLOT);
}

static void dp_player_name(int col, int row) {
    for (int i = 0; i < NAME_LENGTH; i++) {
        uint8_t b = wPlayerName[i];
        if (b == 0x00 || b == 0x50) break;
        dp_set(col++, row, (uint8_t)Font_CharToTile(b));
    }
}

static void dp_border(int col, int row, int h, int w) {
    dp_set(col, row, DP_TL);
    for (int i = 0; i < w; i++) dp_set(col + 1 + i, row, DP_TH);
    dp_set(col + 1 + w, row, DP_TR);
    for (int r = 0; r < h; r++) {
        dp_set(col, row + 1 + r, DP_VL);
        for (int i = 0; i < w; i++) dp_set(col + 1 + i, row + 1 + r, BLANK_TILE_SLOT);
        dp_set(col + 1 + w, row + 1 + r, DP_VR);
    }
    dp_set(col, row + 1 + h, DP_BL);
    for (int i = 0; i < w; i++) dp_set(col + 1 + i, row + 1 + h, DP_BH);
    dp_set(col + 1 + w, row + 1 + h, DP_BR);
}

static void dp_draw_player(void) {
    int base_x = 0x5A + 33, base_y = 0x60;
    for (int c = 0; c < DP_PLAYER_COLS; c++) {
        for (int r = 0; r < DP_PLAYER_ROWS; r++) {
            int t = c * DP_PLAYER_ROWS + r;
            int o = DP_PLAYER_OAM + t;
            if (o >= MAX_SPRITES) break;
            Display_LoadSpriteTile((uint8_t)(DP_PLAYER_TILE + t), gTitlePlayerTiles[t]);
            wShadowOAM[o].y    = (uint8_t)(base_y + r * 8);
            wShadowOAM[o].x    = (uint8_t)(base_x + c * 8);
            wShadowOAM[o].tile = (uint8_t)(DP_PLAYER_TILE + t);
            wShadowOAM[o].flags = 0x80;
        }
    }
}

void Diploma_Open(void) {
    s_open  = 1;
    s_close = 0;

    memcpy(s_saved, gScrollTileMap, sizeof s_saved);
    for (int i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
    hWY = 144;

    for (int r = 0; r < SCREEN_HEIGHT; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            dp_set(c, r, BLANK_TILE_SLOT);

    Display_LoadTile(DP_CIRCLE_SLOT, kBadgeCircleTile);

    dp_border(0, 0, 16, 18);

    dp_title(5, 2);
    dp_str(3, 4, RomText("DiplomaPlayer"));
    dp_player_name(10, 4);
    dp_str(2, 6, RomText("DiplomaCongrats"));
    dp_str(9, 16, RomText("DiplomaGameFreak"));

    dp_draw_player();

    Display_SetPalette(0x00, 0x00, 0x00);
    s_fade = DP_WHITEOUT_FRAMES;
}

int Diploma_IsOpen(void) { return s_open; }

void Diploma_Tick(void) {
    if (!s_open) return;

    if (s_fade > 0) {
        if (--s_fade == 0)
            Display_SetPalette(DP_PAL_NORMAL_R, DP_PAL_NORMAL_G, DP_PAL_NORMAL_B);
        return;
    }

    if (s_close > 0) {
        if (--s_close == 0) {
            memcpy(gScrollTileMap, s_saved, sizeof s_saved);
            Map_ReloadGfx();
            Font_Load();
            NPC_ReloadTiles();
            for (int i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
            Player_SyncOAM();
            NPC_BuildView(gScrollPxX, gScrollPxY);
            Display_LoadMapPalette();
            s_open = 0;
        }
        return;
    }

    if (hJoyPressed & (PAD_A | PAD_B)) {
        Display_SetPalette(0x00, 0x00, 0x00);
        s_close = DP_WHITEOUT_FRAMES;
    }
}
