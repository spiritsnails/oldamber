
#include "battle_transition.h"
#include "speed_settings.h"
#include "constants.h"
#include "../platform/display.h"
#include "../platform/hardware.h"
#include "overworld.h"
#include "player.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#define MAP_VIRIDIAN_FOREST       0x33
#define MAP_ROCK_TUNNEL_1F        0x52
#define MAP_ROCK_TUNNEL_B1F       0xE8
#define MAP_SEAFOAM_ISLANDS_1F    0xC0
#define MAP_MT_MOON_1F            0x3B
#define MAP_MT_MOON_B2F           0x3D
#define MAP_SS_ANNE_1F            0x5F
#define MAP_HALL_OF_FAME          0x76
#define MAP_LAVENDER_POKECENTER   0x8D
#define MAP_LAVENDER_CUBONE_HOUSE 0x97
#define MAP_SILPH_CO_2F           0xCF
#define MAP_CERULEAN_CAVE_1F      0xE4

static int is_dungeon_map(void) {

    int real_id = Map_CurrentRealId();
    uint8_t m;
    if (real_id < 0) return 0;
    m = (uint8_t)real_id;

    if (m == MAP_VIRIDIAN_FOREST)    return 1;
    if (m == MAP_ROCK_TUNNEL_1F)     return 1;
    if (m == MAP_SEAFOAM_ISLANDS_1F) return 1;
    if (m == MAP_ROCK_TUNNEL_B1F)    return 1;

    if (m >= MAP_MT_MOON_1F          && m <= MAP_MT_MOON_B2F)           return 1;
    if (m >= MAP_SS_ANNE_1F          && m <= MAP_HALL_OF_FAME)           return 1;
    if (m >= MAP_LAVENDER_POKECENTER && m <= MAP_LAVENDER_CUBONE_HOUSE)  return 1;
    if (m >= MAP_SILPH_CO_2F         && m <= MAP_CERULEAN_CAVE_1F)       return 1;
    return 0;
}

typedef enum {
    BPHASE_IDLE = 0,
    BPHASE_FLASH,
    BPHASE_ANIM,
    BPHASE_BLACK_HOLD,
    BPHASE_DONE,
} BPhase;

#define BTRANS_BLACK_HOLD_FRAMES 60

static BPhase g_phase       = BPHASE_IDLE;
static int    g_type        = 0;
static int    g_step        = 0;
static int    g_frame       = 0;
static int    g_black_hold  = 0;

static int    g_flash_idx   = 0;
static int    g_flash_cycle = 0;

static int    g_sp_tx, g_sp_ty;
static int    g_sp_dir;
static int    g_sp_total;

#define ISP_MAX 400
static int8_t g_isp_x[ISP_MAX];
static int8_t g_isp_y[ISP_MAX];
static int    g_isp_len;
static int    g_isp_idx;

static const uint8_t kBlackTile[16] = {
    0xFF,0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF,0xFF,
    0xFF,0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFF,0xFF,
};

static const uint8_t kFlashPals[12] = {
    0x6F,
    0xBF,
    0xFF,
    0xBF,
    0x6F,
    0x1B,
    0x06,
    0x01,
    0x00,
    0x01,
    0x06,
    0x1B,
};

static const int8_t kCData1[] = { 2,  3,  5,  4,  9, -1 };
static const int8_t kCData2[] = { 1,  1,  2,  2,  4,  2,  4,  2,  3, -1 };
static const int8_t kCData3[] = { 2,  1,  3,  1,  4,  1,  4,  1,  4,  1,
                                   3,  1,  2,  1,  1,  1,  1, -1 };
static const int8_t kCData4[] = { 4,  1,  4,  0,  3,  1,  3,  0,  2,  1,
                                   2,  0,  1, -1 };
static const int8_t kCData5[] = { 4,  0,  3,  0,  3,  0,  2,  0,  2,  0,
                                   1,  0,  1,  0,  1, -1 };

typedef struct { int qx; const int8_t *data; int tx, ty; } HC;

