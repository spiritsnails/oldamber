#include "seafoam_scripts.h"
#include "rom_text.h"
#include "player.h"
#include "npc.h"
#include "warp.h"
#include "bicycle.h"
#include "overworld.h"
#include "text.h"
#include "constants.h"
#include "battle/battle_loop.h"
#include "../platform/audio.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"
#include <stdint.h>
#include <stdio.h>

#define MAP_SEAFOAM_ISLANDS_1F  0xC0
#define MAP_SEAFOAM_ISLANDS_B1F 0x9F
#define MAP_SEAFOAM_ISLANDS_B2F 0xA0
#define MAP_SEAFOAM_ISLANDS_B3F 0xA1
#define MAP_SEAFOAM_ISLANDS_B4F 0xA2

typedef struct {
    int x;
    int y;
} coord_t;

typedef struct {
    uint8_t map_id;
    coord_t holes[2];
    uint16_t event1;
    uint16_t event2;
    int hide_idx_1;
    int hide_idx_2;
} seafoam_hole_rule_t;

#define DIR_DOWN  0
#define DIR_UP    1
#define DIR_LEFT  2
#define DIR_RIGHT 3

static const int8_t kB3CurrentNearSteps[]        = { DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_DOWN, DIR_DOWN, DIR_DOWN, -1 };

static const int8_t kB3StrongCurrentLeft[]  = { DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_RIGHT, DIR_RIGHT, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, -1 };
static const int8_t kB3StrongCurrentRight[] = { DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_RIGHT, DIR_RIGHT, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_LEFT, -1 };
static const int8_t kB4StrongCurrentLeft[]  = { DIR_UP, DIR_UP, DIR_UP, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_UP, -1 };
static const int8_t kB4StrongCurrentRight[] = { DIR_UP, DIR_UP, DIR_UP, DIR_RIGHT, DIR_RIGHT, DIR_UP, -1 };

static int s_move_object_armed = 0;

static int s_b4_current_active = 0;
static const int8_t kB4ExitPush1[]               = { DIR_UP, -1 };
static const int8_t kB4ExitPush2[]               = { DIR_UP, DIR_UP, -1 };
#define kArticunoBattleText (RomText("_PowerPlantZapdosBattleText"))
static int s_articuno_pending_battle             = 0;
static int s_articuno_battle_ready               = 0;
static int s_articuno_battle_active              = 0;

static const seafoam_hole_rule_t kHoleRules[] = {
    { MAP_SEAFOAM_ISLANDS_1F,  { {17, 6}, {24, 6} }, EVENT_SEAFOAM1_BOULDER1_DOWN_HOLE, EVENT_SEAFOAM1_BOULDER2_DOWN_HOLE, 0, 1 },
    { MAP_SEAFOAM_ISLANDS_B1F, { {18, 6}, {23, 6} }, EVENT_SEAFOAM2_BOULDER1_DOWN_HOLE, EVENT_SEAFOAM2_BOULDER2_DOWN_HOLE, 0, 1 },
    { MAP_SEAFOAM_ISLANDS_B2F, { {19, 6}, {22, 6} }, EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE, EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE, 0, 1 },
    { MAP_SEAFOAM_ISLANDS_B3F, { { 3,16}, { 6,16} }, EVENT_SEAFOAM4_BOULDER1_DOWN_HOLE, EVENT_SEAFOAM4_BOULDER2_DOWN_HOLE, 1, 2 },
};

static int is_seafoam_map(uint8_t map_id) {
    return map_id == MAP_SEAFOAM_ISLANDS_1F ||
           map_id == MAP_SEAFOAM_ISLANDS_B1F ||
           map_id == MAP_SEAFOAM_ISLANDS_B2F ||
           map_id == MAP_SEAFOAM_ISLANDS_B3F ||
           map_id == MAP_SEAFOAM_ISLANDS_B4F;
}

static int seq_last_idx(const int8_t *seq) {
    int i = 0;
    while (seq[i] != -1) i++;
    return i - 1;
}

static int sf_cur_map(void) {
    int real = Map_CurrentRealId();
    return real >= 0 ? real : (int)wCurMap;
}

static void start_sim_seq(const int8_t *seq) {
    int last = seq_last_idx(seq);
    if (last < 0) return;
    Player_StartSimulatedMovement(seq, last);
}

