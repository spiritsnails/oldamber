#include "launcher_save_editor_internal.h"
#include "launcher_save_editor_location.h"
#include "assetpack.h"
#include "assetpack_bind.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void SE_UpdateCanvas(save_editor_t *e) {
    int output_w = 0, output_h = 0;
    if (SDL_GetRendererOutputSize(e->r, &output_w, &output_h) != 0 ||
        output_w < 1 || output_h < 1) return;
    int scale_x = output_w / 800;
    int scale_y = output_h / 520;
    int scale = scale_x < scale_y ? scale_x : scale_y;
    if (scale < 1) scale = 1;
    int logical_w = output_w / scale;
    int logical_h = output_h / scale;
    if (logical_w < 800) logical_w = 800;
    if (logical_h < 520) logical_h = 520;
    if (logical_w != LDRAW_W || logical_h != LDRAW_H) {
        LauncherDraw_SetSize(logical_w, logical_h);
        SDL_RenderSetLogicalSize(e->r, logical_w, logical_h);
    }
    SDL_RenderSetIntegerScale(e->r, SDL_TRUE);
}

unsigned SE_Poll(save_editor_t *e, int *quit) {
    SDL_Event ev;
    unsigned wheel = 0;
    e->wheel = 0;
    e->dropdown_backspace = 0;
    e->dropdown_text[0] = '\0';
    e->nav->ptr_moved = e->nav->ptr_pressed = e->nav->ptr_released = 0;
    while (SDL_PollEvent(&ev)) {
        LauncherNav_HandleEvent(e->nav, &ev, e->r);
        if (ev.type == SDL_TEXTINPUT) {
            snprintf(e->dropdown_text, sizeof(e->dropdown_text), "%s", ev.text.text);
        } else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_BACKSPACE) {
            e->dropdown_backspace = 1;
        }
        if (ev.type == SDL_MOUSEWHEEL) {
            wheel |= ev.wheel.y > 0 ? LNAV_UP : ev.wheel.y < 0 ? LNAV_DOWN : 0;
            e->wheel -= ev.wheel.y;
        }
        if (ev.type == SDL_KEYDOWN &&
            (ev.key.keysym.mod & KMOD_CTRL) && ev.key.keysym.sym == SDLK_s) {
            e->save_requested = 1;
            e->header_pressed = 2;
            e->header_pressed_until = SDL_GetTicks() + 140;
        }
        if (ev.type == SDL_KEYDOWN &&
            (ev.key.keysym.mod & KMOD_CTRL) && ev.key.keysym.sym == SDLK_TAB) {
            int dir = (ev.key.keysym.mod & KMOD_SHIFT) ? -1 : 1;
            e->requested_tab = (e->active_tab + dir + 5) % 5;
            wheel |= LNAV_BACK;
        }
        if (ev.type == SDL_CONTROLLERBUTTONDOWN &&
            ev.cbutton.button == SDL_CONTROLLER_BUTTON_Y) {
            e->save_requested = 1;
            e->header_pressed = 2;
            e->header_pressed_until = SDL_GetTicks() + 140;
        }
    }
    unsigned in = LauncherNav_Poll(e->nav);
    in |= wheel;
    if (LauncherNav_Device(e->nav) == LNAV_INPUT_GAMEPAD &&
        (in & (LNAV_PAGE_UP | LNAV_PAGE_DOWN))) {
        int dir = (in & LNAV_PAGE_UP) ? -1 : 1;
        e->requested_tab = (e->active_tab + dir + 5) % 5;
        in &= ~(LNAV_PAGE_UP | LNAV_PAGE_DOWN);
        in |= LNAV_BACK;
    }
    if (e->nav->ptr_pressed) {
        static const char *const tabs[] = {
            "EVENTS", "PLAYER / BADGES / MONEY", "POKEDEX", "PARTY", "PC BOXES"
        };
        int x = 16;
        for (int i = 0; i < 5; i++) {
            int w = LauncherDraw_TextWidthBold(1, tabs[i]) + 20;
            SDL_Rect tr = { x, 66, w, 28 };
            if (LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, tr)) {
                e->requested_tab = i;
                in |= LNAV_BACK;
            }
            x += w + 3;
        }
        SDL_Rect reload = { LDRAW_W - 194, 18, 82, 28 };
        SDL_Rect save_btn = { LDRAW_W - 104, 18, 88, 28 };
        if (LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, reload)) {
            e->reload_requested = 1;
            e->header_pressed = 1;
            e->header_pressed_until = SDL_GetTicks() + 140;
        }
        if (LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, save_btn)) {
            e->save_requested = 1;
            e->header_pressed = 2;
            e->header_pressed_until = SDL_GetTicks() + 140;
        }
    }
    if (e->nav->ptr_pressed &&
        LauncherNav_Device(e->nav) == LNAV_INPUT_POINTER) {
        ldraw_footer_btn_t btns[2] = { { "SELECT" }, { "BACK" } };
        LauncherDraw_FooterLayout(btns, 2);
        if (LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, btns[0].rect))
            in |= LNAV_ACCEPT;
        if (LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, btns[1].rect))
            in |= LNAV_BACK;
    }
    *quit = e->nav->quit || (in & LNAV_QUIT);
    return in;
}

