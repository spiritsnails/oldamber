
#include "font_data.h"
#include "../platform/display.h"
#include "crystal_font.h"

int gFontStyle = FONT_STYLE_GEN1;
static int s_gen2_frame = 0;

static int s_gen2_enabled = 1;

int Font_Gen2Enabled(void) { return s_gen2_enabled; }

void Font_SetGen2Enabled(int on) {
    s_gen2_enabled = on ? 1 : 0;
    if (!s_gen2_enabled) Font_SetStyle(FONT_STYLE_GEN1);
}

void Font_SetStyle(int style) {
    if (style != FONT_STYLE_GEN1 && style != FONT_STYLE_GEN2) return;
    if (style == gFontStyle) return;
    gFontStyle = style;
    Font_Load();
}

int Font_GetStyle(void) { return gFontStyle; }

void Font_SetGen2Frame(int frame) {
    if (frame < 0 || frame >= CRYSTAL_NUM_FRAMES) return;
    s_gen2_frame = frame;
    if (gFontStyle == FONT_STYLE_GEN2) Font_Load();
}

int Font_GetGen2Frame(void) { return s_gen2_frame; }

void Font_Load(void) {
    if (gFontStyle == FONT_STYLE_GEN2) {
        for (int i = 0; i < CRYSTAL_FONT_TILES; i++)
            Display_LoadTile((uint8_t)(FONT_TILE_BASE + i), gCrystalFont[i]);
        for (int i = 0; i < CRYSTAL_FRAME_TILES; i++)
            Display_LoadTile((uint8_t)(BOX_TILE_BASE + i),
                             gCrystalFrames[s_gen2_frame][i]);

        Display_LoadTile((uint8_t)BLANK_TILE_SLOT, gCrystalTextboxSpace);
        return;
    }

    for (int i = 0; i < 128; i++)
        Display_LoadTile((uint8_t)(FONT_TILE_BASE + i), gFontTiles[i]);

    for (int i = 0; i < GEN1_BOX_TILES; i++)
        Display_LoadTile((uint8_t)(BOX_TILE_BASE + i), gBoxTiles[i]);
}

void Font_LoadHudTiles(void) {
    if (gFontStyle == FONT_STYLE_GEN2) {

        for (int i = 0; i < CRYSTAL_BATTLE_EXTRA_TILES; i++)
            Display_LoadTile((uint8_t)(HUD_TILE_BASE + i),
                             gCrystalBattleExtra[i]);
        return;
    }

    for (int i = 0; i < GEN1_HUD_TILES; i++)
        Display_LoadTile((uint8_t)(HUD_TILE_BASE + i), gHudTiles[i]);
}