static const HC kHC1[10] = {
    { 1, kCData1, 18,  6 },
    { 1, kCData2, 19,  3 },
    { 1, kCData3, 18,  0 },
    { 1, kCData4, 14,  0 },
    { 1, kCData5, 10,  0 },
    { 0, kCData5,  9,  0 },
    { 0, kCData4,  5,  0 },
    { 0, kCData3,  1,  0 },
    { 0, kCData2,  0,  3 },
    { 0, kCData1,  1,  6 },
};

static const HC kHC2[10] = {
    { 0, kCData1,  1, 11 },
    { 0, kCData2,  0, 14 },
    { 0, kCData3,  1, 17 },
    { 0, kCData4,  5, 17 },
    { 0, kCData5,  9, 17 },
    { 1, kCData5, 10, 17 },
    { 1, kCData4, 14, 17 },
    { 1, kCData3, 18, 17 },
    { 1, kCData2, 19, 14 },
    { 1, kCData1, 18, 11 },
};

static int bt_phase_x(void) { return Display_ContentOriginX() % TILE_PX; }
static int bt_col0(void) { return bt_phase_x() ? -1 : 0; }
static int bt_cols(void) { return Map_ViewTilesW() + (bt_phase_x() ? 1 : 0); }
static int bt_rows(void) { return SCREEN_HEIGHT; }
static int bt_is_wide(void) { return bt_cols() != SCREEN_WIDTH; }

static void set_stile(int tx, int ty, uint8_t tile) {
    if ((unsigned)tx >= (unsigned)bt_cols() || (unsigned)ty >= (unsigned)bt_rows()) return;
    gScrollTileMap[(ty + 2) * SCROLL_MAP_W + (tx + bt_col0() + 2)] = tile;
}

static uint8_t get_stile(int tx, int ty) {
    if ((unsigned)tx >= (unsigned)bt_cols() || (unsigned)ty >= (unsigned)bt_rows()) return 0xFF;
    return gScrollTileMap[(ty + 2) * SCROLL_MAP_W + (tx + bt_col0() + 2)];
}

#define BT_ORDER_MAX (SCREEN_WIDTH_MAX * (SCREEN_HEIGHT + 1))
static uint8_t g_wo_x[BT_ORDER_MAX], g_wo_y[BT_ORDER_MAX];
static int g_wo_len, g_wo_idx, g_wo_budget, g_wo_acc;

static void wo_push(int x, int y) {
    if (g_wo_len < BT_ORDER_MAX) {
        g_wo_x[g_wo_len] = (uint8_t)x;
        g_wo_y[g_wo_len] = (uint8_t)y;
        g_wo_len++;
    }
}

static void wo_spiral_in(int cols, int rows) {
    int x0 = 0, y0 = 0, x1 = cols - 1, y1 = rows - 1;
    while (x0 <= x1 && y0 <= y1) {
        for (int y = y0; y <= y1; y++) wo_push(x0, y);
        if (++x0 > x1) break;
        for (int x = x0; x <= x1; x++) wo_push(x, y1);
        if (--y1 < y0) break;
        for (int y = y1; y >= y0; y--) wo_push(x1, y);
        if (--x1 < x0) break;
        for (int x = x1; x >= x0; x--) wo_push(x, y0);
        y0++;
    }
}

static void wo_spiral_out(int cols, int rows) {
    wo_spiral_in(cols, rows);
    for (int i = 0, j = g_wo_len - 1; i < j; i++, j--) {
        uint8_t tx = g_wo_x[i], ty = g_wo_y[i];
        g_wo_x[i] = g_wo_x[j]; g_wo_y[i] = g_wo_y[j];
        g_wo_x[j] = tx;        g_wo_y[j] = ty;
    }
}

