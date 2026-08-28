
#include "launcher_nav.h"

#include <string.h>

#define REPEAT_DELAY_MS  380
#define REPEAT_FAST_MS    55
#define REPEAT_STEP_MS    35

#define STICK_DEADZONE 16000

static int bit_index(unsigned intent) {
    int i = 0;
    while (intent > 1) { intent >>= 1; i++; }
    return i;
}

void LauncherNav_Init(launcher_nav_t *n) {
    memset(n, 0, sizeof(*n));

    n->input = LNAV_INPUT_POINTER;
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) return;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            n->pad = SDL_GameControllerOpen(i);
            if (n->pad) break;
        }
    }

    if (n->pad) n->input = LNAV_INPUT_GAMEPAD;
}

void LauncherNav_Close(launcher_nav_t *n) {
    if (n->pad) SDL_GameControllerClose(n->pad);
    n->pad = NULL;
}

static void win_to_logical(SDL_Renderer *r, int wx, int wy, int *lx, int *ly) {
    int ow = 0, oh = 0, lw = 0, lh = 0;
    SDL_GetRendererOutputSize(r, &ow, &oh);
    SDL_RenderGetLogicalSize(r, &lw, &lh);
    if (lw <= 0 || lh <= 0 || ow <= 0 || oh <= 0) { *lx = wx; *ly = wy; return; }

    float sx = (float)ow / (float)lw;
    float sy = (float)oh / (float)lh;
    float s  = sx < sy ? sx : sy;
    if (s <= 0.0f) { *lx = wx; *ly = wy; return; }

    float offx = ((float)ow - (float)lw * s) * 0.5f;
    float offy = ((float)oh - (float)lh * s) * 0.5f;
    *lx = (int)(((float)wx - offx) / s);
    *ly = (int)(((float)wy - offy) / s);
}

void LauncherNav_HandleEvent(launcher_nav_t *n, const SDL_Event *ev,
                             SDL_Renderer *r) {
    switch (ev->type) {
    case SDL_QUIT:
        n->quit = 1;
        break;

    case SDL_CONTROLLERDEVICEADDED:
        if (!n->pad) n->pad = SDL_GameControllerOpen(ev->cdevice.which);
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        if (n->pad &&
            ev->cdevice.which ==
                SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(n->pad))) {
            SDL_GameControllerClose(n->pad);
            n->pad = NULL;
        }
        break;

    case SDL_MOUSEMOTION:
        n->ptr_x = ev->motion.x;
        n->ptr_y = ev->motion.y;
        n->ptr_moved = 1;

        if (ev->motion.which != SDL_TOUCH_MOUSEID &&
            (ev->motion.xrel || ev->motion.yrel)) {
            n->input = LNAV_INPUT_POINTER;
            n->last_mouse = 1;
        }
        break;
    case SDL_MOUSEBUTTONDOWN:
        if (ev->button.button == SDL_BUTTON_LEFT) {
            n->ptr_x = ev->button.x;
            n->ptr_y = ev->button.y;
            n->ptr_moved = 1;
            n->ptr_pressed = 1;
            if (ev->button.which != SDL_TOUCH_MOUSEID) {
                n->input = LNAV_INPUT_POINTER;
                n->last_mouse = 1;
            }
        }
        break;
    case SDL_MOUSEBUTTONUP:
        if (ev->button.button == SDL_BUTTON_LEFT) n->ptr_released = 1;
        break;

    case SDL_FINGERDOWN:
    case SDL_FINGERMOTION: {
        int ow = 0, oh = 0;
        SDL_GetRendererOutputSize(r, &ow, &oh);
        win_to_logical(r, (int)(ev->tfinger.x * (float)ow),
                          (int)(ev->tfinger.y * (float)oh),
                       &n->ptr_x, &n->ptr_y);
        n->ptr_moved = 1;
        if (ev->type == SDL_FINGERDOWN) n->ptr_pressed = 1;
        break;
    }
    case SDL_FINGERUP:
        n->ptr_released = 1;
        break;

    case SDL_KEYDOWN:
        n->input = LNAV_INPUT_POINTER;
        n->last_mouse = 0;
        break;

    case SDL_CONTROLLERBUTTONDOWN:
        n->input = LNAV_INPUT_GAMEPAD;
        n->last_mouse = 0;
        break;
    case SDL_CONTROLLERAXISMOTION:
        if (ev->caxis.value > STICK_DEADZONE || ev->caxis.value < -STICK_DEADZONE) {
            n->input = LNAV_INPUT_GAMEPAD;
            n->last_mouse = 0;
        }
        break;
    }
}

