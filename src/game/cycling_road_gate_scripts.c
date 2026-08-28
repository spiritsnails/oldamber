
#include "cycling_road_gate_scripts.h"
#include "rom_text.h"
#include "glitches.h"
#include "text.h"
#include "player.h"
#include "inventory.h"
#include "bicycle.h"
#include "overworld.h"
#include "constants.h"
#include "../platform/hardware.h"
#include "../platform/input.h"
#include <stdio.h>
#include <string.h>

#define MAP_ROUTE16GATE1F 0xba
#define MAP_ROUTE18GATE1F 0xbe

enum {
    SCRIPT_DEFAULT = 0,
    SCRIPT_PLAYER_MOVING_UP,
    SCRIPT_GUARD,
    SCRIPT_PLAYER_MOVING_RIGHT,
    SCRIPT_DEFAULT_TEXT_OPEN,
    SCRIPT_GUARD_TEXT_OPEN,
};

typedef struct {
    const char *map_name;
    const char *script_name;
    uint8_t     real_map_id;

    int         stop_x;
    int         stop_y_first;
    int         stop_y_last;
    const char *text_wait_up;
    const char *text_guard;

    int         cur_script;
    int         coord_index;
    int         sim_states_index;

    int         handled_x, handled_y;
} gate_script_t;

static gate_script_t s_gates[] = {
    { "Route16Gate1F", "route16gate1f", MAP_ROUTE16GATE1F, 4, 7, 10,
      "Route16Gate1FGuardWaitUpText",
      "Route16Gate1FGuardText.NoPedestriansAllowedText", 0, 0, 0, -1, -1 },
    { "Route18Gate1F", "route18gate1f", MAP_ROUTE18GATE1F, 4, 3, 6,
      "Route18Gate1FGuardExcuseMeText",
      "Route18Gate1FGuardText.YouNeedABicycleText", 0, 0, 0, -1, -1 },
};
#define N_GATES ((int)(sizeof s_gates / sizeof s_gates[0]))

static int s_registered[N_GATES];

static int is_bicycle_in_bag(void) {
    return Inventory_GetQty(ITEM_BICYCLE) > 0;
}

static int are_player_coords_in_array(gate_script_t *g) {
    if ((int)wXCoord != g->stop_x) return 0;
    for (int y = g->stop_y_first, idx = 1; y <= g->stop_y_last; y++, idx++) {
        if ((int)wYCoord == y) {
            g->coord_index = idx;
            return 1;
        }
    }
    return 0;
}

static int8_t s_sim_seq[8];
static void start_simulating_joypad_states(gate_script_t *g, int dir, int count) {
    if (count > (int)sizeof(s_sim_seq) - 1) count = (int)sizeof(s_sim_seq) - 1;
    for (int i = 0; i < count; i++) s_sim_seq[i] = (int8_t)dir;
    s_sim_seq[count] = -1;
    g->sim_states_index = count;
    Player_StartSimulatedMovement(s_sim_seq, count - 1);
}

static int sim_states_running(gate_script_t *g) {
    if (g->sim_states_index && !Player_IsSimulatingMovement() && !Player_IsMoving())
        g->sim_states_index = 0;
    return g->sim_states_index != 0;
}

#define DIR_UP    1
#define DIR_RIGHT 3

static void trace_state(gate_script_t *g, const char *what) {
    static const char *kNames[] = {
        "DEFAULT", "PLAYER_MOVING_UP", "GUARD", "PLAYER_MOVING_RIGHT",
        "DEFAULT_TEXT_OPEN", "GUARD_TEXT_OPEN"
    };
    printf("[cycroad] %-18s %-18s player=(%d,%d) coord_index=%d "
           "joyign=%02x held=%02x raw=%02x moving=%d\n",
           what, kNames[g->cur_script], (int)wXCoord, (int)wYCoord,
           g->coord_index, wJoyIgnore, hJoyHeld, Input_RawHeld(),
           Player_IsMoving());
    fflush(stdout);
}

static void run_gate_script(gate_script_t *g) {

    Bicycle_ClearAlwaysOnBike();

    if (Player_IsMoving() || Player_IsSimulatingMovement()) return;

    switch (g->cur_script) {

    case SCRIPT_DEFAULT: {

        if (is_bicycle_in_bag()) return;

        if (!are_player_coords_in_array(g)) {

            g->handled_x = g->handled_y = -1;
            return;
        }

        if ((int)wXCoord == g->handled_x && (int)wYCoord == g->handled_y) return;

        Text_ShowASCII(RomText(g->text_wait_up));
        g->cur_script = SCRIPT_DEFAULT_TEXT_OPEN;
        return;
    }

    case SCRIPT_DEFAULT_TEXT_OPEN: {
        if (Text_IsOpen()) return;

        hJoyHeld = 0;
        hJoyHeld = (uint8_t)(Input_RawHeld() & ~wJoyIgnore);

        if (g->coord_index == 1) {

            g->cur_script = SCRIPT_GUARD;
            return;
        }

        start_simulating_joypad_states(g, DIR_UP, g->coord_index - 1);

        g->cur_script = SCRIPT_PLAYER_MOVING_UP;
        return;
    }

    case SCRIPT_PLAYER_MOVING_UP: {

        if (sim_states_running(g)) return;

        wJoyIgnore = PAD_CTRL_PAD;

        g->cur_script = SCRIPT_GUARD;
    }

    case SCRIPT_GUARD: {

        Text_ShowASCII(RomText(g->text_guard));
        g->cur_script = SCRIPT_GUARD_TEXT_OPEN;
        return;
    }

    case SCRIPT_GUARD_TEXT_OPEN: {
        if (Text_IsOpen()) return;

        start_simulating_joypad_states(g, DIR_RIGHT, 1);

        g->cur_script = SCRIPT_PLAYER_MOVING_RIGHT;
        return;
    }

    case SCRIPT_PLAYER_MOVING_RIGHT: {

        if (sim_states_running(g)) return;

        wJoyIgnore = 0;

        g->handled_x = (int)wXCoord;
        g->handled_y = (int)wYCoord;
        g->cur_script = SCRIPT_DEFAULT;
        return;
    }

    default:
        g->cur_script = SCRIPT_DEFAULT;
        return;
    }
}

void CyclingRoadGate_RegisterMapScript(const char *map_name, const char *script_name) {
    if (!map_name || !script_name) return;
    for (int i = 0; i < N_GATES; i++) {
        if (strcmp(map_name, s_gates[i].map_name) == 0 &&
            strcmp(script_name, s_gates[i].script_name) == 0) {
            s_registered[i] = 1;
            return;
        }
    }
    printf("[map_script] no C script named '%s' for map '%s' -- line ignored\n",
           script_name, map_name);
}

void CyclingRoadGate_Reset(void) {
    for (int i = 0; i < N_GATES; i++) {
        s_gates[i].cur_script = SCRIPT_DEFAULT;
        s_gates[i].coord_index = 0;
        s_gates[i].sim_states_index = 0;
        s_gates[i].handled_x = s_gates[i].handled_y = -1;
    }
}

void CyclingRoadGate_Tick(void) {

    if (!Glitches_IsEnabled()) return;
    int real = Map_CurrentRealId();
    if (real < 0) return;
    for (int i = 0; i < N_GATES; i++) {
        if (!s_registered[i]) continue;
        if ((int)s_gates[i].real_map_id != real) continue;
        {
            gate_script_t *g = &s_gates[i];
            const int before = g->cur_script;
            run_gate_script(g);

            if (g->cur_script != before) trace_state(g, "->");
        }
        return;
    }
}