typedef struct { uint8_t x, y; float a; } bt_sweep_t;
static int bt_sweep_cmp(const void *p, const void *q) {
    float d = ((const bt_sweep_t *)p)->a - ((const bt_sweep_t *)q)->a;
    return (d < 0.0f) ? -1 : (d > 0.0f) ? 1 : 0;
}
static void wo_sweep(int arms, int cols, int rows) {
    static bt_sweep_t t[BT_ORDER_MAX];
    int n = 0;
    float cx = cols * 0.5f, cy = rows * 0.5f;
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols && n < BT_ORDER_MAX; x++) {
            float a = atan2f(cy - (y + 0.5f), (x + 0.5f) - cx);
            if (a < 0.0f) a += 6.28318530718f;
            if (arms == 2) a = fmodf(a, 3.14159265359f);
            t[n].x = (uint8_t)x; t[n].y = (uint8_t)y; t[n].a = a; n++;
        }
    }
    qsort(t, (size_t)n, sizeof t[0], bt_sweep_cmp);
    for (int i = 0; i < n; i++) wo_push(t[i].x, t[i].y);
}

static void wo_hstripes(int cols, int rows) {
    for (int s = 0; s < cols; s++) {
        for (int row = 0; row < rows; row += 2) wo_push(s, row);
        for (int row = 1; row < rows; row += 2) wo_push(cols - 1 - s, row);
    }
}
static void wo_vstripes(int cols, int rows) {
    for (int s = 0; s < rows; s++) {
        for (int col = 0; col < cols; col += 2) wo_push(col, s);
        for (int col = 1; col < cols; col += 2) wo_push(col, rows - 1 - s);
    }
}

static int wo_budget_for(int type) {
    switch (type) {
        case 0: return 30;
        case 1: return 139;
        case 2: return 60;
        case 3: return 120;
        case 4: return 60;
        case 6: return 54;
        default: return 60;
    }
}

static int wo_begin(int type) {
    int cols = bt_cols(), rows = bt_rows();
    g_wo_len = 0; g_wo_idx = 0; g_wo_acc = 0;
    switch (type) {
        case 0: wo_sweep(2, cols, rows);      break;
        case 1: wo_spiral_in(cols, rows);     break;
        case 2: wo_sweep(1, cols, rows);      break;
        case 3: wo_spiral_out(cols, rows);    break;
        case 4: wo_hstripes(cols, rows);      break;
        case 6: wo_vstripes(cols, rows);      break;
        default: return 0;
    }
    g_wo_budget = wo_budget_for(type);
    if (g_wo_budget < 1) g_wo_budget = 1;
    return 1;
}

static int wo_tick(void) {
    g_wo_acc += g_wo_len;
    int n = g_wo_acc / g_wo_budget;
    g_wo_acc -= n * g_wo_budget;
    while (n-- > 0 && g_wo_idx < g_wo_len) {
        set_stile(g_wo_x[g_wo_idx], g_wo_y[g_wo_idx], 0xFF);
        g_wo_idx++;
    }
    return (g_wo_idx >= g_wo_len);
}

static void circle_sub3(const int8_t *data, int tx, int ty, int qx, int qy) {
    while (1) {

        int8_t raw = *data++;
        if (raw == -1) break;
        int count = (uint8_t)raw;

        int cx = tx;
        for (int i = 0; i < count; i++) {
            set_stile(cx, ty, 0xFF);
            if (qx) cx++; else cx--;
        }

        if (qy) ty--; else ty++;

        int8_t off = *data++;
        if (off == -1) break;
        if (off == 0)  continue;

        if (qx) tx -= off; else tx += off;
    }
}

static void apply_hc(const HC *e, int qy) {
    circle_sub3(e->data, e->tx, e->ty, e->qx, qy);
}

static void outward_substep(void) {
    static const int8_t check_dx[4] = { -1, 0, 1, 0 };
    static const int8_t check_dy[4] = {  0, 1, 0,-1 };
    static const int8_t move_dx [4] = {  0,-1, 0, 1 };
    static const int8_t move_dy [4] = { -1, 0, 1, 0 };

    int cx = g_sp_tx + check_dx[g_sp_dir];
    int cy = g_sp_ty + check_dy[g_sp_dir];

    int empty = (get_stile(cx, cy) != 0xFF);
    if (empty) {

        set_stile(cx, cy, 0xFF);
        g_sp_tx = cx;
        g_sp_ty = cy;
        g_sp_dir = (g_sp_dir + 1) & 3;
    } else {

        int nx = g_sp_tx + move_dx[g_sp_dir];
        int ny = g_sp_ty + move_dy[g_sp_dir];
        set_stile(nx, ny, 0xFF);
        g_sp_tx = nx;
        g_sp_ty = ny;
    }
}

