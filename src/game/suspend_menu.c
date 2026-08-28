
#include <SDL.h>
#include <string.h>
#include <stdio.h>

#include "suspend_menu.h"
#include "../platform/display.h"
#include "../platform/launcher_draw.h"
#include "../platform/launcher_nav.h"
#include "presentation_menu.h"
#include "../platform/input.h"
#include "../platform/game_version.h"
#include "save_slots.h"
#include "../platform/display_gl.h"

static SDL_Surface  *s_surf;
static SDL_Renderer *s_soft;
static int           s_surf_w, s_surf_h;

static int           s_scale = 1;

static SDL_Rect      s_slot;

static char          s_saved_filter[64];

static int sm_ensure_surface(int w, int h) {
    if (s_surf && s_surf_w == w && s_surf_h == h) return 1;
    if (s_soft) { SDL_DestroyRenderer(s_soft); s_soft = NULL; }
    if (s_surf) { SDL_FreeSurface(s_surf);     s_surf = NULL; }
    s_surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!s_surf) return 0;
    s_soft = SDL_CreateSoftwareRenderer(s_surf);
    if (!s_soft) { SDL_FreeSurface(s_surf); s_surf = NULL; return 0; }
    s_surf_w = w; s_surf_h = h;
    return 1;
}

static int             s_open;
static launcher_nav_t  s_nav;
static int             s_nav_ready;
static int             s_focus;
static unsigned        s_intents;

static int             s_close_armed;

static int             s_dock;

static int             s_closing;
#define SM_DOCK_FRAMES 12

#define SM_MIN_W 480
#define SM_MIN_H 360

typedef struct {
    const char *label;
    const char *detail;
    int         page;
} sm_row_t;

#define SM_PAGE_HUB    (-1)
#define SM_PAGE_STATES (-2)
#define SM_PAGE_EXIT   (-3)
#define SM_PAGE_CONTROLS (-4)

static const sm_row_t kRows[] = {
    { "RESUME",   "Return to the game",  SM_PAGE_HUB },
    { "GRAPHICS", "Colour, curve, sprites, HUD",      1 },
    { "SPEED",    "Overworld and battle pacing",      2 },
    { "AUDIO",    "Volume and cries",                 3 },
    { "DISPLAY",  "Renderer, size, filter",           4 },
    { "PALETTE",  "Colour palettes",                  5 },
    { "GAMEPLAY", "Rules and conveniences",           6 },
    { "CONTROLS", "Keyboard and gamepad", SM_PAGE_CONTROLS },
    { "SAVE STATE", "Suspend and restore",  SM_PAGE_STATES },
    { "EXIT TO LAUNCHER", "Pick a game or a ROM", SM_PAGE_EXIT },
};
#define SM_ROWS ((int)(sizeof kRows / sizeof kRows[0]))

static int s_page = SM_PAGE_HUB;

static int  s_btn_focus;

static int  s_exit_requested;

enum { SM_CAP_KEY, SM_CAP_PAD };
static int  s_cap_row  = -1;
static int  s_cap_kind = SM_CAP_KEY;

static int  s_cap_pad_clear;

static int  s_cap_settle_key;
static int  s_cap_settle_pad;

static const int kBindOrder[INPUT_BIND_COUNT] = { 6, 7, 5, 4, 0, 1, 3, 2 };

enum { SM_ASK_OVERWRITE = 0, SM_ASK_EXIT, SM_ASK_DELETE, SM_ASK_WIDESCREEN };
static int  s_confirm_kind;
static int  s_confirm_slot = -1;
static int  s_confirm_yes;
static int  s_state_msg_slot = -1;
static char s_state_msg[48];

static int s_dd_row  = -1;
static int s_dd_focus;

#define SM_DD_ITEM_H SM_SCALE(28)

static int s_dd_swallow_release;

static int s_row_scroll;

static int s_wheel;

static int sm_rows_now(void) {
    if (s_page == SM_PAGE_CONTROLS) return INPUT_BIND_COUNT;
    if (s_page == SM_PAGE_STATES) return SAVE_SLOT_COUNT;
    return (s_page == SM_PAGE_HUB) ? SM_ROWS : PresentationMenu_PageRowCount(s_page);
}

#define SM_SCALE(v)   ((int)((v) * LDRAW_H / LDRAW_H_BASE))

#define SM_COMPACT    (LDRAW_H < 320)
#define SM_FLOOR(v,f) ((v) < (f) ? (f) : (v))

#define SM_TXT        (LDRAW_H >= 340 ? 2 : 1)

#define SM_TXT_LABEL  (SM_TXT > 1 ? SM_TXT - 1 : 1)

#define SM_PAD        SM_SCALE(24)
#define SM_TITLE_Y    SM_SCALE(22)
#define SM_LIST_TOP   SM_SCALE(66)
#define SM_ROW_H      SM_SCALE(34)

#define SM_STATE_BTN_H SM_FLOOR(SM_SCALE(18), LDRAW_LINE_H(1) + 8)

#define SM_STATE_ROW_H (SM_COMPACT ? SM_FLOOR(SM_SCALE(34), SM_STATE_BTN_H + 6)                                    : SM_FLOOR(SM_SCALE(44), SM_STATE_BTN_H + 6))
#define SM_ROW_TEXT   SM_SCALE(9)

static SDL_Rect sm_row_rect(int i);

static int sm_row_w(void);
static int sm_bind_label_w(void);

#define SM_BIND_KEY 0
#define SM_BIND_PAD 1

static void sm_bind_cols(int *kx, int *px, int *w) {
    const int gap = 6;
    int rw = sm_row_w();
    int avail = rw - sm_bind_label_w() - 14;
    int ww = (avail - gap) / 2;
    if (ww < SM_SCALE(46)) ww = SM_SCALE(46);
    *w  = ww;
    *kx = SM_PAD + rw - 4 - ww * 2 - gap;
    *px = *kx + ww + gap;
}

static int sm_bind_buttons(int i, SDL_Rect *out, char store[2][24]) {
    SDL_Rect rr = sm_row_rect(i);
    int bit, w, kx, px;
    if (rr.h == 0) return 0;
    bit = kBindOrder[i];

    sm_bind_cols(&kx, &px, &w);
    out[SM_BIND_KEY] = (SDL_Rect){ kx, rr.y + 4, w, rr.h - 8 };
    out[SM_BIND_PAD] = (SDL_Rect){ px, rr.y + 4, w, rr.h - 8 };

    if (s_cap_row == i && s_cap_kind == SM_CAP_KEY)
        snprintf(store[SM_BIND_KEY], 24, "PRESS...");
    else
        snprintf(store[SM_BIND_KEY], 24, "%s", Input_KeyLabel(Input_GetKey(bit)));

    if (s_cap_row == i && s_cap_kind == SM_CAP_PAD)
        snprintf(store[SM_BIND_PAD], 24, "PRESS...");
    else
        snprintf(store[SM_BIND_PAD], 24, "%s", Input_PadLabel(Input_GetPad(bit)));
    return 2;
}

static int sm_slot_buttons(int i, SDL_Rect *out, const char **labels) {
    const save_slot_info_t *in = SaveSlots_Info(i);
    SDL_Rect rr = sm_row_rect(i);
    int n = 0, w, x;
    if (rr.h == 0) return 0;

    if (!in || !in->occupied) {
        labels[n++] = "SAVE";
    } else if (!in->readable) {

        labels[n++] = "OVERWRITE";
        labels[n++] = "DELETE";
    } else {
        labels[n++] = "LOAD";
        labels[n++] = "OVERWRITE";
        labels[n++] = "DELETE";
    }

    w = LauncherDraw_TextWidth(1, "OVERWRITE") + 14;

    {
        int room = rr.w - 12 - (n - 1) * 5;
        if (w * n > room) {
            w = room / n;
            if (w < LauncherDraw_TextWidth(1, "DELETE") + 8)
                w = LauncherDraw_TextWidth(1, "DELETE") + 8;
        }
    }
    x = rr.x + rr.w - 6;
    for (int k = n - 1; k >= 0; k--) {
        out[k].w = w;
        out[k].h = SM_STATE_BTN_H;
        out[k].x = x - w;

        out[k].y = SM_COMPACT ? rr.y + (rr.h - SM_STATE_BTN_H) / 2
                              : rr.y + rr.h - SM_STATE_BTN_H - 5;
        x -= w + 5;
    }
    return n;
}

