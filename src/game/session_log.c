#include "session_log.h"
#include "../platform/hardware.h"
#include "../data/base_stats.h"
#include "pokemon.h"
#include "inventory.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#if defined(__GNUC__)
extern int Game_GetScene(void) __attribute__((weak));
extern void DebugCLI_HistoryPushExternal(const char *line) __attribute__((weak));
#else
extern int Game_GetScene(void);
extern void DebugCLI_HistoryPushExternal(const char *line);
#endif

static void ensure_bugs_dir(void) {
#ifdef _WIN32
    _mkdir("bugs");
#else
    mkdir("bugs", 0777);
#endif
}

static FILE *open_narrative_log(void) {
    ensure_bugs_dir();
    return fopen("bugs/session_narrative.log", "a");
}

static FILE *open_jsonl_log(void) {
    ensure_bugs_dir();
    return fopen("bugs/session_events.jsonl", "a");
}

static void make_timestamp(char out[32]) {
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    if (lt) strftime(out, 32, "%Y-%m-%d %H:%M:%S", lt);
    else snprintf(out, 32, "time?");
}

static void json_escape_to_file(FILE *fp, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    fputc('"', fp);
    while (*p) {
        unsigned char c = *p++;
        if (c == '\\' || c == '"') {
            fputc('\\', fp);
            fputc((int)c, fp);
        } else if (c == '\n') {
            fputs("\\n", fp);
        } else if (c == '\r') {
            fputs("\\r", fp);
        } else if (c == '\t') {
            fputs("\\t", fp);
        } else if (c < 0x20) {
            fprintf(fp, "\\u%04x", (unsigned)c);
        } else {
            fputc((int)c, fp);
        }
    }
    fputc('"', fp);
}

static void write_jsonl(const char *type, const char *message, const char *extra_json_fields) {
    FILE *fp = open_jsonl_log();
    char ts[32];
    int scene = 0;
    if (!fp) return;

    make_timestamp(ts);
    if (Game_GetScene) scene = Game_GetScene();

    fputs("{\"ts\":", fp);
    json_escape_to_file(fp, ts);
    fputs(",\"type\":", fp);
    json_escape_to_file(fp, type ? type : "event");
    fputs(",\"map\":", fp);
    fprintf(fp, "%u", (unsigned)wCurMap);
    fputs(",\"x\":", fp);
    fprintf(fp, "%d", (int)wXCoord);
    fputs(",\"y\":", fp);
    fprintf(fp, "%d", (int)wYCoord);
    fputs(",\"scene\":", fp);
    fprintf(fp, "%d", scene);
    fputs(",\"message\":", fp);
    json_escape_to_file(fp, message ? message : "");
    if (extra_json_fields && extra_json_fields[0]) {
        fputs(",", fp);
        fputs(extra_json_fields, fp);
    }
    fputs("}\n", fp);
    fclose(fp);
}

void SessionLog_Eventf(const char *fmt, ...) {
    FILE *fp = open_narrative_log();
    va_list ap;
    char ts[32];
    char msg[512];
    int scene = 0;
    if (!fp) return;

    make_timestamp(ts);
    if (Game_GetScene) scene = Game_GetScene();

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    fprintf(fp, "[%s] map=%u pos=(%d,%d) scene=%d :: %s\n",
            ts, (unsigned)wCurMap, (int)wXCoord, (int)wYCoord, scene, msg);
    fclose(fp);

    if (DebugCLI_HistoryPushExternal)
        DebugCLI_HistoryPushExternal(msg);
    write_jsonl("event", msg, NULL);
}

void SessionLog_NpcSpoke(int npc_idx, int tx, int ty) {
    SessionLog_Eventf("Spoke to NPC idx=%d at (%d,%d)", npc_idx, tx, ty);
}

void SessionLog_ItemPickup(uint8_t item_id, int success) {
    char name[24];
    Inventory_DecodeASCII(item_id, name, sizeof(name));
    if (name[0] == '\0') snprintf(name, sizeof(name), "item_0x%02X", item_id);
    if (success) SessionLog_Eventf("Picked up %s (0x%02X)", name, item_id);
    else SessionLog_Eventf("Tried to pick up %s (0x%02X) but bag was full", name, item_id);
}

