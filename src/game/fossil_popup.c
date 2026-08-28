
#include "fossil_popup.h"
#include "../data/fossil_sprites.h"
#include "constants.h"
#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "text.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../data/font_data.h"
#include <stdint.h>

#define BOX_L 6
#define BOX_T 4
#define BOX_R 14
#define BOX_B 13

#define SPR_COL 7
#define SPR_ROW 5
#define SPR_VRAM 128

#define FP_BOX_DELAY 20

#define GC_TL 0x79u
#define GC_H  0x7Au
#define GC_TR 0x7Bu
#define GC_V  0x7Cu
#define GC_BL 0x7Du
#define GC_BR 0x7Eu
#define GC_SP 0x7Fu

typedef enum { FP_IDLE, FP_BOX, FP_WAIT } fp_state_t;
static fp_state_t g_state = FP_IDLE;
static int  g_box_timer = 0;

static void fp_set(int col, int row, uint8_t tile) {
    if (col < 0 || col >= SCREEN_WIDTH || row < 0 || row >= SCREEN_HEIGHT) return;
    gWindowTileMap[row][col] = tile;
}

static void fp_draw_box(void) {
    fp_set(BOX_L, BOX_T, (uint8_t)Font_CharToTile(GC_TL));
    fp_set(BOX_R, BOX_T, (uint8_t)Font_CharToTile(GC_TR));
    fp_set(BOX_L, BOX_B, (uint8_t)Font_CharToTile(GC_BL));
    fp_set(BOX_R, BOX_B, (uint8_t)Font_CharToTile(GC_BR));
    for (int c = BOX_L + 1; c < BOX_R; c++) { fp_set(c, BOX_T, (uint8_t)Font_CharToTile(GC_H));
                                              fp_set(c, BOX_B, (uint8_t)Font_CharToTile(GC_H)); }
    for (int r = BOX_T + 1; r < BOX_B; r++) {
        fp_set(BOX_L, r, (uint8_t)Font_CharToTile(GC_V));
        fp_set(BOX_R, r, (uint8_t)Font_CharToTile(GC_V));
        for (int c = BOX_L + 1; c < BOX_R; c++) fp_set(c, r, (uint8_t)Font_CharToTile(GC_SP));
    }
}

static void fp_place_sprite(void) {
    for (int r = 0; r < 7; r++)
        for (int c = 0; c < 7; c++)
            fp_set(SPR_COL + c, SPR_ROW + r, (uint8_t)(SPR_VRAM + r * 7 + c));
}

static void fp_clear_window(void) {
    for (int r = 0; r < SCREEN_HEIGHT; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            gWindowTileMap[r][c] = 0;
}

void Fossil_Show(int which) {
    const uint8_t (*spr)[16] = (which == FOSSIL_KABUTOPS) ? gFossilKabutopsSprite
                                                          : gFossilAerodactylSprite;
    for (int i = 0; i < FOSSIL_SPRITE_TILES; i++)
        Display_LoadTile((uint8_t)(SPR_VRAM + i), spr[i]);
    fp_clear_window();
    fp_draw_box();
    hWX = 7;
    hWY = 0;
    g_box_timer = FP_BOX_DELAY;
    g_state = FP_BOX;
}

int Fossil_IsOpen(void) { return g_state != FP_IDLE; }

static void fp_close(void) {
    g_state = FP_IDLE;
    hWY = SCREEN_HEIGHT_PX;
    fp_clear_window();
    Font_Load();
    Map_BuildScrollView();
    NPC_BuildView(gScrollPxX, gScrollPxY);
}

void Fossil_Tick(void) {
    switch (g_state) {
    case FP_BOX:

        if (--g_box_timer <= 0) {
            fp_place_sprite();
            g_state = FP_WAIT;
        }
        break;
    case FP_WAIT:
        if (hJoyPressed & (PAD_A | PAD_B)) fp_close();
        break;
    default:
        break;
    }
}