unsigned SE_FilterTextNavigation(unsigned in, int backspace) {
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    if (keys[SDL_SCANCODE_W]) in &= ~LNAV_UP;
    if (keys[SDL_SCANCODE_S]) in &= ~LNAV_DOWN;
    if (keys[SDL_SCANCODE_A]) in &= ~LNAV_LEFT;
    if (keys[SDL_SCANCODE_D]) in &= ~LNAV_RIGHT;
    if (keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_SPACE]) in &= ~LNAV_ACCEPT;
    if (backspace || keys[SDL_SCANCODE_BACKSPACE])
        in &= ~(LNAV_BACK | LNAV_CANCEL);
    return in;
}

void SE_DecodeName(const uint8_t *src, char *dst, size_t n) {
    size_t j = 0;
    for (int i = 0; i < NAME_LENGTH - 1 && j + 1 < n; i++) {
        uint8_t c = src[i];
        if (c == 0x50) break;
        if (c >= 0x80 && c <= 0x99) dst[j++] = (char)('A' + c - 0x80);
        else if (c >= 0xA0 && c <= 0xB9) dst[j++] = (char)('a' + c - 0xA0);
        else if (c >= 0xF6) dst[j++] = (char)('0' + c - 0xF6);
        else if (c == 0x7F) dst[j++] = ' ';
        else dst[j++] = '?';
    }
    dst[j] = '\0';
}

void SE_EncodeName(const char *src, uint8_t *dst) {
    memset(dst, 0x50, NAME_LENGTH);
    for (int i = 0; src[i] && i < NAME_LENGTH - 1; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c >= 'A' && c <= 'Z') dst[i] = (uint8_t)(0x80 + c - 'A');
        else if (c >= 'a' && c <= 'z') dst[i] = (uint8_t)(0xA0 + c - 'a');
        else if (c >= '0' && c <= '9') dst[i] = (uint8_t)(0xF6 + c - '0');
        else if (c == ' ') dst[i] = 0x7F;
    }
}

