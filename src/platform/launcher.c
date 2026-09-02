
#include "launcher.h"
#include "launcher_font.h"
#include "launcher_draw.h"
#include "launcher_nav.h"
#include "launcher_browse.h"
#include "launcher_save_editor.h"
#include "rom_import.h"
#include "display.h"
#include "save.h"
#include "game_version.h"
#include "steam_shortcut.h"
#include "hardware.h"
#include "data_dir.h"
#include "app_version.h"
#include "update.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

#define DESKTOP_MAX_SCALE 4

static int desktop_scale(void) {
    SDL_Rect b;

    if (SDL_GetDisplayBounds(0, &b) != 0 || b.w <= 0 || b.h <= 0)
        return 1;
    int sx = (b.w * 85 / 100) / LDRAW_W;
    int sy = (b.h * 85 / 100) / LDRAW_H;
    int s  = sx < sy ? sx : sy;
    if (s < 1) s = 1;
    if (s > DESKTOP_MAX_SCALE) s = DESKTOP_MAX_SCALE;
    return s;
}

#define SAVES_BACKUP_DIR "saves_backup"

typedef struct {
    int valid;
    char player_name[16];
    int badges;
    int dex_owned;
} save_preview_t;

static char pokechar_to_ascii(uint8_t c) {
    if (c >= 0x80 && c <= 0x99) return (char)('A' + (c - 0x80));
    if (c >= 0xA0 && c <= 0xB9) return (char)('a' + (c - 0xA0));
    if (c >= 0xF6) return (char)('0' + (c - 0xF6));
    if (c == 0x7F) return ' ';
    return 0;
}

static int count_bits(const uint8_t *buf, int n_bytes) {
    int n = 0;
    for (int i = 0; i < n_bytes; i++)
        for (uint8_t b = buf[i]; b; b >>= 1)
            n += (b & 1);
    return n;
}

static void load_save_preview(const char *path, save_preview_t *out) {
    save_peek_t peek;
    memset(out, 0, sizeof(*out));
    if (Save_PeekFrom(path, &peek) != 0) return;

    int i;
    for (i = 0; i < NAME_LENGTH - 1 && i < (int)sizeof(out->player_name) - 1; i++) {
        char c = pokechar_to_ascii(peek.player_name[i]);
        if (!c) break;
        out->player_name[i] = c;
    }
    out->player_name[i] = '\0';
    out->badges    = count_bits(&peek.badges, 1);
    out->dex_owned = count_bits(peek.pokedex_owned, sizeof(peek.pokedex_owned));
    out->valid     = 1;
}

static void format_save_summary(const save_preview_t *p, char *out, size_t out_sz) {
    if (!p->valid) { snprintf(out, out_sz, "NO SAVE FILE YET"); return; }
    snprintf(out, out_sz, "%s - %d BADGE%s - %d SEEN",
            p->player_name[0] ? p->player_name : "?",
            p->badges, p->badges == 1 ? "" : "S", p->dex_owned);
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return 0;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return 0; }
    char buf[8192];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    fclose(in);
    fclose(out);
    return ok;
}

static int hotswap_save(const char *ver, const char *picked) {
    const char *active = GameVersion_SavePath(ver);
    char backup[1200], relative[128], backup_dir[1200];
    snprintf(relative, sizeof(relative), SAVES_BACKUP_DIR "/%s_prev.sav", ver);
    if (!UserDataPath(relative, backup, sizeof backup))
        snprintf(backup, sizeof backup, "%s", relative);

    FILE *cur = fopen(active, "rb");
    if (cur) {
        fclose(cur);
        if (!UserDataPath(SAVES_BACKUP_DIR, backup_dir, sizeof backup_dir))
            snprintf(backup_dir, sizeof backup_dir, SAVES_BACKUP_DIR);
        LauncherDraw_EnsureDir(backup_dir);
        copy_file(active, backup);
    }
    return copy_file(picked, active);
}

typedef enum { STATE_WAITING, STATE_BUILDING, STATE_ERROR, STATE_READY } ui_state_t;

static const char *const kRomExts[] = { "gb", "gbc", NULL };
static const char *const kSavExts[] = { "sav", NULL };

typedef enum {
    ACT_CHOOSE_ROM = 0, ACT_PLAY, ACT_SWITCH_SAVE, ACT_EDIT_SAVE,
    ACT_UPDATE, ACT_ADD_TO_STEAM, ACT_QUIT
} action_t;

#define MENU_MAX 10

#define PANEL_X       24
#define PANEL_INSET   16

#define DESKTOP_MENU_TOP 168
#define SAVE_LINE_GAP    12
#define SAVE_BOX_H       28

#define DESKTOP_VERSION_Y   (LDRAW_H - 16 - LDRAW_LINE_H(1))
#define DESKTOP_MENU_BOTTOM (DESKTOP_VERSION_Y - 14)

#define ROW_H_SMALL 32
#define ROW_H_GAME  44
#define ROW_H_HERO  56

typedef struct {
    action_t act;
    char     label[40];
    char     ver[16];
    int      h;
    int      scale;

    int      tile;
} menu_row_t;

