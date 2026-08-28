
#include "gate_scripts.h"
#include "rom_text.h"
#include "badge.h"
#include "npc.h"
#include "text.h"
#include "player.h"
#include "music.h"
#include "inventory.h"
#include "bicycle.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "../data/event_constants.h"
#include "amberscript_mapbank.h"
#include <string.h>
#include <stdio.h>
#include "map_music.h"

extern int16_t wXCoord, wYCoord;

#define MAP_VIRIDIAN_CITY   0x01
#define MAP_PEWTER_CITY     0x02
#define MAP_ROUTE22         0x21
#define MAP_ROUTE23         0x22
#define MAP_ROUTE22GATE     0xc1
#define MAP_ROUTE5GATE      0x46
#define MAP_ROUTE6GATE      0x49
#define MAP_ROUTE7GATE      0x4c
#define MAP_ROUTE8GATE      0x4f
#define MAP_ROUTE16GATE1F   0xba
#define MAP_ROUTE18GATE1F   0xbe
#define MAP_CINNABAR_ISLAND 0x08

typedef enum {
    CRS_IDLE = 0,
    CRS_WAIT_TEXT1,
    CRS_MOVE_UP,
    CRS_WAIT_TEXT2,
    CRS_MOVE_RIGHT,
} CyclingRoadState;

static CyclingRoadState s_cycling_state = CRS_IDLE;
static int s_cycling_up_steps = 0;
static int s_cycling_map = 0;
static int s_cycling_coord_index = 0;
static int s_route23_push = 0;
static int s_route22_push = 0;
static int s_route22_checked = 0;
static int s_badge_jingle_pending = 0;

static void pass_guard(uint16_t event_flag) {
    if (event_flag) SetEvent(event_flag);
    NPC_HideSprite(NPC_GetLastInteracted());
}

static void queue_badge_jingle(void) {
    s_badge_jingle_pending = 1;
}

void Gate_LoadMap(void) {
    if (wCurMap != MAP_ROUTE23) {
        s_route23_push = 0;
    }
    if (wCurMap != MAP_ROUTE22GATE) {
        s_route22_push = 0;
        s_route22_checked = 0;
    }

    if (wCurMap == MAP_ROUTE16GATE1F || wCurMap == MAP_ROUTE18GATE1F) {
        Bicycle_ClearAlwaysOnBike();
    } else {

        s_cycling_state = CRS_IDLE;
        s_cycling_up_steps = 0;
        s_cycling_map = 0;
        s_cycling_coord_index = 0;
        wJoyIgnore = 0;
    }

    {
        const char *nm = AmberScript_MapBank_NameForRealId(wCurMap);

        if (nm && strcasecmp(nm, "PewterCity") == 0)
            ClearEvent(EVENT_BOUGHT_MUSEUM_TICKET);
    }

    switch (wCurMap) {

    case MAP_ROUTE22GATE:

        break;

    case MAP_ROUTE23:

        break;

#if 0
    case MAP_PEWTER_CITY:

        if (CheckEvent(EVENT_BEAT_BROCK))
            NPC_HideSprite(4);
        break;

    case MAP_VIRIDIAN_CITY:

        if (CheckEvent(EVENT_GOT_POKEDEX)) {
            NPC_HideSprite(4);
        } else {
            NPC_HideSprite(6);
        }
        break;
#endif

    default:
        break;
    }
}

void Gate_ViridianOldMan(void) {
    if (CheckEvent(EVENT_GOT_POKEDEX)) {

        NPC_HideSprite(NPC_GetLastInteracted());
        Text_ShowASCII(RomText("ViridianCityOldManText.HadMyCoffeeNowText"));
    } else {

        Text_ShowASCII(RomText("ViridianCityOldManSleepyText.PrivatePropertyText"));
    }
}

#define DIR_DOWN  0
#define DIR_UP    1
#define DIR_LEFT  2
#define DIR_RIGHT 3

#define PEWTER_YOUNGSTER_IDX   4
#define PEWTER_NPC_EXIT_STEPS  5

typedef enum {
    PEW_IDLE = 0,
    PEW_TEXT1,
    PEW_WALKING,
    PEW_TEXT2,
    PEW_NPC_EXIT,
    PEW_EXIT_WAIT
} PewterEscortState;

