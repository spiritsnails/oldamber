
#include "route24_scripts.h"
#include "rom_text.h"
#include "text.h"
#include "player.h"
#include "overworld.h"
#include "music.h"
#include "inventory.h"
#include "../platform/audio.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"
#include <stdio.h>

#define MAP_ROUTE24   0x23

#define TRIGGER_X  10
#define TRIGGER_Y  15

#define ROCKET_CLASS  30
#define ROCKET_NO      6

typedef enum {
    RS_IDLE = 0,
    RS_PRE_TEXT_CONGRATS,
    RS_PRE_TEXT_PRIZE,
    RS_PRE_TEXT_NUGGET,
    RS_PRE_TEXT_ROCKET,
    RS_JINGLE_DELAY,
    RS_BATTLE_PENDING,
    RS_POST_TEXT,
    RS_CLEANUP,
} Route24State;

static Route24State g_state          = RS_IDLE;
static int          g_pending_battle = 0;
static int          g_battle_active  = 0;
static int          g_post_step      = 0;

static int          g_jingle_delay   = 0;

#define kCongratsText (RomText("_Route24CooltrainerM1YouBeatOurContestText"))

#define kPrizeText (RomText("_Route24CooltrainerM1YouJustEarnedAPrizeText"))

#define kNuggetText (RomText("_Route24CooltrainerM1ReceivedNuggetText"))

#define kJoinRocketText (RomText("Route24CooltrainerM1Text.JoinTeamRocketText"))

static char kDefeatedTextBuf[128];

void Route24Scripts_OnMapLoad(void) {

}

void Route24Scripts_StepCheck(void) {
    if (wCurMap != MAP_ROUTE24) return;
    if (g_state != RS_IDLE) return;
    if (CheckEvent(EVENT_GOT_NUGGET)) return;

    if ((int)wYCoord != TRIGGER_Y) return;
    if ((int)wXCoord != TRIGGER_X) return;

    printf("[route24] step trigger: Rocket encounter at (%d,%d)\n",
           (int)wXCoord, (int)wYCoord);

    Text_ShowASCII(kCongratsText);
    g_state = RS_PRE_TEXT_CONGRATS;
}

int Route24Scripts_IsActive(void) { return g_state != RS_IDLE; }

int Route24Scripts_GetPendingBattle(uint8_t *class_out, uint8_t *no_out) {
    if (!g_pending_battle) return 0;
    g_pending_battle = 0;
    *class_out = ROCKET_CLASS;
    *no_out    = ROCKET_NO;
    g_battle_active = 1;
    return 1;
}

int Route24Scripts_ConsumeRocketBattle(void) {
    if (!g_battle_active) return 0;
    g_battle_active = 0;
    return 1;
}

void Route24Scripts_OnVictory(void) {
    SetEvent(EVENT_BEAT_ROUTE24_ROCKET);
    g_post_step = 0;
    g_state = RS_POST_TEXT;
    printf("[route24] Rocket defeated — post-battle sequence\n");
}

void Route24Scripts_OnDefeat(void) {

    g_state = RS_IDLE;
}

void Route24Scripts_Tick(void) {
    if (g_state == RS_IDLE || g_state == RS_BATTLE_PENDING) return;

    switch (g_state) {

    case RS_PRE_TEXT_CONGRATS:
        if (Text_IsOpen()) { Text_Update(); return; }
        Audio_PlaySFX_GetItem1();
        Text_ShowASCII(kPrizeText);
        g_state = RS_PRE_TEXT_PRIZE;
        return;

    case RS_PRE_TEXT_PRIZE:
        if (Text_IsOpen()) { Text_Update(); return; }

        Inventory_Add(ITEM_NUGGET, 1);
        SetEvent(EVENT_GOT_NUGGET);
        Audio_PlaySFX_GetItem1();
        Text_SetItemName(ITEM_NUGGET);
        Text_ShowASCII(kNuggetText);
        g_state = RS_PRE_TEXT_NUGGET;
        return;

    case RS_PRE_TEXT_NUGGET:
        if (Text_IsOpen()) { Text_Update(); return; }
        Text_ShowASCII(kJoinRocketText);
        g_state = RS_PRE_TEXT_ROCKET;
        return;

    case RS_PRE_TEXT_ROCKET:
        if (Text_IsOpen()) { Text_Update(); return; }

        Music_Play(MUSIC_MEET_EVIL_TRAINER);
        g_jingle_delay = 24;
        g_state = RS_JINGLE_DELAY;
        return;

    case RS_JINGLE_DELAY:
        if (g_jingle_delay > 0) { g_jingle_delay--; return; }
        g_pending_battle = 1;
        g_state = RS_BATTLE_PENDING;
        return;

    case RS_POST_TEXT:
        if (!g_post_step) {
            snprintf(kDefeatedTextBuf, sizeof(kDefeatedTextBuf), "%s\f%s",
                     RomText("_Route24CooltrainerM1DefeatedText"),
                     RomText("_Route24CooltrainerM1YouCouldBecomeATopLeaderText"));
            Text_ShowASCII(kDefeatedTextBuf);
            g_post_step = 1;
            return;
        }
        if (Text_IsOpen()) { Text_Update(); return; }
        g_state = RS_CLEANUP;
        return;

    case RS_CLEANUP:
        g_state = RS_IDLE;
        printf("[route24] Rocket script complete\n");
        return;

    default:
        g_state = RS_IDLE;
        return;
    }
}
