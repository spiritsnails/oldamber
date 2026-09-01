#include "launcher_dropdown.h"
#include "launcher_draw.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define LDROP_CARET_W 18

void LauncherDropdown_DrawCaret(SDL_Renderer *r, SDL_Rect box, int up) {
    const int w = 9;
    const int h = (w + 1) / 2;
    const int x0 = box.x + (box.w - w) / 2;
    const int y0 = box.y + (box.h - h) / 2;
    SDL_SetRenderDrawColor(r, LCOL_TEXT, 0xFF);
    for (int row = 0; row < h; row++) {
        SDL_Rect line = { x0 + row, up ? y0 + h - 1 - row : y0 + row,
                          w - row * 2, 1 };
        if (line.w <= 0) break;
        SDL_RenderFillRect(r, &line);
    }
}

SDL_Rect LauncherDropdown_PanelRect(SDL_Rect field, int count, int item_h,
                                   int top_limit, int bottom_limit) {
    SDL_Rect panel;
    int below = bottom_limit - (field.y + field.h);
    int above = field.y - top_limit;
    int want = (count > 0 ? count : 1) * item_h + 8;
    panel.x = field.x;
    panel.w = field.w;
    if (want <= below || below >= above) {
        panel.y = field.y + field.h - 2;
        panel.h = want <= below ? want : below;
    } else {
        panel.h = want <= above ? want : above;
        panel.y = field.y - panel.h + 2;
    }
    if (panel.h < item_h + 8) panel.h = item_h + 8;
    return panel;
}

int LauncherDropdown_Visible(const SDL_Rect *panel, int item_h) {
    int visible = (panel->h - 8) / item_h;
    return visible < 1 ? 1 : visible;
}

static SDL_Rect panel_rect(const launcher_dropdown_t *d) {
    return LauncherDropdown_PanelRect(d->field,
                                      d->count + (d->searchable ? 1 : 0),
                                      d->item_h,
                                      4, LDRAW_H - 42);
}

static int dropdown_visible(const launcher_dropdown_t *d, const SDL_Rect *panel) {
    int height = panel->h - 8 - (d->searchable ? d->item_h : 0);
    int visible = height / d->item_h;
    return visible < 1 ? 1 : visible;
}

SDL_Rect LauncherDropdown_ItemRect(const launcher_dropdown_t *d, int index) {
    SDL_Rect panel = panel_rect(d);
    int row = index - d->scroll;
    SDL_Rect item = { panel.x + 4,
                      panel.y + 4 + (d->searchable ? d->item_h : 0) + row * d->item_h,
                      panel.w - 8, d->item_h };
    if (row < 0 || row >= dropdown_visible(d, &panel)) {
        item.y = -1000;
        item.h = 0;
    }
    return item;
}

static void clamp_scroll(launcher_dropdown_t *d) {
    SDL_Rect panel = panel_rect(d);
    int visible = dropdown_visible(d, &panel);
    int maximum = d->count - visible;
    if (maximum < 0) maximum = 0;
    if (d->focus < d->scroll) d->scroll = d->focus;
    if (d->focus >= d->scroll + visible) d->scroll = d->focus - visible + 1;
    if (d->scroll > maximum) d->scroll = maximum;
    if (d->scroll < 0) d->scroll = 0;
}

void LauncherDropdown_Wheel(launcher_dropdown_t *d, int delta) {
    if (!d->open || !delta) return;
    SDL_Rect panel = panel_rect(d);
    int visible = dropdown_visible(d, &panel);
    int maximum = d->count - visible;
    if (maximum < 0) maximum = 0;
    d->scroll += delta * 3;
    if (d->scroll < 0) d->scroll = 0;
    if (d->scroll > maximum) d->scroll = maximum;
    if (d->focus < d->scroll) d->focus = d->scroll;
    if (d->focus >= d->scroll + visible) d->focus = d->scroll + visible - 1;
}