static int sm_panel_w(void);
static int sm_row_w(void);

enum { SM_FOOT_BACK = 0, SM_FOOT_OPEN, SM_FOOT_DEFAULTS };
#define SM_FOOT_MAX 2
static int sm_footer_layout(ldraw_footer_btn_t *b, char store[SM_FOOT_MAX][16],
                            int *ids);

static int s_confirm_fresh;

static int s_notice_tick;

static void sm_check_widescreen_notice(void) {
    if (!PresentationMenu_TakeWidescreenNotice()) return;
    s_confirm_kind  = SM_ASK_WIDESCREEN;
    s_confirm_slot  = 0;
    s_confirm_yes   = 0;
    s_notice_tick   = 0;
    s_confirm_fresh = 1;
}

static SDL_Rect sm_confirm_box(void) {
    SDL_Rect b;
    b.w = SM_COMPACT ? LDRAW_W - SM_PAD * 2 : 260;
    b.h = SM_COMPACT ? SM_FLOOR(SM_SCALE(92), 56) : 92;

    if (s_confirm_kind == SM_ASK_WIDESCREEN) {
        b.w = SM_COMPACT ? LDRAW_W - SM_PAD * 2 : 300;
        b.h = SM_COMPACT ? SM_FLOOR(SM_SCALE(146), 96) : 146;
    }
    b.x = sm_panel_w() / 2 - b.w / 2;
    if (b.x < SM_PAD) b.x = SM_PAD;
    b.y = LDRAW_H / 2 - b.h / 2;
    return b;
}

static void sm_confirm_buttons(const SDL_Rect *box, SDL_Rect *yes, SDL_Rect *no) {
    int bw = (box->w - 3 * 12) / 2;

    int bh = (s_confirm_kind == SM_ASK_WIDESCREEN)
                ? SM_FLOOR(SM_SCALE(34), 26)
                : SM_FLOOR(SM_SCALE(24), 15);
    yes->x = box->x + 12;           yes->y = box->y + box->h - bh - 8;
    yes->w = bw;                    yes->h = bh;
    no->x  = box->x + box->w - bw - 12;  no->y = yes->y;
    no->w  = bw;                    no->h = bh;
}

static int sm_footer_layout(ldraw_footer_btn_t *b, char store[SM_FOOT_MAX][16],
                            int *ids) {
    int n = 0;
    snprintf(store[n], 16, "%s", (s_page == SM_PAGE_HUB) ? "RESUME" : "BACK");
    b[n].label = store[n]; b[n].rect = (SDL_Rect){0,0,0,0}; ids[n] = SM_FOOT_BACK; n++;
    if (s_page == SM_PAGE_HUB) {
        snprintf(store[n], 16, "OPEN");
        b[n].label = store[n]; b[n].rect = (SDL_Rect){0,0,0,0}; ids[n] = SM_FOOT_OPEN; n++;
    } else if (s_page == SM_PAGE_CONTROLS) {

        snprintf(store[n], 16, "DEFAULTS");
        b[n].label = store[n]; b[n].rect = (SDL_Rect){0,0,0,0};
        ids[n] = SM_FOOT_DEFAULTS; n++;
    }
    LauncherDraw_FooterLayout(b, n);
    return n;
}

static int sm_rows_visible(void);
static int sm_rows_now(void);
static int sm_list_bottom(void);

static void sm_focus_rgb(Uint8 *cr, Uint8 *cg, Uint8 *cb) {
    const char *v = GameVersion_Current();
    if (v && strcmp(v, "red")  == 0) { *cr = 0xA0; *cg = 0x00; *cb = 0x00; return; }
    if (v && strcmp(v, "blue") == 0) { *cr = 0x00; *cg = 0x28; *cb = 0xC0; return; }
    *cr = 0x00; *cg = 0x00; *cb = 0x80;
}

static int sm_row_extra(int i) {
    if (s_page < 0) return 0;
    return PresentationMenu_RowHeader(s_page, i) ? SM_SCALE(15) : 0;
}

static int sm_row_h(void) {
    if (s_page == SM_PAGE_STATES) return SM_STATE_ROW_H;

    if (s_page == SM_PAGE_HUB) return SM_ROW_H;
    {

        int n = sm_rows_now();
        int avail = sm_list_bottom() - SM_LIST_TOP;
        int floor_h = LDRAW_LINE_H(1) + 18;
        int extra = 0, fit, i;
        if (n < 1) return SM_ROW_H;
        for (i = 0; i < n; i++) extra += sm_row_extra(i);
        fit = (avail - extra) / n;
        if (fit >= SM_ROW_H) return SM_ROW_H;
        return (fit < floor_h) ? floor_h : fit;
    }
}

static int sm_bind_label_w(void) {
    int w = 0, i;
    for (i = 0; i < INPUT_BIND_COUNT; i++) {
        int t = LauncherDraw_TextWidthBold(SM_TXT_LABEL, Input_BindName(kBindOrder[i]));
        if (t > w) w = t;
    }
    return w + 16;
}

static int sm_panel_w(void) {

    if (SM_COMPACT) return LDRAW_W - SM_PAD;
    return (LDRAW_W * 55) / 100;
}

static int sm_list_bottom(void) { return LDRAW_H - SM_SCALE(64); }

static int sm_row_h(void);

static int sm_rows_visible(void) {
    int n = sm_rows_now(), bot = sm_list_bottom(), h = sm_row_h();
    int y = SM_LIST_TOP, c = 0, i;
    for (i = s_row_scroll; i < n; i++) {
        int step = sm_row_extra(i) + h;
        if (y + step > bot) break;
        y += step;
        c++;
    }
    return c < 1 ? 1 : c;
}

static SDL_Rect sm_row_rect(int i) {
    SDL_Rect r;
    int h = sm_row_h();
    int y = SM_LIST_TOP, k;
    r.x = SM_PAD;
    r.w = sm_row_w();
    r.h = h - 6;
    if (i < s_row_scroll || i >= s_row_scroll + sm_rows_visible()) {
        r.h = 0; r.y = -1000;
        return r;
    }

    for (k = s_row_scroll; k < i; k++) y += sm_row_extra(k) + h;
    r.y = y + sm_row_extra(i);
    return r;
}

static void sm_clamp_row_scroll(int n) {
    int vis = sm_rows_visible();
    int max = n - vis;
    if (max < 0) max = 0;
    if (s_focus < s_row_scroll)        s_row_scroll = s_focus;
    if (s_focus >= s_row_scroll + vis) s_row_scroll = s_focus - vis + 1;
    if (s_row_scroll > max) s_row_scroll = max;
    if (s_row_scroll < 0)   s_row_scroll = 0;
}

static SDL_Rect sm_dock_rect(void) {
    SDL_Rect box, out;
    int fw = Display_FrameWidth(), fh = 144;
    box.x = sm_panel_w() + 8;
    box.y = SM_LIST_TOP - 8;
    box.w = LDRAW_W - box.x - SM_PAD;
    box.h = LDRAW_H - box.y - 64;
    if (fw <= 0) fw = 160;

    out.w = box.w;
    out.h = out.w * fh / fw;
    if (out.h > box.h) { out.h = box.h; out.w = out.h * fw / fh; }
    out.x = box.x + (box.w - out.w) / 2;
    out.y = box.y + (box.h - out.h) / 2;
    return out;
}

static SDL_Rect sm_game_rect(void) {
    SDL_Rect full = { 0, 0, LDRAW_W, LDRAW_H };

    if (SM_COMPACT) { SDL_Rect none = { 0, 0, 0, 0 }; return none; }
    SDL_Rect dock = sm_dock_rect();
    int t = s_dock;
    int inv, e;
    if (t >= 256) return dock;
    inv = 256 - t;
    e   = 256 - (inv * inv) / 256;
    full.x += ((dock.x - full.x) * e) / 256;
    full.y += ((dock.y - full.y) * e) / 256;
    full.w += ((dock.w - full.w) * e) / 256;
    full.h += ((dock.h - full.h) * e) / 256;
    return full;
}

#define SM_CARET_W 18

static SDL_Rect sm_combo_rect(int row);

static int sm_row_w(void) { return sm_panel_w() - SM_PAD - 12; }

