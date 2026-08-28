
#include "amberscript_mapbank.h"
#include "constants.h"
#include "amberscript_core.h"
#include "amberscript_tilemod.h"
#include "../platform/game_version.h"
#include "debug_trace.h"
#include "amberscript_scene.h"
#include "overworld.h"
#include "npc.h"
#include "trainer_sight.h"
#include "pokecenter.h"
#include "pc_menu.h"
#include "players_pc.h"
#include "pokemart.h"
#include "gym_scripts.h"
#include "vermilion_gym_scripts.h"
#include "daycare.h"
#include "name_rater_scripts.h"
#include "pokemontower6f_scripts.h"
#include "elevator_menu.h"
#include "mansion_scripts.h"
#include "cinnabar_gym_scripts.h"
#include "rockethideout_scripts.h"
#include "celadon_city_scripts.h"
#include "viridian_mart_scripts.h"
#include "debug_cli.h"
#include "inventory.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../data/map_data.h"
#include "../data/event_data.h"
#include "../data/wild_data.h"
#include "../data/event_flag_ids.h"
#include "pokemon.h"
#include "species_mod.h"
#include "gen2_species.h"
#include "crystal_color.h"
#include "../data/base_stats.h"
#include "johto_trainers.h"
#include "../data/item_names_gen.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include "text.h"

#define PKS_VMAP_MAX 512

#define PKS_VMAP_WARP_SPOTS_MAX 16

#define PKS_VMAP_NPC_MAX 16
#define PKS_VMAP_TRAINER_MAX 16

#define PKS_TEXT_VARIANT_MAX 4

typedef struct pks_text_variant_t {
    uint16_t event_id;
    uint8_t badge_mask;
    char text[PKS_MAX_TEXT];
} pks_text_variant_t;

typedef struct pks_random_variant_t {
    uint8_t weight;
    char text[PKS_MAX_TEXT];
} pks_random_variant_t;
typedef struct pks_npc_t {
    uint8_t used;
    uint8_t sprite_id;
    int16_t x, y;
    uint8_t movement;
    uint8_t facing;

    uint8_t crystal_pal;
    char text[PKS_MAX_TEXT];

    uint8_t num_variants;
    pks_text_variant_t variants[PKS_TEXT_VARIANT_MAX];

    uint8_t num_random_variants;
    pks_random_variant_t random_variants[PKS_TEXT_VARIANT_MAX];

    void (*script)(void);

    uint8_t  starts_hidden;
    uint16_t hide_if_event;

    uint16_t hide_if_event2;

    uint16_t show_if_event;

    uint16_t no_face_until_event;
} pks_npc_t;
typedef struct pks_trainer_t {
    uint8_t used;
    uint8_t sprite_id;
    int16_t x, y;
    uint8_t facing;
    uint8_t trainer_class;
    uint8_t trainer_no;
    uint8_t sight_dist;
    uint16_t flag_bit;
    char before_text[PKS_MAX_TEXT];
    char after_text[PKS_MAX_TEXT];

    char defeat_text[PKS_MAX_TEXT];

    char after_battle_scene[64];

    uint16_t hide_if_event;
    uint16_t hide_if_event2;
    uint16_t show_if_event;

    uint16_t johto_party;

    uint8_t crystal_pal;
} pks_trainer_t;

#define PKS_VMAP_ITEM_MAX 16

#define PKS_VMAP_HIDDEN_EVENT_MAX 24
typedef struct pks_item_t {
    uint8_t used;
    uint8_t item_id;
    int16_t x, y;
    uint16_t flag_bit;

    uint16_t hide_if_event;
    uint16_t hide_if_event2;
    uint16_t show_if_event;
} pks_item_t;

typedef struct pks_hidden_coin_t {
    uint8_t used;
    uint16_t amount;
    int16_t x, y;
    uint16_t flag_bit;
} pks_hidden_coin_t;

#define PKS_VMAP_SLOT_MACHINE_MAX 40
typedef struct pks_slot_machine_t {
    uint8_t used;
    int16_t x, y;

    uint8_t kind;
} pks_slot_machine_t;

#define PKS_VMAP_STATIC_ENCOUNTER_MAX 16
typedef struct pks_static_t {
    uint8_t used;
    int16_t x, y;
    uint8_t species;
    uint8_t level;
    uint16_t flag_bit;
    uint8_t sprite_id;
    uint8_t facing;

    uint8_t cry;
    char text[PKS_MAX_TEXT];
} pks_static_t;
typedef struct pks_hidden_event_t {
    uint8_t used;
    int16_t x, y;

    uint8_t facing;
    char text[PKS_MAX_TEXT];

    void (*script)(void);

    uint8_t num_variants;
    pks_text_variant_t variants[PKS_TEXT_VARIANT_MAX];
} pks_hidden_event_t;

#define PKS_WILD_SLOTS 10
typedef struct pks_wild_table_t {
    uint8_t rate;
    struct { uint8_t species; uint8_t level; } slots[PKS_WILD_SLOTS];
} pks_wild_table_t;

#define PKS_JOHTO_GRASS_SLOTS 7
#define PKS_JOHTO_WATER_SLOTS 3

typedef struct { uint8_t species; uint8_t level; } pks_wild_slot_t;

#define PKS_JOHTO_TOD_COUNT 3
typedef struct pks_johto_grass_table_t {
    uint8_t rate[PKS_JOHTO_TOD_COUNT];
    pks_wild_slot_t slots[PKS_JOHTO_TOD_COUNT][PKS_JOHTO_GRASS_SLOTS];
} pks_johto_grass_table_t;
typedef struct pks_johto_water_table_t {
    uint8_t rate;
    pks_wild_slot_t slots[PKS_JOHTO_WATER_SLOTS];
} pks_johto_water_table_t;

typedef struct pks_npc_rt_t {
    uint8_t used;
    int16_t key_x, key_y;
    int16_t x, y;
    uint8_t facing;
    uint8_t hidden;
    uint8_t has_pos;
} pks_npc_rt_t;
typedef struct pks_vmap_t {
    int used;
    char name[32];
    int bound_real_id;
    unsigned lru;

    int width_blocks, height_blocks;

    int has_streamed;

    char border_block_name[4][32];

    char border_side_name[4][4][32];

    int16_t warp_spot_x[PKS_VMAP_WARP_SPOTS_MAX];
    int16_t warp_spot_y[PKS_VMAP_WARP_SPOTS_MAX];
    uint8_t warp_spot_set[PKS_VMAP_WARP_SPOTS_MAX];

    int16_t warp_walk_into_x[PKS_VMAP_WARP_SPOTS_MAX];
    int16_t warp_walk_into_y[PKS_VMAP_WARP_SPOTS_MAX];

    uint8_t warp_walk_into_dir[PKS_VMAP_WARP_SPOTS_MAX];
    uint8_t num_warp_walk_into;

    int16_t warp_stair_x[PKS_VMAP_WARP_SPOTS_MAX];
    int16_t warp_stair_y[PKS_VMAP_WARP_SPOTS_MAX];
    uint8_t num_warp_stair;

    uint8_t is_indoor;

    uint8_t is_dark;

    uint8_t no_door_step;

    uint8_t gbc_tileset;
    uint8_t gbc_tileset_set;

    uint8_t crystal_env;
    uint8_t crystal_group;
    uint8_t crystal_env_set;

    uint8_t crystal_tileset;
    uint8_t crystal_tileset_set;
    char    crystal_slug[24];

    char music_track[32];

    pks_npc_t npcs[PKS_VMAP_NPC_MAX];
    uint8_t num_npcs;

    pks_npc_rt_t npc_rt[PKS_VMAP_NPC_MAX];
    uint8_t num_npc_rt;
    pks_trainer_t trainers[PKS_VMAP_TRAINER_MAX];
    uint8_t num_trainers;
    pks_item_t items[PKS_VMAP_ITEM_MAX];
    uint8_t num_items;

    pks_item_t hidden_items[PKS_VMAP_ITEM_MAX];
    uint8_t num_hidden_items;

    pks_hidden_coin_t hidden_coins[PKS_VMAP_ITEM_MAX];
    uint8_t num_hidden_coins;

    pks_slot_machine_t slot_machines[PKS_VMAP_SLOT_MACHINE_MAX];
    uint8_t num_slot_machines;

    pks_static_t static_encounters[PKS_VMAP_STATIC_ENCOUNTER_MAX];
    uint8_t num_static_encounters;
    pks_hidden_event_t hidden_events[PKS_VMAP_HIDDEN_EVENT_MAX];
    uint8_t num_hidden_events;
    pks_wild_table_t wild_grass;
    pks_wild_table_t wild_water;
    pks_johto_grass_table_t johto_wild_grass;
    pks_johto_water_table_t johto_wild_water;
} pks_vmap_t;
static pks_vmap_t s_vmaps[PKS_VMAP_MAX];
static unsigned s_vmap_lru_clock = 0;

static int s_slot_owner[PKS_VIRTUAL_MAP_COUNT];
static int s_slot_owner_init_done = 0;

static void pks_mb_init(void) {
    if (s_slot_owner_init_done) return;
    s_slot_owner_init_done = 1;
    for (int i = 0; i < PKS_VIRTUAL_MAP_COUNT; i++) s_slot_owner[i] = -1;
}

void AmberScript_MapBank_ResetAll(void) {
    memset(s_vmaps, 0, sizeof(s_vmaps));
    for (int i = 0; i < PKS_VIRTUAL_MAP_COUNT; i++) s_slot_owner[i] = -1;
    s_slot_owner_init_done = 1;
    s_vmap_lru_clock = 0;

    AmberScript_Scene_ClearAllMapBindings();

    AmberScript_TileMod_ResetAllMapBindings();
}

static int pks_vmap_find(const char *name) {
    for (int i = 0; i < PKS_VMAP_MAX; i++)
        if (s_vmaps[i].used && strcasecmp(s_vmaps[i].name, name) == 0) return i;
    return -1;
}

static int pks_vmap_register(const char *name) {
    int slot = pks_vmap_find(name);
    if (slot >= 0) return slot;
    for (int i = 0; i < PKS_VMAP_MAX; i++) {
        if (!s_vmaps[i].used) {
            memset(&s_vmaps[i], 0, sizeof(s_vmaps[i]));
            s_vmaps[i].used = 1;
            snprintf(s_vmaps[i].name, sizeof(s_vmaps[i].name), "%s", name);
            s_vmaps[i].bound_real_id = -1;
            return i;
        }
    }
    return -1;
}

#define PKS_VMAP_SLOT_RESERVED(phys) ((PKS_VIRTUAL_MAP_FIRST + (phys)) == 0xFF)

static int pks_mb_pick_slot(void) {
    int victim = -1;
    unsigned oldest = 0;
    pks_mb_init();
    for (int i = 0; i < PKS_VIRTUAL_MAP_COUNT; i++) {
        if (PKS_VMAP_SLOT_RESERVED(i)) continue;
        if (s_slot_owner[i] < 0) return i;
    }
    for (int i = 0; i < PKS_VIRTUAL_MAP_COUNT; i++) {
        if (PKS_VMAP_SLOT_RESERVED(i)) continue;
        unsigned lru = s_vmaps[s_slot_owner[i]].lru;
        if (victim < 0 || lru < oldest) { oldest = lru; victim = i; }
    }
    return victim;
}

#define PKS_GEN_ROOTS_N 2
static const char *const *pks_gen_roots(void) {
    static char kanto[128];
    static const char *roots[PKS_GEN_ROOTS_N] = {
        "mod_runtime/generatedmaps/johto/",

        NULL,
    };
    snprintf(kanto, sizeof kanto, "mod_runtime/generatedmaps/%s/",
             GameVersion_Current());
    roots[1] = kanto;
    return roots;
}

static int pks_genmaps_disabled(void) {
    static int cached = -1;
    if (cached < 0) {
        const char *v = getenv("POKERED_NO_GENMAPS");
        cached = (v && *v && *v != '0') ? 1 : 0;
        if (cached)
            printf("[amberscript] POKERED_NO_GENMAPS set -- generated map roots "
                   "ignored, using mod_runtime/blocks only\n");
    }
    return cached;
}

static int pks_mb_resolve(const char *spec, char *out, size_t out_sz) {
    static const char *kFmt[] = { "%s", "../%s" };
    if (pks_genmaps_disabled()) {
        for (int i = 0; i < 2; i++) {
            FILE *f;
            snprintf(out, out_sz, kFmt[i], spec);
            f = fopen(out, "rb");
            if (f) { fclose(f); return 1; }
        }
        return 0;
    }

    const char *tail = spec;
    if (strncmp(spec, "mod_runtime/", 12) == 0) tail = spec + 12;
    const char *const *gen_roots = pks_gen_roots();
    for (size_t r = 0; r < PKS_GEN_ROOTS_N; r++) {
        char cand[300];
        snprintf(cand, sizeof cand, "%s%s", gen_roots[r], tail);
        for (int i = 0; i < 2; i++) {
            FILE *f;
            snprintf(out, out_sz, kFmt[i], cand);
            f = fopen(out, "rb");
            if (f) { fclose(f); return 1; }
        }
    }
    for (int i = 0; i < 2; i++) {
        FILE *f;
        snprintf(out, out_sz, kFmt[i], spec);
        f = fopen(out, "rb");
        if (f) { fclose(f); return 1; }
    }
    return 0;
}

static int pks_mb_copy_file(const char *src_spec, const char *dst_spec) {
    char src[300], dst[300];
    FILE *in, *out;
    char buf[4096];
    size_t n;
    if (!pks_mb_resolve(src_spec, src, sizeof(src))) return 0;

    {
        static const char *kFmt[] = { "%s", "../%s" };
        int ok = 0;
        for (int i = 0; i < 2 && !ok; i++) {
            snprintf(dst, sizeof(dst), kFmt[i], dst_spec);
            out = fopen(dst, "wb");
            if (out) { ok = 1; break; }
        }
        if (!ok) return 0;
    }
    in = fopen(src, "rb");
    if (!in) { fclose(out); return 0; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
    return 1;
}

static unsigned s_stream_generation;
unsigned AmberScript_MapBank_StreamGeneration(void) { return s_stream_generation; }

uint64_t g_dbg_stream_ticks;
unsigned g_dbg_stream_count;
extern uint64_t SDL_GetPerformanceCounter(void);

static void pks_mb_stream_in_body(int vslot);

static void pks_mb_stream_in(int vslot) {
    uint64_t t0 = SDL_GetPerformanceCounter();
    pks_mb_stream_in_body(vslot);
    g_dbg_stream_ticks += SDL_GetPerformanceCounter() - t0;
    g_dbg_stream_count++;
}

static void pks_mb_stream_in_body(int vslot) {
    char block_spec[200], edits_src[200], edits_dst[200], props_spec[220];
    char resolved[320];
    int real_id = s_vmaps[vslot].bound_real_id;

    s_vmaps[vslot].num_npcs = 0;
    Trace_Emit(TRACE_NPC, "\"ev\":\"stream_in\",\"map\":%d,\"name\":\"%s\"",
               real_id, s_vmaps[vslot].name);

    snprintf(block_spec, sizeof(block_spec), "mod_runtime/blocks/%s.block", s_vmaps[vslot].name);
    if (pks_mb_resolve(block_spec, resolved, sizeof(resolved))) {

        AmberScript_BlockDefLoad(resolved);

        if (strstr(resolved, "generatedmaps")) {
            printf("[amberscript] %s: ROM-IMPORTED version (%s) -- shadows "
                   "mod_runtime/blocks/%s.block\n",
                   s_vmaps[vslot].name, resolved, s_vmaps[vslot].name);
            fflush(stdout);
        }
    }

    Map_RefreshVirtualDims();

    Map_RefreshVirtualAnim();

    snprintf(edits_src, sizeof(edits_src), "mod_runtime/map_edits/vmap_%s.txt", s_vmaps[vslot].name);
    snprintf(edits_dst, sizeof(edits_dst), "mod_runtime/map_edits/map_%d.txt", real_id);
    if (pks_mb_copy_file(edits_src, edits_dst)) {
        AmberScript_MapEditsApply();
    }

    snprintf(props_spec, sizeof(props_spec), "mod_runtime/blocks/vmap_%s_properties.block", s_vmaps[vslot].name);
    if (pks_mb_resolve(props_spec, resolved, sizeof(resolved))) {
        AmberScript_BlockDefLoad(resolved);
    }

    s_stream_generation++;
}

const char *AmberScript_MapBank_NameForRealId(int real_id) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return NULL;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return NULL;
    return s_vmaps[owner].name;
}