static PewterEscortState g_pewter_state             = PEW_IDLE;
static int               g_pewter_dirs[52];
static int               g_pewter_dir_len            = 0;
static int               g_pewter_dir_idx            = 0;
static int               g_pewter_npc_dirs[52];
static int               g_pewter_npc_len            = 0;
static int               g_pewter_npc_idx            = 0;
static int               g_pewter_npc_step           = 0;
static int               g_pewter_exit_wait          = 0;

static void pewter_set_route(int16_t tx, int16_t ty) {
    int n = 0;

    if (tx == 34 && ty == 16) {
        g_pewter_dirs[n++] = DIR_RIGHT;
        g_pewter_dirs[n++] = DIR_DOWN;
        g_pewter_dirs[n++] = DIR_DOWN;
        g_pewter_dirs[n++] = DIR_LEFT;
    }
    if (tx == 35 && ty == 17) {

        g_pewter_dirs[n++] = DIR_LEFT;
        g_pewter_dirs[n++] = DIR_RIGHT;
        g_pewter_dirs[n++] = DIR_DOWN;
        g_pewter_dirs[n++] = DIR_LEFT;
    } else if (tx == 36 && ty == 17) {

        g_pewter_dirs[n++] = -1;
        g_pewter_dirs[n++] = DIR_LEFT;
        g_pewter_dirs[n++] = DIR_DOWN;
        g_pewter_dirs[n++] = DIR_LEFT;
    } else if (tx == 37 && ty == 18) {

        g_pewter_dirs[n++] = -1;
        g_pewter_dirs[n++] = DIR_LEFT;
        g_pewter_dirs[n++] = DIR_LEFT;
        g_pewter_dirs[n++] = DIR_LEFT;
    } else if (tx == 37 && ty == 19) {

        g_pewter_dirs[n++] = DIR_LEFT;
        g_pewter_dirs[n++] = DIR_UP;
        g_pewter_dirs[n++] = DIR_LEFT;
        g_pewter_dirs[n++] = DIR_LEFT;
    }

    for (int i = 0; i < 14; i++) g_pewter_dirs[n++] = DIR_LEFT;
    for (int i = 0; i < 5;  i++) g_pewter_dirs[n++] = DIR_UP;
    for (int i = 0; i < 11; i++) g_pewter_dirs[n++] = DIR_LEFT;
    for (int i = 0; i < 5;  i++) g_pewter_dirs[n++] = DIR_DOWN;
    for (int i = 0; i < 2;  i++) g_pewter_dirs[n++] = DIR_RIGHT;
    g_pewter_dirs[n++] = -1;

    g_pewter_dir_len = n;
    g_pewter_dir_idx = 0;

    int m = 0;
    for (int i = 0; i < 2;  i++) g_pewter_npc_dirs[m++] = DIR_DOWN;
    for (int i = 0; i < 15; i++) g_pewter_npc_dirs[m++] = DIR_LEFT;
    for (int i = 0; i < 5;  i++) g_pewter_npc_dirs[m++] = DIR_UP;
    for (int i = 0; i < 11; i++) g_pewter_npc_dirs[m++] = DIR_LEFT;
    for (int i = 0; i < 5;  i++) g_pewter_npc_dirs[m++] = DIR_DOWN;
    for (int i = 0; i < 3;  i++) g_pewter_npc_dirs[m++] = DIR_RIGHT;
    g_pewter_npc_len = m;
    g_pewter_npc_idx = 0;
}

static void pewter_begin_escort(void) {
    if (CheckEvent(EVENT_BEAT_BROCK)) return;
    if (g_pewter_state != PEW_IDLE) return;

    wJoyIgnore = PAD_CTRL_PAD;
    Text_ShowASCII(RomText("PewterCityYoungsterText.YoureATrainerFollowMeText"));
    pewter_set_route(wXCoord, wYCoord);
    g_pewter_state = PEW_TEXT1;
}

int Gate_PewterIsActive(void) {
    return g_pewter_state != PEW_IDLE;
}

void Gate_PewterEastCheck(void) {
    if (wCurMap != MAP_PEWTER_CITY) return;

    return;

    if (!((wXCoord == 35 && wYCoord == 17) ||
          (wXCoord == 36 && wYCoord == 17) ||
          (wXCoord == 37 && wYCoord == 18) ||
          (wXCoord == 37 && wYCoord == 19))) return;
    pewter_begin_escort();
}