static int sm_page_label_w(int txt) {
    int w = 0, n, i;
    if (s_page < 0) return SM_SCALE(90);
    n = PresentationMenu_PageRowCount(s_page);
    for (i = 0; i < n; i++) {
        int t = LauncherDraw_TextWidthBold(txt, PresentationMenu_RowLabel(s_page, i));
        if (t > w) w = t;
    }
    return w;
}

static int sm_page_value_w(int txt) {
    int w = 0, n, i, j;
    if (s_page < 0) return 0;
    n = PresentationMenu_PageRowCount(s_page);
    for (i = 0; i < n; i++) {
        int id = PresentationMenu_RowId(s_page, i);
        int nc = PresentationMenu_ChoiceCount(id);
        for (j = 0; j < nc; j++) {
            int t = LauncherDraw_TextWidth(txt, PresentationMenu_ChoiceLabel(id, j));
            if (t > w) w = t;
        }
    }
    return w;
}

static int sm_value_txt(void) { return 1; }

static int sm_value_col_w(void) {
    return sm_page_value_w(sm_value_txt()) + SM_CARET_W + 20;
}

static int sm_page_txt(void) {
    int rw = sm_row_w();
    if (SM_TXT_LABEL <= 1 || s_page < 0) return SM_TXT_LABEL;
    if (sm_page_label_w(SM_TXT_LABEL) + 22 + sm_value_col_w() > rw) return 1;
    return SM_TXT_LABEL;
}

static int s_dd_scroll;

static SDL_Rect sm_dd_rect(int row, int n) {
    const int top_limit = 4;
    const int bot_limit = LDRAW_H - 52;
    SDL_Rect f = sm_combo_rect(row);
    SDL_Rect d;
    int below, above, want;

    if (n < 1) n = 1;
    d.x = f.x;
    d.w = f.w;

    below = bot_limit - (f.y + f.h);
    above = f.y - top_limit;
    want  = n * SM_DD_ITEM_H + 8;

    if (want <= below || below >= above) {
        d.y = f.y + f.h - 2;
        d.h = (want <= below) ? want : below;
    } else {
        d.h = (want <= above) ? want : above;
        d.y = f.y - d.h + 2;
    }
    if (d.h < SM_DD_ITEM_H + 8) d.h = SM_DD_ITEM_H + 8;
    return d;
}

static int sm_dd_visible(const SDL_Rect *d) {
    int v = (d->h - 8) / SM_DD_ITEM_H;
    return v < 1 ? 1 : v;
}

static SDL_Rect sm_combo_rect(int row) {
    SDL_Rect rr = sm_row_rect(row);
    SDL_Rect c;

    int lab = sm_page_label_w(sm_page_txt()) + 22;
    int need = sm_value_col_w();
    c.w = rr.w * 44 / 100;
    if (c.w < need) c.w = need;
    if (c.w > rr.w - lab) c.w = rr.w - lab;
    if (c.w < SM_SCALE(70)) c.w = SM_SCALE(70);
    c.x = rr.x + rr.w - c.w - 4;
    c.y = rr.y + 4;
    c.h = rr.h - 8;
    return c;
}

static SDL_Rect sm_caret_rect(int row) {
    SDL_Rect f = sm_combo_rect(row);
    SDL_Rect c;
    c.w = SM_CARET_W;
    c.h = f.h - 4;
    c.x = f.x + f.w - SM_CARET_W - 2;
    c.y = f.y + 2;
    return c;
}

static void sm_draw_caret(SDL_Renderer *r, SDL_Rect box, int up) {
    const int w = 9;
    const int h = (w + 1) / 2;
    const int x0 = box.x + (box.w - w) / 2;
    const int y0 = box.y + (box.h - h) / 2;
    SDL_SetRenderDrawColor(r, LCOL_TEXT, 0xFF);
    for (int row = 0; row < h; row++) {
        SDL_Rect line;
        line.w = w - row * 2;
        if (line.w <= 0) break;
        line.x = x0 + row;
        line.y = up ? (y0 + h - 1 - row) : (y0 + row);
        line.h = 1;
        SDL_RenderFillRect(r, &line);
    }
}

static void sm_dd_clamp_scroll(const SDL_Rect *d, int n) {
    int vis = sm_dd_visible(d);
    int max = n - vis;
    if (max < 0) max = 0;
    if (s_dd_focus < s_dd_scroll)        s_dd_scroll = s_dd_focus;
    if (s_dd_focus >= s_dd_scroll + vis) s_dd_scroll = s_dd_focus - vis + 1;
    if (s_dd_scroll > max) s_dd_scroll = max;
    if (s_dd_scroll < 0)   s_dd_scroll = 0;
}

static SDL_Rect sm_dd_item_rect(const SDL_Rect *d, int i) {
    SDL_Rect r;
    int row = i - s_dd_scroll;
    r.x = d->x + 4;
    r.w = d->w - 8;
    r.h = SM_DD_ITEM_H;
    r.y = d->y + 4 + row * SM_DD_ITEM_H;
    if (row < 0 || row >= sm_dd_visible(d)) { r.h = 0; r.y = -1000; }
    return r;
}

static int sm_slot_buttons(int i, SDL_Rect *out, const char **labels);

static SDL_Rect sm_thumb_rect(const save_slot_info_t *in, SDL_Rect g,
                              int *mul_out, int *div_out) {
    SDL_Rect t = { 0, 0, 0, 0 };
    int mul, mh, div = 1;
    if (!in || in->thumb_w < 1 || in->thumb_h < 1 || g.w < 1 || g.h < 1) {
        *mul_out = 1; *div_out = 1;
        return t;
    }
    mul = g.w / in->thumb_w;
    mh  = g.h / in->thumb_h;
    if (mh < mul) mul = mh;
    if (mul >= 1) {
        t.w = in->thumb_w * mul;
        t.h = in->thumb_h * mul;
    } else {

        mul = 0;
        div = 2;
        while (div < in->thumb_w &&
               (in->thumb_w / div > g.w || in->thumb_h / div > g.h))
            div++;
        t.w = in->thumb_w / div;
        t.h = in->thumb_h / div;
    }
    if (t.w < 1) t.w = 1;
    if (t.h < 1) t.h = 1;
    t.x = g.x + (g.w - t.w) / 2;
    t.y = g.y + (g.h - t.h) / 2;
    *mul_out = mul; *div_out = div;
    return t;
}