int AmberScript_MapBank_HasStreamedForRealId(int real_id) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return -1;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return -1;
    return s_vmaps[owner].has_streamed;
}

int AmberScript_MapSetDims(const char *name, int width_blocks, int height_blocks) {
    int vslot;
    if (!name || !*name || width_blocks <= 0 || height_blocks <= 0) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    s_vmaps[vslot].width_blocks = width_blocks;
    s_vmaps[vslot].height_blocks = height_blocks;
    return 1;
}

int AmberScript_MapBank_GetDimsForRealId(int real_id, int *width_blocks, int *height_blocks) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return 0;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return 0;
    if (s_vmaps[owner].width_blocks <= 0 || s_vmaps[owner].height_blocks <= 0) return 0;
    if (width_blocks) *width_blocks = s_vmaps[owner].width_blocks;
    if (height_blocks) *height_blocks = s_vmaps[owner].height_blocks;
    return 1;
}

int AmberScript_MapSetMusic(const char *name, const char *track_name) {
    int vslot;
    if (!name || !*name || !track_name || !*track_name) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    snprintf(s_vmaps[vslot].music_track, sizeof(s_vmaps[vslot].music_track), "%s", track_name);
    return 1;
}

int AmberScript_MapBank_GetMusicForRealId(int real_id, char *out_track, size_t out_cap) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return 0;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0 || !s_vmaps[owner].music_track[0]) return 0;
    if (out_track) snprintf(out_track, out_cap, "%s", s_vmaps[owner].music_track);
    return 1;
}

static int pks_is_numeric_token(const char *s) {
    if (!s || !*s) return 0;
    if (s[0] == '+' || s[0] == '-') s++;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        if (!*s) return 0;
        while (*s) {
            if (!((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F')))
                return 0;
            s++;
        }
        return 1;
    }
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        s++;
    }
    return 1;
}

static int pks_alloc_flag_bit(const char *what) {
    static int s_next = -1;
    if (s_next < 0) s_next = PKS_EVENT_FLAGS_BASE;
    if (s_next >= PKS_EVENT_FLAGS_BASE + PKS_EVENT_FLAGS_COUNT) {
        printf("[amberscript] %s flag range exhausted (max %d ever, shared across trainers+item balls) -- "
               "this %s's persistent state won't persist\n", what, PKS_EVENT_FLAGS_COUNT, what);
        return PKS_EVENT_FLAGS_BASE;
    }
    return s_next++;
}

enum { PKS_FLAGKIND_TRAINER = 0, PKS_FLAGKIND_ITEM = 1, PKS_FLAGKIND_HIDDEN = 2, PKS_FLAGKIND_HIDDEN_COIN = 3 };

typedef struct {
    char     name[32];
    uint8_t  kind;
    uint16_t index;
    uint16_t bit;
} pks_stable_flag_t;

static pks_stable_flag_t s_stable_flags[PKS_EVENT_FLAGS_COUNT];
static int s_stable_flag_n = -1;

#define PKS_FLAG_REGISTRY_PATH_A "mod_runtime/pks_flag_registry.txt"
#define PKS_FLAG_REGISTRY_PATH_B "../mod_runtime/pks_flag_registry.txt"

static void pks_load_flag_registry(void) {
    s_stable_flag_n = 0;
    const char *paths[] = { PKS_FLAG_REGISTRY_PATH_A, PKS_FLAG_REGISTRY_PATH_B };
    FILE *f = NULL;
    for (int i = 0; i < 2 && !f; i++) f = fopen(paths[i], "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f) && s_stable_flag_n < PKS_EVENT_FLAGS_COUNT) {
        char name[32] = {0};
        unsigned kind = 0, index = 0, bit = 0;
        if (sscanf(line, "%31s %u %u %u", name, &kind, &index, &bit) == 4) {
            pks_stable_flag_t *e = &s_stable_flags[s_stable_flag_n++];
            snprintf(e->name, sizeof(e->name), "%s", name);
            e->kind = (uint8_t)kind;
            e->index = (uint16_t)index;
            e->bit = (uint16_t)bit;
        }
    }
    fclose(f);
}

static void pks_append_flag_registry(const char *name, uint8_t kind, uint16_t index, uint16_t bit) {
    const char *paths[] = { PKS_FLAG_REGISTRY_PATH_A, PKS_FLAG_REGISTRY_PATH_B };
    for (int i = 0; i < 2; i++) {
        FILE *f = fopen(paths[i], "a");
        if (!f) continue;
        fprintf(f, "%s %u %u %u\n", name, (unsigned)kind, (unsigned)index, (unsigned)bit);
        fclose(f);
        return;
    }
}

static int pks_stable_flag_bit(const char *name, uint8_t kind, int index) {
    if (s_stable_flag_n < 0) pks_load_flag_registry();
    for (int i = 0; i < s_stable_flag_n; i++) {
        if (s_stable_flags[i].kind == kind && s_stable_flags[i].index == (uint16_t)index &&
            strcasecmp(s_stable_flags[i].name, name) == 0)
            return s_stable_flags[i].bit;
    }
    if (s_stable_flag_n >= PKS_EVENT_FLAGS_COUNT) {

        printf("[amberscript] stable flag registry FULL (%d identities) -- "
               "'%s' kind=%u index=%d gets no persistent flag. Raise "
               "PKS_EVENT_FLAGS_COUNT.\n",
               s_stable_flag_n, name ? name : "(null)", (unsigned)kind, index);
        return 0;
    }
    {
        uint16_t bit = (uint16_t)(PKS_EVENT_FLAGS_BASE + s_stable_flag_n);
        pks_stable_flag_t *e = &s_stable_flags[s_stable_flag_n++];
        snprintf(e->name, sizeof(e->name), "%s", name);
        e->kind = kind;
        e->index = (uint16_t)index;
        e->bit = bit;
        pks_append_flag_registry(name, kind, (uint16_t)index, bit);

        ClearEvent(bit);
        return bit;
    }
}

void AmberScript_ScrubStaleFlagBits(void) {
    uint8_t owned[PKS_EVENT_FLAGS_COUNT];
    int cleared = 0;
    if (s_stable_flag_n < 0) pks_load_flag_registry();
    if (s_stable_flag_n <= 0) return;
    memset(owned, 0, sizeof(owned));
    for (int i = 0; i < s_stable_flag_n; i++) {
        int rel = (int)s_stable_flags[i].bit - PKS_EVENT_FLAGS_BASE;
        if (rel >= 0 && rel < PKS_EVENT_FLAGS_COUNT) owned[rel] = 1;
    }
    for (int rel = 0; rel < PKS_EVENT_FLAGS_COUNT; rel++) {
        uint16_t bit = (uint16_t)(PKS_EVENT_FLAGS_BASE + rel);
        if (owned[rel] || !CheckEvent(bit)) continue;
        ClearEvent(bit);
        cleared++;
    }
    if (cleared)
        printf("[amberscript] flag registry: cleared %d unowned amberscript flag "
               "bit(s) left over from the pre-registry numbering (%d identities "
               "registered)\n", cleared, s_stable_flag_n);
}

typedef enum { PKS_LAST_NONE = 0, PKS_LAST_NPC, PKS_LAST_HIDDEN_EVENT, PKS_LAST_TRAINER, PKS_LAST_ITEM } pks_last_decl_kind_t;
static pks_last_decl_kind_t s_last_decl_kind = PKS_LAST_NONE;
static int s_last_decl_vslot = -1;
static int s_last_decl_index = -1;

void AmberScript_ResetLastDecl(void) {
    s_last_decl_kind = PKS_LAST_NONE;
    s_last_decl_vslot = -1;
    s_last_decl_index = -1;
}

static void pks_set_last_decl(pks_last_decl_kind_t kind, int vslot, int index) {
    s_last_decl_kind = kind;
    s_last_decl_vslot = vslot;
    s_last_decl_index = index;
}

static void pks_copy_text_with_escapes(char *out, size_t out_cap, const char *text) {
    size_t n = 0;
    const char *p;
    if (!out || out_cap == 0) return;
    for (p = text ? text : ""; *p && n + 1 < out_cap; p++) {
        if (p[0] == '\\' && p[1] == 'n') {
            out[n++] = '\n';
            p++;
        } else if (p[0] == '\\' && p[1] == 'f') {
            out[n++] = '\f';
            p++;
        } else if (p[0] == '\\' && p[1] == 'c') {

            out[n++] = TEXT_ASCII_CONT;
            p++;
        } else {
            out[n++] = *p;
        }
    }
    out[n] = '\0';
}

typedef void (*pks_npc_script_fn)(void);
static pks_npc_script_fn pks_resolve_npc_service(const char *svc) {
    static const struct { const char *name; pks_npc_script_fn fn; } tbl[] = {
        { "heal",           Pokecenter_Start    },

        { "viridian_mart",  ViridianMart_ClerkCallback },
        { "pewter_mart",    PewterMart_Start    },
        { "cerulean_mart",  CeruleanMart_Start  },
        { "vermilion_mart", VermilionMart_Start },
        { "lavender_mart",  LavenderMart_Start  },
        { "fuchsia_mart",   FuchsiaMart_Start   },
        { "cinnabar_mart",  CinnabarMart_Start  },
        { "saffron_mart",   SaffronMart_Start   },
        { "indigo_mart",    IndigoMart_Start    },

        { "celadon_mart_2f_1", Celadon2F1Mart_Start },
        { "celadon_mart_2f_2", Celadon2F2Mart_Start },
        { "celadon_mart_4f",   Celadon4FMart_Start  },
        { "celadon_mart_5f_1", Celadon5F1Mart_Start },
        { "celadon_mart_5f_2", Celadon5F2Mart_Start },

        { "pewter_gym_brock", GymScripts_BrockInteract },
        { "cerulean_gym_misty", GymScripts_MistyInteract },
        { "viridian_gym_giovanni", GymScripts_GiovanniInteract },
        { "vermilion_gym_lt_surge", GymScripts_SurgeInteract },

        { "vermilion_gym_gentleman", VermilionGymScripts_GentlemanInteract },
        { "vermilion_gym_rocker",    VermilionGymScripts_RockerInteract },
        { "vermilion_gym_sailor",    VermilionGymScripts_SailorInteract },

        { "fuchsia_gym_koga",     GymScripts_KogaInteract },
        { "fuchsia_gym_trainer1", GymScripts_FuchsiaTrainer1Interact },
        { "fuchsia_gym_trainer2", GymScripts_FuchsiaTrainer2Interact },
        { "fuchsia_gym_trainer3", GymScripts_FuchsiaTrainer3Interact },
        { "fuchsia_gym_trainer4", GymScripts_FuchsiaTrainer4Interact },
        { "fuchsia_gym_trainer5", GymScripts_FuchsiaTrainer5Interact },
        { "fuchsia_gym_trainer6", GymScripts_FuchsiaTrainer6Interact },
        { "fuchsia_gym_guide",    GymScripts_FuchsiaGuideInteract },

        { "celadon_gym_erika", GymScripts_ErikaInteract },
        { "cinnabar_gym_blaine", GymScripts_BlaineInteract },

        { "cinnabar_gym_trainer1", CinnabarGymScripts_Trainer1Interact },
        { "cinnabar_gym_trainer2", CinnabarGymScripts_Trainer2Interact },
        { "cinnabar_gym_trainer3", CinnabarGymScripts_Trainer3Interact },
        { "cinnabar_gym_trainer4", CinnabarGymScripts_Trainer4Interact },
        { "cinnabar_gym_trainer5", CinnabarGymScripts_Trainer5Interact },
        { "cinnabar_gym_trainer6", CinnabarGymScripts_Trainer6Interact },
        { "cinnabar_gym_trainer7", CinnabarGymScripts_Trainer7Interact },

        { "saffron_gym_sabrina", GymScripts_SabrinaInteract },
        { "saffron_gym_guide",   GymScripts_SaffronGuideInteract },

        { "daycare_man", Daycare_Interact },

        { "name_rater", NameRater_Start },

        { "ghost_marowak", PokemonTower6FScripts_StartGhostEncounter },
        { NULL, NULL }
    };
    if (!svc) return NULL;
    for (int i = 0; tbl[i].name; i++)
        if (strcmp(svc, tbl[i].name) == 0) return tbl[i].fn;
    printf("[amberscript] npc service 'service:%s' unknown -- no callback wired\n", svc);
    return NULL;
}

int AmberScript_RunService(const char *name) {
    pks_npc_script_fn fn = pks_resolve_npc_service(name);
    if (!fn) return 0;
    printf("[amberscript] service '%s' -> C callback\n", name);
    fflush(stdout);
    fn();
    return 1;
}

static int s_pending_crystal_pal = 0;

void AmberScript_SetPendingNpcPalette(int pal_plus_one) {
    s_pending_crystal_pal = pal_plus_one;
}

int AmberScript_MapAddNpc(const char *name, const char *sprite_or_class, int x, int y,
                          int movement, int facing, const char *text) {
    int vslot, slot;
    int sprite;
    void (*npc_script)(void) = NULL;
    if (!name || !*name) return 0;
    sprite = pks_parse_sprite(sprite_or_class);
    if (sprite < 0) {
        printf("[amberscript] npc '%s': unrecognized sprite/class '%s'\n", name, sprite_or_class ? sprite_or_class : "(null)");
        return 0;
    }

    if (text && strncmp(text, "service:", 8) == 0) {
        npc_script = pks_resolve_npc_service(text + 8);
        text = "";
    }
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;

    for (int i = 0; i < s_vmaps[vslot].num_npcs; i++) {
        if (s_vmaps[vslot].npcs[i].used &&
            s_vmaps[vslot].npcs[i].x == (int16_t)x &&
            s_vmaps[vslot].npcs[i].y == (int16_t)y &&
            s_vmaps[vslot].npcs[i].sprite_id == (uint8_t)sprite) {
            slot = i;
            goto fill_npc_slot;
        }
    }
    if (s_vmaps[vslot].num_npcs >= PKS_VMAP_NPC_MAX) {
        printf("[amberscript] npc '%s': map already has the max %d NPCs\n", name, PKS_VMAP_NPC_MAX);
        return 0;
    }
    slot = s_vmaps[vslot].num_npcs++;
fill_npc_slot:
    s_vmaps[vslot].npcs[slot].used = 1;
    s_vmaps[vslot].npcs[slot].sprite_id = (uint8_t)sprite;
    s_vmaps[vslot].npcs[slot].x = (int16_t)x;
    s_vmaps[vslot].npcs[slot].y = (int16_t)y;
    s_vmaps[vslot].npcs[slot].movement = (uint8_t)movement;
    s_vmaps[vslot].npcs[slot].facing = (uint8_t)(facing & 3);

    s_vmaps[vslot].npcs[slot].crystal_pal = (uint8_t)s_pending_crystal_pal;
    s_pending_crystal_pal = 0;
    s_vmaps[vslot].npcs[slot].num_variants = 0;
    s_vmaps[vslot].npcs[slot].num_random_variants = 0;
    s_vmaps[vslot].npcs[slot].hide_if_event = 0;
    s_vmaps[vslot].npcs[slot].hide_if_event2 = 0;
    s_vmaps[vslot].npcs[slot].show_if_event = 0;
    s_vmaps[vslot].npcs[slot].starts_hidden = 0;
    s_vmaps[vslot].npcs[slot].no_face_until_event = 0;
    s_vmaps[vslot].npcs[slot].script = npc_script;
    pks_copy_text_with_escapes(s_vmaps[vslot].npcs[slot].text, sizeof(s_vmaps[vslot].npcs[slot].text), text);
    pks_set_last_decl(PKS_LAST_NPC, vslot, slot);
    return 1;
}

static pks_npc_rt_t *pks_npc_rt_find(int vslot, int key_x, int key_y) {
    if (vslot < 0 || vslot >= PKS_VMAP_MAX) return NULL;
    for (int i = 0; i < s_vmaps[vslot].num_npc_rt; i++) {
        pks_npc_rt_t *r = &s_vmaps[vslot].npc_rt[i];
        if (r->used && r->key_x == (int16_t)key_x && r->key_y == (int16_t)key_y)
            return r;
    }
    return NULL;
}
static pks_npc_rt_t *pks_npc_rt_get_or_add(int vslot, int key_x, int key_y) {
    pks_npc_rt_t *r = pks_npc_rt_find(vslot, key_x, key_y);
    if (r) return r;
    if (s_vmaps[vslot].num_npc_rt >= PKS_VMAP_NPC_MAX) return NULL;
    r = &s_vmaps[vslot].npc_rt[s_vmaps[vslot].num_npc_rt++];
    r->used = 1; r->key_x = (int16_t)key_x; r->key_y = (int16_t)key_y;
    r->x = (int16_t)key_x; r->y = (int16_t)key_y; r->facing = 0;
    r->hidden = 0; r->has_pos = 0;
    return r;
}
static int pks_real_id_to_vslot(int real_id) {
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return -1;
    return s_slot_owner[real_id - PKS_VIRTUAL_MAP_FIRST];
}

void AmberScript_MapNpcSaveRuntime(int real_id, int key_x, int key_y,
                                  int x, int y, int facing, int hidden, int has_pos) {
    int vslot = pks_real_id_to_vslot(real_id);
    if (vslot < 0) return;
    pks_npc_rt_t *r = pks_npc_rt_get_or_add(vslot, key_x, key_y);
    if (!r) return;
    if (has_pos) {
        r->x = (int16_t)x; r->y = (int16_t)y; r->facing = (uint8_t)(facing & 3);
        r->has_pos = 1;
    }
    r->hidden = (uint8_t)(hidden ? 1 : 0);
}

int AmberScript_MapNpcResolveRuntime(int real_id, int key_x, int key_y,
                                    int *out_x, int *out_y) {
    int vslot = pks_real_id_to_vslot(real_id);
    if (vslot < 0) return 0;
    pks_npc_rt_t *r = pks_npc_rt_find(vslot, key_x, key_y);
    if (!r || !r->has_pos) return 0;
    if (out_x) *out_x = r->x;
    if (out_y) *out_y = r->y;
    return 1;
}

int AmberScript_MapFindLiveNpcByDeclaredTile(int real_id, int key_x, int key_y) {

    int owner = pks_real_id_to_vslot(real_id);
    if (owner < 0 || owner >= PKS_VMAP_MAX) return -1;

    int want = -1;
    for (int i = 0; i < s_vmaps[owner].num_npcs; i++) {
        pks_npc_t *src = &s_vmaps[owner].npcs[i];
        if (src->used && src->x == key_x && src->y == key_y) { want = i; break; }
    }
    if (want < 0) {
        for (int i = 0; i < s_vmaps[owner].num_trainers; i++) {
            pks_trainer_t *src = &s_vmaps[owner].trainers[i];
            if (src->used && src->x == key_x && src->y == key_y) {
                want = PKS_TRAINER_DECL_IDX_BASE + i;
                break;
            }
        }
    }
    if (want < 0) return -1;

    for (int i = 0; i < NPC_GetCount(); i++)
        if (NPC_GetDeclIdx(i) == want) return i;
    return -1;
}

static int s_pending_johto_party = 0;

int AmberScript_MapAddTrainer(const char *name, const char *class_name, int trainer_no,
                              int x, int y, int dir, int sight_dist,
                              const char *before_text, const char *after_text,
                              const char *defeat_text, const char *flag_name,
                              const char *sprite_override) {
    int vslot, slot, trainer_class;
    int resolved_flag = -1;
    if (!name || !*name) return 0;

    if (flag_name && *flag_name) {
        uint16_t fb;
        if (pks_is_numeric_token(flag_name)) {
            resolved_flag = (int)strtol(flag_name, NULL, 0);
        } else if (EventFlagIdByName(flag_name, &fb)) {
            resolved_flag = (int)fb;
        } else {
            printf("[amberscript] trainer '%s': unknown defeat flag '%s' -- "
                   "falling back to an auto-allocated bit\n", name, flag_name);
        }
    }
    trainer_class = pks_resolve_trainer_class_id(class_name);
    if (trainer_class <= 0) {
        printf("[amberscript] trainer '%s': unrecognized trainer class '%s'\n", name, class_name ? class_name : "(null)");
        return 0;
    }
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;

    slot = -1;
    for (int i = 0; i < s_vmaps[vslot].num_trainers; i++) {
        if (s_vmaps[vslot].trainers[i].used &&
            s_vmaps[vslot].trainers[i].x == (int16_t)x &&
            s_vmaps[vslot].trainers[i].y == (int16_t)y) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (s_vmaps[vslot].num_trainers >= PKS_VMAP_TRAINER_MAX) {
            printf("[amberscript] trainer '%s': map already has the max %d trainers\n", name, PKS_VMAP_TRAINER_MAX);
            return 0;
        }
        slot = s_vmaps[vslot].num_trainers++;
        s_vmaps[vslot].trainers[slot].flag_bit =
            (uint16_t)(resolved_flag >= 0 ? resolved_flag
                       : pks_stable_flag_bit(name, PKS_FLAGKIND_TRAINER, slot));
    } else if (resolved_flag >= 0) {

        s_vmaps[vslot].trainers[slot].flag_bit = (uint16_t)resolved_flag;
    }
    s_vmaps[vslot].trainers[slot].used = 1;

    s_vmaps[vslot].trainers[slot].hide_if_event = 0;
    s_vmaps[vslot].trainers[slot].hide_if_event2 = 0;
    s_vmaps[vslot].trainers[slot].show_if_event = 0;

    s_vmaps[vslot].trainers[slot].johto_party = (uint16_t)s_pending_johto_party;
    s_pending_johto_party = 0;

    s_vmaps[vslot].trainers[slot].crystal_pal = (uint8_t)s_pending_crystal_pal;
    s_pending_crystal_pal = 0;
    {
        int override_sprite = (sprite_override && *sprite_override) ? pks_parse_sprite(sprite_override) : -1;
        if (sprite_override && *sprite_override && override_sprite < 0)
            printf("[amberscript] trainer '%s': unrecognized sprite override '%s' -- "
                   "falling back to the class-derived sprite\n", name, sprite_override);
        s_vmaps[vslot].trainers[slot].sprite_id = (uint8_t)(override_sprite >= 0
            ? override_sprite : pks_trainer_class_to_overworld_sprite(trainer_class));
    }
    s_vmaps[vslot].trainers[slot].x = (int16_t)x;
    s_vmaps[vslot].trainers[slot].y = (int16_t)y;
    s_vmaps[vslot].trainers[slot].facing = (uint8_t)(dir & 3);
    s_vmaps[vslot].trainers[slot].trainer_class = (uint8_t)trainer_class;
    s_vmaps[vslot].trainers[slot].trainer_no = (uint8_t)trainer_no;
    s_vmaps[vslot].trainers[slot].sight_dist = (uint8_t)sight_dist;

    pks_copy_text_with_escapes(s_vmaps[vslot].trainers[slot].before_text, sizeof(s_vmaps[vslot].trainers[slot].before_text), before_text);
    pks_copy_text_with_escapes(s_vmaps[vslot].trainers[slot].after_text, sizeof(s_vmaps[vslot].trainers[slot].after_text), after_text);
    pks_copy_text_with_escapes(s_vmaps[vslot].trainers[slot].defeat_text, sizeof(s_vmaps[vslot].trainers[slot].defeat_text), defeat_text);
    pks_set_last_decl(PKS_LAST_TRAINER, vslot, slot);
    return 1;
}

int AmberScript_MapAddJohtoTrainer(const char *name, const char *class_name, int trainer_no,
                                   int x, int y, int dir, int sight_dist,
                                   const char *before_text, const char *after_text,
                                   const char *defeat_text, const char *flag_name,
                                   const char *sprite_override) {
    int idx = JohtoTrainer_Find(class_name, trainer_no);
    int cls, ok;
    char cls_buf[16];
    if (idx < 0) {
        printf("[amberscript] johto_trainer '%s': no such trainer %s #%d in "
               "gJohtoTrainers -- is generated/johto_trainers.c current?\n",
               name, class_name ? class_name : "(null)", trainer_no);
        return 0;
    }
    cls = pks_resolve_trainer_class_id(class_name);
    if (cls <= 0) {
        cls = PKS_JOHTO_PLACEHOLDER_CLASS;
        printf("[amberscript] johto_trainer '%s': Crystal class '%s' has no Gen 1 "
               "counterpart -- battle pic/name fall back to class %d; the party "
               "is unaffected\n", name, class_name ? class_name : "(null)", cls);
    }

    snprintf(cls_buf, sizeof(cls_buf), "%d", cls);
    s_pending_johto_party = idx + 1;
    ok = AmberScript_MapAddTrainer(name, cls_buf, 1, x, y, dir, sight_dist,
                                  before_text, after_text, defeat_text,
                                  flag_name, sprite_override);
    s_pending_johto_party = 0;
    return ok;
}

static int pks_resolve_item_id(const char *tok) {
    if (!tok || !*tok) return -1;
    {
        char uc[32] = {0};
        int i = 0;
        for (; tok[i] && i < 31; i++) {
            char c = tok[i];
            if (c == ' ' || c == '-') c = '_';
            uc[i] = (char)toupper((unsigned char)c);
        }
        if (!tok[i]) {
            for (unsigned n = 0; n < NUM_ITEM_NAMES; n++)
                if (strcmp(uc, kItemNames[n].name) == 0) return kItemNames[n].id;
        }

        {
            int tmhm = Inventory_TmHmIdFromName(uc);
            if (tmhm > 0) return tmhm;
        }
    }
    if (pks_is_numeric_token(tok)) return (int)strtol(tok, NULL, 0);
    return -1;
}

static void pks_normalize_species_name(const char *src, char *out, size_t out_sz, int *out_len) {
    int n = 0;
    for (int i = 0; src && src[i] && n < (int)out_sz - 1; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == 0xEF)      { out[n++] = 'm'; continue; }
        if (c == 0xF5)      { out[n++] = 'f'; continue; }
        if (c == ' ' || c == '_' || c == '.' || c == '-') continue;
        out[n++] = (char)tolower(c);
    }
    out[n] = ' ';
    if (out_len) *out_len = n;
}

