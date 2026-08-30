
#include "display.h"
#include "sgb_border.h"
#include "ntsc_filter.h"
#include "crt_renderer.h"
#include "../data/sgb_border_data.h"
#include "display_gl.h"
#include "assetpack_bind.h"
#include "hardware.h"
#include "../game/debug_cli.h"
#include "../game/constants.h"
#include "../game/trainer_sight.h"
#include <SDL.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;

static const uint32_t *(*s_suspend_overlay)(int *, int *, int *, int *, int *, int *) = NULL;

static int s_suspend_dim = 165;

static int s_deferred_win_scale;
static int s_deferred_fullscreen = -1;

static SDL_Texture  *fb_tex   = NULL;
static SDL_Texture  *sgb_tex  = NULL;
static int g_debug_render_mode = 0;

#define FB_MAX_W 256

int g_fb_w = SCREEN_WIDTH_PX;
int g_content_ox = 0;

static int g_blit_ox = 0;

static int g_authored_frame = 0;
void Display_SetAuthoredFrame(int on) { g_authored_frame = on ? 1 : 0; }
int  Display_AuthoredFrame(void)      { return g_authored_frame; }

static int g_letterbox_frame = 0;
static display_box_fill_t g_box_fill = DISPLAY_BOX_EXTEND;
void Display_SetLetterboxFrame(int on, display_box_fill_t fill) {
    g_letterbox_frame = on ? 1 : 0;
    g_box_fill = fill;
}
static int frame_is_boxed(void) { return g_authored_frame || g_letterbox_frame; }

static int g_bleed_y0 = 0, g_bleed_y1 = 0;
void Display_SetAuthoredBleedRows(int tile_row, int num_rows) {
    if (num_rows <= 0) { g_bleed_y0 = g_bleed_y1 = 0; return; }
    g_bleed_y0 = tile_row * TILE_PX;
    g_bleed_y1 = g_bleed_y0 + num_rows * TILE_PX;
}
static int row_bleeds(int dy) { return dy >= g_bleed_y0 && dy < g_bleed_y1; }

int Display_AuthoredRightEdgeCol(void) {
    return (g_fb_w - g_content_ox) / TILE_PX;
}

static int authored_clip_lo(void) { return g_content_ox; }
static int authored_clip_hi(void) { return g_content_ox + SCREEN_WIDTH_PX; }

static uint32_t lcd_previous_fb[FB_MAX_W * SCREEN_HEIGHT_PX];

static uint32_t lcd_present_previous_fb[FB_MAX_W * SCREEN_HEIGHT_PX];
static uint32_t lcd_blended_fb[FB_MAX_W * SCREEN_HEIGHT_PX];
static int g_lcd_ghosting_mode = DISPLAY_LCD_GHOSTING_PERSISTENCE;
static int g_lcd_previous_valid = 0;
static int s_present_previous_valid = 0;

static int s_speed_pct = 100;

static int s_render_fps = 60;
static uint64_t s_present_deadline_ctr;
static int g_lcd_blend_odd_frame = 0;
static int s_present_blend_odd_frame = 0;
static uint64_t s_source_serial;
static uint64_t s_last_presented_source_serial;
static int s_present_advances_source = 1;

static uint8_t  tile_gfx[256][TILE_SIZE];
static int      num_tiles_loaded = 0;

static uint8_t  sprite_tile_gfx[256][TILE_SIZE];

static SDL_Color bg_palette[4];
static SDL_Color obp0_palette[4];
static SDL_Color obp1_palette[4];

static uint32_t fb[FB_MAX_W * SCREEN_HEIGHT_PX];

static int g_shake_ox = 0;
static int g_shake_oy = 0;
static int g_wavy_enabled = 0;
static int g_wavy_phase = 0;

static int g_band_row_start = -1;
static int g_band_num_rows  =  0;
static int g_band_px        =  0;
static int g_window_over_sprites = 1;

static void blit_tile(int px, int py, uint8_t tile_id,
                      const SDL_Color pal[4], int flip_x, int flip_y,
                      int behind_bg);

static uint32_t s_tile_ov[SCREEN_HEIGHT * SCREEN_WIDTH];
static int      s_overlay_on = 0;

static int s_bid_overlay = 0;
static int s_bid_cam_tx  = 0;
static int s_bid_cam_ty  = 0;
static int (*s_bid_query)(int bx, int by) = NULL;

static uint8_t bg_idx[FB_MAX_W * SCREEN_HEIGHT_PX];

static int       g_color_mode = 0;
static uint8_t   bg_tile_attr[256];

static int       g_pos_attr_mode = 0;
static uint8_t   bg_pos_attr[SCREEN_HEIGHT][SCREEN_WIDTH];
static uint16_t  bg_cpal_raw[8][4];

#define OBJ_CPAL_COUNT 16
static uint16_t  obj_cpal_raw[OBJ_CPAL_COUNT][4];

static uint8_t   obj_cpal_perm[OBJ_CPAL_COUNT];
static uint8_t   obj_cpal_perm_on[OBJ_CPAL_COUNT];
static SDL_Color bg_cpal[8][4];
static SDL_Color obj_cpal[OBJ_CPAL_COUNT][4];

static int       g_fade_num = 1, g_fade_den = 1;
static int       g_fade_white = 0;

static uint8_t   g_bgp = 0xE4;
static int       g_cpal_dirty = 1;

static uint8_t bg_prio[FB_MAX_W * SCREEN_HEIGHT_PX];

static int g_gbc_curve = GBC_CURVE_LINEAR;

static const uint8_t kSameBoyCurveLUT[32] = {
    0,6,12,20,28,36,45,56,66,76,88,100,113,125,137,149,
    161,172,182,192,202,210,218,225,232,238,243,247,250,252,254,255
};

static const uint8_t kSameBoySgbCurveLUT[32] = {
    0,2,5,9,15,20,27,34,42,50,58,67,76,85,94,104,
    114,123,133,143,153,163,173,182,192,202,211,220,229,238,247,255
};

int Display_SgbChannelCurve(int v5) {
    if (v5 < 0) v5 = 0;
    if (v5 > 31) v5 = 31;

    if (g_gbc_curve == GBC_CURVE_LINEAR)
        return ((unsigned)v5 << 3) | ((unsigned)v5 >> 2);
    return kSameBoySgbCurveLUT[v5];
}

static int g_panel_layer = 0;

static void gbc_rgb15_lcd_panel(int r5, int g5, int b5, double lighten_screen,
                                int *out_r, int *out_g, int *out_b) {
    const double target_gamma = 2.2, display_gamma = 2.2;
    const double gamma_in = target_gamma - lighten_screen;
    const double lum = 0.94;
    double r = pow(r5 / 31.0, gamma_in) * lum;
    double g = pow(g5 / 31.0, gamma_in) * lum;
    double b = pow(b5 / 31.0, gamma_in) * lum;
    double nr = 0.820 * r + 0.240 * g - 0.060 * b;
    double ng = 0.125 * r + 0.665 * g + 0.210 * b;
    double nb = 0.195 * r + 0.075 * g + 0.730 * b;
    if (nr < 0) nr = 0;
    if (nr > 1) nr = 1;
    if (ng < 0) ng = 0;
    if (ng > 1) ng = 1;
    if (nb < 0) nb = 0;
    if (nb > 1) nb = 1;
    *out_r = (int)(pow(nr, 1.0 / display_gamma) * 255 + 0.5);
    *out_g = (int)(pow(ng, 1.0 / display_gamma) * 255 + 0.5);
    *out_b = (int)(pow(nb, 1.0 / display_gamma) * 255 + 0.5);
}

static void gbc_rgb15_sameboy_blend(uint8_t r5, uint8_t g5, uint8_t b5,
                                    double gamma, int mix,
                                    uint8_t *out_r, uint8_t *out_g, uint8_t *out_b) {
    uint8_t r = kSameBoyCurveLUT[r5];
    uint8_t g = kSameBoyCurveLUT[g5];
    uint8_t b = kSameBoyCurveLUT[b5];
    int nr = r, ng = g, nb = b;
    if (g != b)
        ng = (int)(pow((pow(g / 255.0, gamma) * 3 + pow(b / 255.0, gamma)) / 4,
                       1 / gamma) * 255 + 0.5);
    if (mix) {
        int mr = nr * 15 / 16 + (g + b) / 32;
        int mg = ng * 15 / 16 + (r + b) / 32;
        int mb = nb * 15 / 16 + (r + g) / 32;
        nr = mr > 255 ? 255 : mr;
        ng = mg > 255 ? 255 : mg;
        nb = mb > 255 ? 255 : mb;
    }
    *out_r = (uint8_t)nr; *out_g = (uint8_t)ng; *out_b = (uint8_t)nb;
}

static void gbc_rgb15_sameboy_compress(uint8_t r5, uint8_t g5, uint8_t b5,
                                       const int lo[3], const int hi[3],
                                       int *out_r, int *out_g, int *out_b) {
    uint8_t rr, gg, bb;

    gbc_rgb15_sameboy_blend(r5, g5, b5, 2.2, 1, &rr, &gg, &bb);
    *out_r = (int)rr * (hi[0] - lo[0]) / 255 + lo[0];
    *out_g = (int)gg * (hi[1] - lo[1]) / 255 + lo[1];
    *out_b = (int)bb * (hi[2] - lo[2]) / 255 + lo[2];
}

static void gbc_rgb15_sameboy_boost(uint8_t r5, uint8_t g5, uint8_t b5,
                                    int *out_r, int *out_g, int *out_b) {
    uint8_t r = kSameBoyCurveLUT[r5];
    uint8_t g = kSameBoyCurveLUT[g5];
    uint8_t b = kSameBoyCurveLUT[b5];
    uint8_t nr, ng, nb;
    int old_max, new_max, old_min, new_min;
    int vr, vg, vb;

    gbc_rgb15_sameboy_blend(r5, g5, b5, 1.6, 0, &nr, &ng, &nb);
    vr = nr; vg = ng; vb = nb;

    old_max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    new_max = vr > vg ? (vr > vb ? vr : vb) : (vg > vb ? vg : vb);
    if (new_max != 0) {
        vr = vr * old_max / new_max;
        vg = vg * old_max / new_max;
        vb = vb * old_max / new_max;
    }
    old_min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    new_min = vr < vg ? (vr < vb ? vr : vb) : (vg < vb ? vg : vb);
    if (new_min != 0xFF) {
        vr = 0xFF - (0xFF - vr) * (0xFF - old_min) / (0xFF - new_min);
        vg = 0xFF - (0xFF - vg) * (0xFF - old_min) / (0xFF - new_min);
        vb = 0xFF - (0xFF - vb) * (0xFF - old_min) / (0xFF - new_min);
    }
    *out_r = vr < 0 ? 0 : (vr > 255 ? 255 : vr);
    *out_g = vg < 0 ? 0 : (vg > 255 ? 255 : vg);
    *out_b = vb < 0 ? 0 : (vb > 255 ? 255 : vb);
}

static void gbc_rgb15_sameboy_mellow(uint8_t r5, uint8_t g5, uint8_t b5,
                                     uint8_t *out_r, uint8_t *out_g, uint8_t *out_b) {
    uint8_t r = kSameBoyCurveLUT[r5];
    uint8_t g = kSameBoyCurveLUT[g5];
    uint8_t b = kSameBoyCurveLUT[b5];
    uint8_t new_r, new_g, new_b;

    if (g != b) {

        double gamma = 2.2;
        new_g = (uint8_t)(pow((pow(g / 255.0, gamma) * 3 + pow(b / 255.0, gamma)) / 4,
                               1 / gamma) * 255 + 0.5);
    } else {
        new_g = g;
    }
    new_r = r;
    new_b = b;

    r = new_r;  g = new_g;  b = new_b;
    new_r = (uint8_t)(r * 15 / 16 + (g + b) / 32);
    new_g = (uint8_t)(g * 15 / 16 + (r + b) / 32);
    new_b = (uint8_t)(b * 15 / 16 + (r + g) / 32);

    new_r = (uint8_t)(new_r * (162 - 45) / 255 + 45);
    new_g = (uint8_t)(new_g * (167 - 41) / 255 + 41);
    new_b = (uint8_t)(new_b * (157 - 38) / 255 + 38);

    *out_r = new_r;  *out_g = new_g;  *out_b = new_b;
}

