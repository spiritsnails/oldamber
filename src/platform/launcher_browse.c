
#include "launcher_browse.h"
#include "launcher_draw.h"
#include "launcher_native.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

#define MAX_NODES     3072
#define MAX_ROOTS     32
#define MAX_CHILDREN  1024
#define NAME_MAX_LEN  128
#define PATH_MAX_LEN  1024

#define DOTS_W    28
#define DOTS_GAP  4

#define ROW_H     22
#define LIST_TOP  70
#define LIST_ROWS 12
#define LIST_X    24
#define LIST_W    (LDRAW_W - 48)
#define INDENT_PX 16

typedef struct {
    char      name[NAME_MAX_LEN];
    int       parent;
    int       depth;
    int       is_dir;
    int       expanded;
    int       loaded;
    int       first_child;
    int       next_sibling;
    long long size;
} node_t;

static node_t s_nodes[MAX_NODES];
static int    s_node_count;
static int    s_roots[MAX_ROOTS];
static int    s_root_count;
static char   s_root_paths[MAX_ROOTS][PATH_MAX_LEN];

static int is_sep(char c) { return c == '/' || c == '\\'; }

static void path_join(char *out, size_t out_sz, const char *dir, const char *name) {
    size_t n = strlen(dir);
    if (n && is_sep(dir[n - 1]))
        snprintf(out, out_sz, "%s%s", dir, name);
    else
        snprintf(out, out_sz, "%s%c%s", dir, PATH_SEP, name);
}

static int node_alloc(void) {
    if (s_node_count >= MAX_NODES) return -1;
    int i = s_node_count++;
    memset(&s_nodes[i], 0, sizeof(s_nodes[i]));
    s_nodes[i].parent = s_nodes[i].first_child = s_nodes[i].next_sibling = -1;
    return i;
}

static void node_path(int idx, char *out, size_t out_sz) {
    const char *chain[64];
    int n = 0;
    int i = idx;
    while (i >= 0 && s_nodes[i].parent >= 0 && n < 64) {
        chain[n++] = s_nodes[i].name;
        i = s_nodes[i].parent;
    }

    const char *base = "";
    for (int r = 0; r < s_root_count; r++)
        if (s_roots[r] == i) { base = s_root_paths[r]; break; }

    snprintf(out, out_sz, "%s", base);
    for (int k = n - 1; k >= 0; k--) {
        char tmp[PATH_MAX_LEN];
        path_join(tmp, sizeof(tmp), out, chain[k]);
        snprintf(out, out_sz, "%s", tmp);
    }
}

static int ext_matches(const char *name, const char *const *exts) {
    if (!exts) return 1;
    const char *dot = strrchr(name, '.');
    if (!dot || !dot[1]) return 0;
    for (const char *const *e = exts; *e; e++) {
        const char *a = dot + 1, *b = *e;
        while (*a && *b) {
            char ca = *a;
            if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
            if (ca != *b) break;
            a++; b++;
        }
        if (!*a && !*b) return 1;
    }
    return 0;
}

typedef struct {
    char      name[NAME_MAX_LEN];
    int       is_dir;
    long long size;
} scratch_t;

static int scratch_cmp(const void *pa, const void *pb) {
    const scratch_t *a = (const scratch_t *)pa, *b = (const scratch_t *)pb;
    if (a->is_dir != b->is_dir) return b->is_dir - a->is_dir;
    const char *x = a->name, *y = b->name;
    for (;; x++, y++) {
        char cx = *x, cy = *y;
        if (cx >= 'A' && cx <= 'Z') cx = (char)(cx - 'A' + 'a');
        if (cy >= 'A' && cy <= 'Z') cy = (char)(cy - 'A' + 'a');
        if (cx != cy) return (unsigned char)cx - (unsigned char)cy;
        if (!cx) return 0;
    }
}