static int raw_state(launcher_nav_t *n, const Uint8 *keys, unsigned intent) {
    SDL_GameController *p = n->pad;
    switch (intent) {
    case LNAV_UP:
        return keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W] ||
               (p && (SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_UP) ||
                      SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_LEFTY) < -STICK_DEADZONE));
    case LNAV_DOWN:
        return keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S] ||
               (p && (SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_DOWN) ||
                      SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_LEFTY) > STICK_DEADZONE));
    case LNAV_LEFT:
        return keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A] ||
               (p && (SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_LEFT) ||
                      SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_LEFTX) < -STICK_DEADZONE));
    case LNAV_RIGHT:
        return keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D] ||
               (p && (SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) ||
                      SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_LEFTX) > STICK_DEADZONE));
    case LNAV_PAGE_UP:
        return keys[SDL_SCANCODE_PAGEUP] ||
               (p && SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
    case LNAV_PAGE_DOWN:
        return keys[SDL_SCANCODE_PAGEDOWN] ||
               (p && SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));
    case LNAV_ACCEPT:
        return keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER] ||
               keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_Z] ||
               (p && SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_A));

    case LNAV_BACK:
        return keys[SDL_SCANCODE_BACKSPACE] ||
               (p && SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_B));

    case LNAV_CANCEL:
        return keys[SDL_SCANCODE_ESCAPE] ||
               (p && SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_X));
    default:
        return 0;
    }
}

static int repeats(unsigned intent) {
    return intent == LNAV_UP || intent == LNAV_DOWN ||
           intent == LNAV_LEFT || intent == LNAV_RIGHT ||
           intent == LNAV_PAGE_UP || intent == LNAV_PAGE_DOWN;
}

unsigned LauncherNav_Poll(launcher_nav_t *n) {
    unsigned fired = 0;
    Uint32 now = SDL_GetTicks();

    if (n->pad) SDL_GameControllerUpdate();
    const Uint8 *keys = SDL_GetKeyboardState(NULL);

    static const unsigned kIntents[] = {
        LNAV_UP, LNAV_DOWN, LNAV_LEFT, LNAV_RIGHT,
        LNAV_PAGE_UP, LNAV_PAGE_DOWN, LNAV_ACCEPT, LNAV_BACK, LNAV_CANCEL
    };

    for (size_t i = 0; i < sizeof(kIntents) / sizeof(kIntents[0]); i++) {
        unsigned intent = kIntents[i];
        int idx = bit_index(intent);
        int down = raw_state(n, keys, intent) ? 1 : 0;

        if (down && !n->held[idx]) {
            fired |= intent;
            n->interval[idx]  = REPEAT_DELAY_MS;
            n->next_fire[idx] = now + REPEAT_DELAY_MS;
        } else if (down && repeats(intent) && now >= n->next_fire[idx]) {
            fired |= intent;

            Uint32 iv = n->interval[idx];
            iv = (iv > REPEAT_FAST_MS + REPEAT_STEP_MS) ? iv - REPEAT_STEP_MS
                                                        : REPEAT_FAST_MS;
            n->interval[idx]  = iv;
            n->next_fire[idx] = now + iv;
        }
        n->held[idx] = (Uint8)down;
    }

    if (n->quit) fired |= LNAV_QUIT;
    return fired;
}