static void isp_precompute(void) {
    int tx = 0, ty = 0;
    int n = 0;
    int c = SCREEN_HEIGHT - 1;

    for (int i = 0; i < c && n < ISP_MAX; i++) {
        g_isp_x[n] = (int8_t)tx; g_isp_y[n++] = (int8_t)ty; ty++;
    }
    c += 2;

    while (c > 0 && n < ISP_MAX) {

        for (int i = 0; i < c && n < ISP_MAX; i++) {
            g_isp_x[n] = (int8_t)tx; g_isp_y[n++] = (int8_t)ty; tx++;
        }
        c -= 2;
        if (c <= 0) break;

        for (int i = 0; i < c && n < ISP_MAX; i++) {
            g_isp_x[n] = (int8_t)tx; g_isp_y[n++] = (int8_t)ty; ty--;
        }
        c++;

        for (int i = 0; i < c && n < ISP_MAX; i++) {
            g_isp_x[n] = (int8_t)tx; g_isp_y[n++] = (int8_t)ty; tx--;
        }
        c -= 2;
        if (c <= 0) break;

        for (int i = 0; i < c && n < ISP_MAX; i++) {
            g_isp_x[n] = (int8_t)tx; g_isp_y[n++] = (int8_t)ty; ty++;
        }
        c++;
    }

    g_isp_len = n;
    g_isp_idx = 0;
}

#define ZOOM_STEP_NUM 17
#define ZOOM_STEP_DEN 16

#define ZOOM_PREPAUSE_FRAMES 30

#define ZOOM_BLACK_HOLD_FRAMES 90

static int g_zoom_mode = 0;
static int g_zoom_on   = 0;
static int g_zoom_scale = 1;
static int g_zoom_pre   = 0;

void BattleTransition_SetZoomMode(int on) { g_zoom_mode = on ? 1 : 0; }
int  BattleTransition_GetZoomMode(void)   { return g_zoom_mode; }

static void zoom_pick_focus(int *fx, int *fy) {
    int px = (int)wShadowOAM[0].x - OAM_X_OFS + bt_phase_x();
    int py = (int)wShadowOAM[0].y - OAM_Y_OFS;
    if (!Display_FindDarkestPixel(px, py, 16, 16, fx, fy)) {

        *fx = Display_FrameWidth() / 2;
        *fy = SCREEN_HEIGHT_PX / 2;
    }
}

static int tick_zoom(void) {

    if (g_zoom_pre > 0) { g_zoom_pre--; return 0; }

    int cur  = g_zoom_scale;
    int next = cur * ZOOM_STEP_NUM / ZOOM_STEP_DEN;
    if (next <= cur) next = cur + 1;
    g_zoom_scale = next;
    Display_ZoomSetScale(g_zoom_scale * 256);
    return (g_zoom_scale >= 2 * Display_FrameWidth());
}

static int tick_flash(void) {

    Display_SetBGP(kFlashPals[g_flash_idx]);
    if (++g_frame >= 2) {
        g_frame = 0;
        if (++g_flash_idx >= 12) {
            g_flash_idx = 0;
            if (++g_flash_cycle >= 3) {

                g_phase = BPHASE_ANIM;
                g_step  = 0;
                g_frame = 0;

                Display_SetPalette(0xE4, 0xD0, 0xE0);
            }
        }
    }
    return 0;
}

static int tick_double_circle(void) {
    if (g_frame == 0) {
        apply_hc(&kHC1[g_step], 0);
        apply_hc(&kHC2[g_step], 1);
    }
    if (++g_frame >= 3) { g_frame = 0; g_step++; }
    return (g_step >= 10);
}