static void node_load(int idx, const char *const *exts) {
    if (s_nodes[idx].loaded || !s_nodes[idx].is_dir) return;
    s_nodes[idx].loaded = 1;

    char dir[PATH_MAX_LEN];
    node_path(idx, dir, sizeof(dir));

    DIR *dp = opendir(dir);
    if (!dp) return;

    static scratch_t scratch[MAX_CHILDREN];
    int n = 0;
    struct dirent *de;
    while ((de = readdir(dp)) != NULL && n < MAX_CHILDREN) {
        if (de->d_name[0] == '.') continue;

        char full[PATH_MAX_LEN];
        path_join(full, sizeof(full), dir, de->d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;
        int isdir = (st.st_mode & S_IFDIR) ? 1 : 0;
        if (!isdir && !ext_matches(de->d_name, exts)) continue;

        snprintf(scratch[n].name, NAME_MAX_LEN, "%s", de->d_name);
        scratch[n].is_dir = isdir;
        scratch[n].size   = isdir ? 0 : (long long)st.st_size;
        n++;
    }
    closedir(dp);
    qsort(scratch, (size_t)n, sizeof(scratch_t), scratch_cmp);

    int prev = -1;
    for (int i = 0; i < n; i++) {
        int c = node_alloc();
        if (c < 0) break;
        snprintf(s_nodes[c].name, NAME_MAX_LEN, "%s", scratch[i].name);
        s_nodes[c].parent  = idx;
        s_nodes[c].depth   = s_nodes[idx].depth + 1;
        s_nodes[c].is_dir  = scratch[i].is_dir;
        s_nodes[c].size    = scratch[i].size;
        if (prev < 0) s_nodes[idx].first_child = c;
        else          s_nodes[prev].next_sibling = c;
        prev = c;
    }
}

static int add_root(const char *path, const char *label) {
    if (s_root_count >= MAX_ROOTS) return 0;
    struct stat st;
    if (stat(path, &st) != 0 || !(st.st_mode & S_IFDIR)) return 0;

    int idx = node_alloc();
    if (idx < 0) return 0;
    snprintf(s_nodes[idx].name, NAME_MAX_LEN, "%s", label);
    s_nodes[idx].parent = -1;
    s_nodes[idx].depth  = 0;
    s_nodes[idx].is_dir = 1;

    if (s_root_count > 0)
        s_nodes[s_roots[s_root_count - 1]].next_sibling = idx;

    snprintf(s_root_paths[s_root_count], PATH_MAX_LEN, "%s", path);
    s_roots[s_root_count++] = idx;
    return 1;
}

static void build_roots(void) {
    char buf[PATH_MAX_LEN];
#ifdef _WIN32
    const char *home = getenv("USERPROFILE");
    if (home) {
        snprintf(buf, sizeof(buf), "%s\\Downloads", home); add_root(buf, "DOWNLOADS");
        add_root(home, "HOME");
    }

    for (char d = 'C'; d <= 'Z'; d++) {
        char probe[8], label[16];
        snprintf(probe, sizeof(probe), "%c:\\", d);
        snprintf(label, sizeof(label), "DRIVE %c:", d);
        add_root(probe, label);
    }
#else
    const char *home = getenv("HOME");
    if (home) {
        static const char *kSub[] = { "Downloads", "Games", "roms", "ROMs",
                                      "Emulation", NULL };
        for (const char **s = kSub; *s; s++) {
            snprintf(buf, sizeof(buf), "%s/%s", home, *s);
            add_root(buf, *s);
        }
        add_root(home, "HOME");
    }

    add_root("/run/media", "REMOVABLE MEDIA");
    add_root("/media", "MEDIA");
    add_root("/mnt", "MNT");
    add_root("/", "FILESYSTEM");
#endif
}

static int flatten(int *out, int max) {
    int n = 0;
    int idx = (s_root_count > 0) ? s_roots[0] : -1;

    while (idx >= 0 && n < max) {
        out[n++] = idx;

        if (s_nodes[idx].expanded && s_nodes[idx].first_child >= 0) {
            idx = s_nodes[idx].first_child;
            continue;
        }

        while (idx >= 0 && s_nodes[idx].next_sibling < 0)
            idx = s_nodes[idx].parent;
        if (idx >= 0) idx = s_nodes[idx].next_sibling;
    }
    return n;
}

static void format_size(long long bytes, char *out, size_t out_sz) {
    if (bytes < 0) bytes = 0;
    if (bytes >= 1024 * 1024) {
        long mb    = (long)(bytes / (1024 * 1024));
        long tenth = (long)(((bytes % (1024 * 1024)) * 10) / (1024 * 1024));
        if (bytes / (1024 * 1024) > 0x7FFFFFFFL) { mb = 0x7FFFFFFFL; tenth = 9; }
        snprintf(out, out_sz, "%ld.%ld MB", mb, tenth);
    } else if (bytes >= 1024) {
        snprintf(out, out_sz, "%ld KB", (long)(bytes / 1024));
    } else {
        snprintf(out, out_sz, "%ld B", (long)bytes);
    }
}

static void draw_arrow(SDL_Renderer *r, int x, int y, int down,
                       Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 0xFF);
    if (down) {
        for (int i = 0; i < 4; i++)
            SDL_RenderDrawLine(r, x + i, y + i, x + 6 - i, y + i);
    } else {
        for (int i = 0; i < 4; i++)
            SDL_RenderDrawLine(r, x + i, y + i, x + i, y + 6 - i);
    }
}

