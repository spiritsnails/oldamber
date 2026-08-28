#include "rockethideout_scripts.h"
#include "rom_text.h"
#include "assetpack_bind.h"
#include "player.h"
#include "npc.h"
#include "inventory.h"
#include "overworld.h"
#include "text.h"
#include "elevator_menu.h"
#include "amberscript_tilemod.h"
#include "amberscript_mapbank.h"
#include "constants.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "../data/event_constants.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DIR_DOWN  0
#define DIR_UP    1
#define DIR_LEFT  2
#define DIR_RIGHT 3

typedef struct {
    uint8_t x, y;
    const int8_t *seq;
} spin_coord_t;

static int s_spin_active = 0;
static const int8_t *s_spin_seq = 0;
static int s_spin_idx = 0;

static const uint8_t kFacilityIdle_t032[16] = { 0x80,0x1C, 0x80,0x0E, 0x80,0x07, 0x80,0x03, 0x80,0x01, 0x80,0x00, 0x80,0x00, 0xFF,0x00 };
static const uint8_t kFacilityIdle_t033[16] = { 0xFF,0x00, 0x80,0x00, 0x80,0x00, 0x80,0x01, 0x80,0x03, 0x80,0x07, 0x80,0x0E, 0x80,0x1C };
static const uint8_t kFacilityIdle_t048[16] = { 0x01,0x38, 0x01,0x70, 0x01,0xE0, 0x01,0xC0, 0x01,0x80, 0x01,0x00, 0x01,0x00, 0xFF,0x00 };
static const uint8_t kFacilityIdle_t049[16] = { 0xFF,0x00, 0x01,0x00, 0x01,0x00, 0x01,0x80, 0x01,0xC0, 0x01,0xE0, 0x01,0x70, 0x01,0x38 };

static void apply_spinner_flicker(const char *map_prefix) {
    char name[40];
    int steps = Player_GetSimulatedStepsRemaining();

    int animated = (steps >= 0) && (((steps >> 1) & 1) == 0);

    snprintf(name, sizeof(name), "%s_t032", map_prefix);
    AmberScript_SetSubtilePixels(name, animated ? kSpinnerFrame0 : kFacilityIdle_t032);
    snprintf(name, sizeof(name), "%s_t033", map_prefix);
    AmberScript_SetSubtilePixels(name, animated ? kSpinnerFrame1 : kFacilityIdle_t033);
    snprintf(name, sizeof(name), "%s_t048", map_prefix);
    AmberScript_SetSubtilePixels(name, animated ? kSpinnerFrame2 : kFacilityIdle_t048);
    snprintf(name, sizeof(name), "%s_t049", map_prefix);
    AmberScript_SetSubtilePixels(name, animated ? kSpinnerFrame3 : kFacilityIdle_t049);
}

static int seq_last_idx(const int8_t *seq) {
    int i = 0;
    while (seq[i] != -1) i++;
    return i - 1;
}

