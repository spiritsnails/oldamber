
#include "debug_suite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define suite_mkdir(p) _mkdir(p)

__declspec(dllimport) int __stdcall MoveFileExA(const char *, const char *, unsigned long);
#define SUITE_MOVEFILE_REPLACE_EXISTING 0x1u
#else
#define suite_mkdir(p) mkdir((p), 0755)
#endif

static void suite_replace(const char *tmp, const char *dst) {
#ifdef _WIN32
    if (!MoveFileExA(tmp, dst, SUITE_MOVEFILE_REPLACE_EXISTING)) {
        remove(dst); rename(tmp, dst);
    }
#else
    rename(tmp, dst);
#endif
}

#include "../platform/hardware.h"
#include "../platform/save.h"
#include "../platform/display.h"
#include "constants.h"
#include "text.h"
#include "npc.h"
#include "debug_cli.h"
#include "debug_trace.h"
#include "debug_fields.h"
#include "battle/battle_ui.h"
#include "amberscript_scene.h"
#include "gym_scripts.h"
#include "route24_scripts.h"
#include "bills_house_scripts.h"
#include "amberscript_saveops.h"
#include "amberscript_mapbank.h"
#include "overworld.h"
#include "pokemon.h"
#include "../data/moves_data.h"
#include "../data/base_stats.h"
#include "../data/item_names_gen.h"
#include "../data/event_flag_names.h"

extern int Game_GetScene(void);
extern int Game_IsWarpFadeActive(void);
extern int Game_IsOverworldTickActive(void);

extern uint8_t gCliButtons;
extern int     gCliFrames;

#define SUITE_OUT_PATH   "bugs/suite_out.json"
#define SUITE_REPORTS    "bugs/reports"
#define SUITE_HASH_TRACE "bugs/trace_hash.jsonl"
#define SUITE_LOG_TAIL_LINES 200

#define RPL_MAGIC   0x31504C52u
#define RPL_VERSION 1u

typedef struct suite_rpl_header_t {
    uint32_t magic;
    uint32_t version;
    uint32_t state_size;
    uint32_t input_frames;
} suite_rpl_header_t;

static const uint8_t *s_rw_data = NULL;
static const int     *s_rw_seq  = NULL;
static const int     *s_rw_len  = NULL;
static size_t         s_state_size = 0;
static int            s_slots = 0;

static uint8_t *s_slot_input = NULL;
static uint8_t *s_slot_quiet = NULL;

static int      s_lab = 0;
static int      s_speed_pct = 100;
static int      s_quit = 0;
static int      s_paused = 0;
static int      s_step_budget = 0;
static int      s_rewind_delta = 0;
static int      s_rewind_pos = 0;
static int      s_rewind_len = 0;

static char     s_bp_restore_req[80] = {0};
static int      s_bp_commit_req = 0;
static uint64_t s_frame = 0;
static uint64_t s_runto = 0;

static char     s_pending_load[512] = {0};
static int      s_pending_load_paused = 0;
static uint8_t *s_replay_inputs = NULL;
static uint32_t s_replay_len = 0;
static uint32_t s_replay_pos = 0;
static int      s_replay_active = 0;

static uint8_t *s_scratch = NULL;
static FILE    *s_hash_fp = NULL;
static unsigned s_ack_seq = 0;
static char     s_last_report_id[64] = {0};

static uint64_t fnv1a64(const uint8_t *p, size_t n) {
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static uint64_t suite_state_hash(void) {
    if (!s_scratch) return 0;

    memset(s_scratch, 0, s_state_size);
    if (Save_StateCaptureToBuffer(s_scratch, s_state_size) != 0) return 0;
    return fnv1a64(s_scratch, s_state_size);
}

static int suite_token(const char *src, int n, char *out, size_t out_sz) {
    const char *p = src;
    out[0] = '\0';
    for (int t = 0; ; t++) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '\n' || *p == '\r') return 0;
        const char *start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
        if (t == n) {
            size_t len = (size_t)(p - start);
            if (len >= out_sz) len = out_sz - 1;
            memcpy(out, start, len);
            out[len] = '\0';
            return 1;
        }
    }
}

static int suite_rest(const char *src, int after_token, char *out, size_t out_sz) {
    const char *p = src;
    out[0] = '\0';
    for (int t = 0; t <= after_token; t++) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '\n' || *p == '\r') return 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
    }
    while (*p == ' ' || *p == '\t') p++;
    if (!*p || *p == '\n' || *p == '\r') return 0;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == '\n' || p[len-1] == '\r' ||
                       p[len-1] == ' '  || p[len-1] == '\t')) len--;
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return len > 0;
}

static void suite_json_escape(const char *in, char *out, size_t out_sz) {
    size_t o = 0;
    for (size_t i = 0; in && in[i] && o + 8 < out_sz; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\r') {  }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (c < 0x20)  {  }
        else out[o++] = (char)c;
    }
    out[o] = '\0';
}

static void suite_ack(const char *verb, int ok, const char *data_json) {
    FILE *fp = fopen(SUITE_OUT_PATH, "w");
    if (!fp) return;
    fprintf(fp,
        "{\"seq\":%u,\"verb\":\"%s\",\"ok\":%s,\"frame\":%llu,"
        "\"paused\":%d,\"replay\":{\"active\":%d,\"pos\":%u,\"len\":%u},"
        "\"data\":%s}\n",
        ++s_ack_seq, verb, ok ? "true" : "false",
        (unsigned long long)s_frame,
        s_paused, s_replay_active, s_replay_pos, s_replay_len,
        data_json ? data_json : "null");
    fclose(fp);
}

void DebugSuite_Init(const uint8_t *rw_data, const int *rw_seq,
                     const int *rw_len, size_t state_size, int slots) {
    s_rw_data = rw_data;
    s_rw_seq = rw_seq;
    s_rw_len = rw_len;
    s_state_size = state_size;
    s_slots = slots;
    s_slot_input = (uint8_t *)calloc((size_t)slots, 1);
    s_slot_quiet = (uint8_t *)calloc((size_t)slots, 1);
    s_scratch = (uint8_t *)malloc(state_size);
    suite_mkdir("bugs");
}

void DebugSuite_SetLabMode(int on) {
    s_lab = on;
    if (on) s_speed_pct = 0;
}
int DebugSuite_IsLab(void)         { return s_lab; }
int DebugSuite_IsTurbo(void)       { return s_speed_pct == 0; }
int DebugSuite_SpeedPct(void)      { return s_speed_pct; }
void DebugSuite_SetSpeedPct(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 1000) pct = 1000;
    s_speed_pct = pct;
}
int DebugSuite_QuitRequested(void) { return s_quit; }
int DebugSuite_ReplayActive(void)  { return s_replay_active; }

int  DebugSuite_TakeRewindDelta(void) { int d = s_rewind_delta; s_rewind_delta = 0; return d; }
void DebugSuite_SetRewindPos(int pos, int len) { s_rewind_pos = pos; s_rewind_len = len; }

int DebugSuite_TakeBreakpointRestore(char *buf, size_t n) {
    if (!buf || n == 0 || s_bp_restore_req[0] == '\0') return 0;
    snprintf(buf, n, "%s", s_bp_restore_req);
    s_bp_restore_req[0] = '\0';
    return 1;
}
int DebugSuite_TakeBreakpointCommit(void) { int c = s_bp_commit_req; s_bp_commit_req = 0; return c; }