static int pks_resolve_species_id(const char *species_str) {
    uint8_t sid = 0;
    if (!species_str || !*species_str) return 0;

    if (strncmp(species_str, "dex:", 4) == 0) {
        long d = strtol(species_str + 4, NULL, 10);
        if (d >= 1 && d <= 251) return (int)Gen2Species_AnyDexToInternal((uint8_t)d);
        return 0;
    }
    if (SpeciesMod_ResolveSpeciesToken(species_str, &sid)) return (int)sid;
    {
        char *end;
        long parsed = strtol(species_str, &end, 0);
        if (end != species_str && *end == '\0') {
            if (parsed >= 1 && parsed <= 151) return gDexToSpecies[parsed];
            return (int)parsed;
        }
    }
    {
        char needle[40] = {0};
        int ni = 0;
        pks_normalize_species_name(species_str, needle, sizeof needle, &ni);
        for (int dex = 1; dex <= 151; dex++) {
            const char *nm = Pokemon_GetName((uint8_t)dex);
            char norm[40] = {0};
            int nn = 0;
            pks_normalize_species_name(nm, norm, sizeof norm, &nn);
            if (strcmp(needle, norm) == 0) return gDexToSpecies[dex];
        }
    }
    return 0;
}

int AmberScript_MapAddItemBall(const char *name, int x, int y, const char *item_name_or_id) {
    int vslot, slot, item_id;
    if (!name || !*name) return 0;
    item_id = pks_resolve_item_id(item_name_or_id);
    if (item_id <= 0) {
        printf("[amberscript] item_ball '%s': unrecognized item '%s'\n", name, item_name_or_id ? item_name_or_id : "(null)");
        return 0;
    }
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    slot = -1;
    for (int i = 0; i < s_vmaps[vslot].num_items; i++) {
        if (s_vmaps[vslot].items[i].used &&
            s_vmaps[vslot].items[i].x == (int16_t)x &&
            s_vmaps[vslot].items[i].y == (int16_t)y) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (s_vmaps[vslot].num_items >= PKS_VMAP_ITEM_MAX) {
            printf("[amberscript] item_ball '%s': map already has the max %d item balls\n", name, PKS_VMAP_ITEM_MAX);
            return 0;
        }
        slot = s_vmaps[vslot].num_items++;
        s_vmaps[vslot].items[slot].flag_bit =
            (uint16_t)pks_stable_flag_bit(name, PKS_FLAGKIND_ITEM, slot);
    }
    s_vmaps[vslot].items[slot].used = 1;
    s_vmaps[vslot].items[slot].item_id = (uint8_t)item_id;
    s_vmaps[vslot].items[slot].x = (int16_t)x;
    s_vmaps[vslot].items[slot].y = (int16_t)y;
    pks_set_last_decl(PKS_LAST_ITEM, vslot, slot);
    return 1;
}

static int pks_mb_owner_for_real_id(uint8_t real_id);

int AmberScript_MapAddHiddenItem(const char *name, int x, int y, const char *item_name_or_id) {
    int vslot, slot, item_id;
    if (!name || !*name) return 0;
    item_id = pks_resolve_item_id(item_name_or_id);
    if (item_id <= 0) {
        printf("[amberscript] hidden_item '%s': unrecognized item '%s'\n", name, item_name_or_id ? item_name_or_id : "(null)");
        return 0;
    }
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    slot = -1;
    for (int i = 0; i < s_vmaps[vslot].num_hidden_items; i++) {
        if (s_vmaps[vslot].hidden_items[i].used &&
            s_vmaps[vslot].hidden_items[i].x == (int16_t)x &&
            s_vmaps[vslot].hidden_items[i].y == (int16_t)y) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (s_vmaps[vslot].num_hidden_items >= PKS_VMAP_ITEM_MAX) {
            printf("[amberscript] hidden_item '%s': map already has the max %d hidden items\n", name, PKS_VMAP_ITEM_MAX);
            return 0;
        }
        slot = s_vmaps[vslot].num_hidden_items++;
        s_vmaps[vslot].hidden_items[slot].flag_bit =
            (uint16_t)pks_stable_flag_bit(name, PKS_FLAGKIND_HIDDEN, slot);
    }
    s_vmaps[vslot].hidden_items[slot].used = 1;
    s_vmaps[vslot].hidden_items[slot].item_id = (uint8_t)item_id;
    s_vmaps[vslot].hidden_items[slot].x = (int16_t)x;
    s_vmaps[vslot].hidden_items[slot].y = (int16_t)y;
    return 1;
}

int AmberScript_GetHiddenItemAt(uint8_t real_id, int x, int y, uint8_t *out_item, uint16_t *out_flag) {
    int owner = pks_mb_owner_for_real_id(real_id);
    if (owner < 0) return 0;
    for (int i = 0; i < s_vmaps[owner].num_hidden_items; i++) {
        pks_item_t *h = &s_vmaps[owner].hidden_items[i];
        if (h->used && h->x == (int16_t)x && h->y == (int16_t)y) {
            if (out_item) *out_item = h->item_id;
            if (out_flag) *out_flag = h->flag_bit;
            return 1;
        }
    }
    return 0;
}

