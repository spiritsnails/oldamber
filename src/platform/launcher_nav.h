#pragma once

#include <SDL.h>

enum {
    LNAV_UP        = 1 << 0,
    LNAV_DOWN      = 1 << 1,
    LNAV_LEFT      = 1 << 2,
    LNAV_RIGHT     = 1 << 3,
    LNAV_PAGE_UP   = 1 << 4,
    LNAV_PAGE_DOWN = 1 << 5,
    LNAV_ACCEPT    = 1 << 6,
    LNAV_BACK      = 1 << 7,

    LNAV_CANCEL    = 1 << 8,
    LNAV_QUIT      = 1 << 9,
};

#define LNAV_INTENT_COUNT 10

typedef enum {
    LNAV_INPUT_POINTER = 0,
    LNAV_INPUT_GAMEPAD = 1,
} lnav_input_t;

typedef struct {
    SDL_GameController *pad;
    Uint8  held[LNAV_INTENT_COUNT];
    Uint32 next_fire[LNAV_INTENT_COUNT];
    Uint32 interval[LNAV_INTENT_COUNT];

    int    ptr_x, ptr_y;
    int    ptr_moved;
    int    ptr_pressed;
    int    ptr_released;
    lnav_input_t input;
    int    last_mouse;
    int    quit;
} launcher_nav_t;

void LauncherNav_Init(launcher_nav_t *n);
void LauncherNav_Close(launcher_nav_t *n);

void LauncherNav_HandleEvent(launcher_nav_t *n, const SDL_Event *ev,
                             SDL_Renderer *r);

unsigned LauncherNav_Poll(launcher_nav_t *n);

static inline lnav_input_t LauncherNav_Device(const launcher_nav_t *n) {
    return n->input;
}

static inline int LauncherNav_HoverHighlight(const launcher_nav_t *n) {
    return n->input == LNAV_INPUT_POINTER && n->last_mouse;
}
