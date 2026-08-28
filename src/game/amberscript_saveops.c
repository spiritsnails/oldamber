
#include "amberscript_saveops.h"
#include "amberscript_core.h"

#include "overworld.h"
#include "npc.h"
#include "amberscript_mapbank.h"
#include "text.h"
#include "battle/battle_ui.h"
#include "../platform/save.h"
#include "../platform/display.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"

#include "pallet_scripts.h"
#include "oakslab_scripts.h"
#include "viridian_mart_scripts.h"
#include "route24_scripts.h"
#include "bills_house_scripts.h"
#include "route2gate_scripts.h"
#include "vermilion_gym_scripts.h"
#include "rockethideout_b4f_scripts.h"
#include "rockethideout_scripts.h"
#include "game_corner_scripts.h"
#include "pokemontower6f_scripts.h"
#include "pokemontower7f_scripts.h"
#include "mrfujis_house_scripts.h"
#include "saffron_city_scripts.h"
#include "celadon_city_scripts.h"
#include "cinnabar_gym_scripts.h"
#include "cinnabar_island_scripts.h"
#include "seafoam_scripts.h"
#include "trainer_sight.h"
#include "gate_scripts.h"
#include "pokeflute.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

extern int Game_WarpToRealMap(uint8_t real_id, int x, int y);
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

#define PKS_REPLAY_DIR "bugs/replays"
#define PKS_REPLAY_MAGIC   0x31504C52u
#define PKS_REPLAY_VERSION 1u

typedef struct pks_replay_header_t {
    uint32_t magic;
    uint32_t version;
    uint32_t state_size;
    uint32_t input_frames;
} pks_replay_header_t;

static int      s_replay_recording = 0;
static int      s_replay_playing = 0;
static uint32_t s_replay_play_pos = 0;
static uint32_t s_replay_play_len = 0;
static uint8_t *s_replay_play_buf = NULL;
static FILE    *s_replay_rec_fp = NULL;
static char     s_replay_name[96] = {0};
static char     s_replay_tmp_state[192] = {0};
static char     s_replay_tmp_input[192] = {0};

extern uint8_t gCliButtons;
extern int     gCliFrames;

static void pks_sanitize_key(const char *in, char *out, size_t out_sz) {
    size_t n = 0;
    if (!in || !out || out_sz == 0) return;
    for (size_t i = 0; in[i] && n + 1 < out_sz; i++) {
        char c = in[i];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '-') {
            out[n++] = c;
        } else if (c == ' ') {
            out[n++] = '_';
        }
    }
    out[n] = '\0';
}

static void pks_build_state_path(const char *key, char *path_out, size_t path_sz) {
    char clean[80] = {0};
    int all_digits = 1;
    if (!key || !*key) {
        snprintf(path_out, path_sz, "bugs/qs_slot_1.state");
        return;
    }
    for (int i = 0; key[i]; i++) {
        if (key[i] < '0' || key[i] > '9') { all_digits = 0; break; }
    }
    pks_sanitize_key(key, clean, sizeof(clean));
    if (clean[0] == '\0') strcpy(clean, "slot_1");
    if (all_digits) snprintf(path_out, path_sz, "bugs/qs_slot_%s.state", clean);
    else snprintf(path_out, path_sz, "bugs/qs_%s.state", clean);
}

static int pks_file_exists(const char *path) {
    struct stat st;
    return (path && stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

static int pks_make_dir_if_needed(const char *path) {
    struct stat st;
    if (!path || !*path) return -1;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode) ? 0 : -1;
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0777);
#endif
}

static int pks_make_dir_tree(const char *path) {
    char tmp[256];
    size_t len;
    if (!path || !*path) return -1;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char save = tmp[i];
            tmp[i] = '\0';
            if (*tmp) pks_make_dir_if_needed(tmp);
            tmp[i] = save;
        }
    }
    return pks_make_dir_if_needed(tmp);
}

static int pks_copy_file(const char *src, const char *dst) {
    FILE *in = NULL, *out = NULL;
    char buf[4096];
    size_t n;
    in = fopen(src, "rb");
    if (!in) return -1;
    out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { fclose(in); fclose(out); return -1; }
    }
    fclose(in);
    fclose(out);
    return 0;
}

