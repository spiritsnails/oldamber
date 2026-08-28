
#include "ui_draw.h"
#include "launcher_font.h"

#include <string.h>

static int clip_rect(const ui_target_t *t, ui_rect_t *r) {
    if (!t || !t->px) return 0;
    if (r->w <= 0 || r->h <= 0) return 0;
    if (r->x < 0)          { r->w += r->x; r->x = 0; }
    if (r->y < 0)          { r->h += r->y; r->y = 0; }
    if (r->x + r->w > t->w) r->w = t->w - r->x;
    if (r->y + r->h > t->h) r->h = t->h - r->y;
    return (r->w > 0 && r->h > 0);
}

static void put_px(ui_target_t *t, int x, int y, uint32_t argb) {
    if (x < 0 || y < 0 || x >= t->w || y >= t->h) return;
    t->px[(size_t)y * (size_t)t->pitch + (size_t)x] = argb;
}

static void blend_px(ui_target_t *t, int x, int y, uint32_t argb) {
    uint32_t a, dst, dr, dg, db, sr, sg, sb;
    if (x < 0 || y < 0 || x >= t->w || y >= t->h) return;
    a = (argb >> 24) & 0xFFu;
    if (a == 0xFFu) { put_px(t, x, y, argb); return; }
    if (a == 0) return;
    dst = t->px[(size_t)y * (size_t)t->pitch + (size_t)x];
    sr = (argb >> 16) & 0xFFu; sg = (argb >> 8) & 0xFFu; sb = argb & 0xFFu;
    dr = (dst  >> 16) & 0xFFu; dg = (dst  >> 8) & 0xFFu; db = dst  & 0xFFu;
    dr = (sr * a + dr * (255u - a)) / 255u;
    dg = (sg * a + dg * (255u - a)) / 255u;
    db = (sb * a + db * (255u - a)) / 255u;
    t->px[(size_t)y * (size_t)t->pitch + (size_t)x] =
        0xFF000000u | (dr << 16) | (dg << 8) | db;
}

void UiDraw_Clear(ui_target_t *t, uint32_t argb) {
    ui_rect_t all;
    if (!t || !t->px) return;
    all.x = 0; all.y = 0; all.w = t->w; all.h = t->h;
    UiDraw_FillRect(t, all, argb);
}

void UiDraw_FillRect(ui_target_t *t, ui_rect_t r, uint32_t argb) {
    int x, y;
    if (!clip_rect(t, &r)) return;
    for (y = r.y; y < r.y + r.h; y++)
        for (x = r.x; x < r.x + r.w; x++)
            t->px[(size_t)y * (size_t)t->pitch + (size_t)x] = argb;
}

void UiDraw_BlendRect(ui_target_t *t, ui_rect_t r, uint32_t argb) {
    int x, y;
    if (!clip_rect(t, &r)) return;
    for (y = r.y; y < r.y + r.h; y++)
        for (x = r.x; x < r.x + r.w; x++)
            blend_px(t, x, y, argb);
}

void UiDraw_HLine(ui_target_t *t, int x0, int x1, int y, uint32_t argb) {
    ui_rect_t r;
    if (x1 < x0) { int s = x0; x0 = x1; x1 = s; }
    r.x = x0; r.y = y; r.w = x1 - x0 + 1; r.h = 1;
    UiDraw_FillRect(t, r, argb);
}

void UiDraw_VLine(ui_target_t *t, int x, int y0, int y1, uint32_t argb) {
    ui_rect_t r;
    if (y1 < y0) { int s = y0; y0 = y1; y1 = s; }
    r.x = x; r.y = y0; r.w = 1; r.h = y1 - y0 + 1;
    UiDraw_FillRect(t, r, argb);
}

void UiDraw_Frame(ui_target_t *t, ui_rect_t r, uint32_t argb) {
    if (r.w <= 0 || r.h <= 0) return;
    UiDraw_HLine(t, r.x, r.x + r.w - 1, r.y, argb);
    UiDraw_HLine(t, r.x, r.x + r.w - 1, r.y + r.h - 1, argb);
    UiDraw_VLine(t, r.x, r.y, r.y + r.h - 1, argb);
    UiDraw_VLine(t, r.x + r.w - 1, r.y, r.y + r.h - 1, argb);
}

void UiDraw_Bevel(ui_target_t *t, ui_rect_t r, int raised) {
    const uint32_t lo = raised ? UI_DARK  : UI_LIGHT;
    const uint32_t hi = raised ? UI_LIGHT : UI_DARK;
    int i;
    UiDraw_FillRect(t, r, UI_PANEL);
    for (i = 0; i < 2; i++) {
        UiDraw_VLine(t, r.x + i, r.y, r.y + r.h - 1 - i, lo);
        UiDraw_HLine(t, r.x, r.x + r.w - 1 - i, r.y + i, lo);
    }
    for (i = 0; i < 2; i++) {
        UiDraw_VLine(t, r.x + r.w - 1 - i, r.y + i, r.y + r.h - 1, hi);
        UiDraw_HLine(t, r.x + i, r.x + r.w - 1, r.y + r.h - 1 - i, hi);
    }
}

void UiDraw_FocusBar(ui_target_t *t, ui_rect_t r) {
    UiDraw_FillRect(t, r, UI_FOCUS);
}

int UiDraw_TextWidth(int scale, const char *s) {
    return s ? (int)strlen(s) * UI_ADVANCE(scale) : 0;
}

static void draw_glyph(ui_target_t *t, const uint8_t *g, int x, int y,
                       int scale, uint32_t argb) {
    int row, col, sy, sx;
    for (row = 0; row < 8; row++) {
        const uint8_t bits = g[row];
        for (col = 0; col < 8; col++) {
            if (!(bits & (0x80 >> col))) continue;
            for (sy = 0; sy < scale; sy++)
                for (sx = 0; sx < scale; sx++)
                    put_px(t, x + col * scale + sx, y + row * scale + sy, argb);
        }
    }
}

void UiDraw_Text(ui_target_t *t, int x, int y, int scale, uint32_t argb,
                 const char *s) {
    int cx = x;
    const char *p;
    if (!t || !t->px || !s) return;
    for (p = s; *p; p++) {
        if (*p == '\n') { cx = x; y += UI_LINE_H(scale) + scale * 2; continue; }
        {
            const uint8_t *g = LauncherFont_Glyph(*p);
            if (g) draw_glyph(t, g, cx, y, scale, argb);
        }
        cx += UI_ADVANCE(scale);
    }
}

int UiDraw_TextClipped(ui_target_t *t, int x, int y, int scale, uint32_t argb,
                       const char *s, int max_w) {
    const int adv = UI_ADVANCE(scale);
    int cx = x;
    const char *p;
    if (!t || !t->px || !s || max_w <= 0) return 0;

    for (p = s; *p; p++) {
        const uint8_t *g;
        if (cx + adv - x > max_w) break;
        g = LauncherFont_Glyph(*p);
        if (g) draw_glyph(t, g, cx, y, scale, argb);
        cx += adv;
    }
    return cx - x;
}

int UiDraw_PointInRect(int px, int py, ui_rect_t r) {
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}