void SessionLog_ItemAcquired(uint8_t item_id, uint8_t qty, int is_key_item, const char *reason) {
    char name[24];
    Inventory_DecodeASCII(item_id, name, sizeof(name));
    if (name[0] == '\0') snprintf(name, sizeof(name), "item_0x%02X", item_id);
    SessionLog_Eventf("%s x%u acquired: %s%s%s",
                      name, (unsigned)qty,
                      is_key_item ? "KEY ITEM" : "item",
                      reason ? " via " : "",
                      reason ? reason : "");
}

void SessionLog_CapturedMon(uint8_t species, uint8_t level, int sent_to_box, int new_dex) {
    uint8_t dex = gSpeciesToDex[species];
    const char *name = Pokemon_GetName(dex);
    SessionLog_Eventf("Captured %s (species=%u dex=%u lv=%u)%s%s",
                      name ? name : "UNKNOWN",
                      (unsigned)species, (unsigned)dex, (unsigned)level,
                      sent_to_box ? " -> sent to PC" : " -> added to party",
                      new_dex ? " [new dex entry]" : "");
}

void SessionLog_EventFlagChange(uint16_t flag, int new_value, const char *name) {
    char narrative[160];
    snprintf(narrative, sizeof(narrative), "Event flag %s (#%u) -> %d",
             (name && name[0]) ? name : "UNKNOWN",
             (unsigned)flag,
             new_value ? 1 : 0);
    SessionLog_Eventf("%s", narrative);

    {
        char payload[384];
        if (name && name[0]) {
            snprintf(payload, sizeof(payload),
                     "\"flag\":%u,\"flag_name\":\"%s\",\"new_value\":%d",
                     (unsigned)flag, name, new_value ? 1 : 0);
        } else {
            snprintf(payload, sizeof(payload),
                     "\"flag\":%u,\"flag_name\":null,\"new_value\":%d",
                     (unsigned)flag, new_value ? 1 : 0);
        }
        write_jsonl("event_flag_change", narrative, payload);
    }
}

void SessionLog_BattleStart(int is_trainer, uint8_t trainer_class, uint8_t trainer_no, uint8_t wild_species, uint8_t enemy_level) {
    char extra[256];
    if (is_trainer) {
        SessionLog_Eventf("Battle start (trainer): class=%u no=%u enemy_lv=%u",
                          (unsigned)trainer_class, (unsigned)trainer_no, (unsigned)enemy_level);
    } else {
        uint8_t dex = gSpeciesToDex[wild_species];
        const char *name = Pokemon_GetName(dex);
        SessionLog_Eventf("Battle start (wild): %s species=%u lv=%u",
                          name ? name : "UNKNOWN", (unsigned)wild_species, (unsigned)enemy_level);
    }
    snprintf(extra, sizeof(extra),
             "\"is_trainer\":%d,\"trainer_class\":%u,\"trainer_no\":%u,\"wild_species\":%u,\"enemy_level\":%u",
             is_trainer ? 1 : 0,
             (unsigned)trainer_class, (unsigned)trainer_no,
             (unsigned)wild_species, (unsigned)enemy_level);
    write_jsonl("battle_start", "battle started", extra);
}

void SessionLog_BattleEnd(uint8_t result, int is_trainer, uint8_t trainer_class, uint8_t trainer_no, uint8_t wild_species, uint8_t enemy_level) {
    const char *r = "unknown";
    char extra[384];
    switch (result) {
        case 0: r = "none"; break;
        case 1: r = "wild_victory"; break;
        case 2: r = "trainer_victory"; break;
        case 3: r = "blackout"; break;
        case 4: r = "caught"; break;
    }

    SessionLog_Eventf("Battle end: result=%s(%u) trainer=%d class=%u no=%u wild_species=%u enemy_lv=%u",
                      r, (unsigned)result, is_trainer ? 1 : 0,
                      (unsigned)trainer_class, (unsigned)trainer_no,
                      (unsigned)wild_species, (unsigned)enemy_level);

    snprintf(extra, sizeof(extra),
             "\"result\":%u,\"result_name\":\"%s\",\"is_trainer\":%d,\"trainer_class\":%u,\"trainer_no\":%u,\"wild_species\":%u,\"enemy_level\":%u,\"player_species\":%u,\"player_hp\":%u,\"player_max_hp\":%u",
             (unsigned)result, r, is_trainer ? 1 : 0,
             (unsigned)trainer_class, (unsigned)trainer_no,
             (unsigned)wild_species, (unsigned)enemy_level,
             (unsigned)wBattleMon.species, (unsigned)wBattleMon.hp, (unsigned)wBattleMon.max_hp);
    write_jsonl("battle_end", "battle ended", extra);
}