static int pks_backup_state_if_exists(const char *state_path) {
    char backup_dir[192] = {0};
    char backup_file[256] = {0};
    char base[96] = {0};
    char ext[24] = {0};
    int next_idx = 1;
    const char *name = NULL;
    const char *dot = NULL;
    DIR *d = NULL;
    struct dirent *ent;

    if (!pks_file_exists(state_path)) return 0;

    name = strrchr(state_path, '/');
    name = name ? name + 1 : state_path;
    dot = strrchr(name, '.');
    if (dot) {
        size_t blen = (size_t)(dot - name);
        if (blen >= sizeof(base)) blen = sizeof(base) - 1;
        memcpy(base, name, blen);
        base[blen] = '\0';
        snprintf(ext, sizeof(ext), "%s", dot);
    } else {
        snprintf(base, sizeof(base), "%s", name);
        snprintf(ext, sizeof(ext), ".state");
    }

    snprintf(backup_dir, sizeof(backup_dir), "bugs/%s_backup", base);
    if (pks_make_dir_if_needed(backup_dir) != 0) return -1;

    d = opendir(backup_dir);
    if (d) {
        while ((ent = readdir(d)) != NULL) {
            int idx = -1;
            char pat[128] = {0};
            snprintf(pat, sizeof(pat), "%s_%%d%s", base, ext);
            if (sscanf(ent->d_name, pat, &idx) == 1 && idx >= next_idx) {
                next_idx = idx + 1;
            }
        }
        closedir(d);
    }

    snprintf(backup_file, sizeof(backup_file), "%s/%s_%03d%s", backup_dir, base, next_idx, ext);
    return pks_copy_file(state_path, backup_file);
}

void AmberScript_ReloadAfterStateLoad(void) {
    extern int Game_SceneHasMap(void);

    if (!Game_SceneHasMap()) return;

    Map_Load(wCurMap);
    NPC_Load();
    PalletScripts_OnMapLoad();
    OaksLabScripts_OnMapLoad();
    ViridianMartScripts_OnMapLoad();
    Route24Scripts_OnMapLoad();
    BillsHouseScripts_OnMapLoad();
    Route2GateScripts_OnMapLoad();
    VermilionGymScripts_OnMapLoad();
    RocketHideoutB4FScripts_OnMapLoad();
    RocketHideoutScripts_OnMapLoad();
    GameCornerScripts_OnMapLoad();
    PokemonTower6FScripts_OnMapLoad();
    PokemonTower7FScripts_OnMapLoad();
    MrFujisHouseScripts_OnMapLoad();
    SaffronCityScripts_OnMapLoad();
    CeladonGiftScripts_OnMapLoad();
    CinnabarGymScripts_OnMapLoad();
    CinnabarIslandScripts_OnMapLoad();
    SeafoamScripts_OnMapLoad();
    Trainer_LoadMap();
    Gate_LoadMap();
    PokeFlute_LoadMap();
    if (Save_StateWasBattle()) {
        BattleUI_Restore();
    }
}

static void pks_build_replay_path(const char *name, char *out_path, size_t out_sz) {
    char clean[96] = {0};
    pks_sanitize_key(name, clean, sizeof(clean));
    if (clean[0] == '\0') snprintf(clean, sizeof(clean), "replay");
    snprintf(out_path, out_sz, "%s/%s.rpl", PKS_REPLAY_DIR, clean);
}

static void pks_replay_reset_playback(void) {
    if (s_replay_play_buf) {
        free(s_replay_play_buf);
        s_replay_play_buf = NULL;
    }
    s_replay_playing = 0;
    s_replay_play_pos = 0;
    s_replay_play_len = 0;
}

static void pks_replay_reset_recording(void) {
    if (s_replay_rec_fp) {
        fclose(s_replay_rec_fp);
        s_replay_rec_fp = NULL;
    }
    s_replay_recording = 0;
    s_replay_name[0] = '\0';
    s_replay_tmp_state[0] = '\0';
    s_replay_tmp_input[0] = '\0';
}

static int pks_replay_start_record(const char *name) {
    char clean[96] = {0};
    if (!name || !*name) return -1;
    pks_sanitize_key(name, clean, sizeof(clean));
    if (clean[0] == '\0') return -1;

    pks_make_dir_tree(PKS_REPLAY_DIR);
    pks_replay_reset_playback();
    pks_replay_reset_recording();

    snprintf(s_replay_name, sizeof(s_replay_name), "%s", clean);
    snprintf(s_replay_tmp_state, sizeof(s_replay_tmp_state), "%s/%s.tmp.state", PKS_REPLAY_DIR, clean);
    snprintf(s_replay_tmp_input, sizeof(s_replay_tmp_input), "%s/%s.tmp.inputs", PKS_REPLAY_DIR, clean);

    if (Save_StateWrite(s_replay_tmp_state) != 0) return -1;
    s_replay_rec_fp = fopen(s_replay_tmp_input, "wb");
    if (!s_replay_rec_fp) {
        remove(s_replay_tmp_state);
        return -1;
    }
    s_replay_recording = 1;
    return 0;
}