static void sm_draw(SDL_Renderer *r) {

    {

        SDL_Rect g = sm_game_rect();

        SDL_Rect box = g;
        if (s_page == SM_PAGE_STATES && !s_closing && g.w > 0 && g.h > 0) {
            const save_slot_info_t *ti = SaveSlots_Info(s_focus);
            if (ti && ti->thumb && ti->thumb_w && ti->thumb_h) {
                int m, d;
                box = sm_thumb_rect(ti, g, &m, &d);
            }
        }
        {
            SDL_Rect frame = { box.x - 2, box.y - 2, box.w + 4, box.h + 4 };
            if (box.w > 0 && box.h > 0) {
                SDL_SetRenderDrawColor(r, 0x00, 0x00, 0x00, 0xFF);
                SDL_RenderFillRect(r, &box);
                if (s_dock >= 256) LauncherDraw_Bevel(r, frame, 0);
            }
        }
        s_slot = g;

        if (s_page == SM_PAGE_STATES && !s_closing && g.w > 0 && g.h > 0) {
            const save_slot_info_t *in = SaveSlots_Info(s_focus);
            s_slot.w = s_slot.h = 0;
            if (in && in->thumb && in->thumb_w && in->thumb_h && s_surf) {

                uint32_t *dst = (uint32_t *)s_surf->pixels;
                int mul, div;

                SDL_Rect tr = sm_thumb_rect(in, g, &mul, &div);
                int dw = tr.w, dh = tr.h, ox = tr.x, oy = tr.y;

                for (int yy = 0; yy < dh; yy++) {
                    int ty = oy + yy;
                    int sy = mul ? yy / mul : yy * div;
                    if (ty < 0 || ty >= s_surf_h) continue;
                    if (sy >= in->thumb_h) sy = in->thumb_h - 1;
                    {
                        const uint32_t *row = &in->thumb[sy * in->thumb_w];
                        for (int xx = 0; xx < dw; xx++) {
                            int tx = ox + xx;
                            int sx = mul ? xx / mul : xx * div;
                            if (tx < 0 || tx >= s_surf_w) continue;
                            if (sx >= in->thumb_w) sx = in->thumb_w - 1;
                            dst[ty * s_surf_w + tx] = row[sx] | 0xFFu;
                        }
                    }
                }
            } else {
                const char *msg = (in && in->occupied) ? "NO PREVIEW" : "EMPTY SLOT";
                LauncherDraw_Text(r, g.x + (g.w - LauncherDraw_TextWidth(1, msg)) / 2,
                                  g.y + g.h / 2 - 4, 1, 0x80, 0x80, 0x80, msg);
            }
            {
                char cap[24];
                snprintf(cap, sizeof cap, "SLOT %d", s_focus + 1);
                LauncherDraw_Text(r, g.x, g.y - 14, 1, LCOL_TEXT, cap);
            }
            if (s_state_msg_slot == s_focus && s_state_msg[0])
                LauncherDraw_Text(r, g.x, g.y + g.h + 6, 1,
                                  0x00, 0x60, 0x00, s_state_msg);
        }

        if (s_closing) {
            s_dock -= 256 / SM_DOCK_FRAMES;
            if (s_dock < 0) s_dock = 0;
        } else {
            s_dock += 256 / SM_DOCK_FRAMES;
            if (s_dock > 256) s_dock = 256;
        }
    }

    SDL_Rect panel = { SM_PAD - 12, SM_PAD - 12,
                       sm_panel_w() - (SM_PAD - 12),
                       LDRAW_H - (SM_PAD - 12) * 2 - 28 };
    SDL_SetRenderDrawColor(r, LCOL_BG, 0xFF);
    SDL_RenderFillRect(r, &panel);
    LauncherDraw_Bevel(r, panel, 1);

    LauncherDraw_TextBold(r, SM_PAD, SM_TITLE_Y, SM_TXT_LABEL, LCOL_TEXT,
                      (s_page == SM_PAGE_STATES)   ? "SAVE STATE"
                      : (s_page == SM_PAGE_CONTROLS) ? "CONTROLS"
                      : (s_page == SM_PAGE_HUB)      ? "OPTIONS"
                      : PresentationMenu_PageName(s_page));

    if (s_page == SM_PAGE_CONTROLS) {
        int kx, px, w, hy = SM_LIST_TOP - SM_SCALE(13);
        sm_bind_cols(&kx, &px, &w);
        LauncherDraw_TextClipped(r, kx + 2, hy, 1, 0x40, 0x40, 0x40, "KEYBOARD", w);
        LauncherDraw_TextClipped(r, px + 2, hy, 1, 0x40, 0x40, 0x40, "GAMEPAD",  w);

        {

            static const char *kMsg[] = {
                "HOLD F5 OR BACK+START ON A PAD TO RESET ALL BINDINGS TO DEFAULTS",
                "HOLD F5 OR BACK+START ON A PAD TO RESET ALL BINDINGS",
                "HOLD F5 TO RESET ALL BINDINGS",
            };
            int room = sm_row_w();
            const char *msg = NULL;
            for (int m = 0; m < (int)(sizeof kMsg / sizeof kMsg[0]); m++) {
                if (LauncherDraw_TextWidth(1, kMsg[m]) <= room) { msg = kMsg[m]; break; }
            }
            if (msg) {

                int hy = SM_LIST_TOP + sm_rows_visible() * sm_row_h() + SM_SCALE(7);
                int cap = LDRAW_H - LDRAW_FOOTER_H - LDRAW_INK_H(1) - 4;
                int hw = LauncherDraw_TextWidth(1, msg);
                if (hy > cap) hy = cap;
                LauncherDraw_Text(r, SM_PAD + (room - hw) / 2, hy,
                                  1, 0x60, 0x60, 0x60, msg);
            }
        }
    }

    {
        SDL_Rect rule = { SM_PAD, SM_TITLE_Y + LDRAW_LINE_H(SM_TXT_LABEL) + 8,
                          sm_panel_w() - SM_PAD - 12, 2 };
        LauncherDraw_Bevel(r, rule, 0);
    }

    const int dd_open = (s_page >= 0 && s_dd_row >= 0);

    const int sticky = (s_page == SM_PAGE_STATES);
    const int hover_follows = LauncherNav_HoverHighlight(&s_nav) && !dd_open && !sticky;
    const int nrows = sm_rows_now();
    sm_clamp_row_scroll(nrows);

    {

        int vis = sm_rows_visible();
        int cx  = sm_panel_w() - SM_PAD - 6;
        if (s_row_scroll > 0) {
            SDL_Rect up = { cx, SM_LIST_TOP + 1, 10, 8 };
            sm_draw_caret(r, up, 1);
        }
        if (s_row_scroll + vis < nrows) {
            SDL_Rect dn = { cx, sm_list_bottom() - 9, 10, 8 };
            sm_draw_caret(r, dn, 0);
        }
    }
    for (int i = 0; i < nrows; i++) {
        SDL_Rect rr = sm_row_rect(i);
        if (rr.h == 0) continue;
        int hovered = hover_follows &&
                      LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, rr);
        int focused = hover_follows ? hovered : (i == s_focus);

        Uint8 fr, fg, fb;
        sm_focus_rgb(&fr, &fg, &fb);

        if (focused) LauncherDraw_FocusBarRGB(r, rr, fr, fg, fb);

        else if (sticky && LauncherNav_HoverHighlight(&s_nav) &&
                 LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, rr)) {
            SDL_SetRenderDrawColor(r, fr, fg, fb, 0xFF);
            SDL_RenderDrawRect(r, &rr);
        }

        Uint8 tr = 0x00, tg = 0x00, tb = 0x00;
        if (focused) { tr = tg = tb = 0xFF; }

        if (s_page >= 0 && !focused &&
            !PresentationMenu_RowAvailable(PresentationMenu_RowId(s_page, i)))
            { tr = tg = tb = 0x90; }

        if (s_page >= 0) {
            const char *hdr = PresentationMenu_RowHeader(s_page, i);
            if (hdr && sm_row_extra(i) > 0) {
                int hy = rr.y - sm_row_extra(i) + SM_SCALE(3);
                LauncherDraw_TextBold(r, rr.x + 10, hy, 1, 0x00, 0x00, 0x00, hdr);

                {
                    int lx = rr.x + 14 + LauncherDraw_TextWidthBold(1, hdr);
                    SDL_Rect rule = { lx, hy + LDRAW_INK_H(1) / 2,
                                      rr.x + rr.w - lx, 1 };
                    if (rule.w > 8) {
                        SDL_SetRenderDrawColor(r, 0x90, 0x90, 0x90, 0xFF);
                        SDL_RenderFillRect(r, &rule);
                    }
                }
            }
        }

        char slotlbl[24];
        const char *label;
        if (s_page == SM_PAGE_STATES) {
            snprintf(slotlbl, sizeof slotlbl, "SLOT %d", i + 1);
            label = slotlbl;
        } else if (s_page == SM_PAGE_CONTROLS) {
            label = Input_BindName(kBindOrder[i]);
        } else {
            label = (s_page == SM_PAGE_HUB) ? kRows[i].label
                                 : PresentationMenu_RowLabel(s_page, i);
        }

        {
            const int is_set = (s_page >= 0);

            LauncherDraw_TextClippedBold(r, rr.x + 10, rr.y + SM_ROW_TEXT,
                                         is_set ? sm_page_txt() : SM_TXT_LABEL,
                                         tr, tg, tb, label,
                                         is_set ? sm_combo_rect(i).x - (rr.x + 10) - 8
                                                : rr.w - 20);
        }

        Uint8 dr = 0x40, dg = 0x40, db = 0x40;
        if (focused) { dr = dg = db = 0xE0; }
        if (s_page == SM_PAGE_CONTROLS) {
            SDL_Rect br[2];
            char     bl[2][24];
            int nb = sm_bind_buttons(i, br, bl);
            for (int k = 0; k < nb; k++) {
                int hot = LauncherNav_HoverHighlight(&s_nav) &&
                          LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, br[k]);
                int padsel = !LauncherNav_HoverHighlight(&s_nav) &&
                             i == s_focus && k == s_btn_focus;
                int listening = (s_cap_row == i && s_cap_kind ==
                                 (k == SM_BIND_KEY ? SM_CAP_KEY : SM_CAP_PAD));
                SDL_SetRenderDrawColor(r, LCOL_PANEL, 0xFF);
                SDL_RenderFillRect(r, &br[k]);

                LauncherDraw_Bevel(r, br[k], (hot || padsel || listening) ? 0 : 1);
                LauncherDraw_TextClipped(r, br[k].x + 5,
                                         LDRAW_TEXT_Y(br[k].y, br[k].h, 1),
                                         1, LCOL_TEXT, bl[k], br[k].w - 10);
            }
        } else if (s_page == SM_PAGE_STATES) {

            const save_slot_info_t *in = SaveSlots_Info(i);
            char right[64];
            if (!in || !in->occupied)          snprintf(right, sizeof right, "EMPTY");
            else if (!in->readable)
                snprintf(right, sizeof right,
                         in->err == SAVE_STATE_ERR_VERSION ? "OTHER BUILD" : "DAMAGED");
            else if (in->map[0])
                snprintf(right, sizeof right, "%s  %s", SaveSlots_When(i), in->map);
            else
                snprintf(right, sizeof right, "%s", SaveSlots_When(i));

            {
                SDL_Rect b0[3]; const char *l0[3];
                int nb0 = sm_slot_buttons(i, b0, l0);
                int bx  = nb0 ? b0[0].x : rr.x + rr.w;
                if (SM_COMPACT)
                    LauncherDraw_TextClipped(r, rr.x + 12 +
                                             LauncherDraw_TextWidthBold(SM_TXT_LABEL, label) + 8,
                                             rr.y + SM_ROW_TEXT + 1, 1,
                                             dr, dg, db, right,
                                             bx - rr.x - LauncherDraw_TextWidthBold(SM_TXT_LABEL, label) - 26);
                else
                    LauncherDraw_TextClipped(r, rr.x + 12,
                                             rr.y + SM_ROW_TEXT + SM_SCALE(16), 1,
                                             dr, dg, db, right, rr.w - SM_SCALE(200));
            }

            {
                SDL_Rect br[3];
                const char *bl[3];
                int nb = sm_slot_buttons(i, br, bl);
                for (int k = 0; k < nb; k++) {
                    int hot = LauncherNav_HoverHighlight(&s_nav) &&
                              LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, br[k]);

                    int padsel = !LauncherNav_HoverHighlight(&s_nav) &&
                                 i == s_focus && k == s_btn_focus;
                    SDL_SetRenderDrawColor(r, LCOL_PANEL, 0xFF);
                    SDL_RenderFillRect(r, &br[k]);
                    LauncherDraw_Bevel(r, br[k], (hot || padsel) ? 0 : 1);
                    int tw = LauncherDraw_TextWidth(1, bl[k]);

                    LauncherDraw_Text(r, br[k].x + (br[k].w - tw) / 2,
                                      LDRAW_TEXT_Y(br[k].y, br[k].h, 1),
                                      1, LCOL_TEXT, bl[k]);
                }
            }
        } else if (s_page == SM_PAGE_HUB) {

            int lw = LauncherDraw_TextWidthBold(SM_TXT_LABEL, kRows[i].label);
            if (kRows[i].detail) {
                int dw = LauncherDraw_TextWidth(1, kRows[i].detail);
                if (lw + dw + 34 <= rr.w)
                    LauncherDraw_TextClipped(r, rr.x + rr.w - 10 - dw,
                                             rr.y + SM_ROW_TEXT + 4, 1,
                                             dr, dg, db, kRows[i].detail,
                                             rr.w - lw - 24);
            }
        } else {
            int id  = PresentationMenu_RowId(s_page, i);
            int idx = PresentationMenu_CurrentIndex(id);

            int avail = PresentationMenu_RowAvailable(id);
            const char *val = avail ? PresentationMenu_ChoiceLabel(id, idx)
                                    : "OPENGL ONLY";
            SDL_Rect fr = sm_combo_rect(i);
            SDL_Rect cr = sm_caret_rect(i);
            int mine = (s_dd_row == i);

            SDL_SetRenderDrawColor(r, LCOL_LIGHT, 0xFF);
            SDL_RenderFillRect(r, &fr);
            LauncherDraw_Bevel(r, fr, 0);
            {
                const int vt = sm_value_txt();

                Uint8 vr = 0x00, vg = 0x00, vb = 0x00;
                if (!avail) { vr = 0x60; vg = 0x60; vb = 0x60; }
                LauncherDraw_TextClipped(r, fr.x + 6,
                                         LDRAW_TEXT_Y(fr.y, fr.h, vt),
                                         vt, vr, vg, vb,
                                         val, fr.w - SM_CARET_W - 14);
            }

            SDL_SetRenderDrawColor(r, LCOL_PANEL, 0xFF);
            SDL_RenderFillRect(r, &cr);
            LauncherDraw_Bevel(r, cr, mine ? 0 : 1);
            {
                SDL_Rect g = cr;
                if (mine) { g.x += 1; g.y += 1; }
                sm_draw_caret(r, g, mine);
            }

            if (focused && !dd_open)
                LauncherDraw_Text(r, fr.x - 12, rr.y + SM_ROW_TEXT, 2,
                                  dr, dg, db, "<");
        }
    }

    if (s_page >= 0 && s_dd_row >= 0) {
        int id = PresentationMenu_RowId(s_page, s_dd_row);
        int n  = PresentationMenu_ChoiceCount(id);
        int cur = PresentationMenu_CurrentIndex(id);
        SDL_Rect d = sm_dd_rect(s_dd_row, n);
        sm_dd_clamp_scroll(&d, n);
        SDL_SetRenderDrawColor(r, LCOL_LIGHT, 0xFF);
        SDL_RenderFillRect(r, &d);
        LauncherDraw_Bevel(r, d, 1);

        {
            int vis = sm_dd_visible(&d);
            if (s_dd_scroll > 0) {
                SDL_Rect up = { d.x + d.w - 14, d.y + 3, 10, 8 };
                sm_draw_caret(r, up, 1);
            }
            if (s_dd_scroll + vis < n) {
                SDL_Rect dn = { d.x + d.w - 14, d.y + d.h - 11, 10, 8 };
                sm_draw_caret(r, dn, 0);
            }
        }
        for (int i = 0; i < n; i++) {
            SDL_Rect ir = sm_dd_item_rect(&d, i);
            if (ir.h == 0) continue;

            int on = (i == s_dd_focus);
            if (on) {
                Uint8 fr2, fg2, fb2;
                sm_focus_rgb(&fr2, &fg2, &fb2);
                LauncherDraw_FocusBarRGB(r, ir, fr2, fg2, fb2);
            }
            Uint8 cr2 = on ? 0xFF : 0x00, cg2 = on ? 0xFF : 0x00, cb2 = on ? 0xFF : 0x00;

            {
                const int it = sm_value_txt();
                if (i == cur)
                    LauncherDraw_Text(r, ir.x + 4, LDRAW_TEXT_Y(ir.y, ir.h, 1),
                                      1, cr2, cg2, cb2, ">");
                LauncherDraw_TextClipped(r, ir.x + 16,
                                         LDRAW_TEXT_Y(ir.y, ir.h, it),
                                         it, cr2, cg2, cb2,
                                         PresentationMenu_ChoiceLabel(id, i),
                                         ir.w - 22);
            }
        }
    }

    if (s_confirm_slot >= 0) {
        SDL_Rect box = sm_confirm_box();
        SDL_Rect yes, no;
        sm_confirm_buttons(&box, &yes, &no);
        char q[48];
        if (s_confirm_kind == SM_ASK_EXIT)
            snprintf(q, sizeof q, "EXIT TO LAUNCHER?");
        else if (s_confirm_kind == SM_ASK_DELETE)
            snprintf(q, sizeof q, "DELETE SLOT %d?", s_confirm_slot + 1);
        else
            snprintf(q, sizeof q, "OVERWRITE SLOT %d?", s_confirm_slot + 1);

        SDL_SetRenderDrawColor(r, LCOL_BG, 0xFF);
        SDL_RenderFillRect(r, &box);
        LauncherDraw_Bevel(r, box, 1);

        if (s_confirm_kind == SM_ASK_WIDESCREEN) {

            static const char *kBody[] = {
                "MODERN ASPECT RATIOS ARE",
                "EXPERIMENTAL, SOME THINGS",
                "MAY LOOK OFF PRESENTATION",
                "WISE, FOR THE TIME BEING."
            };
            LauncherDraw_Text(r, box.x + 16, box.y + 14, SM_TXT_LABEL,
                              LCOL_TEXT, "WIDESCREEN");
            for (int i = 0; i < 4; i++)
                LauncherDraw_Text(r, box.x + 16, box.y + 36 + i * 12, 1,
                                  0x60, 0x60, 0x60, kBody[i]);
        } else {
        LauncherDraw_Text(r, box.x + 16, box.y + 14, SM_TXT_LABEL, LCOL_TEXT, q);
        LauncherDraw_Text(r, box.x + 16, box.y + 36, 1, 0x60, 0x60, 0x60,
                          s_confirm_kind == SM_ASK_EXIT
                            ? "UNSAVED PROGRESS WILL BE LOST"
                          : s_confirm_kind == SM_ASK_DELETE
                            ? "THE SAVED STATE WILL BE ERASED"
                            : "THE SAVED STATE WILL BE REPLACED");
        }

        for (int k = 0; k < 2; k++) {
            SDL_Rect b = k ? no : yes;

            const int tick = (s_confirm_kind == SM_ASK_WIDESCREEN && k == 0);
            const char *lbl = (s_confirm_kind == SM_ASK_WIDESCREEN)
                                ? "OK" : (k ? "NO" : "YES");
            int hot = LauncherNav_HoverHighlight(&s_nav)
                        ? LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, b)
                        : (k == !s_confirm_yes);
            SDL_SetRenderDrawColor(r, LCOL_PANEL, 0xFF);
            SDL_RenderFillRect(r, &b);
            LauncherDraw_Bevel(r, b, hot ? 0 : 1);
            if (tick) {

                const char *l1 = s_notice_tick ? "[X] DO NOT SHOW"
                                               : "[ ] DO NOT SHOW";
                const char *l2 = "ME THIS AGAIN";
                int w1 = LauncherDraw_TextWidth(1, l1);
                int w2 = LauncherDraw_TextWidth(1, l2);
                LauncherDraw_Text(r, b.x + (b.w - w1) / 2, b.y + b.h / 2 - 9,
                                  1, LCOL_TEXT, l1);
                LauncherDraw_Text(r, b.x + (b.w - w2) / 2, b.y + b.h / 2 + 1,
                                  1, LCOL_TEXT, l2);
            } else {

            int tw = LauncherDraw_TextWidth(SM_TXT_LABEL, lbl);
            LauncherDraw_Text(r, b.x + (b.w - tw) / 2,
                              LDRAW_TEXT_Y(b.y, b.h, SM_TXT_LABEL),
                              SM_TXT_LABEL, LCOL_TEXT, lbl);
            }
        }
    }

    if (LauncherNav_Device(&s_nav) == LNAV_INPUT_GAMEPAD) {

        if (s_page == SM_PAGE_STATES)
            LauncherDraw_PromptBar(r, "CHOOSE", "BACK", "CLOSE", NULL);
        else if (s_page == SM_PAGE_HUB)
            LauncherDraw_PromptBar(r, "OPEN", NULL, "CLOSE", NULL);
        else
            LauncherDraw_PromptBar(r, NULL, "BACK", "CLOSE", NULL);
    } else {

        ldraw_footer_btn_t btns[SM_FOOT_MAX];
        char store[SM_FOOT_MAX][16];
        int  ids[SM_FOOT_MAX];
        int n = sm_footer_layout(btns, store, ids);
        int hover = -1;
        for (int i = 0; i < n; i++)
            if (LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, btns[i].rect))
                hover = i;
        LauncherDraw_FooterButtons(r, btns, n, hover);
    }
}

