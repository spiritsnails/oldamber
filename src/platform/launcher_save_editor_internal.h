#pragma once
#include "launcher_save_editor.h"
#include "launcher_draw.h"
#include "save.h"

#define SE_X 24
#define SE_TOP 112
#define SE_ROW_H 24
#define SE_ROWS 17
#define SE_W (LDRAW_W - SE_X * 2)

typedef struct {
    SDL_Renderer *r;
    SDL_Window *win;
    launcher_nav_t *nav;
    save_editor_data_t data;
    const char *path;
    const char *version_label;
    int dirty;
    int active_tab;
    int requested_tab;
    int close_requested;
    int save_requested;
    int reload_requested;
    int header_pressed;
    Uint32 header_pressed_until;
    char feedback[64];
    Uint32 feedback_until;
    int feedback_error;
    int focus;
    int scroll;
    int wheel;
    int dropdown_backspace;
    char dropdown_text[32];
    int party_slot;
    int box_num;
    int box_slot;
    int box_detail;
    char search[64];
} save_editor_t;

typedef const char *(*se_choice_label_fn)(void *ctx, int index);

unsigned SE_Poll(save_editor_t *e, int *quit);
unsigned SE_FilterTextNavigation(unsigned in, int backspace);
void SE_UpdateCanvas(save_editor_t *e);
void SE_Header(save_editor_t *e, const char *screen);
void SE_Row(save_editor_t *e, int row, int focused,
            const char *label, const char *value);
int SE_PointerRow(save_editor_t *e, int rows);
void SE_Present(save_editor_t *e, const char *a, const char *b);
void SE_DecodeName(const uint8_t *src, char *dst, size_t n);
void SE_EncodeName(const char *src, uint8_t *dst);
int SE_EditText(save_editor_t *e, const char *title, uint8_t *encoded);
int SE_EditNumber(save_editor_t *e, const char *title, uint32_t current,
                  uint32_t minimum, uint32_t maximum, uint32_t *out);
int SE_EditAscii(save_editor_t *e, const char *title, char *text, size_t size);
int SE_Choose(save_editor_t *e, const char *title, int count, int selected,
              se_choice_label_fn label, void *ctx);

void SE_PlayerScreen(save_editor_t *e);
void SE_InventoryScreen(save_editor_t *e, int pc);
void SE_DexScreen(save_editor_t *e);
void SE_PartyScreen(save_editor_t *e);
void SE_BoxesScreen(save_editor_t *e);
void SE_EventsScreen(save_editor_t *e);
int SE_Workspace(save_editor_t *e);