static int pks_replay_stop_record(char *out_final_path, size_t out_sz) {
    FILE *state = NULL, *in = NULL, *out = NULL;
    long state_sz_l = 0, input_sz_l = 0;
    size_t state_sz, input_sz;
    pks_replay_header_t hdr;
    char final_path[192] = {0};
    char copy_buf[4096];
    size_t n;

    if (!s_replay_recording || !s_replay_rec_fp) return -1;
    fclose(s_replay_rec_fp);
    s_replay_rec_fp = NULL;
    s_replay_recording = 0;

    pks_build_replay_path(s_replay_name, final_path, sizeof(final_path));
    if (pks_backup_state_if_exists(final_path) != 0) {

    }

    state = fopen(s_replay_tmp_state, "rb");
    in = fopen(s_replay_tmp_input, "rb");
    out = fopen(final_path, "wb");
    if (!state || !in || !out) goto fail;

    if (fseek(state, 0, SEEK_END) != 0) goto fail;
    state_sz_l = ftell(state);
    if (state_sz_l < 0) goto fail;
    rewind(state);

    if (fseek(in, 0, SEEK_END) != 0) goto fail;
    input_sz_l = ftell(in);
    if (input_sz_l < 0) goto fail;
    rewind(in);

    state_sz = (size_t)state_sz_l;
    input_sz = (size_t)input_sz_l;

    hdr.magic = PKS_REPLAY_MAGIC;
    hdr.version = PKS_REPLAY_VERSION;
    hdr.state_size = (uint32_t)state_sz;
    hdr.input_frames = (uint32_t)input_sz;
    if (fwrite(&hdr, 1, sizeof(hdr), out) != sizeof(hdr)) goto fail;

    while ((n = fread(copy_buf, 1, sizeof(copy_buf), state)) > 0) {
        if (fwrite(copy_buf, 1, n, out) != n) goto fail;
    }
    while ((n = fread(copy_buf, 1, sizeof(copy_buf), in)) > 0) {
        if (fwrite(copy_buf, 1, n, out) != n) goto fail;
    }

    fclose(state); fclose(in); fclose(out);
    remove(s_replay_tmp_state);
    remove(s_replay_tmp_input);
    if (out_final_path && out_sz > 0) snprintf(out_final_path, out_sz, "%s", final_path);
    s_replay_name[0] = '\0';
    return 0;

fail:
    if (state) fclose(state);
    if (in) fclose(in);
    if (out) fclose(out);
    return -1;
}