static void suite_do_pending_load(void) {
    FILE *fp;
    suite_rpl_header_t hdr;
    uint8_t *state_buf = NULL;
    char path[512];

    snprintf(path, sizeof(path), "%s", s_pending_load);
    s_pending_load[0] = '\0';

    fp = fopen(path, "rb");
    if (!fp) { suite_ack("replayfile", 0, "{\"err\":\"open failed\"}"); return; }
    if (fread(&hdr, 1, sizeof(hdr), fp) != sizeof(hdr) ||
        hdr.magic != RPL_MAGIC || hdr.version != RPL_VERSION ||
        hdr.state_size == 0) {
        fclose(fp);
        suite_ack("replayfile", 0, "{\"err\":\"bad header\"}");
        return;
    }
    state_buf = (uint8_t *)malloc(hdr.state_size);
    if (!state_buf || fread(state_buf, 1, hdr.state_size, fp) != hdr.state_size) {
        free(state_buf); fclose(fp);
        suite_ack("replayfile", 0, "{\"err\":\"state read failed\"}");
        return;
    }
    free(s_replay_inputs);
    s_replay_inputs = NULL;
    s_replay_len = 0;
    s_replay_pos = 0;
    s_replay_active = 0;
    if (hdr.input_frames > 0) {
        s_replay_inputs = (uint8_t *)malloc(hdr.input_frames);
        if (!s_replay_inputs ||
            fread(s_replay_inputs, 1, hdr.input_frames, fp) != hdr.input_frames) {
            free(s_replay_inputs); s_replay_inputs = NULL;
            free(state_buf); fclose(fp);
            suite_ack("replayfile", 0, "{\"err\":\"input read failed\"}");
            return;
        }
    }
    fclose(fp);

    if (Save_StateLoadFromBuffer(state_buf, hdr.state_size) != 0) {
        free(state_buf);
        free(s_replay_inputs); s_replay_inputs = NULL;
        suite_ack("replayfile", 0, "{\"err\":\"state load rejected\"}");
        return;
    }
    free(state_buf);
    AmberScript_ReloadAfterStateLoad();

    gCliButtons = 0;
    gCliFrames = 0;

    s_replay_len = hdr.input_frames;
    s_replay_pos = 0;
    s_replay_active = (s_replay_len > 0);
    s_frame = 0;
    if (s_hash_fp) {

        fclose(s_hash_fp);
        s_hash_fp = fopen(SUITE_HASH_TRACE, "w");
    }
    Trace_RestartStreams();
    s_runto = 0;
    s_step_budget = 0;
    s_paused = s_pending_load_paused;

    {
        char data[192];
        snprintf(data, sizeof(data),
                 "{\"loaded\":true,\"input_frames\":%u,\"paused\":%d}",
                 s_replay_len, s_paused);
        suite_ack("replayfile", 1, data);
    }
    printf("[suite] replayfile loaded: %s (%u input frames)\n", path, s_replay_len);
}

void DebugSuite_TopOfFrame(void) {
    if (s_pending_load[0]) suite_do_pending_load();
}

int DebugSuite_FrameGate(void) {
    if (!s_paused) return 1;
    if (s_step_budget > 0) { s_step_budget--; return 1; }
    return 0;
}

static void DebugSuite_WriteHeartbeat(void);

void DebugSuite_PausedTick(void) {
    DebugCLI_PollExternal();

    if (!s_lab) {
        static int s_pause_hb = 0;
        if (++s_pause_hb >= 12) { s_pause_hb = 0; DebugSuite_WriteHeartbeat(); }
    }
}

void DebugSuite_InjectInput(void) {
    if (s_replay_active && s_replay_pos < s_replay_len) {
        gCliButtons = s_replay_inputs[s_replay_pos++];
        gCliFrames = 1;
    }
}

void DebugSuite_InjectLiveHold(void) {
    struct stat stt;
    FILE *fp;
    unsigned v = 0;
    if (s_lab || s_replay_active) return;

    {
        static unsigned idle_tick;
        static int      driving;
        if (!driving && (idle_tick++ % 15) != 0) return;
        if (stat("bugs/hold.txt", &stt) == 0 && time(NULL) - stt.st_mtime <= 2)
            driving = 1;
        else
            driving = 0;
        if (!driving) return;
    }

    if (stat("bugs/hold.txt", &stt) != 0) return;
    if (time(NULL) - stt.st_mtime > 2) return;
    fp = fopen("bugs/hold.txt", "r");
    if (!fp) return;
    if (fscanf(fp, "%x", &v) != 1) v = 0;
    fclose(fp);
    if (v) {
        gCliButtons = (uint8_t)v;
        gCliFrames = 1;
    }
}

static void suite_trace_actors(void) {
    extern int gScrollPxX, gScrollPxY;
    int count;
    if (Game_GetScene() != 0) return;
    Trace_Emit(TRACE_ACTOR,
        "\"a\":\"p\",\"x\":%d,\"y\":%d,\"dir\":%d,\"wc\":%d,\"spx\":%d,\"spy\":%d,\"g\":%d",
        (int)wXCoord, (int)wYCoord, (int)wPlayerDirection, (int)wWalkCounter,
        gScrollPxX, gScrollPxY, Game_IsOverworldTickActive());
    count = NPC_GetCount();
    for (int i = 0; i < count; i++) {
        int tx = 0, ty = 0, px = 0, py = 0;
        NPC_GetTilePos(i, &tx, &ty);
        NPC_GetScreenPos(i, &px, &py);
        Trace_Emit(TRACE_ACTOR,
            "\"a\":\"n%d\",\"spr\":%d,\"x\":%d,\"y\":%d,\"px\":%d,\"py\":%d,"
            "\"dir\":%d,\"w\":%d",
            i, NPC_GetSpriteId(i), tx, ty, px, py,
            NPC_GetFacing(i), NPC_IsWalking(i));
    }
}

static void suite_trace_ui(void) {
    static int p_bui = -1, p_text = -1, p_wy = -1, p_scene = -1;
    static uint8_t p_held = 0;
    int bui = BattleUI_GetState();
    int text = Text_IsOpen();
    int wy = (int)hWY;
    int scene = Game_GetScene();
    if (bui == p_bui && text == p_text && wy == p_wy &&
        scene == p_scene && hJoyHeld == p_held)
        return;
    Trace_Emit(TRACE_UI,
        "\"bui\":%d,\"text\":%d,\"wy\":%d,\"scene\":%d,\"held\":%d,\"pressed\":%d,"
        "\"ji\":%d,\"battle\":%d",
        bui, text, wy, scene, (int)hJoyHeld, (int)hJoyPressed,
        (int)wJoyIgnore, (int)wIsInBattle);
    p_bui = bui; p_text = text; p_wy = wy; p_scene = scene; p_held = hJoyHeld;
}

enum { SENT_RNG = 0, SENT_NPC_OVERLAP, SENT_TEXT_BLANK, SENT_COUNT };
#define SENT_COOLDOWN_FRAMES (30u * 60u)
static uint64_t s_sent_last_fire[SENT_COUNT];

static void suite_sentinel_fire(int which, const char *what) {
    if (s_sent_last_fire[which] &&
        s_frame - s_sent_last_fire[which] < SENT_COOLDOWN_FRAMES)
        return;
    s_sent_last_fire[which] = s_frame;
    printf("[suite] SENTINEL: %s\n", what);
    fflush(stdout);
    DebugCLI_HistoryPushExternal(what);
    if (!s_lab && !s_replay_active) {
        char msg[160];
        snprintf(msg, sizeof(msg), "[sentinel] %s", what);
        DebugSuite_CaptureReport(msg);
    }
}