static const uint32_t *sm_compose(int *w, int *h,
                                  int *gx, int *gy, int *gw, int *gh) {
    int ow = 0, oh = 0, n = 1;
    Display_GetOutputSize(&ow, &oh);
    if (ow < 1 || oh < 1) { ow = LDRAW_W_DESKTOP; oh = LDRAW_H_BASE; }
    while (ow / (n + 1) >= SM_MIN_W && oh / (n + 1) >= SM_MIN_H) n++;
    s_scale = n;

    if (!sm_ensure_surface(ow / n, oh / n)) { *w = *h = 0; return NULL; }
    LauncherDraw_SetSize(s_surf_w, s_surf_h);

    s_slot.x = s_slot.y = s_slot.w = s_slot.h = 0;
    SDL_SetRenderDrawColor(s_soft, 0, 0, 0, 0xFF);
    SDL_RenderClear(s_soft);
    sm_draw(s_soft);

    if (s_slot.w > 0 && s_slot.h > 0) {
        uint32_t *px = (uint32_t *)s_surf->pixels;
        for (int yy = s_slot.y; yy < s_slot.y + s_slot.h; yy++) {
            if (yy < 0 || yy >= s_surf_h) continue;
            for (int xx = s_slot.x; xx < s_slot.x + s_slot.w; xx++) {
                if (xx < 0 || xx >= s_surf_w) continue;
                px[yy * s_surf_w + xx] &= 0xFFFFFF00u;
            }
        }
    }

    *w = s_surf_w;
    *h = s_surf_h;
    *gx = s_slot.x; *gy = s_slot.y;
    *gw = s_slot.w; *gh = s_slot.h;
    return (const uint32_t *)s_surf->pixels;
}