void Gate_PewterTick(void) {
    if (g_pewter_state == PEW_IDLE) return;

    if (g_pewter_state == PEW_TEXT1) {
        if (Text_IsOpen()) return;
        Music_Play(MUSIC_MUSEUM_GUY);
        g_pewter_state = PEW_WALKING;
        return;
    }

    if (g_pewter_state == PEW_WALKING) {

        if (Player_IsMoving() || NPC_IsWalking(PEWTER_YOUNGSTER_IDX)) return;

        int player_done = (g_pewter_dir_idx >= g_pewter_dir_len);
        int npc_done    = (g_pewter_npc_idx >= g_pewter_npc_len);

        {
            static const char *dname[] = {"DOWN","UP","LEFT","RIGHT","NOOP"};
            int pi = g_pewter_dir_idx, ni = g_pewter_npc_idx;
            const char *pd = player_done ? "DONE" : (g_pewter_dirs[pi] < 0 ? dname[4] : dname[g_pewter_dirs[pi]]);
            const char *nd = npc_done    ? "DONE" : dname[g_pewter_npc_dirs[ni]];
            int ntx = -1, nty = -1;
            NPC_GetTilePos(PEWTER_YOUNGSTER_IDX, &ntx, &nty);
            printf("[pewter] step P=%d/%d N=%d/%d | player(%d,%d) %s | npc(%d,%d) %s\n",
                   pi, g_pewter_dir_len, ni, g_pewter_npc_len,
                   wXCoord, wYCoord, pd,
                   ntx, nty, nd);
        }

        if (player_done && npc_done) {
            Text_ShowASCII(RomText("PewterCityYoungsterGoTakeOnBrockText"));
            g_pewter_state = PEW_TEXT2;
            return;
        }

        if (!player_done) {
            int pdir = g_pewter_dirs[g_pewter_dir_idx++];
            if (pdir >= 0)
                Player_DoScriptedStep(pdir);
        }
        if (!npc_done)
            NPC_DoScriptedStep(PEWTER_YOUNGSTER_IDX, g_pewter_npc_dirs[g_pewter_npc_idx++]);
        return;
    }

    if (g_pewter_state == PEW_TEXT2) {
        if (Text_IsOpen()) return;
        g_pewter_npc_step = 0;
        g_pewter_state = PEW_NPC_EXIT;
        return;
    }

    if (g_pewter_state == PEW_NPC_EXIT) {
        if (NPC_IsWalking(PEWTER_YOUNGSTER_IDX)) return;
        if (g_pewter_npc_step < PEWTER_NPC_EXIT_STEPS) {
            NPC_DoScriptedStep(PEWTER_YOUNGSTER_IDX, DIR_RIGHT);
            g_pewter_npc_step++;
        } else {
            g_pewter_exit_wait = 60;
            g_pewter_state = PEW_EXIT_WAIT;
        }
        return;
    }

    if (g_pewter_state == PEW_EXIT_WAIT) {
        if (g_pewter_exit_wait > 0) {
            g_pewter_exit_wait--;
            return;
        }

        NPC_HideSprite(PEWTER_YOUNGSTER_IDX);
        NPC_SetTilePos(PEWTER_YOUNGSTER_IDX, 35, 16);
        NPC_ShowSprite(PEWTER_YOUNGSTER_IDX);
        wJoyIgnore = 0;

        MapMusic_Restart();
        g_pewter_state = PEW_IDLE;
        return;
    }
}

static int g_viridian_push_pending = 0;
static int g_viridian_push_uses_ledge = 0;
static int g_cinnabar_gym_lock_push_pending = 0;

void Gate_ViridianStepCheck(void) {
    if (wCurMap != MAP_VIRIDIAN_CITY) return;
    if (g_viridian_push_pending) return;

    if (!CheckEvent(EVENT_VIRIDIAN_GYM_OPEN) && wObtainedBadges == (uint8_t)~(1u << BIT_EARTHBADGE)) {
        SetEvent(EVENT_VIRIDIAN_GYM_OPEN);
    }

    if (!CheckEvent(EVENT_VIRIDIAN_GYM_OPEN) && wYCoord == 8 && wXCoord == 32) {
        Text_ShowASCII(RomText("ViridianCityGymLockedText"));
        g_viridian_push_pending = 1;
        g_viridian_push_uses_ledge = 1;
        return;
    }

    if (!CheckEvent(EVENT_GOT_POKEDEX) && wYCoord == 9 && wXCoord == 19) {
        Text_ShowASCII(RomText("ViridianCityOldManSleepyText.PrivatePropertyText"));
        g_viridian_push_pending = 1;
        g_viridian_push_uses_ledge = 0;

    }
}