static void suite_sentinels(void) {

    if ((s_frame & 63u) == 0) {
        static uint8_t samples[8];
        static int n = 0;
        samples[n % 8] = (uint8_t)(hRandomAdd + hRandomSub);
        n++;
        if (n >= 8) {
            int pinned = 1;
            for (int i = 1; i < 8; i++)
                if (samples[i] != samples[0]) { pinned = 0; break; }
            if (pinned) {
                Trace_Emit(TRACE_FLAG, "\"err\":\"rng_pinned_sum\",\"sum\":%d",
                           samples[0]);
                suite_sentinel_fire(SENT_RNG,
                    "sentinel: RNG add+sub pinned (degenerate state)");
            }
        }
    }

    if ((s_frame & 63u) == 1 && Game_GetScene() == 0) {
        int count = NPC_GetCount();
        for (int i = 1; i < count; i++) {
            int xi, yi;
            if (NPC_IsHidden(i) || NPC_IsWalking(i)) continue;
            NPC_GetTilePos(i, &xi, &yi);
            for (int j = 0; j < i; j++) {
                int xj, yj;
                if (NPC_IsHidden(j) || NPC_IsWalking(j)) continue;
                NPC_GetTilePos(j, &xj, &yj);
                if (xi == xj && yi == yj) {
                    char what[96];
                    Trace_Emit(TRACE_NPC,
                        "\"err\":\"npc_overlap\",\"i\":%d,\"j\":%d,\"x\":%d,\"y\":%d",
                        i, j, xi, yi);
                    snprintf(what, sizeof(what),
                        "sentinel: NPCs %d+%d share tile (%d,%d)", j, i, xi, yi);
                    suite_sentinel_fire(SENT_NPC_OVERLAP, what);
                    return;
                }
            }
        }
    }

    if (wIsInBattle) {
        static int prev2 = -1, prev1 = -1;
        int populated = 0;
        for (int r = 12; r < 18; r++)
            for (int c = 0; c < SCREEN_WIDTH; c++)
                if (wTileMap[r * SCREEN_WIDTH + c] != 0x7F) populated++;
        if (prev2 >= 30 && prev1 <= 4 && populated >= 30) {
            Trace_Emit(TRACE_UI, "\"err\":\"textbox_blank_1f\",\"bui\":%d",
                       BattleUI_GetState());
            suite_sentinel_fire(SENT_TEXT_BLANK,
                "sentinel: 1-frame battle textbox blank");
        }
        prev2 = prev1;
        prev1 = populated;
    }
}

static void suite_decode_name(const uint8_t *in, char *out, size_t out_sz) {
    size_t o = 0;
    for (int i = 0; i < NAME_LENGTH && o + 1 < out_sz; i++) {
        uint8_t b = in[i];
        char c = 0;
        if (b == 0x00 || b == 0x50) break;
        if (b >= 0x80 && b <= 0x99)      c = (char)('A' + (b - 0x80));
        else if (b >= 0xA0 && b <= 0xB9) c = (char)('a' + (b - 0xA0));
        else if (b >= 0xF6 && b <= 0xFF) c = (char)('0' + (b - 0xF6));
        else if (b == 0x7F)              c = ' ';
        else                             c = '?';
        out[o++] = c;
    }
    out[o] = '\0';
}

static const char *suite_status_str(uint8_t st) {
    if (st == 0) return "OK";
    if (st & 0x07) return "SLP";
    if (st & 0x08) return "PSN";
    if (st & 0x10) return "BRN";
    if (st & 0x20) return "FRZ";
    if (st & 0x40) return "PAR";
    return "OK";
}

static const char *suite_item_name(uint8_t id) {
    for (size_t i = 0; i < NUM_ITEM_NAMES; i++)
        if (kItemNames[i].id == id) return kItemNames[i].name;
    return "?";
}

static void DebugSuite_WriteHeartbeat(void) {
    FILE *fp = fopen("bugs/live.json.tmp", "w");
    const char *map_name;
    uint32_t money;
    char nm[24], nick[24];
    if (!fp) return;

    map_name = AmberScript_MapBank_NameForRealId((int)wCurMap);
    money = (uint32_t)(((wPlayerMoney[0] >> 4) * 10 + (wPlayerMoney[0] & 0xF)) * 10000u
                     + ((wPlayerMoney[1] >> 4) * 10 + (wPlayerMoney[1] & 0xF)) * 100u
                     + ((wPlayerMoney[2] >> 4) * 10 + (wPlayerMoney[2] & 0xF)));

    fprintf(fp, "{\"frame\":%llu,\"map\":%d,\"map_name\":\"%s\","
            "\"x\":%d,\"y\":%d,\"facing\":%d,\"scene\":%d,\"in_battle\":%d,"
            "\"money\":%u,\"badges\":%d,\"paused\":%d,\"speed_pct\":%d,"
            "\"rewind_pos\":%d,\"rewind_len\":%d,\"party\":[",
            (unsigned long long)s_frame, (int)wCurMap,
            map_name ? map_name : "", (int)wXCoord, (int)wYCoord,
            (int)wPlayerDirection, Game_GetScene(), (int)wIsInBattle,
            money, (int)wObtainedBadges, s_paused, s_speed_pct,
            s_rewind_pos, s_rewind_len);

    for (int i = 0; i < (int)wPartyCount && i < 6; i++) {
        party_mon_t *m = &wPartyMons[i];
        uint8_t species = m->base.species;
        int dex = gSpeciesToDex[species];
        snprintf(nm, sizeof(nm), "%s", Pokemon_GetNameBySpecies(species));
        suite_decode_name(wPartyMonNicks[i], nick, sizeof(nick));
        fprintf(fp, "%s{\"slot\":%d,\"species\":%d,\"dex\":%d,\"name\":\"%s\","
                "\"nick\":\"%s\",\"level\":%d,\"hp\":%d,\"max_hp\":%d,"
                "\"status\":\"%s\",\"moves\":[",
                i ? "," : "", i + 1, species, dex, nm, nick,
                (int)m->level, (int)m->base.hp, (int)m->max_hp,
                suite_status_str(m->base.status));
        for (int mv = 0; mv < 4; mv++) {
            uint8_t mid = m->base.moves[mv];
            if (mid == 0) continue;
            fprintf(fp, "%s{\"id\":%d,\"name\":\"%s\",\"pp\":%d}",
                    mv && m->base.moves[mv - 1] ? "," : "",
                    mid, (mid < NUM_MOVE_DEFS) ? gMoveNames[mid] : "?",
                    (int)(m->base.pp[mv] & 0x3F));
        }
        fprintf(fp, "]}");
    }
    fprintf(fp, "],\"bag\":[");
    for (int i = 0; i < (int)wNumBagItems && i < BAG_ITEM_CAPACITY; i++) {
        uint8_t id = wBagItems[i * 2];
        uint8_t qty = wBagItems[i * 2 + 1];
        fprintf(fp, "%s{\"id\":%d,\"name\":\"%s\",\"qty\":%d}",
                i ? "," : "", id, suite_item_name(id), qty);
    }
    fprintf(fp, "],\"battle\":");
    if (wIsInBattle) {
        fprintf(fp,
            "{\"type\":%d,"
            "\"enemy\":{\"species\":%d,\"name\":\"%s\",\"level\":%d,"
            "\"hp\":%d,\"max_hp\":%d,\"status\":\"%s\"},"
            "\"player\":{\"slot\":%d,\"species\":%d,\"name\":\"%s\",\"level\":%d,"
            "\"hp\":%d,\"max_hp\":%d,\"status\":\"%s\"}}",
            (int)wBattleType,
            (int)wEnemyMon.species, Pokemon_GetNameBySpecies(wEnemyMon.species),
            (int)wEnemyMon.level, (int)wEnemyMon.hp, (int)wEnemyMon.max_hp,
            suite_status_str(wEnemyMon.status),
            (int)wPlayerMonNumber + 1, (int)wBattleMon.species,
            Pokemon_GetNameBySpecies(wBattleMon.species), (int)wBattleMon.level,
            (int)wBattleMon.hp, (int)wBattleMon.max_hp,
            suite_status_str(wBattleMon.status));
    } else {
        fprintf(fp, "null");
    }
    fprintf(fp, "}\n");
    fclose(fp);

    suite_replace("bugs/live.json.tmp", "bugs/live.json");
}

