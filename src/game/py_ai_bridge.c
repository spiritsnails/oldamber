#include "py_ai_bridge.h"

#include "../platform/hardware.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

static int s_py_ai_enabled = 0;
static char s_py_ai_script[260] = "mods/python_ai/trainer_move_ai.py";

void PyAI_SetEnabled(int enabled, const char *script_path) {
    s_py_ai_enabled = enabled ? 1 : 0;
    if (script_path && *script_path) {
        snprintf(s_py_ai_script, sizeof(s_py_ai_script), "%s", script_path);
    }
}

int PyAI_IsEnabled(void) {
    return s_py_ai_enabled;
}

const char *PyAI_GetScriptPath(void) {
    return s_py_ai_script;
}

static int py_ai_slot_disabled(uint8_t slot) {
    uint8_t dis = (wEnemyDisabledMove >> 4) & 0x0F;
    return (dis != 0 && dis == (uint8_t)(slot + 1));
}

static int py_ai_is_valid_pick(uint8_t slot, uint8_t move) {
    if (slot > 3) return 0;
    if (move == 0) return 0;
    if (wEnemyMon.moves[slot] != move) return 0;
    if (py_ai_slot_disabled(slot)) return 0;
    return 1;
}

int PyAI_ChooseEnemyMove(uint8_t *out_slot, uint8_t *out_move) {
    char cmd[768];
    char line[128];
    int slot = -1;
    int move = -1;
    FILE *fp;
    if (!out_slot || !out_move) return 0;
    if (!s_py_ai_enabled || !s_py_ai_script[0]) return 0;
    if (wIsInBattle != 2) return 0;

    snprintf(
        cmd,
        sizeof(cmd),
        "python \"%s\" %u %u %u %u %u %u %u %u %u %u",
        s_py_ai_script,
        (unsigned)wEnemyMon.moves[0],
        (unsigned)wEnemyMon.moves[1],
        (unsigned)wEnemyMon.moves[2],
        (unsigned)wEnemyMon.moves[3],
        (unsigned)((wEnemyDisabledMove >> 4) & 0x0F),
        (unsigned)wEnemyMon.hp,
        (unsigned)wEnemyMon.max_hp,
        (unsigned)wBattleMon.hp,
        (unsigned)wBattleMon.max_hp,
        (unsigned)wEnemyMon.level
    );

    fp = POPEN(cmd, "r");
    if (!fp) return 0;
    if (!fgets(line, sizeof(line), fp)) {
        PCLOSE(fp);
        return 0;
    }
    PCLOSE(fp);

    if (sscanf(line, "%d %d", &slot, &move) != 2) return 0;
    if (slot < 0 || slot > 3) return 0;
    if (move < 1 || move > 255) return 0;
    if (!py_ai_is_valid_pick((uint8_t)slot, (uint8_t)move)) return 0;

    *out_slot = (uint8_t)slot;
    *out_move = (uint8_t)move;
    return 1;
}
