
#include "amberscript_core.h"
#include "amberscript_movement.h"
#include "amberscript_tilemod.h"
#include "amberscript_battle_debug.h"
#include "amberscript_saveops.h"
#include "amberscript_zone_resets.h"
#include "amberscript_scene.h"
#include "amberscript_story.h"
#include "amberscript_party_items.h"
#include "amberscript_misc.h"
#include "amberscript_checkpoint.h"
#include "amberscript_mapbank.h"

#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "warp.h"
#include "text.h"
#include "trainer_sight.h"
#include "pokecenter.h"
#include "pokemon.h"
#include "battle/battle_ui.h"
#include "../data/base_stats.h"
#include "../data/map_data.h"
#include "../data/event_data.h"
#include "../data/moves_data.h"
#include "../platform/hardware.h"
#include "constants.h"

#include <stdio.h>
#include <string.h>
#include "gen2_species.h"

#define PKS_STATE_FILE "bugs/cli_state.txt"

extern uint8_t gCliButtons;
extern int     gCliFrames;

static int s_amberscript_enabled = 1;

void AmberScript_SetEnabled(int enabled) { s_amberscript_enabled = enabled ? 1 : 0; }
int  AmberScript_IsEnabled(void)         { return s_amberscript_enabled; }

#define PKS_SEQ_MAX 512
static uint8_t s_seq[PKS_SEQ_MAX];
static int     s_seq_len = 0;
static int     s_seq_pos = 0;

void AmberScript_SeqClear(void) { s_seq_len = 0; s_seq_pos = 0; }

void AmberScript_SeqPush(uint8_t btn, int press_frames, int gap_frames) {
    for (int i = 0; i < press_frames && s_seq_len < PKS_SEQ_MAX; i++)
        s_seq[s_seq_len++] = btn;
    for (int i = 0; i < gap_frames && s_seq_len < PKS_SEQ_MAX; i++)
        s_seq[s_seq_len++] = 0;
}

int AmberScript_SeqPending(void) { return s_seq_pos < s_seq_len; }

void AmberScript_SeqBattleMenu(int pos) {
    if (pos & 2) AmberScript_SeqPush(PKS_BTN_DOWN,  1, 8);
    if (pos & 1) AmberScript_SeqPush(PKS_BTN_RIGHT, 1, 8);
    AmberScript_SeqPush(PKS_BTN_A, 1, 8);
}

void AmberScript_SeqMoveSelect(int n) {
    for (int i = 1; i < n; i++)
        AmberScript_SeqPush(PKS_BTN_DOWN, 1, 8);
    AmberScript_SeqPush(PKS_BTN_A, 1, 8);
}

static int s_pending_write  = 0;
static int s_wait_remaining = 0;

void AmberScript_RequestDeferredWrite(int wait_frames) {
    s_pending_write  = 1;
    s_wait_remaining = wait_frames;
}

int AmberScript_ParseArg(const char *src, int arg_index, char *out, size_t out_sz) {
    int idx = 0;
    const char *p = src;
    if (!src || !out || out_sz == 0) return 0;
    out[0] = '\0';
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (idx == arg_index) {
            size_t n = 0;
            char quote = 0;
            if (*p == '"' || *p == '\'') {
                quote = *p++;
                while (*p && *p != quote && n + 1 < out_sz) out[n++] = *p++;
                if (*p == quote) p++;
            } else {
                while (*p && *p != ' ' && *p != '\t' && n + 1 < out_sz) out[n++] = *p++;
            }
            out[n] = '\0';

            return 1;
        }
        if (*p == '"' || *p == '\'') {
            char q = *p++;
            while (*p && *p != q) p++;
            if (*p == q) p++;
        } else {
            while (*p && *p != ' ' && *p != '\t') p++;
        }
        idx++;
    }
    return 0;
}

static int get_scene(void) {
    extern int Game_GetScene(void);
    return Game_GetScene();
}

static const char *facing_name(uint8_t dir) {
    switch (dir) {
        case 0:  return "DOWN";
        case 4:  return "UP";
        case 8:  return "LEFT";
        case 12: return "RIGHT";
        default: return "?";
    }
}