enum { SM_SLOT_SAVE, SM_SLOT_LOAD, SM_SLOT_DELETE };

static void sm_slot_action(int slot, int what) {
    const save_slot_info_t *in = SaveSlots_Info(slot);
    s_state_msg_slot = slot;
    s_state_msg[0] = 0;

    switch (what) {
    case SM_SLOT_SAVE: {
        extern int Game_SceneHasMap(void);

        if (!Game_SceneHasMap())
            snprintf(s_state_msg, sizeof s_state_msg, "NOT IN GAME");
        else
            snprintf(s_state_msg, sizeof s_state_msg,
                     SaveSlots_Write(slot) == 0 ? "SAVED" : "SAVE FAILED");
        break;
    }
    case SM_SLOT_LOAD:
        if (!in || !in->occupied) {
            snprintf(s_state_msg, sizeof s_state_msg, "SLOT IS EMPTY");
        } else if (!in->readable) {

            snprintf(s_state_msg, sizeof s_state_msg,
                     in->err == SAVE_STATE_ERR_VERSION ? "FROM ANOTHER BUILD"
                                                       : "FILE DAMAGED");
        } else if (SaveSlots_Read(slot) == 0) {

            SuspendMenu_Close();
        } else {
            snprintf(s_state_msg, sizeof s_state_msg, "LOAD FAILED");
        }
        break;
    case SM_SLOT_DELETE:
        if (in && in->occupied) {
            SaveSlots_Delete(slot);
            snprintf(s_state_msg, sizeof s_state_msg, "DELETED");
        }
        break;
    }
}

void SuspendMenu_Open(void) {
    if (s_open) return;
    if (!s_nav_ready) { LauncherNav_Init(&s_nav); s_nav_ready = 1; }

    s_focus       = 0;
    s_intents     = 0;
    s_close_armed = 0;
    s_dock        = 0;
    s_page        = -1;
    s_dd_row      = -1;
    s_cap_row     = -1;
    s_closing     = 0;
    s_row_scroll  = 0;
    s_wheel       = 0;
    s_open        = 1;

    Display_SetSuspendOverlay(sm_compose);
}

static void sm_finish_close(void) {
    s_open    = 0;
    s_closing = 0;
    Display_SetSuspendOverlay(NULL);
    if (DisplayGL_IsActive() && s_saved_filter[0])
        DisplayGL_SetFilter(s_saved_filter);

    Display_RestoreLogicalSize();
}

void SuspendMenu_Close(void) {
    if (!s_open || s_closing) return;

    s_dd_row       = -1;
    s_cap_row      = -1;
    s_confirm_slot = -1;

    if (SM_COMPACT || s_dock <= 0) { sm_finish_close(); return; }
    s_closing = 1;
}

void SuspendMenu_Toggle(void) { if (s_open) SuspendMenu_Close(); else SuspendMenu_Open(); }
int  SuspendMenu_IsOpen(void) { return s_open; }
int  SuspendMenu_ExitToLauncherRequested(void) { return s_exit_requested; }

