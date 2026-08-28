
#include "amberscript_zone_resets.h"
#include "amberscript_core.h"
#include "amberscript_saveops.h"

#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "text.h"
#include "../data/map_data.h"
#include "../data/event_data.h"
#include "../data/event_constants.h"
#include "../platform/hardware.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int Game_WarpToRealMap(uint8_t real_id, int x, int y);

static int s_walkoff_active = 0;
static int s_walkoff_idx = -1;
static int s_walkoff_phase = 0;
static int s_walkoff_pretext_frames = 0;

static int pks_start_npc_walkoff(int print_log) {
    int spawn_x = (int)wXCoord + 1;
    int spawn_y = (int)wYCoord;
    int npc_face = 0;
    int idx;
    if (s_walkoff_active && s_walkoff_idx >= 0) {
        NPC_DebugDespawn(s_walkoff_idx);
        s_walkoff_active = 0;
        s_walkoff_idx = -1;
        s_walkoff_phase = 0;
        s_walkoff_pretext_frames = 0;
    }
    if (wPlayerDirection == 0) npc_face = 0;
    else if (wPlayerDirection == 4) npc_face = 1;
    else if (wPlayerDirection == 8) npc_face = 2;
    else if (wPlayerDirection == 12) npc_face = 3;
    idx = NPC_DebugSpawn(0x01, spawn_x, spawn_y, npc_face, 0 );
    if (idx < 0) {
        if (print_log) printf("[amberscript] npc_walkoff: failed to spawn NPC (no free slot?)\n");
        return 0;
    }
    s_walkoff_active = 1;
    s_walkoff_idx = idx;
    s_walkoff_phase = 1;
    s_walkoff_pretext_frames = 24;
    wJoyIgnore = PAD_CTRL_PAD;
    if (print_log) {
        printf("[amberscript] npc_walkoff: spawned npc idx=%d at (%d,%d), dialogue->walkoff sequence started\n",
               idx, spawn_x, spawn_y);
    }
    return 1;
}

void AmberScript_ZoneResets_Tick(void) {
    if (!s_walkoff_active) return;

    if (s_walkoff_idx < 0 || s_walkoff_idx >= NPC_GetCount()) {
        s_walkoff_active = 0;
        s_walkoff_idx = -1;
        s_walkoff_phase = 0;
        s_walkoff_pretext_frames = 0;
        wJoyIgnore = 0;
    } else if (s_walkoff_phase == 1) {
        NPC_SetFacing(s_walkoff_idx,
            (wPlayerDirection == 4) ? 1 :
            (wPlayerDirection == 8) ? 2 :
            (wPlayerDirection == 12) ? 3 : 0);
        if (s_walkoff_pretext_frames > 0) {
            s_walkoff_pretext_frames--;
        } else if (!Text_IsOpen()) {
            Text_ShowASCII("Hello! I am a\namberscript NPC\ntest!\fHope this works!@");
            s_walkoff_phase = 2;
        }
    } else if (s_walkoff_phase == 2) {
        NPC_SetFacing(s_walkoff_idx,
            (wPlayerDirection == 4) ? 1 :
            (wPlayerDirection == 8) ? 2 :
            (wPlayerDirection == 12) ? 3 : 0);
        if (!Text_IsOpen()) {
            wJoyIgnore = 0;
            s_walkoff_phase = 3;
        }
    } else if (s_walkoff_phase == 3 && !NPC_IsWalking(s_walkoff_idx)) {
        int tx, ty, sy;
        NPC_GetTilePos(s_walkoff_idx, &tx, &ty);
        sy = ty * 2 + 1 - gCamY;
        if (sy < 0) {
            NPC_DebugDespawn(s_walkoff_idx);
            s_walkoff_active = 0;
            s_walkoff_idx = -1;
            s_walkoff_phase = 0;
            s_walkoff_pretext_frames = 0;
            printf("[amberscript] npc_walkoff: despawned after leaving top of screen\n");
        } else {
            NPC_DoScriptedStep(s_walkoff_idx, 1);
        }
    }
}