static int tick_circle(void) {
    if (g_frame == 0) {
        if (g_step < 10)
            apply_hc(&kHC1[g_step],     0);
        else
            apply_hc(&kHC2[g_step - 10], 1);
    }
    if (++g_frame >= 3) { g_frame = 0; g_step++; }
    return (g_step >= 20);
}

static int tick_hstripes(void) {
    if (g_frame == 0) {
        int s = g_step;
        for (int row = 0; row < SCREEN_HEIGHT; row += 2)
            set_stile(s, row, 0xFF);
        for (int row = 1; row < SCREEN_HEIGHT; row += 2)
            set_stile(SCREEN_WIDTH - 1 - s, row, 0xFF);
    }
    if (++g_frame >= 3) { g_frame = 0; g_step++; }
    return (g_step >= SCREEN_WIDTH);
}

static int tick_vstripes(void) {
    if (g_frame == 0) {
        int s = g_step;
        for (int col = 0; col < SCREEN_WIDTH; col += 2)
            set_stile(col, s, 0xFF);
        for (int col = 1; col < SCREEN_WIDTH; col += 2)
            set_stile(col, SCREEN_HEIGHT - 1 - s, 0xFF);
    }
    if (++g_frame >= 3) { g_frame = 0; g_step++; }
    return (g_step >= SCREEN_HEIGHT);
}

static int tick_spiral_out(void) {

    for (int i = 0; i < 3; i++) {
        outward_substep();
        if (++g_sp_total >= 360) return 1;
    }
    return 0;
}

static int tick_spiral_in(void) {

    if (g_frame > 0) { g_frame--; return 0; }
    for (int i = 0; i < 7; i++) {
        if (g_isp_idx >= g_isp_len) return 1;
        set_stile(g_isp_x[g_isp_idx], g_isp_y[g_isp_idx], 0xFF);
        g_isp_idx++;
    }
    g_frame = 2;
    return (g_isp_idx >= g_isp_len);
}

static void shift_rows_down(int blank_row, int to_row) {
    for (int y = to_row; y > blank_row; y--)
        for (int x = 0; x < bt_cols(); x++) set_stile(x, y, get_stile(x, y - 1));
    for (int x = 0; x < bt_cols(); x++) set_stile(x, blank_row, 0xFF);
}
static void shift_rows_up(int blank_row, int to_row) {
    for (int y = to_row; y < blank_row; y++)
        for (int x = 0; x < bt_cols(); x++) set_stile(x, y, get_stile(x, y + 1));
    for (int x = 0; x < bt_cols(); x++) set_stile(x, blank_row, 0xFF);
}
static void shift_cols_right(int blank_col, int to_col) {
    for (int x = to_col; x > blank_col; x--)
        for (int y = 0; y < bt_rows(); y++) set_stile(x, y, get_stile(x - 1, y));
    for (int y = 0; y < bt_rows(); y++) set_stile(blank_col, y, 0xFF);
}
static void shift_cols_left(int blank_col, int to_col) {
    for (int x = to_col; x < blank_col; x++)
        for (int y = 0; y < bt_rows(); y++) set_stile(x, y, get_stile(x + 1, y));
    for (int y = 0; y < bt_rows(); y++) set_stile(blank_col, y, 0xFF);
}

static int bt_col_shifts(void) {
    int steps = bt_rows() / 2;
    if (steps < 1) steps = 1;
    return (bt_cols() / 2 + steps - 1) / steps;
}

static int tick_shrink(void) {
    if (g_frame == 0) {
        shift_rows_down(0, bt_rows() / 2 - 1);
        shift_rows_up(bt_rows() - 1, bt_rows() / 2);
        for (int i = 0, k = bt_col_shifts(); i < k; i++) {
            shift_cols_right(0, bt_cols() / 2 - 1);
            shift_cols_left(bt_cols() - 1, bt_cols() / 2);
        }
    }
    if (++g_frame >= 6) { g_frame = 0; g_step++; }
    return (g_step >= bt_rows() / 2);
}