int AmberScript_MapAddHiddenCoin(const char *name, int x, int y, int amount) {
    int vslot, slot;
    if (!name || !*name) return 0;
    if (amount <= 0) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    slot = -1;
    for (int i = 0; i < s_vmaps[vslot].num_hidden_coins; i++) {
        if (s_vmaps[vslot].hidden_coins[i].used &&
            s_vmaps[vslot].hidden_coins[i].x == (int16_t)x &&
            s_vmaps[vslot].hidden_coins[i].y == (int16_t)y) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (s_vmaps[vslot].num_hidden_coins >= PKS_VMAP_ITEM_MAX) {
            printf("[amberscript] hidden_coin '%s': map already has the max %d hidden coins\n", name, PKS_VMAP_ITEM_MAX);
            return 0;
        }
        slot = s_vmaps[vslot].num_hidden_coins++;
        s_vmaps[vslot].hidden_coins[slot].flag_bit =
            (uint16_t)pks_stable_flag_bit(name, PKS_FLAGKIND_HIDDEN_COIN, slot);
    }
    s_vmaps[vslot].hidden_coins[slot].used = 1;
    s_vmaps[vslot].hidden_coins[slot].amount = (uint16_t)amount;
    s_vmaps[vslot].hidden_coins[slot].x = (int16_t)x;
    s_vmaps[vslot].hidden_coins[slot].y = (int16_t)y;
    return 1;
}

int AmberScript_GetHiddenCoinAt(uint8_t real_id, int x, int y, uint16_t *out_amount, uint16_t *out_flag) {
    int owner = pks_mb_owner_for_real_id(real_id);
    if (owner < 0) return 0;
    for (int i = 0; i < s_vmaps[owner].num_hidden_coins; i++) {
        pks_hidden_coin_t *h = &s_vmaps[owner].hidden_coins[i];
        if (h->used && h->x == (int16_t)x && h->y == (int16_t)y) {
            if (out_amount) *out_amount = h->amount;
            if (out_flag) *out_flag = h->flag_bit;
            return 1;
        }
    }
    return 0;
}

int AmberScript_MapAddSlotMachine(const char *name, int x, int y, int kind) {
    int vslot, slot;
    if (!name || !*name) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    slot = -1;
    for (int i = 0; i < s_vmaps[vslot].num_slot_machines; i++) {
        if (s_vmaps[vslot].slot_machines[i].used &&
            s_vmaps[vslot].slot_machines[i].x == (int16_t)x &&
            s_vmaps[vslot].slot_machines[i].y == (int16_t)y) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (s_vmaps[vslot].num_slot_machines >= PKS_VMAP_SLOT_MACHINE_MAX) {
            printf("[amberscript] slot_machine '%s': map already has the max %d slot machines\n",
                   name, PKS_VMAP_SLOT_MACHINE_MAX);
            return 0;
        }
        slot = s_vmaps[vslot].num_slot_machines++;
    }
    s_vmaps[vslot].slot_machines[slot].used = 1;
    s_vmaps[vslot].slot_machines[slot].x = (int16_t)x;
    s_vmaps[vslot].slot_machines[slot].y = (int16_t)y;
    s_vmaps[vslot].slot_machines[slot].kind = (uint8_t)kind;
    return 1;
}

int AmberScript_GetSlotMachineAt(uint8_t real_id, int x, int y, int *out_index, int *out_kind) {
    int owner = pks_mb_owner_for_real_id(real_id);
    if (owner < 0) return 0;
    for (int i = 0; i < s_vmaps[owner].num_slot_machines; i++) {
        pks_slot_machine_t *m = &s_vmaps[owner].slot_machines[i];
        if (m->used && m->x == (int16_t)x && m->y == (int16_t)y) {
            if (out_index) *out_index = i;
            if (out_kind)  *out_kind  = m->kind;
            return 1;
        }
    }
    return 0;
}

int AmberScript_MapAddStaticEncounter(const char *name, const char *species_str,
                                      int level, int x, int y,
                                      const char *event_name,
                                      const char *text, int cry,
                                      const char *sprite_str, int facing) {
    int vslot, slot, species, sprite_id;
    uint16_t flag_bit = 0;
    if (!name || !*name || level <= 0) return 0;
    species = pks_resolve_species_id(species_str);
    if (species <= 0) {
        printf("[amberscript] static_encounter '%s': unknown species '%s'\n",
               name, species_str ? species_str : "(null)");
        return 0;
    }
    if (!event_name || !EventFlagIdByName(event_name, &flag_bit)) {
        printf("[amberscript] static_encounter '%s': '%s' isn't a recognized "
               "EVENT_* flag name -- the encounter would respawn forever\n",
               name, event_name ? event_name : "(null)");
        return 0;
    }

    sprite_id = (sprite_str && *sprite_str) ? pks_parse_sprite(sprite_str) : 0x3D;
    if (sprite_id <= 0) {
        printf("[amberscript] static_encounter '%s': unknown sprite '%s' -- using POKEBALL\n",
               name, sprite_str ? sprite_str : "(null)");
        sprite_id = 0x3D;
    }
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    slot = -1;
    for (int i = 0; i < s_vmaps[vslot].num_static_encounters; i++) {
        if (s_vmaps[vslot].static_encounters[i].used &&
            s_vmaps[vslot].static_encounters[i].x == (int16_t)x &&
            s_vmaps[vslot].static_encounters[i].y == (int16_t)y) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (s_vmaps[vslot].num_static_encounters >= PKS_VMAP_STATIC_ENCOUNTER_MAX) {
            printf("[amberscript] static_encounter '%s': map already has the max %d\n",
                   name, PKS_VMAP_STATIC_ENCOUNTER_MAX);
            return 0;
        }
        slot = s_vmaps[vslot].num_static_encounters++;
    }
    {
        pks_static_t *e = &s_vmaps[vslot].static_encounters[slot];
        e->used = 1;
        e->x = (int16_t)x;
        e->y = (int16_t)y;
        e->species = (uint8_t)species;
        e->level = (uint8_t)level;
        e->flag_bit = flag_bit;
        e->cry = (uint8_t)(cry ? 1 : 0);
        e->sprite_id = (uint8_t)sprite_id;
        e->facing = (uint8_t)facing;
        snprintf(e->text, sizeof(e->text), "%s", text ? text : "");
    }
    return 1;
}

int AmberScript_GetStaticEncounterAt(uint8_t real_id, int x, int y, int *out_index) {
    int owner = pks_mb_owner_for_real_id(real_id);
    if (owner < 0) return 0;
    for (int i = 0; i < s_vmaps[owner].num_static_encounters; i++) {
        pks_static_t *e = &s_vmaps[owner].static_encounters[i];
        if (e->used && e->x == (int16_t)x && e->y == (int16_t)y) {
            if (out_index) *out_index = i;
            return 1;
        }
    }
    return 0;
}

int AmberScript_GetStaticEncounterInfo(uint8_t real_id, int index,
                                       int *out_species, int *out_level,
                                       uint16_t *out_flag, int *out_cry,
                                       const char **out_text) {
    int owner = pks_mb_owner_for_real_id(real_id);
    if (owner < 0 || index < 0 || index >= s_vmaps[owner].num_static_encounters) return 0;
    {
        pks_static_t *e = &s_vmaps[owner].static_encounters[index];
        if (!e->used) return 0;
        if (out_species) *out_species = e->species;
        if (out_level)   *out_level   = e->level;
        if (out_flag)    *out_flag    = e->flag_bit;
        if (out_cry)     *out_cry     = e->cry;
        if (out_text)    *out_text    = e->text[0] ? e->text : NULL;
    }
    return 1;
}

static void (*pks_resolve_hidden_service(const char *svc))(void) {
    static const struct { const char *name; void (*fn)(void); } tbl[] = {
        { "player_pc", PlayersPC_Activate },
        { "pc",        PCMenu_Activate    },

        { "vermilion_gym_trash_0",  VermilionGymScripts_Trash0  },
        { "vermilion_gym_trash_1",  VermilionGymScripts_Trash1  },
        { "vermilion_gym_trash_2",  VermilionGymScripts_Trash2  },
        { "vermilion_gym_trash_3",  VermilionGymScripts_Trash3  },
        { "vermilion_gym_trash_4",  VermilionGymScripts_Trash4  },
        { "vermilion_gym_trash_5",  VermilionGymScripts_Trash5  },
        { "vermilion_gym_trash_6",  VermilionGymScripts_Trash6  },
        { "vermilion_gym_trash_7",  VermilionGymScripts_Trash7  },
        { "vermilion_gym_trash_8",  VermilionGymScripts_Trash8  },
        { "vermilion_gym_trash_9",  VermilionGymScripts_Trash9  },
        { "vermilion_gym_trash_10", VermilionGymScripts_Trash10 },
        { "vermilion_gym_trash_11", VermilionGymScripts_Trash11 },
        { "vermilion_gym_trash_12", VermilionGymScripts_Trash12 },
        { "vermilion_gym_trash_13", VermilionGymScripts_Trash13 },
        { "vermilion_gym_trash_14", VermilionGymScripts_Trash14 },

        { "celadon_mart_elevator", CeladonMartElevator_PanelInteract },

        { "mansion_switch", MansionScripts_SwitchInteract },

        { "cinnabar_gym_quiz1", CinnabarGymScripts_Quiz1Interact },
        { "cinnabar_gym_quiz2", CinnabarGymScripts_Quiz2Interact },
        { "cinnabar_gym_quiz3", CinnabarGymScripts_Quiz3Interact },
        { "cinnabar_gym_quiz4", CinnabarGymScripts_Quiz4Interact },
        { "cinnabar_gym_quiz5", CinnabarGymScripts_Quiz5Interact },
        { "cinnabar_gym_quiz6", CinnabarGymScripts_Quiz6Interact },

        { "silph_co_elevator", SilphCoElevator_PanelInteract },

        { "rockethideout_elevator", RocketHideoutElevator_PanelInteract },

        { "celadon_mart_roof_vending", CeladonMartRoof_VendingMachineScript },
        { NULL, NULL }
    };
    if (!svc) return NULL;
    for (int i = 0; tbl[i].name; i++)
        if (strcmp(svc, tbl[i].name) == 0) return tbl[i].fn;
    printf("[amberscript] hidden_event service 'service:%s' unknown -- no callback wired\n", svc);
    return NULL;
}

int AmberScript_MapAddHiddenEvent(const char *name, int x, int y, const char *text, int facing) {
    int vslot, slot;
    void (*hidden_script)(void) = NULL;
    if (!name || !*name) return 0;

    if (text && strncmp(text, "service:", 8) == 0) {
        hidden_script = pks_resolve_hidden_service(text + 8);
        text = "";
    }
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    slot = -1;
    for (int i = 0; i < s_vmaps[vslot].num_hidden_events; i++) {
        if (s_vmaps[vslot].hidden_events[i].used &&
            s_vmaps[vslot].hidden_events[i].x == (int16_t)x &&
            s_vmaps[vslot].hidden_events[i].y == (int16_t)y) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (s_vmaps[vslot].num_hidden_events >= PKS_VMAP_HIDDEN_EVENT_MAX) {
            printf("[amberscript] hidden_event '%s': map already has the max %d hidden events\n", name, PKS_VMAP_HIDDEN_EVENT_MAX);
            return 0;
        }
        slot = s_vmaps[vslot].num_hidden_events++;
    }
    s_vmaps[vslot].hidden_events[slot].used = 1;
    s_vmaps[vslot].hidden_events[slot].x = (int16_t)x;
    s_vmaps[vslot].hidden_events[slot].y = (int16_t)y;
    s_vmaps[vslot].hidden_events[slot].facing =
        (facing >= 0 && facing <= 4) ? (uint8_t)facing : 0;
    s_vmaps[vslot].hidden_events[slot].num_variants = 0;
    s_vmaps[vslot].hidden_events[slot].script = hidden_script;
    pks_copy_text_with_escapes(s_vmaps[vslot].hidden_events[slot].text, sizeof(s_vmaps[vslot].hidden_events[slot].text), text);
    pks_set_last_decl(PKS_LAST_HIDDEN_EVENT, vslot, slot);
    return 1;
}

int AmberScript_AddTextVariant(const char *event_name, const char *text) {
    uint16_t event_id;
    pks_text_variant_t *variants;
    uint8_t *num_variants;
    if (!EventFlagIdByName(event_name, &event_id)) {
        printf("[amberscript] text_if: '%s' isn't a recognized EVENT_* flag name\n",
               event_name ? event_name : "(null)");
        return 0;
    }
    if (s_last_decl_kind == PKS_LAST_NPC) {
        variants = s_vmaps[s_last_decl_vslot].npcs[s_last_decl_index].variants;
        num_variants = &s_vmaps[s_last_decl_vslot].npcs[s_last_decl_index].num_variants;
    } else if (s_last_decl_kind == PKS_LAST_HIDDEN_EVENT) {
        variants = s_vmaps[s_last_decl_vslot].hidden_events[s_last_decl_index].variants;
        num_variants = &s_vmaps[s_last_decl_vslot].hidden_events[s_last_decl_index].num_variants;
    } else {
        printf("[amberscript] text_if: no preceding npc/hidden_event line to attach to\n");
        return 0;
    }
    if (*num_variants >= PKS_TEXT_VARIANT_MAX) {
        printf("[amberscript] text_if: already has the max %d variants\n", PKS_TEXT_VARIANT_MAX);
        return 0;
    }
    variants[*num_variants].event_id = event_id;
    variants[*num_variants].badge_mask = 0;
    pks_copy_text_with_escapes(variants[*num_variants].text, sizeof(variants[*num_variants].text), text);
    (*num_variants)++;
    return 1;
}

static const struct { const char *name; uint8_t mask; } kPksBadgeMasks[] = {
    { "BOULDERBADGE", 1u << BIT_BOULDERBADGE },
    { "CASCADEBADGE", 1u << BIT_CASCADEBADGE },
    { "THUNDERBADGE", 1u << BIT_THUNDERBADGE },
    { "RAINBOWBADGE", 1u << BIT_RAINBOWBADGE },
    { "SOULBADGE",    1u << BIT_SOULBADGE    },
    { "MARSHBADGE",   1u << BIT_MARSHBADGE   },
    { "VOLCANOBADGE", 1u << BIT_VOLCANOBADGE },
    { "EARTHBADGE",   1u << BIT_EARTHBADGE   },
    { NULL, 0 }
};

int AmberScript_AddBadgeTextVariant(const char *badge_name, const char *text) {
    uint8_t mask = 0;
    pks_text_variant_t *variants;
    uint8_t *num_variants;
    for (int i = 0; kPksBadgeMasks[i].name; i++) {
        if (badge_name && strcmp(badge_name, kPksBadgeMasks[i].name) == 0) {
            mask = kPksBadgeMasks[i].mask;
            break;
        }
    }
    if (!mask) {
        printf("[amberscript] badge_if: '%s' isn't a recognized badge name\n",
               badge_name ? badge_name : "(null)");
        return 0;
    }
    if (s_last_decl_kind == PKS_LAST_NPC) {
        variants = s_vmaps[s_last_decl_vslot].npcs[s_last_decl_index].variants;
        num_variants = &s_vmaps[s_last_decl_vslot].npcs[s_last_decl_index].num_variants;
    } else if (s_last_decl_kind == PKS_LAST_HIDDEN_EVENT) {
        variants = s_vmaps[s_last_decl_vslot].hidden_events[s_last_decl_index].variants;
        num_variants = &s_vmaps[s_last_decl_vslot].hidden_events[s_last_decl_index].num_variants;
    } else {
        printf("[amberscript] badge_if: no preceding npc/hidden_event line to attach to\n");
        return 0;
    }
    if (*num_variants >= PKS_TEXT_VARIANT_MAX) {
        printf("[amberscript] badge_if: already has the max %d variants\n", PKS_TEXT_VARIANT_MAX);
        return 0;
    }
    variants[*num_variants].event_id = 0;
    variants[*num_variants].badge_mask = mask;
    pks_copy_text_with_escapes(variants[*num_variants].text, sizeof(variants[*num_variants].text), text);
    (*num_variants)++;
    return 1;
}

static int pks_variant_active(const pks_text_variant_t *v) {
    if (v->badge_mask) return (wObtainedBadges & v->badge_mask) != 0;
    return CheckEvent(v->event_id) != 0;
}