static void DebugSuite_WriteMapGrid(void) {
    FILE *fp;
    const char *name;
    int w = (int)wCurMapWidth * 2;
    int h = (int)wCurMapHeight * 2;
    int first;
    if (w <= 0 || h <= 0 || w > 256 || h > 256) return;
    fp = fopen("bugs/mapgrid.json.tmp", "w");
    if (!fp) return;
    name = AmberScript_MapBank_NameForRealId((int)wCurMap);

    fprintf(fp, "{\"name\":\"%s\",\"map\":%d,\"w\":%d,\"h\":%d,"
            "\"player\":{\"x\":%d,\"y\":%d},\"pass\":\"",
            name ? name : "", (int)wCurMap, w, h,
            (int)wXCoord, (int)wYCoord);
    for (int gy = 0; gy < h; gy++)
        for (int gx = 0; gx < w; gx++)
            fputc(Map_IsTilePassableAt(gx, gy) ? '1' : '0', fp);
    fprintf(fp, "\",\"npcs\":[");
    first = 1;
    for (int i = 0, n = NPC_GetCount(); i < n; i++) {
        int tx = 0, ty = 0;
        if (NPC_IsHidden(i)) continue;
        NPC_GetTilePos(i, &tx, &ty);
        fprintf(fp, "%s{\"x\":%d,\"y\":%d,\"spr\":%d,\"i\":%d}",
                first ? "" : ",", tx, ty, NPC_GetSpriteId(i), i);
        first = 0;
    }
    fprintf(fp, "],\"warps\":[");
    first = 1;
    for (int i = 0; i < 16; i++) {
        int wx = 0, wy = 0;
        if (!AmberScript_MapBank_GetWarpSpotForRealId((int)wCurMap, i, &wx, &wy))
            continue;
        fprintf(fp, "%s{\"x\":%d,\"y\":%d,\"i\":%d}",
                first ? "" : ",", wx, wy, i);
        first = 0;
    }
    fprintf(fp, "]}\n");
    fclose(fp);
    suite_replace("bugs/mapgrid.json.tmp", "bugs/mapgrid.json");
}

static void DebugSuite_WriteTrail(void) {
    FILE *fp = fopen("bugs/trail.json.tmp", "w");
    int first = 1;
    if (!fp) return;
    fprintf(fp, "{\"f\":%llu,\"p\":[%d,%d],\"n\":[",
            (unsigned long long)s_frame, (int)wXCoord, (int)wYCoord);
    for (int i = 0, n = NPC_GetCount(); i < n; i++) {
        int tx = 0, ty = 0;
        if (NPC_IsHidden(i)) continue;
        NPC_GetTilePos(i, &tx, &ty);
        fprintf(fp, "%s[%d,%d,%d]", first ? "" : ",", i, tx, ty);
        first = 0;
    }
    fprintf(fp, "]}\n");
    fclose(fp);
    suite_replace("bugs/trail.json.tmp", "bugs/trail.json");
}

enum { WP_FLAG_SET = 1, WP_FLAG_CLEAR, WP_TILE, WP_MAP_ENTER, WP_HP_BELOW, WP_BATTLE };
typedef struct { int used, type, a, b, c, prev; char desc[56]; } watchpoint_t;
#define WP_MAX 16
static watchpoint_t s_wp[WP_MAX];

static int wp_condition(const watchpoint_t *w) {
    switch (w->type) {
        case WP_FLAG_SET:   return CheckEvent((uint16_t)w->a) ? 1 : 0;
        case WP_FLAG_CLEAR: return CheckEvent((uint16_t)w->a) ? 0 : 1;
        case WP_TILE:       return (wCurMap == w->a && wXCoord == w->b && wYCoord == w->c);
        case WP_MAP_ENTER:  return (wCurMap == w->a);
        case WP_HP_BELOW:
            for (int i = 0; i < (int)wPartyCount; i++)
                if (wPartyMons[i].base.hp > 0 && (int)wPartyMons[i].base.hp <= w->a) return 1;
            return 0;
        case WP_BATTLE:     return (wIsInBattle != 0);
    }
    return 0;
}

static void wp_write_json(void) {
    FILE *fp = fopen("bugs/watch.json", "w");
    int first = 1;
    if (!fp) return;
    fprintf(fp, "[");
    for (int i = 0; i < WP_MAX; i++) {
        if (!s_wp[i].used) continue;
        fprintf(fp, "%s{\"i\":%d,\"desc\":\"%s\"}", first ? "" : ",", i, s_wp[i].desc);
        first = 0;
    }
    fprintf(fp, "]\n");
    fclose(fp);
}

static void suite_eval_watchpoints(void) {
    for (int i = 0; i < WP_MAX; i++) {
        watchpoint_t *w = &s_wp[i];
        int cur;
        if (!w->used) continue;
        cur = wp_condition(w);
        if (cur && !w->prev) {
            char msg[80];
            snprintf(msg, sizeof(msg), "[watch] %s", w->desc);
            printf("[suite] WATCHPOINT tripped: %s -- freezing + capturing\n", w->desc);
            fflush(stdout);
            DebugCLI_HistoryPushExternal(msg);
            s_paused = 1;
            DebugSuite_CaptureReport(msg);
        }
        w->prev = cur;
    }
}

void DebugSuite_EndFrame(void) {
    s_frame++;
    Trace_SetFrame(s_frame);
    suite_eval_watchpoints();
    suite_trace_actors();
    suite_trace_ui();
    suite_sentinels();
    Trace_FlushStreams();

    if (!s_lab && (s_frame % 20u) == 0)
        DebugSuite_WriteHeartbeat();

    if (!s_lab && ((s_frame % 2u) == 0 || s_paused)) {
        if (Display_SaveScreenshot("bugs/screen_tmp.bmp") == 0)
            suite_replace("bugs/screen_tmp.bmp", "bugs/screen.bmp");
    }

    if (!s_lab) {
        static int s_last_grid_map = -1;
        if ((int)wCurMap != s_last_grid_map) {
            s_last_grid_map = (int)wCurMap;
            DebugSuite_WriteMapGrid();
        }
    }

    if (!s_lab && (s_frame % 4u) == 0 && Game_GetScene() == 0)
        DebugSuite_WriteTrail();

    if (s_hash_fp) {
        fprintf(s_hash_fp, "{\"f\":%llu,\"h\":\"%016llx\"}\n",
                (unsigned long long)s_frame,
                (unsigned long long)suite_state_hash());
        fflush(s_hash_fp);
    }

    if (s_replay_active && s_replay_pos >= s_replay_len) {
        s_replay_active = 0;
        s_paused = 1;
        suite_ack("replay_done", 1, NULL);
        printf("[suite] replay complete at frame %llu — paused\n",
               (unsigned long long)s_frame);
    }

    if (s_runto > 0 && s_frame >= s_runto) {
        s_runto = 0;
        s_paused = 1;
        suite_ack("runto_done", 1, NULL);
    }
}

void DebugSuite_RecordFrame(uint8_t joy_input, int slot) {
    int quiet;
    if (!s_slot_input || slot < 0 || slot >= s_slots) return;
    quiet = (Game_GetScene() == 0)
        && !Text_IsOpen()
        && !Game_IsWarpFadeActive()
        && !DebugCLI_SceneIsActive()
        && !AmberScript_Scene_IsActive()
        && wJoyIgnore == 0
        && wIsInBattle == 0;
    s_slot_input[slot] = joy_input;
    s_slot_quiet[slot] = (uint8_t)quiet;
}