void SE_Header(save_editor_t *e, const char *screen) {
    char title[80], name[24], sub[80];
    snprintf(title, sizeof(title), "%s SAVE EDITOR", e->version_label);
    SE_DecodeName(e->data.player_name, name, sizeof(name));
    snprintf(sub, sizeof(sub), "%s - %s%s", name[0] ? name : "PLAYER", screen,
             e->dirty ? " *" : "");
    SDL_SetRenderDrawColor(e->r, LCOL_BG, 0xFF);
    SDL_RenderClear(e->r);
    LauncherDraw_TextBold(e->r, 16, 20, 2, LCOL_TEXT, title);
    LauncherDraw_TextBold(e->r,
        (LDRAW_W - LauncherDraw_TextWidthBold(1, sub)) / 2,
        48, 1, LCOL_TEXT_DIM, sub);

    static const char *const tabs[] = {
        "EVENTS", "PLAYER / BADGES / MONEY", "POKEDEX", "PARTY", "PC BOXES"
    };
    int x = 16;
    for (int i = 0; i < 5; i++) {
        int w = LauncherDraw_TextWidthBold(1, tabs[i]) + 20;
        SDL_Rect tr = { x, 66, w, 28 };
        LauncherDraw_Bevel(e->r, tr, i != e->active_tab);
        if (i == e->active_tab) {
            SDL_Rect fill = { tr.x + 2, tr.y + 2, tr.w - 4, tr.h - 3 };
            SDL_SetRenderDrawColor(e->r, LCOL_PANEL, 0xFF);
            SDL_RenderFillRect(e->r, &fill);
        }
        LauncherDraw_TextBold(e->r,
            tr.x + (tr.w - LauncherDraw_TextWidthBold(1, tabs[i])) / 2,
            LDRAW_TEXT_Y(tr.y, tr.h, 1), 1, LCOL_TEXT, tabs[i]);
        x += w + 3;
    }
    if (LauncherNav_Device(e->nav) == LNAV_INPUT_GAMEPAD)
        LauncherDraw_TextBold(e->r, LDRAW_W - 108, 98, 1,
                              LCOL_TEXT_DIM, "LB/RB: TABS");
    SDL_Rect reload = { LDRAW_W - 194, 18, 82, 28 };
    SDL_Rect save_btn = { LDRAW_W - 104, 18, 88, 28 };
    Uint32 now=SDL_GetTicks();
    int pressed=(Sint32)(e->header_pressed_until-now)>0?e->header_pressed:0;
    LauncherDraw_Bevel(e->r, reload, pressed==1?0:1);
    LauncherDraw_Bevel(e->r, save_btn, pressed==2?0:1);
    LauncherDraw_TextBold(e->r, reload.x + 12+(pressed==1), LDRAW_TEXT_Y(reload.y, reload.h, 1)+(pressed==1),
                          1, LCOL_TEXT, "RELOAD");
    LauncherDraw_TextBold(e->r, save_btn.x + 16+(pressed==2), LDRAW_TEXT_Y(save_btn.y, save_btn.h, 1)+(pressed==2),
                          1, LCOL_TEXT, "SAVE");
    if(e->feedback[0]&&(Sint32)(e->feedback_until-now)>0){
        if(e->feedback_error)
            LauncherDraw_TextClippedBold(e->r,reload.x,50,1,LCOL_ERROR,e->feedback,save_btn.x+save_btn.w-reload.x);
        else
            LauncherDraw_TextClippedBold(e->r,reload.x,50,1,LCOL_OK,e->feedback,save_btn.x+save_btn.w-reload.x);
    }
}

void SE_Row(save_editor_t *e, int row, int focused,
            const char *label, const char *value) {
    SDL_Rect rr = { SE_X, SE_TOP + row * SE_ROW_H, SE_W, SE_ROW_H };
    if (focused) LauncherDraw_FocusBar(e->r, rr);
    Uint8 tc = focused ? 0xFF : 0x00;
    Uint8 dc = focused ? 0xFF : 0x60;
    LauncherDraw_TextClippedBold(e->r, rr.x + 7,
        LDRAW_TEXT_Y(rr.y, rr.h, 1), 1, tc, tc, tc, label, rr.w / 2 - 14);
    if (value && value[0]) {
        SDL_Rect field = { rr.x + rr.w / 2, rr.y + 2, rr.w / 2 - 5, rr.h - 4 };
        LauncherDraw_Bevel(e->r, field, 0);
        LauncherDraw_TextClippedBold(e->r, field.x + 6,
            LDRAW_TEXT_Y(field.y, field.h, 1), 1, dc, dc, dc, value, field.w - 12);
    }
}

int SE_PointerRow(save_editor_t *e, int rows) {
    SDL_Rect hit = { SE_X, SE_TOP, SE_W, rows * SE_ROW_H };
    if (!LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, hit)) return -1;
    return (e->nav->ptr_y - SE_TOP) / SE_ROW_H;
}

void SE_Present(save_editor_t *e, const char *a, const char *b) {
    if (LauncherNav_Device(e->nav) == LNAV_INPUT_POINTER) {
        ldraw_footer_btn_t btns[2] = { { "SELECT" }, { "BACK" } };
        LauncherDraw_FooterLayout(btns, 2);
        int hover = -1;
        for (int i = 0; i < 2; i++)
            if (LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, btns[i].rect))
                hover = i;
        LauncherDraw_FooterButtons(e->r, btns, 2, hover);
    } else {
        LauncherDraw_PromptBar(e->r, a, b, "CANCEL", "SAVE");
    }
    SDL_RenderPresent(e->r);
    SDL_Delay(16);
}