static const char *status_str(uint8_t st) {
    if (st & 0x07) return "SLP";
    if (st & 0x40) return "PSN";
    if (st & 0x10) return "BRN";
    if (st & 0x20) return "FRZ";
    if (st & 0x08) return "PAR";
    return "OK";
}

static const char *bui_state_name(int s) {

    switch (s) {
        case  0: return "INACTIVE";
        case  1: return "SLIDE_IN";
        case  2: return "APPEARED";
        case  3: return "SEND_OUT";
        case  4: return "ENEMY_SLIDE_OUT";
        case  5: return "TRAINER_SLIDE_OUT";
        case  6: return "ENEMY_SEND_OUT";
        case  7: return "POKEMON_APPEAR";
        case  8: return "INTRO";
        case  9: return "DRAW_HUD";
        case 10: return "MENU";
        case 11: return "MOVE_SELECT";
        case 12: return "MOVE_ANIM";
        case 13: return "HP_ANIM";
        case 14: return "EXEC_MOVE_B";
        case 15: return "EXEC_SECOND";
        case 16: return "TURN_END";
        case 17: return "TURN_FINISH";
        case 18: return "EXP_DRAIN";
        case 19: return "LEVELUP_STATS";
        case 20: return "ENEMY_FAINT_ANIM";
        case 21: return "PLAYER_FAINTED";
        case 22: return "USE_NEXT_MON";
        case 23: return "PARTY_SELECT";
        case 24: return "SWITCH_SELECT";
        case 25: return "RETREAT_ANIM";
        case 26: return "SWITCH_ENEMY_TURN";
        case 27: return "BAG_BATTLE";
        case 28: return "BALL_THROW";
        case 29: return "BALL_POOF";
        case 30: return "BALL_SHAKE";
        case 31: return "CAUGHT";
        case 32: return "END";
        default: return "?";
    }
}

static int count_bits8(uint8_t value) {
#ifdef _MSC_VER
    unsigned int bits = value;
    bits = bits - ((bits >> 1) & 0x55u);
    bits = (bits & 0x33u) + ((bits >> 2) & 0x33u);
    return (int)((bits + (bits >> 4)) & 0x0Fu);
#else
    return __builtin_popcount((unsigned int)value);
#endif
}

static void pks_write_battle_state(FILE *fp) {
    int bui = BattleUI_GetState();
    const char *btype = (wIsInBattle == 2) ? "TRAINER" : "WILD";

    fprintf(fp, "=== BATTLE (%s) ===\n", btype);
    fprintf(fp, "UI State: %s\n\n", bui_state_name(bui));

    AmberScript_BattleDebug_WriteStateExtra(fp);

    fprintf(fp, "ENEMY:  %s Lv%d  HP: %d/%d  [%s]\n",
            Pokemon_GetName(Species_Dex(wEnemyMon.species)),
            wEnemyMon.level,
            wEnemyMon.hp, wEnemyMon.max_hp,
            status_str(wEnemyMon.status));

    fprintf(fp, "PLAYER: %s Lv%d  HP: %d/%d  [%s]\n\n",
            Pokemon_GetName(Species_Dex(wBattleMon.species)),
            wBattleMon.level,
            wBattleMon.hp, wBattleMon.max_hp,
            status_str(wBattleMon.status));

    fprintf(fp, "Moves:\n");
    for (int i = 0; i < 4; i++) {
        uint8_t mid = wBattleMon.moves[i];
        if (!mid) { fprintf(fp, "  [%d] ---\n", i + 1); continue; }
        uint8_t pp = wBattleMon.pp[i] & 0x3F;
        const char *mname = (mid < NUM_MOVE_DEFS && gMoveNames[mid]) ? gMoveNames[mid] : "???";
        fprintf(fp, "  [%d] %-12s  %d pp\n", i + 1, mname, pp);
    }

    fprintf(fp, "\n");
    if (bui == 10 ) {
        fprintf(fp, ">> Waiting for action: fight <1-4> | run | pkmn | bag\n");
    } else if (bui == 11 ) {
        fprintf(fp, ">> Waiting for move: fight <1-4> | b (back)\n");
    } else if (bui == 22 ) {
        fprintf(fp, ">> \"Use next Monster?\"  a (yes) | b (no)\n");
    } else if (bui == 23 || bui == 24 ) {
        fprintf(fp, ">> Choose next Monster from party menu\n");
    } else {
        fprintf(fp, ">> Animation in progress — wait or press a/b to advance text\n");
    }
}