int AmberScript_AddTextRandom(int weight, const char *text) {
    pks_random_variant_t *variants;
    uint8_t *num_variants;
    if (weight < 1 || weight > 255) {
        printf("[amberscript] text_random: weight %d out of range (1-255)\n", weight);
        return 0;
    }
    if (s_last_decl_kind != PKS_LAST_NPC) {
        printf("[amberscript] text_random: no preceding npc line to attach to\n");
        return 0;
    }
    variants = s_vmaps[s_last_decl_vslot].npcs[s_last_decl_index].random_variants;
    num_variants = &s_vmaps[s_last_decl_vslot].npcs[s_last_decl_index].num_random_variants;
    if (*num_variants >= PKS_TEXT_VARIANT_MAX) {
        printf("[amberscript] text_random: already has the max %d variants\n", PKS_TEXT_VARIANT_MAX);
        return 0;
    }
    {
        int sum = weight;
        for (int i = 0; i < *num_variants; i++) sum += variants[i].weight;
        if (sum >= 256) {
            printf("[amberscript] text_random: weights sum to %d, must stay under 256 "
                   "(the base npc text takes whatever's left)\n", sum);
            return 0;
        }
    }
    variants[*num_variants].weight = (uint8_t)weight;
    pks_copy_text_with_escapes(variants[*num_variants].text, sizeof(variants[*num_variants].text), text);
    (*num_variants)++;
    return 1;
}

int AmberScript_AddAfterBattle(const char *scene) {
    if (!scene || !*scene) return 0;
    if (s_last_decl_kind != PKS_LAST_TRAINER) {
        printf("[amberscript] after_battle: must follow a `trainer` line (got kind %d)\n",
               (int)s_last_decl_kind);
        return 0;
    }
    {
        pks_trainer_t *t = &s_vmaps[s_last_decl_vslot].trainers[s_last_decl_index];
        snprintf(t->after_battle_scene, sizeof(t->after_battle_scene), "%s", scene);
    }
    return 1;
}

int AmberScript_AddStartsHidden(void) {
    if (s_last_decl_kind != PKS_LAST_NPC) {
        printf("[amberscript] hidden: no preceding npc line to attach to\n");
        return 0;
    }
    s_vmaps[s_last_decl_vslot].npcs[s_last_decl_index].starts_hidden = 1;
    return 1;
}

int AmberScript_AddHideIf(const char *event_name) {
    uint16_t event_id;
    if (!EventFlagIdByName(event_name, &event_id)) {
        printf("[amberscript] hide_if: '%s' isn't a recognized EVENT_* flag name\n",
               event_name ? event_name : "(null)");
        return 0;
    }

    if (s_last_decl_kind == PKS_LAST_TRAINER) {
        pks_trainer_t *t = &s_vmaps[s_last_decl_vslot].trainers[s_last_decl_index];
        if (t->hide_if_event) t->hide_if_event2 = event_id;
        else t->hide_if_event = event_id;
        return 1;
    }
    if (s_last_decl_kind == PKS_LAST_ITEM) {
        pks_item_t *it = &s_vmaps[s_last_decl_vslot].items[s_last_decl_index];
        if (it->hide_if_event) it->hide_if_event2 = event_id;
        else it->hide_if_event = event_id;
        return 1;
    }
    if (s_last_decl_kind != PKS_LAST_NPC) {
        printf("[amberscript] hide_if: no preceding npc/trainer/item_ball line to attach to\n");
        return 0;
    }
    if (s_vmaps[s_last_decl_vslot].npcs[s_last_decl_index].hide_if_event)
        s_vmaps[s_last_decl_vslot].npcs[s_last_decl_index].hide_if_event2 = event_id;
    else
        s_vmaps[s_last_decl_vslot].npcs[s_last_decl_index].hide_if_event = event_id;
    return 1;
}

int AmberScript_AddShowIf(const char *event_name) {
    uint16_t event_id;
    if (!EventFlagIdByName(event_name, &event_id)) {
        printf("[amberscript] show_if: '%s' isn't a recognized EVENT_* flag name\n",
               event_name ? event_name : "(null)");
        return 0;
    }
    if (s_last_decl_kind == PKS_LAST_TRAINER) {
        s_vmaps[s_last_decl_vslot].trainers[s_last_decl_index].show_if_event = event_id;
        return 1;
    }
    if (s_last_decl_kind == PKS_LAST_ITEM) {
        s_vmaps[s_last_decl_vslot].items[s_last_decl_index].show_if_event = event_id;
        return 1;
    }
    if (s_last_decl_kind != PKS_LAST_NPC) {
        printf("[amberscript] show_if: no preceding npc/trainer/item_ball line to attach to\n");
        return 0;
    }
    s_vmaps[s_last_decl_vslot].npcs[s_last_decl_index].show_if_event = event_id;
    return 1;
}

int AmberScript_AddNoFaceUntil(const char *event_name) {
    uint16_t event_id;
    if (!EventFlagIdByName(event_name, &event_id)) {
        printf("[amberscript] no_face_until: '%s' isn't a recognized EVENT_* flag name\n",
               event_name ? event_name : "(null)");
        return 0;
    }
    if (s_last_decl_kind != PKS_LAST_NPC) {
        printf("[amberscript] no_face_until: no preceding npc line to attach to\n");
        return 0;
    }
    s_vmaps[s_last_decl_vslot].npcs[s_last_decl_index].no_face_until_event = event_id;
    return 1;
}

static const char *pks_roll_random_variant(pks_npc_t *src) {
    int remaining;
    uint8_t r;
    if (src->num_random_variants == 0) return NULL;
    r = (uint8_t)(hRandomAdd ^ hRandomSub ^ hFrameCounter);
    remaining = 256;
    for (int i = 0; i < src->num_random_variants; i++) {
        remaining -= src->random_variants[i].weight;
        if (r >= remaining) return src->random_variants[i].text;
    }
    return src->text[0] ? src->text : NULL;
}

const char *AmberScript_ResolveNpcText(uint8_t map_id, int npc_idx) {
    int phys, owner, n;
    pks_mb_init();
    if (map_id < PKS_VIRTUAL_MAP_FIRST || map_id > PKS_VIRTUAL_MAP_LAST) return NULL;
    phys = map_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0 || npc_idx < 0) return NULL;

    n = 0;
    for (int i = 0; i < s_vmaps[owner].num_npcs; i++) {
        pks_npc_t *src = &s_vmaps[owner].npcs[i];
        if (!src->used) continue;
        if (src->hide_if_event && CheckEvent(src->hide_if_event)) continue;
        if (src->hide_if_event2 && CheckEvent(src->hide_if_event2)) continue;
        if (src->show_if_event && !CheckEvent(src->show_if_event)) continue;

        { const pks_npc_rt_t *rt = pks_npc_rt_find(owner, src->x, src->y);
          if (rt && rt->hidden) continue; }
        if (n == npc_idx) {
            if (src->num_random_variants > 0) return pks_roll_random_variant(src);
            for (int v = 0; v < src->num_variants; v++)
                if (pks_variant_active(&src->variants[v])) return src->variants[v].text;
            return src->text[0] ? src->text : NULL;
        }
        n++;
    }
    return NULL;
}

const char *AmberScript_ResolveNpcTextByDecl(uint8_t map_id, int decl_idx) {
    int phys, owner;
    pks_mb_init();
    if (map_id < PKS_VIRTUAL_MAP_FIRST || map_id > PKS_VIRTUAL_MAP_LAST) return NULL;
    phys = map_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return NULL;
    if (decl_idx < 0 || decl_idx >= s_vmaps[owner].num_npcs) return NULL;
    pks_npc_t *src = &s_vmaps[owner].npcs[decl_idx];
    if (!src->used) return NULL;
    if (src->num_random_variants > 0) return pks_roll_random_variant(src);
    for (int v = 0; v < src->num_variants; v++)
        if (pks_variant_active(&src->variants[v])) return src->variants[v].text;
    return src->text[0] ? src->text : NULL;
}

int AmberScript_GetNpcDeclaredPos(uint8_t map_id, int decl_idx, int *out_x, int *out_y) {
    int phys, owner;
    pks_mb_init();
    if (map_id < PKS_VIRTUAL_MAP_FIRST || map_id > PKS_VIRTUAL_MAP_LAST) return 0;
    phys = map_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return 0;
    if (decl_idx < 0 || decl_idx >= s_vmaps[owner].num_npcs) return 0;
    if (!s_vmaps[owner].npcs[decl_idx].used) return 0;
    if (out_x) *out_x = s_vmaps[owner].npcs[decl_idx].x;
    if (out_y) *out_y = s_vmaps[owner].npcs[decl_idx].y;
    return 1;
}

int AmberScript_NpcSuppressesFacePlayerByDecl(uint8_t map_id, int decl_idx) {
    int phys, owner;
    pks_mb_init();
    if (map_id < PKS_VIRTUAL_MAP_FIRST || map_id > PKS_VIRTUAL_MAP_LAST) return 0;
    phys = map_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return 0;
    if (decl_idx < 0 || decl_idx >= s_vmaps[owner].num_npcs) return 0;
    pks_npc_t *src = &s_vmaps[owner].npcs[decl_idx];
    if (!src->used || !src->no_face_until_event) return 0;
    return CheckEvent(src->no_face_until_event) ? 0 : 1;
}

const char *AmberScript_ResolveHiddenEventText(uint8_t map_id, int hidden_idx) {
    int phys, owner;
    pks_hidden_event_t *he;
    pks_mb_init();
    if (map_id < PKS_VIRTUAL_MAP_FIRST || map_id > PKS_VIRTUAL_MAP_LAST) return NULL;
    phys = map_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return NULL;
    if (hidden_idx < 0 || hidden_idx >= s_vmaps[owner].num_hidden_events) return NULL;
    he = &s_vmaps[owner].hidden_events[hidden_idx];
    if (!he->used) return NULL;
    for (int i = 0; i < he->num_variants; i++) {
        if (pks_variant_active(&he->variants[i])) return he->variants[i].text;
    }
    return he->text[0] ? he->text : NULL;
}

int AmberScript_MapSetWildRate(const char *name, int is_water, int rate) {
    int vslot;
    if (!name || !*name) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    (is_water ? &s_vmaps[vslot].wild_water : &s_vmaps[vslot].wild_grass)->rate = (uint8_t)rate;
    return 1;
}

int AmberScript_MapSetWildSlot(const char *name, int is_water, int slot_no,
                               const char *species_name_or_id, int level) {
    int vslot, species;
    pks_wild_table_t *table;
    if (!name || !*name) return 0;
    if (slot_no < 1 || slot_no > PKS_WILD_SLOTS) {
        printf("[amberscript] wild_encounter '%s': slot %d out of range (want 1-%d)\n", name, slot_no, PKS_WILD_SLOTS);
        return 0;
    }
    species = pks_resolve_species_id(species_name_or_id);
    if (species <= 0) {
        printf("[amberscript] wild_encounter '%s': unrecognized species '%s'\n", name, species_name_or_id ? species_name_or_id : "(null)");
        return 0;
    }
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    table = is_water ? &s_vmaps[vslot].wild_water : &s_vmaps[vslot].wild_grass;
    table->slots[slot_no - 1].species = (uint8_t)species;
    table->slots[slot_no - 1].level = (uint8_t)level;
    return 1;
}

int AmberScript_MapSetJohtoWildRate(const char *name, int is_water, int rate,
                                    int tod) {
    int vslot;
    if (!name || !*name) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    if (is_water) {
        s_vmaps[vslot].johto_wild_water.rate = (uint8_t)rate;
    } else if (tod < 0) {

        for (int i = 0; i < PKS_JOHTO_TOD_COUNT; i++)
            s_vmaps[vslot].johto_wild_grass.rate[i] = (uint8_t)rate;
    } else if (tod < PKS_JOHTO_TOD_COUNT) {
        s_vmaps[vslot].johto_wild_grass.rate[tod] = (uint8_t)rate;
    }
    return 1;
}

int AmberScript_MapSetJohtoWildSlot(const char *name, int is_water, int slot_no,
                                    const char *species_name_or_id, int level,
                                    int tod) {
    int vslot, species, max_slot;
    if (!name || !*name) return 0;
    max_slot = is_water ? PKS_JOHTO_WATER_SLOTS : PKS_JOHTO_GRASS_SLOTS;
    if (slot_no < 1 || slot_no > max_slot) {
        printf("[amberscript] wild_%s_slot '%s': slot %d out of range (want 1-%d)\n",
               is_water ? "water" : "grass", name, slot_no, max_slot);
        return 0;
    }
    species = pks_resolve_species_id(species_name_or_id);
    if (species <= 0) {
        printf("[amberscript] wild_%s_slot '%s': unrecognized species '%s'\n",
               is_water ? "water" : "grass", name, species_name_or_id ? species_name_or_id : "(null)");
        return 0;
    }
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    if (is_water) {
        s_vmaps[vslot].johto_wild_water.slots[slot_no - 1].species = (uint8_t)species;
        s_vmaps[vslot].johto_wild_water.slots[slot_no - 1].level = (uint8_t)level;
    } else if (tod < 0) {
        for (int i = 0; i < PKS_JOHTO_TOD_COUNT; i++) {
            s_vmaps[vslot].johto_wild_grass.slots[i][slot_no - 1].species = (uint8_t)species;
            s_vmaps[vslot].johto_wild_grass.slots[i][slot_no - 1].level = (uint8_t)level;
        }
    } else if (tod < PKS_JOHTO_TOD_COUNT) {
        s_vmaps[vslot].johto_wild_grass.slots[tod][slot_no - 1].species = (uint8_t)species;
        s_vmaps[vslot].johto_wild_grass.slots[tod][slot_no - 1].level = (uint8_t)level;
    }
    return 1;
}

const wild_mons_t *AmberScript_GetWildMonsFor(uint8_t map_id, int is_water) {
    static wild_mons_t s_synth;
    int phys, owner;
    const pks_wild_table_t *src;
    if (!AmberScript_IsEnabled() || map_id < PKS_VIRTUAL_MAP_FIRST || map_id > PKS_VIRTUAL_MAP_LAST)
        return is_water ? &gWildWater[map_id] : &gWildGrass[map_id];
    pks_mb_init();
    phys = map_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    memset(&s_synth, 0, sizeof(s_synth));
    if (owner < 0) return &s_synth;
    src = is_water ? &s_vmaps[owner].wild_water : &s_vmaps[owner].wild_grass;
    s_synth.rate = src->rate;
    for (int i = 0; i < PKS_WILD_SLOTS; i++) {
        s_synth.slots[i].level = src->slots[i].level;
        s_synth.slots[i].species = src->slots[i].species;
    }
    return &s_synth;
}

static const uint8_t kJohtoGrassProbCum[PKS_JOHTO_GRASS_SLOTS] = { 30, 60, 80, 90, 95, 99, 100 };
static const uint8_t kJohtoWaterProbCum[PKS_JOHTO_WATER_SLOTS] = { 60, 90, 100 };

int AmberScript_HasJohtoWildTable(uint8_t map_id, int is_water) {
    int phys, owner;
    if (!AmberScript_IsEnabled() || map_id < PKS_VIRTUAL_MAP_FIRST || map_id > PKS_VIRTUAL_MAP_LAST) return 0;
    pks_mb_init();
    phys = map_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return 0;
    return is_water ? (s_vmaps[owner].johto_wild_water.rate != 0)
                     : (s_vmaps[owner].johto_wild_grass.rate[0] ||
                        s_vmaps[owner].johto_wild_grass.rate[1] ||
                        s_vmaps[owner].johto_wild_grass.rate[2]);
}

