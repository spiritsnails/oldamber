
#include "sgb_border.h"
#include "display.h"
#include "../data/sgb_border_data.h"
#include <string.h>

static uint32_t s_frame[SGB_FRAME_W * SGB_FRAME_H];
static int      s_decoded = 0;
static int      s_on = 0;

static uint32_t bgr555_to_rgba(uint16_t v) {
    uint32_t r = (uint32_t)Display_SgbChannelCurve((v      ) & 31);
    uint32_t g = (uint32_t)Display_SgbChannelCurve((v >>  5) & 31);
    uint32_t b = (uint32_t)Display_SgbChannelCurve((v >> 10) & 31);
    return (r << 24) | (g << 16) | (b << 8) | 0xFF;
}

#define SGB_BACKDROP_BGR555  ((29u << 10) | (29u << 5) | 30u)

static int sgb_border_decode(void) {
    uint32_t pal[SGB_BORDER_PALETTES][4];
    uint32_t backdrop;

    if (s_decoded) return 1;

    if (!gSgbBorderTilemap || !gSgbBorderTiles || !gSgbBorderPalettes)
        return 0;

    for (int p = 0; p < SGB_BORDER_PALETTES; p++) {
        for (int i = 0; i < 4; i++) {
            uint16_t v = (uint16_t)(gSgbBorderPalettes[p][i * 2] |
                                   (gSgbBorderPalettes[p][i * 2 + 1] << 8));
            pal[p][i] = bgr555_to_rgba(v);
        }
    }
    backdrop = bgr555_to_rgba(SGB_BACKDROP_BGR555);

    memset(s_frame, 0, sizeof s_frame);
    for (int ty = 0; ty < SGB_BORDER_MAP_H; ty++) {
        for (int tx = 0; tx < SGB_BORDER_MAP_W; tx++) {
            const uint8_t *e = gSgbBorderTilemap[ty * SGB_BORDER_MAP_W + tx];
            uint16_t w     = (uint16_t)(e[0] | (e[1] << 8));
            int      tile  = w & 0x3FF;
            int      p     = ((w >> 10) & 7) - 4;
            int      xflip = (w >> 14) & 1;
            int      yflip = (w >> 15) & 1;
            const uint8_t *g;

            if (tile >= SGB_BORDER_TILES) continue;
            if (p < 0 || p >= SGB_BORDER_PALETTES) continue;
            g = gSgbBorderTiles[tile];

            for (int y = 0; y < 8; y++) {
                int sy = yflip ? 7 - y : y;
                uint8_t lo = g[sy * 2], hi = g[sy * 2 + 1];
                for (int x = 0; x < 8; x++) {
                    int sx  = xflip ? 7 - x : x;
                    int bit = 7 - sx;
                    int ci  = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);

                    s_frame[(ty * 8 + y) * SGB_FRAME_W + (tx * 8 + x)] =
                        ci ? pal[p][ci] : backdrop;
                }
            }
        }
    }
    s_decoded = 1;
    return 1;
}

int SgbBorder_Available(void) {
    return gSgbBorderTilemap && gSgbBorderTiles && gSgbBorderPalettes;
}

int SgbBorder_SetEnabled(int on) {
    if (!on) { s_on = 0; return 0; }
    if (!sgb_border_decode()) { s_on = 0; return 0; }
    s_on = 1;
    return 1;
}

int SgbBorder_IsEnabled(void) { return s_on; }

const uint32_t *SgbBorder_Frame(void) {
    return (s_on && s_decoded) ? s_frame : 0;
}
