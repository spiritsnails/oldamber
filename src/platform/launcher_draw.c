
#include "launcher_draw.h"

#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

static void ldraw_str(SDL_Renderer *r, int x, int y, int scale,
                      Uint8 cr, Uint8 cg, Uint8 cb, const char *s, int bold) {
    if (!s) return;
    SDL_SetRenderDrawColor(r, cr, cg, cb, 0xFF);
    int cx = x;
    int adv = LDRAW_ADVANCE(scale) + (bold ? 1 : 0);
    for (const char *p = s; *p; p++) {
        if (*p == '\n') { cx = x; y += LAUNCHER_FONT_H * scale + scale * 2; continue; }
        const uint8_t *g = LauncherFont_Glyph(*p);
        if (g) {
            for (int row = 0; row < LAUNCHER_FONT_H; row++) {
                uint8_t bits = g[row];
                for (int col = 0; col < LAUNCHER_FONT_W; col++) {
                    if (bits & (0x80 >> col)) {
                        SDL_Rect px = { cx + col * scale, y + row * scale,
                                        scale + (bold ? 1 : 0), scale };
                        SDL_RenderFillRect(r, &px);
                    }
                }
            }
        }
        cx += adv;
    }
}

void LauncherDraw_Text(SDL_Renderer *r, int x, int y, int scale,
                       Uint8 cr, Uint8 cg, Uint8 cb, const char *s) {
    ldraw_str(r, x, y, scale, cr, cg, cb, s, 0);
}

void LauncherDraw_TextBold(SDL_Renderer *r, int x, int y, int scale,
                           Uint8 cr, Uint8 cg, Uint8 cb, const char *s) {
    ldraw_str(r, x, y, scale, cr, cg, cb, s, 1);
}

int g_ldraw_w = LDRAW_W_DECK;
int g_ldraw_h = LDRAW_H_BASE;

void LauncherDraw_SetWidth(int w) {
    if (w > 0) g_ldraw_w = w;
}

void LauncherDraw_SetSize(int w, int h) {
    if (w > 0) g_ldraw_w = w;
    if (h > 0) g_ldraw_h = h;
}

int LauncherDraw_TextWidth(int scale, const char *s) {
    return s ? (int)strlen(s) * LDRAW_ADVANCE(scale) : 0;
}

int LauncherDraw_TextWidthBold(int scale, const char *s) {
    return s ? (int)strlen(s) * (LDRAW_ADVANCE(scale) + 1) : 0;
}

static void ldraw_str_clipped(SDL_Renderer *r, int x, int y, int scale,
                              Uint8 cr, Uint8 cg, Uint8 cb,
                              const char *s, int max_w, int bold) {
    if (!s) return;
    int adv  = LDRAW_ADVANCE(scale) + (bold ? 1 : 0);
    int fits = adv > 0 ? max_w / adv : 0;
    if (fits <= 0) return;

    if ((int)strlen(s) <= fits) {
        ldraw_str(r, x, y, scale, cr, cg, cb, s, bold);
        return;
    }

    char buf[256];
    int keep = fits - 2;
    if (keep < 1) keep = 1;
    if (keep > (int)sizeof(buf) - 3) keep = (int)sizeof(buf) - 3;
    memcpy(buf, s, (size_t)keep);
    buf[keep]     = '.';
    buf[keep + 1] = '.';
    buf[keep + 2] = '\0';
    ldraw_str(r, x, y, scale, cr, cg, cb, buf, bold);
}

void LauncherDraw_TextClipped(SDL_Renderer *r, int x, int y, int scale,
                              Uint8 cr, Uint8 cg, Uint8 cb,
                              const char *s, int max_w) {
    ldraw_str_clipped(r, x, y, scale, cr, cg, cb, s, max_w, 0);
}

void LauncherDraw_TextClippedBold(SDL_Renderer *r, int x, int y, int scale,
                                  Uint8 cr, Uint8 cg, Uint8 cb,
                                  const char *s, int max_w) {
    ldraw_str_clipped(r, x, y, scale, cr, cg, cb, s, max_w, 1);
}