void Display_SetColorCurve(int curve) {
    if (curve == g_gbc_curve) return;
    g_gbc_curve = curve;
    g_cpal_dirty = 1;
}

int Display_GetColorCurve(void) { return g_gbc_curve; }

void Display_SetLCDGhosting(int on) {
    Display_SetLCDGhostingMode(on ? DISPLAY_LCD_GHOSTING_PERSISTENCE : DISPLAY_LCD_GHOSTING_OFF);
}

int Display_GetLCDGhosting(void) { return g_lcd_ghosting_mode != DISPLAY_LCD_GHOSTING_OFF; }

void Display_SetLCDGhostingMode(int mode) {
    if (mode < DISPLAY_LCD_GHOSTING_OFF || mode > DISPLAY_LCD_GHOSTING_SAMEBOY_ACCURATE) return;
    if (g_lcd_ghosting_mode == mode) return;
    g_lcd_ghosting_mode = mode;

    g_lcd_previous_valid = 0;
    s_present_previous_valid = 0;
}

int Display_GetLCDGhostingMode(void) { return g_lcd_ghosting_mode; }

static void gbc_curve_convert(int r5, int g5, int b5, int *r, int *g, int *b) {
    if (g_gbc_curve == GBC_CURVE_GAMBATTE) {
        int rr = (r5 * 13 + g5 *  2 + b5 *  1) >> 1;
        int gg = (          g5 *  3 + b5 *  1) << 1;
        int bb = (r5 *  3 + g5 *  2 + b5 * 11) >> 1;
        if (rr > 255) rr = 255;
        if (gg > 255) gg = 255;
        if (bb > 255) bb = 255;
        *r = rr;  *g = gg;  *b = bb;
    } else if (g_gbc_curve == GBC_CURVE_SAMEBOY_CURVE) {
        *r = kSameBoyCurveLUT[r5];
        *g = kSameBoyCurveLUT[g5];
        *b = kSameBoyCurveLUT[b5];
    } else if (g_gbc_curve == GBC_CURVE_SAMEBOY_SGB) {

        *r = kSameBoySgbCurveLUT[r5];
        *g = kSameBoySgbCurveLUT[g5];
        *b = kSameBoySgbCurveLUT[b5];
    } else if (g_gbc_curve == GBC_CURVE_SAMEBOY_MELLOW) {
        uint8_t rr, gg, bb;
        gbc_rgb15_sameboy_mellow((uint8_t)r5, (uint8_t)g5, (uint8_t)b5, &rr, &gg, &bb);
        *r = rr;  *g = gg;  *b = bb;
    } else if (g_gbc_curve == GBC_CURVE_SAMEBOY_HW) {
        uint8_t rr, gg, bb;
        gbc_rgb15_sameboy_blend((uint8_t)r5, (uint8_t)g5, (uint8_t)b5, 1.6, 0,
                                &rr, &gg, &bb);
        *r = rr;  *g = gg;  *b = bb;
    } else if (g_gbc_curve == GBC_CURVE_LCD_PANEL) {

        double lighten = g_panel_layer ? 1.0 : 0.0;
        gbc_rgb15_lcd_panel(r5, g5, b5, lighten, r, g, b);
    } else if (g_gbc_curve == GBC_CURVE_SAMEBOY_REDUCE) {
        static const int lo[3] = { 40,  36,  32};
        static const int hi[3] = {220, 224, 216};
        gbc_rgb15_sameboy_compress((uint8_t)r5, (uint8_t)g5, (uint8_t)b5, lo, hi, r, g, b);
    } else if (g_gbc_curve == GBC_CURVE_SAMEBOY_HARSH) {
        static const int lo[3] = { 45,  41,  38};
        static const int hi[3] = {162, 167, 157};
        gbc_rgb15_sameboy_compress((uint8_t)r5, (uint8_t)g5, (uint8_t)b5, lo, hi, r, g, b);
    } else if (g_gbc_curve == GBC_CURVE_SAMEBOY_BOOST) {
        gbc_rgb15_sameboy_boost((uint8_t)r5, (uint8_t)g5, (uint8_t)b5, r, g, b);
    } else if (g_gbc_curve == GBC_CURVE_SAMEBOY_SOFT) {
        uint8_t rr, gg, bb;
        gbc_rgb15_sameboy_blend((uint8_t)r5, (uint8_t)g5, (uint8_t)b5, 2.2, 1,
                                &rr, &gg, &bb);
        *r = rr;  *g = gg;  *b = bb;
    } else {
        *r = (r5 * 255 + 15) / 31;
        *g = (g5 * 255 + 15) / 31;
        *b = (b5 * 255 + 15) / 31;
    }
}

static void gbc_rgb555_to_rgba_w(uint16_t c, SDL_Color *out, int fnum, int fden,
                                 int fwhite) {
    int r5 = c & 0x1F, g5 = (c >> 5) & 0x1F, b5 = (c >> 10) & 0x1F;
    int r, g, b;
    gbc_curve_convert(r5, g5, b5, &r, &g, &b);

    if (fwhite > 0 || (fden > 0 && fnum < fden)) {
        int wr, wg, wb, kr, kg, kb;
        gbc_curve_convert(31, 31, 31, &wr, &wg, &wb);
        gbc_curve_convert(0, 0, 0, &kr, &kg, &kb);
        if (fwhite > 0) {
            r += (wr - r) * fwhite / 255;
            g += (wg - g) * fwhite / 255;
            b += (wb - b) * fwhite / 255;
        }
        if (fden > 0 && fnum < fden) {
            r = kr + (r - kr) * fnum / fden;
            g = kg + (g - kg) * fnum / fden;
            b = kb + (b - kb) * fnum / fden;
        }
    }
    out->r = (uint8_t)r;  out->g = (uint8_t)g;  out->b = (uint8_t)b;  out->a = 0xFF;
}

static void gbc_rgb555_to_rgba(uint16_t c, SDL_Color *out, int fnum, int fden) {
    gbc_rgb555_to_rgba_w(c, out, fnum, fden, g_fade_white);
}

static int s_sgb_flash_compat = 0;

int Display_SgbFlashCompat(void) { return s_sgb_flash_compat; }

void Display_SetSgbBorderLogicalSize(int on) {
    if (!renderer || g_debug_render_mode) return;
    if (on) SDL_RenderSetLogicalSize(renderer, SGB_FRAME_W, SGB_FRAME_H);
    else    SDL_RenderSetLogicalSize(renderer, g_fb_w, SCREEN_HEIGHT_PX);
    Display_ApplyScalingMode();
}

void Display_SetSgbFlashCompat(int on) {
    if (s_sgb_flash_compat == (on ? 1 : 0)) return;
    s_sgb_flash_compat = on ? 1 : 0;
    g_cpal_dirty = 1;
}

static void gbc_rebuild_palettes(void) {
    uint8_t bgp = g_bgp;
    g_panel_layer = 0;
    for (int p = 0; p < 8; p++) {
        for (int i = 0; i < 4; i++) {
            uint16_t c;
            if      (bgp == 0x00) c = 0x7FFF;
            else if (bgp == 0xFF) c = 0x0000;
            else if (s_sgb_flash_compat) {

                gbc_rgb555_to_rgba_w(bg_cpal_raw[p][i], &bg_cpal[p][i],
                                     g_fade_num, g_fade_den, g_fade_white);
                continue;
            }
            else                  c = bg_cpal_raw[p][(bgp >> (2 * i)) & 3];

            gbc_rgb555_to_rgba_w(c, &bg_cpal[p][i], 1, 1, 0);
        }
    }
    g_panel_layer = 1;
    for (int p = 0; p < OBJ_CPAL_COUNT; p++)
        for (int i = 0; i < 4; i++) {

            int src = obj_cpal_perm_on[p] ? ((obj_cpal_perm[p] >> (2 * i)) & 3) : i;
            gbc_rgb555_to_rgba(obj_cpal_raw[p][src], &obj_cpal[p][i],
                               g_fade_num, g_fade_den);
        }
    g_panel_layer = 0;
    g_cpal_dirty = 0;
}

int  Display_GetColorMode(void) { return g_color_mode; }

void Display_SetColorMode(int on) {
    on = on ? 1 : 0;
    if (on == g_color_mode) return;
    g_color_mode = on;
    g_cpal_dirty = 1;
}
int Display_ColorMode(void) { return g_color_mode; }

void Display_SetBGColorPalette(int slot, const uint16_t rgb555[4]) {
    if ((unsigned)slot >= 8 || !rgb555) return;
    for (int i = 0; i < 4; i++) bg_cpal_raw[slot][i] = rgb555[i];
    g_cpal_dirty = 1;
}

void Display_SetBGColorEntry(int slot, int color, uint16_t rgb555) {
    if ((unsigned)slot >= 8 || (unsigned)color >= 4) return;
    bg_cpal_raw[slot][color] = rgb555;
    g_cpal_dirty = 1;
}

void Display_SetOBJColorPalette(int slot, const uint16_t rgb555[4]) {
    if ((unsigned)slot >= OBJ_CPAL_COUNT || !rgb555) return;
    for (int i = 0; i < 4; i++) obj_cpal_raw[slot][i] = rgb555[i];
    g_cpal_dirty = 1;
}

void Display_SetOBJColorPermute(int slot, int dmg_pal) {
    if ((unsigned)slot >= OBJ_CPAL_COUNT) return;
    if (dmg_pal < 0) {
        if (!obj_cpal_perm_on[slot]) return;
        obj_cpal_perm_on[slot] = 0;
    } else {
        if (obj_cpal_perm_on[slot] && obj_cpal_perm[slot] == (uint8_t)dmg_pal) return;
        obj_cpal_perm[slot]    = (uint8_t)dmg_pal;
        obj_cpal_perm_on[slot] = 1;
    }
    g_cpal_dirty = 1;
}

void Display_SetTileAttr(uint8_t tile_id, uint8_t attr) {
    bg_tile_attr[tile_id] = attr;
}

uint8_t Display_GetTileAttr(uint8_t tile_id) {
    return bg_tile_attr[tile_id];
}

void Display_SetTileAttrs(uint8_t first, const uint8_t *attrs, int count) {
    if (!attrs) return;
    for (int i = 0; i < count; i++) {
        int id = first + i;
        if (id > 0xFF) break;
        bg_tile_attr[id] = attrs[i];
    }
}

void Display_FillTileAttrs(uint8_t first, int count, uint8_t attr) {
    for (int i = 0; i < count; i++) {
        int id = first + i;
        if (id > 0xFF) break;
        bg_tile_attr[id] = attr;
    }
}

void Display_SetPositionAttrMode(int on) {
    g_pos_attr_mode = on ? 1 : 0;
}

int Display_GetPositionAttrMode(void) { return g_pos_attr_mode; }

void Display_FillAttrBox(int col, int row, int w, int h, uint8_t attr) {
    for (int r = row; r < row + h; r++) {
        if ((unsigned)r >= SCREEN_HEIGHT) continue;
        for (int c = col; c < col + w; c++) {
            if ((unsigned)c >= SCREEN_WIDTH) continue;
            bg_pos_attr[r][c] = attr;
        }
    }
}

void Display_ClearAttrBoxes(uint8_t attr) {
    memset(bg_pos_attr, attr, sizeof(bg_pos_attr));
}

uint16_t Display_GetBGColorEntry(int slot, int color) {
    if ((unsigned)slot >= 8 || (unsigned)color >= 4) return 0;
    return bg_cpal_raw[slot][color];
}

