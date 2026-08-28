
#include "game_corner_scripts.h"
#include "amberscript_mapbank.h"
#include "amberscript_tilemod.h"
#include "slot_machine.h"
#include "amberscript_core.h"
#include "overworld.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"
#include <string.h>
#include <stdio.h>

void GameCornerScripts_OnMapLoad(void) {

    const char *n = AmberScript_MapBank_NameForRealId(wCurMap);

    if (!n || strcasecmp(n, "GameCorner") != 0) return;

    SlotMachine_SelectLucky();

    {
        const char *q = CheckEvent(EVENT_FOUND_ROCKET_HIDEOUT)
                        ? "gamecorner_stairs_open_tr"
                        : "gamecorner_stairs_closed_tr";
        if (!AmberScript_TilePlaceCustom(q, 17, 4)) {
            printf("[gamecorner] hideout stair swap FAILED (%s not defined) -- "
                   "regenerate with tools/romimport/emit_kanto.py --all\n", q);
            fflush(stdout);
        }
    }
}

void GameCornerScripts_Tick(void) {
    const char *n;
    const char *q;
    uint8_t tiles[4], passable;

    if (!AmberScript_IsEnabled()) return;
    n = AmberScript_MapBank_NameForRealId(wCurMap);
    if (!n || strcasecmp(n, "GameCorner") != 0) return;

    q = CheckEvent(EVENT_FOUND_ROCKET_HIDEOUT) ? "gamecorner_stairs_open_tr"
                                               : "gamecorner_stairs_closed_tr";
    if (!AmberScript_ResolveNamedBlock(q, tiles, &passable)) return;
    if (Map_GetGameTile(17, 4) == tiles[2]) return;
    AmberScript_TilePlaceCustom(q, 17, 4);
}

void GameCorner_RocketScript(void) { }
