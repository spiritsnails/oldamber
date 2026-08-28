
#include "cinnabar_island_scripts.h"
#include "overworld.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"

#define MAP_CINNABAR_ISLAND 0x08

void CinnabarIslandScripts_OnMapLoad(void) {
    if (Map_CurrentRealId() != MAP_CINNABAR_ISLAND) return;
    ClearEvent(EVENT_MANSION_SWITCH_ON);
    ClearEvent(EVENT_LAB_STILL_REVIVING_FOSSIL);
}