typedef struct {
    SDL_Renderer *r;
    menu_row_t    rows[MENU_MAX];
    SDL_Rect      rect[MENU_MAX];

    SDL_Rect      save_box;
    int           count;
    int           focus;
} menu_t;

static int nav_step(const menu_t *m, int dx, int dy) {
    if (m->count <= 0) return 0;
    const SDL_Rect *cur = &m->rect[m->focus];
    int cx = cur->x + cur->w / 2, cy = cur->y + cur->h / 2;

    int best = -1, best_score = 0;
    int far  = -1, far_score  = 0;

    for (int i = 0; i < m->count; i++) {
        if (i == m->focus) continue;
        const SDL_Rect *r = &m->rect[i];
        if (r->w <= 0 || r->h <= 0) continue;
        int ix = r->x + r->w / 2, iy = r->y + r->h / 2;
        int along = dx ? (ix - cx) * dx : (iy - cy) * dy;
        int drift = dx ? (iy - cy) : (ix - cx);
        int overlap;
        if (drift < 0) drift = -drift;
        if (along <= 0) continue;

        overlap = dx ? (r->y < cur->y + cur->h && cur->y < r->y + r->h)
                     : (r->x < cur->x + cur->w && cur->x < r->x + r->w);
        if (overlap) {
            if (best < 0 || along < best_score) { best = i; best_score = along; }
        } else {
            int score = along * 4 + drift;
            if (far < 0 || score < far_score) { far = i; far_score = score; }
        }
    }
    if (best >= 0) return best;
    if (far  >= 0) return far;
    return m->focus;
}

static void menu_add(menu_t *m, action_t act, const char *label,
                     const char *ver, int h, int scale) {
    if (m->count >= MENU_MAX) return;
    m->rows[m->count].act   = act;
    m->rows[m->count].h     = h;
    m->rows[m->count].scale = scale;
    m->rows[m->count].tile  = 0;
    snprintf(m->rows[m->count].label, sizeof(m->rows[m->count].label), "%s", label);
    snprintf(m->rows[m->count].ver, sizeof(m->rows[m->count].ver), "%s", ver ? ver : "");
    m->count++;
}

static int s_can_import = 1;

static int s_import_via_setup = 0;