int SE_EditText(save_editor_t *e, const char *title, uint8_t *encoded) {
    char buf[NAME_LENGTH], shown[96];
    int done = 0, accepted = 0;
    SE_DecodeName(encoded, buf, sizeof(buf));
    SDL_StartTextInput();
    while (!done) {
        SDL_Event ev;
        int backspace = 0;
        e->nav->ptr_moved = e->nav->ptr_pressed = e->nav->ptr_released = 0;
        while (SDL_PollEvent(&ev)) {
            LauncherNav_HandleEvent(e->nav, &ev, e->r);
            if (ev.type == SDL_TEXTINPUT) {
                size_t n = strlen(buf);
                for (const char *p = ev.text.text; *p && n < NAME_LENGTH - 1; p++)
                    if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                        (*p >= '0' && *p <= '9') || *p == ' ') buf[n++] = *p;
                buf[n] = '\0';
            } else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_BACKSPACE) {
                size_t n = strlen(buf); if (n) buf[n - 1] = '\0';
                backspace = 1;
            }
        }
        unsigned in = LauncherNav_Poll(e->nav);
        in = SE_FilterTextNavigation(in, backspace);
        if (e->nav->ptr_pressed) {
            ldraw_footer_btn_t btns[2] = { { "SELECT" }, { "BACK" } };
            LauncherDraw_FooterLayout(btns, 2);
            if (LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, btns[0].rect)) in |= LNAV_ACCEPT;
            if (LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, btns[1].rect)) in |= LNAV_BACK;
        }
        if (in & LNAV_ACCEPT) { accepted = 1; done = 1; }
        if (in & (LNAV_BACK | LNAV_CANCEL)) done = 1;
        SE_Header(e, title);
        SDL_Rect box = { SE_X, 150, SE_W, 42 };
        LauncherDraw_Bevel(e->r, box, 0);
        snprintf(shown, sizeof(shown), "%s_", buf);
        LauncherDraw_TextBold(e->r, box.x + 10, LDRAW_TEXT_Y(box.y, box.h, 2),
                              2, LCOL_TEXT, shown);
        LauncherDraw_TextBold(e->r, SE_X, 215, 1, LCOL_TEXT_DIM,
                              "LETTERS, NUMBERS AND SPACES - 10 CHARACTERS");
        SE_Present(e, "ACCEPT", "BACK");
    }
    SDL_StopTextInput();
    if (accepted) SE_EncodeName(buf, encoded);
    return accepted;
}