uint16_t Display_GetOBJColorEntry(int slot, int color) {
    if ((unsigned)slot >= OBJ_CPAL_COUNT || (unsigned)color >= 4) return 0;
    return obj_cpal_raw[slot][color];
}

uint8_t Display_GetPositionAttr(int col, int row) {
    if ((unsigned)col >= SCREEN_WIDTH || (unsigned)row >= SCREEN_HEIGHT) return 0;
    return bg_pos_attr[row][col];
}

int Display_GetNumOBJPalettes(void) { return OBJ_CPAL_COUNT; }

void Display_GetColorFade(int *num, int *den, int *white) {
    if (num)   *num   = g_fade_num;
    if (den)   *den   = g_fade_den;
    if (white) *white = g_fade_white;
}

void Display_SetColorFade(int num, int den) {
    if (den <= 0) den = 1;
    if (num < 0) num = 0;
    if (num > den) num = den;
    if (num == g_fade_num && den == g_fade_den) return;
    g_fade_num = num;
    g_fade_den = den;
    g_cpal_dirty = 1;
}

static const SDL_Color MONO_PALETTES[DISPLAY_MONO_PAL_CUSTOM][5] = {

    {{0xE0,0xF8,0xD0,0xFF},{0x88,0xC0,0x70,0xFF},{0x34,0x68,0x56,0xFF},{0x08,0x18,0x20,0xFF},{0xE0,0xF8,0xD0,0xFF}},

    {{0xFF,0xFF,0xFF,0xFF},{0xAA,0xAA,0xAA,0xFF},{0x55,0x55,0x55,0xFF},{0x00,0x00,0x00,0xFF},{0xFF,0xFF,0xFF,0xFF}},

    {{0xC6,0xDE,0x8C,0xFF},{0x84,0xA5,0x63,0xFF},{0x39,0x61,0x39,0xFF},{0x08,0x18,0x10,0xFF},{0xD2,0xE6,0xA6,0xFF}},

    {{0xC2,0xCE,0x93,0xFF},{0x81,0x8D,0x66,0xFF},{0x3A,0x4C,0x3A,0xFF},{0x07,0x10,0x0E,0xFF},{0xCF,0xDA,0xAC,0xFF}},

    {{0x7F,0xE2,0xC3,0xFF},{0x56,0xB4,0x95,0xFF},{0x35,0x78,0x62,0xFF},{0x0A,0x1C,0x15,0xFF},{0x91,0xEA,0xD0,0xFF}},
};

static SDL_Color s_custom_pal[5] = {
    {0xE0,0xF8,0xD0,0xFF},{0x88,0xC0,0x70,0xFF},
    {0x34,0x68,0x56,0xFF},{0x08,0x18,0x20,0xFF},{0xE0,0xF8,0xD0,0xFF}
};
static int s_mono_pal = DISPLAY_MONO_PAL_PORT;

static const SDL_Color *mono_shades(void) {
    return s_mono_pal == DISPLAY_MONO_PAL_CUSTOM ? s_custom_pal
                                                 : MONO_PALETTES[s_mono_pal];
}

void Display_SetCustomShade(int shade, int r, int g, int b) {
    if (shade < 0 || shade > 3) return;
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    s_custom_pal[shade].r = (Uint8)r;
    s_custom_pal[shade].g = (Uint8)g;
    s_custom_pal[shade].b = (Uint8)b;
    s_custom_pal[shade].a = 0xFF;
    if (shade == 0) s_custom_pal[4] = s_custom_pal[0];

    if (s_mono_pal == DISPLAY_MONO_PAL_CUSTOM) g_cpal_dirty = 1;
}

void Display_GetCustomShade(int shade, int *r, int *g, int *b) {
    if (shade < 0 || shade > 3) shade = 0;
    if (r) *r = s_custom_pal[shade].r;
    if (g) *g = s_custom_pal[shade].g;
    if (b) *b = s_custom_pal[shade].b;
}

void Display_SetMonoPalette(int pal) {
    if (pal < 0 || pal >= DISPLAY_MONO_PAL_COUNT) return;
    s_mono_pal = pal;
    g_cpal_dirty = 1;
}
int Display_MonoPalette(void) { return s_mono_pal; }

static float s_light_temp;
static int s_tint_r = 256, s_tint_g = 256, s_tint_b = 256;

static void recompute_tint(void) {
    double t = (double)s_light_temp, r, g, b;
    if (t == 0.0) { s_tint_r = s_tint_g = s_tint_b = 256; return; }
    if (t >= 0) {
        r = 1.0;
        g = pow(1.0 - t, 0.375);
        b = (t >= 0.75) ? 0.0 : sqrt(0.75 - t) / sqrt(0.75);
    }
    else {
        double sq = t * t;
        b = 1.0;
        g = 0.125   * sq + 0.3 * t + 1.0;
        r = 0.21875 * sq + 0.5 * t + 1.0;
    }
    s_tint_r = (int)(r * 256.0 + 0.5);
    s_tint_g = (int)(g * 256.0 + 0.5);
    s_tint_b = (int)(b * 256.0 + 0.5);
}

void Display_SetLightTemperature(float t) {
    if (t < -1.0f) t = -1.0f;
    if (t >  1.0f) t =  1.0f;
    s_light_temp = t;
    recompute_tint();
    g_cpal_dirty = 1;
}
float Display_LightTemperature(void) { return s_light_temp; }

int Display_GetTint(int *r256, int *g256, int *b256) {
    if (r256) *r256 = s_tint_r;
    if (g256) *g256 = s_tint_g;
    if (b256) *b256 = s_tint_b;
    return (s_tint_r != 256 || s_tint_g != 256 || s_tint_b != 256);
}

static void decode_palette(uint8_t reg, SDL_Color out[4]) {
    for (int i = 0; i < 4; i++)
        out[i] = mono_shades()[(reg >> (i * 2)) & 3];
}

static int s_hidden_window = 0;

static int s_win_scale = DISPLAY_SCALE;

int Display_WindowScaleApplies(void) {
    return !Display_IsFullscreen() && !g_debug_render_mode && !s_hidden_window;
}

static int fit_window_scale(int rw, int rh, int scale) {
    SDL_Rect b;
    int dpy = window ? SDL_GetWindowDisplayIndex(window) : 0;
    int maxw, maxh, max;
    if (dpy < 0) dpy = 0;
    if (rw <= 0 || rh <= 0) return scale;
    if (SDL_GetDisplayBounds(dpy, &b) != 0 || b.w <= 0 || b.h <= 0)
        return scale;
    maxw = (b.w * 85 / 100) / rw;
    maxh = (b.h * 85 / 100) / rh;
    max  = maxw < maxh ? maxw : maxh;
    if (max < 1) max = 1;
    return scale > max ? max : scale;
}