static void menu_build(menu_t *m, ui_state_t state,
                       const char **installed, int n_installed, int pointer) {
    int  prev_act = (m->count > 0 && m->focus < m->count)
                        ? (int)m->rows[m->focus].act : -1;
    char prev_ver[16] = "";
    if (m->count > 0 && m->focus < m->count)
        snprintf(prev_ver, sizeof(prev_ver), "%s", m->rows[m->focus].ver);
    m->count = 0;

    if (state == STATE_READY && n_installed > 0) {
        int hero = (n_installed == 1);
        for (int i = 0; i < n_installed; i++) {
            char lbl[40];
            snprintf(lbl, sizeof(lbl), "PLAY %s", GameVersion_Label(installed[i]));
            menu_add(m, ACT_PLAY, lbl, installed[i],
                     hero ? ROW_H_HERO : ROW_H_GAME, hero ? 3 : 2);
        }

        if (n_installed < GameVersion_SupportedCount() && s_can_import)
            menu_add(m, ACT_CHOOSE_ROM, "ADD ANOTHER GAME", "", ROW_H_SMALL, 2);

        if (s_can_import)
            menu_add(m, ACT_CHOOSE_ROM, "RE-IMPORT A ROM", "", ROW_H_SMALL, 2);
    } else if (s_can_import) {
        menu_add(m, ACT_CHOOSE_ROM, "CHOOSE ROM FILE", "", ROW_H_SMALL, 2);
    }

    menu_add(m, ACT_SWITCH_SAVE, "SWITCH SAVE FILE", "", ROW_H_SMALL, 2);
    menu_add(m, ACT_EDIT_SAVE, "EDIT SAVE FILE", "", ROW_H_SMALL, 2);

    update_snapshot_t update;
    Update_GetSnapshot(&update);
    if (update.state != UPDATE_DISABLED) {
        char label[40];
        if (update.state == UPDATE_CHECKING) snprintf(label, sizeof label, "CHECKING FOR UPDATES...");
        else if (update.state == UPDATE_AVAILABLE) snprintf(label, sizeof label, "UPDATE TO V%s", update.version);
        else if (update.state == UPDATE_DOWNLOADING) snprintf(label, sizeof label, "DOWNLOADING UPDATE...");
        else if (update.state == UPDATE_READY) snprintf(label, sizeof label, "RESTART TO FINISH UPDATE");
        else if (update.state == UPDATE_ERROR) snprintf(label, sizeof label, "RETRY UPDATE CHECK");
        else snprintf(label, sizeof label, "CHECK FOR UPDATES");
        menu_add(m, ACT_UPDATE, label, "", ROW_H_SMALL, 2);
    }

    if (SteamShortcut_Offer())
        menu_add(m, ACT_ADD_TO_STEAM, "ADD TO STEAM", "", ROW_H_SMALL, 2);

    if (!pointer)
        menu_add(m, ACT_QUIT, "QUIT", "", ROW_H_SMALL, 2);

    const int bw = 240, gap = 8, bottom = LDRAW_H - 26 - 16;

    const int menu_top = 178 + LDRAW_LINE_H(1) + 8;

    m->save_box = (SDL_Rect){ 0, 0, 0, 0 };

    {

        int x = (LDRAW_W - bw) / 2;

        for (int attempt = 0; ; attempt++) {
            int row_gap = (attempt >= 3) ? 2 : (attempt >= 2) ? 4 : gap;
            int box_gap = (attempt >= 3) ? 4 : (attempt >= 2) ? 6 : SAVE_LINE_GAP;

            int row_h = (attempt >= 4) ? 26 : ROW_H_SMALL;

            const int tile_gap = 28;
            int y = DESKTOP_MENU_TOP;
            int first = 1;
            int n_game = 0;

            for (int i = 0; i < m->count; i++)
                if (m->rows[i].act == ACT_PLAY) n_game++;

            int base = (n_game <= 1 ? 150 : 108) * LDRAW_W / LDRAW_W_DESKTOP;
            int side = (attempt >= 3) ? base * 63 / 100
                     : (attempt >= 2) ? base * 78 / 100
                     : (attempt >= 1) ? base * 89 / 100 : base;

            if (n_game > 0) {
                int total = n_game * side + (n_game - 1) * tile_gap;
                int tx = (LDRAW_W - total) / 2;
                for (int i = 0; i < m->count; i++) {
                    if (m->rows[i].act != ACT_PLAY) continue;
                    m->rows[i].tile = 1;
                    m->rect[i] = (SDL_Rect){ tx, y, side, side };
                    tx += side + tile_gap;
                }
                y += side;
                first = 0;
            }

            for (int i = 0; i < m->count; i++) {
                if (m->rows[i].act == ACT_SWITCH_SAVE ||
                    m->rows[i].act == ACT_EDIT_SAVE) continue;
                if (m->rows[i].act == ACT_PLAY) continue;
                m->rows[i].tile = 0;
                m->rows[i].h = row_h;
                m->rows[i].scale = 2;
                if (!first) y += row_gap;
                m->rect[i] = (SDL_Rect){ x, y, bw, m->rows[i].h };
                y += m->rows[i].h;
                first = 0;
            }

            y += box_gap;
            m->save_box = (SDL_Rect){ PANEL_X, y, LDRAW_W - PANEL_X * 2, SAVE_BOX_H };
            y += SAVE_BOX_H + box_gap;

            for (int i = 0; i < m->count; i++) {
                if (m->rows[i].act != ACT_SWITCH_SAVE &&
                    m->rows[i].act != ACT_EDIT_SAVE) continue;
                m->rows[i].tile = 0;
                m->rows[i].h = row_h;
                m->rect[i] = (SDL_Rect){ x, y, bw, m->rows[i].h };
                y += m->rows[i].h;
            }

            if (y <= DESKTOP_MENU_BOTTOM || attempt >= 4) break;
        }
    }

    m->focus = 0;
    for (int i = 0; i < m->count && prev_act >= 0; i++)
        if ((int)m->rows[i].act == prev_act &&
            strcmp(m->rows[i].ver, prev_ver) == 0) { m->focus = i; return; }
    for (int i = 0; i < m->count && prev_act >= 0; i++)
        if ((int)m->rows[i].act == prev_act) { m->focus = i; return; }
}

static void draw_centred(SDL_Renderer *r, int x, int span, int y, int scale,
                         Uint8 cr, Uint8 cg, Uint8 cb, const char *s) {
    int w;
    if (!s || !*s) return;
    w = LauncherDraw_TextWidthBold(scale, s);
    if (w <= span)
        LauncherDraw_TextBold(r, x + (span - w) / 2, y, scale, cr, cg, cb, s);
    else
        LauncherDraw_TextClippedBold(r, x, y, scale, cr, cg, cb, s, span);
}

static void row_focus_rgb(const char *ver, Uint8 *cr, Uint8 *cg, Uint8 *cb) {
    if (ver && strcmp(ver, "red") == 0)  { *cr = 0xA0; *cg = 0x00; *cb = 0x00; return; }
    if (ver && strcmp(ver, "blue") == 0) { *cr = 0x00; *cg = 0x28; *cb = 0xC0; return; }
    *cr = 0x00; *cg = 0x00; *cb = 0x80;
}

static void draw_play_glyph(SDL_Renderer *r, int x, int cy, int h,
                            Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 0xFF);
    for (int i = 0; i < h; i++) {

        int half = (i < h / 2) ? i : (h - 1 - i);
        SDL_Rect run = { x, cy - h / 2 + i, half + 1, 1 };
        SDL_RenderFillRect(r, &run);
    }
}

