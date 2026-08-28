
#ifndef UI_DRAW_H
#define UI_DRAW_H

#include <stdint.h>

typedef struct {
    uint32_t *px;
    int       w, h;
    int       pitch;
} ui_target_t;

typedef struct { int x, y, w, h; } ui_rect_t;

#define UI_BG        0xFFC0C0C0u
#define UI_PANEL     0xFFD4D4D4u
#define UI_LIGHT     0xFFFFFFFFu
#define UI_DARK      0xFF404040u
#define UI_TEXT      0xFF000000u
#define UI_TEXT_DIM  0xFF606060u
#define UI_ERROR     0xFF800000u
#define UI_OK        0xFF006000u
#define UI_FOCUS     0xFF000080u
#define UI_FOCUS_TXT 0xFFFFFFFFu

#define UI_ADVANCE(scale) ((8 - 2) * (scale))
#define UI_LINE_H(scale)  (8 * (scale))

void UiDraw_Clear(ui_target_t *t, uint32_t argb);
void UiDraw_FillRect(ui_target_t *t, ui_rect_t r, uint32_t argb);

void UiDraw_BlendRect(ui_target_t *t, ui_rect_t r, uint32_t argb);
void UiDraw_HLine(ui_target_t *t, int x0, int x1, int y, uint32_t argb);
void UiDraw_VLine(ui_target_t *t, int x, int y0, int y1, uint32_t argb);

void UiDraw_Frame(ui_target_t *t, ui_rect_t r, uint32_t argb);

void UiDraw_Bevel(ui_target_t *t, ui_rect_t r, int raised);
void UiDraw_FocusBar(ui_target_t *t, ui_rect_t r);

void UiDraw_Text(ui_target_t *t, int x, int y, int scale, uint32_t argb,
                 const char *s);

int  UiDraw_TextClipped(ui_target_t *t, int x, int y, int scale, uint32_t argb,
                        const char *s, int max_w);
int  UiDraw_TextWidth(int scale, const char *s);

int  UiDraw_PointInRect(int px, int py, ui_rect_t r);

#endif
