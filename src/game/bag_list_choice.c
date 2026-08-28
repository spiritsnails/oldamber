
#include "bag_list_choice.h"
#include "text.h"
#include "npc.h"
#include "inventory.h"
#include "overworld.h"
#include "player.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "../data/font_data.h"
#include <string.h>

#define BLC_TMIDX(r, c) (((r) + 2) * SCROLL_MAP_W + ((c) + 2) + Map_UiColOfsRight())
#define BLC_TL 0x79u
#define BLC_H  0x7Au
#define BLC_TR 0x7Bu
#define BLC_V  0x7Cu
#define BLC_BL 0x7Du
#define BLC_BR 0x7Eu
#define BLC_SP 0x7Fu
#define BLC_CUR 0xEDu

#define BLC_MAX_ITEMS 8

typedef enum { BLC_IDLE, BLC_OPEN, BLC_HELD } blc_state_t;
static blc_state_t g_state = BLC_IDLE;
static uint8_t g_owned[BLC_MAX_ITEMS];
static int g_owned_count = 0;
static int g_cursor = 0;
static uint8_t g_result = 0;
static int g_interior_w = 12;

static void blc_set(int col, int row, uint8_t tile) {
    if (col < 0 || col >= SCREEN_WIDTH || row < 0 || row >= SCREEN_HEIGHT) return;
    gScrollTileMap[BLC_TMIDX(row, col)] = tile;
}

static uint8_t blc_ct(char c) {
    if (c >= 'A' && c <= 'Z') return (uint8_t)Font_CharToTile((unsigned char)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return (uint8_t)Font_CharToTile((unsigned char)(0xA0 + (c - 'a')));
    return (uint8_t)Font_CharToTile(BLC_SP);
}

static void blc_label(int col, int row, const char *s) {
    while (*s) blc_set(col++, row, blc_ct(*s++));
}

static void blc_box(int top, int left, int h, int w) {
    for (int r = 1; r < h - 1; r++) {
        for (int c = 1; c < w - 1; c++)
            blc_set(left + c, top + r, (uint8_t)Font_CharToTile(BLC_SP));
        blc_set(left,         top + r, (uint8_t)Font_CharToTile(BLC_V));
        blc_set(left + w - 1, top + r, (uint8_t)Font_CharToTile(BLC_V));
    }
    blc_set(left,         top,         (uint8_t)Font_CharToTile(BLC_TL));
    blc_set(left + w - 1, top,         (uint8_t)Font_CharToTile(BLC_TR));
    blc_set(left,         top + h - 1, (uint8_t)Font_CharToTile(BLC_BL));
    blc_set(left + w - 1, top + h - 1, (uint8_t)Font_CharToTile(BLC_BR));
    for (int c = 1; c < w - 1; c++) {
        blc_set(left + c, top,         (uint8_t)Font_CharToTile(BLC_H));
        blc_set(left + c, top + h - 1, (uint8_t)Font_CharToTile(BLC_H));
    }
}

static void blc_draw(void) {

    int h = 2 * g_owned_count + 2;
    int w = g_interior_w + 2;
    blc_box(0, 0, h, w);
    for (int i = 0; i < g_owned_count; i++) {
        int row = 2 + i * 2;
        char name[16];
        Inventory_DecodeASCII(g_owned[i], name, sizeof(name));
        blc_set(1, row, (uint8_t)Font_CharToTile((g_cursor == i) ? BLC_CUR : BLC_SP));
        blc_label(2, row, name);
    }
    NPC_HideOverUITiles();
    Player_HideIfOverUI();
}

static void blc_close(void) {
    g_state = BLC_IDLE;
    Map_BuildScrollView();
    NPC_BuildView(gScrollPxX, gScrollPxY);
}

int  BagListChoice_IsHeld(void) { return g_state == BLC_HELD; }

void BagListChoice_Refresh(void) {
    if (g_state == BLC_HELD) blc_draw();
}

void BagListChoice_ClearHeld(void) {
    if (g_state != BLC_HELD) return;
    blc_close();
}

void BagListChoice_Open(const uint8_t *candidates, int count, int interior_w) {
    if (interior_w <= 0) interior_w = 12;
    if (interior_w > SCREEN_WIDTH - 2) interior_w = SCREEN_WIDTH - 2;
    g_interior_w = interior_w;
    g_owned_count = 0;
    for (int i = 0; i < count && g_owned_count < BLC_MAX_ITEMS; i++)
        if (Inventory_GetQty(candidates[i]) > 0)
            g_owned[g_owned_count++] = candidates[i];
    g_cursor = 0;
    g_result = 0;
    if (g_owned_count == 0) {
        g_state = BLC_IDLE;
        return;
    }

    g_state = BLC_OPEN;
}

int BagListChoice_IsOpen(void) { return g_state == BLC_OPEN; }

void BagListChoice_Tick(void) {
    if (g_state != BLC_OPEN) return;
    if (hJoyPressed & PAD_UP)   g_cursor = (g_cursor + g_owned_count - 1) % g_owned_count;
    if (hJoyPressed & PAD_DOWN) g_cursor = (g_cursor + 1) % g_owned_count;

    if (hJoyPressed & PAD_A) {
        Audio_PlaySFX_PressAB();
        g_result = g_owned[g_cursor];
        g_state = BLC_HELD;
        return;
    } else if (hJoyPressed & PAD_B) {
        Audio_PlaySFX_PressAB();
        g_result = 0;
        g_state = BLC_HELD;
        return;
    }

    blc_draw();
}

uint8_t BagListChoice_GetResult(void) { return g_result; }