static void draw_button(SDL_Renderer *r, SDL_Rect rect, const char *label,
                        int focused, int scale, const char *ver, int tile) {
    Uint8 fr, fg, fb;
    row_focus_rgb(ver, &fr, &fg, &fb);
    LauncherDraw_Bevel(r, rect, 1);
    if (focused) {
        SDL_Rect inner = { rect.x + 3, rect.y + 3, rect.w - 6, rect.h - 6 };
        LauncherDraw_FocusBarRGB(r, inner, fr, fg, fb);
    }

    if (tile) {
        const char *big = GameVersion_Label(ver);

        int vs = 3, ls = 2;
        int widest = LauncherDraw_TextWidthBold(vs, big);
        for (int vi = 0; ; vi++) {
            const char *lab = GameVersion_LabelAt(vi);
            int w;
            if (!lab) break;
            w = LauncherDraw_TextWidthBold(vs, lab);
            if (w > widest) widest = w;
        }
        if (widest > rect.w - 16) { vs = 2; ls = 1; }
        {
            int lw   = LauncherDraw_TextWidthBold(ls, "PLAY");
            int bwid = LauncherDraw_TextWidthBold(vs, big);
            int gap2 = ls * 3;
            int block = LDRAW_INK_H(ls) + gap2 + LDRAW_INK_H(vs);
            int top = rect.y + (rect.h - block) / 2;
            Uint8 t = focused ? 0xFF : 0x00;

            int gh = LDRAW_INK_H(ls);
            int gw = gh / 2 + 1;
            int pad = ls * 2;
            int lx = rect.x + (rect.w - (gw + pad + lw)) / 2;
            draw_play_glyph(r, lx, top + gh / 2, gh, t, t, t);
            LauncherDraw_TextBold(r, lx + gw + pad, top, ls, t, t, t, "PLAY");
            LauncherDraw_TextBold(r, rect.x + (rect.w - bwid) / 2,
                                  top + LDRAW_INK_H(ls) + gap2, vs, t, t, t, big);
        }
        return;
    }
    Uint8 tr = focused ? 0xFF : 0x00;

    int s = scale;
    while (s > 1 && LauncherDraw_TextWidthBold(s, label) > rect.w - 12) s--;
    LauncherDraw_TextBold(r,
                      rect.x + (rect.w - LauncherDraw_TextWidthBold(s, label)) / 2,
                      LDRAW_TEXT_Y(rect.y, rect.h, s),
                      s, tr, tr, tr, label);
}

static void import_finished(menu_t *m, ui_state_t *state, const char *rom_path,
                            const char **installed, int *n_installed,
                            char *sel_ver, size_t sel_sz, int *preview_focus,
                            int pointer) {
    const char *added = GameVersion_FromRomHeader(rom_path);

    *n_installed = GameVersion_ScanInstalled(installed, GAMEVER_MAX);
    *state = (*n_installed > 0) ? STATE_READY : STATE_ERROR;

    if (added) snprintf(sel_ver, sel_sz, "%s", added);
    menu_build(m, *state, installed, *n_installed, pointer);
    if (added)
        for (int i = 0; i < m->count; i++)
            if (m->rows[i].act == ACT_PLAY && strcmp(m->rows[i].ver, added) == 0) {
                m->focus = i;
                break;
            }
    *preview_focus = -1;
}