static int suite_write_rpl(const char *path, const uint8_t *state,
                           const uint8_t *inputs, uint32_t n_inputs) {
    suite_rpl_header_t hdr;
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    hdr.magic = RPL_MAGIC;
    hdr.version = RPL_VERSION;
    hdr.state_size = (uint32_t)s_state_size;
    hdr.input_frames = n_inputs;
    if (fwrite(&hdr, 1, sizeof(hdr), fp) != sizeof(hdr) ||
        fwrite(state, 1, s_state_size, fp) != s_state_size ||
        (n_inputs > 0 && fwrite(inputs, 1, n_inputs, fp) != n_inputs)) {
        fclose(fp);
        remove(path);
        return -1;
    }
    fclose(fp);
    return 0;
}

static void suite_write_log_tail(const char *path) {

    long sz, want;
    static char buf[64 * 1024];
    size_t got;
    FILE *in, *out;

    fflush(stdout);
    in = fopen("pokered_log.txt", "rb");
    if (!in) return;
    fseek(in, 0, SEEK_END);
    sz = ftell(in);
    want = sizeof(buf) - 1;
    if (sz < want) want = sz;
    fseek(in, sz - want, SEEK_SET);
    got = fread(buf, 1, (size_t)want, in);
    fclose(in);
    buf[got] = '\0';

    {
        int lines = 0;
        char *p = buf + got;
        while (p > buf) {
            if (*(p - 1) == '\n' && ++lines > SUITE_LOG_TAIL_LINES) break;
            p--;
        }
        out = fopen(path, "wb");
        if (!out) return;
        fwrite(p, 1, (size_t)(buf + got - p), out);
        fclose(out);
    }
}

int DebugSuite_CaptureReport(const char *message) {
    int len, base_i = -1, base_quiescent, i;
    uint32_t n_inputs;
    char id[64], dir[256], path[512], toast[96];
    static uint8_t inputs[8192];
    const uint8_t *base_state;
    time_t now;
    struct tm *tmv;

    if (!s_rw_data || !s_rw_seq || !s_rw_len) return -1;

    {
        static uint64_t s_last_capture_frame = 0;
        if (s_last_capture_frame != 0 &&
            s_frame - s_last_capture_frame < 120) {
            printf("[suite] capture: ignored (cooldown, last at frame %llu)\n",
                   (unsigned long long)s_last_capture_frame);
            return -1;
        }
        s_last_capture_frame = s_frame;
    }

    len = *s_rw_len;
    if (len < 2) {
        printf("[suite] capture: ring too small (%d)\n", len);
        return -1;
    }

    for (i = 0; i < len - 1; i++) {
        if (s_slot_quiet[s_rw_seq[i]]) { base_i = i; break; }
    }
    base_quiescent = (base_i >= 0);
    if (base_i < 0) base_i = 0;

    n_inputs = (uint32_t)(len - 1 - base_i);
    if (n_inputs > sizeof(inputs)) n_inputs = sizeof(inputs);
    for (i = 0; i < (int)n_inputs; i++)
        inputs[i] = s_slot_input[s_rw_seq[base_i + 1 + i]];
    base_state = s_rw_data + (size_t)s_rw_seq[base_i] * s_state_size;

    now = time(NULL);
    tmv = localtime(&now);
    strftime(id, sizeof(id), "r%Y%m%d_%H%M%S", tmv);
    snprintf(dir, sizeof(dir), "%s/%s", SUITE_REPORTS, id);
    suite_mkdir("bugs");
    suite_mkdir(SUITE_REPORTS);
    if (suite_mkdir(dir) != 0) {

        int k;
        for (k = 2; k < 10; k++) {
            snprintf(dir, sizeof(dir), "%s/%s_%d", SUITE_REPORTS, id, k);
            if (suite_mkdir(dir) == 0) {
                snprintf(id + strlen(id), sizeof(id) - strlen(id), "_%d", k);
                break;
            }
        }
        if (k >= 10) return -1;
    }

    snprintf(path, sizeof(path), "%s/capture.rpl", dir);
    if (suite_write_rpl(path, base_state, inputs, n_inputs) != 0) {
        printf("[suite] capture: rpl write failed\n");
        return -1;
    }

    if (s_scratch && Save_StateCaptureToBuffer(s_scratch, s_state_size) == 0) {
        snprintf(path, sizeof(path), "%s/state_now.rpl", dir);
        suite_write_rpl(path, s_scratch, NULL, 0);
    }

    snprintf(path, sizeof(path), "%s/screen.bmp", dir);
    Display_SaveScreenshot(path);
    snprintf(path, sizeof(path), "%s/log_tail.txt", dir);
    suite_write_log_tail(path);
    Trace_DumpAll(dir);

    snprintf(path, sizeof(path), "%s/report.json", dir);
    {
        FILE *fp = fopen(path, "w");
        char created[40], msg_esc[1024];
        if (!fp) return -1;
        strftime(created, sizeof(created), "%Y-%m-%dT%H:%M:%S", tmv);
        suite_json_escape(message, msg_esc, sizeof(msg_esc));
        fprintf(fp,
            "{\n"
            "  \"id\": \"%s\",\n"
            "  \"status\": \"active\",\n"
            "  \"created\": \"%s\",\n"
            "  \"message\": \"%s\",\n"
            "  \"notes\": [],\n"
            "  \"map\": %d,\n"
            "  \"x\": %d,\n"
            "  \"y\": %d,\n"
            "  \"scene\": %d,\n"
            "  \"in_battle\": %d,\n"
            "  \"frame\": %llu,\n"
            "  \"input_frames\": %u,\n"
            "  \"ring_frames\": %d,\n"
            "  \"base_quiescent\": %s,\n"
            "  \"base_offset_frames\": %d,\n"
            "  \"files\": {\n"
            "    \"replay\": \"capture.rpl\",\n"
            "    \"state_now\": \"state_now.rpl\",\n"
            "    \"screenshot\": \"screen.bmp\",\n"
            "    \"log\": \"log_tail.txt\"\n"
            "  }\n"
            "}\n",
            id, created, msg_esc,
            (int)wCurMap, (int)wXCoord, (int)wYCoord,
            Game_GetScene(), (int)wIsInBattle,
            (unsigned long long)s_frame, n_inputs, len - 1,
            base_quiescent ? "true" : "false", base_i);
        fclose(fp);
    }

    {
        FILE *fp = fopen(SUITE_REPORTS "/latest.txt", "w");
        if (fp) { fprintf(fp, "%s\n", id); fclose(fp); }
    }

    snprintf(s_last_report_id, sizeof(s_last_report_id), "%s", id);
    snprintf(toast, sizeof(toast), "report %s captured (%u frames%s)",
             id, n_inputs, base_quiescent ? "" : ", base NOT quiescent");
    DebugCLI_HistoryPushExternal(toast);
    printf("[suite] %s\n", toast);
    fflush(stdout);
    return 0;
}

static void suite_probe_player(void) {
    char data[512];
    snprintf(data, sizeof(data),
        "{\"map\":%d,\"x\":%d,\"y\":%d,\"dir\":%d,\"walk_counter\":%d,"
        "\"walkbike\":%d,\"scene\":%d,\"in_battle\":%d,\"joy_ignore\":%d,"
        "\"text_open\":%d,\"warp_fade\":%d,\"scene_legacy\":%d,\"scene_v2\":%d,"
        "\"gym_active\":%d,\"gym_leader_no\":%d,"
        "\"route24_active\":%d,\"bills_house_active\":%d}",
        (int)wCurMap, (int)wXCoord, (int)wYCoord, (int)wPlayerDirection,
        (int)wWalkCounter, (int)wWalkBikeSurfState, Game_GetScene(),
        (int)wIsInBattle, (int)wJoyIgnore, Text_IsOpen(),
        Game_IsWarpFadeActive(), DebugCLI_SceneIsActive(),
        AmberScript_Scene_IsActive(),
        GymScripts_IsActive(), (int)wGymLeaderNo,
        Route24Scripts_IsActive(), BillsHouseScripts_IsActive());
    suite_ack("probe_player", 1, data);
}