int AmberScript_TryJohtoWildEncounter(uint8_t map_id, int is_water,
                                      uint8_t *out_species, uint8_t *out_level) {
    int phys, owner, num_slots;
    uint8_t rate, roll;
    const uint8_t *prob_cum;
    const pks_wild_slot_t *slots;

    if (!AmberScript_IsEnabled() || map_id < PKS_VIRTUAL_MAP_FIRST || map_id > PKS_VIRTUAL_MAP_LAST) return 0;
    pks_mb_init();
    phys = map_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return 0;

    if (is_water) {
        rate = s_vmaps[owner].johto_wild_water.rate;
        num_slots = PKS_JOHTO_WATER_SLOTS;
        prob_cum = kJohtoWaterProbCum;
        slots = s_vmaps[owner].johto_wild_water.slots;
    } else {

        int tod = CrystalColor_TimeOfDay();
        if (tod < 0 || tod >= PKS_JOHTO_TOD_COUNT) tod = 1;
        rate = s_vmaps[owner].johto_wild_grass.rate[tod];
        num_slots = PKS_JOHTO_GRASS_SLOTS;
        prob_cum = kJohtoGrassProbCum;
        slots = s_vmaps[owner].johto_wild_grass.slots[tod];
    }
    if (!rate) return 0;
    if (BattleRandom() >= rate) return 0;

    do {
        roll = BattleRandom();
    } while (roll >= 100);
    roll++;

    int slot_idx = num_slots - 1;
    for (int i = 0; i < num_slots; i++) {
        if (prob_cum[i] >= roll) { slot_idx = i; break; }
    }

    uint8_t level = slots[slot_idx].level;
    if (!is_water) {

        uint8_t buff_roll = BattleRandom();
        if (buff_roll >= 35 * 255 / 100) {
            level++;
            if (buff_roll >= 65 * 255 / 100) {
                level++;
                if (buff_roll >= 85 * 255 / 100) {
                    level++;
                    if (buff_roll >= 95 * 255 / 100) level++;
                }
            }
        }
    }

    if (out_species) *out_species = slots[slot_idx].species;
    if (out_level) *out_level = level;
    return 1;
}

static int s_trainer_base_latch_valid = 0;
static int s_trainer_base_latch_value = 0;

static int s_trainer_slot_latch[PKS_VMAP_TRAINER_MAX];
static int s_trainer_slot_latch_valid = 0;

static int pks_item_provably_uncollected(uint8_t item_id) {
    int i;
    if (!item_id || !Inventory_IsKeyItem(item_id)) return 0;
    for (i = 0; i < (int)wNumBagItems && i < BAG_ITEM_CAPACITY; i++)
        if (wBagItems[i * 2] == item_id) return 0;
    for (i = 0; i < (int)wNumBoxItems && i < PC_ITEM_CAPACITY; i++)
        if (wBoxItems[i * 2] == item_id) return 0;
    return 1;
}

static const map_events_t *pks_mb_build_events_for_owner(int owner, int fresh_load) {
    static npc_event_t s_npc_buf[PKS_VMAP_NPC_MAX + PKS_VMAP_TRAINER_MAX];
    static map_trainer_t s_trainer_buf[PKS_VMAP_TRAINER_MAX];
    static item_event_t s_item_buf[PKS_VMAP_ITEM_MAX];
    static hidden_event_t s_hidden_buf[PKS_VMAP_HIDDEN_EVENT_MAX];
    static map_events_t s_ev;
    int n;

    memset(&s_ev, 0, sizeof(s_ev));
    if (owner < 0) return &s_ev;

    n = 0;
    for (int i = 0; i < s_vmaps[owner].num_npcs && n < (int)(sizeof(s_npc_buf)/sizeof(s_npc_buf[0])); i++) {
        pks_npc_t *src = &s_vmaps[owner].npcs[i];
        if (!src->used) continue;

        if (src->hide_if_event && CheckEvent(src->hide_if_event)) continue;
        if (src->hide_if_event2 && CheckEvent(src->hide_if_event2)) continue;

        if (src->show_if_event && !CheckEvent(src->show_if_event)) continue;

        const pks_npc_rt_t *rt = pks_npc_rt_find(owner, src->x, src->y);

        if (rt && rt->hidden) continue;
        if (rt && rt->has_pos) {
            s_npc_buf[n].x = rt->x;
            s_npc_buf[n].y = rt->y;
            s_npc_buf[n].facing = rt->facing;
        } else {
            s_npc_buf[n].x = src->x;
            s_npc_buf[n].y = src->y;
            s_npc_buf[n].facing = src->facing;
        }
        s_npc_buf[n].sprite_id = src->sprite_id;
        s_npc_buf[n].movement = src->movement;
        s_npc_buf[n].text = src->text[0] ? src->text : NULL;
        s_npc_buf[n].script = src->script;
        s_npc_buf[n].src_idx = (uint8_t)i;
        s_npc_buf[n].crystal_pal = src->crystal_pal;
        s_npc_buf[n].starts_hidden = src->starts_hidden;
        n++;
    }

    int trainer_base_idx;
    if (fresh_load || !s_trainer_base_latch_valid) {
        trainer_base_idx = n;
        s_trainer_base_latch_value = trainer_base_idx;
        s_trainer_base_latch_valid = 1;
    } else {
        trainer_base_idx = s_trainer_base_latch_value;
    }

    if (fresh_load || !s_trainer_slot_latch_valid) {
        for (int i = 0; i < PKS_VMAP_TRAINER_MAX; i++) s_trainer_slot_latch[i] = -1;
        s_trainer_slot_latch_valid = 1;
        int slot = trainer_base_idx;
        for (int i = 0; i < s_vmaps[owner].num_trainers; i++) {
            pks_trainer_t *src = &s_vmaps[owner].trainers[i];
            if (!src->used) continue;
            if (src->hide_if_event && CheckEvent(src->hide_if_event)) continue;
            if (src->hide_if_event2 && CheckEvent(src->hide_if_event2)) continue;
            if (src->show_if_event && !CheckEvent(src->show_if_event)) continue;
            if (i < PKS_VMAP_TRAINER_MAX) s_trainer_slot_latch[i] = slot;
            slot++;
        }
    }
    for (int i = 0; i < s_vmaps[owner].num_trainers && n < (int)(sizeof(s_npc_buf)/sizeof(s_npc_buf[0])); i++) {
        pks_trainer_t *src = &s_vmaps[owner].trainers[i];
        if (!src->used) continue;

        if (src->hide_if_event && CheckEvent(src->hide_if_event)) continue;
        if (src->hide_if_event2 && CheckEvent(src->hide_if_event2)) continue;
        if (src->show_if_event && !CheckEvent(src->show_if_event)) continue;
        s_npc_buf[n].x = src->x;
        s_npc_buf[n].y = src->y;
        s_npc_buf[n].sprite_id = src->sprite_id;
        s_npc_buf[n].movement = 0;
        s_npc_buf[n].facing = 0;
        s_npc_buf[n].text = NULL;
        s_npc_buf[n].script = NULL;

        s_npc_buf[n].starts_hidden = 0;

        s_npc_buf[n].crystal_pal = src->crystal_pal;

        s_npc_buf[n].src_idx = (uint8_t)(PKS_TRAINER_DECL_IDX_BASE + i);
        n++;
    }

    for (int i = 0; i < s_vmaps[owner].num_static_encounters &&
                    n < (int)(sizeof(s_npc_buf)/sizeof(s_npc_buf[0])); i++) {
        pks_static_t *src = &s_vmaps[owner].static_encounters[i];
        if (!src->used) continue;
        if (src->flag_bit && CheckEvent(src->flag_bit)) continue;
        s_npc_buf[n].x = (uint16_t)src->x;
        s_npc_buf[n].y = (uint16_t)src->y;
        s_npc_buf[n].sprite_id = src->sprite_id;
        s_npc_buf[n].movement = 0;
        s_npc_buf[n].text = NULL;
        s_npc_buf[n].script = AmberScript_StaticEncounterInteract;

        s_npc_buf[n].starts_hidden = 0;
        s_npc_buf[n].facing = src->facing;
        s_npc_buf[n].src_idx = (uint8_t)(PKS_STATIC_DECL_IDX_BASE + i);
        n++;
    }

    s_ev.npcs = s_npc_buf;
    s_ev.num_npcs = (uint8_t)n;

    int nt = 0;
    for (int i = 0; i < s_vmaps[owner].num_trainers && nt < PKS_VMAP_TRAINER_MAX; i++) {
        pks_trainer_t *src = &s_vmaps[owner].trainers[i];
        if (!src->used) continue;

        if (src->hide_if_event && CheckEvent(src->hide_if_event)) continue;
        if (src->hide_if_event2 && CheckEvent(src->hide_if_event2)) continue;
        if (src->show_if_event && !CheckEvent(src->show_if_event)) continue;

        if (i >= PKS_VMAP_TRAINER_MAX || s_trainer_slot_latch[i] < 0) continue;
        s_trainer_buf[nt].npc_idx = (uint8_t)s_trainer_slot_latch[i];
        s_trainer_buf[nt].facing = src->facing;
        s_trainer_buf[nt].trainer_class = src->trainer_class;
        s_trainer_buf[nt].trainer_no = src->trainer_no;
        s_trainer_buf[nt].sight_dist = src->sight_dist;
        s_trainer_buf[nt].flag_bit = src->flag_bit;
        s_trainer_buf[nt].before_text = src->before_text[0] ? src->before_text : NULL;
        s_trainer_buf[nt].after_text = src->after_text[0] ? src->after_text : NULL;
        s_trainer_buf[nt].end_text = NULL;
        s_trainer_buf[nt].defeat_text = src->defeat_text[0] ? src->defeat_text : NULL;
        s_trainer_buf[nt].after_battle_scene =
            src->after_battle_scene[0] ? src->after_battle_scene : NULL;
        s_trainer_buf[nt].johto_party = src->johto_party;
        nt++;
    }
    s_ev.trainers = s_trainer_buf;
    s_ev.num_trainers = (uint8_t)nt;

    int ni = 0;
    for (int i = 0; i < s_vmaps[owner].num_items && ni < PKS_VMAP_ITEM_MAX; i++) {
        pks_item_t *src = &s_vmaps[owner].items[i];
        if (!src->used) continue;

        if (src->hide_if_event && CheckEvent(src->hide_if_event)) continue;
        if (src->hide_if_event2 && CheckEvent(src->hide_if_event2)) continue;
        if (src->show_if_event && !CheckEvent(src->show_if_event)) {

            if (src->flag_bit) ClearEvent(src->flag_bit);
            continue;
        }

        if (src->show_if_event && src->flag_bit && CheckEvent(src->flag_bit) &&
            pks_item_provably_uncollected(src->item_id)) {
            printf("[amberscript] item_ball %s (%d,%d): taken-bit %u set but key "
                   "item %u is in neither bag nor PC -- impossible, clearing "
                   "(stale flag from the window where this show_if ball was "
                   "emitted ungated)\n",
                   s_vmaps[owner].name, (int)src->x, (int)src->y,
                   (unsigned)src->flag_bit, (unsigned)src->item_id);
            fflush(stdout);
            ClearEvent(src->flag_bit);
        }
        s_item_buf[ni].x = (uint16_t)src->x;
        s_item_buf[ni].y = (uint16_t)src->y;
        s_item_buf[ni].item_id = src->item_id;
        s_item_buf[ni].src_idx = (uint8_t)i;
        ni++;
    }
    s_ev.items = s_item_buf;
    s_ev.num_items = (uint8_t)ni;

    int nh = 0;
    for (int i = 0; i < s_vmaps[owner].num_hidden_events && nh < PKS_VMAP_HIDDEN_EVENT_MAX; i++) {
        pks_hidden_event_t *src = &s_vmaps[owner].hidden_events[i];
        if (!src->used) continue;
        s_hidden_buf[nh].x = src->x;
        s_hidden_buf[nh].y = src->y;
        s_hidden_buf[nh].text = src->text[0] ? src->text : NULL;
        s_hidden_buf[nh].script = src->script;
        s_hidden_buf[nh].facing = src->facing;
        nh++;
    }
    s_ev.hidden_events = s_hidden_buf;
    s_ev.num_hidden_events = (uint8_t)nh;

    return &s_ev;
}

static int pks_mb_owner_for_real_id(uint8_t real_id) {
    int phys;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return -1;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    return s_slot_owner[phys];
}

const map_events_t *AmberScript_GetMapEventsForCurrentMap(uint8_t real_id) {
    return pks_mb_build_events_for_owner(pks_mb_owner_for_real_id(real_id), 0);
}

const map_events_t *AmberScript_GetMapEventsForFreshLoad(uint8_t real_id) {
    if (!AmberScript_IsEnabled() || real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST)
        return &gMapEvents[real_id];
    return pks_mb_build_events_for_owner(pks_mb_owner_for_real_id(real_id), 1);
}

const map_events_t *AmberScript_GetMapEventsFor(uint8_t map_id) {
    if (AmberScript_IsEnabled() && map_id >= PKS_VIRTUAL_MAP_FIRST && map_id <= PKS_VIRTUAL_MAP_LAST)
        return AmberScript_GetMapEventsForCurrentMap(map_id);
    return &gMapEvents[map_id];
}

uint16_t AmberScript_GetItemFlagBitAt(uint8_t real_id, int item_index) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return 0;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0 || item_index < 0 || item_index >= (int)s_vmaps[owner].num_items) return 0;
    return s_vmaps[owner].items[item_index].flag_bit;
}

int AmberScript_IsTrainerBeatenAt(uint8_t real_id, int x, int y) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return 0;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return 0;
    for (int i = 0; i < s_vmaps[owner].num_trainers; i++) {
        if (s_vmaps[owner].trainers[i].used &&
            s_vmaps[owner].trainers[i].x == (int16_t)x &&
            s_vmaps[owner].trainers[i].y == (int16_t)y) {
            return CheckEvent(s_vmaps[owner].trainers[i].flag_bit) ? 1 : 0;
        }
    }
    return 0;
}

void AmberScript_MarkAllTrainersDefeated(const char *map_name) {
    int vslot;
    pks_mb_init();
    vslot = pks_vmap_find(map_name);
    if (vslot < 0) return;
    for (int i = 0; i < (int)s_vmaps[vslot].num_trainers; i++) {
        if (s_vmaps[vslot].trainers[i].used)
            SetEvent(s_vmaps[vslot].trainers[i].flag_bit);
    }
}

int AmberScript_MapSetBorder(const char *name, const char *tl, const char *tr,
                             const char *bl, const char *br) {
    int vslot;
    if (!name || !*name || !tl || !*tl || !tr || !*tr || !bl || !*bl || !br || !*br) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    snprintf(s_vmaps[vslot].border_block_name[0], sizeof(s_vmaps[vslot].border_block_name[0]), "%s", tl);
    snprintf(s_vmaps[vslot].border_block_name[1], sizeof(s_vmaps[vslot].border_block_name[1]), "%s", tr);
    snprintf(s_vmaps[vslot].border_block_name[2], sizeof(s_vmaps[vslot].border_block_name[2]), "%s", bl);
    snprintf(s_vmaps[vslot].border_block_name[3], sizeof(s_vmaps[vslot].border_block_name[3]), "%s", br);
    return 1;
}

int AmberScript_MapSetBorderSide(const char *name, int side, const char *tl, const char *tr,
                                 const char *bl, const char *br) {
    int vslot;
    if (!name || !*name || side < 0 || side > 3) return 0;
    if (!tl || !*tl || !tr || !*tr || !bl || !*bl || !br || !*br) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    snprintf(s_vmaps[vslot].border_side_name[side][0], 32, "%s", tl);
    snprintf(s_vmaps[vslot].border_side_name[side][1], 32, "%s", tr);
    snprintf(s_vmaps[vslot].border_side_name[side][2], 32, "%s", bl);
    snprintf(s_vmaps[vslot].border_side_name[side][3], 32, "%s", br);
    return 1;
}

int AmberScript_MapBank_GetBorderTileForRealId(int real_id, int tx, int ty,
                                               uint8_t *tile_id, uint8_t *passable_out) {
    int phys, owner, quadrant, sub;
    uint8_t tiles[4], passable;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return 0;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0 || !s_vmaps[owner].border_block_name[0][0]) return 0;

    quadrant = (((ty >> 1) & 1) ? 2 : 0) + (((tx >> 1) & 1) ? 1 : 0);
    {

        const char *name = 0;
        int w = s_vmaps[owner].width_blocks * 4;
        int h = s_vmaps[owner].height_blocks * 4;
        int side = -1;
        if (h > 0 && ty < 0)       side = 0;
        else if (h > 0 && ty >= h) side = 1;
        else if (w > 0 && tx < 0)  side = 2;
        else if (w > 0 && tx >= w) side = 3;
        if (side >= 0 && s_vmaps[owner].border_side_name[side][quadrant][0])
            name = s_vmaps[owner].border_side_name[side][quadrant];
        if (!name) name = s_vmaps[owner].border_block_name[quadrant];
        if (!AmberScript_ResolveNamedBlock(name, tiles, &passable)) return 0;
    }
    sub = ((ty & 1) ? 2 : 0) + ((tx & 1) ? 1 : 0);
    if (tile_id) *tile_id = tiles[sub];
    if (passable_out) *passable_out = passable;
    return 1;
}

