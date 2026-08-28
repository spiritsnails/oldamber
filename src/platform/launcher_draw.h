#pragma once

#include <SDL.h>
#include "launcher_font.h"

#define LDRAW_W_DECK    640
#define LDRAW_W_DESKTOP 512

#define LDRAW_H_BASE    400

extern int g_ldraw_w;
extern int g_ldraw_h;
#define LDRAW_W (g_ldraw_w)
#define LDRAW_H (g_ldraw_h)

void LauncherDraw_SetWidth(int w);

void LauncherDraw_SetSize(int w, int h);

#define LCOL_BG        0xC0, 0xC0, 0xC0
#define LCOL_PANEL     0xD4, 0xD4, 0xD4
#define LCOL_LIGHT     0xFF, 0xFF, 0xFF
#define LCOL_DARK      0x40, 0x40, 0x40
#define LCOL_TEXT      0x00, 0x00, 0x00
#define LCOL_TEXT_DIM  0x60, 0x60, 0x60
#define LCOL_ERROR     0x80, 0x00, 0x00
#define LCOL_OK        0x00, 0x60, 0x00
#define LCOL_FOCUS     0x00, 0x00, 0x80
#define LCOL_FOCUS_TXT 0xFF, 0xFF, 0xFF

#define LDRAW_ADVANCE(scale) ((LAUNCHER_FONT_W - 2) * (scale))
#define LDRAW_LINE_H(scale)  (LAUNCHER_FONT_H * (scale))

#define LDRAW_INK_H(scale)   ((LAUNCHER_FONT_H - 1) * (scale))
#define LDRAW_TEXT_Y(y, h, scale) ((y) + ((h) - LDRAW_INK_H(scale)) / 2)

void LauncherDraw_Text(SDL_Renderer *r, int x, int y, int scale,
                       Uint8 cr, Uint8 cg, Uint8 cb, const char *s);
int  LauncherDraw_TextWidth(int scale, const char *s);

void LauncherDraw_TextBold(SDL_Renderer *r, int x, int y, int scale,
                           Uint8 cr, Uint8 cg, Uint8 cb, const char *s);
int  LauncherDraw_TextWidthBold(int scale, const char *s);
void LauncherDraw_TextClippedBold(SDL_Renderer *r, int x, int y, int scale,
                                  Uint8 cr, Uint8 cg, Uint8 cb,
                                  const char *s, int max_w);

void LauncherDraw_TextClipped(SDL_Renderer *r, int x, int y, int scale,
                              Uint8 cr, Uint8 cg, Uint8 cb,
                              const char *s, int max_w);

void LauncherDraw_Bevel(SDL_Renderer *r, SDL_Rect rect, int raised);

void LauncherDraw_FocusBar(SDL_Renderer *r, SDL_Rect rect);

void LauncherDraw_FocusBarRGB(SDL_Renderer *r, SDL_Rect rect,
                              Uint8 cr, Uint8 cg, Uint8 cb);

void LauncherDraw_PromptBar(SDL_Renderer *r, const char *a, const char *b,
                            const char *x, const char *y);

#define LDRAW_FOOTER_MAX 4

typedef struct {
    const char *label;
    SDL_Rect    rect;
} ldraw_footer_btn_t;

#define LDRAW_FOOTER_H 26

void LauncherDraw_FooterLayout(ldraw_footer_btn_t *btns, int n);
void LauncherDraw_FooterButtons(SDL_Renderer *r, const ldraw_footer_btn_t *btns,
                                int n, int hover);

int  LauncherDraw_PointInRect(int px, int py, SDL_Rect rect);

void LauncherDraw_EnsureDir(const char *path);
