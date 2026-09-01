#pragma once

#include "launcher_nav.h"

typedef const char *(*launcher_dropdown_label_fn)(void *ctx, int index);

enum {
    LDROP_DISMISSED = -1,
    LDROP_NONE = -2
};

typedef struct {
    int open;
    int focus;
    int scroll;
    int swallow_release;
    int count;
    int source_count;
    int current;
    int item_h;
    SDL_Rect field;
    int searchable;
    char query[64];
    int matches[512];
    launcher_dropdown_label_fn label;
    void *label_ctx;
} launcher_dropdown_t;

void LauncherDropdown_Open(launcher_dropdown_t *d, SDL_Rect field,
                           int count, int current, int pointer_opened,
                           int searchable, launcher_dropdown_label_fn label,
                           void *label_ctx);
void LauncherDropdown_Text(launcher_dropdown_t *d, const char *text,
                           int backspace);
int LauncherDropdown_Tick(launcher_dropdown_t *d, launcher_nav_t *nav,
                          unsigned in);
void LauncherDropdown_Wheel(launcher_dropdown_t *d, int delta);
void LauncherDropdown_Draw(SDL_Renderer *r, launcher_dropdown_t *d);
void LauncherDropdown_DrawField(SDL_Renderer *r, SDL_Rect field,
                                const char *value, int focused, int open);
void LauncherDropdown_DrawCaret(SDL_Renderer *r, SDL_Rect box, int up);
SDL_Rect LauncherDropdown_PanelRect(SDL_Rect field, int count, int item_h,
                                   int top_limit, int bottom_limit);
int LauncherDropdown_Visible(const SDL_Rect *panel, int item_h);
SDL_Rect LauncherDropdown_ItemRect(const launcher_dropdown_t *d, int index);