int AmberScript_MapSetWarpSpot(const char *name, int spot_idx, int x, int y) {
    int vslot;
    if (!name || !*name || spot_idx < 0 || spot_idx >= PKS_VMAP_WARP_SPOTS_MAX) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    s_vmaps[vslot].warp_spot_x[spot_idx] = (int16_t)x;
    s_vmaps[vslot].warp_spot_y[spot_idx] = (int16_t)y;
    s_vmaps[vslot].warp_spot_set[spot_idx] = 1;
    return 1;
}

int AmberScript_MapBank_GetWarpSpotForName(const char *name, int spot_idx, int *x, int *y) {
    int vslot;
    pks_mb_init();
    if (!name || !*name) return 0;
    if (spot_idx < 0 || spot_idx >= PKS_VMAP_WARP_SPOTS_MAX) return 0;
    vslot = pks_vmap_find(name);
    if (vslot < 0 || !s_vmaps[vslot].warp_spot_set[spot_idx]) return 0;
    if (x) *x = s_vmaps[vslot].warp_spot_x[spot_idx];
    if (y) *y = s_vmaps[vslot].warp_spot_y[spot_idx];
    return 1;
}

int AmberScript_MapBank_GetWarpSpotForRealId(int real_id, int spot_idx, int *x, int *y) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return 0;
    if (spot_idx < 0 || spot_idx >= PKS_VMAP_WARP_SPOTS_MAX) return 0;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0 || !s_vmaps[owner].warp_spot_set[spot_idx]) return 0;
    if (x) *x = s_vmaps[owner].warp_spot_x[spot_idx];
    if (y) *y = s_vmaps[owner].warp_spot_y[spot_idx];
    return 1;
}

int AmberScript_MapBank_RegisterName(const char *name) {
    pks_mb_init();
    if (!name || !*name) return 0;
    return pks_vmap_register(name) >= 0;
}

int AmberScript_MapSetIndoor(const char *name) {
    int vslot;
    if (!name || !*name) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    s_vmaps[vslot].is_indoor = 1;
    return 1;
}

int AmberScript_MapSetDark(const char *name) {
    int vslot;
    if (!name || !*name) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    s_vmaps[vslot].is_dark = 1;
    return 1;
}

int AmberScript_MapSetNoDoorStep(const char *name) {
    int vslot;
    if (!name || !*name) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    s_vmaps[vslot].no_door_step = 1;
    return 1;
}

int AmberScript_MapSetGbcTileset(const char *name, int tileset_id) {
    int vslot;
    if (!name || !*name) return 0;
    if ((unsigned)tileset_id >= NUM_TILESETS) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    s_vmaps[vslot].gbc_tileset = (uint8_t)tileset_id;
    s_vmaps[vslot].gbc_tileset_set = 1;
    return 1;
}

int AmberScript_MapSetCrystalEnv(const char *name, int environment, int map_group) {
    int vslot;
    if (!name || !*name) return 0;
    if (environment < 0 || environment > 255 || map_group < 0 || map_group > 255)
        return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    s_vmaps[vslot].crystal_env = (uint8_t)environment;
    s_vmaps[vslot].crystal_group = (uint8_t)map_group;
    s_vmaps[vslot].crystal_env_set = 1;
    return 1;
}

int AmberScript_MapSetCrystalAnim(const char *name, int tileset_id,
                                  const char *slug) {
    int vslot;
    if (!name || !*name || tileset_id < 0 || tileset_id > 255) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    s_vmaps[vslot].crystal_tileset = (uint8_t)tileset_id;
    s_vmaps[vslot].crystal_tileset_set = 1;
    snprintf(s_vmaps[vslot].crystal_slug, sizeof(s_vmaps[vslot].crystal_slug),
             "%s", slug ? slug : "");
    return 1;
}

int AmberScript_MapBank_GetCrystalAnimForRealId(int real_id, const char **out_slug) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return -1;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0 || !s_vmaps[owner].crystal_tileset_set) return -1;
    if (out_slug) *out_slug = s_vmaps[owner].crystal_slug;
    return s_vmaps[owner].crystal_tileset;
}

int AmberScript_MapBank_GetCrystalEnvForRealId(int real_id, int *out_group) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return -1;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0 || !s_vmaps[owner].crystal_env_set) return -1;
    if (out_group) *out_group = s_vmaps[owner].crystal_group;
    return s_vmaps[owner].crystal_env;
}

int AmberScript_MapBank_GetGbcTilesetForRealId(int real_id) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return -1;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0 || !s_vmaps[owner].gbc_tileset_set) return -1;
    return s_vmaps[owner].gbc_tileset;
}

int AmberScript_MapBank_GetGbcTilesetForName(const char *name) {
    int vslot;
    pks_mb_init();
    if (!name || !*name) return -1;
    vslot = pks_vmap_find(name);
    if (vslot < 0 || !s_vmaps[vslot].gbc_tileset_set) return -1;
    return s_vmaps[vslot].gbc_tileset;
}

int AmberScript_MapBank_GetNoDoorStepForRealId(int real_id) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return 0;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return 0;
    return s_vmaps[owner].no_door_step;
}

int AmberScript_MapSetWarpWalkInto(const char *name, int x, int y, int dir) {
    int vslot;
    if (!name || !*name) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    for (int i = 0; i < s_vmaps[vslot].num_warp_walk_into; i++)
        if (s_vmaps[vslot].warp_walk_into_x[i] == (int16_t)x &&
            s_vmaps[vslot].warp_walk_into_y[i] == (int16_t)y) {
            s_vmaps[vslot].warp_walk_into_dir[i] = (uint8_t)dir;
            return 1;
        }
    if (s_vmaps[vslot].num_warp_walk_into >= PKS_VMAP_WARP_SPOTS_MAX) return 0;
    {
        int n = s_vmaps[vslot].num_warp_walk_into++;
        s_vmaps[vslot].warp_walk_into_x[n] = (int16_t)x;
        s_vmaps[vslot].warp_walk_into_y[n] = (int16_t)y;
        s_vmaps[vslot].warp_walk_into_dir[n] = (uint8_t)dir;
    }
    return 1;
}

int AmberScript_MapBank_GetWarpWalkIntoDirAt(int real_id, int x, int y) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return 0;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return 0;
    for (int i = 0; i < s_vmaps[owner].num_warp_walk_into; i++)
        if (s_vmaps[owner].warp_walk_into_x[i] == (int16_t)x &&
            s_vmaps[owner].warp_walk_into_y[i] == (int16_t)y)
            return s_vmaps[owner].warp_walk_into_dir[i];
    return 0;
}

int AmberScript_MapBank_IsWarpWalkIntoAt(int real_id, int x, int y) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return 0;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return 0;
    for (int i = 0; i < s_vmaps[owner].num_warp_walk_into; i++)
        if (s_vmaps[owner].warp_walk_into_x[i] == (int16_t)x &&
            s_vmaps[owner].warp_walk_into_y[i] == (int16_t)y) return 1;
    return 0;
}

int AmberScript_MapSetWarpStair(const char *name, int x, int y) {
    int vslot;
    if (!name || !*name) return 0;
    pks_mb_init();
    vslot = pks_vmap_register(name);
    if (vslot < 0) return 0;
    for (int i = 0; i < s_vmaps[vslot].num_warp_stair; i++)
        if (s_vmaps[vslot].warp_stair_x[i] == (int16_t)x &&
            s_vmaps[vslot].warp_stair_y[i] == (int16_t)y) return 1;
    if (s_vmaps[vslot].num_warp_stair >= PKS_VMAP_WARP_SPOTS_MAX) return 0;
    {
        int n = s_vmaps[vslot].num_warp_stair++;
        s_vmaps[vslot].warp_stair_x[n] = (int16_t)x;
        s_vmaps[vslot].warp_stair_y[n] = (int16_t)y;
    }
    return 1;
}

int AmberScript_MapBank_IsWarpStairAt(int real_id, int x, int y) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return 0;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return 0;
    for (int i = 0; i < s_vmaps[owner].num_warp_stair; i++)
        if (s_vmaps[owner].warp_stair_x[i] == (int16_t)x &&
            s_vmaps[owner].warp_stair_y[i] == (int16_t)y) return 1;
    return 0;
}

int AmberScript_MapBank_IsIndoorForRealId(int real_id) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return 0;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return 0;
    return s_vmaps[owner].is_indoor;
}

int AmberScript_MapBank_IsDarkForRealId(int real_id) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return 0;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return 0;
    return s_vmaps[owner].is_dark;
}

static int pks_mb_assign_slot(int vslot) {
    int real_id;
    if (s_vmaps[vslot].bound_real_id >= 0) {

        int phys = s_vmaps[vslot].bound_real_id - PKS_VIRTUAL_MAP_FIRST;
        if (phys >= 0 && phys < PKS_VIRTUAL_MAP_COUNT && s_slot_owner[phys] == vslot) {
            s_vmaps[vslot].lru = ++s_vmap_lru_clock;
            return s_vmaps[vslot].bound_real_id;
        }
        printf("[amberscript] map_bank: WARNING stale binding '%s' claimed real id %d "
               "but slot is owned by %s -- dropping and re-picking\n",
               s_vmaps[vslot].name, s_vmaps[vslot].bound_real_id,
               (phys >= 0 && phys < PKS_VIRTUAL_MAP_COUNT && s_slot_owner[phys] >= 0)
                   ? s_vmaps[s_slot_owner[phys]].name : "(none)");
        s_vmaps[vslot].bound_real_id = -1;
        s_vmaps[vslot].has_streamed  = 0;
    }
    {
        int phys_slot = pks_mb_pick_slot();
        int old_owner = s_slot_owner[phys_slot];
        real_id = PKS_VIRTUAL_MAP_FIRST + phys_slot;

        if (old_owner >= 0) {
            s_vmaps[old_owner].bound_real_id = -1;
            s_vmaps[old_owner].has_streamed = 0;
            AmberScript_TilesetUnbindMap((uint8_t)real_id);
            AmberScript_ClearTileOverrides((uint8_t)real_id);

            AmberScript_Scene_ClearNpcBindingsForMap((uint8_t)real_id);
            DebugCLI_ClearSceneNpcBindingsForMap((uint8_t)real_id);

            AmberScript_Scene_ClearTriggersForMap((uint8_t)real_id);
            printf("[amberscript] map_bank: evicting '%s' from real id %d for '%s'\n",
                   s_vmaps[old_owner].name, real_id, s_vmaps[vslot].name);
            Trace_Emit(TRACE_NPC, "\"ev\":\"evict\",\"map\":%d,\"old\":\"%s\",\"new\":\"%s\"",
                       real_id, s_vmaps[old_owner].name, s_vmaps[vslot].name);
        }
        s_slot_owner[phys_slot] = vslot;
        s_vmaps[vslot].bound_real_id = real_id;
        s_vmaps[vslot].has_streamed = 0;
    }
    s_vmaps[vslot].lru = ++s_vmap_lru_clock;
    return real_id;
}

static int pks_mb_ensure_resident(int vslot) {
    int real_id = pks_mb_assign_slot(vslot);
    if (!s_vmaps[vslot].has_streamed) {
        uint8_t saved_map = wCurMap;

        Map_SuppressScrollRebuild(1);
        wCurMap = (uint8_t)real_id;
        pks_mb_stream_in(vslot);
        wCurMap = saved_map;

        Map_RefreshVirtualDims();
        Map_SuppressScrollRebuild(0);
        s_vmaps[vslot].has_streamed = 1;
    }
    return real_id;
}

int AmberScript_MapBank_EnsureResidentByName(const char *name) {
    int vslot;
    pks_mb_init();
    if (!name || !*name) return -1;
    vslot = pks_vmap_register(name);
    if (vslot < 0) return -1;
    return pks_mb_ensure_resident(vslot);
}

int AmberScript_MapBank_GetOrAssignRealId(const char *name) {
    int vslot;
    pks_mb_init();
    if (!name || !*name) return -1;
    vslot = pks_vmap_register(name);
    if (vslot < 0) return -1;
    return pks_mb_assign_slot(vslot);
}

void AmberScript_MapBank_TouchRealId(uint8_t real_id) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return;
    s_vmaps[owner].lru = ++s_vmap_lru_clock;
}

void AmberScript_MapBank_SnapshotBindings(char out[][PKS_VMAP_BIND_NAME_LEN], int count) {
    pks_mb_init();
    for (int i = 0; i < count && i < PKS_VIRTUAL_MAP_COUNT; i++) {
        out[i][0] = '\0';
        if (s_slot_owner[i] >= 0)
            snprintf(out[i], PKS_VMAP_BIND_NAME_LEN, "%s",
                     s_vmaps[s_slot_owner[i]].name);
    }
}

int AmberScript_MapBank_SerializeNpcRt(uint8_t *buf, int cap) {
    pks_mb_init();
    if (!buf || cap < 2) return 0;
    int n = 2;
    uint16_t nvmaps = 0;
    int dropped = 0;
    for (int v = 0; v < PKS_VMAP_MAX; v++) {
        if (!s_vmaps[v].used || s_vmaps[v].num_npc_rt == 0) continue;
        int active = 0;
        for (int i = 0; i < s_vmaps[v].num_npc_rt; i++)
            if (s_vmaps[v].npc_rt[i].used) active++;
        if (active == 0) continue;
        int need = PKS_VMAP_BIND_NAME_LEN + 1 + active * 11;
        if (n + need > cap) { dropped++; continue; }
        memcpy(buf + n, s_vmaps[v].name, PKS_VMAP_BIND_NAME_LEN);
        n += PKS_VMAP_BIND_NAME_LEN;
        buf[n++] = (uint8_t)active;
        for (int i = 0; i < s_vmaps[v].num_npc_rt; i++) {
            const pks_npc_rt_t *r = &s_vmaps[v].npc_rt[i];
            if (!r->used) continue;
            buf[n++] = (uint8_t)(r->key_x & 0xFF); buf[n++] = (uint8_t)((r->key_x >> 8) & 0xFF);
            buf[n++] = (uint8_t)(r->key_y & 0xFF); buf[n++] = (uint8_t)((r->key_y >> 8) & 0xFF);
            buf[n++] = (uint8_t)(r->x & 0xFF);     buf[n++] = (uint8_t)((r->x >> 8) & 0xFF);
            buf[n++] = (uint8_t)(r->y & 0xFF);     buf[n++] = (uint8_t)((r->y >> 8) & 0xFF);
            buf[n++] = r->facing;
            buf[n++] = r->hidden;
            buf[n++] = r->has_pos;
        }
        nvmaps++;
    }
    buf[0] = (uint8_t)(nvmaps & 0xFF);
    buf[1] = (uint8_t)((nvmaps >> 8) & 0xFF);
    if (dropped)
        printf("[amberscript] npc_rt serialize: %d byte buffer full, %d vmap(s) dropped\n", cap, dropped);
    return n;
}

void AmberScript_MapBank_DeserializeNpcRt(const uint8_t *buf, int len) {
    pks_mb_init();
    for (int v = 0; v < PKS_VMAP_MAX; v++)
        if (s_vmaps[v].used) s_vmaps[v].num_npc_rt = 0;
    if (!buf || len < 2) return;
    int n = 2;
    uint16_t nvmaps = (uint16_t)(buf[0] | (buf[1] << 8));
    for (uint16_t vi = 0; vi < nvmaps; vi++) {
        if (n + PKS_VMAP_BIND_NAME_LEN + 1 > len) break;
        char name[PKS_VMAP_BIND_NAME_LEN + 1];
        memcpy(name, buf + n, PKS_VMAP_BIND_NAME_LEN);
        name[PKS_VMAP_BIND_NAME_LEN] = '\0';
        n += PKS_VMAP_BIND_NAME_LEN;
        uint8_t cnt = buf[n++];
        int vslot = pks_vmap_register(name);
        for (int i = 0; i < cnt; i++) {
            if (n + 11 > len) return;
            int16_t kx = (int16_t)(buf[n] | (buf[n + 1] << 8)); n += 2;
            int16_t ky = (int16_t)(buf[n] | (buf[n + 1] << 8)); n += 2;
            int16_t x  = (int16_t)(buf[n] | (buf[n + 1] << 8)); n += 2;
            int16_t y  = (int16_t)(buf[n] | (buf[n + 1] << 8)); n += 2;
            uint8_t facing = buf[n++], hidden = buf[n++], has_pos = buf[n++];
            if (vslot < 0) continue;
            pks_npc_rt_t *r = pks_npc_rt_get_or_add(vslot, kx, ky);
            if (r) { r->x = x; r->y = y; r->facing = facing; r->hidden = hidden; r->has_pos = has_pos; }
        }
    }
}