void Display_SetWindowScale(int scale) {
    if (scale < 1) scale = 1;
    if (scale > 8) scale = 8;
    s_win_scale = scale;
    if (!window || !Display_WindowScaleApplies()) return;

    if (s_suspend_overlay) { s_deferred_win_scale = scale; return; }

    {

        int rw = Display_Widescreen() ? FB_MAX_W : SCREEN_WIDTH_PX;
        int rh = SCREEN_HEIGHT_PX;
        if (SgbBorder_IsEnabled() && SgbBorder_Available()) {
            rw = SGB_FRAME_W;
            rh = SGB_FRAME_H;
        }

        {
            int fitted = fit_window_scale(rw, rh, scale);
            if (fitted != scale) {

                fprintf(stderr, "[display] window scale %dx does not fit this "
                                "display; using %dx\n", scale, fitted);
                scale = fitted;
                s_win_scale = fitted;
            }
        }
        SDL_SetWindowSize(window, rw * scale, rh * scale);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
}

void Display_RefreshWindowScale(void) {
    Display_SetWindowScale(s_win_scale);
}
int Display_WindowScale(void) { return s_win_scale; }

void Display_SetHiddenWindow(int hidden) { s_hidden_window = hidden; }

static int s_fullscreen;
static int s_fullscreen_set;

int Display_IsSteamDeck(void) {
#ifdef _WIN32
    return 0;
#else
    static int cached = -1;
    if (cached >= 0) return cached;
    {
        const char *sd = getenv("SteamDeck");
        if (sd && sd[0] == '1') { cached = 1; return cached; }
    }
    cached = 0;
    {
        FILE *f = fopen("/sys/devices/virtual/dmi/id/product_name", "r");
        if (f) {
            char name[64] = {0};
            if (fgets(name, sizeof name, f) &&
                (strstr(name, "Jupiter") || strstr(name, "Galileo")))
                cached = 1;
            fclose(f);
        }
    }
    return cached;
#endif
}

#if defined(_WIN32) || defined(__APPLE__)
#define AMBER_CREATE_FULLSCREEN_FLAG SDL_WINDOW_FULLSCREEN_DESKTOP
static void apply_fullscreen(int on) {
    if (window)
        SDL_SetWindowFullscreen(window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}
#else

#define AMBER_CREATE_FULLSCREEN_FLAG SDL_WINDOW_BORDERLESS
static int s_saved_x, s_saved_y, s_saved_w, s_saved_h, s_saved_valid;

static void apply_fullscreen(int on) {
    if (!window) return;
    if (on) {
        if (!s_saved_valid) {
            SDL_GetWindowPosition(window, &s_saved_x, &s_saved_y);
            SDL_GetWindowSize(window, &s_saved_w, &s_saved_h);
            s_saved_valid = 1;
        }
        int di = SDL_GetWindowDisplayIndex(window);
        SDL_Rect b;
        if (di < 0 || SDL_GetDisplayBounds(di, &b) != 0) {
            if (SDL_GetDisplayBounds(0, &b) != 0) {
                b.x = b.y = 0; b.w = 1280; b.h = 800;
            }
        }
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowBordered(window, SDL_FALSE);
        SDL_SetWindowPosition(window, b.x, b.y);
        SDL_SetWindowSize(window, b.w, b.h);
    } else {
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowBordered(window, SDL_TRUE);
        if (s_saved_valid) {
            SDL_SetWindowSize(window, s_saved_w, s_saved_h);
            SDL_SetWindowPosition(window, s_saved_x, s_saved_y);
            s_saved_valid = 0;
        }
    }
}
#endif

void Display_SetFullscreen(int on) {
    s_fullscreen = on ? 1 : 0;

    s_fullscreen_set = 1;

    if (s_suspend_overlay) { s_deferred_fullscreen = s_fullscreen; return; }
    apply_fullscreen(s_fullscreen);
}

int Display_IsFullscreen(void) { return s_fullscreen; }

int Display_HasInputFocus(void) {
    return window && (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

void Display_SuspendFullscreenForOverlay(int suspended) {
    static int held;
    if (!window || !s_fullscreen) return;
    if (suspended == held) return;
    held = suspended;

    if (suspended) {

        SDL_Rect b;
        int di = SDL_GetWindowDisplayIndex(window);
        if (di < 0) di = 0;
        if (SDL_GetDisplayBounds(di, &b) != 0) { b.x = b.y = 0; b.w = 1280; b.h = 800; }
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowBordered(window, SDL_FALSE);
        SDL_SetWindowPosition(window, b.x, b.y);
        SDL_SetWindowSize(window, b.w, b.h);
    } else {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
}

void Display_ToggleFullscreen(void) { Display_SetFullscreen(!s_fullscreen); }

static int s_restart_pending;

void Display_RequestBackendRestart(void) { s_restart_pending = 1; }
int  Display_BackendRestartPending(void) { return s_restart_pending; }

int Display_Init(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return -1;

    if (!s_fullscreen_set) s_fullscreen = Display_IsSteamDeck();

    window = SDL_CreateWindow(

        "OldAmber",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,

        g_debug_render_mode ? 768
            : (g_fb_w * fit_window_scale(g_fb_w, SCREEN_HEIGHT_PX, s_win_scale)),
        g_debug_render_mode ? 432
            : (SCREEN_HEIGHT_PX * fit_window_scale(g_fb_w, SCREEN_HEIGHT_PX, s_win_scale)),
        (s_hidden_window ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN) |
            (g_debug_render_mode ? 0 : SDL_WINDOW_RESIZABLE) |
            ((s_fullscreen && !g_debug_render_mode && !s_hidden_window)
                 ? AMBER_CREATE_FULLSCREEN_FLAG : 0u) |

            (g_debug_render_mode ? 0u : DisplayGL_WantedWindowFlags())
    );
    if (!window) return -1;

#if !defined(_WIN32) && !defined(__APPLE__)

    if (s_fullscreen && !g_debug_render_mode && !s_hidden_window)
        apply_fullscreen(1);
#endif

    if (!g_debug_render_mode && DisplayGL_IsRequested()) {
        if (DisplayGL_Init(window) == 0) {

            Display_SetPalette(0xE4, 0xE4, 0xE4);
            return 0;
        }

        printf("[display] GL backend unavailable, using SDL renderer\n");
        fflush(stdout);
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) return -1;

    if (g_debug_render_mode) {
        SDL_RenderSetLogicalSize(renderer, 256, 144);
        SDL_RenderSetIntegerScale(renderer, SDL_TRUE);
    } else {
        SDL_RenderSetLogicalSize(renderer, g_fb_w, SCREEN_HEIGHT_PX);
        Display_ApplyScalingMode();
    }

    fb_tex = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        g_fb_w, SCREEN_HEIGHT_PX);
    if (!fb_tex) return -1;

    Display_SetPalette(0xE4, 0xE4, 0xE4);

    return 0;
}

void Display_ApplyScalingMode(void) {
    if (!renderer) return;
    SDL_RenderSetIntegerScale(renderer,
        DisplayGL_Scaling() == DISPLAY_GL_SCALE_INTEGER ? SDL_TRUE : SDL_FALSE);
}

void Display_SetDebugRenderMode(int on) {
    g_debug_render_mode = on ? 1 : 0;
}

static const struct { char ch; uint8_t rows[5]; } kFont3x5[] = {
        {'A',{0x2,0x5,0x7,0x5,0x5}}, {'B',{0x6,0x5,0x6,0x5,0x6}},
        {'C',{0x3,0x4,0x4,0x4,0x3}}, {'D',{0x6,0x5,0x5,0x5,0x6}},
        {'E',{0x7,0x4,0x6,0x4,0x7}}, {'F',{0x7,0x4,0x6,0x4,0x4}},
        {'G',{0x3,0x4,0x5,0x5,0x3}}, {'H',{0x5,0x5,0x7,0x5,0x5}},
        {'I',{0x7,0x2,0x2,0x2,0x7}}, {'J',{0x1,0x1,0x1,0x5,0x2}},
        {'K',{0x5,0x5,0x6,0x5,0x5}}, {'L',{0x4,0x4,0x4,0x4,0x7}},
        {'M',{0x5,0x7,0x7,0x5,0x5}}, {'N',{0x5,0x7,0x7,0x7,0x5}},
        {'O',{0x2,0x5,0x5,0x5,0x2}}, {'P',{0x6,0x5,0x6,0x4,0x4}},
        {'Q',{0x2,0x5,0x5,0x3,0x1}}, {'R',{0x6,0x5,0x6,0x5,0x5}},
        {'S',{0x3,0x4,0x2,0x1,0x6}}, {'T',{0x7,0x2,0x2,0x2,0x2}},
        {'U',{0x5,0x5,0x5,0x5,0x7}}, {'V',{0x5,0x5,0x5,0x5,0x2}},
        {'W',{0x5,0x5,0x7,0x7,0x5}}, {'X',{0x5,0x5,0x2,0x5,0x5}},
        {'Y',{0x5,0x5,0x2,0x2,0x2}}, {'Z',{0x7,0x1,0x2,0x4,0x7}},
        {'0',{0x7,0x5,0x5,0x5,0x7}}, {'1',{0x2,0x6,0x2,0x2,0x7}},
        {'2',{0x6,0x1,0x7,0x4,0x7}}, {'3',{0x6,0x1,0x3,0x1,0x6}},
        {'4',{0x5,0x5,0x7,0x1,0x1}}, {'5',{0x7,0x4,0x6,0x1,0x6}},
        {'6',{0x3,0x4,0x6,0x5,0x2}}, {'7',{0x7,0x1,0x2,0x2,0x2}},
        {'8',{0x2,0x5,0x2,0x5,0x2}}, {'9',{0x2,0x5,0x3,0x1,0x6}},
        {'-',{0x0,0x0,0x7,0x0,0x0}}, {'_',{0x0,0x0,0x0,0x0,0x7}},
        {':',{0x0,0x2,0x0,0x2,0x0}}, {'.',{0x0,0x0,0x0,0x0,0x2}},
        {'>',{0x4,0x2,0x1,0x2,0x4}}, {'/',{0x1,0x1,0x2,0x4,0x4}},
        {'%',{0x5,0x1,0x2,0x4,0x5}},
        {' ',{0x0,0x0,0x0,0x0,0x0}},
};

static int font3x5_rows(char ch, uint8_t out[5]) {
    char up = (char)((ch >= 'a' && ch <= 'z') ? (ch - 32) : ch);
    unsigned i;
    for (i = 0; i < sizeof(kFont3x5)/sizeof(kFont3x5[0]); i++) {
        if (kFont3x5[i].ch == up) { memcpy(out, kFont3x5[i].rows, 5); return 1; }
    }
    return 0;
}

static void draw_glyph_3x5(int x, int y, char ch, SDL_Color c) {
    uint8_t rows[5] = {0};
    if (!font3x5_rows(ch, rows)) return;
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
    for (int ry = 0; ry < 5; ry++) {
        for (int rx = 0; rx < 3; rx++) {
            if (rows[ry] & (1u << (2 - rx))) {
                SDL_RenderDrawPoint(renderer, x + rx, y + ry);
            }
        }
    }
}

static void draw_text_3x5(int x, int y, const char *s, SDL_Color c, int max_chars) {
    int i;
    if (!s) return;
    for (i = 0; s[i] && i < max_chars; i++) draw_glyph_3x5(x + i * 4, y, s[i], c);
}

static int draw_text_3x5_wrapped(int x, int y, const char *s, SDL_Color c, int max_chars, int max_rows) {
    int row = 0;
    const char *p = s;
    if (!s || !*s || max_chars <= 0 || max_rows <= 0) return 0;
    while (*p && row < max_rows) {
        char linebuf[128];
        int n = 0;
        while (*p && n < max_chars) {
            if (*p == '\n') { p++; break; }
            linebuf[n++] = *p++;
        }
        linebuf[n] = '\0';
        draw_text_3x5(x, y + row * 6, linebuf, c, max_chars);
        row++;
    }
    return row;
}

static int lcd_ghost_scaled(int weight_256) {
    if (s_speed_pct == 0) return 0;
    if (s_speed_pct <= 100) return weight_256;
    return weight_256 * 100 / s_speed_pct;
}

static int lcd_ghost_speed_scaled(void) {
    return (s_speed_pct == 0) || (s_speed_pct > 100);
}

static uint32_t lcd_mix_256(uint32_t current, uint32_t previous, int w) {
    const int cw = 256 - w;
    const uint32_t r = ((((current >> 24) & 0xFF) * cw) + (((previous >> 24) & 0xFF) * w)) >> 8;
    const uint32_t g = ((((current >> 16) & 0xFF) * cw) + (((previous >> 16) & 0xFF) * w)) >> 8;
    const uint32_t b = ((((current >>  8) & 0xFF) * cw) + (((previous >>  8) & 0xFF) * w)) >> 8;
    return (r << 24) | (g << 16) | (b << 8) | 0xFF;
}

static uint32_t lcd_blend_pixel(uint32_t current, uint32_t previous) {

    const uint32_t r = ((((current >> 24) & 0xFF) * 3) +
                        ((previous >> 24) & 0xFF)) / 4;
    const uint32_t g = ((((current >> 16) & 0xFF) * 3) +
                        ((previous >> 16) & 0xFF)) / 4;
    const uint32_t b = ((((current >>  8) & 0xFF) * 3) +
                        ((previous >>  8) & 0xFF)) / 4;
    return (r << 24) | (g << 16) | (b << 8) | 0xFF;
}

static const uint32_t *lcd_ghosted_frame(void) {
    if (g_lcd_ghosting_mode == DISPLAY_LCD_GHOSTING_OFF ||
        !s_present_previous_valid) return fb;

    if (g_lcd_ghosting_mode == DISPLAY_LCD_GHOSTING_SAMEBOY_ACCURATE) {
        for (int y = 0; y < SCREEN_HEIGHT_PX; y++) {

            const int previous_weight = ((y & 1) == s_present_blend_odd_frame) ? 1 : 2;
            const int current_weight = 3 - previous_weight;
            if (lcd_ghost_speed_scaled()) {

                const int w = lcd_ghost_scaled(previous_weight * 256 / 3);
                for (int x = 0; x < g_fb_w; x++) {
                    const int i = y * g_fb_w + x;
                    lcd_blended_fb[i] = lcd_mix_256(fb[i], lcd_present_previous_fb[i], w);
                }
                continue;
            }
            for (int x = 0; x < g_fb_w; x++) {
                const int i = y * g_fb_w + x;
                const uint32_t current = fb[i];
                const uint32_t previous = lcd_present_previous_fb[i];
                const uint32_t r = ((((current >> 24) & 0xFF) * current_weight) + (((previous >> 24) & 0xFF) * previous_weight)) / 3;
                const uint32_t g = ((((current >> 16) & 0xFF) * current_weight) + (((previous >> 16) & 0xFF) * previous_weight)) / 3;
                const uint32_t b = ((((current >>  8) & 0xFF) * current_weight) + (((previous >>  8) & 0xFF) * previous_weight)) / 3;
                lcd_blended_fb[i] = (r << 24) | (g << 16) | (b << 8) | 0xFF;
            }
        }
        return lcd_blended_fb;
    }

    if (lcd_ghost_speed_scaled()) {

        const int w = lcd_ghost_scaled(64);
        for (int i = 0; i < g_fb_w * SCREEN_HEIGHT_PX; i++)
            lcd_blended_fb[i] = lcd_mix_256(fb[i], lcd_present_previous_fb[i], w);
        return lcd_blended_fb;
    }

    for (int i = 0; i < g_fb_w * SCREEN_HEIGHT_PX; i++) {
        lcd_blended_fb[i] = lcd_blend_pixel(fb[i], lcd_present_previous_fb[i]);
    }
    return lcd_blended_fb;
}

static void lcd_remember_frame(const uint32_t *presented) {
    if (!s_present_advances_source) return;

    memcpy(lcd_previous_fb,
           g_lcd_ghosting_mode == DISPLAY_LCD_GHOSTING_SAMEBOY_ACCURATE ? fb : presented,
           (size_t)g_fb_w * SCREEN_HEIGHT_PX * sizeof(uint32_t));
    g_lcd_previous_valid = 1;
    g_lcd_blend_odd_frame = !g_lcd_blend_odd_frame;
}

static uint32_t s_tinted[FB_MAX_W * SCREEN_HEIGHT_PX];
static uint32_t s_tinted_prev[FB_MAX_W * SCREEN_HEIGHT_PX];

static const uint32_t *apply_tint_into(const uint32_t *src, uint32_t *dst) {
    int tr, tg, tb;
    if (!src) return NULL;
    if (!Display_GetTint(&tr, &tg, &tb)) return src;
    for (int i = 0; i < g_fb_w * SCREEN_HEIGHT_PX; i++) {
        uint32_t px = src[i];
        int r = (int)((px >> 24) & 0xFF) * tr >> 8;
        int g = (int)((px >> 16) & 0xFF) * tg >> 8;
        int b = (int)((px >>  8) & 0xFF) * tb >> 8;
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        dst[i] = ((uint32_t)r << 24) | ((uint32_t)g << 16) |
                 ((uint32_t)b << 8) | (px & 0xFF);
    }
    return dst;
}

static const uint32_t *apply_tint(const uint32_t *src) {
    return apply_tint_into(src, s_tinted);
}

static uint32_t s_sgb_composite[SGB_FRAME_W * SGB_FRAME_H];
static uint64_t s_crt_frame_number;

static int sgb_ensure_tex(int w, int h) {
    static int tw, th;
    if (!renderer) return 0;
    if (sgb_tex && tw == w && th == h) return 1;
    if (sgb_tex) SDL_DestroyTexture(sgb_tex);
    sgb_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!sgb_tex) { tw = th = 0; return 0; }
    tw = w; th = h;
    if (!g_debug_render_mode) {
        SDL_RenderSetLogicalSize(renderer, w, h);
        Display_ApplyScalingMode();
    }
    return 1;
}

static const uint32_t *sgb_compose(const uint32_t *game,
                                   int *out_w, int *out_h) {
    const uint32_t *border = SgbBorder_Frame();
    if (out_w) *out_w = SGB_FRAME_W;
    if (out_h) *out_h = SGB_FRAME_H;
    if (!border) return NULL;
    memcpy(s_sgb_composite, border, sizeof s_sgb_composite);
    for (int y = 0; y < SCREEN_HEIGHT_PX; y++) {

        int copy_w = g_fb_w;
        if (SGB_SCREEN_X + copy_w > SGB_FRAME_W) copy_w = SGB_FRAME_W - SGB_SCREEN_X;
        memcpy(&s_sgb_composite[(SGB_SCREEN_Y + y) * SGB_FRAME_W + SGB_SCREEN_X],
               &game[y * g_fb_w],
               (size_t)copy_w * sizeof(uint32_t));
    }

    return NtscFilter_Apply(s_sgb_composite, SGB_FRAME_W, SGB_FRAME_H,
                            out_w, out_h);
}

static void fill_crt_desc(crt_frame_desc_t *d, int tex_w, int tex_h,
                          int raster_w, int raster_h, int par_n, int par_d) {
    memset(d, 0, sizeof *d);
    d->texture_width  = tex_w;
    d->texture_height = tex_h;
    d->raster_width   = raster_w;
    d->raster_height  = raster_h;
    d->texture_active_rect = (crt_rect_i_t){ 0, 0, tex_w, tex_h };
    d->raster_active_rect  = (crt_rect_i_t){ 0, 0, raster_w, raster_h };
    d->pixel_aspect_num = par_n;
    d->pixel_aspect_den = par_d;
    d->scan_mode      = CRT_SCAN_PROGRESSIVE;
    d->field          = CRT_FIELD_NONE;
    d->input_transfer = CRT_TRANSFER_SRGB;
    d->input_gamma    = 2.2f;
    d->source_refresh_hz = 59.7275f;
    d->frame_number   = ++s_crt_frame_number;
}

void Display_SetSpeedPct(int pct) {
    if (pct != s_speed_pct) s_present_deadline_ctr = 0;
    s_speed_pct = pct;
}

void Display_SetRenderFPS(int fps) {
    if (fps < 15 || fps > 360) return;
    if (fps != s_render_fps) s_present_deadline_ctr = 0;
    s_render_fps = fps;
}

int Display_RenderFPS(void) { return s_render_fps; }
uint64_t Display_NextPresentCounter(void) { return s_present_deadline_ctr; }

static int present_is_due(void) {
    uint64_t hz, now, period;
    int catchup;

    if (s_render_fps == 60 && s_speed_pct != 0 && s_speed_pct <= 100) return 1;

    hz = SDL_GetPerformanceFrequency();
    if (!hz) return 1;
    now    = SDL_GetPerformanceCounter();
    period = hz / (uint64_t)s_render_fps;
    if (s_present_deadline_ctr == 0) {
        s_present_deadline_ctr = now + period;
        return 1;
    }
    if (now < s_present_deadline_ctr) return 0;

    catchup = 0;
    do {
        s_present_deadline_ctr += period;
        catchup++;
    } while (s_present_deadline_ctr <= now && catchup < 4);
    if (s_present_deadline_ctr <= now) s_present_deadline_ctr = now + period;
    return 1;
}

void Display_SetSuspendOverlay(const uint32_t *(*fn)(int *, int *, int *, int *, int *, int *)) {
    s_suspend_overlay = fn;

    if (!fn && DisplayGL_IsActive()) {
        DisplayGL_SetOverlay(NULL, 0, 0);
        DisplayGL_SetGameRect(0, 0, 0, 0);
    }

    if (!fn) {

        if (s_deferred_fullscreen >= 0) {
            int want = s_deferred_fullscreen;
            s_deferred_fullscreen = -1;
            apply_fullscreen(want);
        }
        if (s_deferred_win_scale) {
            s_deferred_win_scale = 0;
            Display_RefreshWindowScale();
        }
        Display_RestoreLogicalSize();
    }
}
int  Display_SuspendOverlayActive(void) { return s_suspend_overlay != NULL; }
void Display_SetSuspendDim(int a) { s_suspend_dim = (a < 0) ? 0 : (a > 255) ? 255 : a; }

static void present_now(void) { SDL_RenderPresent(renderer); }

static SDL_Texture *s_ov_tex;
static int s_ov_tex_w, s_ov_tex_h;

static int present_suspend_overlay(void) {
    int ow = 0, oh = 0, gx = 0, gy = 0, gw = 0, gh = 0;
    const uint32_t *px = s_suspend_overlay(&ow, &oh, &gx, &gy, &gw, &gh);
    if (!px || ow <= 0 || oh <= 0) return 0;

    if (DisplayGL_IsActive()) {

        int win_w = 0, win_h = 0, sc;
        Display_GetOutputSize(&win_w, &win_h);
        sc = (ow > 0) ? win_w / ow : 1;
        if (sc < 1) sc = 1;
        DisplayGL_SetOverlay(px, ow, oh);
        if (gw > 0 && gh > 0) {

            DisplayGL_SetGameRect(gx * sc, win_h - (gy + gh) * sc,
                                  gw * sc, gh * sc);
        }
        return 0;
    }
    if (!renderer) return 0;

    if (!s_ov_tex || s_ov_tex_w != ow || s_ov_tex_h != oh) {
        if (s_ov_tex) SDL_DestroyTexture(s_ov_tex);
        s_ov_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                     SDL_TEXTUREACCESS_STREAMING, ow, oh);
        s_ov_tex_w = ow; s_ov_tex_h = oh;
    }
    if (!s_ov_tex) return 0;

    SDL_UpdateTexture(s_ov_tex, NULL, px, ow * (int)sizeof(uint32_t));

    SDL_SetTextureBlendMode(s_ov_tex, SDL_BLENDMODE_BLEND);

    SDL_RenderSetLogicalSize(renderer, ow, oh);
    SDL_RenderSetIntegerScale(renderer, SDL_FALSE);
    SDL_RenderClear(renderer);

    const uint32_t *presented = apply_tint(lcd_ghosted_frame());
    if (gw > 0 && gh > 0 && presented) {
        SDL_Rect dst = { gx, gy, gw, gh };
        int cw = SGB_FRAME_W, ch = SGB_FRAME_H;
        const uint32_t *comp = g_debug_render_mode ? NULL
                                                   : sgb_compose(presented, &cw, &ch);
        if (comp && sgb_ensure_tex(cw, ch)) {
            SDL_UpdateTexture(sgb_tex, NULL, comp, cw * (int)sizeof(uint32_t));
            SDL_RenderCopy(renderer, sgb_tex, NULL, &dst);
        } else if (fb_tex) {
            SDL_UpdateTexture(fb_tex, NULL, presented,
                              g_fb_w * (int)sizeof(uint32_t));
            SDL_RenderCopy(renderer, fb_tex, NULL, &dst);
        }
    }

    SDL_RenderCopy(renderer, s_ov_tex, NULL, NULL);
    SDL_RenderPresent(renderer);
    return 1;
}

static void present_fb(void) {
    const uint32_t *presented;
    int advances_source;

    if (!present_is_due() || s_source_serial == 0) return;

    advances_source = s_source_serial != s_last_presented_source_serial;
    s_present_advances_source = advances_source;
    if (advances_source) {
        if (g_lcd_previous_valid) {
            memcpy(lcd_present_previous_fb, lcd_previous_fb,
                   (size_t)g_fb_w * SCREEN_HEIGHT_PX * sizeof(uint32_t));
        }
        s_present_previous_valid = g_lcd_previous_valid;
        s_present_blend_odd_frame = g_lcd_blend_odd_frame;
        s_last_presented_source_serial = s_source_serial;
    }

    DisplayGL_SetSourceFrameAdvanced(advances_source);

    if (s_suspend_overlay && present_suspend_overlay()) return;

    if (DisplayGL_IsActive()) {

        const uint32_t *prev = s_present_previous_valid
                             ? lcd_present_previous_fb : NULL;
        const uint32_t *cur_t  = apply_tint(fb);
        const uint32_t *prev_t = apply_tint_into(prev, s_tinted_prev);
        int cw = SGB_FRAME_W, ch = SGB_FRAME_H;
        const uint32_t *comp = sgb_compose(cur_t, &cw, &ch);
        if (comp) {

            crt_frame_desc_t desc;
            fill_crt_desc(&desc, cw, ch, SGB_FRAME_W, SGB_FRAME_H, 8, 7);

            DisplayGL_SetPixelAspect(8, 7);

            if (!DisplayGL_PresentCRT(comp, &desc))
                DisplayGL_PresentSized(comp, NULL, cw, ch);
            DisplayGL_SetPixelAspect(1, 1);
        } else {

            crt_frame_desc_t desc;
            fill_crt_desc(&desc, g_fb_w, SCREEN_HEIGHT_PX,
                          g_fb_w, SCREEN_HEIGHT_PX, 1, 1);

            if (!DisplayGL_PresentCRT(cur_t, &desc))
                DisplayGL_Present(cur_t, prev_t);
        }
        if (advances_source) {
            memcpy(lcd_previous_fb, fb,
                   (size_t)g_fb_w * SCREEN_HEIGHT_PX * sizeof(uint32_t));
            g_lcd_previous_valid = 1;
            g_lcd_blend_odd_frame = !g_lcd_blend_odd_frame;
        }
        return;
    }

    presented = apply_tint(lcd_ghosted_frame());
    if (!g_debug_render_mode) {
        int cw = SGB_FRAME_W, ch = SGB_FRAME_H;
        const uint32_t *comp = sgb_compose(presented, &cw, &ch);
        if (comp && sgb_ensure_tex(cw, ch)) {
            SDL_UpdateTexture(sgb_tex, NULL, comp,
                              cw * sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, sgb_tex, NULL, NULL);
            present_now();
            lcd_remember_frame(presented);
            return;
        }
    }
    SDL_UpdateTexture(fb_tex, NULL, presented, g_fb_w * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    if (!g_debug_render_mode) {

        SDL_RenderCopy(renderer, fb_tex, NULL, NULL);
        present_now();
        lcd_remember_frame(presented);
        return;
    }
    {
        SDL_Rect game_dst = {0, 0, 160, 144};
        SDL_Rect side_bg = {160, 0, 96, 144};
        SDL_Color fg = {0xB8, 0xF8, 0xD0, 0xFF};
        SDL_Color inp = {0xE0, 0xF8, 0xD0, 0xFF};
        SDL_Color okc = {0x7C, 0xF0, 0x7C, 0xFF};
        SDL_Color errc = {0xFF, 0x6C, 0x6C, 0xFF};
        SDL_Color logc = {0x7C, 0xBC, 0xFF, 0xFF};
        int lines = DebugCLI_GetHistoryCount();
        int y = 8;
        const int hist_x = 166;
        const int hist_max_chars = 22;
        const int hist_max_y = 135;
        const char *buf = DebugCLI_ConsoleGetBuffer();
        SDL_RenderCopy(renderer, fb_tex, NULL, &game_dst);
        SDL_SetRenderDrawColor(renderer, 0x08, 0x18, 0x20, 255);
        SDL_RenderFillRect(renderer, &side_bg);
        SDL_SetRenderDrawColor(renderer, 0x34, 0x68, 0x56, 255);
        SDL_RenderDrawLine(renderer, 160, 0, 160, 143);
        draw_text_3x5(hist_x, 1, "DEBUG CLI", fg, 16);
        for (int i = lines - 1; i >= 0 && y < hist_max_y; i--) {
            const char *ln = DebugCLI_GetHistoryLine(i);
            SDL_Color line_c = fg;
            int hist_color = DebugCLI_GetHistoryColor(i);
            int rows_left;
            int used_rows;
            if (!ln) continue;
            if (hist_color == CLI_HIST_COLOR_OK) line_c = okc;
            else if (hist_color == CLI_HIST_COLOR_ERROR) line_c = errc;
            else if (hist_color == CLI_HIST_COLOR_LOG) line_c = logc;
            rows_left = (hist_max_y - y) / 6;
            if (rows_left <= 0) break;
            used_rows = draw_text_3x5_wrapped(hist_x, y, ln, line_c, hist_max_chars, rows_left);
            if (used_rows <= 0) used_rows = 1;
            y += used_rows * 6;
        }
        draw_text_3x5(166, 138, "> ", inp, 2);
        draw_text_3x5(174, 138, buf ? buf : "", inp, 20);
    }
    present_now();
    lcd_remember_frame(presented);
}

void Display_PresentLatestIfDue(void) {
    present_fb();
}

int Display_LoadedTileCount(void) {
    return num_tiles_loaded;
}

void Display_LoadTileset(const uint8_t *gfx, int num_tiles) {
    if (num_tiles > 256) num_tiles = 256;
    memcpy(tile_gfx, gfx, num_tiles * TILE_SIZE);
    num_tiles_loaded = num_tiles;
}

void Display_LoadTile(uint8_t tile_id, const uint8_t *gfx) {
    memcpy(tile_gfx[tile_id], gfx, TILE_SIZE);
}

void Display_GetTile(uint8_t tile_id, uint8_t out[16]) {
    memcpy(out, tile_gfx[tile_id & 0xFF], TILE_SIZE);
}

void Display_LoadSpriteTile(uint8_t tile_id, const uint8_t *gfx) {
    memcpy(sprite_tile_gfx[tile_id], gfx, TILE_SIZE);
}

const uint8_t *Display_GetSpriteTile(uint8_t tile_id) {
    return sprite_tile_gfx[tile_id];
}

static void gbc_brightness_from_bgp(uint8_t bgp) {
    int sum = 0;

    g_bgp = bgp;
    for (int i = 0; i < 4; i++) sum += (bgp >> (i * 2)) & 3;
    if (sum > 6) {
        g_fade_num   = 12 - sum;
        g_fade_den   = 6;
        g_fade_white = 0;
    } else if (sum < 6) {
        g_fade_num   = 1;
        g_fade_den   = 1;
        g_fade_white = (6 - sum) * 255 / 6;
    } else {
        g_fade_num = 1; g_fade_den = 1; g_fade_white = 0;
    }
    g_cpal_dirty = 1;
}

static uint8_t s_last_bgp = 0xE4, s_last_obp0 = 0xD0, s_last_obp1 = 0xE0;

void Display_SetPalette(uint8_t bgp, uint8_t obp0, uint8_t obp1) {
    decode_palette(bgp,  bg_palette);
    decode_palette(obp0, obp0_palette);
    decode_palette(obp1, obp1_palette);
    gbc_brightness_from_bgp(bgp);
    s_last_bgp = bgp; s_last_obp0 = obp0; s_last_obp1 = obp1;
}

static void paldump(const char *why, uint8_t bgp) {
    static int last = -1;
    FILE *f;
    if ((int)bgp == last) return;
    last = (int)bgp;
    f = fopen("bugs/paldbg.log", "a");
    if (!f) return;
    fprintf(f, "[PALDUMP] %-14s bgp=%02X sgbcompat=%d color=%d\n",
            why, bgp, s_sgb_flash_compat, g_color_mode);
    if (g_color_mode) {
        gbc_rebuild_palettes();
        for (int p = 0; p < 8; p++) {
            fprintf(f, "    pal%d raw=", p);
            for (int i = 0; i < 4; i++) fprintf(f, "%04X ", bg_cpal_raw[p][i]);
            fprintf(f, " -> ");
            for (int i = 0; i < 4; i++)
                fprintf(f, "%02X%02X%02X ", bg_cpal[p][i].r,
                        bg_cpal[p][i].g, bg_cpal[p][i].b);
            fprintf(f, "\n");
        }
    }
    fclose(f);
}

void Display_SetBGP(uint8_t bgp)   { decode_palette(bgp,  bg_palette); gbc_brightness_from_bgp(bgp); s_last_bgp = bgp;  paldump("SetBGP", bgp); }

uint8_t Display_GetBGP(void)  { return s_last_bgp; }
uint8_t Display_GetOBP0(void) { return s_last_obp0; }
uint8_t Display_GetOBP1(void) { return s_last_obp1; }
void Display_SetOBP0(uint8_t obp0) { decode_palette(obp0, obp0_palette); s_last_obp0 = obp0; }
void Display_SetOBP1(uint8_t obp1) { decode_palette(obp1, obp1_palette); s_last_obp1 = obp1; }

void Display_LoadMapPalette(void) {

    int idx = 3 - (int)(gMapPalOffset / 3);
    if (idx < 0) idx = 0;
    if (idx > 7) idx = 7;

    Display_SetPalette(kFadePals[idx][0], kFadePals[idx][1], kFadePals[idx][2]);
}

void Display_SetShakeOffset(int ox, int oy) {
    g_shake_ox = ox;
    g_shake_oy = oy;
}

void Display_SetWavyPhase(int enabled, int phase) {
    g_wavy_enabled = enabled ? 1 : 0;
    g_wavy_phase = phase & 31;
}

void Display_SetBandXPx(int row_start, int num_rows, int px) {
    g_band_row_start = row_start;
    g_band_num_rows  = num_rows;
    g_band_px        = px;
}

void Display_SetWindowOverSprites(int on) {
    g_window_over_sprites = on ? 1 : 0;
}

static void draw_window_layer(void) {

    g_blit_ox = g_content_ox;
    if (hWY >= SCREEN_HEIGHT_PX) return;
    int win_x = (int)hWX - 7;
    int win_row_start = hWY / TILE_PX;
    for (int wy = win_row_start; wy < SCREEN_HEIGHT; wy++) {
        for (int wx = 0; wx < SCREEN_WIDTH; wx++) {
            int sx = win_x + wx * TILE_PX;
            if (sx + TILE_PX <= 0 || sx >= g_fb_w) continue;
            uint8_t tid = gWindowTileMap[wy][wx];
            if (tid == 0) continue;
            blit_tile(sx, wy * TILE_PX, tid, bg_palette, 0, 0, 0);
        }
    }
    g_blit_ox = 0;
}

static void apply_shake_to_fb(void) {
    static const int8_t kWavyOffsets[32] = {
        0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1,
        0, 0, 0, 0, 0,-1,-1,-1,-2,-2,-2,-2,-2,-1,-1,-1
    };
    if (g_shake_ox == 0 && g_shake_oy == 0 && !g_wavy_enabled) return;
    static uint32_t tmp[FB_MAX_W * SCREEN_HEIGHT_PX];
    for (int y = 0; y < SCREEN_HEIGHT_PX; y++) {
        int wave = 0;
        if (g_wavy_enabled) {
            wave = (int)kWavyOffsets[(g_wavy_phase + y) & 31];
        }
        for (int x = 0; x < g_fb_w; x++) {
            int sx = x - g_shake_ox - wave;
            int sy = y - g_shake_oy;
            tmp[y * g_fb_w + x] =
                (sx >= 0 && sx < g_fb_w && sy >= 0 && sy < SCREEN_HEIGHT_PX)
                    ? fb[sy * g_fb_w + sx]
                    : 0x000000FFu;
        }
    }
    memcpy(fb, tmp, sizeof(uint32_t) * g_fb_w * SCREEN_HEIGHT_PX);
}

static uint32_t g_zoom_snap[FB_MAX_W * SCREEN_HEIGHT_PX];
static int      g_zoom_active = 0;
static int      g_zoom_fx = 0, g_zoom_fy = 0;
static uint32_t g_zoom_fill = 0x000000FFu;
static int      g_zoom_scale_q8 = 256;

int Display_FindDarkestPixel(int x0, int y0, int w, int h, int *out_x, int *out_y) {
    int best = -1, bx = -1, by = -1;
    for (int y = y0; y < y0 + h; y++) {
        if (y < 0 || y >= SCREEN_HEIGHT_PX) continue;
        for (int x = x0; x < x0 + w; x++) {
            if (x < 0 || x >= g_fb_w) continue;
            uint32_t p = fb[y * g_fb_w + x];
            int lum = (int)((p >> 24) & 0xFF) + (int)((p >> 16) & 0xFF)
                    + (int)((p >> 8) & 0xFF);
            if (best < 0 || lum < best) { best = lum; bx = x; by = y; }
        }
    }
    if (bx < 0) return 0;
    if (out_x) *out_x = bx;
    if (out_y) *out_y = by;
    return 1;
}

void Display_ZoomBegin(int focus_x, int focus_y) {
    if (focus_x < 0) focus_x = 0;
    if (focus_y < 0) focus_y = 0;
    if (focus_x >= g_fb_w)  focus_x = g_fb_w - 1;
    if (focus_y >= SCREEN_HEIGHT_PX) focus_y = SCREEN_HEIGHT_PX - 1;
    memcpy(g_zoom_snap, fb, sizeof g_zoom_snap);
    g_zoom_fx = focus_x;
    g_zoom_fy = focus_y;

    g_zoom_fill = g_zoom_snap[focus_y * g_fb_w + focus_x];
    g_zoom_scale_q8 = 256;
    g_zoom_active = 1;
}

void Display_ZoomSetScale(int scale_q8) {
    if (scale_q8 < 256) scale_q8 = 256;
    g_zoom_scale_q8 = scale_q8;
}

void Display_ZoomEnd(void) { g_zoom_active = 0; }

static int zoom_src(int d, int focus, int scale_q8) {
    int num = d << 8;
    int q   = (num >= 0) ? (num + scale_q8 / 2) / scale_q8
                         : -((-num + scale_q8 / 2) / scale_q8);
    return focus + q;
}

static void apply_zoom_to_fb(void) {
    if (!g_zoom_active) return;
    for (int y = 0; y < SCREEN_HEIGHT_PX; y++) {
        int sy = zoom_src(y - g_zoom_fy, g_zoom_fy, g_zoom_scale_q8);
        for (int x = 0; x < g_fb_w; x++) {
            int sx = zoom_src(x - g_zoom_fx, g_zoom_fx, g_zoom_scale_q8);
            fb[y * g_fb_w + x] =
                (sx >= 0 && sx < g_fb_w && sy >= 0 && sy < SCREEN_HEIGHT_PX)
                    ? g_zoom_snap[sy * g_fb_w + sx]
                    : g_zoom_fill;
        }
    }
}

static uint32_t s_edge_l[SCREEN_HEIGHT_PX];
static uint32_t s_edge_r[SCREEN_HEIGHT_PX];

static void note_backdrop_edge(int dx, int dy, uint32_t px) {
    if (g_content_ox <= 0) return;
    if (dx == g_content_ox)                            s_edge_l[dy] = px;
    else if (dx == g_content_ox + SCREEN_WIDTH_PX - 1) s_edge_r[dy] = px;
}

static void blit_tile(int px, int py, uint8_t tile_id,
                      const SDL_Color pal[4], int flip_x, int flip_y,
                      int behind_bg) {
    px += g_blit_ox;
    const uint8_t *t = tile_gfx[tile_id & 0xFF];
    int prio = 0;

    if (g_color_mode) {
        uint8_t attr;
        if (g_pos_attr_mode) {
            int tc = px >> 3, tr = py >> 3;
            attr = ((unsigned)tc < SCREEN_WIDTH && (unsigned)tr < SCREEN_HEIGHT)
                 ? bg_pos_attr[tr][tc] : 0;
        } else {
            attr = bg_tile_attr[tile_id & 0xFF];
        }
        if (g_cpal_dirty) gbc_rebuild_palettes();
        pal  = bg_cpal[attr & 0x07];
        prio = (attr & 0x80) != 0;
    }

    const int boxed   = frame_is_boxed();
    const int clip_lo = g_content_ox;
    const int clip_hi = g_content_ox + SCREEN_WIDTH_PX;
    const int edge_l  = g_content_ox;
    const int edge_r  = clip_hi - 1;
    const int want_edge = (g_content_ox > 0);

    for (int row = 0; row < 8; row++) {
        int sy = flip_y ? (7 - row) : row;
        uint8_t lo = t[sy * 2];
        uint8_t hi = t[sy * 2 + 1];
        int dy = py + row;
        if (dy < 0 || dy >= SCREEN_HEIGHT_PX) continue;
        const int bleed = boxed ? row_bleeds(dy) : 0;
        for (int col = 0; col < 8; col++) {
            int sx    = flip_x ? col : (7 - col);
            int color = ((hi >> sx) & 1) << 1 | ((lo >> sx) & 1);
            int dx    = px + col;
            if (dx < 0 || dx >= g_fb_w) continue;
            if (boxed && !bleed && (dx < clip_lo || dx >= clip_hi)) continue;
            SDL_Color c = pal[color];
            uint32_t rgba = ((uint32_t)c.r << 24) | ((uint32_t)c.g << 16) |
                            ((uint32_t)c.b <<  8) | 0xFF;
            fb[dy * g_fb_w + dx] = rgba;
            if (want_edge) {
                if (dx == edge_l)      s_edge_l[dy] = rgba;
                else if (dx == edge_r) s_edge_r[dy] = rgba;
            }
            bg_idx[dy * g_fb_w + dx] = (uint8_t)color;
            bg_prio[dy * g_fb_w + dx] = (uint8_t)(prio && color != 0);
        }
    }
}

static const SDL_Color *sprite_palette_for(const oam_entry_t *s) {
    if (g_color_mode) {
        if (g_cpal_dirty) gbc_rebuild_palettes();

        return obj_cpal[s->flags & 0x0F];
    }
    return (s->flags & OAM_FLAG_PALETTE) ? obp1_palette : obp0_palette;
}

static void blit_sprite_entry(const oam_entry_t *s, int force_front) {
    if (s->y == 0 || s->y >= 160) return;

    int px      = (int)s->x - OAM_X_OFS + g_blit_ox;
    int py      = (int)s->y - OAM_Y_OFS;
    int flipx   = (s->flags & OAM_FLAG_FLIP_X) != 0;
    int flipy   = (s->flags & OAM_FLAG_FLIP_Y) != 0;
    int behind  = force_front ? 0 : ((s->flags & OAM_FLAG_PRIORITY) != 0);
    const SDL_Color *pal = sprite_palette_for(s);
    const uint8_t *t = sprite_tile_gfx[s->tile & 0xFF];
    for (int row = 0; row < 8; row++) {
        int sy = flipy ? (7 - row) : row;
        uint8_t lo = t[sy * 2];
        uint8_t hi = t[sy * 2 + 1];
        int dy = py + row;
        if (dy < 0 || dy >= SCREEN_HEIGHT_PX) continue;
        for (int col = 0; col < 8; col++) {
            int sx    = flipx ? col : (7 - col);
            int color = ((hi >> sx) & 1) << 1 | ((lo >> sx) & 1);
            if (color == 0) continue;
            int dx = px + col;
            if (dx < 0 || dx >= g_fb_w) continue;

            if (behind && bg_idx[dy * g_fb_w + dx] != 0) continue;

            if (!force_front && bg_prio[dy * g_fb_w + dx]) continue;
            SDL_Color c = pal[color];
            fb[dy * g_fb_w + dx] =
                ((uint32_t)c.r << 24) | ((uint32_t)c.g << 16) |
                ((uint32_t)c.b <<  8) | 0xFF;
        }
    }
}

static void draw_emotion_bubble_overlay(void) {

    oam_entry_t em[4];
    if (Emote_BuildOAM(em)) {
        for (int i = 0; i < 4; i++)
            blit_sprite_entry(&em[i], 1);
    }
}

static void apply_tile_overlay(void);
static void apply_block_id_overlay(int px, int py);
static void apply_speed_badge(void);

static void extend_frame_edges(void) {
    if (g_content_ox <= 0) return;
    const int right0 = g_content_ox + SCREEN_WIDTH_PX;
    for (int y = 0; y < SCREEN_HEIGHT_PX; y++) {

        if (row_bleeds(y)) continue;
        uint32_t *row = &fb[y * g_fb_w];

        uint32_t l = (g_box_fill == DISPLAY_BOX_BLACK) ? 0x000000FFu : s_edge_l[y];
        uint32_t r = (g_box_fill == DISPLAY_BOX_BLACK) ? 0x000000FFu : s_edge_r[y];
        for (int x = 0; x < g_content_ox; x++) row[x] = l;
        for (int x = right0; x < g_fb_w; x++)  row[x] = r;
    }
}

static void begin_source_frame(void) {
    if (s_source_serial != 0 &&
        s_source_serial != s_last_presented_source_serial &&
        g_lcd_ghosting_mode != DISPLAY_LCD_GHOSTING_OFF) {
        memcpy(lcd_previous_fb, fb,
               (size_t)g_fb_w * SCREEN_HEIGHT_PX * sizeof(uint32_t));
        g_lcd_previous_valid = 1;
    }
    s_source_serial++;
    if (s_source_serial == 0) s_source_serial = 1;
}

void Display_Render(void) {
    begin_source_frame();

    g_blit_ox = g_content_ox;

    for (int ty = 0; ty < SCREEN_HEIGHT; ty++) {
        for (int tx = 0; tx < SCREEN_WIDTH; tx++) {
            uint8_t tile_id = wTileMap[ty * SCREEN_WIDTH + tx];
            blit_tile(tx * TILE_PX, ty * TILE_PX, tile_id,
                      bg_palette, 0, 0, 0);
        }
    }

    extend_frame_edges();

    for (int i = MAX_SPRITES - 1; i >= 0; i--) {
        blit_sprite_entry(&wShadowOAM[i], 0);
    }

    draw_emotion_bubble_overlay();

    g_blit_ox = 0;

    apply_shake_to_fb();

    apply_zoom_to_fb();
    apply_tile_overlay();
    apply_block_id_overlay(0, 0);
    apply_speed_badge();
    present_fb();
}

void Display_RenderScrolled(int px, int py, const uint8_t *tile_map, int stride) {
    begin_source_frame();

    const int frame_ox = g_authored_frame ? g_content_ox : 0;

    const int sprite_ox = frame_is_boxed() ? g_content_ox : 0;

    SDL_Color c0;
    uint32_t clear_px;
    if (g_color_mode) {
        if (g_cpal_dirty) gbc_rebuild_palettes();
        c0 = bg_cpal[0][0];
    } else {
        c0 = bg_palette[0];
    }
    clear_px = ((uint32_t)c0.r << 24) | ((uint32_t)c0.g << 16) |
               ((uint32_t)c0.b <<  8) | 0xFF;
    for (int i = 0; i < g_fb_w * SCREEN_HEIGHT_PX; i++) {
        fb[i] = clear_px;
        bg_idx[i] = 0;
        bg_prio[i] = 0;
    }

    for (int by = 0; by < SCREEN_HEIGHT + 4; by++) {

        int screen_row = by - 2;
        int row_px = px;
        if (g_band_row_start >= 0 &&
            screen_row >= g_band_row_start &&
            screen_row < g_band_row_start + g_band_num_rows) {
            row_px += g_band_px;
        }

        g_blit_ox = frame_ox;
        for (int bx = 0, cols = g_fb_w / TILE_PX + 4; bx < cols; bx++) {
            int sx = bx * TILE_PX - 2 * TILE_PX + row_px;
            int sy = by * TILE_PX - 2 * TILE_PX + py;

            if (sx + frame_ox + TILE_PX <= 0 || sx + frame_ox >= g_fb_w) continue;
            if (sy + TILE_PX <= 0 || sy >= SCREEN_HEIGHT_PX) continue;
            uint8_t tid = tile_map[by * stride + bx];
            blit_tile(sx, sy, tid, bg_palette, 0, 0, 0);
        }
    }

    g_blit_ox = 0;

    if (!g_window_over_sprites) {
        draw_window_layer();
    }

    g_blit_ox = sprite_ox;
    for (int i = MAX_SPRITES - 1; i >= 0; i--) {
        blit_sprite_entry(&wShadowOAM[i], 0);
    }
    g_blit_ox = 0;

    if (g_window_over_sprites) {

        draw_window_layer();
    }

    if (frame_is_boxed()) extend_frame_edges();

    apply_shake_to_fb();

    apply_zoom_to_fb();
    apply_tile_overlay();
    apply_block_id_overlay(px, py);
    draw_emotion_bubble_overlay();
    apply_speed_badge();
    present_fb();
}

void Display_SetFrameWidth(int px) {
    if (px != SCREEN_WIDTH_PX && px != FB_MAX_W) return;
    if (px == g_fb_w) return;

    g_fb_w = px;
    g_content_ox = (g_fb_w - SCREEN_WIDTH_PX) / 2;

    g_lcd_previous_valid = 0;
    s_present_previous_valid = 0;

    s_last_presented_source_serial = s_source_serial;

    if (renderer) {
        if (fb_tex) { SDL_DestroyTexture(fb_tex); fb_tex = NULL; }
        fb_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                   SDL_TEXTUREACCESS_STREAMING,
                                   g_fb_w, SCREEN_HEIGHT_PX);

        if (!g_debug_render_mode && !s_suspend_overlay &&
            !(SgbBorder_IsEnabled() && SgbBorder_Available()))
            SDL_RenderSetLogicalSize(renderer, g_fb_w, SCREEN_HEIGHT_PX);
    }

    if (s_suspend_overlay) { s_deferred_win_scale = s_win_scale; return; }

    Display_RefreshWindowScale();

    printf("[display] frame width %d\n", g_fb_w);
}

int Display_FrameWidth(void)   { return g_fb_w; }

struct SDL_Renderer *Display_GetRenderer(void) { return renderer; }

void Display_GetOutputSize(int *w, int *h) {
    int ww = 0, hh = 0;
    if (renderer) {
        SDL_GetRendererOutputSize(renderer, &ww, &hh);
    } else if (window) {
        SDL_GL_GetDrawableSize(window, &ww, &hh);
        if (ww <= 0 || hh <= 0) SDL_GetWindowSize(window, &ww, &hh);
    }
    if (w) *w = ww;
    if (h) *h = hh;
}

void Display_BlitGameFrameTo(uint32_t *dst, int dst_w, int dst_h,
                             int x, int y, int w, int h) {
    if (!dst || w <= 0 || h <= 0 || dst_w <= 0 || dst_h <= 0) return;
    for (int dy = 0; dy < h; dy++) {
        int ty = y + dy;
        if (ty < 0 || ty >= dst_h) continue;
        int sy = dy * SCREEN_HEIGHT_PX / h;
        if (sy < 0) sy = 0;
        if (sy >= SCREEN_HEIGHT_PX) sy = SCREEN_HEIGHT_PX - 1;
        const uint32_t *src = &fb[sy * g_fb_w];
        uint32_t *row = &dst[ty * dst_w];
        for (int dx = 0; dx < w; dx++) {
            int tx = x + dx;
            if (tx < 0 || tx >= dst_w) continue;
            int sx = dx * g_fb_w / w;
            if (sx < 0) sx = 0;
            if (sx >= g_fb_w) sx = g_fb_w - 1;
            row[tx] = src[sx];
        }
    }
}

void Display_RestoreLogicalSize(void) {
    if (!renderer) return;
    if (g_debug_render_mode) {
        SDL_RenderSetLogicalSize(renderer, 256, SCREEN_HEIGHT_PX);
    } else if (SgbBorder_IsEnabled() && SgbBorder_Available()) {
        SDL_RenderSetLogicalSize(renderer, SGB_FRAME_W, SGB_FRAME_H);
    } else {
        SDL_RenderSetLogicalSize(renderer, g_fb_w, SCREEN_HEIGHT_PX);
    }
    Display_ApplyScalingMode();
}

static int g_widescreen_pref = 0;
void Display_SetWidescreen(int on) { g_widescreen_pref = on ? 1 : 0; }
int  Display_Widescreen(void)      { return g_widescreen_pref; }

int Display_WantFrameWidth(int content_supports_wide) {

    if (SgbBorder_IsEnabled() && SgbBorder_Available()) return SCREEN_WIDTH_PX;
    return (g_widescreen_pref && content_supports_wide) ? FB_MAX_W
                                                        : SCREEN_WIDTH_PX;
}
int Display_ContentOriginX(void) { return g_content_ox; }

void Display_SetOverlayEnabled(int on) {
    s_overlay_on = on;
    if (!on) memset(s_tile_ov, 0, sizeof(s_tile_ov));
}

void Display_ClearOverlay(void) {
    memset(s_tile_ov, 0, sizeof(s_tile_ov));
}

void Display_SetOverlayTile(int tx, int ty, uint32_t rgba) {
    if ((unsigned)tx < SCREEN_WIDTH && (unsigned)ty < SCREEN_HEIGHT)
        s_tile_ov[ty * SCREEN_WIDTH + tx] = rgba;
}

static void apply_tile_overlay(void) {
    if (!s_overlay_on) return;
    for (int ty = 0; ty < SCREEN_HEIGHT; ty++) {
        for (int tx = 0; tx < SCREEN_WIDTH; tx++) {
            uint32_t ov = s_tile_ov[ty * SCREEN_WIDTH + tx];
            if (!ov) continue;
            uint8_t oa  = (uint8_t)(ov        & 0xFF);
            uint8_t ovr = (uint8_t)(ov >> 24);
            uint8_t ovg = (uint8_t)(ov >> 16);
            uint8_t ovb = (uint8_t)(ov >>  8);
            for (int row = 0; row < TILE_PX; row++) {
                int py = ty * TILE_PX + row;
                for (int col = 0; col < TILE_PX; col++) {
                    int px = tx * TILE_PX + col;
                    uint32_t *p = &fb[py * g_fb_w + px];
                    uint8_t pr = (uint8_t)(*p >> 24);
                    uint8_t pg = (uint8_t)(*p >> 16);
                    uint8_t pb = (uint8_t)(*p >>  8);
                    pr = (uint8_t)((ovr * oa + pr * (255 - oa)) / 255);
                    pg = (uint8_t)((ovg * oa + pg * (255 - oa)) / 255);
                    pb = (uint8_t)((ovb * oa + pb * (255 - oa)) / 255);
                    *p = ((uint32_t)pr << 24) | ((uint32_t)pg << 16) |
                         ((uint32_t)pb <<  8) | 0xFF;
                }
            }
        }
    }
}

static const uint8_t kHexFont[16][5] = {
    {0x6,0x9,0x9,0x9,0x6},
    {0x2,0x6,0x2,0x2,0x7},
    {0xE,0x1,0x6,0x8,0xF},
    {0xE,0x1,0x6,0x1,0xE},
    {0x9,0x9,0xF,0x1,0x1},
    {0xF,0x8,0xE,0x1,0xE},
    {0x6,0x8,0xE,0x9,0x6},
    {0xF,0x1,0x2,0x4,0x4},
    {0x6,0x9,0x6,0x9,0x6},
    {0x6,0x9,0x7,0x1,0x6},
    {0x6,0x9,0xF,0x9,0x9},
    {0xE,0x9,0xE,0x9,0xE},
    {0x7,0x8,0x8,0x8,0x7},
    {0xE,0x9,0x9,0x9,0xE},
    {0xF,0x8,0xE,0x8,0xF},
    {0xF,0x8,0xE,0x8,0x8},
};

static void draw_hex_char(int sx, int sy, int nibble) {
    nibble &= 0xF;
    for (int row = 0; row < 5; row++) {
        int py = sy + row;
        if (py < 0 || py >= SCREEN_HEIGHT_PX) continue;
        uint8_t bits = kHexFont[nibble][row];
        for (int col = 0; col < 4; col++) {
            if (!(bits & (0x8u >> col))) continue;
            int px2 = sx + col;
            if (px2 < 0 || px2 >= g_fb_w) continue;
            fb[py * g_fb_w + px2] = 0xFFFFFFFFu;
        }
    }
}

static void apply_block_id_overlay(int px, int py) {
    if (!s_bid_overlay || !s_bid_query) return;

    int bx_start = s_bid_cam_tx / 4 - 2;
    int bx_end   = s_bid_cam_tx / 4 + 7;
    int by_start = s_bid_cam_ty / 4 - 2;
    int by_end   = s_bid_cam_ty / 4 + 7;

    for (int by = by_start; by <= by_end; by++) {
        for (int bx = bx_start; bx <= bx_end; bx++) {

            int sx = (bx * 4 - s_bid_cam_tx) * 8 + px;
            int sy = (by * 4 - s_bid_cam_ty) * 8 + py;
            if (sx + 32 <= 0 || sx >= g_fb_w) continue;
            if (sy + 32 <= 0 || sy >= SCREEN_HEIGHT_PX) continue;

            int bid = s_bid_query(bx, by);

            int bg_x = sx + 10, bg_y = sy + 12;
            for (int r = 0; r < 7; r++) {
                int fy = bg_y + r;
                if (fy < 0 || fy >= SCREEN_HEIGHT_PX) continue;
                for (int c = 0; c < 11; c++) {
                    int fx = bg_x + c;
                    if (fx < 0 || fx >= g_fb_w) continue;
                    uint32_t *p2 = &fb[fy * g_fb_w + fx];
                    uint8_t r2 = (uint8_t)((*p2 >> 24)        * 80 / 255);
                    uint8_t g2 = (uint8_t)((*p2 >> 16 & 0xFF) * 80 / 255);
                    uint8_t b2 = (uint8_t)((*p2 >>  8 & 0xFF) * 80 / 255);
                    *p2 = ((uint32_t)r2 << 24) | ((uint32_t)g2 << 16) |
                          ((uint32_t)b2 << 8) | 0xFF;
                }
            }

            draw_hex_char(sx + 11, sy + 13, (bid >> 4) & 0xF);
            draw_hex_char(sx + 16, sy + 13, bid & 0xF);
        }
    }
}

static char s_speed_badge[16] = "";

void Display_SetSpeedBadge(const char *label) {
    if (!label || !*label) { s_speed_badge[0] = '\0'; return; }
    snprintf(s_speed_badge, sizeof(s_speed_badge), "%s", label);
}

static void draw_text_3x5_fb(int x, int y, const char *s, uint32_t rgba) {
    int i;
    for (i = 0; s[i]; i++) {
        uint8_t rows[5];
        int gx = x + i * 4, ry, rx;
        if (!font3x5_rows(s[i], rows)) continue;
        for (ry = 0; ry < 5; ry++) {
            int py = y + ry;
            if (py < 0 || py >= SCREEN_HEIGHT_PX) continue;
            for (rx = 0; rx < 3; rx++) {
                int px2 = gx + rx;
                if (!(rows[ry] & (1u << (2 - rx)))) continue;
                if (px2 < 0 || px2 >= g_fb_w) continue;
                fb[py * g_fb_w + px2] = rgba;
            }
        }
    }
}

static void apply_speed_badge(void) {
    int len = (int)strlen(s_speed_badge);
    int w, h, bx, by, r, c;
    if (len <= 0) return;

    w  = len * 4 + 1;
    h  = 7;
    bx = g_fb_w - w - 2;
    by = 2;

    for (r = 0; r < h; r++) {
        int fy = by + r;
        if (fy < 0 || fy >= SCREEN_HEIGHT_PX) continue;
        for (c = 0; c < w; c++) {
            int fx = bx + c;
            uint32_t *p;
            if (fx < 0 || fx >= g_fb_w) continue;
            p = &fb[fy * g_fb_w + fx];
            *p = ((uint32_t)((*p >> 24)        * 80 / 255) << 24) |
                 ((uint32_t)((*p >> 16 & 0xFF) * 80 / 255) << 16) |
                 ((uint32_t)((*p >>  8 & 0xFF) * 80 / 255) <<  8) | 0xFF;
        }
    }
    draw_text_3x5_fb(bx + 1, by + 1, s_speed_badge, 0xFFFFFFFFu);
}

void Display_SetBlockIDOverlay(int enabled) { s_bid_overlay = enabled; }
int  Display_GetBlockIDOverlay(void)        { return s_bid_overlay; }
void Display_SetBlockIDQueryFn(int (*fn)(int bx, int by)) { s_bid_query = fn; }
void Display_SetBlockIDCam(int cam_tx, int cam_ty) {
    s_bid_cam_tx = cam_tx;
    s_bid_cam_ty = cam_ty;
}

int Display_SaveScreenshot(const char *path) {

    SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(
        (void *)fb,
        g_fb_w, SCREEN_HEIGHT_PX,
        32,
        g_fb_w * 4,
        0xFF000000u,
        0x00FF0000u,
        0x0000FF00u,
        0x000000FFu
    );
    if (!surf) return -1;
    int ret = SDL_SaveBMP(surf, path);
    SDL_FreeSurface(surf);
    return ret;
}

void Display_ApplyBackendRestart(void) {
    int px = SDL_WINDOWPOS_CENTERED, py = SDL_WINDOWPOS_CENTERED;
    uint8_t bgp, obp0, obp1;

    if (!s_restart_pending) return;
    s_restart_pending = 0;
    if (!window) return;

    bgp = s_last_bgp; obp0 = s_last_obp0; obp1 = s_last_obp1;
    if (!s_fullscreen) SDL_GetWindowPosition(window, &px, &py);

    DisplayGL_Shutdown();

    if (s_ov_tex) { SDL_DestroyTexture(s_ov_tex); s_ov_tex = NULL; }
    if (sgb_tex)  { SDL_DestroyTexture(sgb_tex);  sgb_tex  = NULL; }
    if (fb_tex)   { SDL_DestroyTexture(fb_tex);   fb_tex   = NULL; }
    if (renderer) { SDL_DestroyRenderer(renderer); renderer = NULL; }
    SDL_DestroyWindow(window);
    window = NULL;

    if (Display_Init() != 0) {

        printf("[display] backend restart failed; falling back to SDL\n");
        fflush(stdout);
        DisplayGL_SetRequested(0);
        if (Display_Init() != 0) return;
    }

    if (window && !s_fullscreen) SDL_SetWindowPosition(window, px, py);
    Display_SetPalette(bgp, obp0, obp1);
    Display_RefreshWindowScale();
    printf("[display] backend now: %s\n",
           DisplayGL_IsActive() ? "OpenGL" : "SDL");
    fflush(stdout);
}

void Display_Quit(void) {
    if (fb_tex)   SDL_DestroyTexture(fb_tex);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window)   SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