void Gate_ViridianDoPush(void) {
    if (!g_viridian_push_pending) return;
    if (Text_IsOpen()) return;
    g_viridian_push_pending = 0;
    if (g_viridian_push_uses_ledge) {
        Player_DoScriptedStepWithLedge(DIR_DOWN);
    } else {
        Player_ForceStepDown();
    }
    g_viridian_push_uses_ledge = 0;
}

void Gate_CinnabarGymLockStepCheck(void) {
    if (wCurMap != MAP_CINNABAR_ISLAND) return;
    if (g_cinnabar_gym_lock_push_pending) return;
    if (Inventory_GetQty(ITEM_SECRET_KEY) > 0) return;
    if (wYCoord != 4 || wXCoord != 18) return;

    Text_ShowASCII(RomText("CinnabarIslandDoorIsLockedText"));
    g_cinnabar_gym_lock_push_pending = 1;
}

void Gate_CinnabarGymLockDoPush(void) {
    if (!g_cinnabar_gym_lock_push_pending) return;
    if (Text_IsOpen()) return;
    g_cinnabar_gym_lock_push_pending = 0;
    Player_DoScriptedStep(DIR_DOWN);
}

static const char *route22_no_boulderbadge_text(void) {
    static char buf[192];
    if (!buf[0])
        snprintf(buf, sizeof buf, "%s%s",
                 RomText("_Route22GateGuardNoBoulderbadgeText"),
                 RomText("_Route22GateGuardICantLetYouPassText"));
    return buf;
}

void Gate_Route22_Guard(void) {
    if (Badge_Has(BADGE_BOULDER)) {

        queue_badge_jingle();
        Text_ShowASCII(RomText("Route22GateGuardGoRightAheadText"));
    } else {
        Text_ShowASCII(route22_no_boulderbadge_text());
    }
}

void Gate_Route22GateTick(void) {
    if (wCurMap != MAP_ROUTE22GATE) return;
    wLastMap = (wYCoord < 4) ? MAP_ROUTE23 : MAP_ROUTE22;
}

void Gate_Route22StepCheck(void) {
    if (wCurMap != MAP_ROUTE22GATE) return;
    if (s_route22_push) return;
    if (s_route22_checked) return;

    if (wYCoord == 2 && (wXCoord == 4 || wXCoord == 5)) {
        if (Badge_Has(BADGE_BOULDER)) {

            s_route22_checked = 1;
            queue_badge_jingle();
            Text_ShowASCII(RomText("Route22GateGuardGoRightAheadText"));
        } else {
            Text_ShowASCII(route22_no_boulderbadge_text());
            s_route22_push = 1;
        }
    }
}

void Gate_Route22DoPush(void) {
    if (!s_route22_push) return;
    if (Text_IsOpen()) return;
    s_route22_push = 0;
    Player_DoScriptedStep(DIR_DOWN);
}