static void suite_probe_npcs(void) {
    static char data[16 * 1024];
    size_t o = 0;
    int count = NPC_GetCount();
    o += (size_t)snprintf(data + o, sizeof(data) - o, "[");
    for (int i = 0; i < count && o + 128 < sizeof(data); i++) {
        int tx = 0, ty = 0, px = 0, py = 0;
        NPC_GetTilePos(i, &tx, &ty);
        NPC_GetScreenPos(i, &px, &py);
        o += (size_t)snprintf(data + o, sizeof(data) - o,
            "%s{\"i\":%d,\"sprite\":%d,\"x\":%d,\"y\":%d,"
            "\"px\":%d,\"py\":%d,\"face\":%d}",
            i ? "," : "", i, NPC_GetSpriteId(i), tx, ty, px, py,
            NPC_GetFacing(i));
    }
    snprintf(data + o, sizeof(data) - o, "]");
    suite_ack("probe_npcs", 1, data);
}

static void suite_probe_hash(void) {
    char data[64];
    snprintf(data, sizeof(data), "{\"hash\":\"%016llx\"}",
             (unsigned long long)suite_state_hash());
    suite_ack("probe_hash", 1, data);
}

static void suite_probe_status(void) {
    char data[256];
    snprintf(data, sizeof(data),
        "{\"lab\":%d,\"speed_pct\":%d,\"paused\":%d,\"ring_len\":%d,"
        "\"last_report\":\"%s\",\"hashtrace\":%d}",
        s_lab, s_speed_pct, s_paused, s_rw_len ? *s_rw_len : 0,
        s_last_report_id, s_hash_fp != NULL);
    suite_ack("probe_status", 1, data);
}

static void bag_remove(int s) {
    if (s < 0 || s >= (int)wNumBagItems) return;
    for (int i = s; i < (int)wNumBagItems - 1; i++) {
        wBagItems[i * 2]     = wBagItems[(i + 1) * 2];
        wBagItems[i * 2 + 1] = wBagItems[(i + 1) * 2 + 1];
    }
    wNumBagItems--;
    wBagItems[wNumBagItems * 2] = 0xFF;
}

static void box_release(int s) {
    int b = wCurrentBoxNum % NUM_BOXES;
    int cnt = wBoxCount[b];
    if (s < 0 || s >= cnt) return;
    for (int i = s; i < cnt - 1; i++) {
        wBoxSpecies[b][i] = wBoxSpecies[b][i + 1];
        wBoxMons[b][i]    = wBoxMons[b][i + 1];
        memcpy(wBoxMonOT[b][i],    wBoxMonOT[b][i + 1],    NAME_LENGTH);
        memcpy(wBoxMonNicks[b][i], wBoxMonNicks[b][i + 1], NAME_LENGTH);
    }
    wBoxCount[b]--;
    wBoxSpecies[b][wBoxCount[b]] = 0xFF;
}

static void box_dump(void) {
    int b = wCurrentBoxNum % NUM_BOXES;
    FILE *fp = fopen("bugs/box.json.tmp", "w");
    if (!fp) return;
    fprintf(fp, "{\"current\":%d,\"capacity\":%d,\"counts\":[", b, BOX_CAPACITY);
    for (int i = 0; i < NUM_BOXES; i++)
        fprintf(fp, "%s%d", i ? "," : "", wBoxCount[i]);
    fprintf(fp, "],\"mons\":[");
    for (int i = 0; i < wBoxCount[b] && i < BOX_CAPACITY; i++) {
        box_mon_t *m = &wBoxMons[b][i];
        char nk[24];
        suite_decode_name(wBoxMonNicks[b][i], nk, sizeof(nk));
        fprintf(fp, "%s{\"slot\":%d,\"species\":%d,\"dex\":%d,\"name\":\"%s\","
                "\"nick\":\"%s\",\"level\":%d,\"hp\":%d,\"status\":\"%s\"}",
                i ? "," : "", i, m->species, gSpeciesToDex[m->species],
                Pokemon_GetNameBySpecies(m->species), nk,
                (int)m->box_level, (int)m->hp, suite_status_str(m->status));
    }
    fprintf(fp, "]}\n");
    fclose(fp);
    suite_replace("bugs/box.json.tmp", "bugs/box.json");
}

