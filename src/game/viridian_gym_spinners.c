
#include "viridian_gym_spinners.h"
#include "player.h"
#include "overworld.h"
#include "constants.h"
#include "amberscript_mapbank.h"
#include "amberscript_tilemod.h"
#include "assetpack_bind.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include <string.h>

#define DIR_DOWN  0
#define DIR_UP    1
#define DIR_LEFT  2
#define DIR_RIGHT 3

typedef struct {
    uint8_t x, y;
    const int8_t *seq;
} viridian_spin_coord_t;

static int          s_spin_active = 0;
static const int8_t *s_spin_seq   = 0;
static int          s_spin_idx    = 0;

static int seq_last_idx(const int8_t *seq) {
    int i = 0;
    while (seq[i] != -1) i++;
    return i - 1;
}

static const int8_t kVGymM1[]  = { DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    -1 };
static const int8_t kVGymM2[]  = { DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  -1 };
static const int8_t kVGymM3[]  = { DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  -1 };
static const int8_t kVGymM4[]  = { DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kVGymM5[]  = { DIR_DOWN,  DIR_DOWN,  -1 };
static const int8_t kVGymM6[]  = { DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  -1 };
static const int8_t kVGymM7[]  = { DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kVGymM8[]  = { DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kVGymM9[]  = { DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    -1 };
static const int8_t kVGymM10[] = { DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    -1 };
static const int8_t kVGymM11[] = { DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  -1 };
static const int8_t kVGymM12[] = { DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  -1 };

static const viridian_spin_coord_t kVGymSpin[] = {
    { 19, 11, kVGymM1  },
    { 19,  1, kVGymM2  },
    { 18,  2, kVGymM3  },
    { 11,  2, kVGymM4  },
    { 16, 10, kVGymM5  },
    {  4,  6, kVGymM6  },
    {  5, 13, kVGymM7  },
    {  4, 14, kVGymM8  },
    {  0, 15, kVGymM9  },
    {  1, 15, kVGymM10 },
    { 13, 16, kVGymM11 },
    { 13, 17, kVGymM12 },
};

static const int8_t *find_spin_seq(uint8_t x, uint8_t y) {
    for (int i = 0; i < (int)(sizeof kVGymSpin / sizeof kVGymSpin[0]); i++) {
        if (kVGymSpin[i].x == x && kVGymSpin[i].y == y) return kVGymSpin[i].seq;
    }
    return 0;
}

static const uint8_t kGymIdle_t060[16] = { 0xFF,0x00, 0x80,0x00, 0x80,0x00, 0x80,0x01, 0x80,0x03, 0x80,0x07, 0x80,0x0E, 0x80,0x1C };
static const uint8_t kGymIdle_t061[16] = { 0xFF,0x00, 0x01,0x00, 0x01,0x00, 0x01,0x80, 0x01,0xC0, 0x01,0xE0, 0x01,0x70, 0x01,0x38 };
static const uint8_t kGymIdle_t076[16] = { 0x80,0x1C, 0x80,0x0E, 0x80,0x07, 0x80,0x03, 0x80,0x01, 0x80,0x00, 0x80,0x00, 0xFF,0x00 };
static const uint8_t kGymIdle_t077[16] = { 0x01,0x38, 0x01,0x70, 0x01,0xE0, 0x01,0xC0, 0x01,0x80, 0x01,0x00, 0x01,0x00, 0xFF,0x00 };

static void apply_spinner_flicker(void) {
    int steps = Player_GetSimulatedStepsRemaining();

    int animated = (steps >= 0) && (((steps >> 1) & 1) == 0);

    AmberScript_SetSubtilePixels("viridiangym_t060", animated ? kSpinnerFrame1 : kGymIdle_t060);
    AmberScript_SetSubtilePixels("viridiangym_t061", animated ? kSpinnerFrame3 : kGymIdle_t061);
    AmberScript_SetSubtilePixels("viridiangym_t076", animated ? kSpinnerFrame0 : kGymIdle_t076);
    AmberScript_SetSubtilePixels("viridiangym_t077", animated ? kSpinnerFrame2 : kGymIdle_t077);
}

static int on_viridian_gym(void) {
    const char *n = AmberScript_MapBank_NameForRealId(wCurMap);

    return n && strcasecmp(n, "ViridianGym") == 0;
}

void ViridianGymSpinners_OnMapLoad(void) {
    s_spin_active = 0;
    s_spin_seq = 0;
    s_spin_idx = 0;
    Player_SetSpinnerSpin(0);
}

void ViridianGymSpinners_StepCheck(void) {
    const int8_t *seq;
    if (!on_viridian_gym()) return;
    if (s_spin_active) return;

    seq = find_spin_seq((uint8_t)wXCoord, (uint8_t)wYCoord);
    if (!seq) return;

    s_spin_active = 1;
    s_spin_seq = seq;
    s_spin_idx = seq_last_idx(seq);
    if (s_spin_idx < 0) {
        s_spin_active = 0;
        s_spin_seq = 0;
        return;
    }

    Audio_PlaySFX_ArrowTiles();
    Player_SetSpinnerSpin(1);
    Player_StartSimulatedMovement(s_spin_seq, s_spin_idx);
}

void ViridianGymSpinners_Tick(void) {
    if (s_spin_active && Player_IsSimulatingMovement())
        apply_spinner_flicker();

    if (s_spin_active && !Player_IsSimulatingMovement()) {
        s_spin_active = 0;
        s_spin_seq = 0;
        s_spin_idx = 0;
        Player_SetSpinnerSpin(0);
    }
}