static int tick_split(void) {
    if (g_frame == 0) {
        shift_rows_down(0, bt_rows() - 1);
        shift_rows_up(bt_rows() - 1, 0);
        for (int i = 0, k = bt_col_shifts(); i < k; i++) {
            shift_cols_right(0, bt_cols() - 1);
            shift_cols_left(bt_cols() - 1, 0);
        }
    }
    if (++g_frame >= 6) { g_frame = 0; g_step++; }
    return (g_step >= SCREEN_HEIGHT / 2);
}

void BattleTransition_Start(int is_trainer, int enemy_level, int player_level) {
    int stronger = (enemy_level >= player_level + 3);
    int dungeon  = is_dungeon_map();
    int type     = (is_trainer ? 1 : 0)
                 | (stronger   ? 2 : 0)
                 | (dungeon    ? 4 : 0);

    g_type        = type;
    g_step        = 0;
    g_frame       = 0;
    g_flash_idx   = 0;
    g_flash_cycle = 0;
    g_black_hold  = 0;

    g_wo_len = 0;
    if (bt_is_wide()) wo_begin(type);

    Display_ZoomEnd();

    Display_LoadTile(0xFF, kBlackTile);

    gScrollPxX = 0;
    gScrollPxY = 0;

    g_zoom_on = g_zoom_mode;
    if (g_zoom_on) {
        int fx, fy;
        zoom_pick_focus(&fx, &fy);
        g_zoom_scale = 1;
        g_zoom_pre   = ZOOM_PREPAUSE_FRAMES;
        Display_ZoomBegin(fx, fy);
        g_phase = BPHASE_ANIM;
        return;
    }

    switch (type) {
        case 1:
            isp_precompute();
            g_phase = BPHASE_ANIM;
            break;
        case 3:
            g_sp_tx    = 10;
            g_sp_ty    = 10;
            g_sp_dir   = 3;
            g_sp_total = 0;
            g_phase = BPHASE_ANIM;
            break;
        case 0:
        case 2:

            g_phase = BPHASE_FLASH;
            break;
        default:

            g_phase = BPHASE_ANIM;
            break;
    }
}

static int transition_tick_once(void);

int BattleTransition_Tick(void) {
    int steps = SpeedSettings_Transition();
    if (steps == SPEED_UNCAPPED) {

        for (int guard = 0; guard < 4096; guard++)
            if (transition_tick_once()) return 1;
        return 1;
    }
    for (int i = 0; i < steps; i++)
        if (transition_tick_once()) return 1;
    return 0;
}

static int transition_tick_once(void) {
    if (g_phase == BPHASE_IDLE || g_phase == BPHASE_DONE) return 1;

    if (g_phase == BPHASE_FLASH) {
        tick_flash();
        return 0;
    }

    if (g_phase == BPHASE_BLACK_HOLD) {
        if (--g_black_hold > 0) return 0;
        g_phase = BPHASE_DONE;
        return 1;
    }

    int done = 0;
    if (g_zoom_on) {
        done = tick_zoom();
        if (done) {

            Display_ZoomEnd();
        }
    } else if (bt_is_wide() && g_wo_len > 0) {

        done = wo_tick();
    } else switch (g_type) {
        case 0: done = tick_double_circle(); break;
        case 1: done = tick_spiral_in();     break;
        case 2: done = tick_circle();        break;
        case 3: done = tick_spiral_out();    break;
        case 4: done = tick_hstripes();      break;
        case 5: done = tick_shrink();        break;
        case 6: done = tick_vstripes();      break;
        case 7: done = tick_split();         break;
        default: done = 1; break;
    }

    if (done) {

        Display_SetPalette(0xFF, 0xFF, 0xFF);

        g_phase = BPHASE_BLACK_HOLD;
        g_black_hold = g_zoom_on ? ZOOM_BLACK_HOLD_FRAMES
                                 : BTRANS_BLACK_HOLD_FRAMES;
        return 0;
    }
    return 0;
}

int BattleTransition_IsActive(void) {
    return (g_phase != BPHASE_IDLE && g_phase != BPHASE_DONE);
}