static void pks_seafoam_reset(void) {
    ClearEvent(EVENT_SEAFOAM1_BOULDER1_DOWN_HOLE);
    ClearEvent(EVENT_SEAFOAM1_BOULDER2_DOWN_HOLE);
    ClearEvent(EVENT_SEAFOAM2_BOULDER1_DOWN_HOLE);
    ClearEvent(EVENT_SEAFOAM2_BOULDER2_DOWN_HOLE);
    ClearEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE);
    ClearEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE);
    ClearEvent(EVENT_SEAFOAM4_BOULDER1_DOWN_HOLE);
    ClearEvent(EVENT_SEAFOAM4_BOULDER2_DOWN_HOLE);
    ClearEvent(EVENT_IN_SEAFOAM_ISLANDS);
    AmberScript_ReloadAfterStateLoad();

    ClearEvent(EVENT_SEAFOAM1_BOULDER1_DOWN_HOLE);
    ClearEvent(EVENT_SEAFOAM1_BOULDER2_DOWN_HOLE);
    ClearEvent(EVENT_SEAFOAM2_BOULDER1_DOWN_HOLE);
    ClearEvent(EVENT_SEAFOAM2_BOULDER2_DOWN_HOLE);
    ClearEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE);
    ClearEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE);
    ClearEvent(EVENT_SEAFOAM4_BOULDER1_DOWN_HOLE);
    ClearEvent(EVENT_SEAFOAM4_BOULDER2_DOWN_HOLE);
    Map_BuildScrollView();
    NPC_BuildView(gScrollPxX, gScrollPxY);
    printf("[amberscript] seafoam_reset: Seafoam flags now: S1[%d,%d] S2[%d,%d] S3[%d,%d] S4[%d,%d] IN=%d\n",
           CheckEvent(EVENT_SEAFOAM1_BOULDER1_DOWN_HOLE),
           CheckEvent(EVENT_SEAFOAM1_BOULDER2_DOWN_HOLE),
           CheckEvent(EVENT_SEAFOAM2_BOULDER1_DOWN_HOLE),
           CheckEvent(EVENT_SEAFOAM2_BOULDER2_DOWN_HOLE),
           CheckEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE),
           CheckEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE),
           CheckEvent(EVENT_SEAFOAM4_BOULDER1_DOWN_HOLE),
           CheckEvent(EVENT_SEAFOAM4_BOULDER2_DOWN_HOLE),
           CheckEvent(EVENT_IN_SEAFOAM_ISLANDS));
}