static int goto_typed_dir(const char *path, const char *const *exts) {
    if (!path || !path[0]) return -1;

    char clean[PATH_MAX_LEN];
    snprintf(clean, sizeof(clean), "%s", path);
    size_t n = strlen(clean);
    while (n > 1 && is_sep(clean[n - 1]) && clean[n - 2] != ':')
        clean[--n] = '\0';

    struct stat st;
    if (stat(clean, &st) != 0 || !(st.st_mode & S_IFDIR)) return -1;
    if (!add_root(clean, clean)) return -1;

    int idx = s_roots[s_root_count - 1];
    node_load(idx, exts);
    s_nodes[idx].expanded = 1;
    return idx;
}

int LauncherBrowse_Run(SDL_Renderer *r, SDL_Window *win, launcher_nav_t *nav,
                       const char *title, const char *const *exts,
                       const char *start_dir, char *out_path, size_t out_sz) {
    static int visible[MAX_NODES];

    s_node_count = 0;
    s_root_count = 0;
    memset(s_nodes, 0, sizeof(s_nodes));
    build_roots();

    if (start_dir && start_dir[0]) add_root(start_dir, start_dir);

    int sel = 0, top = 0, picked = 0, running = 1;

    enum { FBTN_CANCEL = 0, FBTN_OPEN, FBTN_N };
    ldraw_footer_btn_t fbtn[FBTN_N];
    fbtn[FBTN_CANCEL].label = "CANCEL";
    fbtn[FBTN_OPEN].label   = "OPEN";

    int  editing = 0;
    char edit_buf[PATH_MAX_LEN] = "";
    size_t edit_len = 0;

    SDL_Rect pathbar = { 24, 44, LDRAW_W - 48 - DOTS_W - DOTS_GAP, 20 };
    SDL_Rect dots    = { pathbar.x + pathbar.w + DOTS_GAP, 44, DOTS_W, 20 };

    while (running) {
        SDL_Event ev;
        int typed_go = 0, typed_abandon = 0;
        nav->ptr_moved = nav->ptr_pressed = nav->ptr_released = 0;
        while (SDL_PollEvent(&ev)) {

            if (editing) {
                if (ev.type == SDL_TEXTINPUT) {
                    for (const char *c = ev.text.text; *c; c++)
                        if (edit_len + 1 < sizeof(edit_buf))
                            edit_buf[edit_len++] = *c;
                    edit_buf[edit_len] = '\0';
                } else if (ev.type == SDL_KEYDOWN) {
                    SDL_Keycode k = ev.key.keysym.sym;
                    if (k == SDLK_BACKSPACE && edit_len > 0)
                        edit_buf[--edit_len] = '\0';
                    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER)
                        typed_go = 1;
                    else if (k == SDLK_ESCAPE)
                        typed_abandon = 1;
                }
            }
            LauncherNav_HandleEvent(nav, &ev, r);
        }

        unsigned in = LauncherNav_Poll(nav);
        if (in & LNAV_QUIT) { picked = 0; break; }

        if (editing) in = 0;

        int n_items = flatten(visible, MAX_NODES);
        int pointer = (LauncherNav_Device(nav) == LNAV_INPUT_POINTER);

        LauncherDraw_FooterLayout(fbtn, FBTN_N);
        int fhover = -1;
        if (pointer)
            for (int i = 0; i < FBTN_N; i++)
                if (LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, fbtn[i].rect))
                    fhover = i;

        int show_dots  = pointer && LauncherNative_HasFileDialog();
        int dots_hover = show_dots &&
                         LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, dots);
        int bar_hover  = pointer &&
                         LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, pathbar);

        if (nav->ptr_pressed && dots_hover) {

            char native_path[PATH_MAX_LEN];
            if (LauncherNative_BrowseFile(win, title,
                                          "Game Boy ROMs|*.gb;*.gbc|All files|*.*|",
                                          native_path, sizeof(native_path))) {
                snprintf(out_path, out_sz, "%s", native_path);
                picked = 1;
                running = 0;
            }

            nav->ptr_pressed = 0;
        } else if (nav->ptr_pressed && bar_hover && !editing) {

            if (n_items > 0) {
                char p[PATH_MAX_LEN];
                node_path(visible[sel], p, sizeof(p));
                snprintf(edit_buf, sizeof(edit_buf), "%s", p);
            } else {
                edit_buf[0] = '\0';
            }
            edit_len = strlen(edit_buf);
            editing = 1;
            SDL_StartTextInput();
            nav->ptr_pressed = 0;
        } else if (nav->ptr_pressed && editing && !bar_hover) {
            typed_abandon = 1;
        }

        if (typed_abandon) {
            editing = 0;
            SDL_StopTextInput();
        } else if (typed_go) {
            int idx = goto_typed_dir(edit_buf, exts);
            if (idx >= 0) {
                editing = 0;
                SDL_StopTextInput();
                n_items = flatten(visible, MAX_NODES);
                for (int i = 0; i < n_items; i++)
                    if (visible[i] == idx) { sel = i; top = i; break; }
            } else {

                struct stat st;
                if (stat(edit_buf, &st) == 0 && !(st.st_mode & S_IFDIR)) {
                    snprintf(out_path, out_sz, "%s", edit_buf);
                    picked = 1;
                    running = 0;
                    editing = 0;
                    SDL_StopTextInput();
                }

            }
        }

        SDL_Rect list_hit = { LIST_X, LIST_TOP, LIST_W, LIST_ROWS * ROW_H };
        if (nav->ptr_moved &&
            LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, list_hit)) {
            int row = (nav->ptr_y - LIST_TOP) / ROW_H;
            if (row >= 0 && row < LIST_ROWS && top + row < n_items)
                sel = top + row;
        }

        if (in & LNAV_UP)        sel--;
        if (in & LNAV_DOWN)      sel++;
        if (in & LNAV_PAGE_UP)   sel -= LIST_ROWS;
        if (in & LNAV_PAGE_DOWN) sel += LIST_ROWS;
        if (sel >= n_items) sel = n_items - 1;
        if (sel < 0)        sel = 0;

        int fclick = (nav->ptr_pressed && fhover >= 0) ? fhover : -1;

        int accept = (in & LNAV_ACCEPT) || fclick == FBTN_OPEN ||
                     (fclick < 0 && nav->ptr_pressed &&
                      LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, list_hit));

        if (accept && n_items > 0) {
            int idx = visible[sel];
            if (s_nodes[idx].is_dir) {

                if (!s_nodes[idx].expanded) {
                    node_load(idx, exts);
                    s_nodes[idx].expanded = 1;
                } else {
                    s_nodes[idx].expanded = 0;
                }
            } else {
                node_path(idx, out_path, out_sz);
                picked = 1;
                running = 0;
            }
        }

        if ((in & LNAV_BACK) && n_items > 0) {
            int idx = visible[sel];
            if (s_nodes[idx].is_dir && s_nodes[idx].expanded) {
                s_nodes[idx].expanded = 0;
            } else if (s_nodes[idx].parent >= 0) {
                int p = s_nodes[idx].parent;
                s_nodes[p].expanded = 0;
                int fresh = flatten(visible, MAX_NODES);
                for (int i = 0; i < fresh; i++)
                    if (visible[i] == p) { sel = i; break; }
            }
        }
        if ((in & LNAV_CANCEL) || fclick == FBTN_CANCEL) { picked = 0; running = 0; }

        n_items = flatten(visible, MAX_NODES);
        if (sel >= n_items) sel = n_items > 0 ? n_items - 1 : 0;
        if (sel < top)              top = sel;
        if (sel >= top + LIST_ROWS) top = sel - LIST_ROWS + 1;
        if (top < 0) top = 0;

        SDL_SetRenderDrawColor(r, LCOL_BG, 0xFF);
        SDL_RenderClear(r);

        {
            int tw = LauncherDraw_TextWidthBold(2, title);
            LauncherDraw_TextBold(r, (LDRAW_W - tw) / 2, 18, 2, LCOL_TEXT, title);
        }

        LauncherDraw_Bevel(r, pathbar, 0);
        if (editing) {

            const char *shown = edit_buf;
            int avail = pathbar.w - 16;

            while (*shown && LauncherDraw_TextWidthBold(1, shown) > avail) shown++;
            int tw = LauncherDraw_TextWidthBold(1, shown);
            LauncherDraw_TextBold(r, pathbar.x + 5,
                                  LDRAW_TEXT_Y(pathbar.y, pathbar.h, 1),
                                  1, LCOL_TEXT, shown);

            if ((SDL_GetTicks() / 500) % 2 == 0) {
                SDL_Rect caret = { pathbar.x + 6 + tw, pathbar.y + 5, 1, 10 };
                SDL_SetRenderDrawColor(r, LCOL_TEXT, 0xFF);
                SDL_RenderFillRect(r, &caret);
            }
        } else if (n_items > 0) {
            char p[PATH_MAX_LEN];
            node_path(visible[sel], p, sizeof(p));
            LauncherDraw_TextClippedBold(r, pathbar.x + 5,
                                         LDRAW_TEXT_Y(pathbar.y, pathbar.h, 1), 1,
                                         LCOL_TEXT, p, pathbar.w - 10);
        }

        if (show_dots) {
            LauncherDraw_Bevel(r, dots, 1);
            if (dots_hover) {
                SDL_Rect inner = { dots.x + 2, dots.y + 2, dots.w - 4, dots.h - 4 };
                LauncherDraw_FocusBar(r, inner);
            }
            Uint8 t = dots_hover ? 0xFF : 0x00;
            LauncherDraw_TextBold(r,
                              dots.x + (dots.w - LauncherDraw_TextWidthBold(1, "...")) / 2,
                              LDRAW_TEXT_Y(dots.y, dots.h, 1), 1, t, t, t, "...");
        }

        SDL_Rect frame = { LIST_X - 4, LIST_TOP - 4, LIST_W + 8,
                           LIST_ROWS * ROW_H + 8 };
        LauncherDraw_Bevel(r, frame, 0);

        int hl_row = -1;
        if (LauncherNav_HoverHighlight(nav)) {
            if (LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, list_hit))
                hl_row = (nav->ptr_y - LIST_TOP) / ROW_H;
        } else {
            hl_row = sel - top;
        }

        for (int i = 0; i < LIST_ROWS; i++) {
            int vi = top + i;
            if (vi >= n_items) break;
            int idx = visible[vi];
            SDL_Rect row = { LIST_X, LIST_TOP + i * ROW_H, LIST_W, ROW_H };

            int focused = (i == hl_row);
            if (focused) LauncherDraw_FocusBar(r, row);
            Uint8 tc = focused ? 0xFF : 0x00;

            int x = row.x + 6 + s_nodes[idx].depth * INDENT_PX;
            if (s_nodes[idx].is_dir) {
                draw_arrow(r, x, row.y + 7, s_nodes[idx].expanded, tc, tc, tc);
                x += 12;
            } else {
                x += 12;
            }

            char label[NAME_MAX_LEN + 8];
            snprintf(label, sizeof(label), "%s%s", s_nodes[idx].name,
                     s_nodes[idx].is_dir ? "/" : "");

            int label_w = (row.x + LIST_W - 110) - x;
            LauncherDraw_TextClippedBold(r, x, LDRAW_TEXT_Y(row.y, ROW_H, 2),
                                         2, tc, tc, tc,
                                         label, label_w > 0 ? label_w : 0);

            if (!s_nodes[idx].is_dir && s_nodes[idx].size > 0) {
                char sz[32];
                format_size(s_nodes[idx].size, sz, sizeof(sz));
                Uint8 dc = focused ? 0xFF : 0x60;
                LauncherDraw_TextBold(r,
                                  row.x + LIST_W - 8 - LauncherDraw_TextWidthBold(1, sz),
                                  LDRAW_TEXT_Y(row.y, ROW_H, 1), 1, dc, dc, dc, sz);
            }

            if (s_nodes[idx].is_dir && s_nodes[idx].expanded &&
                s_nodes[idx].first_child < 0) {
                Uint8 dc = focused ? 0xFF : 0x60;
                LauncherDraw_TextBold(r, row.x + LIST_W - 8 -
                                     LauncherDraw_TextWidthBold(1, "EMPTY"),
                                  LDRAW_TEXT_Y(row.y, ROW_H, 1), 1, dc, dc, dc,
                                  "EMPTY");
            }
        }

        if (pointer) LauncherDraw_FooterButtons(r, fbtn, FBTN_N, fhover);
        else         LauncherDraw_PromptBar(r, "OPEN", "CLOSE", "CANCEL", NULL);
        SDL_RenderPresent(r);
        SDL_Delay(16);
    }

    return picked;
}