void Gate_Route23StepCheck(void) {
    if (wCurMap != MAP_ROUTE23) return;
    if (s_route23_push) return;

    if (wYCoord == 136) {
        if (!CheckEvent(EVENT_PASSED_CASCADEBADGE_CHECK)) {
            Gate_Route23_Cascade();
            if (!CheckEvent(EVENT_PASSED_CASCADEBADGE_CHECK)) s_route23_push = 1;
        }
        return;
    }
    if (wYCoord == 119) {
        if (!CheckEvent(EVENT_PASSED_THUNDERBADGE_CHECK)) {
            Gate_Route23_Thunder();
            if (!CheckEvent(EVENT_PASSED_THUNDERBADGE_CHECK)) s_route23_push = 1;
        }
        return;
    }
    if (wYCoord == 105) {
        if (!CheckEvent(EVENT_PASSED_RAINBOWBADGE_CHECK)) {
            Gate_Route23_Rainbow();
            if (!CheckEvent(EVENT_PASSED_RAINBOWBADGE_CHECK)) s_route23_push = 1;
        }
        return;
    }
    if (wYCoord == 96) {
        if (!CheckEvent(EVENT_PASSED_SOULBADGE_CHECK)) {
            Gate_Route23_Soul();
            if (!CheckEvent(EVENT_PASSED_SOULBADGE_CHECK)) s_route23_push = 1;
        }
        return;
    }
    if (wYCoord == 85) {
        if (!CheckEvent(EVENT_PASSED_MARSHBADGE_CHECK)) {
            Gate_Route23_Marsh();
            if (!CheckEvent(EVENT_PASSED_MARSHBADGE_CHECK)) s_route23_push = 1;
        }
        return;
    }
    if (wYCoord == 56) {
        if (!CheckEvent(EVENT_PASSED_VOLCANOBADGE_CHECK)) {
            Gate_Route23_Volcano();
            if (!CheckEvent(EVENT_PASSED_VOLCANOBADGE_CHECK)) s_route23_push = 1;
        }
        return;
    }
    if (wYCoord == 35) {

        if (wXCoord < 14 && !CheckEvent(EVENT_PASSED_EARTHBADGE_CHECK)) {
            Gate_Route23_Earth();
            if (!CheckEvent(EVENT_PASSED_EARTHBADGE_CHECK)) s_route23_push = 1;
        }
    }
}

void Gate_Route23DoPush(void) {
    if (wCurMap != MAP_ROUTE23) {
        s_route23_push = 0;
        return;
    }
    if (!s_route23_push) return;
    if (Text_IsOpen()) return;
    s_route23_push = 0;
    Player_DoScriptedStep(DIR_DOWN);
}

void Gate_BadgeJingleTick(void) {
    if (!s_badge_jingle_pending) return;
    if (Text_IsOpen()) return;
    s_badge_jingle_pending = 0;
    Audio_PlaySFX_GetItem1();
}

static char s_route23_badge_text[192];

static void route23_badge_gate(uint16_t passed_event, int badge_flag,
                               const char *badge_name) {
    if (Badge_Has(badge_flag) || CheckEvent(passed_event)) {
        SetEvent(passed_event);
        queue_badge_jingle();
        RomTextSplice(s_route23_badge_text, sizeof(s_route23_badge_text),
                     "_Route23OhThatIsTheBadgeText", "{badge}", badge_name);
    } else {
        RomTextSplice(s_route23_badge_text, sizeof(s_route23_badge_text),
                     "_Route23YouDontHaveTheBadgeYetText", "{badge}", badge_name);
    }
    Text_ShowASCII(s_route23_badge_text);
}

void Gate_Route23_Earth(void) {
    route23_badge_gate(EVENT_PASSED_EARTHBADGE_CHECK, BADGE_EARTH, "EARTHBADGE");
}

void Gate_Route23_Volcano(void) {
    route23_badge_gate(EVENT_PASSED_VOLCANOBADGE_CHECK, BADGE_VOLCANO, "VOLCANOBADGE");
}

void Gate_Route23_Marsh(void) {
    route23_badge_gate(EVENT_PASSED_MARSHBADGE_CHECK, BADGE_MARSH, "MARSHBADGE");
}

void Gate_Route23_Soul(void) {
    route23_badge_gate(EVENT_PASSED_SOULBADGE_CHECK, BADGE_SOUL, "SOULBADGE");
}

void Gate_Route23_Rainbow(void) {
    route23_badge_gate(EVENT_PASSED_RAINBOWBADGE_CHECK, BADGE_RAINBOW, "RAINBOWBADGE");
}

void Gate_Route23_Thunder(void) {
    route23_badge_gate(EVENT_PASSED_THUNDERBADGE_CHECK, BADGE_THUNDER, "THUNDERBADGE");
}

void Gate_Route23_Cascade(void) {
    route23_badge_gate(EVENT_PASSED_CASCADEBADGE_CHECK, BADGE_CASCADE, "CASCADEBADGE");
}

static int s_saffron_push_dir = -1;