static char pks_tile_char(int mx, int my, int px, int py, int nc,
                           const map_events_t *ev) {
    if (mx == px * 2 && my == py * 2 + 1) return '@';
    for (int i = 0; i < nc; i++) {
        int ntx, nty;
        NPC_GetTilePos(i, &ntx, &nty);
        if (ntx * 2 == mx && nty * 2 + 1 == my) return 'N';
    }
    if (ev) {
        for (int i = 0; i < ev->num_signs; i++)
            if ((int)ev->signs[i].x * 2 == mx && (int)ev->signs[i].y * 2 + 1 == my) return 'S';
        for (int i = 0; i < ev->num_items; i++)
            if ((int)ev->items[i].x * 2 == mx && (int)ev->items[i].y * 2 + 1 == my) return 'I';
        for (int i = 0; i < ev->num_hidden_events; i++)
            if ((int)ev->hidden_events[i].x == mx && (int)ev->hidden_events[i].y == my) return 'H';
    }
    uint8_t tid = Map_GetTile(mx, my);
    if (Warp_IsDoorTile(tid))                    return '+';
    if (tid == wGrassTile && wGrassTile != 0xFF) return '"';
    int ld = Player_GetLedgeDir(tid);
    if (ld ==  0) return 'v';
    if (ld ==  4) return '^';
    if (ld ==  8) return '<';
    if (ld == 12) return '>';
    if (!Tile_IsPassable(tid))                   return '#';
    return '.';
}

static void pks_write_overworld_state(FILE *fp) {
    const char *mname = (wCurMap < PKS_VIRTUAL_MAP_FIRST) ? gMapTable[wCurMap].name : "???";
    fprintf(fp, "=== OVERWORLD ===\n");
    fprintf(fp, "Map: %d (%s)  Player: (%d, %d)  Facing: %s\n\n",
            wCurMap, mname, wXCoord, wYCoord, facing_name(wPlayerDirection));

    static const char *legend[] = {
        "@  = Player",
        "#  = Wall/Solid",
        ".  = Open",
        "\"  = Grass",
        "+  = Warp/Door",
        "N  = NPC",
        "^v<> = Ledge",
        "S  = Sign",
        "I  = Item",
        "H  = Hidden event",
    };
    static const int LEGEND_COUNT = (int)(sizeof(legend) / sizeof(legend[0]));

    int nc = NPC_GetCount();
    int px = (int)wXCoord, py = (int)wYCoord;
    const map_events_t *ev = (wCurMap < PKS_VIRTUAL_MAP_FIRST) ? &gMapEvents[wCurMap] : NULL;

    fprintf(fp, "+");
    for (int x = 0; x < SCREEN_WIDTH; x++) fprintf(fp, "-");
    fprintf(fp, "+  Legend:\n");
    for (int ty = 0; ty < SCREEN_HEIGHT; ty++) {
        fprintf(fp, "|");
        for (int tx = 0; tx < SCREEN_WIDTH; tx++)
            fprintf(fp, "%c", pks_tile_char(gCamX + tx, gCamY + ty, px, py, nc, ev));
        fprintf(fp, "|");
        if (ty < LEGEND_COUNT) fprintf(fp, "  %s", legend[ty]);
        fprintf(fp, "\n");
    }
    fprintf(fp, "+");
    for (int x = 0; x < SCREEN_WIDTH; x++) fprintf(fp, "-");
    fprintf(fp, "+\n");
}

