
#include "crystal_fade.h"
#include "gbc_color.h"
#include "../platform/display.h"

static const uint8_t kCgbFade[7] = {
    0xFF, 0xFE, 0xF9, 0xE4, 0x90, 0x40, 0x00
};

#define FADE_IDENTITY 3
#define FADE_BLACK    0
#define FADE_WHITE    6
#define FADE_STEPS    4

#define FADE_DELAY_ROM 2
static int s_delay = 4;
#define FADE_DELAY (s_delay)

static int s_active = 0;
static int s_index  = FADE_IDENTITY;
static int s_step   = 0;
static int s_dir    = 1;
static int s_timer  = 0;

static void apply(void) {
    uint8_t v = kCgbFade[s_index];
    Display_SetPalette(v, v, v);
}

static void fill_white_bg_color(void) {
    uint16_t white;
    if (!GbcColor_IsEnabled()) return;
    white = Display_GetBGColorEntry(0, 0);
    for (int p = 1; p <= 6; p++)
        Display_SetBGColorEntry(p, 0, white);
}

void CrystalFade_Start(int kind) {
    switch (kind) {
    case CRYSTAL_FADE_OUT_TO_WHITE:
        fill_white_bg_color();
        s_index = FADE_IDENTITY; s_dir = +1; break;
    case CRYSTAL_FADE_IN_FROM_WHITE:
        fill_white_bg_color();
        s_index = FADE_WHITE;    s_dir = -1; break;
    case CRYSTAL_FADE_OUT_TO_BLACK:
        s_index = FADE_IDENTITY; s_dir = -1; break;
    case CRYSTAL_FADE_IN_FROM_BLACK:
        s_index = FADE_BLACK;    s_dir = +1; break;
    default:
        return;
    }

    s_active = 1;
    s_step   = 1;
    s_timer  = FADE_DELAY;
    apply();
}

int CrystalFade_Tick(void) {
    if (!s_active) return 0;
    if (--s_timer > 0) return 1;
    if (s_step >= FADE_STEPS) {
        s_active = 0;
        return 0;
    }
    s_index += s_dir;
    if (s_index < 0) s_index = 0;
    if (s_index > 6) s_index = 6;
    s_step++;
    s_timer = FADE_DELAY;
    apply();
    return 1;
}

int CrystalFade_Active(void) { return s_active; }

void CrystalFade_SetDelay(int frames) {
    if (frames < 1) frames = 1;
    if (frames > 30) frames = 30;
    s_delay = frames;
}

int CrystalFade_GetDelay(void) { return s_delay; }

void CrystalFade_Reset(void) {
    s_active = 0;
    s_index  = FADE_IDENTITY;
    s_step   = 0;
    apply();
}