void LauncherDropdown_Open(launcher_dropdown_t *d, SDL_Rect field,
                           int count, int current, int pointer_opened,
                           int searchable, launcher_dropdown_label_fn label,
                           void *label_ctx) {
    memset(d, 0, sizeof(*d));
    d->open = 1;
    d->field = field;
    d->source_count = count > 512 ? 512 : count;
    d->count = d->source_count;
    d->current = current >= 0 && current < count ? current : 0;
    d->searchable = searchable;
    d->label = label;
    d->label_ctx = label_ctx;
    for (int i = 0; i < d->count; i++) d->matches[i] = i;
    d->focus = d->current < d->count ? d->current : 0;
    d->item_h = 24;
    d->swallow_release = pointer_opened;
    if (d->searchable) SDL_StartTextInput();
    clamp_scroll(d);
}

static int contains_ci(const char *text, const char *query) {
    if (!query[0]) return 1;
    for (; *text; text++) {
        const char *a = text;
        const char *b = query;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
        if (!*b) return 1;
    }
    return 0;
}

static void rebuild_matches(launcher_dropdown_t *d) {
    d->count = 0;
    for (int i = 0; i < d->source_count; i++) {
        const char *name = d->label ? d->label(d->label_ctx, i) : "";
        if (contains_ci(name, d->query)) d->matches[d->count++] = i;
    }
    d->focus = 0;
    for (int i = 0; i < d->count; i++)
        if (d->matches[i] == d->current) { d->focus = i; break; }
    d->scroll = 0;
    clamp_scroll(d);
}

void LauncherDropdown_Text(launcher_dropdown_t *d, const char *text,
                           int backspace) {
    if (!d->open || !d->searchable) return;
    int changed = 0;
    if (backspace) {
        size_t n = strlen(d->query);
        if (n) { d->query[n - 1] = '\0'; changed = 1; }
    }
    if (text && text[0]) {
        size_t n = strlen(d->query);
        for (const char *p = text; *p && n + 1 < sizeof(d->query); p++) {
            if ((unsigned char)*p >= 0x20 && (unsigned char)*p <= 0x7E)
                d->query[n++] = *p;
        }
        d->query[n] = '\0';
        changed = 1;
    }
    if (changed) rebuild_matches(d);
}

int LauncherDropdown_Tick(launcher_dropdown_t *d, launcher_nav_t *nav,
                          unsigned in) {
    if (!d->open) return LDROP_NONE;
    SDL_Rect panel = panel_rect(d);
    int visible = dropdown_visible(d, &panel);
    SDL_Rect up_arrow = { panel.x + panel.w - 18,
                          panel.y + 2 + (d->searchable ? d->item_h : 0), 16, 12 };
    SDL_Rect down_arrow = { panel.x + panel.w - 18, panel.y + panel.h - 14, 16, 12 };
    if (nav->ptr_pressed && d->scroll > 0 &&
        LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, up_arrow)) {
        LauncherDropdown_Wheel(d, -1);
        d->swallow_release = 1;
        return LDROP_NONE;
    }
    if (nav->ptr_pressed && d->scroll + visible < d->count &&
        LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, down_arrow)) {
        LauncherDropdown_Wheel(d, 1);
        d->swallow_release = 1;
        return LDROP_NONE;
    }
    if (d->count > 0) {
        if (in & LNAV_UP) { if (--d->focus < 0) d->focus = d->count - 1; }
        if (in & LNAV_DOWN) { if (++d->focus >= d->count) d->focus = 0; }
        if (in & LNAV_PAGE_UP) d->focus -= visible;
        if (in & LNAV_PAGE_DOWN) d->focus += visible;
        if (d->focus < 0) d->focus = 0;
        if (d->focus >= d->count) d->focus = d->count - 1;
    }
    clamp_scroll(d);
    if (LauncherNav_HoverHighlight(nav) && nav->ptr_moved) {
        for (int i = 0; i < d->count; i++) {
            if (LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y,
                                         LauncherDropdown_ItemRect(d, i))) {
                d->focus = i;
                break;
            }
        }
    }
    if (in & (LNAV_BACK | LNAV_CANCEL)) { d->open = 0; if(d->searchable)SDL_StopTextInput(); return LDROP_DISMISSED; }
    if (nav->ptr_pressed &&
        !LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, panel)) {
        d->open = 0;
        if(d->searchable)SDL_StopTextInput();
        return LDROP_DISMISSED;
    }
    if (nav->ptr_released) {
        if (d->swallow_release) d->swallow_release = 0;
        else {
            for (int i = 0; i < d->count; i++) {
                if (LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y,
                                             LauncherDropdown_ItemRect(d, i))) {
                    d->open = 0;
                    if(d->searchable)SDL_StopTextInput();
                    return d->matches[i];
                }
            }
        }
    }
    if ((in & LNAV_ACCEPT) && d->count > 0) {
        d->open = 0;
        if(d->searchable)SDL_StopTextInput();
        return d->matches[d->focus];
    }
    return LDROP_NONE;
}