static void draw_main(menu_t *m, ui_state_t state, const char *status,
                      int status_err, const char *save_summary, int has_pad,
                      int pointer, int highlight,
                      const char **installed, int n_installed) {
    SDL_Renderer *r = m->r;
    SDL_SetRenderDrawColor(r, LCOL_BG, 0xFF);
    SDL_RenderClear(r);

    draw_centred(r, 24, LDRAW_W - 48, 26, 3, LCOL_TEXT, "OLDAMBER");

    SDL_Rect zone = { 24, 76, LDRAW_W - 48, 62 };
    LauncherDraw_Bevel(r, zone, 0);

    if (state == STATE_READY) {

        char line[128];
        size_t used = 0;
        line[0] = '\0';
        for (int i = 0; i < n_installed; i++)
            used += (size_t)snprintf(line + used, sizeof(line) - used, "%s%s",
                                     i ? " AND " : "", GameVersion_Label(installed[i]));
        draw_centred(r, zone.x + 16, zone.w - 32, zone.y + 14, 2, 0x00, 0x60, 0x00,
                     n_installed > 1 ? "GAMES READY" : "GAME DATA READY");
        draw_centred(r, zone.x + 16, zone.w - 32, zone.y + 38, 1, LCOL_TEXT_DIM,
                     line);
    } else if (has_pad) {

        draw_centred(r, zone.x + 16, zone.w - 32, zone.y + 14, 2, LCOL_TEXT_DIM,
                     "NO GAME DATA YET");
        draw_centred(r, zone.x + 16, zone.w - 32, zone.y + 38, 1, LCOL_TEXT_DIM,
                     "CHOOSE YOUR ROM BELOW TO GET STARTED.");
    } else {
        draw_centred(r, zone.x + 16, zone.w - 32, zone.y + 14, 2, LCOL_TEXT_DIM,
                     "DROP YOUR ROM IN THIS WINDOW");
        draw_centred(r, zone.x + 16, zone.w - 32, zone.y + 38, 1, LCOL_TEXT_DIM,
                     "OR USE THE BUTTON BELOW.");
    }

    if (status && status[0]) {
        Uint8 cr = 0x00, cg = 0x00, cb = 0x00;
        if (status_err)                { cr = 0x80; cg = 0x00; cb = 0x00; }
        else if (state == STATE_READY) { cr = 0x00; cg = 0x60; cb = 0x00; }
        draw_centred(r, 24, LDRAW_W - 48, 150, 2, cr, cg, cb, status);
    }

    int save_x = PANEL_X, save_y = 178;
    if (m->save_box.w > 0) {
        LauncherDraw_Bevel(r, m->save_box, 0);
        save_x = m->save_box.x + PANEL_INSET;
        save_y = LDRAW_TEXT_Y(m->save_box.y, m->save_box.h, 1);
    }

    {
        int span  = (m->save_box.w > 0) ? m->save_box.w - PANEL_INSET * 2
                                        : LDRAW_W - PANEL_X * 2;
        int lw    = LauncherDraw_TextWidthBold(1, "CURRENT SAVE: ");
        int vw    = LauncherDraw_TextWidthBold(1, save_summary ? save_summary : "");
        int total = lw + vw;
        int lx    = save_x + (total < span ? (span - total) / 2 : 0);
        LauncherDraw_TextBold(r, lx, save_y, 1, LCOL_TEXT_DIM, "CURRENT SAVE:");
        LauncherDraw_TextClippedBold(r, lx + lw, save_y, 1, LCOL_TEXT,
                                     save_summary, save_x + span - (lx + lw));
    }

    for (int i = 0; i < m->count; i++)
        draw_button(r, m->rect[i], m->rows[i].label, i == highlight,
                    m->rows[i].scale, m->rows[i].ver, m->rows[i].tile);

    if (!pointer) {
        LauncherDraw_PromptBar(r, "SELECT", NULL, NULL, NULL);

        char ver[64];
        snprintf(ver, sizeof(ver), "%s - V%s", OLDAMBER_NAME, OLDAMBER_VERSION);
        LauncherDraw_Text(r, LDRAW_W - PANEL_X - LauncherDraw_TextWidth(1, ver),
                          LDRAW_TEXT_Y(LDRAW_H - LDRAW_FOOTER_H, LDRAW_FOOTER_H, 1),
                          1, LCOL_TEXT_DIM, ver);
    } else {

        char ver[64];
        int lowest = 0;
        for (int i = 0; i < m->count; i++) {
            int bottom = m->rect[i].y + m->rect[i].h;
            if (bottom > lowest) lowest = bottom;
        }
        if (lowest <= DESKTOP_VERSION_Y - 4) {
            snprintf(ver, sizeof(ver), "%s - V%s", OLDAMBER_NAME, OLDAMBER_VERSION);
            draw_centred(r, PANEL_X, LDRAW_W - PANEL_X * 2, DESKTOP_VERSION_Y, 1,
                         LCOL_TEXT_DIM, ver);
        }
    }
    SDL_RenderPresent(r);
}

static void draw_notice(menu_t *m, const char *title,
                        const char *const *lines, int n_lines, int pointer) {
    SDL_Renderer *r = m->r;
    SDL_SetRenderDrawColor(r, LCOL_BG, 0xFF);
    SDL_RenderClear(r);

    int block = LDRAW_INK_H(3) + 18 + n_lines * (LDRAW_LINE_H(1) + 4);
    int y = (LDRAW_H - block) / 2 - 8;
    if (y < 24) y = 24;

    draw_centred(r, 24, LDRAW_W - 48, y, 3, LCOL_TEXT, title);
    y += LDRAW_INK_H(3) + 18;
    for (int i = 0; i < n_lines; i++) {
        draw_centred(r, 24, LDRAW_W - 48, y, 1, LCOL_TEXT_DIM, lines[i]);
        y += LDRAW_LINE_H(1) + 4;
    }

    if (!pointer)
        LauncherDraw_PromptBar(r, "CONTINUE", NULL, NULL, NULL);
    else
        draw_centred(r, 24, LDRAW_W - 48, y + 16, 1, LCOL_TEXT_DIM,
                     "CLICK OR PRESS ENTER TO CONTINUE");

    SDL_RenderPresent(r);
}

static void draw_stage(menu_t *m, const char *stage, int step, int of) {
    SDL_Renderer *r = m->r;
    SDL_SetRenderDrawColor(r, LCOL_BG, 0xFF);
    SDL_RenderClear(r);

    draw_centred(r, 24, LDRAW_W - 48, 26, 3, LCOL_TEXT, "BUILDING GAME DATA");
    draw_centred(r, 24, LDRAW_W - 48, 76, 1, LCOL_TEXT_DIM,
                 "THIS TAKES A FEW MINUTES THE FIRST TIME.");
    draw_centred(r, 24, LDRAW_W - 48, 92, 1, LCOL_TEXT_DIM,
                 "DO NOT CLOSE THE GAME WHILE THIS RUNS.");

    draw_centred(r, 24, LDRAW_W - 48, 140, 2, LCOL_TEXT, stage);

    SDL_Rect bar = { 24, 176, LDRAW_W - 48, 22 };
    LauncherDraw_Bevel(r, bar, 0);
    if (of > 0) {
        int fill = ((bar.w - 8) * step) / of;
        SDL_Rect f = { bar.x + 4, bar.y + 4, fill, bar.h - 8 };
        SDL_SetRenderDrawColor(r, LCOL_FOCUS, 0xFF);
        SDL_RenderFillRect(r, &f);
        char lbl[32];
        snprintf(lbl, sizeof(lbl), "STEP %d OF %d", step, of);
        draw_centred(r, bar.x, bar.w, bar.y + bar.h + 6, 1, LCOL_TEXT_DIM, lbl);
    }
    SDL_RenderPresent(r);
}