static const int8_t kB2M1[]  = { DIR_LEFT, DIR_LEFT, -1 };
static const int8_t kB2M2[]  = { DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kB2M3[]  = { DIR_UP, DIR_UP, DIR_UP, DIR_UP, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kB2M4[]  = { DIR_UP, DIR_UP, DIR_UP, DIR_UP, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_UP, -1 };
static const int8_t kB2M5[]  = { DIR_LEFT, DIR_LEFT, DIR_UP, DIR_UP, DIR_UP, -1 };
static const int8_t kB2M6[]  = { DIR_DOWN, DIR_DOWN, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kB2M7[]  = { DIR_UP, DIR_UP, -1 };
static const int8_t kB2M8[]  = { DIR_UP, DIR_UP, DIR_UP, DIR_UP, -1 };
static const int8_t kB2M9[]  = { DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, -1 };
static const int8_t kB2M10[] = { DIR_UP, -1 };
static const int8_t kB2M11[] = { DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_UP, DIR_UP, DIR_UP, DIR_UP, -1 };
static const int8_t kB2M12[] = { DIR_DOWN, DIR_DOWN, -1 };
static const int8_t kB2M13[] = { DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, -1 };
static const int8_t kB2M14[] = { DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_UP, -1 };
static const int8_t kB2M15[] = { DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_UP, DIR_UP, DIR_UP, DIR_UP, DIR_UP, DIR_UP, -1 };
static const int8_t kB2M16[] = { DIR_UP, DIR_UP, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kB2M17[] = { DIR_UP, DIR_UP, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP, -1 };
static const int8_t kB2M18[] = { DIR_DOWN, DIR_DOWN, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_DOWN, DIR_DOWN, -1 };
static const int8_t kB2M19[] = { DIR_DOWN, DIR_DOWN, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kB2M20[] = { DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, -1 };
static const int8_t kB2M21[] = { DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_UP, DIR_UP, -1 };
static const int8_t kB2M22[] = { DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_UP, DIR_UP, DIR_UP, DIR_UP, -1 };
static const int8_t kB2M23[] = { DIR_UP, DIR_UP, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kB2M24[] = { DIR_RIGHT, DIR_DOWN, DIR_DOWN, -1 };
static const int8_t kB2M25[] = { DIR_RIGHT, -1 };
static const int8_t kB2M26[] = { DIR_DOWN, DIR_DOWN, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kB2M27[] = { DIR_DOWN, DIR_DOWN, DIR_LEFT, DIR_LEFT, -1 };
static const int8_t kB2M28[] = { DIR_UP, DIR_UP, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP, DIR_LEFT, DIR_LEFT, DIR_LEFT, -1 };
static const int8_t kB2M29[] = { DIR_DOWN, DIR_DOWN, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, -1 };
static const int8_t kB2M30[] = { DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_UP, DIR_UP, DIR_UP, DIR_UP, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, -1 };
static const int8_t kB2M31[] = { DIR_UP, DIR_UP, -1 };
static const int8_t kB2M32[] = { DIR_UP, -1 };
static const int8_t kB2M33[] = { DIR_UP, DIR_UP, DIR_UP, -1 };
static const int8_t kB2M34[] = { DIR_UP, DIR_UP, DIR_UP, DIR_UP, DIR_UP, -1 };
static const int8_t kB2M35[] = { DIR_RIGHT, DIR_DOWN, DIR_DOWN, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, -1 };
static const int8_t kB2M36[] = { DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_UP, DIR_UP, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, DIR_LEFT, -1 };

static const spin_coord_t kB2Spin[] = {
    { 4,  9, kB2M1  }, { 4, 11, kB2M2  }, { 4, 15, kB2M3  }, { 4, 16, kB2M4  },
    { 4, 19, kB2M1  }, { 4, 22, kB2M5  }, { 5, 14, kB2M6  }, { 6, 22, kB2M7  },
    { 6, 24, kB2M8  }, { 8,  9, kB2M9  }, { 8, 12, kB2M10 }, { 8, 15, kB2M8  },
    { 8, 19, kB2M9  }, { 8, 23, kB2M11 }, { 9, 14, kB2M12 }, { 9, 22, kB2M12 },
    {10,  9, kB2M13 }, {10, 10, kB2M14 }, {10, 15, kB2M15 }, {10, 17, kB2M16 },
    {10, 19, kB2M17 }, {10, 25, kB2M2  }, {11, 14, kB2M18 }, {11, 16, kB2M19 },
    {11, 18, kB2M12 }, {12,  9, kB2M20 }, {12, 11, kB2M21 }, {12, 13, kB2M22 },
    {12, 17, kB2M23 }, {13, 10, kB2M24 }, {13, 12, kB2M25 }, {13, 16, kB2M26 },
    {13, 18, kB2M27 }, {13, 19, kB2M28 }, {13, 22, kB2M29 }, {13, 23, kB2M30 },
    {14, 17, kB2M31 }, {15, 16, kB2M12 }, {16, 14, kB2M32 }, {16, 16, kB2M33 },
    {16, 18, kB2M34 }, {17, 10, kB2M35 }, {17, 11, kB2M36 },
};

static const int8_t kB3M1[]  = { DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP, DIR_UP, DIR_UP, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kB3M2[]  = { DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kB3M3[]  = { DIR_LEFT, DIR_LEFT, -1 };
static const int8_t kB3M4[]  = { DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kB3M5[]  = { DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP, DIR_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP, DIR_UP, -1 };
static const int8_t kB3M6[]  = { DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kB3M7[]  = { DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kB3M8[]  = { DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP, -1 };
static const int8_t kB3M9[]  = { DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP, DIR_UP, DIR_UP, -1 };
static const int8_t kB3M10[] = { DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, -1 };
static const int8_t kB3M11[] = { DIR_UP, DIR_UP, -1 };
static const int8_t kB3M12[] = { DIR_UP, -1 };

static const spin_coord_t kB3Spin[] = {
    {10, 13, kB3M6  }, {10, 19, kB3M1  }, {11, 18, kB3M2  }, {12, 11, kB3M3  },
    {12, 17, kB3M4  }, {12, 20, kB3M5  }, {13, 16, kB3M6  }, {14, 11, kB3M7  },
    {14, 15, kB3M6  }, {14, 17, kB3M8  }, {14, 19, kB3M9  }, {15, 16, kB3M7  },
    {15, 18, kB3M10 }, {16, 13, kB3M11 }, {17, 12, kB3M10 }, {18, 16, kB3M12 },
};

static const int8_t *find_spin_seq(uint8_t map, uint8_t x, uint8_t y) {
    const spin_coord_t *tbl = 0;
    int n = 0;
    const char *n_ = AmberScript_MapBank_NameForRealId(map);

    if (n_ && strcasecmp(n_, "RocketHideoutB2F") == 0) {
        tbl = kB2Spin;
        n = (int)(sizeof(kB2Spin) / sizeof(kB2Spin[0]));
    } else if (n_ && strcmp(n_, "RocketHideoutB3F") == 0) {
        tbl = kB3Spin;
        n = (int)(sizeof(kB3Spin) / sizeof(kB3Spin[0]));
    } else {
        return 0;
    }
    for (int i = 0; i < n; i++) {
        if (tbl[i].x == x && tbl[i].y == y) return tbl[i].seq;
    }
    return 0;
}

void RocketHideoutScripts_OnMapLoad(void) {
    s_spin_active = 0;
    s_spin_seq = 0;
    s_spin_idx = 0;
    Player_SetSpinnerSpin(0);
}

void RocketHideoutScripts_Tick(void) {
    const char *n = AmberScript_MapBank_NameForRealId(wCurMap);
    if (!s_spin_active && !Player_IsMoving())
        RocketHideoutScripts_StepCheck();

    if (s_spin_active && Player_IsSimulatingMovement()) {
        if (n && strcmp(n, "RocketHideoutB2F") == 0)
            apply_spinner_flicker("rockethideoutb2f");
        else if (n && strcmp(n, "RocketHideoutB3F") == 0)
            apply_spinner_flicker("rockethideoutb3f");
    }

    if (s_spin_active && !Player_IsSimulatingMovement()) {
        s_spin_active = 0;
        s_spin_seq = 0;
        s_spin_idx = 0;
        Player_SetSpinnerSpin(0);
    }
}

int RocketHideoutScripts_IsActive(void) {
    return s_spin_active || Player_IsSimulatingMovement();
}

void RocketHideoutScripts_StepCheck(void) {
    const int8_t *seq;
    if (s_spin_active) return;

    seq = find_spin_seq((uint8_t)wCurMap, (uint8_t)wXCoord, (uint8_t)wYCoord);
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

void RocketHideoutElevator_PanelInteract(void) {
    if (Inventory_GetQty(0x4A) > 0) {
        wDoNotWaitForButtonPress = 1;
        Text_ShowASCII(RomText("WhichFloorText"));
        ElevatorMenu_QueueOpenRocketHideout();
    } else {
        Text_ShowASCII(RomText("RocketHideoutElevatorText.AppearsToNeedKeyText"));
    }
}
