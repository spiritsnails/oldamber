#include "rival_starter.h"
#include "constants.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"
#include <stdio.h>

uint8_t RivalStarter_Get(void) {
    if (wRivalStarter == STARTER1 || wRivalStarter == STARTER2 || wRivalStarter == STARTER3) {
        return wRivalStarter;
    }

    if (CheckEvent(EVENT_GOT_STARTER)) {
        const int hide1 = CheckEvent(EVENT_HIDE_STARTER_BALL_1);
        const int hide2 = CheckEvent(EVENT_HIDE_STARTER_BALL_2);
        const int hide3 = CheckEvent(EVENT_HIDE_STARTER_BALL_3);
        if (hide2 && hide3 && !hide1) return STARTER3;
        if (hide1 && hide2 && !hide3) return STARTER2;
        if (hide1 && hide3 && !hide2) return STARTER1;
    }

    printf("[rival] warning: invalid wRivalStarter=0x%02X; defaulting STARTER1\n",
           (unsigned)wRivalStarter);
    return STARTER1;
}