typedef struct { menu_t *m; } stage_ctx_t;

static void on_setup_stage(void *ctx, int stage) {
    stage_ctx_t *s = (stage_ctx_t *)ctx;
    if (!s || !s->m || stage != 2) return;
    draw_stage(s->m, "IMPORTING KANTO MAPS", 2, 2);
}

launcher_result_t Launcher_Run(const char *tools_dir, const char *out_pak_path,
                               const char *romimport_tools_dir,
                               char *chosen_version, size_t chosen_sz) {

    s_import_via_setup = (tools_dir == NULL) && RomImport_HaveBundledSetup();
    s_can_import = (tools_dir != NULL) || s_import_via_setup;

    {
        char setup_path[1200];
        if (tools_dir)
            fprintf(stderr, "[launcher] import: tools/ (%s)\n", tools_dir);
        else if (s_import_via_setup &&
                 RomImport_BundledSetupPath(setup_path, sizeof setup_path))
            fprintf(stderr, "[launcher] import: bundled setup (%s)\n", setup_path);
        else
            fprintf(stderr, "[launcher] import: UNAVAILABLE -- no tools/ and no "
                            "setup binary beside the game; the ROM picker will "
                            "not be offered\n");
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "launcher: SDL_Init failed: %s\n", SDL_GetError());
        return LAUNCHER_CANCELLED;
    }

    int fullscreen = Display_IsSteamDeck() || Display_IsFullscreen();

    LauncherDraw_SetWidth(fullscreen ? LDRAW_W_DECK : LDRAW_W_DESKTOP);
    int dscale = desktop_scale();
    int win_w = LDRAW_W * dscale, win_h = LDRAW_H * dscale;
    int win_x = SDL_WINDOWPOS_CENTERED, win_y = SDL_WINDOWPOS_CENTERED;

    Uint32 win_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

    if (fullscreen) {
#ifdef _WIN32
        win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#else

        if (Display_IsSteamDeck()) {
            win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        } else {
            SDL_Rect b;
            if (SDL_GetDisplayBounds(0, &b) == 0) {
                win_x = b.x; win_y = b.y; win_w = b.w; win_h = b.h;
            }
            win_flags |= SDL_WINDOW_BORDERLESS;
        }
#endif
    }

    SDL_Window *win = SDL_CreateWindow(
        "OldAmber Setup", win_x, win_y, win_w, win_h, win_flags);
    if (!win) {
        fprintf(stderr, "launcher: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return LAUNCHER_CANCELLED;
    }
    SDL_Renderer *r = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!r) r = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!r) {
        fprintf(stderr, "launcher: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        return LAUNCHER_CANCELLED;
    }

    SDL_RenderSetLogicalSize(r, LDRAW_W, LDRAW_H);
    Update_Init();

#if SDL_VERSION_ATLEAST(2, 0, 5)

    if (!fullscreen) SDL_RenderSetIntegerScale(r, SDL_TRUE);
#endif

    launcher_nav_t nav;
    LauncherNav_Init(&nav);

    const char *installed[GAMEVER_MAX];
    int n_installed = GameVersion_ScanInstalled(installed, GAMEVER_MAX);

    ui_state_t state = (n_installed > 0) ? STATE_READY : STATE_WAITING;
    char status[256] = "";
    int  status_err = 0;

    int  steam_pending = 0;

    int  notice_active = 0;
    const char *notice_title = "";
    const char *notice_lines[3];
    int  notice_n = 0;
    char rom_path[1024] = "";

    menu_t m;
    memset(&m, 0, sizeof(m));
    m.r = r;

    int pointer = (LauncherNav_Device(&nav) == LNAV_INPUT_POINTER);
    SDL_ShowCursor(pointer ? SDL_ENABLE : SDL_DISABLE);
    menu_build(&m, state, installed, n_installed, pointer);
    launcher_result_t result = LAUNCHER_CANCELLED;
    update_snapshot_t update;
    Update_GetSnapshot(&update);
    update_state_t update_seen = update.state;
    int running = 1;

    save_preview_t save_preview;
    char save_summary[128];
    int  preview_focus = -1;

    char sel_ver[16] = "";
    snprintf(sel_ver, sizeof(sel_ver), "%s",
             n_installed > 0 ? installed[0] : GameVersion_Current());

    while (running) {
        SDL_Event ev;
        nav.ptr_moved = nav.ptr_pressed = nav.ptr_released = 0;
        int dropped = 0;
        char drop_path[1024] = "";

        while (SDL_PollEvent(&ev)) {
            LauncherNav_HandleEvent(&nav, &ev, r);
            if (ev.type == SDL_DROPFILE) {
                snprintf(drop_path, sizeof(drop_path), "%s", ev.drop.file);
                dropped = 1;
                SDL_free(ev.drop.file);
            }
        }

        unsigned in = LauncherNav_Poll(&nav);
        if (in & LNAV_QUIT) { running = 0; break; }

        if ((LauncherNav_Device(&nav) == LNAV_INPUT_POINTER) != pointer) {
            pointer = !pointer;
            menu_build(&m, state, installed, n_installed, pointer);
            preview_focus = -1;

            SDL_ShowCursor(pointer ? SDL_ENABLE : SDL_DISABLE);
        }

        Update_GetSnapshot(&update);
        if (update.state != update_seen) {
            update_seen = update.state;
            menu_build(&m, state, installed, n_installed, pointer);
            preview_focus = -1;
            status_err = (update.state == UPDATE_ERROR);
            snprintf(status, sizeof status, "%s", update.message);
        }

        int busy = (state == STATE_BUILDING) || notice_active;

        if (notice_active) {
            if ((in & (LNAV_ACCEPT | LNAV_BACK)) || nav.ptr_pressed)
                notice_active = 0;
        }

        if (!busy && nav.ptr_moved) {
            for (int i = 0; i < m.count; i++)
                if (LauncherDraw_PointInRect(nav.ptr_x, nav.ptr_y, m.rect[i]))
                    m.focus = i;
        }
        if (!busy && m.count > 0) {
            if (in & LNAV_UP)    m.focus = nav_step(&m, 0, -1);
            if (in & LNAV_DOWN)  m.focus = nav_step(&m, 0, +1);
            if (in & LNAV_LEFT)  m.focus = nav_step(&m, -1, 0);
            if (in & LNAV_RIGHT) m.focus = nav_step(&m, +1, 0);
        }

        int activate = 0;
        if (!busy && m.count > 0) {
            if (in & LNAV_ACCEPT) activate = 1;
            if (nav.ptr_pressed &&
                LauncherDraw_PointInRect(nav.ptr_x, nav.ptr_y, m.rect[m.focus]))
                activate = 1;
        }

        if (dropped && !busy) {
            snprintf(rom_path, sizeof(rom_path), "%s", drop_path);
            if (RomImport_LooksLikeGBRom(rom_path)) {
                state = STATE_BUILDING;
                status_err = 0;
                snprintf(status, sizeof(status), "BUILDING GAME DATA...");
            } else {
                state = STATE_ERROR;
                status_err = 1;
                snprintf(status, sizeof(status), "NOT A VALID GAME BOY ROM");
            }
        }

        if (activate) {
            action_t act = m.rows[m.focus].act;
            if (act == ACT_QUIT) {
                running = 0;
                break;
            } else if (act == ACT_PLAY) {

                if (chosen_version && chosen_sz)
                    snprintf(chosen_version, chosen_sz, "%s", m.rows[m.focus].ver);
                result = LAUNCHER_GOT_PAK;
                running = 0;
                break;
            } else if (act == ACT_CHOOSE_ROM) {
                char picked[1024];
                if (LauncherBrowse_Run(r, win, &nav, "CHOOSE YOUR ROM", kRomExts,
                                       NULL, picked, sizeof(picked))) {
                    snprintf(rom_path, sizeof(rom_path), "%s", picked);
                    if (RomImport_LooksLikeGBRom(rom_path)) {
                        state = STATE_BUILDING;
                        status_err = 0;
                        snprintf(status, sizeof(status), "BUILDING GAME DATA...");
                    } else {
                        state = STATE_ERROR;
                        status_err = 1;
                        snprintf(status, sizeof(status), "NOT A VALID GAME BOY ROM");
                    }
                }
            } else if (act == ACT_ADD_TO_STEAM) {

                char err[128];
                if (SteamShortcut_Request(err, sizeof err)) {
                    steam_pending = 1;
                    status_err = 0;
                    snprintf(status, sizeof(status), "ADDING TO STEAM...");
                } else {
                    status_err = 1;
                    snprintf(status, sizeof(status), "%s", err);
                }
            } else if (act == ACT_SWITCH_SAVE) {
                char picked[1024];

                if (LauncherBrowse_Run(r, win, &nav, "CHOOSE A SAVE FILE", kSavExts,
                                       NULL, picked, sizeof(picked))) {
                    if (hotswap_save(sel_ver, picked)) {
                        preview_focus = -1;
                    } else {
                        state = STATE_ERROR;
                        status_err = 1;
                        snprintf(status, sizeof(status), "COULD NOT SWITCH SAVE FILE");
                        fprintf(stderr, "launcher: could not switch to %s\n", picked);
                    }
                }
            } else if (act == ACT_EDIT_SAVE) {
                int edited = LauncherSaveEditor_Run(
                    r, win, &nav, GameVersion_SavePath(sel_ver),
                    GameVersion_Label(sel_ver));
                preview_focus = -1;
                if (edited > 0) {
                    status_err = 0;
                    snprintf(status, sizeof(status), "SAVE CHANGES WRITTEN");
                } else if (edited < 0) {
                    status_err = 1;
                    snprintf(status, sizeof(status), "COULD NOT EDIT SAVE FILE");
                }
            } else if (act == ACT_UPDATE) {
                Update_GetSnapshot(&update);
                if (update.state == UPDATE_AVAILABLE) Update_Install();
                else if (update.state == UPDATE_READY) {
                    result = LAUNCHER_RESTART;
                    running = 0;
                } else if (update.state == UPDATE_ERROR ||
                           update.state == UPDATE_CURRENT ||
                           update.state == UPDATE_IDLE) Update_Check();
            }
        }

        if (steam_pending) {
            int added = SteamShortcut_Poll();
            if (added > 0) {
                steam_pending = 0;
                status_err = 0;
                status[0] = '\0';
                notice_title    = "ADDED TO STEAM";
                notice_lines[0] = "OLDAMBER IS NOW IN YOUR STEAM LIBRARY.";
                notice_lines[1] = "SWITCH TO GAME MODE TO PLAY IT THERE.";
                notice_n = 2;
                notice_active = 1;
                menu_build(&m, state, installed, n_installed, pointer);
                preview_focus = -1;
            } else if (added < 0) {

                steam_pending = 0;
                status_err = 0;
                status[0] = '\0';
                notice_title    = "ASKED STEAM";
                notice_lines[0] = "STEAM TOOK THE REQUEST BUT HAS NOT";
                notice_lines[1] = "CONFIRMED IT YET.";
                notice_lines[2] = "RESTART STEAM, THEN CHECK YOUR LIBRARY.";
                notice_n = 3;
                notice_active = 1;
            }
        }

        if (m.focus != preview_focus) {
            if (m.count > 0 && m.rows[m.focus].ver[0])
                snprintf(sel_ver, sizeof(sel_ver), "%s", m.rows[m.focus].ver);
            load_save_preview(GameVersion_SavePath(sel_ver), &save_preview);
            format_save_summary(&save_preview, save_summary, sizeof(save_summary));
            preview_focus = m.focus;
        }

        if (notice_active) {
            draw_notice(&m, notice_title, notice_lines, notice_n, pointer);
        }
        else if (state != STATE_BUILDING)
        {

            int highlight = m.focus;
            if (LauncherNav_HoverHighlight(&nav)) {
                highlight = -1;
                for (int i = 0; i < m.count; i++)
                    if (LauncherDraw_PointInRect(nav.ptr_x, nav.ptr_y, m.rect[i]))
                        highlight = i;
            }
            draw_main(&m, state, status, status_err, save_summary,
                      nav.pad != NULL, pointer, highlight,
                      installed, n_installed);
        }

        if (state == STATE_BUILDING && s_import_via_setup && rom_path[0]) {

            char err[512];
            stage_ctx_t sc = { &m };

            draw_stage(&m, "READING ROM AND BUILDING ASSETS", 1, 2);
            if (RomImport_RunBundledSetup(rom_path, on_setup_stage, &sc,
                                          err, sizeof(err))) {
                state = STATE_READY;
                import_finished(&m, &state, rom_path, installed, &n_installed,
                                sel_ver, sizeof(sel_ver), &preview_focus, pointer);
                status_err = 0;
                snprintf(status, sizeof(status), "IMPORT COMPLETE.");
            } else {
                state = (n_installed > 0) ? STATE_READY : STATE_ERROR;
                menu_build(&m, state, installed, n_installed, pointer);
                status_err = 1;
                snprintf(status, sizeof(status),
                        "BUILD FAILED - SEE POKERED_LOG.TXT");
                fprintf(stderr, "launcher: setup failed: %s\n", err);
                rom_path[0] = '\0';
            }
        }
        else if (state == STATE_BUILDING && rom_path[0]) {
            char err[512];

            draw_stage(&m, "READING ROM AND BUILDING ASSETS", 1, 2);
            if (!RomImport_BuildPak(rom_path, tools_dir, out_pak_path, err, sizeof(err))) {
                state = (n_installed > 0) ? STATE_READY : STATE_ERROR;
                menu_build(&m, state, installed, n_installed, pointer);
                status_err = 1;
                snprintf(status, sizeof(status),
                        "BUILD FAILED - SEE POKERED_LOG.TXT");
                fprintf(stderr, "launcher: extraction failed: %s\n", err);
                rom_path[0] = '\0';
            } else if (romimport_tools_dir == NULL) {

                state = STATE_READY;
                import_finished(&m, &state, rom_path, installed, &n_installed,
                                sel_ver, sizeof(sel_ver), &preview_focus, pointer);
                status_err = 0;
                snprintf(status, sizeof(status), "IMPORT COMPLETE.");
            } else {
                draw_stage(&m, "IMPORTING KANTO MAPS", 2, 2);
                if (RomImport_EmitKantoMaps(rom_path, romimport_tools_dir,
                                            err, sizeof(err))) {
                    state = STATE_READY;
                    import_finished(&m, &state, rom_path, installed, &n_installed,
                                    sel_ver, sizeof(sel_ver), &preview_focus, pointer);
                    status_err = 0;
                    snprintf(status, sizeof(status), "IMPORT COMPLETE.");
                } else {
                    state = STATE_ERROR;
                    menu_build(&m, state, installed, n_installed, pointer);
                    status_err = 1;
                    snprintf(status, sizeof(status),
                            "MAP IMPORT FAILED - SEE POKERED_LOG.TXT");
                    fprintf(stderr, "launcher: map generation failed: %s\n", err);
                    rom_path[0] = '\0';
                }
            }
        }

        SDL_Delay(16);
    }

    LauncherNav_Close(&nav);
    Update_Shutdown();
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(win);

    return result;
}