int SE_EditNumber(save_editor_t *e, const char *title, uint32_t current,
                  uint32_t minimum, uint32_t maximum, uint32_t *out) {
    char buf[32];
    int done = 0, accepted = 0, replace_on_type = 1;
    snprintf(buf, sizeof(buf), "%u", current);
    SDL_StartTextInput();
    while (!done) {
        SDL_Event ev;
        int backspace = 0;
        e->nav->ptr_moved = e->nav->ptr_pressed = e->nav->ptr_released = 0;
        while (SDL_PollEvent(&ev)) {
            LauncherNav_HandleEvent(e->nav, &ev, e->r);
            if (ev.type == SDL_TEXTINPUT) {
                if (replace_on_type) { buf[0] = '\0'; replace_on_type = 0; }
                size_t n = strlen(buf);
                for (const char *p = ev.text.text; *p && n + 1 < sizeof(buf); p++)
                    if (*p >= '0' && *p <= '9') buf[n++] = *p;
                buf[n] = '\0';
            } else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_BACKSPACE) {
                size_t n = strlen(buf); if (n) buf[n - 1] = '\0'; replace_on_type = 0;
                backspace = 1;
            }
        }
        unsigned in = LauncherNav_Poll(e->nav);
        in = SE_FilterTextNavigation(in, backspace);
        uint32_t live = (uint32_t)strtoul(buf[0] ? buf : "0", NULL, 10);
        if (in & (LNAV_UP | LNAV_RIGHT)) {
            if (live < maximum) live++;
            snprintf(buf, sizeof(buf), "%u", live); replace_on_type = 1;
        }
        if (in & (LNAV_DOWN | LNAV_LEFT)) {
            if (live > minimum) live--;
            snprintf(buf, sizeof(buf), "%u", live); replace_on_type = 1;
        }
        if (e->nav->ptr_pressed) {
            SDL_Rect minus = { SE_X + SE_W - 86, 154, 34, 34 };
            SDL_Rect plus  = { SE_X + SE_W - 46, 154, 34, 34 };
            if (LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, minus) && live > minimum) {
                live--; snprintf(buf, sizeof(buf), "%u", live); replace_on_type = 1;
            }
            if (LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, plus) && live < maximum) {
                live++; snprintf(buf, sizeof(buf), "%u", live); replace_on_type = 1;
            }
            ldraw_footer_btn_t btns[2] = { { "SELECT" }, { "BACK" } };
            LauncherDraw_FooterLayout(btns, 2);
            if (LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, btns[0].rect)) in |= LNAV_ACCEPT;
            if (LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, btns[1].rect)) in |= LNAV_BACK;
        }
        if (in & LNAV_ACCEPT) {
            unsigned long v = strtoul(buf[0] ? buf : "0", NULL, 10);
            if (v >= minimum && v <= maximum) { *out = (uint32_t)v; accepted = 1; done = 1; }
        }
        if (in & (LNAV_BACK | LNAV_CANCEL)) done = 1;
        SE_Header(e, title);
        SDL_Rect box = { SE_X, 150, SE_W, 42 };
        LauncherDraw_Bevel(e->r, box, 0);
        LauncherDraw_TextBold(e->r, box.x + 10, LDRAW_TEXT_Y(box.y, box.h, 2),
                              2, LCOL_TEXT, buf[0] ? buf : "_");
        SDL_Rect minus = { SE_X + SE_W - 86, 154, 34, 34 };
        SDL_Rect plus  = { SE_X + SE_W - 46, 154, 34, 34 };
        LauncherDraw_Bevel(e->r, minus, 1); LauncherDraw_Bevel(e->r, plus, 1);
        LauncherDraw_TextBold(e->r, minus.x + 13, LDRAW_TEXT_Y(minus.y, minus.h, 2), 2, LCOL_TEXT, "-");
        LauncherDraw_TextBold(e->r, plus.x + 10, LDRAW_TEXT_Y(plus.y, plus.h, 2), 2, LCOL_TEXT, "+");
        char range[96];
        snprintf(range, sizeof(range), "ENTER A VALUE FROM %u TO %u", minimum, maximum);
        LauncherDraw_TextBold(e->r, SE_X, 215, 1, LCOL_TEXT_DIM, range);
        SE_Present(e, "ACCEPT", "BACK");
    }
    SDL_StopTextInput();
    return accepted;
}

int SE_EditAscii(save_editor_t *e, const char *title, char *text, size_t size) {
    char buf[128];
    int done = 0, accepted = 0, replace_on_type = 0;
    snprintf(buf, sizeof(buf), "%s", text ? text : "");
    SDL_StartTextInput();
    while (!done) {
        SDL_Event ev;
        int backspace = 0;
        e->nav->ptr_moved = e->nav->ptr_pressed = e->nav->ptr_released = 0;
        while (SDL_PollEvent(&ev)) {
            LauncherNav_HandleEvent(e->nav, &ev, e->r);
            if (ev.type == SDL_TEXTINPUT) {
                if (replace_on_type) { buf[0] = '\0'; replace_on_type = 0; }
                size_t n = strlen(buf);
                for (const char *p = ev.text.text; *p && n + 1 < sizeof(buf); p++)
                    if ((unsigned char)*p >= 0x20 && (unsigned char)*p <= 0x7E)
                        buf[n++] = *p;
                buf[n] = '\0';
            } else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_BACKSPACE) {
                size_t n = strlen(buf); if (n) buf[n - 1] = '\0';
                backspace = 1;
            }
        }
        unsigned in = LauncherNav_Poll(e->nav);
        in = SE_FilterTextNavigation(in, backspace);
        if (e->nav->ptr_pressed) {
            ldraw_footer_btn_t btns[2] = { { "SELECT" }, { "BACK" } };
            LauncherDraw_FooterLayout(btns, 2);
            if (LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, btns[0].rect)) in |= LNAV_ACCEPT;
            if (LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, btns[1].rect)) in |= LNAV_BACK;
        }
        if (in & LNAV_ACCEPT) { accepted = 1; done = 1; }
        if (in & (LNAV_BACK | LNAV_CANCEL)) done = 1;
        SE_Header(e, title);
        SDL_Rect box = { SE_X, 150, SE_W, 42 };
        LauncherDraw_Bevel(e->r, box, 0);
        LauncherDraw_TextClippedBold(e->r, box.x + 10,
            LDRAW_TEXT_Y(box.y, box.h, 2), 2, LCOL_TEXT, buf, box.w - 20);
        SE_Present(e, "ACCEPT", "BACK");
    }
    SDL_StopTextInput();
    if (accepted && text && size) snprintf(text, size, "%s", buf);
    return accepted;
}

