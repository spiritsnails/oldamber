#pragma once

#include <stdint.h>
#include "assetpack_bind.h"

#define GEN1_FONT_TILES  128
#define GEN1_BOX_TILES     7
#define GEN1_HUD_TILES    23

#define FONT_STYLE_GEN1   0
#define FONT_STYLE_GEN2   1
void Font_SetStyle(int style);
int  Font_GetStyle(void);

void Font_SetGen2Frame(int frame);
int  Font_GetGen2Frame(void);

void Font_SetGen2Enabled(int on);
int  Font_Gen2Enabled(void);

#define FONT_TILE_BASE    128
#define BOX_TILE_BASE     120
#define BLANK_TILE_SLOT   126

#define HUD_TILE_COUNT    25

#define HUD_TILE_BASE     2

extern int gFontStyle;

static inline unsigned char Font_Gen2Char(unsigned char c) {
    switch (c) {
    case 0xBA: return 0xEA;
    case 0xBB: return 0xD0;
    case 0xBC: return 0xD1;
    case 0xE5: return 0xD2;
    case 0xE4: return 0xD3;
    case 0xBD: return 0xD4;
    case 0xBE: return 0xD5;
    case 0xBF: return 0xD6;
    default:   return c;
    }
}

static inline int Font_CharToTile(unsigned char c) {
    if (gFontStyle == FONT_STYLE_GEN2) c = Font_Gen2Char(c);
    if (c >= 0x80) return FONT_TILE_BASE + (c - 0x80);

    if (gFontStyle == FONT_STYLE_GEN2 && c >= 0x60 && c <= 0x78)
        return HUD_TILE_BASE + (c - 0x60);
    if (c == 0x7F) return BLANK_TILE_SLOT;
    if (c >= 0x79 && c <= 0x7E) return BOX_TILE_BASE + (c - 0x79);

    if (c >= 0x62 && c <= 0x78) return HUD_TILE_BASE + (c - 0x62);
    return BLANK_TILE_SLOT;
}

void Font_Load(void);

void Font_LoadHudTiles(void);