int SuspendMenu_IsCapturing(void) { return s_cap_row >= 0; }

void SuspendMenu_HandleEvent(const union SDL_Event *ev) {
    if (!s_open || !ev) return;

    if (s_cap_row >= 0 && s_cap_kind == SM_CAP_KEY) {
        const SDL_Event *e = (const SDL_Event *)ev;
        if (e->type == SDL_KEYDOWN && !e->key.repeat) {
            SDL_Scancode sc = e->key.keysym.scancode;
            if (sc != SDL_SCANCODE_ESCAPE)
                Input_SetKey(kBindOrder[s_cap_row], (int)sc);
            s_cap_row = -1;
            s_intents = 0;

            s_cap_settle_key = (int)sc;
        }
        if (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP ||
            e->type == SDL_TEXTINPUT)
            return;
    }

    if (((const SDL_Event *)ev)->type == SDL_MOUSEWHEEL) {
        int dy = ((const SDL_Event *)ev)->wheel.y;
        if (((const SDL_Event *)ev)->wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
            dy = -dy;
        s_wheel -= dy;
    }

    SDL_Renderer *hw = Display_GetRenderer();
    if (hw) {
        LauncherNav_HandleEvent(&s_nav, (const SDL_Event *)ev, hw);
        return;
    }

    {
        SDL_Event scaled = *(const SDL_Event *)ev;
        int n = s_scale > 0 ? s_scale : 1;
        switch (scaled.type) {
        case SDL_MOUSEMOTION:
            scaled.motion.x /= n; scaled.motion.y /= n; break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            scaled.button.x /= n; scaled.button.y /= n; break;
        default: break;
        }
        LauncherNav_HandleEvent(&s_nav, &scaled, NULL);
    }
}

static void sm_consume_pointer_edges(void) {
    s_nav.ptr_pressed = s_nav.ptr_released = s_nav.ptr_moved = 0;
}

static int sm_bound_key_edge(int bit, int *held)
{
    const Uint8 *ks = SDL_GetKeyboardState(NULL);
    int sc = Input_GetKey(bit);
    int down = (ks && sc > 0 && sc < SDL_NUM_SCANCODES) ? ks[sc] : 0;
    int edge = down && !*held;
    *held = down;
    return edge;
}

static void sm_tick_inner(void) {
    if (!s_open) return;

    s_intents |= LauncherNav_Poll(&s_nav);
    unsigned in = s_intents;
    s_intents = 0;

    if (s_cap_settle_key) {
        const Uint8 *ks = SDL_GetKeyboardState(NULL);
        if (ks && ks[s_cap_settle_key]) { s_intents = 0; return; }
        s_cap_settle_key = 0;
        s_intents = 0;
        return;
    }
    if (s_cap_settle_pad) {
        if (Input_PadAnyButton() >= 0) { s_intents = 0; return; }
        s_cap_settle_pad = 0;
        s_intents = 0;
        return;
    }

    const int nrows = sm_rows_now();

    if (s_cap_row >= 0) {
        if (s_cap_kind == SM_CAP_PAD) {
            int b = Input_PadAnyButton();

            if (!s_cap_pad_clear) { if (b < 0) s_cap_pad_clear = 1; }
            else if (b >= 0)      { Input_SetPad(kBindOrder[s_cap_row], b);
                                    s_cap_row = -1;
                                    s_cap_settle_pad = 1;
                                    return; }
        }

        if (in & (LNAV_CANCEL | LNAV_BACK)) {
            s_cap_row = -1;

            s_cap_settle_pad = 1;
            s_cap_settle_key = SDL_SCANCODE_ESCAPE;
        }
        return;
    }

    {
        static int held_a, held_b;
        if (sm_bound_key_edge(1, &held_b)) in |= LNAV_BACK;
        if (sm_bound_key_edge(0, &held_a)) in |= LNAV_ACCEPT;
    }

    if (s_dd_row >= 0) {
        int id = PresentationMenu_RowId(s_page, s_dd_row);
        int n  = PresentationMenu_ChoiceCount(id);
        SDL_Rect d = sm_dd_rect(s_dd_row, n);
        int chosen = -1;

        if (n > 0) {
            if (in & LNAV_UP)   { if (--s_dd_focus < 0) s_dd_focus = n - 1; }
            if (in & LNAV_DOWN) { if (++s_dd_focus >= n) s_dd_focus = 0; }
        }

        if (s_wheel && n > 0) {
            int vis = sm_dd_visible(&d);
            int max = n - vis; if (max < 0) max = 0;
            s_dd_scroll += s_wheel;
            if (s_dd_scroll > max) s_dd_scroll = max;
            if (s_dd_scroll < 0)   s_dd_scroll = 0;

            if (s_dd_focus < s_dd_scroll)        s_dd_focus = s_dd_scroll;
            if (s_dd_focus >= s_dd_scroll + vis) s_dd_focus = s_dd_scroll + vis - 1;
            s_wheel = 0;
        }
        sm_dd_clamp_scroll(&d, n);

        if (LauncherNav_HoverHighlight(&s_nav) && s_nav.ptr_moved) {
            for (int i = 0; i < n; i++) {
                if (LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y,
                                             sm_dd_item_rect(&d, i))) {
                    s_dd_focus = i;
                    break;
                }
            }
        }

        if (in & (LNAV_CANCEL | LNAV_BACK)) { s_dd_row = -1; s_close_armed = 0; return; }

        if (s_nav.ptr_pressed &&
            !LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, d)) {
            s_dd_row = -1;
            return;
        }

        if (s_nav.ptr_released) {
            if (s_dd_swallow_release) {
                s_dd_swallow_release = 0;
            } else {
                for (int i = 0; i < n; i++) {
                    if (LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y,
                                                 sm_dd_item_rect(&d, i))) {
                        chosen = i;
                        break;
                    }
                }
            }
        }
        if (chosen < 0 && (in & LNAV_ACCEPT)) chosen = s_dd_focus;

        if (chosen >= 0) {
            PresentationMenu_SetIndex(id, chosen);
            sm_check_widescreen_notice();
            s_dd_row = -1;
        }
        return;
    }

    if (nrows > 0) {
        if (in & LNAV_UP)   { if (--s_focus < 0) s_focus = nrows - 1; }
        if (in & LNAV_DOWN) { if (++s_focus >= nrows) s_focus = 0; }
        if (s_focus >= nrows) s_focus = nrows - 1;
    }

    if (s_page == SM_PAGE_CONTROLS) {

        if (in & LNAV_RIGHT) s_btn_focus++;
        if (in & LNAV_LEFT)  s_btn_focus--;
        if (s_btn_focus < 0) s_btn_focus = 1;
        if (s_btn_focus > 1) s_btn_focus = 0;
    } else if (s_page == SM_PAGE_STATES) {
        SDL_Rect br[3];
        const char *bl[3];
        int nb = sm_slot_buttons(s_focus, br, bl);
        if (nb < 1) nb = 1;
        if (in & LNAV_RIGHT) s_btn_focus++;
        if (in & LNAV_LEFT)  s_btn_focus--;
        if (s_btn_focus < 0)   s_btn_focus = nb - 1;
        if (s_btn_focus >= nb) s_btn_focus = 0;

        if (in & (LNAV_UP | LNAV_DOWN)) s_btn_focus = 0;
    }

    if (s_wheel && nrows > 0) {
        int vis = sm_rows_visible();
        int max = nrows - vis; if (max < 0) max = 0;
        s_row_scroll += s_wheel;
        if (s_row_scroll > max) s_row_scroll = max;
        if (s_row_scroll < 0)   s_row_scroll = 0;
        if (s_focus < s_row_scroll)        s_focus = s_row_scroll;
        if (s_focus >= s_row_scroll + vis) s_focus = s_row_scroll + vis - 1;
    }
    s_wheel = 0;

    if (s_page >= 0 && (in & (LNAV_LEFT | LNAV_RIGHT))) {
        int id = PresentationMenu_RowId(s_page, s_focus);
        int n  = PresentationMenu_ChoiceCount(id);

        if (id >= 0 && n > 0 && PresentationMenu_RowAvailable(id)) {
            int idx = PresentationMenu_CurrentIndex(id);
            idx += (in & LNAV_RIGHT) ? 1 : -1;
            if (idx < 0)  idx = n - 1;
            if (idx >= n) idx = 0;

            PresentationMenu_SetIndex(id, idx);
            sm_check_widescreen_notice();
        }
    }

    if (LauncherNav_HoverHighlight(&s_nav)) {
        for (int i = 0; i < nrows; i++) {
            if (LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, sm_row_rect(i))) {
                s_focus = i;
                break;
            }
        }
    }
    if (s_nav.ptr_pressed) {
        for (int i = 0; i < nrows; i++) {
            if (LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, sm_row_rect(i))) {
                s_focus = i;
                in |= LNAV_ACCEPT;
                break;
            }
        }
    }

    if (s_confirm_slot >= 0) {
        SDL_Rect box = sm_confirm_box();
        SDL_Rect yes, no;
        sm_confirm_buttons(&box, &yes, &no);
        int slot = s_confirm_slot;
        int kind = s_confirm_kind;

        if (s_confirm_fresh) { s_confirm_fresh = 0; return; }

        if (in & (LNAV_LEFT | LNAV_RIGHT)) s_confirm_yes = !s_confirm_yes;

        if (kind == SM_ASK_WIDESCREEN) {
            int close = 0, toggle = 0;

            if (in & (LNAV_CANCEL | LNAV_BACK)) close = 1;
            else if (in & LNAV_ACCEPT) {
                if (s_confirm_yes) toggle = 1;
                else               close  = 1;
            }
            if (s_nav.ptr_pressed) {
                if (LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, yes))
                    toggle = 1;
                else if (LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, no) ||
                         !LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, box))
                    close = 1;
            }
            if (toggle) { s_notice_tick = !s_notice_tick; return; }
            if (close) {
                PresentationMenu_DismissWidescreenNotice(s_notice_tick);
                s_confirm_slot = -1;
            }
            return;
        }

        if (in & (LNAV_CANCEL | LNAV_BACK)) { s_confirm_slot = -1; return; }
        if (s_nav.ptr_pressed) {
            if (LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, yes)) {
                s_confirm_slot = -1;
                if (kind == SM_ASK_EXIT) { s_exit_requested = 1; SuspendMenu_Close(); }
                else sm_slot_action(slot, kind == SM_ASK_DELETE ? SM_SLOT_DELETE
                                                                : SM_SLOT_SAVE);
            } else if (LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, no) ||
                       !LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, box)) {
                s_confirm_slot = -1;
            }
            return;
        }
        if (in & LNAV_ACCEPT) {
            s_confirm_slot = -1;
            if (s_confirm_yes) {
                if (kind == SM_ASK_EXIT) { s_exit_requested = 1; SuspendMenu_Close(); }
                else sm_slot_action(slot, kind == SM_ASK_DELETE ? SM_SLOT_DELETE
                                                                : SM_SLOT_SAVE);
            }
        }
        return;
    }

    if (s_nav.ptr_pressed) {
        ldraw_footer_btn_t fb[SM_FOOT_MAX];
        char fstore[SM_FOOT_MAX][16];
        int  fids[SM_FOOT_MAX];
        int fn = sm_footer_layout(fb, fstore, fids);
        for (int i = 0; i < fn; i++) {
            if (!LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, fb[i].rect))
                continue;

            if (fids[i] == SM_FOOT_BACK) {
                if (s_page == SM_PAGE_HUB) SuspendMenu_Close();
                else { s_page = SM_PAGE_HUB; s_focus = 0; s_row_scroll = 0;
                       s_dd_row = -1; s_btn_focus = 0; }
            } else if (fids[i] == SM_FOOT_DEFAULTS) {
                Input_ResetBindings();
            } else {
                in |= LNAV_ACCEPT;
                break;
            }
            return;
        }
    }

    if (s_page == SM_PAGE_CONTROLS && s_nav.ptr_pressed) {
        for (int i = 0; i < nrows; i++) {
            SDL_Rect br[2];
            char     bl[2][24];
            int nb = sm_bind_buttons(i, br, bl);
            for (int k = 0; k < nb; k++) {
                if (!LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, br[k]))
                    continue;
                s_focus     = i;
                s_btn_focus = k;
                s_cap_row   = i;
                s_cap_kind  = (k == SM_BIND_KEY) ? SM_CAP_KEY : SM_CAP_PAD;
                s_cap_pad_clear = 0;
                return;
            }
        }
    }

    if (s_page == SM_PAGE_STATES && s_nav.ptr_pressed) {
        for (int i = 0; i < nrows; i++) {
            SDL_Rect br[3];
            const char *bl[3];
            int nb = sm_slot_buttons(i, br, bl);
            for (int k = 0; k < nb; k++) {
                if (!LauncherDraw_PointInRect(s_nav.ptr_x, s_nav.ptr_y, br[k]))
                    continue;
                s_focus = i;
                s_btn_focus = k;

                if (bl[k][0] == 'O')      { s_confirm_kind = SM_ASK_OVERWRITE;
                                            s_confirm_slot = i; s_confirm_yes = 0; }
                else if (bl[k][0] == 'D') { s_confirm_kind = SM_ASK_DELETE;
                                            s_confirm_slot = i; s_confirm_yes = 0; }
                else if (bl[k][0] == 'S')  sm_slot_action(i, SM_SLOT_SAVE);
                else                       sm_slot_action(i, SM_SLOT_LOAD);
                return;
            }
        }
    }

    if (!(in & (LNAV_CANCEL | LNAV_BACK))) s_close_armed = 1;
    else if (s_close_armed) {

        if (s_page != SM_PAGE_HUB) {
            s_page       = -1;
            s_focus      = 0;
            s_dd_row     = -1;
            s_cap_row    = -1;
            s_row_scroll = 0;
        } else {
            SuspendMenu_Close();
        }
        return;
    }

    if (in & LNAV_ACCEPT) {
        if (s_page == SM_PAGE_HUB) {

            if (kRows[s_focus].page == SM_PAGE_HUB) { SuspendMenu_Close(); return; }
            if (kRows[s_focus].page == SM_PAGE_EXIT) {

                s_confirm_kind = SM_ASK_EXIT;
                s_confirm_slot = 0;
                s_confirm_yes  = 0;
                return;
            }
            s_page       = kRows[s_focus].page;
            s_focus      = 0;
            s_row_scroll = 0;
            s_state_msg[0] = 0;
            s_state_msg_slot = -1;
            s_confirm_slot = -1;
            s_btn_focus = 0;

            s_close_armed = 0;
        } else if (s_page == SM_PAGE_CONTROLS) {
            s_cap_row   = s_focus;
            s_cap_kind  = (s_btn_focus == SM_BIND_KEY) ? SM_CAP_KEY : SM_CAP_PAD;
            s_cap_pad_clear = 0;
        } else if (s_page == SM_PAGE_STATES) {

            SDL_Rect br[3];
            const char *bl[3];
            int nb = sm_slot_buttons(s_focus, br, bl);
            int k  = (s_btn_focus < nb) ? s_btn_focus : 0;
            if (nb > 0) {
                if (bl[k][0] == 'O')      { s_confirm_kind = SM_ASK_OVERWRITE;
                                            s_confirm_slot = s_focus; s_confirm_yes = 0; }
                else if (bl[k][0] == 'D') { s_confirm_kind = SM_ASK_DELETE;
                                            s_confirm_slot = s_focus; s_confirm_yes = 0; }
                else if (bl[k][0] == 'S')  sm_slot_action(s_focus, SM_SLOT_SAVE);
                else                       sm_slot_action(s_focus, SM_SLOT_LOAD);
            }
        } else {

            int id = PresentationMenu_RowId(s_page, s_focus);
            int n  = PresentationMenu_ChoiceCount(id);
            if (id >= 0 && n > 0 && PresentationMenu_RowAvailable(id) &&
                LauncherNav_Device(&s_nav) == LNAV_INPUT_POINTER) {
                s_dd_row   = s_focus;
                s_dd_focus = PresentationMenu_CurrentIndex(id);
                s_dd_swallow_release = 1;
                s_dd_scroll = 0;
            }
        }
    }
}

void SuspendMenu_Tick(void) {
    if (!s_open) return;
    if (s_closing) {

        if (s_dock <= 0) sm_finish_close();
        return;
    }
    sm_tick_inner();
    sm_consume_pointer_edges();

    PresentationMenu_RefreshNow();
}