static void seafoam_move_object_script(void) {
    const int map = sf_cur_map();

    if (map == MAP_SEAFOAM_ISLANDS_B3F) {
        if (CheckEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE) &&
            CheckEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE)) return;
        if ((int)wXCoord == 18) {
            printf("[seafoam] B3F strong current (left) at (%d,%d)\n", (int)wXCoord, (int)wYCoord);
            start_sim_seq(kB3StrongCurrentLeft);
        } else if ((int)wXCoord == 19) {
            printf("[seafoam] B3F strong current (right) at (%d,%d)\n", (int)wXCoord, (int)wYCoord);
            start_sim_seq(kB3StrongCurrentRight);
        } else {
            return;
        }

        Warp_SetForced(1);
        return;
    }

    if (map == MAP_SEAFOAM_ISLANDS_B4F) {
        if (CheckEvent(EVENT_SEAFOAM4_BOULDER1_DOWN_HOLE) &&
            CheckEvent(EVENT_SEAFOAM4_BOULDER2_DOWN_HOLE)) return;
        if ((int)wYCoord != 14) return;
        if ((int)wXCoord == 4) {
            printf("[seafoam] B4F strong current (left) at (4,14)\n");
            start_sim_seq(kB4StrongCurrentLeft);
        } else if ((int)wXCoord == 5) {
            printf("[seafoam] B4F strong current (right) at (5,14)\n");
            start_sim_seq(kB4StrongCurrentRight);
        } else {
            return;
        }

        s_b4_current_active = 1;
    }
}

void SeafoamScripts_ArmMoveObject(void) {
    s_move_object_armed = 1;
}

static void apply_boulder_visibility_for_current_map(void) {

    if (sf_cur_map() == MAP_SEAFOAM_ISLANDS_1F) {
        if (CheckEvent(EVENT_SEAFOAM1_BOULDER1_DOWN_HOLE)) NPC_HideSprite(0); else NPC_ShowSprite(0);
        if (CheckEvent(EVENT_SEAFOAM1_BOULDER2_DOWN_HOLE)) NPC_HideSprite(1); else NPC_ShowSprite(1);
        return;
    }

    if (sf_cur_map() == MAP_SEAFOAM_ISLANDS_B1F) {
        if (CheckEvent(EVENT_SEAFOAM1_BOULDER1_DOWN_HOLE) && !CheckEvent(EVENT_SEAFOAM2_BOULDER1_DOWN_HOLE)) NPC_ShowSprite(0); else NPC_HideSprite(0);
        if (CheckEvent(EVENT_SEAFOAM1_BOULDER2_DOWN_HOLE) && !CheckEvent(EVENT_SEAFOAM2_BOULDER2_DOWN_HOLE)) NPC_ShowSprite(1); else NPC_HideSprite(1);
        return;
    }

    if (sf_cur_map() == MAP_SEAFOAM_ISLANDS_B2F) {
        if (CheckEvent(EVENT_SEAFOAM2_BOULDER1_DOWN_HOLE) && !CheckEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE)) NPC_ShowSprite(0); else NPC_HideSprite(0);
        if (CheckEvent(EVENT_SEAFOAM2_BOULDER2_DOWN_HOLE) && !CheckEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE)) NPC_ShowSprite(1); else NPC_HideSprite(1);
        return;
    }

    if (sf_cur_map() == MAP_SEAFOAM_ISLANDS_B3F) {
        if (CheckEvent(EVENT_SEAFOAM4_BOULDER1_DOWN_HOLE)) NPC_HideSprite(1); else NPC_ShowSprite(1);
        if (CheckEvent(EVENT_SEAFOAM4_BOULDER2_DOWN_HOLE)) NPC_HideSprite(2); else NPC_ShowSprite(2);
        if (CheckEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE)) NPC_ShowSprite(4); else NPC_HideSprite(4);
        if (CheckEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE)) NPC_ShowSprite(5); else NPC_HideSprite(5);

        NPC_ShowSprite(0);
        NPC_ShowSprite(3);
        return;
    }

    if (sf_cur_map() == MAP_SEAFOAM_ISLANDS_B4F) {
        if (CheckEvent(EVENT_SEAFOAM4_BOULDER1_DOWN_HOLE)) NPC_ShowSprite(0); else NPC_HideSprite(0);
        if (CheckEvent(EVENT_SEAFOAM4_BOULDER2_DOWN_HOLE)) NPC_ShowSprite(1); else NPC_HideSprite(1);
        if (CheckEvent(EVENT_BEAT_ARTICUNO)) NPC_HideSprite(2); else NPC_ShowSprite(2);
        return;
    }
}

void SeafoamScripts_OnMapLoad(void) {
    if (!is_seafoam_map(sf_cur_map())) return;
    if (sf_cur_map() == MAP_SEAFOAM_ISLANDS_1F) {
        SetEvent(EVENT_IN_SEAFOAM_ISLANDS);
    }
    apply_boulder_visibility_for_current_map();
}

static void seafoam_position_scripts(void);