int SE_Choose(save_editor_t *e, const char *title, int count, int selected,
              se_choice_label_fn label, void *ctx) {
    int sel = selected >= 0 && selected < count ? selected : 0;
    int top = sel - SE_ROWS / 2;
    int running = 1;
    if (top < 0) top = 0;
    if (top > count - SE_ROWS) top = count > SE_ROWS ? count - SE_ROWS : 0;
    while (running) {
        int quit = 0;
        unsigned in = SE_Poll(e, &quit);
        if (quit) return -1;
        if (e->nav->ptr_moved) {
            int row = SE_PointerRow(e, SE_ROWS);
            if (row >= 0 && top + row < count) sel = top + row;
        }
        if (in & LNAV_UP) sel--;
        if (in & LNAV_DOWN) sel++;
        if (in & LNAV_PAGE_UP) sel -= SE_ROWS;
        if (in & LNAV_PAGE_DOWN) sel += SE_ROWS;
        if (sel < 0) sel = 0;
        if (sel >= count) sel = count - 1;
        if (sel < top) top = sel;
        if (sel >= top + SE_ROWS) top = sel - SE_ROWS + 1;
        int accept = (in & LNAV_ACCEPT) != 0;
        if (e->nav->ptr_pressed && SE_PointerRow(e, SE_ROWS) == sel - top)
            accept = 1;
        if (accept) return sel;
        if (in & (LNAV_BACK | LNAV_CANCEL)) return -1;

        SE_Header(e, title);
        SDL_Rect frame = { SE_X - 4, SE_TOP - 4, SE_W + 8,
                           SE_ROWS * SE_ROW_H + 8 };
        LauncherDraw_Bevel(e->r, frame, 0);
        int hover = LauncherNav_HoverHighlight(e->nav)
                  ? SE_PointerRow(e, SE_ROWS) : sel - top;
        for (int i = 0; i < SE_ROWS && top + i < count; i++) {
            char n[24];
            snprintf(n, sizeof(n), "%d", top + i + 1);
            SE_Row(e, i, i == hover, label(ctx, top + i), n);
        }
        SE_Present(e, "CHOOSE", "BACK");
    }
    return -1;
}

static int mount_editor_assets(const char *ver) {
    char dir[256], err[512];
    Pkg_UnmountAll();
    snprintf(dir, sizeof(dir), "packages/%s", ver);
    if (!Pkg_MountList(dir, err, sizeof(err))) {
        snprintf(dir, sizeof(dir), "../packages/%s", ver);
        if (!Pkg_MountList(dir, err, sizeof(err))) return 0;
    }
    AssetPack_BindAll();
    return 1;
}

int LauncherSaveEditor_Run(SDL_Renderer *r, SDL_Window *win,
                           launcher_nav_t *nav, const char *path,
                           const char *version_label) {
    save_editor_t e;
    int running = 1, result = 0;
    int old_lw = LDRAW_W, old_lh = LDRAW_H, old_ww = 0, old_wh = 0;
    SDL_bool old_integer_scale = SDL_RenderGetIntegerScale(r);
    memset(&e, 0, sizeof(e));
    e.r = r; e.win = win; e.nav = nav; e.path = path; e.version_label = version_label;
    if (Save_EditorRead(path, &e.data) != 0) return -1;
    if (!mount_editor_assets(version_label && strcmp(version_label, "BLUE") == 0 ? "blue" : "red"))
        return -1;
    SE_LocationLoad(version_label);
    SDL_GetWindowSize(win, &old_ww, &old_wh);
    if (!(SDL_GetWindowFlags(win) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) &&
        (old_ww < 800 || old_wh < 520)) {
        SDL_SetWindowSize(win, 800, 600);
        SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    SE_UpdateCanvas(&e);
    e.active_tab = 0;
    e.requested_tab = -1;

    (void)running;
    SE_Workspace(&e);
    Pkg_UnmountAll();
    LauncherDraw_SetSize(old_lw, old_lh);
    SDL_RenderSetLogicalSize(r, old_lw, old_lh);
    SDL_RenderSetIntegerScale(r, old_integer_scale);
    SDL_SetWindowSize(win, old_ww, old_wh);
    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    return result;
}