static int pks_replay_start_play(const char *name) {
    FILE *fp = NULL;
    pks_replay_header_t hdr;
    char path[192] = {0};
    char tmp_state[192] = {0};
    uint8_t *state_buf = NULL;

    if (!name || !*name) return -1;
    pks_build_replay_path(name, path, sizeof(path));

    fp = fopen(path, "rb");
    if (!fp) return -1;
    if (fread(&hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) { fclose(fp); return -1; }
    if (hdr.magic != PKS_REPLAY_MAGIC || hdr.version != PKS_REPLAY_VERSION) { fclose(fp); return -1; }
    if (hdr.state_size == 0) { fclose(fp); return -1; }

    state_buf = (uint8_t *)malloc(hdr.state_size);
    if (!state_buf) { fclose(fp); return -1; }
    if (fread(state_buf, 1, hdr.state_size, fp) != hdr.state_size) {
        free(state_buf); fclose(fp); return -1;
    }

    pks_replay_reset_playback();
    s_replay_play_buf = NULL;
    if (hdr.input_frames > 0) {
        s_replay_play_buf = (uint8_t *)malloc(hdr.input_frames);
        if (!s_replay_play_buf) { free(state_buf); fclose(fp); return -1; }
        if (fread(s_replay_play_buf, 1, hdr.input_frames, fp) != hdr.input_frames) {
            free(state_buf); pks_replay_reset_playback(); fclose(fp); return -1;
        }
    }
    fclose(fp);

    snprintf(tmp_state, sizeof(tmp_state), "%s/.replay_load_%s.state", PKS_REPLAY_DIR, name);
    {
        FILE *sf = fopen(tmp_state, "wb");
        if (!sf) { free(state_buf); pks_replay_reset_playback(); return -1; }
        if (fwrite(state_buf, 1, hdr.state_size, sf) != hdr.state_size) {
            fclose(sf); free(state_buf); pks_replay_reset_playback(); return -1;
        }
        fclose(sf);
    }
    free(state_buf);

    if (Save_StateLoad(tmp_state) != 0) {
        remove(tmp_state);
        pks_replay_reset_playback();
        return -1;
    }
    remove(tmp_state);
    AmberScript_ReloadAfterStateLoad();

    s_replay_playing = 1;
    s_replay_play_pos = 0;
    s_replay_play_len = hdr.input_frames;
    return 0;
}

void AmberScript_SaveOps_Tick(void) {
    if (s_replay_playing) {
        if (s_replay_play_pos < s_replay_play_len) {
            gCliButtons = s_replay_play_buf[s_replay_play_pos++];
            gCliFrames  = 1;
        } else {
            pks_replay_reset_playback();
            printf("[amberscript] replay: playback complete\n");
            AmberScript_RequestDeferredWrite(4);
        }
    }

    if (s_replay_recording && s_replay_rec_fp) {
        uint8_t raw = hJoyInput;
        fwrite(&raw, 1, 1, s_replay_rec_fp);
    }
}

int AmberScript_SaveOps_TryHandle(const char *cmd, const char *verb, int n) {
    (void)n;

    if (strcmp(verb, "teleport") == 0 || strcmp(verb, "tele") == 0) {
        static const struct { const char *name; int map, x, y; } kPlaces[] = {
            { "pallet",           0,   5,  6 },
            { "pallet_town",      0,   5,  6 },
            { "viridian",         1,  23, 26 },
            { "viridian_city",    1,  23, 26 },
            { "pewter",           2,  13, 26 },
            { "pewter_city",      2,  13, 26 },
            { "cerulean",         3,  19, 18 },
            { "cerulean_city",    3,  19, 18 },
            { "vermilion",        5,  11,  4 },
            { "vermilion_city",   5,  11,  4 },
            { "lavender",         4,   3,  6 },
            { "lavender_town",    4,   3,  6 },
            { "celadon",          6,  41, 10 },
            { "celadon_city",     6,  41, 10 },
            { "fuchsia",          7,  19, 28 },
            { "fuchsia_city",     7,  19, 28 },
            { "cinnabar",         8,  11, 12 },
            { "cinnabar_island",  8,  11, 12 },
            { "indigo",           9,   9,  6 },
            { "indigo_plateau",   9,   9,  6 },
            { "saffron",         10,   9, 30 },
            { "saffron_city",    10,   9, 30 },
            { "route_4_fly",     15,  11,  6 },
            { "route_10_fly",    21,  11, 20 },
            { "viridian_gym",    52,   8,  9 },
            { "pewter_gym",      54,   8,  9 },
            { "cerulean_gym",    65,   8,  9 },
            { "vermilion_gym",   92,   8,  9 },
            { "celadon_gym",    135,   8,  9 },
            { "fuchsia_gym",    166,   8,  9 },
            { "saffron_gym",    178,   8,  9 },
            { "cinnabar_gym",   234,   8,  9 },
            { "oaks_lab",        37,  12, 11 },
            { "viridian_forest", 51,  14, 40 },
            { "mt_moon",         59,  14, 10 },
            { "rock_tunnel",    155,  14, 10 },
            { "pokemon_tower",  142,   8,  9 },
            { "silph_co",       200,   8,  9 },
            { "safari_zone",    217,  28, 20 },
            { "route_1",         12,  14, 70 },
            { "route_2",         13,  14, 10 },
            { "route_3",         14,  14, 10 },
            { "route_4",         15,  14, 10 },
            { "route_5",         16,  14, 10 },
            { "route_6",         17,  14, 70 },
            { "route_7",         18,  14, 10 },
            { "route_8",         19,  14, 70 },
            { "route_9",         20,  14, 10 },
            { "route_10",        21,  14, 10 },
            { "route_11",        22,  14, 10 },
            { "route_12",        23,  14, 10 },
            { "route_24",        33,  14, 10 },
            { "route_25",        34,  14, 10 },
            { NULL, 0, 0, 0 }
        };

        int map_id = 0, x = 3, y = 3;
        char name_arg[64] = {0};
        sscanf(cmd, "%*s %63s", name_arg);

        int found = 0;
        if (name_arg[0] && !(name_arg[0] >= '0' && name_arg[0] <= '9')) {
            for (int k = 0; name_arg[k]; k++)
                if (name_arg[k] >= 'A' && name_arg[k] <= 'Z') name_arg[k] += 32;
            for (int k = 0; kPlaces[k].name; k++) {
                if (strcmp(name_arg, kPlaces[k].name) == 0) {
                    map_id = kPlaces[k].map;
                    x      = kPlaces[k].x;
                    y      = kPlaces[k].y;
                    found  = 1;
                    break;
                }
            }
            if (!found) {
                printf("[amberscript] Unknown location: %s\n", name_arg);
                AmberScript_WriteState();
                return 1;
            }
        } else {
            char m[16]="0", xs[16]="3", ys[16]="3";
            sscanf(cmd, "%*s %15s %15s %15s", m, xs, ys);
            map_id = (int)strtol(m,  NULL, 0);
            x      = (int)strtol(xs, NULL, 0);
            y      = (int)strtol(ys, NULL, 0);
        }

        Game_WarpToRealMap((uint8_t)map_id, x, y);

        gMapPalOffset = (map_id == 0x52  ||
                         map_id == 0xE8  ||
                         AmberScript_MapBank_IsDarkForRealId(map_id)) ? 6 : 0;
        Display_LoadMapPalette();
        printf("[amberscript] teleport -> map %d (%d,%d)\n", map_id, x, y);
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "givebadge") == 0) {
        char tok[16] = {0};
        sscanf(cmd, "%*s %15s", tok);
        int v = (int)strtol(tok, NULL, 0);
        int bit = -1;
        if (v >= 0 && v <= 7) bit = v;
        else if (v >= 1 && v <= 8) bit = v - 1;
        if (bit < 0 || bit > 7) {
            printf("[amberscript] givebadge: n must be 0-7 or 1-8\n");
        } else {
            wObtainedBadges |= (uint8_t)(1u << bit);
            printf("[amberscript] givebadge %d (bit %d) -> wObtainedBadges=0x%02X\n", v, bit, wObtainedBadges);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "removebadge") == 0) {
        char tok[16] = {0};
        sscanf(cmd, "%*s %15s", tok);
        int v = (int)strtol(tok, NULL, 0);
        int bit = -1;
        if (v >= 0 && v <= 7) bit = v;
        else if (v >= 1 && v <= 8) bit = v - 1;
        if (bit < 0 || bit > 7) {
            printf("[amberscript] removebadge: n must be 0-7 or 1-8\n");
        } else {
            wObtainedBadges &= (uint8_t)~(1u << bit);
            printf("[amberscript] removebadge %d (bit %d) -> wObtainedBadges=0x%02X\n", v, bit, wObtainedBadges);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "clearguardchecks") == 0) {
        ClearEvent(EVENT_PASSED_CASCADEBADGE_CHECK);
        ClearEvent(EVENT_PASSED_THUNDERBADGE_CHECK);
        ClearEvent(EVENT_PASSED_RAINBOWBADGE_CHECK);
        ClearEvent(EVENT_PASSED_SOULBADGE_CHECK);
        ClearEvent(EVENT_PASSED_MARSHBADGE_CHECK);
        ClearEvent(EVENT_PASSED_VOLCANOBADGE_CHECK);
        ClearEvent(EVENT_PASSED_EARTHBADGE_CHECK);
        printf("[amberscript] clearguardchecks -- cleared Route 23 guard pass flags\n");
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "gamesave") == 0) {

        extern int Save_Write(void);
        if (Save_Write() == 0)
            printf("[amberscript] gamesave -> progress save written\n");
        else
            printf("[amberscript] gamesave failed\n");
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "quicksave") == 0) {
        char key[96] = {0};
        char path[160] = {0};
        if (!AmberScript_ParseArg(cmd, 1, key, sizeof(key))) strcpy(key, "1");
        pks_build_state_path(key, path, sizeof(path));
        if (pks_backup_state_if_exists(path) != 0)
            printf("[amberscript] quicksave backup failed (continuing): %s\n", path);
        if (Save_StateWrite(path) == 0)
            printf("[amberscript] quicksave %s -> %s\n", key, path);
        else
            printf("[amberscript] quicksave failed: %s\n", path);
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "quickload") == 0) {
        char key[96] = {0};
        char path[160] = {0};
        if (!AmberScript_ParseArg(cmd, 1, key, sizeof(key))) strcpy(key, "1");
        pks_build_state_path(key, path, sizeof(path));
        if (Save_StateLoad(path) == 0) {
            AmberScript_ReloadAfterStateLoad();
            printf("[amberscript] quickload %s <- %s\n", key, path);
        } else {
            printf("[amberscript] quickload failed: %s\n", path);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "csave") == 0) {
        char sub[32] = {0};
        char name[96] = {0};
        char path[160] = {0};
        if (!AmberScript_ParseArg(cmd, 1, sub, sizeof(sub))) {
            printf("[amberscript] csave usage: csave create <name> | csave load <name>\n");
            AmberScript_WriteState();
            return 1;
        }
        if (!AmberScript_ParseArg(cmd, 2, name, sizeof(name))) {
            printf("[amberscript] csave: missing name\n");
            AmberScript_WriteState();
            return 1;
        }
        pks_build_state_path(name, path, sizeof(path));
        if (strcmp(sub, "create") == 0 || strcmp(sub, "save") == 0) {
            if (pks_backup_state_if_exists(path) != 0)
                printf("[amberscript] csave backup failed (continuing): %s\n", path);
            if (Save_StateWrite(path) == 0)
                printf("[amberscript] csave create %s -> %s\n", name, path);
            else
                printf("[amberscript] csave create failed: %s\n", path);
        } else if (strcmp(sub, "load") == 0) {
            if (Save_StateLoad(path) == 0) {
                AmberScript_ReloadAfterStateLoad();
                printf("[amberscript] csave load %s <- %s\n", name, path);
            } else {
                printf("[amberscript] csave load failed: %s\n", path);
            }
        } else {
            printf("[amberscript] csave usage: csave create <name> | csave load <name>\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "replay") == 0) {
        char sub[32] = {0};
        char name[96] = {0};
        char final_path[192] = {0};
        if (!AmberScript_ParseArg(cmd, 1, sub, sizeof(sub))) {
            printf("[amberscript] replay usage: replay record <name> | replay stop | replay play <name> | replay status\n");
            AmberScript_WriteState();
            return 1;
        }
        if (strcmp(sub, "record") == 0) {
            if (!AmberScript_ParseArg(cmd, 2, name, sizeof(name))) {
                printf("[amberscript] replay record: missing name\n");
            } else if (pks_replay_start_record(name) == 0) {
                printf("[amberscript] replay record: started '%s'\n", name);
            } else {
                printf("[amberscript] replay record: failed\n");
            }
        } else if (strcmp(sub, "stop") == 0) {
            int did = 0;
            if (s_replay_recording) {
                if (pks_replay_stop_record(final_path, sizeof(final_path)) == 0)
                    printf("[amberscript] replay stop: saved %s\n", final_path);
                else
                    printf("[amberscript] replay stop: failed to finalize recording\n");
                did = 1;
            }
            if (s_replay_playing) {
                pks_replay_reset_playback();
                printf("[amberscript] replay stop: playback stopped\n");
                did = 1;
            }
            if (!did) printf("[amberscript] replay stop: nothing active\n");
        } else if (strcmp(sub, "play") == 0) {
            if (!AmberScript_ParseArg(cmd, 2, name, sizeof(name))) {
                printf("[amberscript] replay play: missing name\n");
            } else if (pks_replay_start_play(name) == 0) {
                printf("[amberscript] replay play: started '%s' (%lu frames)\n",
                       name, (unsigned long)s_replay_play_len);
            } else {
                printf("[amberscript] replay play: failed for '%s'\n", name);
            }
        } else if (strcmp(sub, "status") == 0) {
            printf("[amberscript] replay status: record=%s play=%s frame=%lu/%lu\n",
                   s_replay_recording ? "ON" : "OFF",
                   s_replay_playing ? "ON" : "OFF",
                   (unsigned long)s_replay_play_pos,
                   (unsigned long)s_replay_play_len);
        } else {
            printf("[amberscript] replay usage: replay record <name> | replay stop | replay play <name> | replay status\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "setflag") == 0) {
        char tok[16] = {0};
        sscanf(cmd, "%*s %15s", tok);
        int flag = (int)strtol(tok, NULL, 0);
        SetEvent((uint16_t)flag);
        printf("[amberscript] setflag %d -> set\n", flag);
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "clearflag") == 0) {
        char tok[16] = {0};
        sscanf(cmd, "%*s %15s", tok);
        int flag = (int)strtol(tok, NULL, 0);
        ClearEvent((uint16_t)flag);
        printf("[amberscript] clearflag %d -> cleared\n", flag);
        AmberScript_WriteState();
        return 1;
    }

    return 0;
}