void AmberScript_WriteState(void) {
    FILE *fp = fopen(PKS_STATE_FILE, "w");
    if (!fp) return;

    if (Text_IsOpen()) {
        char tbuf[256];
        fprintf(fp, "=== TEXT ===\n");
        if (Text_GetCurrentStr(tbuf, sizeof(tbuf)))
            fprintf(fp, "%s\n", tbuf);
        else
            fprintf(fp, "<dialog open>\n");
        fprintf(fp, "\n>> press a to continue | b to dismiss\n");
        fclose(fp);
        return;
    }

    if (Pokecenter_IsWaitingYesNo()) {
        fprintf(fp, "=== POKECENTER ===\n");
        fprintf(fp, "Nurse Joy: Shall we heal your Monster?\n");
        fprintf(fp, "\n>> a (yes, heal) | b (no, cancel)\n");
        fclose(fp);
        return;
    }

    int sc = get_scene();
    if (sc == 2  || sc == 1 ) {
        pks_write_battle_state(fp);
    } else {
        pks_write_overworld_state(fp);
    }

    fprintf(fp, "\nParty (%d):\n", wPartyCount);
    for (int i = 0; i < wPartyCount && i < 6; i++) {
        const party_mon_t *m = &wPartyMons[i];
        fprintf(fp, "  [%d] %s Lv%d  HP:%d/%d  [%s]\n",
                i + 1,
                Pokemon_GetName(Species_Dex(m->base.species)),
                m->level, (int)m->base.hp, (int)m->max_hp,
                status_str(m->base.status));
    }

    unsigned money = ((unsigned)wPlayerMoney[0] >> 4) * 100000
                   + ((unsigned)wPlayerMoney[0] & 0xF) * 10000
                   + ((unsigned)wPlayerMoney[1] >> 4) * 1000
                   + ((unsigned)wPlayerMoney[1] & 0xF) * 100
                   + ((unsigned)wPlayerMoney[2] >> 4) * 10
                   + ((unsigned)wPlayerMoney[2] & 0xF);
    fprintf(fp, "\nMoney: $%u  Badges: %d  Frame: %d\n",
            money, count_bits8(wObtainedBadges),
            (int)(255 - hFrameCounter));

    if (Trainer_IsEngaging())
        fprintf(fp, "\n!! TRAINER SPOTTED YOU — engaging\n");

    fprintf(fp, "\n[amberscript: ON]\n");

    fclose(fp);
}

int AmberScript_Dispatch(const char *cmd) {
    char verb[32] = {0};
    int  n = 1;
    sscanf(cmd, "%31s %d", verb, &n);
    if (n < 1) n = 1;

    AmberScript_SeqClear();

    if (AmberScript_Movement_TryHandle(cmd, verb, n)) return 1;
    if (AmberScript_TileMod_TryHandle(cmd, verb, n)) return 1;
    if (AmberScript_BattleDebug_TryHandle(cmd, verb, n)) return 1;
    if (AmberScript_SaveOps_TryHandle(cmd, verb, n)) return 1;
    if (AmberScript_ZoneResets_TryHandle(cmd, verb, n)) return 1;
    if (AmberScript_Scene_TryHandle(cmd, verb, n)) return 1;
    if (AmberScript_Story_TryHandle(cmd, verb, n)) return 1;
    if (AmberScript_PartyItems_TryHandle(cmd, verb, n)) return 1;
    if (AmberScript_Misc_TryHandle(cmd, verb, n)) return 1;
    if (AmberScript_Checkpoint_TryHandle(cmd, verb, n)) return 1;
    if (AmberScript_MapBank_TryHandle(cmd, verb, n)) return 1;

    return 0;
}

void AmberScript_Tick(void) {

    AmberScript_Scene_Tick();
    AmberScript_TileMod_Tick();

    if (s_seq_pos < s_seq_len) {
        gCliButtons = s_seq[s_seq_pos++];
        gCliFrames  = 1;
    }

    AmberScript_SaveOps_Tick();
    AmberScript_ZoneResets_Tick();
    AmberScript_BattleDebug_Tick();

    if (s_pending_write && s_seq_pos >= s_seq_len) {
        if (s_wait_remaining > 0) {
            s_wait_remaining--;
        } else {
            AmberScript_WriteState();
            s_pending_write = 0;
        }
    }
}
