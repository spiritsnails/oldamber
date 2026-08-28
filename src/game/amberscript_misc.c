
#include "amberscript_misc.h"
#include "amberscript_core.h"

#include "pokemon.h"
#include "safari_zone_scripts.h"
#include "battle/battle_exp.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../data/event_constants.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int AmberScript_Misc_TryHandle(const char *cmd, const char *verb, int n) {
    (void)n;

    if (strcmp(verb, "screenshot") == 0) {
        char path[200] = {0};
        if (!AmberScript_ParseArg(cmd, 1, path, sizeof(path))) {
            printf("[amberscript] screenshot usage: screenshot <path.bmp>\n");
        } else if (Display_SaveScreenshot(path)) {
            printf("[amberscript] screenshot: saved to %s\n", path);
        } else {
            printf("[amberscript] screenshot: failed (bad path?)\n");
        }
        AmberScript_WriteState();
        return 1;
    }

    if (strcmp(verb, "safari_state") == 0 ||
        strcmp(verb, "safari-state") == 0 ||
        strcmp(verb, "safaristate") == 0 ||
        strcmp(verb, "safari") == 0) {
        uint16_t steps = 0;
        uint8_t balls = 0;
        uint8_t script_state = 0;
        char steps_arg[32] = {0};
        SafariZoneScripts_DebugGetState(&steps, &balls, &script_state);
        if (AmberScript_ParseArg(cmd, 1, steps_arg, sizeof(steps_arg))) {
            int new_steps = atoi(steps_arg);
            if (new_steps < 0) new_steps = 0;
            if (new_steps > 502) new_steps = 502;
            SafariZoneScripts_DebugSetState((uint16_t)new_steps);
            SafariZoneScripts_DebugGetState(&steps, &balls, &script_state);
            printf("[amberscript] safari_state: steps set to %u\n", (unsigned)steps);
            AmberScript_WriteState();
            return 1;
        }
        printf("[amberscript] safari_state: in_zone=%d game_over=%d map=0x%02X pos=(%d,%d) steps=%u balls=%u script=%u\n",
               CheckEvent(EVENT_IN_SAFARI_ZONE) ? 1 : 0,
               CheckEvent(EVENT_SAFARI_GAME_OVER) ? 1 : 0,
               (unsigned)wCurMap, (int)wXCoord, (int)wYCoord,
               (unsigned)steps, (unsigned)balls, (unsigned)script_state);
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "giveteam") == 0) {
        static const uint8_t kTeam[] = {
            SPECIES_MEWTWO,
            SPECIES_DRAGONITE,
            SPECIES_ALAKAZAM,
            SPECIES_MACHAMP,
            SPECIES_GENGAR,
            SPECIES_LAPRAS,
        };
        wPartyCount = 6;
        for (int i = 0; i < 6; i++)
            Pokemon_InitMon(&wPartyMons[i], kTeam[i], 100);
        printf("[amberscript] giveteam -- party set to Mewtwo/Dragonite/Alakazam/Machamp/Gengar/Lapras @ lv100\n");
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "addexp") == 0) {
        int slot = 1, amount = 0;
        sscanf(cmd, "%*s %d %d", &slot, &amount);
        slot--;
        if (slot < 0 || slot >= (int)wPartyCount) {
            printf("[amberscript] addexp: slot must be 1-%d\n", wPartyCount);
        } else if (amount <= 0) {
            printf("[amberscript] addexp: amount must be > 0\n");
        } else {
            Battle_AddExpDirect((uint8_t)slot, (uint32_t)amount);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "exprate") == 0) {
        int rate = 100;
        sscanf(cmd, "%*s %d", &rate);
        if (rate < 1 || rate > 9999) {
            printf("[amberscript] exprate: value must be 1-9999 (100=1x, 200=2x, 300=3x)\n");
        } else {
            gDebugExpRate = rate;
            printf("[amberscript] exprate: exp multiplier set to %d%% (%d.%02dx)\n",
                   rate, rate / 100, rate % 100);
        }
        AmberScript_WriteState();
        return 1;
    }

    return 0;
}