void SeafoamScripts_Tick(void) {
    if (!is_seafoam_map(sf_cur_map())) return;

    if (s_articuno_pending_battle &&
        !Text_IsOpen() &&
        !Audio_IsCryPlaying()) {
        s_articuno_pending_battle = 0;
        s_articuno_battle_ready = 1;
    }

    uint8_t pushed_map;
    int pushed_x, pushed_y;
    if (Player_ConsumePushedBoulderEvent(&pushed_map, &pushed_x, &pushed_y) &&
        is_seafoam_map(pushed_map)) {
        for (int i = 0; i < (int)(sizeof(kHoleRules) / sizeof(kHoleRules[0])); i++) {
            const seafoam_hole_rule_t *r = &kHoleRules[i];
            if (pushed_map != r->map_id) continue;

            if (pushed_x == r->holes[0].x && pushed_y == r->holes[0].y) {
                SetEvent(r->event1);
                if (sf_cur_map() == pushed_map) NPC_HideSprite(r->hide_idx_1);
                printf("[seafoam] boulder fell map=%u hole=1 at (%d,%d) set_event=%u\n",
                       (unsigned)pushed_map, pushed_x, pushed_y, (unsigned)r->event1);
                break;
            }
            if (pushed_x == r->holes[1].x && pushed_y == r->holes[1].y) {
                SetEvent(r->event2);
                if (sf_cur_map() == pushed_map) NPC_HideSprite(r->hide_idx_2);
                printf("[seafoam] boulder fell map=%u hole=2 at (%d,%d) set_event=%u\n",
                       (unsigned)pushed_map, pushed_x, pushed_y, (unsigned)r->event2);
                break;
            }
        }
    }

    if (s_b4_current_active) {

        int rem = Player_GetSimulatedStepsRemaining();
        if (rem == 0 || (rem < 0 && !Player_IsMoving())) {
            s_b4_current_active = 0;
            wWalkBikeSurfState = 0;
            printf("[seafoam] B4F current: ashore at (%d,%d)\n",
                   (int)wXCoord, (int)wYCoord);
            Bicycle_PlayDefaultMusic();
        }
    }

    if (wWalkBikeSurfState != 2) return;
    if (Player_IsMoving() || Player_IsSimulatingMovement()) return;

    if (s_move_object_armed) {
        s_move_object_armed = 0;
        seafoam_move_object_script();
        return;
    }

    seafoam_position_scripts();
}

static void seafoam_position_scripts(void) {

    if (sf_cur_map() == MAP_SEAFOAM_ISLANDS_B3F &&
        !(CheckEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE) &&
          CheckEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE))) {
        if ((int)wYCoord == 8 && (int)wXCoord == 15) {
            printf("[seafoam] B3F current trigger at (%d,%d) flags S3=[%d,%d]\n",
                   (int)wXCoord, (int)wYCoord,
                   CheckEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE),
                   CheckEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE));
            start_sim_seq(kB3CurrentNearSteps);

            Warp_SetForced(1);
            return;
        }
    }

    if (sf_cur_map() == MAP_SEAFOAM_ISLANDS_B4F) {
        if (!(CheckEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE) &&
              CheckEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE))) {

            const int8_t *push = NULL;
            if      ((int)wXCoord == 20 && (int)wYCoord == 17) push = kB4ExitPush2;
            else if ((int)wXCoord == 21 && (int)wYCoord == 17) push = kB4ExitPush2;
            else if ((int)wXCoord == 20 && (int)wYCoord == 16) push = kB4ExitPush1;
            else if ((int)wXCoord == 21 && (int)wYCoord == 16) push = kB4ExitPush1;
            if (push) {
                printf("[seafoam] B4F exit push trigger at (%d,%d)\n",
                       (int)wXCoord, (int)wYCoord);
                start_sim_seq(push);
                Warp_SetForced(0);
                return;
            }
        }
    }
}

void SeafoamScripts_ArticunoInteract(void) {
    if (sf_cur_map() != MAP_SEAFOAM_ISLANDS_B4F) return;
    if (CheckEvent(EVENT_BEAT_ARTICUNO)) return;
    if (s_articuno_pending_battle || s_articuno_battle_ready || s_articuno_battle_active) return;
    Text_ShowASCII(kArticunoBattleText);
    Audio_PlayCry(SPECIES_ARTICUNO);
    s_articuno_pending_battle = 1;
}

int SeafoamScripts_ConsumeArticunoBattle(void) {
    if (!s_articuno_battle_ready) return 0;
    s_articuno_battle_ready = 0;
    s_articuno_battle_active = 1;
    return 1;
}

int SeafoamScripts_ConsumeArticunoPostBattle(void) {
    if (!s_articuno_battle_active) return 0;
    s_articuno_battle_active = 0;
    return 1;
}

void SeafoamScripts_OnArticunoBattleOutcome(uint8_t battle_outcome) {
    if (battle_outcome == BATTLE_OUTCOME_WILD_VICTORY ||
        battle_outcome == BATTLE_OUTCOME_CAUGHT) {
        SetEvent(EVENT_BEAT_ARTICUNO);
        if (sf_cur_map() == MAP_SEAFOAM_ISLANDS_B4F) NPC_HideSprite(2);
    }
}

void SeafoamScripts_StepCheck(void) {
    if (!is_seafoam_map(sf_cur_map())) return;

    if (Player_IsSimulatingMovement()) return;

    if (s_move_object_armed) {
        s_move_object_armed = 0;
        seafoam_move_object_script();
        return;
    }
    seafoam_position_scripts();
}
