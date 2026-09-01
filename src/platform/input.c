
#include "input.h"
#include "data_dir.h"
#include "hardware.h"
#include "display.h"
#include "../game/constants.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>

static FILE *s_rec_fp  = NULL;
static FILE *s_play_fp = NULL;
static int   s_playing = 0;
static int   s_block_gameplay_input = 0;

static const char *controls_cfg_path(void) {
    static char path[1200];
    return UserDataPath("controls.cfg", path, sizeof path) ? path : "controls.cfg";
}
#define CONTROLS_CFG controls_cfg_path()

static void bindings_save(void);

static SDL_Scancode key_map[8] = {
    SDL_SCANCODE_Z,
    SDL_SCANCODE_X,
    SDL_SCANCODE_RSHIFT,
    SDL_SCANCODE_RETURN,
    SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_LEFT,
    SDL_SCANCODE_UP,
    SDL_SCANCODE_DOWN,
};

static SDL_GameControllerButton pad_map[8] = {
    SDL_CONTROLLER_BUTTON_A,
    SDL_CONTROLLER_BUTTON_B,
    SDL_CONTROLLER_BUTTON_BACK,
    SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SDL_CONTROLLER_BUTTON_DPAD_UP,
    SDL_CONTROLLER_BUTTON_DPAD_DOWN,
};

static SDL_Scancode              key_def[8];
static SDL_GameControllerButton  pad_def[8];
static int                       defs_taken = 0;

static const char *kBindNames[8] = {
    "A", "B", "SELECT", "START", "RIGHT", "LEFT", "UP", "DOWN"
};

static const char *kBindKeys[8] = {
    "a", "b", "select", "start", "right", "left", "up", "down"
};

const char *Input_BindName(int bit) {
    return (bit >= 0 && bit < 8) ? kBindNames[bit] : "";
}

int Input_GetKey(int bit) {
    return (bit >= 0 && bit < 8) ? (int)key_map[bit] : (int)SDL_SCANCODE_UNKNOWN;
}

int Input_GetPad(int bit) {
    return (bit >= 0 && bit < 8) ? (int)pad_map[bit] : -1;
}

void Input_SetKey(int bit, int scancode) {
    if (bit < 0 || bit >= 8) return;
    if (scancode == SDL_SCANCODE_UNKNOWN) return;

    for (int i = 0; i < 8; i++)
        if (i != bit && key_map[i] == (SDL_Scancode)scancode)
            key_map[i] = key_map[bit];
    key_map[bit] = (SDL_Scancode)scancode;
    bindings_save();
}

void Input_SetPad(int bit, int button) {
    if (bit < 0 || bit >= 8) return;
    if (button < 0 || button >= SDL_CONTROLLER_BUTTON_MAX) return;
    for (int i = 0; i < 8; i++)
        if (i != bit && pad_map[i] == (SDL_GameControllerButton)button)
            pad_map[i] = pad_map[bit];
    pad_map[bit] = (SDL_GameControllerButton)button;
    bindings_save();
}