int DebugSuite_TryCommand(const char *cmd) {
    char verb[32] = {0}, arg[64] = {0};
    if (!cmd || !suite_token(cmd, 0, verb, sizeof(verb))) return 0;

    if (strcmp(verb, "capture") == 0) {
        char msg[1024] = {0};
        suite_rest(cmd, 0, msg, sizeof(msg));
        if (DebugSuite_CaptureReport(msg[0] ? msg : NULL) == 0) {
            char data[128], id_esc[96];
            suite_json_escape(s_last_report_id, id_esc, sizeof(id_esc));
            snprintf(data, sizeof(data), "{\"report\":\"%s\"}", id_esc);
            suite_ack("capture", 1, data);
        } else {
            suite_ack("capture", 0, NULL);
        }
        return 1;
    }
    if (strcmp(verb, "pause") == 0) {
        s_paused = 1;
        s_runto = 0;
        s_step_budget = 0;
        suite_ack("pause", 1, NULL);
        printf("[suite] paused at frame %llu\n", (unsigned long long)s_frame);
        return 1;
    }
    if (strcmp(verb, "resume") == 0) {
        s_paused = 0;
        suite_ack("resume", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "step") == 0) {
        int n = 1;
        if (suite_token(cmd, 1, arg, sizeof(arg))) n = atoi(arg);
        if (n < 1) n = 1;
        s_paused = 1;
        s_step_budget = n;
        suite_ack("step", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "runto") == 0) {
        unsigned long long target = 0;
        if (suite_token(cmd, 1, arg, sizeof(arg)))
            target = strtoull(arg, NULL, 10);
        if (target <= s_frame) {
            suite_ack("runto", 0, "{\"err\":\"target not in the future\"}");
            return 1;
        }
        s_runto = target;
        s_paused = 0;
        s_step_budget = 0;
        suite_ack("runto", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "probe") == 0) {
        if (!suite_token(cmd, 1, arg, sizeof(arg))) arg[0] = '\0';
        if      (strcmp(arg, "player") == 0) suite_probe_player();
        else if (strcmp(arg, "npcs") == 0)   suite_probe_npcs();
        else if (strcmp(arg, "hash") == 0)   suite_probe_hash();
        else if (strcmp(arg, "status") == 0) suite_probe_status();
        else suite_ack("probe", 0, "{\"err\":\"unknown probe (player|npcs|hash|status)\"}");
        return 1;
    }
    if (strcmp(verb, "replayfile") == 0) {
        char rest[512] = {0};
        if (!suite_rest(cmd, 0, rest, sizeof(rest))) {
            suite_ack("replayfile", 0, "{\"err\":\"usage: replayfile <path> [paused]\"}");
            return 1;
        }

        s_pending_load_paused = 0;
        {
            size_t rl = strlen(rest);
            if (rl > 7 && strcmp(rest + rl - 7, " paused") == 0) {
                s_pending_load_paused = 1;
                rest[rl - 7] = '\0';
                while (rl > 1 && rest[strlen(rest) - 1] == ' ')
                    rest[strlen(rest) - 1] = '\0';
            }
        }
        snprintf(s_pending_load, sizeof(s_pending_load), "%s", rest);

        return 1;
    }
    if (strcmp(verb, "trace") == 0) {
        char what[64] = {0};
        int on;
        suite_token(cmd, 1, arg, sizeof(arg));
        if (strcmp(arg, "on") == 0) on = 1;
        else if (strcmp(arg, "off") == 0) on = 0;
        else {
            suite_ack("trace", 0,
                "{\"err\":\"usage: trace on|off <actor|ui|npc|warp|all>[,..]\"}");
            return 1;
        }
        if (!suite_token(cmd, 2, what, sizeof(what)))
            snprintf(what, sizeof(what), "all");
        {

            int ok = 0;
            char *tok, *save = NULL;
            tok = strtok_r(what, ",", &save);
            while (tok) {
                ok |= Trace_SetStreamByName(tok, on);
                tok = strtok_r(NULL, ",", &save);
            }
            suite_ack("trace", ok, NULL);
        }
        return 1;
    }
    if (strcmp(verb, "hashtrace") == 0) {
        suite_token(cmd, 1, arg, sizeof(arg));
        if (strcmp(arg, "on") == 0) {
            if (s_hash_fp) fclose(s_hash_fp);
            s_hash_fp = fopen(SUITE_HASH_TRACE, "w");
            suite_ack("hashtrace", s_hash_fp != NULL, NULL);
        } else if (strcmp(arg, "off") == 0) {
            if (s_hash_fp) { fclose(s_hash_fp); s_hash_fp = NULL; }
            suite_ack("hashtrace", 1, NULL);
        } else {
            suite_ack("hashtrace", 0, "{\"err\":\"usage: hashtrace on|off\"}");
        }
        return 1;
    }
    if (strcmp(verb, "turbo") == 0) {
        suite_token(cmd, 1, arg, sizeof(arg));
        if (strcmp(arg, "on") == 0) s_speed_pct = 0;
        else if (strcmp(arg, "off") == 0) s_speed_pct = 100;
        suite_ack("turbo", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "speed") == 0) {

        int pct;
        if (!suite_token(cmd, 1, arg, sizeof(arg))) {
            suite_ack("speed", 0, "{\"err\":\"usage: speed <pct> (0=turbo)\"}");
            return 1;
        }
        pct = atoi(arg);
        DebugSuite_SetSpeedPct(pct);
        {
            char data[48];
            snprintf(data, sizeof(data), "{\"speed_pct\":%d}", s_speed_pct);
            suite_ack("speed", 1, data);
        }
        return 1;
    }
    if (strcmp(verb, "bagqty") == 0) {
        int s = -1, q = 0;
        sscanf(cmd, "%*s %d %d", &s, &q);
        if (s >= 0 && s < (int)wNumBagItems) {
            if (q <= 0) bag_remove(s);
            else wBagItems[s * 2 + 1] = (uint8_t)(q > 99 ? 99 : q);
        }
        suite_ack("bagqty", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "bagremove") == 0) {
        int s = -1; sscanf(cmd, "%*s %d", &s);
        bag_remove(s);
        suite_ack("bagremove", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "boxdump") == 0) {
        box_dump();
        suite_ack("boxdump", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "boxswitch") == 0) {
        int n = -1; sscanf(cmd, "%*s %d", &n);
        if (n >= 0 && n < NUM_BOXES) wCurrentBoxNum = (uint8_t)n;
        box_dump();
        suite_ack("boxswitch", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "boxwithdraw") == 0) {
        int s = -1, ok; sscanf(cmd, "%*s %d", &s);
        ok = Pokemon_WithdrawBoxMonToParty(s);
        box_dump();
        suite_ack("boxwithdraw", ok, ok ? NULL : "{\"err\":\"party full or bad slot\"}");
        return 1;
    }
    if (strcmp(verb, "boxrelease") == 0) {
        int s = -1; sscanf(cmd, "%*s %d", &s);
        box_release(s);
        box_dump();
        suite_ack("boxrelease", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "field") == 0) {
        char sub[16] = {0};
        suite_token(cmd, 1, sub, sizeof(sub));
        if (strcmp(sub, "dump") == 0) {
            DebugFields_Dump();
            suite_ack("field", 1, NULL);
        } else if (strcmp(sub, "set") == 0) {
            char nm[48] = {0}, vs[24] = {0};
            int ok;
            suite_token(cmd, 2, nm, sizeof(nm));
            suite_token(cmd, 3, vs, sizeof(vs));
            ok = DebugFields_Set(nm, strtol(vs, NULL, 0));
            if (ok) DebugFields_Dump();
            suite_ack("field", ok, ok ? NULL : "{\"err\":\"unknown field\"}");
        } else {
            suite_ack("field", 0, "{\"err\":\"usage: field set <group.name> <value> | field dump\"}");
        }
        return 1;
    }
    if (strcmp(verb, "watch") == 0) {
        char sub[16] = {0};
        suite_token(cmd, 1, sub, sizeof(sub));
        if (strcmp(sub, "clear") == 0) {
            memset(s_wp, 0, sizeof(s_wp));
            wp_write_json();
            suite_ack("watch", 1, NULL);
        } else if (strcmp(sub, "remove") == 0) {
            char ns[16] = {0}; int n;
            suite_token(cmd, 2, ns, sizeof(ns)); n = atoi(ns);
            if (n >= 0 && n < WP_MAX) s_wp[n].used = 0;
            wp_write_json();
            suite_ack("watch", 1, NULL);
        } else if (strcmp(sub, "add") == 0) {
            char ty[16] = {0}, a1[16] = {0}, a2[16] = {0}, a3[16] = {0};
            int type = 0, slot = -1;
            suite_token(cmd, 2, ty, sizeof(ty));
            suite_token(cmd, 3, a1, sizeof(a1));
            suite_token(cmd, 4, a2, sizeof(a2));
            suite_token(cmd, 5, a3, sizeof(a3));
            if      (strcmp(ty, "flag") == 0)      type = WP_FLAG_SET;
            else if (strcmp(ty, "flagclear") == 0) type = WP_FLAG_CLEAR;
            else if (strcmp(ty, "tile") == 0)      type = WP_TILE;
            else if (strcmp(ty, "map") == 0)       type = WP_MAP_ENTER;
            else if (strcmp(ty, "hp") == 0)        type = WP_HP_BELOW;
            else if (strcmp(ty, "battle") == 0)    type = WP_BATTLE;
            if (!type) { suite_ack("watch", 0, "{\"err\":\"unknown type\"}"); return 1; }
            for (int i = 0; i < WP_MAX; i++) if (!s_wp[i].used) { slot = i; break; }
            if (slot < 0) { suite_ack("watch", 0, "{\"err\":\"watchpoints full\"}"); return 1; }
            {
                watchpoint_t *w = &s_wp[slot];
                memset(w, 0, sizeof(*w));
                w->used = 1; w->type = type;
                w->a = atoi(a1); w->b = atoi(a2); w->c = atoi(a3);
                w->prev = wp_condition(w);
                switch (type) {
                    case WP_FLAG_SET:   snprintf(w->desc, sizeof(w->desc), "flag %d set", w->a); break;
                    case WP_FLAG_CLEAR: snprintf(w->desc, sizeof(w->desc), "flag %d cleared", w->a); break;
                    case WP_TILE:       snprintf(w->desc, sizeof(w->desc), "reach map %d (%d,%d)", w->a, w->b, w->c); break;
                    case WP_MAP_ENTER:  snprintf(w->desc, sizeof(w->desc), "enter map %d", w->a); break;
                    case WP_HP_BELOW:   snprintf(w->desc, sizeof(w->desc), "any HP <= %d", w->a); break;
                    case WP_BATTLE:     snprintf(w->desc, sizeof(w->desc), "battle starts"); break;
                }
            }
            wp_write_json();
            suite_ack("watch", 1, NULL);
        } else {
            suite_ack("watch", 0, "{\"err\":\"usage: watch add|remove|clear\"}");
        }
        return 1;
    }
    if (strcmp(verb, "nowilds") == 0) {
        extern int gNoWilds;
        suite_token(cmd, 1, arg, sizeof(arg));
        gNoWilds = (strcmp(arg, "on") == 0);
        suite_ack("nowilds", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "collision") == 0) {
        extern void Debug_SetCollisionOverlay(int);
        suite_token(cmd, 1, arg, sizeof(arg));
        Debug_SetCollisionOverlay(strcmp(arg, "on") == 0);
        suite_ack("collision", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "gridoverlay") == 0) {
        suite_token(cmd, 1, arg, sizeof(arg));
        Display_SetBlockIDOverlay(strcmp(arg, "on") == 0);
        suite_ack("gridoverlay", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "flagdump") == 0) {

        FILE *fp = fopen("bugs/flags.json", "w");
        int first = 1;
        if (!fp) { suite_ack("flagdump", 0, NULL); return 1; }
        fprintf(fp, "{\"set\":[");
        for (int n = 0; n < NUM_EVENTS; n++) {
            if (!CheckEvent((uint16_t)n)) continue;
            fprintf(fp, "%s{\"n\":%d,\"name\":\"%s\"}",
                    first ? "" : ",", n, EventFlagName((uint16_t)n));
            first = 0;
        }
        fprintf(fp, "]}\n");
        fclose(fp);
        suite_ack("flagdump", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "savegame") == 0) {
        int ok = (Save_Write() == 0);
        suite_ack("savegame", ok, NULL);
        printf("[suite] savegame: %s\n", ok ? "written" : "FAILED");
        return 1;
    }
    if (strcmp(verb, "setmove") == 0) {

        char pslot_s[16] = {0}, mslot_s[16] = {0}, move_s[32] = {0};
        int pslot, mslot, id = 0;
        suite_token(cmd, 1, pslot_s, sizeof(pslot_s));
        suite_token(cmd, 2, mslot_s, sizeof(mslot_s));
        suite_rest(cmd, 2, move_s, sizeof(move_s));
        pslot = atoi(pslot_s) - 1;
        mslot = atoi(mslot_s) - 1;
        if (move_s[0] >= '0' && move_s[0] <= '9') {
            id = atoi(move_s);
        } else {
            for (int i = 1; i < NUM_MOVE_DEFS; i++)
                if (gMoveNames[i] && strcasecmp(gMoveNames[i], move_s) == 0) { id = i; break; }
        }
        if (pslot < 0 || pslot >= (int)wPartyCount || mslot < 0 || mslot > 3) {
            suite_ack("setmove", 0, "{\"err\":\"bad party/move slot\"}");
        } else if (id < 1 || id >= NUM_MOVE_DEFS) {
            suite_ack("setmove", 0, "{\"err\":\"unknown move\"}");
        } else {
            wPartyMons[pslot].base.moves[mslot] = (uint8_t)id;
            wPartyMons[pslot].base.pp[mslot] = gMoves[id].pp;
            suite_ack("setmove", 1, NULL);
        }
        return 1;
    }
    if (strcmp(verb, "mapdump") == 0) {
        DebugSuite_WriteMapGrid();
        suite_ack("mapdump", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "rewind") == 0) {

        if (!suite_token(cmd, 1, arg, sizeof(arg))) {
            suite_ack("rewind", 0, "{\"err\":\"usage: rewind <frames> (neg=forward)\"}");
            return 1;
        }
        s_rewind_delta += atoi(arg);
        suite_ack("rewind", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "bp_restore") == 0) {

        if (!suite_token(cmd, 1, arg, sizeof(arg))) {
            suite_ack("bp_restore", 0, "{\"err\":\"usage: bp_restore <name>\"}");
            return 1;
        }
        snprintf(s_bp_restore_req, sizeof(s_bp_restore_req), "%s", arg);

        s_paused = 1;
        s_runto = 0;
        s_step_budget = 0;
        suite_ack("bp_restore", 1, NULL);
        return 1;
    }
    if (strcmp(verb, "bp_commit") == 0) {

        s_bp_commit_req = 1;
        suite_ack("bp_commit", 1, NULL);
        return 1;
    }

    if (strcmp(verb, "npc_face") == 0 || strcmp(verb, "npc_walk") == 0 ||
        strcmp(verb, "npc_move") == 0 || strcmp(verb, "npc_hide") == 0 ||
        strcmp(verb, "npc_show") == 0) {
        char a1[16] = {0}, a2[16] = {0}, a3[16] = {0};
        int i;
        suite_token(cmd, 1, a1, sizeof(a1));
        i = atoi(a1);
        if (i < 0 || i >= NPC_GetCount()) {
            suite_ack(verb, 0, "{\"err\":\"bad npc index\"}");
            return 1;
        }
        if (strcmp(verb, "npc_face") == 0) {
            suite_token(cmd, 2, a2, sizeof(a2));
            NPC_SetFacing(i, atoi(a2));
        } else if (strcmp(verb, "npc_walk") == 0) {
            suite_token(cmd, 2, a2, sizeof(a2));
            NPC_DoScriptedStep(i, atoi(a2));
        } else if (strcmp(verb, "npc_move") == 0) {
            suite_token(cmd, 2, a2, sizeof(a2));
            suite_token(cmd, 3, a3, sizeof(a3));
            NPC_SetTilePos(i, atoi(a2), atoi(a3));
        } else if (strcmp(verb, "npc_hide") == 0) {
            NPC_HideSprite(i);
        } else {
            NPC_ShowSprite(i);
        }
        DebugSuite_WriteMapGrid();
        suite_ack(verb, 1, NULL);
        return 1;
    }
    if (strcmp(verb, "scenedump") == 0) {
        char rest[512] = {0};
        const char *path = suite_rest(cmd, 0, rest, sizeof(rest))
                         ? rest : "bugs/scene_disasm.txt";
        int n = AmberScript_SceneDisasm(path);
        char data[64];
        snprintf(data, sizeof(data), "{\"commands\":%d}", n);
        suite_ack("scenedump", n >= 0, data);
        return 1;
    }
    if (strcmp(verb, "statedump") == 0) {
        char rest[512] = {0};
        const char *path = suite_rest(cmd, 0, rest, sizeof(rest))
                         ? rest : "bugs/state.bin";
        int ok = 0;
        if (s_scratch && Save_StateCaptureToBuffer(s_scratch, s_state_size) == 0) {
            FILE *fp = fopen(path, "wb");
            if (fp) {
                ok = fwrite(s_scratch, 1, s_state_size, fp) == s_state_size;
                fclose(fp);
            }
        }
        suite_ack("statedump", ok, NULL);
        return 1;
    }
    if (strcmp(verb, "screenshot") == 0) {
        char rest[512] = {0};
        const char *path = suite_rest(cmd, 0, rest, sizeof(rest))
                         ? rest : "bugs/shot.bmp";
        int ok = (Display_SaveScreenshot(path) == 0);
        char data[600], esc[540];
        suite_json_escape(path, esc, sizeof(esc));
        snprintf(data, sizeof(data), "{\"path\":\"%s\"}", esc);
        suite_ack("screenshot", ok, data);
        return 1;
    }
    if (strcmp(verb, "suite") == 0) {
        suite_probe_status();
        return 1;
    }
    if (strcmp(verb, "quit") == 0) {
        s_quit = 1;
        if (s_hash_fp) { fclose(s_hash_fp); s_hash_fp = NULL; }
        suite_ack("quit", 1, NULL);
        printf("[suite] quit requested via CLI\n");
        fflush(stdout);
        return 1;
    }
    return 0;
}