void LauncherDraw_Bevel(SDL_Renderer *r, SDL_Rect rect, int raised) {
    Uint8 lo = raised ? 0xFF : 0x40;
    Uint8 hi = raised ? 0x40 : 0xFF;

    SDL_SetRenderDrawColor(r, LCOL_PANEL, 0xFF);
    SDL_RenderFillRect(r, &rect);

    SDL_SetRenderDrawColor(r, lo, lo, lo, 0xFF);
    for (int i = 0; i < 2; i++) {
        SDL_RenderDrawLine(r, rect.x + i, rect.y, rect.x + i, rect.y + rect.h - 1 - i);
        SDL_RenderDrawLine(r, rect.x, rect.y + i, rect.x + rect.w - 1 - i, rect.y + i);
    }
    SDL_SetRenderDrawColor(r, hi, hi, hi, 0xFF);
    for (int i = 0; i < 2; i++) {
        SDL_RenderDrawLine(r, rect.x + rect.w - 1 - i, rect.y + i,
                           rect.x + rect.w - 1 - i, rect.y + rect.h - 1);
        SDL_RenderDrawLine(r, rect.x + i, rect.y + rect.h - 1 - i,
                           rect.x + rect.w - 1, rect.y + rect.h - 1 - i);
    }
}

void LauncherDraw_FocusBarRGB(SDL_Renderer *r, SDL_Rect rect,
                              Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 0xFF);
    SDL_RenderFillRect(r, &rect);
}

void LauncherDraw_FocusBar(SDL_Renderer *r, SDL_Rect rect) {
    SDL_SetRenderDrawColor(r, LCOL_FOCUS, 0xFF);
    SDL_RenderFillRect(r, &rect);
}

void LauncherDraw_PromptBar(SDL_Renderer *r, const char *a, const char *b,
                            const char *x, const char *y) {
    const int h = 26;
    SDL_Rect bar = { 0, LDRAW_H - h, LDRAW_W, h };
    SDL_SetRenderDrawColor(r, LCOL_DARK, 0xFF);
    SDL_RenderFillRect(r, &bar);

    struct { const char *key, *label; } slots[4] = {
        { "A", a }, { "B", b }, { "X", x }, { "Y", y }
    };
    int cx = 12;
    for (int i = 0; i < 4; i++) {
        if (!slots[i].label) continue;
        SDL_Rect cell = { cx, bar.y + 5, 16, 16 };
        LauncherDraw_Bevel(r, cell, 1);
        LauncherDraw_TextBold(r, cell.x + 5, LDRAW_TEXT_Y(cell.y, cell.h, 1), 1,
                              LCOL_TEXT, slots[i].key);
        cx += cell.w + 6;
        LauncherDraw_TextBold(r, cx, LDRAW_TEXT_Y(bar.y, h, 1), 1,
                              0xFF, 0xFF, 0xFF, slots[i].label);
        cx += LauncherDraw_TextWidthBold(1, slots[i].label) + 18;
    }
}

#define FOOTER_H     LDRAW_FOOTER_H
#define FOOTER_PAD_X 10
#define FOOTER_BTN_H 18

void LauncherDraw_FooterLayout(ldraw_footer_btn_t *btns, int n) {
    int x = LDRAW_W - 12;
    int y = LDRAW_H - FOOTER_H + (FOOTER_H - FOOTER_BTN_H) / 2;
    for (int i = 0; i < n; i++) {
        if (!btns[i].label) { btns[i].rect = (SDL_Rect){ 0, 0, 0, 0 }; continue; }
        int w = LauncherDraw_TextWidth(1, btns[i].label) + FOOTER_PAD_X * 2;
        x -= w;
        btns[i].rect = (SDL_Rect){ x, y, w, FOOTER_BTN_H };
        x -= 8;
    }
}

void LauncherDraw_FooterButtons(SDL_Renderer *r, const ldraw_footer_btn_t *btns,
                                int n, int hover) {
    SDL_Rect bar = { 0, LDRAW_H - FOOTER_H, LDRAW_W, FOOTER_H };
    SDL_SetRenderDrawColor(r, LCOL_DARK, 0xFF);
    SDL_RenderFillRect(r, &bar);

    for (int i = 0; i < n; i++) {
        if (!btns[i].label || btns[i].rect.w <= 0) continue;
        SDL_Rect b = btns[i].rect;
        LauncherDraw_Bevel(r, b, 1);
        if (i == hover) {
            SDL_Rect inner = { b.x + 2, b.y + 2, b.w - 4, b.h - 4 };
            LauncherDraw_FocusBar(r, inner);
        }
        Uint8 t = (i == hover) ? 0xFF : 0x00;
        LauncherDraw_Text(r, b.x + (b.w - LauncherDraw_TextWidth(1, btns[i].label)) / 2,
                          LDRAW_TEXT_Y(b.y, b.h, 1), 1, t, t, t,
                          btns[i].label);
    }
}

int LauncherDraw_PointInRect(int px, int py, SDL_Rect rect) {
    return px >= rect.x && px < rect.x + rect.w &&
           py >= rect.y && py < rect.y + rect.h;
}

void LauncherDraw_EnsureDir(const char *path) {
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}