static void pks_mb_release_slot(int phys) {
    int cur = s_slot_owner[phys];
    uint8_t real_id = (uint8_t)(PKS_VIRTUAL_MAP_FIRST + phys);
    if (cur < 0) return;
    s_vmaps[cur].bound_real_id = -1;
    s_vmaps[cur].has_streamed = 0;
    AmberScript_TilesetUnbindMap(real_id);
    AmberScript_ClearTileOverrides(real_id);
    s_slot_owner[phys] = -1;
}

void AmberScript_MapBank_RestoreBindings(const char in[][PKS_VMAP_BIND_NAME_LEN], int count) {
    pks_mb_init();
    for (int i = 0; i < count && i < PKS_VIRTUAL_MAP_COUNT; i++) {
        int vslot, cur;

        if (PKS_VMAP_SLOT_RESERVED(i)) { pks_mb_release_slot(i); continue; }
        cur = s_slot_owner[i];
        if (in[i][0] == '\0') {
            pks_mb_release_slot(i);
            continue;
        }
        if (cur >= 0 && strcasecmp(s_vmaps[cur].name, in[i]) == 0)
            continue;
        pks_mb_release_slot(i);
        vslot = pks_vmap_register(in[i]);
        if (vslot < 0) continue;

        if (s_vmaps[vslot].bound_real_id >= 0) {
            int old_phys = s_vmaps[vslot].bound_real_id - PKS_VIRTUAL_MAP_FIRST;
            if (old_phys >= 0 && old_phys < PKS_VIRTUAL_MAP_COUNT &&
                s_slot_owner[old_phys] == vslot)
                pks_mb_release_slot(old_phys);
        }
        s_slot_owner[i] = vslot;
        s_vmaps[vslot].bound_real_id = PKS_VIRTUAL_MAP_FIRST + i;
        s_vmaps[vslot].has_streamed = 0;
        s_vmaps[vslot].lru = ++s_vmap_lru_clock;
        printf("[amberscript] map_bank: state restore bound '%s' -> real id %d\n",
               in[i], PKS_VIRTUAL_MAP_FIRST + i);
    }
}

int AmberScript_MapBank_EnsureResidentForRealId(uint8_t real_id) {
    int phys, owner;
    pks_mb_init();
    if (real_id < PKS_VIRTUAL_MAP_FIRST || real_id > PKS_VIRTUAL_MAP_LAST) return 0;
    phys = real_id - PKS_VIRTUAL_MAP_FIRST;
    owner = s_slot_owner[phys];
    if (owner < 0) return 0;
    if (!s_vmaps[owner].has_streamed) {
        Map_SuppressScrollRebuild(1);
        pks_mb_stream_in(owner);
        Map_SuppressScrollRebuild(0);
        s_vmaps[owner].has_streamed = 1;
    }
    return 1;
}

int AmberScript_MapWarp(const char *name, int x, int y) {
    int vslot, real_id;

    int prev_was_dark;
    uint8_t prev_pal_offset;

    if (!name || !*name) return 0;
    pks_mb_init();
    prev_was_dark   = Map_IsDarkMap((int)wCurMap);
    prev_pal_offset = gMapPalOffset;
    vslot = pks_vmap_register(name);
    if (vslot < 0) {
        printf("[amberscript] map_warp: virtual map registry full (max %d)\n", PKS_VMAP_MAX);
        return 0;
    }

    if (!s_vmaps[vslot].has_streamed) {
        char probe_spec[300], probe_resolved[300];
        snprintf(probe_spec, sizeof(probe_spec), "mod_runtime/blocks/%s.block", name);
        if (!pks_mb_resolve(probe_spec, probe_resolved, sizeof(probe_resolved))) {
            printf("[amberscript] WARNING: map_warp '%s' has no matching .block file -- "
                   "this will get a blank canvas and permanently occupy one of only 8 real "
                   "map slots for the rest of this process. If this was a typo, restart to "
                   "release the wasted slot (there is currently no way to un-register a name "
                   "mid-session).\n", name);
        }
    }

    real_id = pks_mb_assign_slot(vslot);

    {
        extern void PalletScripts_OnMapLoad(void);
        extern void OaksLabScripts_OnMapLoad(void);
        wCurMap = (uint8_t)real_id;
        wXCoord = (uint8_t)x;
        wYCoord = (uint8_t)y;
        gMapPalOffset = 0;
        Map_Load(wCurMap);
        NPC_Load();
        PalletScripts_OnMapLoad();
        OaksLabScripts_OnMapLoad();
        Display_LoadMapPalette();
    }

    pks_mb_stream_in(vslot);
    s_vmaps[vslot].has_streamed = 1;

    Map_RefreshVirtualDims();

    Map_ApplyDarknessForWarp(prev_was_dark, prev_pal_offset, real_id);
    Display_LoadMapPalette();

    NPC_Load();

    Trainer_LoadMap();

    AmberScript_TileMod_PrewarmNeighbors();

    printf("[amberscript] map_warp: '%s' -> real id %d (%d,%d)\n", name, real_id, x, y);
    return 1;
}

#define PKS_CONN_MAX 1024
typedef struct pks_conn_t {
    int used;
    char from_name[32]; int from_real_id;
    int direction;
    char to_name[32];   int to_real_id;
    int16_t player_coord, adjust;
} pks_conn_t;
static pks_conn_t s_conns[PKS_CONN_MAX];

static int pks_conn_side_matches(const char *side_name, int side_real_id,
                                  const char *query_name, uint8_t query_real_id) {
    if (side_real_id >= 0) return side_real_id == query_real_id;
    return side_name[0] && query_name && strcasecmp(side_name, query_name) == 0;
}

int AmberScript_ConnectionDefine(const char *from, int direction, const char *to,
                                 int player_coord, int adjust) {
    int slot = -1, free_slot = -1;
    int from_real_id = -1;
    char from_name[32];

    if (!from || !*from || !to || !*to || direction < 0 || direction > 3) return 0;
    pks_mb_init();

    from_name[0] = '\0';
    if (from[0] >= '0' && from[0] <= '9') {
        from_real_id = atoi(from);
    } else {
        snprintf(from_name, sizeof(from_name), "%s", from);
    }

    for (int i = 0; i < PKS_CONN_MAX; i++) {
        if (!s_conns[i].used) {
            if (free_slot < 0) free_slot = i;
            continue;
        }
        if (s_conns[i].direction != direction) continue;
        if (from_real_id >= 0) {
            if (s_conns[i].from_real_id != from_real_id) continue;
        } else {
            if (s_conns[i].from_real_id >= 0) continue;
            if (strcasecmp(s_conns[i].from_name, from_name) != 0) continue;
        }
        slot = i;
        break;
    }
    if (slot < 0) slot = free_slot;
    if (slot < 0) {
        printf("[amberscript] map_connect: connection registry full (max %d)\n", PKS_CONN_MAX);
        return 0;
    }

    memset(&s_conns[slot], 0, sizeof(s_conns[slot]));
    s_conns[slot].used = 1;
    s_conns[slot].direction = direction;

    if (from_real_id >= 0) {
        s_conns[slot].from_real_id = from_real_id;
    } else {
        s_conns[slot].from_real_id = -1;
        snprintf(s_conns[slot].from_name, sizeof(s_conns[slot].from_name), "%s", from);
        pks_vmap_register(from);
    }
    if (to[0] >= '0' && to[0] <= '9') {
        s_conns[slot].to_real_id = atoi(to);
    } else {
        s_conns[slot].to_real_id = -1;
        snprintf(s_conns[slot].to_name, sizeof(s_conns[slot].to_name), "%s", to);
        pks_vmap_register(to);
    }
    s_conns[slot].player_coord = (int16_t)player_coord;
    s_conns[slot].adjust = (int16_t)adjust;
    return 1;
}

static void pks_log_connection_peek_miss(uint8_t cur_real_id, int direction, int to_vslot) {
    static struct { uint8_t cur; int dir; int to_vslot; } s_last = { 0xFF, -1, -1 };
    if (s_last.cur == cur_real_id && s_last.dir == direction && s_last.to_vslot == to_vslot) return;
    s_last.cur = cur_real_id; s_last.dir = direction; s_last.to_vslot = to_vslot;
    printf("[amberscript] CONNECTION PEEK MISS: map '%s' (real %d) dir=%d -> vmap '%s' not yet resident "
           "(bound_real_id=%d, has_streamed=%d) -- border art will show instead of real content until an actual crossing\n",
           AmberScript_MapBank_NameForRealId(cur_real_id) ? AmberScript_MapBank_NameForRealId(cur_real_id) : "?",
           (int)cur_real_id, direction, s_vmaps[to_vslot].name,
           s_vmaps[to_vslot].bound_real_id, s_vmaps[to_vslot].has_streamed);
}

static int pks_mb_resolve_connection(uint8_t cur_real_id, int direction, int force_stream,
                                      uint8_t *dest_real_id, int16_t *player_coord, int16_t *adjust) {
    const char *cur_name;
    if (!dest_real_id || !player_coord || !adjust) return 0;
    pks_mb_init();
    cur_name = AmberScript_MapBank_NameForRealId(cur_real_id);
    for (int i = 0; i < PKS_CONN_MAX; i++) {
        if (!s_conns[i].used || s_conns[i].direction != direction) continue;
        if (!pks_conn_side_matches(s_conns[i].from_name, s_conns[i].from_real_id, cur_name, cur_real_id)) continue;

        if (s_conns[i].to_real_id >= 0) {
            *dest_real_id = (uint8_t)s_conns[i].to_real_id;
        } else {
            int to_vslot = pks_vmap_register(s_conns[i].to_name);
            if (to_vslot < 0) continue;
            if (force_stream) {
                *dest_real_id = (uint8_t)pks_mb_ensure_resident(to_vslot);
            } else {
                if (s_vmaps[to_vslot].bound_real_id < 0) {
                    pks_log_connection_peek_miss(cur_real_id, direction, to_vslot);
                    continue;
                }
                *dest_real_id = (uint8_t)s_vmaps[to_vslot].bound_real_id;
            }
        }
        *player_coord = s_conns[i].player_coord;
        *adjust = s_conns[i].adjust;
        return 1;
    }
    return 0;
}

int AmberScript_GetConnectionOverride(uint8_t cur_real_id, int direction,
                                      uint8_t *dest_real_id, int16_t *player_coord, int16_t *adjust) {
    return pks_mb_resolve_connection(cur_real_id, direction, 1, dest_real_id, player_coord, adjust);
}

int AmberScript_GetConnectionOverridePassive(uint8_t cur_real_id, int direction,
                                             uint8_t *dest_real_id, int16_t *player_coord, int16_t *adjust) {
    return pks_mb_resolve_connection(cur_real_id, direction, 0, dest_real_id, player_coord, adjust);
}

int AmberScript_MapSave(const char *name) {
    char src[64], dst_spec[200];
    if (!name || !*name) return 0;
    pks_mb_init();
    if (pks_vmap_register(name) < 0) return 0;

    snprintf(src, sizeof(src), "mod_runtime/map_edits/map_%d.txt", (int)wCurMap);
    snprintf(dst_spec, sizeof(dst_spec), "mod_runtime/map_edits/vmap_%s.txt", name);
    return pks_mb_copy_file(src, dst_spec);
}

void AmberScript_MapList(void) {
    int reg = 0, resident = 0;
    for (int i = 0; i < PKS_VMAP_MAX; i++) {
        if (!s_vmaps[i].used) continue;
        reg++;
        if (s_vmaps[i].bound_real_id >= 0) {
            resident++;
            printf("    %-24s real id %d (resident)\n", s_vmaps[i].name, s_vmaps[i].bound_real_id);
        } else {
            printf("    %-24s (not streamed in)\n", s_vmaps[i].name);
        }
    }
    printf("[amberscript] map_list: %d registered, %d/%d real slot(s) resident\n",
           reg, resident, PKS_VIRTUAL_MAP_COUNT);
}

int AmberScript_MapBank_TryHandle(const char *cmd, const char *verb, int n) {
    (void)n;
    if (strcmp(verb, "map_warp") == 0) {
        char name[32] = {0}, xs[16] = {0}, ys[16] = {0};
        int x = 5, y = 5;
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name))) {
            printf("[amberscript] map_warp usage: map_warp <name> [x y]\n");
        } else {
            if (AmberScript_ParseArg(cmd, 2, xs, sizeof(xs))) x = atoi(xs);
            if (AmberScript_ParseArg(cmd, 3, ys, sizeof(ys))) y = atoi(ys);
            AmberScript_MapWarp(name, x, y);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "map_save") == 0) {
        char name[32] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name))) {
            printf("[amberscript] map_save usage: map_save <name>\n");
        } else if (AmberScript_MapSave(name)) {
            printf("[amberscript] map_save: '%s' <- map %d's current edits\n", name, (int)wCurMap);
        } else {
            printf("[amberscript] map_save: failed (no edits file for map %d, or registry full)\n", (int)wCurMap);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "map_list") == 0) {
        AmberScript_MapList();
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "map_release") == 0) {

        char name[32] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name))) {
            printf("[amberscript] map_release usage: map_release <name>\n");
        } else {
            int vslot = pks_vmap_find(name);
            if (vslot < 0) {
                printf("[amberscript] map_release: '%s' not registered\n", name);
            } else {
                if (s_vmaps[vslot].bound_real_id >= 0) {
                    int phys = s_vmaps[vslot].bound_real_id - PKS_VIRTUAL_MAP_FIRST;
                    if (phys >= 0 && phys < PKS_VIRTUAL_MAP_COUNT) s_slot_owner[phys] = -1;
                }
                memset(&s_vmaps[vslot], 0, sizeof(s_vmaps[vslot]));
                printf("[amberscript] map_release: '%s' released\n", name);
            }
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "map_connect") == 0) {
        char from[32] = {0}, dirs[16] = {0}, to[32] = {0}, pc[16] = {0}, adj[16] = {0};
        int direction = -1;
        if (!AmberScript_ParseArg(cmd, 1, from, sizeof(from)) ||
            !AmberScript_ParseArg(cmd, 2, dirs, sizeof(dirs)) ||
            !AmberScript_ParseArg(cmd, 3, to, sizeof(to)) ||
            !AmberScript_ParseArg(cmd, 4, pc, sizeof(pc)) ||
            !AmberScript_ParseArg(cmd, 5, adj, sizeof(adj))) {
            printf("[amberscript] map_connect usage: map_connect <from> <north|south|west|east> <to> <player_coord> <adjust>\n");
        } else {
            if (strcmp(dirs, "north") == 0) direction = 0;
            else if (strcmp(dirs, "south") == 0) direction = 1;
            else if (strcmp(dirs, "west") == 0) direction = 2;
            else if (strcmp(dirs, "east") == 0) direction = 3;
            if (direction < 0) {
                printf("[amberscript] map_connect: unknown direction '%s' (want north|south|west|east)\n", dirs);
            } else if (AmberScript_ConnectionDefine(from, direction, to, atoi(pc), atoi(adj))) {
                printf("[amberscript] map_connect: '%s' --%s--> '%s'\n", from, dirs, to);
            } else {
                printf("[amberscript] map_connect: failed\n");
            }
        }
        AmberScript_WriteState();
        return 1;
    }
    return 0;
}
