
#include "route25_scripts.h"
#include "player.h"
#include "overworld.h"
#include "amberscript_mapbank.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"
#include <string.h>

void Route25Scripts_Tick(void) {
    const char *name = AmberScript_MapBank_NameForRealId(wCurMap);

    if (!name || strcasecmp(name, "Route25") != 0) return;

    if (CheckEvent(EVENT_LEFT_BILLS_HOUSE_AFTER_HELPING)) return;
    if (!CheckEvent(EVENT_MET_BILL_2)) return;
    if (!CheckEvent(EVENT_GOT_SS_TICKET)) return;

    SetEvent(EVENT_LEFT_BILLS_HOUSE_AFTER_HELPING);
}