void Gate_SaffronStepCheck(void) {
    if (CheckEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK)) return;
    if (s_saffron_push_dir >= 0) return;

    int trigger = 0, push_dir = 0;
    switch (wCurMap) {
    case MAP_ROUTE5GATE:
        if (wYCoord == 3 && (wXCoord == 3 || wXCoord == 4))
            { trigger = 1; push_dir = DIR_UP; }
        break;
    case MAP_ROUTE6GATE:
        if (wYCoord == 2 && (wXCoord == 3 || wXCoord == 4))
            { trigger = 1; push_dir = DIR_DOWN; }
        break;
    case MAP_ROUTE7GATE:
        if (wXCoord == 3 && (wYCoord == 3 || wYCoord == 4))
            { trigger = 1; push_dir = DIR_LEFT; }
        break;
    case MAP_ROUTE8GATE:
        if (wXCoord == 2 && (wYCoord == 3 || wYCoord == 4))
            { trigger = 1; push_dir = DIR_RIGHT; }
        break;
    default: return;
    }
    if (!trigger) return;

    Text_ShowASCII(RomText("SaffronGateGuardGeeImThirstyText"));
    s_saffron_push_dir = push_dir;
}

void Gate_SaffronDoPush(void) {
    if (s_saffron_push_dir < 0) return;
    if (Text_IsOpen()) return;
    int dir = s_saffron_push_dir;
    s_saffron_push_dir = -1;
    Player_DoScriptedStep(dir);
}

static int is_currently_on_bike(void) {
    return wWalkBikeSurfState == 1;
}

void Gate_CyclingRoadStepCheck(void) {
    if (s_cycling_state != CRS_IDLE) return;
    if (is_currently_on_bike()) return;

    if (wCurMap == MAP_ROUTE16GATE1F && wXCoord == 4 && wYCoord >= 7 && wYCoord <= 10) {
        s_cycling_map = MAP_ROUTE16GATE1F;
        s_cycling_coord_index = (int)wYCoord - 7 + 1;
        s_cycling_up_steps = s_cycling_coord_index - 1;
        hJoyHeld = 0;
        Text_ShowASCII(RomText("Route16Gate1FGuardWaitUpText"));
        s_cycling_state = CRS_WAIT_TEXT1;
        return;
    }

    if (wCurMap == MAP_ROUTE18GATE1F && wXCoord == 4 && wYCoord >= 3 && wYCoord <= 6) {
        s_cycling_map = MAP_ROUTE18GATE1F;
        s_cycling_coord_index = (int)wYCoord - 3 + 1;
        s_cycling_up_steps = s_cycling_coord_index - 1;
        hJoyHeld = 0;
        Text_ShowASCII(RomText("Route18Gate1FGuardExcuseMeText"));
        s_cycling_state = CRS_WAIT_TEXT1;
        return;
    }
}

void Gate_CyclingRoadTick(void) {
    if (s_cycling_state == CRS_IDLE) return;
    if (wCurMap != s_cycling_map) {
        wJoyIgnore = 0;
        s_cycling_up_steps = 0;
        s_cycling_map = 0;
        s_cycling_coord_index = 0;
        s_cycling_state = CRS_IDLE;
        return;
    }

    if (s_cycling_state == CRS_WAIT_TEXT1) {
        if (Text_IsOpen()) return;
        s_cycling_state = CRS_MOVE_UP;
        return;
    }

    if (s_cycling_state == CRS_MOVE_UP) {
        if (Player_IsMoving()) return;
        if (s_cycling_up_steps > 0) {
            Player_DoScriptedStep(DIR_UP);
            s_cycling_up_steps--;
            return;
        }

        wJoyIgnore = PAD_CTRL_PAD;
        if (s_cycling_map == MAP_ROUTE16GATE1F)
            Text_ShowASCII(RomText("Route16Gate1FGuardText.NoPedestriansAllowedText"));
        else
            Text_ShowASCII(RomText("Route18Gate1FGuardText.YouNeedABicycleText"));
        s_cycling_state = CRS_WAIT_TEXT2;
        return;
    }

    if (s_cycling_state == CRS_WAIT_TEXT2) {
        if (Text_IsOpen()) return;
        s_cycling_state = CRS_MOVE_RIGHT;
        return;
    }

    if (s_cycling_state == CRS_MOVE_RIGHT) {
        if (Player_IsMoving()) return;
        Player_DoScriptedStep(DIR_RIGHT);

        wJoyIgnore = 0;
        s_cycling_coord_index = 0;
        s_cycling_state = CRS_IDLE;
    }
}