const char *Input_KeyLabel(int scancode) {
    static char buf[24];
    const char *n = SDL_GetScancodeName((SDL_Scancode)scancode);
    int i = 0;
    if (!n || !*n) return "---";

    while (n[i] && i < (int)sizeof buf - 1) {
        char c = n[i];
        buf[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
        i++;
    }
    buf[i] = 0;
    return buf;
}

const char *Input_PadLabel(int button) {

    switch (button) {
    case SDL_CONTROLLER_BUTTON_A:             return "A";
    case SDL_CONTROLLER_BUTTON_B:             return "B";
    case SDL_CONTROLLER_BUTTON_X:             return "X";
    case SDL_CONTROLLER_BUTTON_Y:             return "Y";
    case SDL_CONTROLLER_BUTTON_BACK:          return "BACK";
    case SDL_CONTROLLER_BUTTON_GUIDE:         return "GUIDE";
    case SDL_CONTROLLER_BUTTON_START:         return "START";
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:     return "L STICK";
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    return "R STICK";
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return "L1";
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "R1";
    case SDL_CONTROLLER_BUTTON_DPAD_UP:       return "D-UP";
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     return "D-DOWN";
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     return "D-LEFT";
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    return "D-RIGHT";
    default:                                  return "---";
    }
}

#define PAD_STICK_DEADZONE 16384

static SDL_GameController *s_pad = NULL;

static void pad_refresh(void) {
    if (s_pad) {
        if (SDL_GameControllerGetAttached(s_pad)) return;
        SDL_GameControllerClose(s_pad);
        s_pad = NULL;
    }
    for (int i = 0, n = SDL_NumJoysticks(); i < n; i++) {
        if (!SDL_IsGameController(i)) continue;
        s_pad = SDL_GameControllerOpen(i);
        if (s_pad) {
            printf("[input] gamepad: %s\n", SDL_GameControllerName(s_pad));
            fflush(stdout);
            return;
        }
    }
}

static uint8_t pad_read(void) {
    uint8_t raw = 0;
    Sint16 ax, ay;
    if (!s_pad) return 0;

    for (int i = 0; i < 8; i++)
        if (SDL_GameControllerGetButton(s_pad, pad_map[i]))
            raw |= (uint8_t)(1 << i);

    ax = SDL_GameControllerGetAxis(s_pad, SDL_CONTROLLER_AXIS_LEFTX);
    ay = SDL_GameControllerGetAxis(s_pad, SDL_CONTROLLER_AXIS_LEFTY);
    if (ax >  PAD_STICK_DEADZONE) raw |= (uint8_t)(1 << 4);
    if (ax < -PAD_STICK_DEADZONE) raw |= (uint8_t)(1 << 5);
    if (ay < -PAD_STICK_DEADZONE) raw |= (uint8_t)(1 << 6);
    if (ay >  PAD_STICK_DEADZONE) raw |= (uint8_t)(1 << 7);
    return raw;
}

int Input_PadMenuPressed(void) {
    static int was_down = 0;
    int down = s_pad &&
        SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) &&
        SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    int edge = down && !was_down;
    was_down = down;
    return edge;
}

int Input_PadAttached(void) { return s_pad != NULL; }

int Input_PadAnyButton(void) {
    if (!s_pad) return -1;
    for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++)
        if (SDL_GameControllerGetButton(s_pad, (SDL_GameControllerButton)b))
            return b;
    return -1;
}

static void bindings_save(void) {
    FILE *f = fopen(CONTROLS_CFG, "w");
    if (!f) return;
    fprintf(f, "# oldamber controls -- rewritten on every change\n");
    for (int i = 0; i < 8; i++)
        fprintf(f, "key_%s %d\n", kBindKeys[i], (int)key_map[i]);
    for (int i = 0; i < 8; i++)
        fprintf(f, "pad_%s %d\n", kBindKeys[i], (int)pad_map[i]);
    fclose(f);
}

static void bindings_load(void) {
    FILE *f;
    char line[128];
    if (!defs_taken) {
        for (int i = 0; i < 8; i++) { key_def[i] = key_map[i]; pad_def[i] = pad_map[i]; }
        defs_taken = 1;
    }
    f = fopen(CONTROLS_CFG, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char key[32];
        int  val;
        if (sscanf(line, "%31s %d", key, &val) != 2) continue;
        if (key[0] == '#') continue;
        for (int i = 0; i < 8; i++) {
            char kn[40], pn[40];
            snprintf(kn, sizeof kn, "key_%s", kBindKeys[i]);
            snprintf(pn, sizeof pn, "pad_%s", kBindKeys[i]);

            if (strcmp(key, kn) == 0) {
                if (val > 0 && val < SDL_NUM_SCANCODES) key_map[i] = (SDL_Scancode)val;
                break;
            }
            if (strcmp(key, pn) == 0) {
                if (val >= 0 && val < SDL_CONTROLLER_BUTTON_MAX)
                    pad_map[i] = (SDL_GameControllerButton)val;
                break;
            }
        }
    }
    fclose(f);
}