void LauncherDropdown_DrawField(SDL_Renderer *r, SDL_Rect field,
                                const char *value, int focused, int open) {
    LauncherDraw_Bevel(r, field, 0);
    if (focused) {
        SDL_Rect f = { field.x + 3, field.y + 3, field.w - 6, field.h - 6 };
        LauncherDraw_FocusBar(r, f);
    }
    Uint8 c = focused ? 0xFF : 0x00;
    LauncherDraw_TextClippedBold(r, field.x + 6,
        LDRAW_TEXT_Y(field.y, field.h, 1), 1, c, c, c,
        value, field.w - LDROP_CARET_W - 14);
    SDL_Rect caret = { field.x + field.w - LDROP_CARET_W - 2,
                       field.y + 2, LDROP_CARET_W, field.h - 4 };
    SDL_SetRenderDrawColor(r, LCOL_PANEL, 0xFF);
    SDL_RenderFillRect(r, &caret);
    LauncherDraw_Bevel(r, caret, open ? 0 : 1);
    LauncherDropdown_DrawCaret(r, caret, open);
}

void LauncherDropdown_Draw(SDL_Renderer *r, launcher_dropdown_t *d) {
    if (!d->open) return;
    SDL_Rect panel = panel_rect(d);
    clamp_scroll(d);
    SDL_SetRenderDrawColor(r, LCOL_LIGHT, 0xFF);
    SDL_RenderFillRect(r, &panel);
    LauncherDraw_Bevel(r, panel, 1);
    int visible = dropdown_visible(d, &panel);
    if (d->searchable) {
        SDL_Rect search = { panel.x + 4, panel.y + 4, panel.w - 8, d->item_h };
        LauncherDraw_Bevel(r, search, 0);
        char shown[80];
        if (d->query[0]) snprintf(shown, sizeof(shown), "%s_", d->query);
        else snprintf(shown, sizeof(shown), "TYPE TO SEARCH...");
        LauncherDraw_TextClipped(r, search.x + 6,
            LDRAW_TEXT_Y(search.y,search.h,1),1,0,0,0,shown,search.w-12);
    }
    if (d->scroll > 0) {
        SDL_Rect up = { panel.x + panel.w - 14,
                        panel.y + 3 + (d->searchable ? d->item_h : 0), 10, 8 };
        LauncherDropdown_DrawCaret(r, up, 1);
    }
    if (d->scroll + visible < d->count) {
        SDL_Rect down = { panel.x + panel.w - 14, panel.y + panel.h - 11, 10, 8 };
        LauncherDropdown_DrawCaret(r, down, 0);
    }
    for (int i = 0; i < d->count; i++) {
        SDL_Rect item = LauncherDropdown_ItemRect(d, i);
        if (!item.h) continue;
        int focused = i == d->focus;
        if (focused) LauncherDraw_FocusBar(r, item);
        Uint8 c = focused ? 0xFF : 0x00;
        int source = d->matches[i];
        if (source == d->current)
            LauncherDraw_Text(r, item.x + 4, LDRAW_TEXT_Y(item.y, item.h, 1),
                              1, c, c, c, ">");
        LauncherDraw_TextClipped(r, item.x + 16,
            LDRAW_TEXT_Y(item.y, item.h, 1), 1, c, c, c,
            d->label(d->label_ctx, source), item.w - 22);
    }
    if (d->count == 0) {
        SDL_Rect empty = LauncherDropdown_ItemRect(d, 0);
        if (!empty.h) empty = (SDL_Rect){panel.x+4,panel.y+4+(d->searchable?d->item_h:0),panel.w-8,d->item_h};
        LauncherDraw_TextClipped(r,empty.x+7,LDRAW_TEXT_Y(empty.y,empty.h,1),1,
                                 0x60,0x60,0x60,"NO MATCHES",empty.w-14);
    }
}
