
#include "route2gate_scripts.h"
#include "rom_text.h"
#include "text.h"
#include "inventory.h"
#include "constants.h"
#include "../data/event_constants.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include <stdio.h>

#define MAP_ROUTE_2_GATE  0x31
#define ITEM_HM05         0xC8
#define OAK_AIDE_REQUIRED 10

typedef enum {
    RG_IDLE = 0,
    RG_TEXT_WAIT,
    RG_JINGLE_WAIT,
    RG_GIVE_WAIT,
} rg_state_t;

static rg_state_t s_state = RG_IDLE;

static const char kText_NeedMore[] =
    "Hi! I'm one of\nPROF. OAK's\naides!\nIf you've caught\n10 kinds of\nMonster, I'll\ngive you\nsomething!";

static const char kText_Received[] =
    "You've caught 10\nkinds of Monster!\nAs promised,\nhere's HM 05!\nFLASH will light\nup dark caves.";

#define kText_AlreadyGiven (RomText("_Route2GateOaksAideFlashExplanationText"))

static char kText_BagFullBuf[64];

static int count_owned(void) {
    int n = 0;
    for (int i = 0; i < 19; i++) {
        uint8_t b = wPokedexOwned[i];
        while (b) { n += b & 1; b >>= 1; }
    }
    return n;
}

void Route2GateScripts_OnMapLoad(void) {

    (void)0;
}

int Route2GateScripts_IsActive(void) {
    return s_state != RG_IDLE;
}

void Route2GateScripts_Tick(void) {
    switch (s_state) {
        case RG_IDLE:
            break;

        case RG_TEXT_WAIT:
            if (!Text_IsOpen())
                s_state = RG_IDLE;
            break;

        case RG_JINGLE_WAIT:
            if (!Audio_IsSFXPlaying_GetKeyItem()) {
                Text_ShowASCII(kText_Received);
                s_state = RG_GIVE_WAIT;
            }
            break;

        case RG_GIVE_WAIT:
            if (!Text_IsOpen())
                s_state = RG_IDLE;
            break;
    }
}