int AmberScript_ZoneResets_TryHandle(const char *cmd, const char *verb, int n) {
    (void)n;

    if (strcmp(verb, "seafoam_reset") == 0) {
        pks_seafoam_reset();
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "victoryroad_reset") == 0 ||
        strcmp(verb, "victory_road_reset") == 0 ||
        strcmp(verb, "vr_reset") == 0) {
        ClearEvent(EVENT_VICTORY_ROAD_1_BOULDER_ON_SWITCH);
        ClearEvent(EVENT_VICTORY_ROAD_2_BOULDER_ON_SWITCH1);
        ClearEvent(EVENT_VICTORY_ROAD_2_BOULDER_ON_SWITCH2);
        ClearEvent(EVENT_VICTORY_ROAD_3_BOULDER_ON_SWITCH1);
        ClearEvent(EVENT_VICTORY_ROAD_3_BOULDER_ON_SWITCH2);

        ClearEvent(EVENT_BEAT_VICTORY_ROAD_1_TRAINER_0);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_1_TRAINER_1);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_2_TRAINER_0);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_2_TRAINER_1);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_2_TRAINER_2);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_2_TRAINER_3);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_2_TRAINER_4);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_3_TRAINER_0);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_3_TRAINER_1);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_3_TRAINER_2);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_3_TRAINER_3);
        ClearEvent(EVENT_BEAT_MOLTRES);

        AmberScript_ReloadAfterStateLoad();
        Map_BuildScrollView();
        NPC_BuildView(gScrollPxX, gScrollPxY);

        printf("[amberscript] victoryroad_reset: switches=[1F:%d 2F:%d,%d 3F:%d,%d] moltres=%d\n",
               CheckEvent(EVENT_VICTORY_ROAD_1_BOULDER_ON_SWITCH),
               CheckEvent(EVENT_VICTORY_ROAD_2_BOULDER_ON_SWITCH1),
               CheckEvent(EVENT_VICTORY_ROAD_2_BOULDER_ON_SWITCH2),
               CheckEvent(EVENT_VICTORY_ROAD_3_BOULDER_ON_SWITCH1),
               CheckEvent(EVENT_VICTORY_ROAD_3_BOULDER_ON_SWITCH2),
               CheckEvent(EVENT_BEAT_MOLTRES));
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "seafoam_state") == 0) {
        char mode[32] = {0};
        if (!AmberScript_ParseArg(cmd, 1, mode, sizeof(mode))) {
            printf("[amberscript] seafoam_state usage: seafoam_state <reset|b3_current|b4_current>\n");
            AmberScript_WriteState();
            return 1;
        }
        if (strcmp(mode, "reset") == 0) {
            pks_seafoam_reset();
            AmberScript_WriteState();
            return 1;
        }
        if (strcmp(mode, "b3_current") == 0) {
            SetEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE);
            SetEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE);
            ClearEvent(EVENT_SEAFOAM4_BOULDER1_DOWN_HOLE);
            ClearEvent(EVENT_SEAFOAM4_BOULDER2_DOWN_HOLE);
            AmberScript_ReloadAfterStateLoad();
            Map_BuildScrollView();
            NPC_BuildView(gScrollPxX, gScrollPxY);
            printf("[amberscript] seafoam_state b3_current: set flags S3=1,1 S4=0,0 (position unchanged)\n");
            AmberScript_WriteState();
            return 1;
        }
        if (strcmp(mode, "b4_current") == 0) {
            SetEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE);
            SetEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE);
            SetEvent(EVENT_SEAFOAM4_BOULDER1_DOWN_HOLE);
            SetEvent(EVENT_SEAFOAM4_BOULDER2_DOWN_HOLE);
            AmberScript_ReloadAfterStateLoad();
            Map_BuildScrollView();
            NPC_BuildView(gScrollPxX, gScrollPxY);
            printf("[amberscript] seafoam_state b4_current: set flags S3=1,1 S4=1,1 (position unchanged)\n");
            AmberScript_WriteState();
            return 1;
        }
        printf("[amberscript] seafoam_state: unknown mode '%s'\n", mode);
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "unstuck") == 0) {
        int moved = 0;
        if (wCurMap < PKS_VIRTUAL_MAP_FIRST) {
            const map_events_t *ev = &gMapEvents[wCurMap];
            if (ev->warps && ev->num_warps > 0) {
                int best_i = -1;
                int best_d = 0x7fffffff;
                for (int i = 0; i < ev->num_warps; i++) {
                    int wx = (int)ev->warps[i].x;
                    int wy = (int)ev->warps[i].y;
                    int d = abs((int)wXCoord - wx) + abs((int)wYCoord - wy);
                    if (d < best_d) {
                        best_d = d;
                        best_i = i;
                    }
                }
                if (best_i >= 0) {
                    static const int kAdj[4][2] = {
                        { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 }
                    };
                    int wx = (int)ev->warps[best_i].x;
                    int wy = (int)ev->warps[best_i].y;
                    for (int a = 0; a < 4; a++) {
                        int nx = wx + kAdj[a][0];
                        int ny = wy + kAdj[a][1];
                        if (nx < 0 || ny < 0 ||
                            nx >= (int)wCurMapWidth * 2 ||
                            ny >= (int)wCurMapHeight * 2) continue;
                        if (!Tile_IsPassable(Map_GetGameTile(nx, ny))) continue;
                        if (NPC_IsBlocked(nx, ny)) continue;
                        wXCoord = (uint8_t)nx;
                        wYCoord = (uint8_t)ny;
                        wWalkBikeSurfState = 0;
                        gNoClip = 0;
                        AmberScript_ReloadAfterStateLoad();
                        Map_BuildScrollView();
                        NPC_BuildView(gScrollPxX, gScrollPxY);
                        printf("[amberscript] unstuck: moved near nearest warp (%d,%d) -> (%d,%d)\n",
                               wx, wy, nx, ny);
                        moved = 1;
                        break;
                    }
                }
            }
        }
        if (!moved) {
            wWalkBikeSurfState = 0;
            gNoClip = 0;
            Game_WarpToRealMap(0x28, 6, 8);
            printf("[amberscript] unstuck: fallback to OAKS_LAB @ (6,8)\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "sprint_holdb") == 0) {
        char sub[16] = {0};
        if (!AmberScript_ParseArg(cmd, 1, sub, sizeof(sub))) strcpy(sub, "status");
        if (strcmp(sub, "on") == 0 || strcmp(sub, "1") == 0) {
            Player_SetHoldBSprintEnabled(1);
            printf("[amberscript] sprint_holdb: ON (hold X/B to move 2x)\n");
        } else if (strcmp(sub, "off") == 0 || strcmp(sub, "0") == 0) {
            Player_SetHoldBSprintEnabled(0);
            printf("[amberscript] sprint_holdb: OFF\n");
        } else if (strcmp(sub, "status") == 0) {
            printf("[amberscript] sprint_holdb: %s\n",
                   Player_GetHoldBSprintEnabled() ? "ON" : "OFF");
        } else {
            printf("[amberscript] sprint_holdb usage: sprint_holdb on|off|status\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "npc_walkoff") == 0) {
        pks_start_npc_walkoff(1);
        AmberScript_WriteState();
        return 1;
    }

    return 0;
}