static int s_reset_held;
static int s_reset_fired;

static void emergency_reset_poll(void) {
    const Uint8 *ks = SDL_GetKeyboardState(NULL);
    int down = ks && ks[SDL_SCANCODE_F5];
    if (!down && s_pad)
        down = SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_BACK) &&
               SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_START);
    s_reset_fired = 0;
    if (!down) { s_reset_held = 0; return; }
    if (++s_reset_held == INPUT_RESET_HOLD_FRAMES) {
        Input_ResetBindings();
        s_reset_fired = 1;
        printf("[input] emergency reset: bindings back to defaults\n");
        fflush(stdout);
    }
}

int Input_EmergencyResetFired(void) { return s_reset_fired; }

void Input_ResetBindings(void) {
    if (!defs_taken) return;
    for (int i = 0; i < 8; i++) { key_map[i] = key_def[i]; pad_map[i] = pad_def[i]; }
    bindings_save();
}

void Input_Init(void) { bindings_load(); pad_refresh(); }
void Input_Quit(void) {
    Input_StopRecording();
    Input_StopPlayback();
    if (s_pad) { SDL_GameControllerClose(s_pad); s_pad = NULL; }
}

extern uint8_t gCliButtons;
extern int     gCliFrames;

static uint8_t s_raw_held = 0;

uint8_t Input_RawHeld(void) { return s_raw_held; }

void Input_Update(void) {
    uint8_t raw;

    emergency_reset_poll();

    if (gCliFrames > 0) {
        raw = gCliButtons;
        gCliFrames--;
    } else if (s_playing) {
        if (fread(&raw, 1, 1, s_play_fp) != 1) {
            Input_StopPlayback();
            raw = 0;
        }
    } else {
        const uint8_t *keys = SDL_GetKeyboardState(NULL);
        raw = 0;

        pad_refresh();

        if (!(s_pad && Display_IsSteamDeck())) {
            for (int i = 0; i < 8; i++)
                if (keys[key_map[i]])
                    raw |= (uint8_t)(1 << i);
        }
        raw |= pad_read();

        s_raw_held = raw;
        raw &= ~wJoyIgnore;
    }

    if (s_rec_fp) fwrite(&raw, 1, 1, s_rec_fp);

    if (s_block_gameplay_input)
        raw = 0;

    hJoyReleased = hJoyHeld & ~raw;
    hJoyPressed  = raw & ~hJoyHeld;
    hJoyHeld     = raw;
    hJoyInput    = raw;
}

void Input_StartRecording(const char *path) {
    Input_StopRecording();
    s_rec_fp = fopen(path, "wb");
    if (s_rec_fp) printf("[input] Recording: %s\n", path);
    else          printf("[input] Record open failed: %s\n", path);
}
void Input_StopRecording(void) {
    if (!s_rec_fp) return;
    fclose(s_rec_fp); s_rec_fp = NULL;
    printf("[input] Recording stopped.\n");
}
void Input_StartPlayback(const char *path) {
    Input_StopPlayback();
    s_play_fp = fopen(path, "rb");
    if (s_play_fp) { s_playing = 1; printf("[input] Playback: %s\n", path); }
    else           printf("[input] Playback open failed: %s\n", path);
}
void Input_StopPlayback(void) {
    if (s_play_fp) { fclose(s_play_fp); s_play_fp = NULL; }
    if (s_playing) { s_playing = 0; printf("[input] Playback stopped.\n"); }
}
int Input_IsRecording(void) { return s_rec_fp != NULL; }
int Input_IsPlaying(void)   { return s_playing; }
void Input_SetGameplayInputBlocked(int blocked) { s_block_gameplay_input = blocked ? 1 : 0; }
