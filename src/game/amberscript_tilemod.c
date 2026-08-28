
#include "amberscript_tilemod.h"
#include "cycling_road_gate_scripts.h"
#include "assetpack_bind.h"
#include "amberscript_core.h"
#include "amberscript_mapbank.h"
#include "amberscript_scene.h"

#include "overworld.h"
#include "../data/tileset_data.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "gbc_color.h"
#include "data/gbc_palettes.h"
#include "../data/map_data.h"
#include "../data/event_data.h"
#include "../data/event_flag_ids.h"
#include "../data/event_flag_names.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdarg.h>
#include <dirent.h>
#include "text.h"
#include "crystal_tile_anim.h"

#ifdef _WIN32
#define PKS_TM_POPEN _popen
#define PKS_TM_PCLOSE _pclose
#else
#define PKS_TM_POPEN popen
#define PKS_TM_PCLOSE pclose
#endif

#define PKS_TILE_PROP_MAX 20480
typedef struct pks_tile_prop_t {
    int used;
    uint8_t map_id;
    int x;
    int y;
    uint8_t block_id;

    uint16_t tiles[4];
    uint8_t warp_mode;
    uint8_t dest_map;
    uint8_t dest_warp_idx;
    uint8_t dest_map_is_named;
    uint8_t dest_is_last;
    char dest_map_name[32];
    int art_slot;

    char sign_text[PKS_MAX_TEXT];

    int ledge_dirs;

    uint16_t cond_event;
    int cond_art_slot;

    uint8_t cond_negate;
} pks_tile_prop_t;
static pks_tile_prop_t s_tile_props[PKS_TILE_PROP_MAX];

static uint16_t s_map_tile_prop_count[256];

typedef enum pks_tile_source_kind_t {
    PKS_SRC_DIRECT     = 0,
    PKS_SRC_CUSTOM_ART = 1,
    PKS_SRC_QUAD       = 2
} pks_tile_source_kind_t;

#define PKS_SAVED_TILE_MAX 32768
typedef struct pks_saved_tile_t {
    int used;
    char name[32];
    uint8_t block_id;

    uint16_t tiles[4];
    uint8_t warp_mode;
    uint8_t dest_map;
    uint8_t dest_warp_idx;

    uint8_t dest_map_is_named;

    uint8_t dest_is_last;
    char dest_map_name[32];

    pks_tile_source_kind_t source_kind;
    uint8_t art_pixels[4][16];
    int art_passable;
    int art_passable_set;
    char art_src[200];

    int art_surfable;
    int art_surfable_set;

    int art_grass;
    int art_grass_set;

    int art_grass_rustle;
    int art_grass_rustle_set;

    int art_cuttable;
    int art_cuttable_set;

    int art_counter;
    int art_counter_set;

    int art_cut_span_block;

    char cut_replacement_name[32];

    char pair_block_group[32];
} pks_saved_tile_t;
static pks_saved_tile_t s_saved_tiles[PKS_SAVED_TILE_MAX];

#define PKS_ART_CACHE_LINES 6
#define PKS_ART_REAL_BASE   96
typedef struct pks_art_cache_line_t {
    int saved_tile_slot;
    unsigned lru;
} pks_art_cache_line_t;
static pks_art_cache_line_t s_art_cache[PKS_ART_CACHE_LINES] = {
    {-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0}
};
static unsigned s_art_cache_clock = 0;

static int pks_art_cache_find(int saved_tile_slot) {
    for (int i = 0; i < PKS_ART_CACHE_LINES; i++)
        if (s_art_cache[i].saved_tile_slot == saved_tile_slot) return i;
    return -1;
}

static int pks_art_cache_pick_line(void) {
    int victim = 0;
    unsigned oldest;
    for (int i = 0; i < PKS_ART_CACHE_LINES; i++)
        if (s_art_cache[i].saved_tile_slot < 0) return i;
    oldest = s_art_cache[0].lru;
    for (int i = 1; i < PKS_ART_CACHE_LINES; i++) {
        if (s_art_cache[i].lru < oldest) { oldest = s_art_cache[i].lru; victim = i; }
    }
    return victim;
}

static uint8_t pks_art_resolve_real_tile(int saved_tile_slot, int which) {
    int line = pks_art_cache_find(saved_tile_slot);
    if (line < 0) {
        line = pks_art_cache_pick_line();
        s_art_cache[line].saved_tile_slot = saved_tile_slot;
    }
    s_art_cache[line].lru = ++s_art_cache_clock;
    Display_LoadTile((uint8_t)(PKS_ART_REAL_BASE + line * 4 + which),
                      s_saved_tiles[saved_tile_slot].art_pixels[which]);
    return (uint8_t)(PKS_ART_REAL_BASE + line * 4 + which);
}

static int pks_saved_tile_find(const char *name);
static int pks_tile_place_data_ex(const pks_saved_tile_t *data, int x, int y, int art_slot);

#define PKS_SUBTILE_INITIAL_CAP 512
typedef struct pks_subtile_t {
    int used;
    char name[32];
    uint8_t pixels[16];

    uint8_t gbc_attr;
    uint8_t gbc_attr_set;
} pks_subtile_t;
static pks_subtile_t *s_subtiles = NULL;
static int s_subtiles_cap = 0;

static void pks_subtile_cache_line_of_grow(int old_cap, int new_cap);

static int pks_subtiles_grow(int needed) {
    int new_cap;
    pks_subtile_t *grown;
    if (needed <= s_subtiles_cap) return 1;
    new_cap = (s_subtiles_cap > 0) ? s_subtiles_cap : PKS_SUBTILE_INITIAL_CAP;
    while (new_cap < needed) new_cap *= 2;
    grown = (pks_subtile_t *)realloc(s_subtiles, (size_t)new_cap * sizeof(pks_subtile_t));
    if (!grown) return 0;
    memset(grown + s_subtiles_cap, 0, (size_t)(new_cap - s_subtiles_cap) * sizeof(pks_subtile_t));
    s_subtiles = grown;
    pks_subtile_cache_line_of_grow(s_subtiles_cap, new_cap);
    s_subtiles_cap = new_cap;
    return 1;
}

#define PKS_SUBTILE_HASH_SIZE 8192
static int s_subtile_hash[PKS_SUBTILE_HASH_SIZE];
static int s_subtile_hash_ready = 0;

static unsigned pks_hash_str(const char *s) {
    unsigned h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

static int pks_subtile_find(const char *name) {
    unsigned mask = PKS_SUBTILE_HASH_SIZE - 1;
    unsigned idx = pks_hash_str(name) & mask;
    if (!s_subtile_hash_ready) {
        s_subtile_hash_ready = 1;
        for (int i = 0; i < PKS_SUBTILE_HASH_SIZE; i++) s_subtile_hash[i] = -1;
    }
    for (unsigned probe = 0; probe < PKS_SUBTILE_HASH_SIZE; probe++) {
        unsigned h = (idx + probe) & mask;
        int slot = s_subtile_hash[h];
        if (slot < 0) return -1;
        if (s_subtiles[slot].used && strcmp(s_subtiles[slot].name, name) == 0)
            return slot;
    }
    return -1;
}

static void pks_subtile_hash_insert(const char *name, int slot) {
    unsigned mask = PKS_SUBTILE_HASH_SIZE - 1;
    unsigned idx = pks_hash_str(name) & mask;
    if (!s_subtile_hash_ready) {
        s_subtile_hash_ready = 1;
        for (int i = 0; i < PKS_SUBTILE_HASH_SIZE; i++) s_subtile_hash[i] = -1;
    }
    for (unsigned probe = 0; probe < PKS_SUBTILE_HASH_SIZE; probe++) {
        unsigned h = (idx + probe) & mask;
        if (s_subtile_hash[h] < 0) { s_subtile_hash[h] = slot; return; }
    }

}

static int s_subtiles_count = 0;

static int pks_subtile_alloc(const char *name) {
    int slot = pks_subtile_find(name);
    if (slot >= 0) return slot;

    if (s_subtiles_count >= s_subtiles_cap) {
        if (!pks_subtiles_grow(s_subtiles_count + 1)) return -1;
    }
    slot = s_subtiles_count++;
    pks_subtile_hash_insert(name, slot);
    return slot;
}

#define PKS_ANIM_RESERVED_A 0x03
#define PKS_ANIM_RESERVED_B 0x14

#define PKS_SUBTILE_CACHE_LINES 94

typedef struct pks_subtile_cache_line_t {
    int      subtile_slot;
    unsigned lru;

    unsigned gen;
} pks_subtile_cache_line_t;
static pks_subtile_cache_line_t s_subtile_cache[PKS_SUBTILE_CACHE_LINES];
static unsigned s_subtile_cache_clock = 0;

static unsigned s_subtile_cache_generation = 0;

static unsigned s_subtile_cache_last_overcapacity_gen = 0xFFFFFFFFu;

static int *s_subtile_cache_line_of = NULL;
static int s_subtile_cache_line_of_ready = 0;

static void pks_subtile_cache_line_of_grow(int old_cap, int new_cap) {
    int *grown = (int *)realloc(s_subtile_cache_line_of, (size_t)new_cap * sizeof(int));
    if (!grown) return;
    for (int i = old_cap; i < new_cap; i++) grown[i] = -1;
    s_subtile_cache_line_of = grown;
}

static void pks_subtile_cache_ensure_ready(void) {
    if (s_subtile_cache_line_of_ready) return;
    s_subtile_cache_line_of_ready = 1;
    for (int i = 0; i < PKS_SUBTILE_CACHE_LINES; i++) {
        s_subtile_cache[i].subtile_slot = -1;
        s_subtile_cache[i].lru = 0;
        s_subtile_cache[i].gen = 0;
    }
}

static int pks_subtile_cache_real_id(int line) {
    static int table[PKS_SUBTILE_CACHE_LINES];
    static int built = 0;
    if (!built) {
        int real = 0, i = 0;
        while (i < PKS_SUBTILE_CACHE_LINES) {
            if (real == PKS_ANIM_RESERVED_A || real == PKS_ANIM_RESERVED_B) { real++; continue; }
            table[i++] = real++;
        }
        built = 1;
    }
    return table[line];
}

static int pks_subtile_cache_pick_line(void) {
    int victim = -1;
    unsigned oldest = 0;
    pks_subtile_cache_ensure_ready();
    for (int i = 0; i < PKS_SUBTILE_CACHE_LINES; i++) {
        if (s_subtile_cache[i].subtile_slot < 0) return i;
        if (s_subtile_cache[i].gen == s_subtile_cache_generation) continue;
        if (victim < 0 || s_subtile_cache[i].lru < oldest) { victim = i; oldest = s_subtile_cache[i].lru; }
    }
    if (victim >= 0) return victim;

    if (s_subtile_cache_last_overcapacity_gen != s_subtile_cache_generation) {
        s_subtile_cache_last_overcapacity_gen = s_subtile_cache_generation;
        printf("[amberscript] subtile_cache: over capacity (>%d distinct subtiles in one "
               "rebuild pass) -- some cells this pass may show a reused-too-early real "
               "id until the next rebuild recovers. Reduce simultaneous visual variety "
               "or split the offending viewport's art.\n", PKS_SUBTILE_CACHE_LINES);
    }
    victim = 0; oldest = s_subtile_cache[0].lru;
    for (int i = 1; i < PKS_SUBTILE_CACHE_LINES; i++)
        if (s_subtile_cache[i].lru < oldest) { oldest = s_subtile_cache[i].lru; victim = i; }
    return victim;
}

long g_subtile_cache_hits = 0;
long g_subtile_cache_misses = 0;

static uint8_t pks_subtile_cache_resolve(int subtile_slot) {
    int line;
    pks_subtile_cache_ensure_ready();
    line = (subtile_slot >= 0 && subtile_slot < s_subtiles_cap)
         ? s_subtile_cache_line_of[subtile_slot] : -1;
    if (line < 0) {
        g_subtile_cache_misses++;
        line = pks_subtile_cache_pick_line();
        if (s_subtile_cache[line].subtile_slot >= 0)
            s_subtile_cache_line_of[s_subtile_cache[line].subtile_slot] = -1;
        s_subtile_cache[line].subtile_slot = subtile_slot;
        s_subtile_cache_line_of[subtile_slot] = line;
        Display_LoadTile((uint8_t)pks_subtile_cache_real_id(line),
                          s_subtiles[subtile_slot].pixels);
    } else {
        g_subtile_cache_hits++;
    }
    s_subtile_cache[line].lru = ++s_subtile_cache_clock;
    s_subtile_cache[line].gen = s_subtile_cache_generation;
    {

        uint8_t real_id = (uint8_t)pks_subtile_cache_real_id(line);
        Display_SetTileAttr(real_id, s_subtiles[subtile_slot].gbc_attr_set
                                         ? s_subtiles[subtile_slot].gbc_attr
                                         : 0);
        return real_id;
    }
}

static const uint8_t kJohtoWaterFrames[4][16] = {
    { 0x00,0xFE, 0x00,0xFB, 0x00,0xFF, 0x00,0xFF, 0x00,0xEF, 0x00,0xFF, 0x00,0xDF, 0x00,0xFF },
    { 0x00,0x7F, 0x00,0xF7, 0x00,0xFF, 0x00,0xFF, 0x00,0xF7, 0x00,0xFF, 0x00,0xBF, 0x00,0xFF },
    { 0x00,0xBF, 0x00,0xEF, 0x00,0xFF, 0x00,0xFF, 0x00,0xFB, 0x00,0xFF, 0x00,0x7F, 0x00,0xFF },
    { 0x00,0x7F, 0x00,0xF7, 0x00,0xFF, 0x00,0xFF, 0x00,0xF7, 0x00,0xFF, 0x00,0xBF, 0x00,0xFF },
};
static const uint8_t kJohtoFlowerFrames[2][16] = {
    { 0xA2,0x0C, 0x41,0x12, 0x8C,0x21, 0x0C,0x61, 0x20,0x92, 0x31,0x8C, 0x82,0x48, 0x45,0x30 },
    { 0xA2,0x18, 0x41,0x24, 0x98,0x42, 0x19,0x42, 0x80,0x66, 0x01,0x5A, 0x82,0x24, 0x45,0x18 },
};

static uint8_t s_johto_anim_frame_counter = 0;
static uint8_t s_johto_anim_timer = 0;

static void pks_johto_anim_update_subtile(const char *name, const uint8_t *pixels) {
    int slot = pks_subtile_find(name);
    if (slot < 0) return;
    memcpy(s_subtiles[slot].pixels, pixels, 16);
    if (slot < s_subtiles_cap) {
        int line = s_subtile_cache_line_of[slot];
        if (line >= 0)
            Display_LoadTile((uint8_t)pks_subtile_cache_real_id(line), pixels);
    }
}

void AmberScript_SetSubtilePixels(const char *name, const uint8_t pixels[16]) {
    pks_johto_anim_update_subtile(name, pixels);
}

static const uint8_t kKantoIndoorWaterBase[16] = {
    0x00,0x3c, 0x00,0x89, 0x00,0x60, 0x00,0xf7, 0x00,0xe7, 0x00,0xe7, 0x00,0x9b, 0x00,0xdc };

#define PKS_KANTO_ANIM_SLOT_MAX 256
static int s_kanto_water_slots[PKS_KANTO_ANIM_SLOT_MAX];
static int s_kanto_water_count = 0;
static int s_kanto_flower_slots[PKS_KANTO_ANIM_SLOT_MAX];
static int s_kanto_flower_count = 0;
static uint8_t s_kanto_anim_counter1 = 0;
static uint8_t s_kanto_anim_counter2 = 0;

static inline uint8_t pks_rrca8(uint8_t v) { return (v >> 1) | (uint8_t)(v << 7); }
static inline uint8_t pks_rlca8(uint8_t v) { return (uint8_t)(v << 1) | (v >> 7); }

static void pks_kanto_anim_register(int slot) {
    if (slot < 0 || slot >= s_subtiles_cap) return;
    const uint8_t *px = s_subtiles[slot].pixels;
    if (memcmp(px, kKantoWaterBase, 16) == 0 ||
        memcmp(px, kKantoIndoorWaterBase, 16) == 0) {
        if (s_kanto_water_count < PKS_KANTO_ANIM_SLOT_MAX)
            s_kanto_water_slots[s_kanto_water_count++] = slot;
    } else if (memcmp(px, kKantoFlowerBase, 16) == 0 ||
               memcmp(px, kKantoGymFlowerBase, 16) == 0) {
        if (s_kanto_flower_count < PKS_KANTO_ANIM_SLOT_MAX)
            s_kanto_flower_slots[s_kanto_flower_count++] = slot;
    }
}

static void pks_kanto_anim_blit_slot(int slot, const uint8_t *pixels) {
    if (slot < 0 || slot >= s_subtiles_cap) return;
    if (s_subtiles[slot].pixels != pixels)
        memcpy(s_subtiles[slot].pixels, pixels, 16);
    int line = s_subtile_cache_line_of[slot];
    if (line >= 0)
        Display_LoadTile((uint8_t)pks_subtile_cache_real_id(line), s_subtiles[slot].pixels);
}

int AmberScript_SubtileBlitPixels(const char *name, const uint8_t *pixels) {
    int slot;
    if (!name || !*name || !pixels) return 0;
    slot = pks_subtile_find(name);
    if (slot < 0) return 0;
    pks_kanto_anim_blit_slot(slot, pixels);
    return 1;
}

int AmberScript_SubtileReadPixels(const char *name, uint8_t *out) {
    int slot;
    if (!name || !*name || !out) return 0;
    slot = pks_subtile_find(name);
    if (slot < 0) return 0;
    memcpy(out, s_subtiles[slot].pixels, 16);
    return 1;
}

static void pks_kanto_anim_tick(void) {
    s_kanto_anim_counter1++;
    if (s_kanto_anim_counter1 < 20) return;

    if (s_kanto_anim_counter1 == 21) {

        s_kanto_anim_counter1 = 0;
        uint8_t v = s_kanto_anim_counter2 & 3;
        int f = (v < 2) ? 0 : (v == 2) ? 1 : 2;
        for (int i = 0; i < s_kanto_flower_count; i++)
            pks_kanto_anim_blit_slot(s_kanto_flower_slots[i], kKantoFlowerFrames[f]);
        return;
    }

    s_kanto_anim_counter2 = (s_kanto_anim_counter2 + 1) & 7;
    int rotate_left = (s_kanto_anim_counter2 & 4) != 0;
    for (int i = 0; i < s_kanto_water_count; i++) {
        int slot = s_kanto_water_slots[i];
        if (slot < 0 || slot >= s_subtiles_cap) continue;
        uint8_t *px = s_subtiles[slot].pixels;
        for (int b = 0; b < 16; b++)
            px[b] = rotate_left ? pks_rlca8(px[b]) : pks_rrca8(px[b]);
        pks_kanto_anim_blit_slot(slot, px);
    }
}

void AmberScript_TickTileAnimations(void) {
    if (wCurMap < PKS_VIRTUAL_MAP_FIRST || wCurMap > PKS_VIRTUAL_MAP_LAST) return;

    pks_kanto_anim_tick();

    s_johto_anim_frame_counter++;
    if (s_johto_anim_frame_counter < 11) return;
    s_johto_anim_frame_counter = 0;
    s_johto_anim_timer = (s_johto_anim_timer + 1) & 7;

    pks_johto_anim_update_subtile("johto_t035", kJohtoWaterFrames[(s_johto_anim_timer >> 1) & 3]);
    pks_johto_anim_update_subtile("johto_t036", kJohtoFlowerFrames[(s_johto_anim_timer >> 1) & 1]);
}

static void pks_crystal_anim_tick(void) {
    const char *slug = NULL;
    int tileset = AmberScript_MapBank_GetCrystalAnimForRealId((int)wCurMap, &slug);
    CrystalTileAnim_SetTileset(tileset, slug);
    if (tileset >= 0) CrystalTileAnim_Tick();
}

static void pks_subtile_cache_invalidate_all(void) {
    pks_subtile_cache_ensure_ready();
    for (int i = 0; i < PKS_SUBTILE_CACHE_LINES; i++) {
        if (s_subtile_cache[i].subtile_slot >= 0)
            s_subtile_cache_line_of[s_subtile_cache[i].subtile_slot] = -1;
        s_subtile_cache[i].subtile_slot = -1;
    }
    Map_BuildScrollView();
}

void AmberScript_TileMod_InvalidateSubtileCache(void) {
    pks_subtile_cache_invalidate_all();
}

void AmberScript_TileMod_BumpCacheGeneration(void) {
    s_subtile_cache_generation++;
}

#define PKS_TILESET_MAX 16
#define PKS_TILESET_ASSET_MAX 96
#define PKS_TILESET_ASSET_MODE_MAX 24
typedef struct pks_tileset_t {
    int used;
    char name[32];
    int is_subtile;
    int asset_slot[PKS_TILESET_ASSET_MAX];
    int asset_count;
    int bound;
    uint8_t bound_map;
} pks_tileset_t;
static pks_tileset_t s_tilesets[PKS_TILESET_MAX];
static uint8_t s_tileset_last_map = 0xFF;

static int pks_tileset_find(const char *name) {
    for (int i = 0; i < PKS_TILESET_MAX; i++)
        if (s_tilesets[i].used && strcmp(s_tilesets[i].name, name) == 0) return i;
    return -1;
}

static int pks_tileset_alloc(const char *name) {
    int slot = pks_tileset_find(name);
    if (slot >= 0) return slot;
    for (int i = 0; i < PKS_TILESET_MAX; i++) {
        if (!s_tilesets[i].used) {
            memset(&s_tilesets[i], 0, sizeof(s_tilesets[i]));
            s_tilesets[i].used = 1;
            snprintf(s_tilesets[i].name, sizeof(s_tilesets[i].name), "%s", name);
            return i;
        }
    }
    return -1;
}

static void pks_tileset_blit(int ts_slot) {
    pks_tileset_t *t = &s_tilesets[ts_slot];
    if (t->is_subtile) {

        return;
    }
    for (int i = 0; i < t->asset_count; i++) {
        int saved = t->asset_slot[i];
        if (saved < 0 || saved >= PKS_SAVED_TILE_MAX || !s_saved_tiles[saved].used) continue;
        if (s_saved_tiles[saved].source_kind != PKS_SRC_CUSTOM_ART) continue;
        for (int k = 0; k < 4; k++) {
            Display_LoadTile((uint8_t)(i * 4 + k), s_saved_tiles[saved].art_pixels[k]);
        }
    }
}

int AmberScript_TilesetAdd(const char *tileset_name, const char *asset_name) {
    int ts_slot, asset_slot;
    if (!tileset_name || !*tileset_name || !asset_name || !*asset_name) return 0;
    asset_slot = pks_saved_tile_find(asset_name);
    if (asset_slot < 0 || s_saved_tiles[asset_slot].source_kind != PKS_SRC_CUSTOM_ART) return 0;
    ts_slot = pks_tileset_alloc(tileset_name);
    if (ts_slot < 0 || s_tilesets[ts_slot].is_subtile) return 0;
    for (int i = 0; i < s_tilesets[ts_slot].asset_count; i++)
        if (s_tilesets[ts_slot].asset_slot[i] == asset_slot) return 1;
    if (s_tilesets[ts_slot].asset_count >= PKS_TILESET_ASSET_MODE_MAX) return 0;
    s_tilesets[ts_slot].asset_slot[s_tilesets[ts_slot].asset_count++] = asset_slot;
    return 1;
}

int AmberScript_SubtileTilesetAdd(const char *tileset_name, const char *subtile_name) {
    int ts_slot, sub_slot;
    if (!tileset_name || !*tileset_name || !subtile_name || !*subtile_name) return 0;
    sub_slot = pks_subtile_find(subtile_name);
    if (sub_slot < 0) return 0;
    ts_slot = pks_tileset_alloc(tileset_name);
    if (ts_slot < 0) return 0;
    if (s_tilesets[ts_slot].asset_count > 0 && !s_tilesets[ts_slot].is_subtile) return 0;
    s_tilesets[ts_slot].is_subtile = 1;

    for (int i = 0; i < s_tilesets[ts_slot].asset_count; i++)
        if (s_tilesets[ts_slot].asset_slot[i] == sub_slot) return 1;
    if (s_tilesets[ts_slot].asset_count >= PKS_TILESET_ASSET_MAX) return 0;
    s_tilesets[ts_slot].asset_slot[s_tilesets[ts_slot].asset_count++] = sub_slot;
    return 1;
}

int AmberScript_TilesetClear(const char *tileset_name) {
    int ts_slot = pks_tileset_find(tileset_name);
    if (ts_slot < 0) return 0;
    memset(&s_tilesets[ts_slot], 0, sizeof(s_tilesets[ts_slot]));
    return 1;
}

void AmberScript_TileMod_ResetAllMapBindings(void) {
    for (int i = 0; i < PKS_TILESET_MAX; i++)
        if (s_tilesets[i].used) s_tilesets[i].bound = 0;
    s_tileset_last_map = 0xFF;
    memset(s_tile_props, 0, sizeof(s_tile_props));
    memset(s_map_tile_prop_count, 0, sizeof(s_map_tile_prop_count));
    AmberScript_TileMod_InvalidateSubtileCache();
}

void AmberScript_TilesetUnbindMap(uint8_t map_id) {
    for (int i = 0; i < PKS_TILESET_MAX; i++) {
        if (s_tilesets[i].used && s_tilesets[i].bound && s_tilesets[i].bound_map == map_id)
            s_tilesets[i].bound = 0;
    }
}

int AmberScript_TilesetApply(const char *tileset_name) {
    int ts_slot = pks_tileset_find(tileset_name);
    if (ts_slot < 0 || s_tilesets[ts_slot].asset_count <= 0) return 0;
    pks_tileset_blit(ts_slot);
    s_tilesets[ts_slot].bound = 1;
    s_tilesets[ts_slot].bound_map = wCurMap;
    s_tileset_last_map = wCurMap;
    Map_BuildScrollView();
    return 1;
}

static int pks_resolve_output_path(const char *subdir, const char *name, const char *ext,
                                    char *out_path, size_t out_sz) {
    static const char *kDirFmt[] = {
        "mod_runtime/%s/", "../mod_runtime/%s/"
    };
    for (int i = 0; i < (int)(sizeof(kDirFmt) / sizeof(kDirFmt[0])); i++) {
        char dir[256];
        char probe_path[300];
        FILE *probe;
        snprintf(dir, sizeof(dir), kDirFmt[i], subdir);
        snprintf(probe_path, sizeof(probe_path), "%s.pks_probe", dir);
        probe = fopen(probe_path, "w");
        if (!probe) continue;
        fclose(probe);
        remove(probe_path);
        snprintf(out_path, out_sz, "%s%s.%s", dir, name, ext);
        return 1;
    }
    return 0;
}

static void pks_decode_tile_row(const uint8_t gfx[16], int row, uint8_t out_vals[8]) {
    uint8_t lo = gfx[row * 2], hi = gfx[row * 2 + 1];
    for (int col = 0; col < 8; col++) {
        int bit = 7 - col;
        out_vals[col] = (uint8_t)((((hi >> bit) & 1) << 1) | ((lo >> bit) & 1));
    }
}

static void pks_export_tileset_atlas(void);
static void pks_export_tile_collision(void);

int AmberScript_MapExport(const char *name) {
    char ppm_path[300], grid_path[300];
    int tiles_w, tiles_h, cells_w, cells_h;
    uint8_t *pixels;
    FILE *ppm, *grid;

    if (!name || !*name) return 0;
    {
        char tiles_name[160];
        if (!pks_resolve_output_path("map_export", name, "ppm", ppm_path, sizeof(ppm_path))) return 0;
        snprintf(tiles_name, sizeof(tiles_name), "%s_tiles", name);
        pks_resolve_output_path("map_export", tiles_name, "txt", grid_path, sizeof(grid_path));
    }

    tiles_w = (int)wCurMapWidth * 4;
    tiles_h = (int)wCurMapHeight * 4;
    cells_w = tiles_w / 2;
    cells_h = tiles_h / 2;
    if (tiles_w <= 0 || tiles_h <= 0) return 0;

    pixels = (uint8_t *)malloc((size_t)tiles_w * 8 * (size_t)tiles_h * 8 * 3);
    if (!pixels) return 0;

    for (int ty = 0; ty < tiles_h; ty++) {
        for (int tx = 0; tx < tiles_w; tx++) {
            uint8_t id = Map_GetTile(tx, ty);
            uint8_t gfx[16];
            Display_GetTile(id, gfx);
            for (int row = 0; row < 8; row++) {
                uint8_t vals[8];
                pks_decode_tile_row(gfx, row, vals);
                int py = ty * 8 + row;
                for (int col = 0; col < 8; col++) {
                    int px = tx * 8 + col;
                    uint8_t shade = (uint8_t)(255 - vals[col] * 85);
                    size_t off = ((size_t)py * (tiles_w * 8) + px) * 3;
                    pixels[off + 0] = shade;
                    pixels[off + 1] = shade;
                    pixels[off + 2] = shade;
                }
            }
        }
    }

    ppm = fopen(ppm_path, "wb");
    if (!ppm) { free(pixels); return 0; }
    fprintf(ppm, "P6\n%d %d\n255\n", tiles_w * 8, tiles_h * 8);
    fwrite(pixels, 1, (size_t)tiles_w * 8 * (size_t)tiles_h * 8 * 3, ppm);
    fclose(ppm);
    free(pixels);

    grid = fopen(grid_path, "w");
    if (grid) {
        const char *vmap_name = AmberScript_MapBank_NameForRealId((int)wCurMap);
        fprintf(grid, "%d %d %d %s\n", (int)wCurMap, cells_w, cells_h, vmap_name ? vmap_name : "-");
        for (int cy = 0; cy < cells_h; cy++) {
            for (int cx = 0; cx < cells_w; cx++) {
                int gx = cx * 2, gy = cy * 2;
                fprintf(grid, "%d,%d,%d,%d ",
                        Map_GetTile(gx, gy), Map_GetTile(gx + 1, gy),
                        Map_GetTile(gx, gy + 1), Map_GetTile(gx + 1, gy + 1));
            }
            fprintf(grid, "\n");
        }
        fclose(grid);
    }

    pks_export_tileset_atlas();
    pks_export_tile_collision();

    printf("[amberscript] map_export: map %d -> %s (%dx%d cells, %dx%d px)\n",
           (int)wCurMap, ppm_path, cells_w, cells_h, tiles_w * 8, tiles_h * 8);
    return 1;
}

static void pks_export_tile_collision(void) {
    char path[300];
    FILE *f;
    if (!pks_resolve_output_path("map_export", "tile_collision", "txt", path, sizeof(path))) return;
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# tile_gfx_id passable(0/1) is_grass(0/1) -- for tileset currently loaded (map %d)\n",
            (int)wCurMap);
    for (int id = 0; id < 256; id++) {
        int passable = Tile_IsPassable((uint8_t)id) ? 1 : 0;
        int is_grass = (wGrassTile != 0xFF && (uint8_t)id == wGrassTile) ? 1 : 0;
        fprintf(f, "%d %d %d\n", id, passable, is_grass);
    }
    fclose(f);
}

static void pks_export_tileset_atlas(void) {
    char path[300];
    uint8_t pixels[128 * 128 * 3];
    FILE *f;
    if (!pks_resolve_output_path("map_export", "tileset_atlas", "ppm", path, sizeof(path))) return;

    for (int id = 0; id < 256; id++) {
        uint8_t gfx[16];
        int atlas_tx = id % 16, atlas_ty = id / 16;
        Display_GetTile((uint8_t)id, gfx);
        for (int row = 0; row < 8; row++) {
            uint8_t vals[8];
            pks_decode_tile_row(gfx, row, vals);
            int py = atlas_ty * 8 + row;
            for (int col = 0; col < 8; col++) {
                int px = atlas_tx * 8 + col;
                uint8_t shade = (uint8_t)(255 - vals[col] * 85);
                size_t off = ((size_t)py * 128 + px) * 3;
                pixels[off + 0] = shade;
                pixels[off + 1] = shade;
                pixels[off + 2] = shade;
            }
        }
    }

    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n128 128\n255\n");
    fwrite(pixels, 1, sizeof(pixels), f);
    fclose(f);
}

int AmberScript_MapEditsApply(void) {
    char path[300];
    char line[128];
    FILE *f = NULL;
    int applied = 0;
    static const char *kDirFmt[] = {
        "mod_runtime/map_edits/", "../mod_runtime/map_edits/"
    };
    for (int i = 0; i < (int)(sizeof(kDirFmt) / sizeof(kDirFmt[0])); i++) {
        snprintf(path, sizeof(path), "%smap_%d.txt", kDirFmt[i], (int)wCurMap);
        f = fopen(path, "r");
        if (f) break;
    }
    if (!f) return -1;

    while (fgets(line, sizeof(line), f)) {
        int x, y;
        char kind[16] = {0}, ref[64] = {0};
        if (sscanf(line, "%d %d %15s %63s", &x, &y, kind, ref) != 4) continue;
        if (strcmp(kind, "custom") == 0) {
            int slot = pks_saved_tile_find(ref);
            if (slot >= 0 && pks_tile_place_data_ex(&s_saved_tiles[slot], x, y, slot)) applied++;
        } else if (strcmp(kind, "orig") == 0) {
            pks_saved_tile_t tile;
            uint8_t tile_id = (uint8_t)atoi(ref);
            memset(&tile, 0, sizeof(tile));
            tile.tiles[0] = tile.tiles[1] = tile.tiles[2] = tile.tiles[3] = tile_id;
            if (pks_tile_place_data_ex(&tile, x, y, -1)) applied++;
        }
    }
    fclose(f);
    if (applied > 0) Map_BuildScrollView();
    return applied;
}

static void pks_reapply_current_map_gfx(void) {
    for (int i = 0; i < PKS_TILESET_MAX; i++) {
        if (s_tilesets[i].used && s_tilesets[i].bound && s_tilesets[i].bound_map == wCurMap) {
            pks_tileset_blit(i);
        }
    }
    AmberScript_MapEditsApply();
}

void AmberScript_TileMod_ReapplyCurrentMapNow(void) {
    if (wCurMap == s_tileset_last_map) return;
    s_tileset_last_map = wCurMap;
    pks_reapply_current_map_gfx();
}

void AmberScript_TileMod_ForceReapplyGfx(void) {

    for (int i = 0; i < PKS_TILESET_MAX; i++) {
        if (s_tilesets[i].used && s_tilesets[i].bound) {
            pks_tileset_blit(i);
        }
    }
    AmberScript_MapEditsApply();

    pks_subtile_cache_invalidate_all();
}

static uint8_t s_prewarm_last_map = 0xFF;

void AmberScript_TileMod_PrewarmNeighbors(void) {

    int found[4] = {0, 0, 0, 0};
    s_prewarm_last_map = wCurMap;
    for (int dir = 0; dir < 4; dir++) {
        uint8_t dest_real_id;
        int16_t player_coord, adjust;
        found[dir] = AmberScript_GetConnectionOverride(wCurMap, dir, &dest_real_id, &player_coord, &adjust);
    }
    printf("[amberscript] PrewarmNeighbors: map real=%d -- N=%d S=%d W=%d E=%d (1=connection found+streamed, 0=none)\n",
           (int)wCurMap, found[0], found[1], found[2], found[3]);
}

void AmberScript_TileMod_Tick(void) {

    if (Map_IsHeldForBootScreen()) return;

    AmberScript_TileMod_ReapplyCurrentMapNow();

    pks_crystal_anim_tick();

    if (wCurMap == s_prewarm_last_map) return;
    AmberScript_TileMod_PrewarmNeighbors();
}

#define PKS_SAVED_BLOCK_MAX 16
#define PKS_SAVED_BLOCK_CELL_MAX 128
typedef struct pks_saved_block_cell_t {
    int dx;
    int dy;
    uint8_t block_id;
    uint8_t tiles[4];
    uint8_t warp_mode;
    uint8_t dest_map;
    uint8_t dest_warp_idx;
} pks_saved_block_cell_t;
typedef struct pks_saved_block_t {
    int used;
    char name[32];
    int cell_count;
    pks_saved_block_cell_t cells[PKS_SAVED_BLOCK_CELL_MAX];
} pks_saved_block_t;
static pks_saved_block_t s_saved_blocks[PKS_SAVED_BLOCK_MAX];

#define PKS_TILE_PROP_HASH_SIZE 32768
static int s_tile_prop_hash[PKS_TILE_PROP_HASH_SIZE];
static int s_tile_prop_hash_ready = 0;

static unsigned pks_hash_prop(uint8_t map_id, int x, int y) {
    unsigned h = 2166136261u;
    h ^= (unsigned)map_id; h *= 16777619u;
    h ^= (unsigned)(x & 0xFFFF); h *= 16777619u;
    h ^= (unsigned)(y & 0xFFFF); h *= 16777619u;
    return h;
}

static void pks_tile_prop_hash_insert(uint8_t map_id, int x, int y, int slot) {
    unsigned mask = PKS_TILE_PROP_HASH_SIZE - 1;
    unsigned idx = pks_hash_prop(map_id, x, y) & mask;
    if (!s_tile_prop_hash_ready) {
        s_tile_prop_hash_ready = 1;
        for (int i = 0; i < PKS_TILE_PROP_HASH_SIZE; i++) s_tile_prop_hash[i] = -1;
    }
    for (unsigned probe = 0; probe < PKS_TILE_PROP_HASH_SIZE; probe++) {
        unsigned h = (idx + probe) & mask;
        if (s_tile_prop_hash[h] < 0) { s_tile_prop_hash[h] = slot; return; }
    }

}

static void pks_tile_prop_hash_rebuild(void) {
    if (!s_tile_prop_hash_ready) return;
    for (int i = 0; i < PKS_TILE_PROP_HASH_SIZE; i++) s_tile_prop_hash[i] = -1;
    for (int i = 0; i < PKS_TILE_PROP_MAX; i++) {
        if (!s_tile_props[i].used) continue;
        pks_tile_prop_hash_insert(s_tile_props[i].map_id,
                                  s_tile_props[i].x, s_tile_props[i].y, i);
    }
}

static int pks_tile_prop_find_slot_for_map(uint8_t map_id, int x, int y) {
    unsigned mask = PKS_TILE_PROP_HASH_SIZE - 1;
    unsigned idx;
    if (s_map_tile_prop_count[map_id] == 0) return -1;
    if (!s_tile_prop_hash_ready) return -1;
    idx = pks_hash_prop(map_id, x, y) & mask;
    for (unsigned probe = 0; probe < PKS_TILE_PROP_HASH_SIZE; probe++) {
        unsigned h = (idx + probe) & mask;
        int slot = s_tile_prop_hash[h];
        if (slot < 0) return -1;
        if (s_tile_props[slot].used &&
            s_tile_props[slot].map_id == map_id &&
            s_tile_props[slot].x == x &&
            s_tile_props[slot].y == y) return slot;

    }
    return -1;
}

static int pks_tile_prop_find_slot(int x, int y) {
    return pks_tile_prop_find_slot_for_map(wCurMap, x, y);
}

static int s_tile_prop_next_free_hint = 0;

static int pks_tile_prop_alloc_slot(void) {
    for (int i = 0; i < PKS_TILE_PROP_MAX; i++) {
        int idx = (s_tile_prop_next_free_hint + i) % PKS_TILE_PROP_MAX;
        if (!s_tile_props[idx].used) {
            s_tile_prop_next_free_hint = idx + 1;
            return idx;
        }
    }
    return -1;
}

#define PKS_SAVED_TILE_HASH_SIZE 65536
static int s_saved_tile_hash[PKS_SAVED_TILE_HASH_SIZE];
static int s_saved_tile_hash_ready = 0;

static int pks_saved_tile_find(const char *name) {
    unsigned mask = PKS_SAVED_TILE_HASH_SIZE - 1;
    unsigned idx = pks_hash_str(name) & mask;
    if (!s_saved_tile_hash_ready) {
        s_saved_tile_hash_ready = 1;
        for (int i = 0; i < PKS_SAVED_TILE_HASH_SIZE; i++) s_saved_tile_hash[i] = -1;
    }
    for (unsigned probe = 0; probe < PKS_SAVED_TILE_HASH_SIZE; probe++) {
        unsigned h = (idx + probe) & mask;
        int slot = s_saved_tile_hash[h];
        if (slot < 0) return -1;
        if (s_saved_tiles[slot].used && strcmp(s_saved_tiles[slot].name, name) == 0)
            return slot;
    }
    return -1;
}

static void pks_saved_tile_hash_insert(const char *name, int slot) {
    unsigned mask = PKS_SAVED_TILE_HASH_SIZE - 1;
    unsigned idx = pks_hash_str(name) & mask;
    if (!s_saved_tile_hash_ready) {
        s_saved_tile_hash_ready = 1;
        for (int i = 0; i < PKS_SAVED_TILE_HASH_SIZE; i++) s_saved_tile_hash[i] = -1;
    }
    for (unsigned probe = 0; probe < PKS_SAVED_TILE_HASH_SIZE; probe++) {
        unsigned h = (idx + probe) & mask;
        if (s_saved_tile_hash[h] < 0) { s_saved_tile_hash[h] = slot; return; }
    }

}

static int s_saved_tiles_count = 0;

static int pks_saved_tile_alloc(const char *name) {
    int slot = pks_saved_tile_find(name);
    if (slot >= 0) return slot;
    if (s_saved_tiles_count >= PKS_SAVED_TILE_MAX) {

        printf("[amberscript] pks_saved_tile_alloc: POOL EXHAUSTED (%d/%d) -- "
               "'%s' will never be defined this session; bump PKS_SAVED_TILE_MAX\n",
               s_saved_tiles_count, PKS_SAVED_TILE_MAX, name ? name : "(null)");
        return -1;
    }
    slot = s_saved_tiles_count++;
    pks_saved_tile_hash_insert(name, slot);
    return slot;
}

int AmberScript_SavedBlockFind(const char *name) {
    for (int i = 0; i < PKS_SAVED_BLOCK_MAX; i++) {
        if (s_saved_blocks[i].used && strcmp(s_saved_blocks[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int pks_saved_block_alloc(const char *name) {
    int slot = AmberScript_SavedBlockFind(name);
    if (slot >= 0) return slot;
    for (int i = 0; i < PKS_SAVED_BLOCK_MAX; i++) {
        if (!s_saved_blocks[i].used) return i;
    }
    return -1;
}

static int pks_game_coord_in_bounds(int x, int y) {
    return x >= 0 && y >= 0 &&
           x < (int)wCurMapWidth * 2 &&
           y < (int)wCurMapHeight * 2;
}

static int pks_get_canonical_warp_at(int x, int y, uint8_t *dest_map, uint8_t *dest_warp_idx) {
    if (wCurMap >= PKS_VIRTUAL_MAP_FIRST) return 0;
    const map_events_t *ev = &gMapEvents[wCurMap];
    if (!ev->warps) return 0;
    for (int i = 0; i < ev->num_warps; i++) {
        if (ev->warps[i].x == x && ev->warps[i].y == y) {
            if (dest_map) *dest_map = ev->warps[i].dest_map;
            if (dest_warp_idx) *dest_warp_idx = ev->warps[i].dest_warp_idx;
            return 1;
        }
    }
    return 0;
}

static int pks_get_effective_warp_at(int x, int y, uint8_t *has_warp, uint8_t *dest_map, uint8_t *dest_warp_idx) {
    int slot = pks_tile_prop_find_slot(x, y);
    if (slot >= 0) {
        if (has_warp) *has_warp = s_tile_props[slot].warp_mode ? 1 : 0;
        if (dest_map) *dest_map = s_tile_props[slot].dest_map;
        if (dest_warp_idx) *dest_warp_idx = s_tile_props[slot].dest_warp_idx;
        return 1;
    }
    if (pks_get_canonical_warp_at(x, y, dest_map, dest_warp_idx)) {
        if (has_warp) *has_warp = 1;
        return 1;
    }
    if (has_warp) *has_warp = 0;
    if (dest_map) *dest_map = 0;
    if (dest_warp_idx) *dest_warp_idx = 0;
    return 1;
}

static int pks_tile_capture_at(int x, int y, pks_saved_tile_t *out) {
    uint8_t has_warp = 0, dest_map = 0, dest_warp_idx = 0;
    if (!out || !pks_game_coord_in_bounds(x, y)) return 0;
    memset(out, 0, sizeof(*out));
    out->block_id = Map_GetBlockAt(x, y);
    out->tiles[0] = Map_GetTile(x * 2,     y * 2);
    out->tiles[1] = Map_GetTile(x * 2 + 1, y * 2);
    out->tiles[2] = Map_GetTile(x * 2,     y * 2 + 1);
    out->tiles[3] = Map_GetTile(x * 2 + 1, y * 2 + 1);
    pks_get_effective_warp_at(x, y, &has_warp, &dest_map, &dest_warp_idx);
    out->warp_mode = has_warp ? 1 : 0;
    out->dest_map = dest_map;
    out->dest_warp_idx = dest_warp_idx;
    return 1;
}

static int pks_tile_place_data_ex(const pks_saved_tile_t *data, int x, int y, int art_slot) {
    int slot;
    if (!data || !pks_game_coord_in_bounds(x, y)) return 0;
    slot = pks_tile_prop_find_slot(x, y);
    if (slot < 0) {
        slot = pks_tile_prop_alloc_slot();
        if (slot >= 0) {
            s_map_tile_prop_count[(uint8_t)wCurMap]++;
            pks_tile_prop_hash_insert((uint8_t)wCurMap, x, y, slot);
        }
    }
    if (slot < 0) return 0;
    s_tile_props[slot].used = 1;
    s_tile_props[slot].map_id = wCurMap;
    s_tile_props[slot].x = x;
    s_tile_props[slot].y = y;
    s_tile_props[slot].block_id = data->block_id;
    s_tile_props[slot].warp_mode = data->warp_mode ? 1 : 0;
    s_tile_props[slot].dest_map = data->dest_map;
    s_tile_props[slot].dest_warp_idx = data->dest_warp_idx;
    s_tile_props[slot].dest_map_is_named = data->dest_map_is_named;
    s_tile_props[slot].dest_is_last = data->dest_is_last;
    memcpy(s_tile_props[slot].dest_map_name, data->dest_map_name, sizeof(data->dest_map_name));
    s_tile_props[slot].art_slot = art_slot;
    memcpy(s_tile_props[slot].tiles, data->tiles, sizeof(data->tiles));
    return 1;
}

static int pks_tile_place_data(const pks_saved_tile_t *data, int x, int y) {
    return pks_tile_place_data_ex(data, x, y, -1);
}

int AmberScript_TileCopy(int sx, int sy, int dx, int dy) {
    pks_saved_tile_t tile;
    if (!pks_tile_capture_at(sx, sy, &tile)) return 0;
    if (!pks_tile_place_data(&tile, dx, dy)) return 0;
    Map_BuildScrollView();
    return 1;
}

int AmberScript_TileSaveRightOfPlayer(const char *name) {
    int slot;
    if (!name || !*name) return 0;
    slot = pks_saved_tile_alloc(name);
    if (slot < 0) return 0;
    if (!pks_tile_capture_at((int)wXCoord + 1, (int)wYCoord, &s_saved_tiles[slot])) return 0;
    s_saved_tiles[slot].used = 1;
    snprintf(s_saved_tiles[slot].name, sizeof(s_saved_tiles[slot].name), "%s", name);
    return 1;
}

int AmberScript_TilePlaceCustom(const char *name, int x, int y) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;

    if (!pks_tile_place_data_ex(&s_saved_tiles[slot], x, y, slot)) return 0;
    Map_BuildScrollView();
    return 1;
}

static int pks_tile_is_named_at(const char *name, int x, int y) {
    int want = pks_saved_tile_find(name);
    int slot;
    if (want < 0) return 0;
    slot = pks_tile_prop_find_slot(x, y);
    return slot >= 0 && s_tile_props[slot].used && s_tile_props[slot].art_slot == want;
}

int AmberScript_PlaceSwapBlock(const char *prefix, const char *state,
                               int bx, int by) {
    static const char *kCorner[4] = { "tl", "tr", "bl", "br" };

    const int cx = bx * 2, cy = by * 2;
    const int dx[4] = { 0, 1, 0, 1 };
    const int dy[4] = { 0, 0, 1, 1 };
    int ok = 1;

    {
        int settled = 1;
        for (int i = 0; i < 4 && settled; i++) {
            char name[96];
            snprintf(name, sizeof name, "%s_%s_%s", prefix, state, kCorner[i]);
            if (!pks_tile_is_named_at(name, cx + dx[i], cy + dy[i])) settled = 0;
        }
        if (settled) return 1;
    }
    for (int i = 0; i < 4; i++) {
        char name[96];
        snprintf(name, sizeof name, "%s_%s_%s", prefix, state, kCorner[i]);
        if (!AmberScript_TilePlaceCustom(name, cx + dx[i], cy + dy[i])) {
            printf("[amberscript] PlaceSwapBlock: '%s' not defined -- the "
                   "importer did not emit this alias. Regenerate with "
                   "tools/romimport/emit_kanto.py --all, or add the swap to "
                   "SCRIPT_TILE_SWAPS.\n", name);
            ok = 0;
        }
    }
    if (!ok) fflush(stdout);
    return ok;
}

int AmberScript_TilePlaceRawTile(uint8_t tile_id, int x, int y) {
    pks_saved_tile_t tile;
    memset(&tile, 0, sizeof(tile));
    tile.tiles[0] = tile.tiles[1] = tile.tiles[2] = tile.tiles[3] = tile_id;
    if (!pks_tile_place_data_ex(&tile, x, y, -1)) return 0;
    Map_BuildScrollView();
    return 1;
}

int AmberScript_BlockSave(const char *name, int sx, int sy, int ex, int ey) {
    int min_x = sx < ex ? sx : ex;
    int max_x = sx > ex ? sx : ex;
    int min_y = sy < ey ? sy : ey;
    int max_y = sy > ey ? sy : ey;
    int slot, count = 0;
    pks_saved_block_t tmp;
    if (!name || !*name) return 0;
    if ((max_x - min_x + 1) * (max_y - min_y + 1) > PKS_SAVED_BLOCK_CELL_MAX) return 0;
    slot = pks_saved_block_alloc(name);
    if (slot < 0) return 0;
    memset(&tmp, 0, sizeof(tmp));
    tmp.used = 1;
    snprintf(tmp.name, sizeof(tmp.name), "%s", name);
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            pks_saved_tile_t tile;
            pks_saved_block_cell_t *cell;
            if (!pks_tile_capture_at(x, y, &tile)) return 0;
            cell = &tmp.cells[count++];
            cell->dx = x - sx;
            cell->dy = y - sy;
            cell->block_id = tile.block_id;
            cell->warp_mode = tile.warp_mode;
            cell->dest_map = tile.dest_map;
            cell->dest_warp_idx = tile.dest_warp_idx;

            for (int k = 0; k < 4; k++) cell->tiles[k] = (uint8_t)tile.tiles[k];
        }
    }
    tmp.cell_count = count;
    s_saved_blocks[slot] = tmp;
    return 1;
}

int AmberScript_BlockPlaceCustom(const char *name, int x, int y) {
    int slot = AmberScript_SavedBlockFind(name);
    int needed = 0;
    if (slot < 0) return 0;
    if (s_saved_blocks[slot].cell_count <= 0) return 0;
    for (int i = 0; i < s_saved_blocks[slot].cell_count; i++) {
        int tx = x + s_saved_blocks[slot].cells[i].dx;
        int ty = y + s_saved_blocks[slot].cells[i].dy;
        if (!pks_game_coord_in_bounds(tx, ty)) return 0;
        if (pks_tile_prop_find_slot(tx, ty) < 0) needed++;
    }
    for (int i = 0; i < PKS_TILE_PROP_MAX && needed > 0; i++) {
        if (!s_tile_props[i].used) needed--;
    }
    if (needed > 0) return 0;
    for (int i = 0; i < s_saved_blocks[slot].cell_count; i++) {
        pks_saved_tile_t tile;
        pks_saved_block_cell_t *cell = &s_saved_blocks[slot].cells[i];
        memset(&tile, 0, sizeof(tile));
        tile.block_id = cell->block_id;
        tile.warp_mode = cell->warp_mode;
        tile.dest_map = cell->dest_map;
        tile.dest_warp_idx = cell->dest_warp_idx;

        for (int k = 0; k < 4; k++) tile.tiles[k] = cell->tiles[k];
        if (!pks_tile_place_data(&tile, x + cell->dx, y + cell->dy)) return 0;
    }
    Map_BuildScrollView();
    return 1;
}

int AmberScript_SavedBlockCellCount(int slot) {
    if (slot < 0 || slot >= PKS_SAVED_BLOCK_MAX || !s_saved_blocks[slot].used) return -1;
    return s_saved_blocks[slot].cell_count;
}

static int pks_tile_prop_effective_art_slot(int slot) {
    pks_tile_prop_t *t = &s_tile_props[slot];

    if (t->cond_event != 0 && (CheckEvent(t->cond_event) != 0) != (t->cond_negate != 0))
        return t->cond_art_slot;
    return t->art_slot;
}

static int pks_get_tile_override_for_map(uint8_t map_id, int tx, int ty, uint8_t *tile_id) {
    int gx = tx >> 1;
    int gy = ty >> 1;
    int slot, which;
    if (!tile_id) return 0;
    slot = pks_tile_prop_find_slot_for_map(map_id, gx, gy);
    if (slot < 0) return 0;
    if ((ty & 1) && (tx & 1)) which = 3;
    else if ((ty & 1) && !(tx & 1)) which = 2;
    else if (!(ty & 1) && (tx & 1)) which = 1;
    else which = 0;
    {
        int art_slot = pks_tile_prop_effective_art_slot(slot);

        if (art_slot >= 0 && art_slot < PKS_SAVED_TILE_MAX && s_saved_tiles[art_slot].used) {
            switch (s_saved_tiles[art_slot].source_kind) {
                case PKS_SRC_CUSTOM_ART:
                    *tile_id = pks_art_resolve_real_tile(art_slot, which);
                    break;
                case PKS_SRC_QUAD:

                    *tile_id = pks_subtile_cache_resolve(s_saved_tiles[art_slot].tiles[which]);
                    break;
                default:

                    *tile_id = (uint8_t)s_saved_tiles[art_slot].tiles[which];
                    break;
            }
        } else {
            *tile_id = s_tile_props[slot].tiles[which];
        }
    }
    return 1;
}

int AmberScript_GetTileOverrideAt(int tx, int ty, uint8_t *tile_id) {
    return pks_get_tile_override_for_map(wCurMap, tx, ty, tile_id);
}

int AmberScript_GetTileOverrideAtForMap(uint8_t map_id, int tx, int ty, uint8_t *tile_id) {
    return pks_get_tile_override_for_map(map_id, tx, ty, tile_id);
}

int AmberScript_ResolveNamedBlock(const char *name, uint8_t tiles[4], uint8_t *passable_out) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0 || !s_saved_tiles[slot].used) return 0;
    if (tiles) {
        for (int i = 0; i < 4; i++) {
            switch (s_saved_tiles[slot].source_kind) {
                case PKS_SRC_CUSTOM_ART:
                    tiles[i] = pks_art_resolve_real_tile(slot, i);
                    break;
                case PKS_SRC_QUAD:
                    tiles[i] = pks_subtile_cache_resolve(s_saved_tiles[slot].tiles[i]);
                    break;
                default:
                    tiles[i] = (uint8_t)s_saved_tiles[slot].tiles[i];
                    break;
            }
        }
    }
    if (passable_out) {
        *passable_out = (!s_saved_tiles[slot].art_passable_set || s_saved_tiles[slot].art_passable) ? 1 : 0;
    }
    return 1;
}

static int pks_get_passable_override_for_map(uint8_t map_id, int tx, int ty, uint8_t *passable) {
    int gx = tx >> 1;
    int gy = ty >> 1;
    int slot, art_slot;
    if (!passable) return 0;
    slot = pks_tile_prop_find_slot_for_map(map_id, gx, gy);
    if (slot < 0) return 0;
    art_slot = pks_tile_prop_effective_art_slot(slot);
    if (art_slot < 0 || art_slot >= PKS_SAVED_TILE_MAX || !s_saved_tiles[art_slot].used) return 0;
    if (!s_saved_tiles[art_slot].art_passable_set) return 0;
    *passable = s_saved_tiles[art_slot].art_passable ? 1 : 0;
    return 1;
}

int AmberScript_GetPassableOverrideAt(int tx, int ty, uint8_t *passable) {
    return pks_get_passable_override_for_map(wCurMap, tx, ty, passable);
}

void AmberScript_DebugDumpTilePropAt(int x, int y, char *out, size_t out_sz) {
    int slot = pks_tile_prop_find_slot_for_map(wCurMap, x, y);
    if (slot < 0) {
        snprintf(out, out_sz,
                 "no pks_tile_prop_t registered at map=%d (x=%d,y=%d) -- "
                 "this cell was never placed via map_edits/tile_place_custom",
                 (int)wCurMap, x, y);
        return;
    }
    {
        pks_tile_prop_t *t = &s_tile_props[slot];
        int eff_slot = pks_tile_prop_effective_art_slot(slot);
        int def_passable_set = (t->art_slot >= 0 && t->art_slot < PKS_SAVED_TILE_MAX && s_saved_tiles[t->art_slot].used)
                                ? s_saved_tiles[t->art_slot].art_passable_set : -1;
        int def_passable = def_passable_set > 0 ? s_saved_tiles[t->art_slot].art_passable : -1;
        int cond_passable_set = -1, cond_passable = -1;
        int event_true = t->cond_event ? CheckEvent(t->cond_event) : -1;
        if (t->cond_event && t->cond_art_slot >= 0 && t->cond_art_slot < PKS_SAVED_TILE_MAX &&
            s_saved_tiles[t->cond_art_slot].used) {
            cond_passable_set = s_saved_tiles[t->cond_art_slot].art_passable_set;
            cond_passable = cond_passable_set > 0 ? s_saved_tiles[t->cond_art_slot].art_passable : -1;
        }
        snprintf(out, out_sz,
                 "slot=%d map=%d(x=%d,y=%d) block_id=0x%02X art_slot=%d(name='%s' passable_set=%d passable=%d) "
                 "cond_event=%u(%s) negate=%d CheckEvent=%d cond_art_slot=%d(name='%s' passable_set=%d passable=%d) "
                 "-> EFFECTIVE art_slot=%d",
                 slot, (int)t->map_id, t->x, t->y, t->block_id,
                 t->art_slot, (t->art_slot >= 0 && t->art_slot < PKS_SAVED_TILE_MAX && s_saved_tiles[t->art_slot].used) ? s_saved_tiles[t->art_slot].name : "?",
                 def_passable_set, def_passable,
                 (unsigned)t->cond_event, t->cond_event ? EventFlagName(t->cond_event) : "none",
                 (int)t->cond_negate, event_true,
                 t->cond_art_slot, (t->cond_event && t->cond_art_slot >= 0 && t->cond_art_slot < PKS_SAVED_TILE_MAX && s_saved_tiles[t->cond_art_slot].used) ? s_saved_tiles[t->cond_art_slot].name : "?",
                 cond_passable_set, cond_passable,
                 eff_slot);
    }
}

static int pks_get_surfable_override_for_map(uint8_t map_id, int tx, int ty, uint8_t *surfable) {
    int gx = tx >> 1;
    int gy = ty >> 1;
    int slot, art_slot;
    if (!surfable) return 0;
    slot = pks_tile_prop_find_slot_for_map(map_id, gx, gy);
    if (slot < 0) return 0;
    art_slot = pks_tile_prop_effective_art_slot(slot);
    if (art_slot < 0 || art_slot >= PKS_SAVED_TILE_MAX || !s_saved_tiles[art_slot].used) return 0;
    if (!s_saved_tiles[art_slot].art_surfable_set) return 0;
    *surfable = s_saved_tiles[art_slot].art_surfable ? 1 : 0;
    return 1;
}

int AmberScript_GetSurfableOverrideAt(int tx, int ty, uint8_t *surfable) {
    return pks_get_surfable_override_for_map(wCurMap, tx, ty, surfable);
}

int AmberScript_GetSurfableOverrideAtForMap(uint8_t map_id, int tx, int ty, uint8_t *surfable) {
    return pks_get_surfable_override_for_map(map_id, tx, ty, surfable);
}

int AmberScript_GetCuttableOverrideAt(int tx, int ty, uint8_t *cuttable) {
    int gx = tx >> 1;
    int gy = ty >> 1;
    int slot, art_slot;
    if (!cuttable) return 0;
    slot = pks_tile_prop_find_slot(gx, gy);
    if (slot < 0) return 0;
    art_slot = pks_tile_prop_effective_art_slot(slot);
    if (art_slot < 0 || art_slot >= PKS_SAVED_TILE_MAX || !s_saved_tiles[art_slot].used) return 0;
    if (!s_saved_tiles[art_slot].art_cuttable_set) return 0;
    *cuttable = s_saved_tiles[art_slot].art_cuttable ? 1 : 0;
    return 1;
}

int AmberScript_GetCounterOverrideAt(int tx, int ty, uint8_t *counter) {
    int gx = tx >> 1;
    int gy = ty >> 1;
    int slot, art_slot;
    if (!counter) return 0;
    slot = pks_tile_prop_find_slot(gx, gy);
    if (slot < 0) return 0;
    art_slot = pks_tile_prop_effective_art_slot(slot);
    if (art_slot < 0 || art_slot >= PKS_SAVED_TILE_MAX || !s_saved_tiles[art_slot].used) return 0;
    if (!s_saved_tiles[art_slot].art_counter_set) return 0;
    *counter = s_saved_tiles[art_slot].art_counter ? 1 : 0;
    return 1;
}

int AmberScript_GetGrassOverrideAt(int tx, int ty, uint8_t *grass) {
    int gx = tx >> 1;
    int gy = ty >> 1;
    int slot, art_slot;
    if (!grass) return 0;
    slot = pks_tile_prop_find_slot(gx, gy);
    if (slot < 0) return 0;
    art_slot = pks_tile_prop_effective_art_slot(slot);
    if (art_slot < 0 || art_slot >= PKS_SAVED_TILE_MAX || !s_saved_tiles[art_slot].used) return 0;
    if (!s_saved_tiles[art_slot].art_grass_set) return 0;
    *grass = s_saved_tiles[art_slot].art_grass ? 1 : 0;
    return 1;
}

int AmberScript_GetGrassRustleOverrideAt(int tx, int ty, uint8_t *rustle) {
    int gx = tx >> 1;
    int gy = ty >> 1;
    int slot, art_slot;
    if (!rustle) return 0;
    slot = pks_tile_prop_find_slot(gx, gy);
    if (slot < 0) return 0;
    art_slot = pks_tile_prop_effective_art_slot(slot);
    if (art_slot < 0 || art_slot >= PKS_SAVED_TILE_MAX || !s_saved_tiles[art_slot].used) return 0;
    if (!s_saved_tiles[art_slot].art_grass_rustle_set) return 0;
    *rustle = s_saved_tiles[art_slot].art_grass_rustle ? 1 : 0;
    return 1;
}

int AmberScript_GetCutReplacementAt(int tx, int ty, char *out_name, size_t out_cap) {
    int gx = tx >> 1;
    int gy = ty >> 1;
    int slot, art_slot;
    slot = pks_tile_prop_find_slot(gx, gy);
    if (slot < 0) return 0;
    art_slot = s_tile_props[slot].art_slot;
    if (art_slot < 0 || art_slot >= PKS_SAVED_TILE_MAX || !s_saved_tiles[art_slot].used) return 0;
    if (!s_saved_tiles[art_slot].cut_replacement_name[0]) return 0;
    if (out_name) snprintf(out_name, out_cap, "%s", s_saved_tiles[art_slot].cut_replacement_name);
    return 1;
}

static int pks_get_pair_block_group(int tx, int ty, char *out_group, size_t out_cap) {
    int gx = tx >> 1;
    int gy = ty >> 1;
    int slot, art_slot;
    slot = pks_tile_prop_find_slot(gx, gy);
    if (slot < 0) return 0;
    art_slot = s_tile_props[slot].art_slot;
    if (art_slot < 0 || art_slot >= PKS_SAVED_TILE_MAX || !s_saved_tiles[art_slot].used) return 0;
    if (!s_saved_tiles[art_slot].pair_block_group[0]) return 0;
    if (out_group) snprintf(out_group, out_cap, "%s", s_saved_tiles[art_slot].pair_block_group);
    return 1;
}

int AmberScript_IsPairBlockedAt(int from_tx, int from_ty, int to_tx, int to_ty) {
    char from_group[32], to_group[32];
    if (!pks_get_pair_block_group(from_tx, from_ty, from_group, sizeof(from_group))) return 0;
    if (!pks_get_pair_block_group(to_tx, to_ty, to_group, sizeof(to_group))) return 0;
    return strcasecmp(from_group, to_group) != 0;
}

int AmberScript_GetLedgeOverrideAt(int tx, int ty, int *ledge_dirs) {
    int gx = tx >> 1;
    int gy = ty >> 1;
    int slot;
    if (!ledge_dirs) return 0;
    slot = pks_tile_prop_find_slot(gx, gy);
    if (slot < 0 || !s_tile_props[slot].ledge_dirs) return 0;
    *ledge_dirs = s_tile_props[slot].ledge_dirs;
    return 1;
}

int AmberScript_GetPassableOverrideAtForMap(uint8_t map_id, int tx, int ty, uint8_t *passable) {
    return pks_get_passable_override_for_map(map_id, tx, ty, passable);
}

int AmberScript_GetWarpOverrideAt(int x, int y, uint8_t *has_warp, uint8_t *dest_map, uint8_t *dest_warp_idx) {
    int slot = pks_tile_prop_find_slot(x, y);
    if (slot < 0) return 0;
    if (s_tile_props[slot].dest_map_is_named) {

        int real_id = AmberScript_MapBank_EnsureResidentByName(s_tile_props[slot].dest_map_name);
        if (real_id < 0) return 0;
        if (has_warp) *has_warp = s_tile_props[slot].warp_mode ? 1 : 0;
        if (dest_map) *dest_map = (uint8_t)real_id;
        if (dest_warp_idx) *dest_warp_idx = s_tile_props[slot].dest_warp_idx;
        return 1;
    }
    if (s_tile_props[slot].dest_is_last) {

        if (has_warp) *has_warp = s_tile_props[slot].warp_mode ? 1 : 0;
        if (dest_map) *dest_map = wLastMap;
        if (dest_warp_idx) *dest_warp_idx = s_tile_props[slot].dest_warp_idx;
        return 1;
    }
    if (has_warp) *has_warp = s_tile_props[slot].warp_mode ? 1 : 0;
    if (dest_map) *dest_map = s_tile_props[slot].dest_map;
    if (dest_warp_idx) *dest_warp_idx = s_tile_props[slot].dest_warp_idx;
    return 1;
}

int AmberScript_GetWarpOverrideDestNameAt(int x, int y, char *out_name, size_t out_cap) {
    int slot = pks_tile_prop_find_slot(x, y);
    if (slot < 0 || !s_tile_props[slot].dest_map_is_named) return 0;
    if (out_name) snprintf(out_name, out_cap, "%s", s_tile_props[slot].dest_map_name);
    return 1;
}

const char *AmberScript_GetSignTextAt(int x, int y) {
    int slot = pks_tile_prop_find_slot(x, y);
    if (slot < 0 || !s_tile_props[slot].sign_text[0]) return NULL;
    return s_tile_props[slot].sign_text;
}

void AmberScript_ClearTileOverrides(uint8_t map_id) {

    if (AmberScript_MapBank_HasStreamedForRealId(map_id) == 1) return;
    if (s_map_tile_prop_count[map_id] == 0) return;
    for (int i = 0; i < PKS_TILE_PROP_MAX; i++) {
        if (s_tile_props[i].used && s_tile_props[i].map_id == map_id)
            memset(&s_tile_props[i], 0, sizeof(s_tile_props[i]));
    }
    s_map_tile_prop_count[map_id] = 0;

    pks_tile_prop_hash_rebuild();
}

void AmberScript_DebugTilePropCount(uint8_t map_id, int *counter_value, int *real_count, int *total_used) {
    int real = 0, total = 0;
    for (int i = 0; i < PKS_TILE_PROP_MAX; i++) {
        if (!s_tile_props[i].used) continue;
        total++;
        if (s_tile_props[i].map_id == map_id) real++;
    }
    if (counter_value) *counter_value = s_map_tile_prop_count[map_id];
    if (real_count) *real_count = real;
    if (total_used) *total_used = total;
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

int AmberScript_ParseCoordExpr(const char *tok, int is_x, int *out) {
    const char *base = is_x ? "player.x" : "player.y";
    const char *base_typo = is_x ? "play.x" : "play.y";
    const char *base_short = "player";
    if (!tok || !out) return 0;
    if (pks_is_numeric_token(tok)) {
        *out = (int)strtol(tok, NULL, 0);
        return 1;
    }
    if (strncmp(tok, base, strlen(base)) == 0) {
        int v = is_x ? (int)wXCoord : (int)wYCoord;
        const char *p = tok + strlen(base);
        if (*p == '\0') { *out = v; return 1; }
        if ((*p == '+' || *p == '-') && pks_is_numeric_token(p + 1)) {
            int off = (int)strtol(p + 1, NULL, 0);
            if (*p == '-') off = -off;
            *out = v + off;
            return 1;
        }
    }
    if (strncmp(tok, base_typo, strlen(base_typo)) == 0) {
        int v = is_x ? (int)wXCoord : (int)wYCoord;
        const char *p = tok + strlen(base_typo);
        if (*p == '\0') { *out = v; return 1; }
        if ((*p == '+' || *p == '-') && pks_is_numeric_token(p + 1)) {
            int off = (int)strtol(p + 1, NULL, 0);
            if (*p == '-') off = -off;
            *out = v + off;
            return 1;
        }
    }
    if (strncmp(tok, base_short, strlen(base_short)) == 0) {
        int v = is_x ? (int)wXCoord : (int)wYCoord;
        const char *p = tok + strlen(base_short);
        if (*p == '\0') { *out = v; return 1; }
        if ((*p == '+' || *p == '-') && pks_is_numeric_token(p + 1)) {
            int off = (int)strtol(p + 1, NULL, 0);
            if (*p == '-') off = -off;
            *out = v + off;
            return 1;
        }
    }
    return 0;
}

int AmberScript_ParseCoordExprOrAny(const char *tok, int is_x, int *out) {
    if (!tok || !out) return 0;
    if (strcmp(tok, "any") == 0) { *out = -1; return 1; }
    return AmberScript_ParseCoordExpr(tok, is_x, out);
}

void AmberScript_NormalizeCoordArgs(const char *src, char *dst, size_t dst_sz) {
    size_t di = 0;
    char prev = '\0';
    if (!dst || dst_sz == 0) return;
    if (!src) src = "";
    for (size_t si = 0; src[si] && di + 1 < dst_sz; si++) {
        char c = src[si];
        if (c == ',') c = ' ';
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            size_t ni = si + 1;
            while (src[ni] == ' ' || src[ni] == '\t') ni++;
            if (src[ni] == '+' || src[ni] == '-' || src[ni] == ',') continue;
            if (prev == '+' || prev == '-') continue;
            if (di > 0 && dst[di - 1] != ' ') {
                dst[di++] = ' ';
                prev = ' ';
            }
            continue;
        }
        if ((c == '+' || c == '-') && di > 0 && dst[di - 1] == ' ')
            di--;
        dst[di++] = c;
        prev = c;
    }
    while (di > 0 && dst[di - 1] == ' ') di--;
    dst[di] = '\0';
}

int AmberScript_ParseBlockSaveArgs(const char *args, char *name, size_t name_sz,
                                      int *sx, int *sy, int *ex, int *ey) {
    char norm[256];
    char t0[32], t1[32], t2[32], t3[32], t4[32], t5[32], t6[32];
    AmberScript_NormalizeCoordArgs(args, norm, sizeof(norm));
    if (sscanf(norm, "%31s %31s %31s %31s %31s %31s %31s", t0, t1, t2, t3, t4, t5, t6) != 7)
        return 0;
    if (strcmp(t1, "start") != 0 || strcmp(t4, "end") != 0) return 0;
    if (!AmberScript_ParseCoordExpr(t2, 1, sx) || !AmberScript_ParseCoordExpr(t3, 0, sy)) return 0;
    if (!AmberScript_ParseCoordExpr(t5, 1, ex) || !AmberScript_ParseCoordExpr(t6, 0, ey)) return 0;
    snprintf(name, name_sz, "%s", t0);
    return 1;
}

int AmberScript_ParseNamedCoordArgs(const char *args, char *name, size_t name_sz,
                                       int *x, int *y) {
    char norm[192];
    char t0[32], t1[32], t2[32];
    AmberScript_NormalizeCoordArgs(args, norm, sizeof(norm));
    if (sscanf(norm, "%31s %31s %31s", t0, t1, t2) != 3) return 0;
    if (!AmberScript_ParseCoordExpr(t1, 1, x) || !AmberScript_ParseCoordExpr(t2, 0, y)) return 0;
    snprintf(name, name_sz, "%s", t0);
    return 1;
}

static int pks_resolve_input_path(const char *spec, char *out_path, size_t out_sz) {
    static const char *kPathFmt[] = {
        "%s",
        "../%s",
        "mod_runtime/custom_art/%s",
        "../mod_runtime/custom_art/%s",
    };
    for (int pi = 0; pi < (int)(sizeof(kPathFmt) / sizeof(kPathFmt[0])); pi++) {
        FILE *probe;
        snprintf(out_path, out_sz, kPathFmt[pi], spec);
        probe = fopen(out_path, "rb");
        if (probe) { fclose(probe); return 1; }
    }
    out_path[0] = '\0';
    return 0;
}

static void pks_derive_bin_path(const char *png_path, char *out_path, size_t out_sz) {
    size_t len = strlen(png_path);
    if (len >= 4) {
        const char *ext = png_path + len - 4;
        char lc[5];
        for (int i = 0; i < 4; i++) lc[i] = (char)tolower((unsigned char)ext[i]);
        lc[4] = '\0';
        if (strcmp(lc, ".png") == 0) {
            size_t base_len = len - 4;
            if (base_len >= out_sz) base_len = out_sz - 1;
            memcpy(out_path, png_path, base_len);
            snprintf(out_path + base_len, out_sz - base_len, ".bin");
            return;
        }
    }
    snprintf(out_path, out_sz, "%s.bin", png_path);
}

static int pks_find_python_runner(char *out_path, size_t out_sz) {
    static const char *kCandidates[] = {
#ifdef _WIN32
        "C:/Progra~1/Python311/python.exe",
#else
        "/usr/bin/python3",
        "/usr/local/bin/python3",
        "/bin/python3",
#endif
        NULL
    };
    for (int i = 0; kCandidates[i]; i++) {
        FILE *probe = fopen(kCandidates[i], "r");
        if (probe) {
            fclose(probe);
            snprintf(out_path, out_sz, "%s", kCandidates[i]);
            return 1;
        }
    }
    {
        char line[256];
#ifdef _WIN32
        FILE *wf = PKS_TM_POPEN("where python 2>nul", "r");
#else
        FILE *wf = PKS_TM_POPEN("command -v python3 2>/dev/null", "r");
#endif
        if (wf) {
            if (fgets(line, sizeof(line), wf)) {
                size_t n = strlen(line);
                while (n > 0 && (line[n-1] == '\r' || line[n-1] == '\n')) line[--n] = '\0';
                if (line[0]) { snprintf(out_path, out_sz, "%s", line); PKS_TM_PCLOSE(wf); return 1; }
            }
            PKS_TM_PCLOSE(wf);
        }
    }
    return 0;
}

static int pks_convert_png_to_bin(const char *png_path, const char *bin_path) {
    static const char *kScriptFmt[] = {
        "mod_runtime/python/png_to_gb2bpp.py",
        "../mod_runtime/python/png_to_gb2bpp.py"
    };
    char runner[320] = {0};
    char script_path[320] = {0};
    char cmdline[900];
    FILE *fp;
    int rc;

    for (int i = 0; i < (int)(sizeof(kScriptFmt) / sizeof(kScriptFmt[0])); i++) {
        FILE *probe = fopen(kScriptFmt[i], "r");
        if (probe) { fclose(probe); snprintf(script_path, sizeof(script_path), "%s", kScriptFmt[i]); break; }
    }
    if (!script_path[0]) {
        printf("[amberscript] tile_art_load: png_to_gb2bpp.py not found\n");
        return 0;
    }
    if (!pks_find_python_runner(runner, sizeof(runner))) {
        printf("[amberscript] tile_art_load: no python runner found\n");
        return 0;
    }
#ifdef _WIN32
    _putenv_s("COMSPEC", "C:\\Windows\\System32\\cmd.exe");
#endif

    snprintf(cmdline, sizeof(cmdline), "\"\"%s\" \"%s\" \"%s\" \"%s\" 2>&1\"",
             runner, script_path, png_path, bin_path);
    fp = PKS_TM_POPEN(cmdline, "r");
    if (!fp) {
        printf("[amberscript] tile_art_load: failed to launch converter\n");
        return 0;
    }
    {
        char out_line[256];
        while (fgets(out_line, sizeof(out_line), fp)) {
            printf("[amberscript] %s", out_line);
        }
    }
    rc = PKS_TM_PCLOSE(fp);
    return rc == 0;
}

static void pks_write_loaded_assets_manifest(void) {
    char path[300];
    FILE *f;
    if (!pks_resolve_output_path("custom_art", "loaded_assets", "txt", path, sizeof(path))) return;
    f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < PKS_SAVED_TILE_MAX; i++) {
        if (s_saved_tiles[i].used && s_saved_tiles[i].source_kind == PKS_SRC_CUSTOM_ART)
            fprintf(f, "%s\t%s\n", s_saved_tiles[i].name, s_saved_tiles[i].art_src);
    }
    fclose(f);
}

int AmberScript_LoadCustomTileArt(const char *name, const char *png_path) {
    char resolved_png[320] = {0};
    char bin_path[330] = {0};
    struct stat png_stat, bin_stat;
    int need_convert = 1;
    FILE *bf;
    uint8_t blob[64];
    int slot;

    if (!name || !*name || !png_path || !*png_path) return 0;
    if (!pks_resolve_input_path(png_path, resolved_png, sizeof(resolved_png))) {
        printf("[amberscript] tile_art_load: PNG not found '%s'\n", png_path);
        return 0;
    }
    pks_derive_bin_path(resolved_png, bin_path, sizeof(bin_path));

    if (stat(resolved_png, &png_stat) == 0 && stat(bin_path, &bin_stat) == 0) {
        if (bin_stat.st_mtime >= png_stat.st_mtime) need_convert = 0;
    }
    if (need_convert) {
        if (!pks_convert_png_to_bin(resolved_png, bin_path)) {
            printf("[amberscript] tile_art_load: conversion failed for '%s'\n", resolved_png);
            return 0;
        }
    }

    bf = fopen(bin_path, "rb");
    if (!bf) {
        printf("[amberscript] tile_art_load: cannot open converted blob '%s'\n", bin_path);
        return 0;
    }
    {
        size_t got = fread(blob, 1, sizeof(blob), bf);
        fclose(bf);
        if (got != sizeof(blob)) {
            printf("[amberscript] tile_art_load: blob '%s' is %lu bytes, expected 64\n", bin_path, (unsigned long)got);
            return 0;
        }
    }

    slot = pks_saved_tile_alloc(name);
    if (slot < 0) {
        printf("[amberscript] tile_art_load: no free saved-tile slots (max %d)\n", PKS_SAVED_TILE_MAX);
        return 0;
    }

    if (s_saved_tiles[slot].source_kind != PKS_SRC_CUSTOM_ART) memset(&s_saved_tiles[slot], 0, sizeof(s_saved_tiles[slot]));
    s_saved_tiles[slot].used = 1;
    snprintf(s_saved_tiles[slot].name, sizeof(s_saved_tiles[slot].name), "%s", name);
    s_saved_tiles[slot].source_kind = PKS_SRC_CUSTOM_ART;
    for (int i = 0; i < 4; i++) memcpy(s_saved_tiles[slot].art_pixels[i], blob + i * 16, 16);
    s_saved_tiles[slot].block_id = 0;
    snprintf(s_saved_tiles[slot].art_src, sizeof(s_saved_tiles[slot].art_src), "%s", resolved_png);

    pks_write_loaded_assets_manifest();

    printf("[amberscript] tile_art_load: '%s' <- %s (4 tiles, virtual bank slot %d)\n",
           name, resolved_png, slot);
    return 1;
}

static int s_gbc_tileset_ctx = -1;

static int s_gbc_map_ctx = -1;

static int pks_tileset_id_from_name(const char *s) {
    static const struct { const char *name; int id; } kNames[] = {
        { "overworld",    TILESET_OVERWORLD   },
        { "reds_house_1", TILESET_REDS_HOUSE1 },
        { "reds_house",   TILESET_REDS_HOUSE1 },
        { "mart",         TILESET_MART        },
        { "forest",       TILESET_FOREST      },
        { "reds_house_2", TILESET_REDS_HOUSE2 },
        { "dojo",         TILESET_DOJO        },
        { "pokecenter",   TILESET_POKECENTER  },
        { "gym",          TILESET_GYM         },
        { "house",        TILESET_HOUSE       },
        { "forest_gate",  TILESET_FOREST_GATE },
        { "museum",       TILESET_MUSEUM      },
        { "underground",  TILESET_UNDERGROUND },
        { "gate",         TILESET_GATE        },
        { "ship",         TILESET_SHIP        },
        { "ship_port",    TILESET_SHIP_PORT   },
        { "cemetery",     TILESET_CEMETERY    },
        { "interior",     TILESET_INTERIOR    },
        { "cavern",       TILESET_CAVERN      },
        { "lobby",        TILESET_LOBBY       },
        { "mansion",      TILESET_MANSION     },
        { "lab",          TILESET_LAB         },
        { "club",         TILESET_CLUB        },
        { "facility",     TILESET_FACILITY    },
        { "plateau",      TILESET_PLATEAU     },
    };
    if (!s || !*s) return -1;
    for (unsigned i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++)
        if (strcmp(s, kNames[i].name) == 0) return kNames[i].id;
    return -1;
}

static int pks_subtile_tile_index(const char *name) {
    const char *p, *last = NULL;
    if (!name) return -1;
    for (p = name; *p; p++)
        if (p[0] == '_' && p[1] == 't' && p[2] >= '0' && p[2] <= '9') last = p;
    if (!last) return -1;
    {
        int v = 0;
        for (p = last + 2; *p; p++) {
            if (*p < '0' || *p > '9') return -1;
            v = v * 10 + (*p - '0');
            if (v > 0xFFFF) return -1;
        }
        return v;
    }
}

int AmberScript_SubtileSetPalette(const char *name, int pal) {
    int slot;
    if (!name || !*name || pal < 0 || pal > 7) return 0;
    slot = pks_subtile_find(name);
    if (slot < 0) return 0;
    s_subtiles[slot].gbc_attr = (uint8_t)pal;
    s_subtiles[slot].gbc_attr_set = 1;
    return 1;
}

static void pks_subtile_assign_gbc_attr(int slot) {
    int idx;
    if (slot < 0 || slot >= s_subtiles_cap) return;
    if (s_gbc_tileset_ctx < 0) return;
    idx = pks_subtile_tile_index(s_subtiles[slot].name);
    if (idx < 0 || idx >= GBC_TILESET_SIZE) return;
    s_subtiles[slot].gbc_attr =
        GbcColor_AttrForTileOnMap(s_gbc_tileset_ctx, idx, s_gbc_map_ctx);
    s_subtiles[slot].gbc_attr_set = 1;
}

int AmberScript_LoadSubtileArt(const char *name, const char *png_path) {
    char resolved_png[320] = {0};
    char bin_path[330] = {0};
    struct stat png_stat, bin_stat;
    int need_convert = 1;
    FILE *bf;
    uint8_t blob[16];
    int slot;

    if (!name || !*name || !png_path || !*png_path) return 0;

    {
        int existing = pks_subtile_find(name);
        if (existing >= 0) {

            if (!s_subtiles[existing].gbc_attr_set)
                pks_subtile_assign_gbc_attr(existing);
            return 1;
        }
    }
    if (!pks_resolve_input_path(png_path, resolved_png, sizeof(resolved_png))) {
        printf("[amberscript] subtile_load: PNG not found '%s'\n", png_path);
        return 0;
    }
    pks_derive_bin_path(resolved_png, bin_path, sizeof(bin_path));

    if (stat(resolved_png, &png_stat) == 0 && stat(bin_path, &bin_stat) == 0) {
        if (bin_stat.st_mtime >= png_stat.st_mtime) need_convert = 0;
    }
    if (need_convert) {
        if (!pks_convert_png_to_bin(resolved_png, bin_path)) {
            printf("[amberscript] subtile_load: conversion failed for '%s'\n", resolved_png);
            return 0;
        }
    }

    bf = fopen(bin_path, "rb");
    if (!bf) {
        printf("[amberscript] subtile_load: cannot open converted blob '%s'\n", bin_path);
        return 0;
    }
    {
        size_t got = fread(blob, 1, sizeof(blob), bf);
        fclose(bf);
        if (got != sizeof(blob)) {
            printf("[amberscript] subtile_load: blob '%s' is %lu bytes, expected 16\n", bin_path, (unsigned long)got);
            return 0;
        }
    }

    slot = pks_subtile_alloc(name);
    if (slot < 0) {
        printf("[amberscript] subtile_load: allocation failed (out of memory growing the subtile bank)\n");
        return 0;
    }
    s_subtiles[slot].used = 1;
    snprintf(s_subtiles[slot].name, sizeof(s_subtiles[slot].name), "%s", name);
    memcpy(s_subtiles[slot].pixels, blob, 16);
    pks_kanto_anim_register(slot);
    pks_subtile_assign_gbc_attr(slot);

    printf("[amberscript] subtile_load: '%s' <- %s (1 tile, subtile bank slot %d)\n", name, resolved_png, slot);
    return 1;
}

void AmberScript_TileMod_PreloadIndoorSubtiles(void) {
    const char *dir = "mod_runtime/blocks";
    DIR *d = opendir(dir);
    if (!d) return;

    static char names[512][64];
    static char paths[512][256];
    struct dirent *ent;
    int warmed = 0, maps = 0;
    while ((ent = readdir(d)) != NULL) {
        const char *nm = ent->d_name;
        size_t len = strlen(nm);
        if (len < 6 || strcmp(nm + len - 6, ".block") != 0) continue;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, nm);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        int n = 0, indoor = 0;
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "indoor ", 7) == 0 || strcmp(line, "indoor\n") == 0) {
                indoor = 1;
            } else if (strncmp(line, "subtile ", 8) == 0 && n < 512) {
                if (sscanf(line + 8, "%63s %255s", names[n], paths[n]) == 2) n++;
            }
        }
        fclose(f);
        if (!indoor) continue;
        for (int i = 0; i < n; i++)
            if (AmberScript_LoadSubtileArt(names[i], paths[i])) warmed++;
        maps++;
    }
    closedir(d);
    printf("[amberscript] PreloadIndoorSubtiles: warmed %d subtile(s) across %d indoor map(s)\n",
           warmed, maps);
}

int AmberScript_TileSetPassable(const char *name, int passable) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    s_saved_tiles[slot].art_passable = passable ? 1 : 0;
    s_saved_tiles[slot].art_passable_set = 1;
    return 1;
}

int AmberScript_TileSetWarp(const char *name, int dest_map, int dest_warp_idx) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    if (dest_map < 0 || dest_map >= NUM_MAPS || dest_warp_idx < 0 || dest_warp_idx > 255) return 0;
    s_saved_tiles[slot].warp_mode = 1;
    s_saved_tiles[slot].dest_map = (uint8_t)dest_map;
    s_saved_tiles[slot].dest_warp_idx = (uint8_t)dest_warp_idx;
    s_saved_tiles[slot].dest_map_is_named = 0;
    s_saved_tiles[slot].dest_is_last = 0;
    s_saved_tiles[slot].dest_map_name[0] = '\0';
    return 1;
}

int AmberScript_TileSetWarpLast(const char *name, int dest_warp_idx) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    if (dest_warp_idx < 0 || dest_warp_idx > 255) return 0;
    s_saved_tiles[slot].warp_mode = 1;
    s_saved_tiles[slot].dest_map = 0;
    s_saved_tiles[slot].dest_warp_idx = (uint8_t)dest_warp_idx;
    s_saved_tiles[slot].dest_map_is_named = 0;
    s_saved_tiles[slot].dest_is_last = 1;
    s_saved_tiles[slot].dest_map_name[0] = '\0';
    return 1;
}

int AmberScript_TileSetWarpNamed(const char *name, const char *dest_vmap_name, int dest_warp_idx) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    if (!dest_vmap_name || !*dest_vmap_name || dest_warp_idx < 0 || dest_warp_idx > 255) return 0;
    if (!AmberScript_MapBank_RegisterName(dest_vmap_name)) return 0;
    s_saved_tiles[slot].warp_mode = 1;
    s_saved_tiles[slot].dest_map = 0;
    s_saved_tiles[slot].dest_warp_idx = (uint8_t)dest_warp_idx;
    s_saved_tiles[slot].dest_map_is_named = 1;
    s_saved_tiles[slot].dest_is_last = 0;
    snprintf(s_saved_tiles[slot].dest_map_name, sizeof(s_saved_tiles[slot].dest_map_name), "%s", dest_vmap_name);
    return 1;
}

int AmberScript_TileClearWarp(const char *name) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    s_saved_tiles[slot].warp_mode = 0;
    s_saved_tiles[slot].dest_map = 0;
    s_saved_tiles[slot].dest_warp_idx = 0;
    s_saved_tiles[slot].dest_map_is_named = 0;
    s_saved_tiles[slot].dest_is_last = 0;
    s_saved_tiles[slot].dest_map_name[0] = '\0';
    return 1;
}

int AmberScript_TileClearPassable(const char *name) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    s_saved_tiles[slot].art_passable_set = 0;
    s_saved_tiles[slot].art_passable = 0;
    return 1;
}

int AmberScript_TileSetSurfable(const char *name, int surfable) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    s_saved_tiles[slot].art_surfable = surfable ? 1 : 0;
    s_saved_tiles[slot].art_surfable_set = 1;
    return 1;
}

int AmberScript_TileClearSurfable(const char *name) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    s_saved_tiles[slot].art_surfable_set = 0;
    s_saved_tiles[slot].art_surfable = 0;
    return 1;
}

int AmberScript_TileSetCuttable(const char *name, int cuttable) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    s_saved_tiles[slot].art_cuttable = cuttable ? 1 : 0;
    s_saved_tiles[slot].art_cuttable_set = 1;
    return 1;
}

int AmberScript_TileClearCuttable(const char *name) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    s_saved_tiles[slot].art_cuttable_set = 0;
    s_saved_tiles[slot].art_cuttable = 0;
    return 1;
}

int AmberScript_TileSetCounter(const char *name, int counter) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    s_saved_tiles[slot].art_counter = counter ? 1 : 0;
    s_saved_tiles[slot].art_counter_set = 1;
    return 1;
}

int AmberScript_TileClearCounter(const char *name) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    s_saved_tiles[slot].art_counter_set = 0;
    s_saved_tiles[slot].art_counter = 0;
    return 1;
}

int AmberScript_TileSetCutReplacement(const char *name, const char *replacement_name) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0 || !replacement_name) return 0;
    snprintf(s_saved_tiles[slot].cut_replacement_name, sizeof(s_saved_tiles[slot].cut_replacement_name), "%s", replacement_name);
    return 1;
}

int AmberScript_TileSetGrass(const char *name, int grass) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    s_saved_tiles[slot].art_grass = grass ? 1 : 0;
    s_saved_tiles[slot].art_grass_set = 1;
    return 1;
}

int AmberScript_TileClearGrass(const char *name) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    s_saved_tiles[slot].art_grass_set = 0;
    s_saved_tiles[slot].art_grass = 0;
    return 1;
}

int AmberScript_TileSetCutSpanBlock(const char *name, int block_wide) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    s_saved_tiles[slot].art_cut_span_block = block_wide ? 1 : 0;
    return 1;
}

int AmberScript_GetCutSpanBlockAt(int tx, int ty) {
    int gx = tx >> 1;
    int gy = ty >> 1;
    int slot = pks_tile_prop_find_slot(gx, gy);
    int art_slot;
    if (slot < 0) return 0;
    art_slot = pks_tile_prop_effective_art_slot(slot);
    if (art_slot < 0 || art_slot >= PKS_SAVED_TILE_MAX || !s_saved_tiles[art_slot].used) return 0;
    return s_saved_tiles[art_slot].art_cut_span_block ? 1 : 0;
}

int AmberScript_TileSetGrassRustle(const char *name, int rustle) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    s_saved_tiles[slot].art_grass_rustle = rustle ? 1 : 0;
    s_saved_tiles[slot].art_grass_rustle_set = 1;
    return 1;
}

int AmberScript_TileClearGrassRustle(const char *name) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0) return 0;
    s_saved_tiles[slot].art_grass_rustle_set = 0;
    s_saved_tiles[slot].art_grass_rustle = 0;
    return 1;
}

int AmberScript_TileSetPairBlockGroup(const char *name, const char *group_name) {
    int slot = pks_saved_tile_find(name);
    if (slot < 0 || !group_name) return 0;
    snprintf(s_saved_tiles[slot].pair_block_group, sizeof(s_saved_tiles[slot].pair_block_group), "%s", group_name);
    return 1;
}

int AmberScript_TileSetLedgeAt(int x, int y, int ledge_dirs) {
    int slot = pks_tile_prop_find_slot(x, y);
    if (slot < 0) return 0;
    s_tile_props[slot].ledge_dirs = ledge_dirs;
    return 1;
}

int AmberScript_TileSetSign(int x, int y, const char *text) {
    int slot = pks_tile_prop_find_slot(x, y);
    char *out;
    size_t out_cap, n;
    const char *p;
    if (slot < 0 || !text) return 0;

    out = s_tile_props[slot].sign_text;
    out_cap = sizeof(s_tile_props[slot].sign_text);
    n = 0;
    for (p = text; *p && n + 1 < out_cap; p++) {
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
    return 1;
}

int AmberScript_TileSetConditionalEx(int x, int y, const char *event_name,
                                     const char *alt_block_name, int negate) {
    const char *kw = negate ? "tile_if_not" : "tile_if";
    int slot = pks_tile_prop_find_slot(x, y);
    int alt_slot;
    uint16_t event_id;
    if (slot < 0) {
        printf("[amberscript] %s: no placed cell at (%d,%d) to attach to\n", kw, x, y);
        return 0;
    }
    if (!EventFlagIdByName(event_name, &event_id)) {
        printf("[amberscript] %s: '%s' isn't a recognized EVENT_* flag name\n", kw,
               event_name ? event_name : "(null)");
        return 0;
    }
    alt_slot = pks_saved_tile_find(alt_block_name);
    if (alt_slot < 0) {
        printf("[amberscript] %s: '%s' isn't a defined block name\n", kw,
               alt_block_name ? alt_block_name : "(null)");
        return 0;
    }
    s_tile_props[slot].cond_event = event_id;
    s_tile_props[slot].cond_art_slot = alt_slot;
    s_tile_props[slot].cond_negate = (uint8_t)(negate != 0);
    return 1;
}

int AmberScript_TileSetConditional(int x, int y, const char *event_name, const char *alt_block_name) {
    return AmberScript_TileSetConditionalEx(x, y, event_name, alt_block_name, 0);
}

int AmberScript_DefineOriginalTile(const char *name, uint8_t tile_id) {
    int slot;
    if (!name || !*name) return 0;
    slot = pks_saved_tile_alloc(name);
    if (slot < 0) return 0;
    if (s_saved_tiles[slot].source_kind == PKS_SRC_CUSTOM_ART) memset(&s_saved_tiles[slot], 0, sizeof(s_saved_tiles[slot]));
    s_saved_tiles[slot].used = 1;
    snprintf(s_saved_tiles[slot].name, sizeof(s_saved_tiles[slot].name), "%s", name);
    s_saved_tiles[slot].source_kind = PKS_SRC_DIRECT;
    s_saved_tiles[slot].tiles[0] = s_saved_tiles[slot].tiles[1] =
        s_saved_tiles[slot].tiles[2] = s_saved_tiles[slot].tiles[3] = tile_id;
    return 1;
}

int AmberScript_DefineQuadTile(const char *name, const char *sub_tl, const char *sub_tr,
                               const char *sub_bl, const char *sub_br) {
    int slot;
    int subs[4];
    const char *names[4] = {sub_tl, sub_tr, sub_bl, sub_br};
    if (!name || !*name) return 0;
    for (int i = 0; i < 4; i++) {
        int s = pks_subtile_find(names[i]);
        if (s < 0) return 0;
        subs[i] = s;
    }
    slot = pks_saved_tile_alloc(name);
    if (slot < 0) return 0;
    if (s_saved_tiles[slot].source_kind == PKS_SRC_CUSTOM_ART) memset(&s_saved_tiles[slot], 0, sizeof(s_saved_tiles[slot]));
    s_saved_tiles[slot].used = 1;
    snprintf(s_saved_tiles[slot].name, sizeof(s_saved_tiles[slot].name), "%s", name);
    s_saved_tiles[slot].source_kind = PKS_SRC_QUAD;
    for (int i = 0; i < 4; i++)
        s_saved_tiles[slot].tiles[i] = (uint16_t)subs[i];
    return 1;
}

static int pks_parse_bool(const char *s) {
    return strcmp(s, "yes") == 0 || strcmp(s, "true") == 0 || strcmp(s, "1") == 0;
}

int AmberScript_BlockDefLoad(const char *path) {
    char resolved[320];
    FILE *f;

    char line[3 * PKS_MAX_TEXT + 256];
    char cur_name[32] = {0};
    int mode = 0;
    int blocks_seen = 0, tilesets_seen = 0;

    if (!path || !*path) return -1;
    if (!pks_resolve_input_path(path, resolved, sizeof(resolved))) return -1;
    f = fopen(resolved, "r");
    if (!f) return -1;

    AmberScript_ResetLastDecl();

    s_gbc_tileset_ctx = -1;
    s_gbc_map_ctx = -1;

    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        size_t len;

        {
            size_t raw = strlen(line);
            if (raw == sizeof(line) - 1 && line[raw - 1] != '\n') {
                int c;
                printf("[amberscript] block_def_load: '%s' line too long "
                       "(>%u chars), SKIPPED: '%.60s...'\n",
                       resolved, (unsigned)(sizeof(line) - 1), line);
                fflush(stdout);
                while ((c = fgetc(f)) != EOF && c != '\n') { }
                continue;
            }
        }
        while (*p == ' ' || *p == '\t') p++;
        len = strlen(p);
        while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r' ||
                            p[len - 1] == ' ' || p[len - 1] == '\t')) {
            p[--len] = '\0';
        }
        if (!*p || *p == '#') continue;

        if (mode == 0 && strncmp(p, "subtile ", 8) == 0) {
            char t0[32] = {0}, t1[PKS_MAX_TEXT] = {0}, t2[24] = {0};

            int n = sscanf(p + 8, "%31s %199s %23s", t0, t1, t2);
            if (n >= 2) {
                if (!AmberScript_LoadSubtileArt(t0, t1))
                    printf("[amberscript] block_def_load: subtile '%s' failed: %s\n", t0, t1);
                else if (n == 3 && strncmp(t2, "pal:", 4) == 0)
                    AmberScript_SubtileSetPalette(t0, atoi(t2 + 4));
            } else {
                printf("[amberscript] block_def_load: malformed subtile line: '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "crystal_env ", 12) == 0) {

            char name[32] = {0}, es[16] = {0}, gs[16] = {0};
            if (sscanf(p + 12, "%31s %15s %15s", name, es, gs) == 3) {
                if (!AmberScript_MapSetCrystalEnv(name, atoi(es), atoi(gs)))
                    printf("[amberscript] block_def_load: crystal_env line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed crystal_env line "
                       "(want: crystal_env <map> <environment> <group>): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "crystal_anim ", 13) == 0) {

            char name[32] = {0}, ts[16] = {0}, slug[24] = {0};
            if (sscanf(p + 13, "%31s %15s %23s", name, ts, slug) == 3) {
                if (!AmberScript_MapSetCrystalAnim(name, atoi(ts), slug))
                    printf("[amberscript] block_def_load: crystal_anim line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed crystal_anim line "
                       "(want: crystal_anim <map> <tileset id> <slug>): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "mapsize ", 8) == 0) {
            char name[32] = {0}, ws[16] = {0}, hs[16] = {0};
            if (sscanf(p + 8, "%31s %15s %15s", name, ws, hs) == 3) {
                if (!AmberScript_MapSetDims(name, atoi(ws), atoi(hs)))
                    printf("[amberscript] block_def_load: mapsize line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed mapsize line: '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "border ", 7) == 0) {
            char name[32] = {0}, tl[32] = {0}, tr[32] = {0}, bl[32] = {0}, br[32] = {0};
            if (sscanf(p + 7, "%31s %31s %31s %31s %31s", name, tl, tr, bl, br) == 5) {
                if (!AmberScript_MapSetBorder(name, tl, tr, bl, br))
                    printf("[amberscript] block_def_load: border line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed border line: '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "border_side ", 12) == 0) {

            char name[32] = {0}, sd[16] = {0};
            char tl[32] = {0}, tr[32] = {0}, bl[32] = {0}, br[32] = {0};
            if (sscanf(p + 12, "%31s %15s %31s %31s %31s %31s",
                       name, sd, tl, tr, bl, br) == 6) {
                int side = strcmp(sd, "north") == 0 ? 0 :
                           strcmp(sd, "south") == 0 ? 1 :
                           strcmp(sd, "west")  == 0 ? 2 :
                           strcmp(sd, "east")  == 0 ? 3 : -1;
                if (side < 0)
                    printf("[amberscript] block_def_load: border_side bad side '%s' in '%s'\n", sd, p);
                else if (!AmberScript_MapSetBorderSide(name, side, tl, tr, bl, br))
                    printf("[amberscript] block_def_load: border_side line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed border_side line: '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "map_script ", 11) == 0) {

            char name[32] = {0}, script[32] = {0};
            if (sscanf(p + 11, "%31s %31s", name, script) == 2) {
                CyclingRoadGate_RegisterMapScript(name, script);
            } else {
                printf("[amberscript] block_def_load: malformed map_script line: '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "warpspot ", 9) == 0) {

            char name[32] = {0}, idx[16] = {0}, xs[16] = {0}, ys[16] = {0};
            if (sscanf(p + 9, "%31s %15s %15s %15s", name, idx, xs, ys) == 4) {
                if (!AmberScript_MapSetWarpSpot(name, atoi(idx), atoi(xs), atoi(ys)))
                    printf("[amberscript] block_def_load: warpspot line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed warpspot line: '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "indoor ", 7) == 0) {

            char name[32] = {0};
            if (sscanf(p + 7, "%31s", name) == 1) {
                if (!AmberScript_MapSetIndoor(name))
                    printf("[amberscript] block_def_load: indoor line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed indoor line: '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "gbc_tileset ", 12) == 0) {

            char name[32] = {0}, ts[32] = {0};
            if (sscanf(p + 12, "%31s %31s", name, ts) == 2) {
                int id = pks_tileset_id_from_name(ts);
                if (id < 0) {
                    printf("[amberscript] block_def_load: unknown gbc_tileset '%s'\n", ts);
                } else if (!AmberScript_MapSetGbcTileset(name, id)) {
                    printf("[amberscript] block_def_load: gbc_tileset line failed: '%s'\n", p);
                } else {
                    s_gbc_tileset_ctx = id;
                    s_gbc_map_ctx = GbcColor_MapIdForName(name);
                }
            } else {
                printf("[amberscript] block_def_load: malformed gbc_tileset line: '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "dark ", 5) == 0) {

            char name[32] = {0};
            if (sscanf(p + 5, "%31s", name) == 1) {
                if (!AmberScript_MapSetDark(name))
                    printf("[amberscript] block_def_load: dark line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed dark line: '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "no_door_step ", 13) == 0) {

            char name[32] = {0};
            if (sscanf(p + 13, "%31s", name) == 1) {
                if (!AmberScript_MapSetNoDoorStep(name))
                    printf("[amberscript] block_def_load: no_door_step line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed no_door_step line: '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "music ", 6) == 0) {

            char name[32] = {0}, track[32] = {0};
            if (sscanf(p + 6, "%31s %31s", name, track) == 2) {
                if (!AmberScript_MapSetMusic(name, track))
                    printf("[amberscript] block_def_load: music line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed music line: '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "npc ", 4) == 0) {

            char name[32] = {0}, sprite[32] = {0}, xs[16] = {0}, ys[16] = {0}, move_s[24] = {0}, text[PKS_MAX_TEXT] = {0};
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, sprite, sizeof(sprite)) &&
                AmberScript_ParseArg(p, 3, xs, sizeof(xs)) &&
                AmberScript_ParseArg(p, 4, ys, sizeof(ys)) &&
                AmberScript_ParseArg(p, 5, move_s, sizeof(move_s)) &&
                AmberScript_ParseArg(p, 6, text, sizeof(text))) {

                int facing = 0;
                int movement = strcmp(move_s, "stay") == 0 ? 0
                             : strcmp(move_s, "face_down") == 0 ? 0
                             : strcmp(move_s, "face_up") == 0 ? (facing = 1, 0)
                             : strcmp(move_s, "face_left") == 0 ? (facing = 2, 0)
                             : strcmp(move_s, "face_right") == 0 ? (facing = 3, 0)
                             : strcmp(move_s, "walk_random") == 0 ? 1
                             : strcmp(move_s, "walk_left_right") == 0 ? 2
                             : strcmp(move_s, "walk_up_down") == 0 ? 3
                             : strcmp(move_s, "look_around") == 0 ? 4
                             : -1;
                if (movement < 0)
                    printf("[amberscript] block_def_load: npc '%s': unrecognized movement '%s' "
                           "(want stay|face_down|face_up|face_left|face_right|look_around|"
                           "walk_random|walk_left_right|walk_up_down)\n", name, move_s);
                else {

                    char pal_tok[24] = {0};
                    for (int ai = 7; ai <= 9; ai++) {
                        if (AmberScript_ParseArg(p, ai, pal_tok, sizeof(pal_tok)) &&
                            strncmp(pal_tok, "pal:", 4) == 0) {
                            AmberScript_SetPendingNpcPalette(atoi(pal_tok + 4) + 1);
                            break;
                        }
                    }
                    if (!AmberScript_MapAddNpc(name, sprite, atoi(xs), atoi(ys), movement, facing, text))
                        printf("[amberscript] block_def_load: npc line failed: '%s'\n", p);
                }
            } else {
                printf("[amberscript] block_def_load: malformed npc line (want: npc <name> <sprite> <x> <y> <movement> \"<text>\"): '%s'\n", p);
            }
            continue;
        }

        if (mode == 0 && (strncmp(p, "trainer ", 8) == 0 ||
                          strncmp(p, "johto_trainer ", 14) == 0)) {
            const int johto = (p[0] == 'j');

            char name[32] = {0}, class_name[32] = {0}, nos[16] = {0}, xs[16] = {0}, ys[16] = {0},
                 dir_s[16] = {0}, sights[16] = {0}, before[PKS_MAX_TEXT] = {0}, after[PKS_MAX_TEXT] = {0}, defeat[PKS_MAX_TEXT] = {0},
                 flag[48] = {0}, sprite_override[32] = {0};
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, class_name, sizeof(class_name)) &&
                AmberScript_ParseArg(p, 3, nos, sizeof(nos)) &&
                AmberScript_ParseArg(p, 4, xs, sizeof(xs)) &&
                AmberScript_ParseArg(p, 5, ys, sizeof(ys)) &&
                AmberScript_ParseArg(p, 6, dir_s, sizeof(dir_s)) &&
                AmberScript_ParseArg(p, 7, sights, sizeof(sights)) &&
                AmberScript_ParseArg(p, 8, before, sizeof(before)) &&
                AmberScript_ParseArg(p, 9, after, sizeof(after))) {

                char a10[PKS_MAX_TEXT] = {0}, a11[48] = {0}, a12[48] = {0};
                AmberScript_ParseArg(p, 10, a10, sizeof(a10));
                AmberScript_ParseArg(p, 11, a11, sizeof(a11));
                AmberScript_ParseArg(p, 12, a12, sizeof(a12));
                const char *slots[3] = { a10, a11, a12 };
                for (int si = 0; si < 3; si++) {
                    const char *tok = slots[si];
                    if (!tok[0]) continue;
                    if (strncmp(tok, "sprite:", 7) == 0) {
                        snprintf(sprite_override, sizeof(sprite_override), "%s", tok + 7);
                        continue;
                    }
                    int is_flag = (strncmp(tok, "EVENT_", 6) == 0);
                    if (!is_flag) {
                        is_flag = 1;
                        for (const char *q = tok; *q; q++)
                            if (*q < '0' || *q > '9') { is_flag = 0; break; }
                    }
                    if (is_flag)
                        snprintf(flag, sizeof(flag), "%s", tok);
                    else
                        snprintf(defeat, sizeof(defeat), "%s", tok);
                }
                int dir = strcmp(dir_s, "down") == 0 ? 0
                        : strcmp(dir_s, "up") == 0 ? 1
                        : strcmp(dir_s, "left") == 0 ? 2
                        : strcmp(dir_s, "right") == 0 ? 3
                        : -1;

                {
                    char pal_tok[24] = {0};
                    for (int ai = 10; ai <= 13; ai++) {
                        if (AmberScript_ParseArg(p, ai, pal_tok, sizeof(pal_tok)) &&
                            strncmp(pal_tok, "pal:", 4) == 0) {
                            AmberScript_SetPendingNpcPalette(atoi(pal_tok + 4) + 1);
                            break;
                        }
                    }
                }
                if (dir < 0)
                    printf("[amberscript] block_def_load: trainer '%s': unrecognized direction '%s' "
                           "(want down|up|left|right)\n", name, dir_s);
                else if (!(johto
                           ? AmberScript_MapAddJohtoTrainer(name, class_name, atoi(nos), atoi(xs), atoi(ys),
                                                            dir, atoi(sights), before, after, defeat, flag, sprite_override)
                           : AmberScript_MapAddTrainer(name, class_name, atoi(nos), atoi(xs), atoi(ys),
                                                    dir, atoi(sights), before, after, defeat, flag, sprite_override)))
                    printf("[amberscript] block_def_load: %strainer line failed: '%s'\n",
                           johto ? "johto_" : "", p);
            } else {
                printf("[amberscript] block_def_load: malformed %strainer line "
                       "(want: %strainer <name> <class> <no> <x> <y> <dir> <sight_dist> \"<before>\" \"<after>\" [\"<defeat_quote>\"] [EVENT_FLAG] [sprite:NAME]): '%s'\n",
                       johto ? "johto_" : "", johto ? "johto_" : "", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "item_ball ", 10) == 0) {

            char name[32] = {0}, xs[16] = {0}, ys[16] = {0}, item[32] = {0};
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, xs, sizeof(xs)) &&
                AmberScript_ParseArg(p, 3, ys, sizeof(ys)) &&
                AmberScript_ParseArg(p, 4, item, sizeof(item))) {
                if (!AmberScript_MapAddItemBall(name, atoi(xs), atoi(ys), item))
                    printf("[amberscript] block_def_load: item_ball line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed item_ball line "
                       "(want: item_ball <name> <x> <y> <item>): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "hidden_item ", 12) == 0) {

            char name[32] = {0}, xs[16] = {0}, ys[16] = {0}, item[32] = {0};
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, xs, sizeof(xs)) &&
                AmberScript_ParseArg(p, 3, ys, sizeof(ys)) &&
                AmberScript_ParseArg(p, 4, item, sizeof(item))) {
                if (!AmberScript_MapAddHiddenItem(name, atoi(xs), atoi(ys), item))
                    printf("[amberscript] block_def_load: hidden_item line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed hidden_item line "
                       "(want: hidden_item <name> <x> <y> <item>): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "hidden_coin ", 12) == 0) {

            char name[32] = {0}, xs[16] = {0}, ys[16] = {0}, amt[16] = {0};
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, xs, sizeof(xs)) &&
                AmberScript_ParseArg(p, 3, ys, sizeof(ys)) &&
                AmberScript_ParseArg(p, 4, amt, sizeof(amt))) {
                if (!AmberScript_MapAddHiddenCoin(name, atoi(xs), atoi(ys), atoi(amt)))
                    printf("[amberscript] block_def_load: hidden_coin line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed hidden_coin line "
                       "(want: hidden_coin <name> <x> <y> <amount>): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "static_encounter ", 17) == 0) {

            char name[32] = {0}, sp[32] = {0}, lv[16] = {0}, xs[16] = {0}, ys[16] = {0};
            char ev[48] = {0}, text[PKS_MAX_TEXT] = {0}, opt[32] = {0};
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, sp, sizeof(sp)) &&
                AmberScript_ParseArg(p, 3, lv, sizeof(lv)) &&
                AmberScript_ParseArg(p, 4, xs, sizeof(xs)) &&
                AmberScript_ParseArg(p, 5, ys, sizeof(ys)) &&
                AmberScript_ParseArg(p, 6, ev, sizeof(ev)) &&
                AmberScript_ParseArg(p, 7, text, sizeof(text))) {

                int cry = 0, facing = 0;
                char sprite[32] = {0};
                for (int ai = 8; ai <= 11; ai++) {
                    opt[0] = 0;
                    if (!AmberScript_ParseArg(p, ai, opt, sizeof(opt)) || !opt[0]) break;
                    if (!strcmp(opt, "cry")) {
                        cry = 1;
                    } else if (!strncmp(opt, "sprite:", 7)) {
                        snprintf(sprite, sizeof(sprite), "%s", opt + 7);
                    } else if (!strncmp(opt, "facing:", 7)) {
                        const char *f = opt + 7;
                        if      (!strcmp(f, "down"))  facing = 0;
                        else if (!strcmp(f, "up"))    facing = 1;
                        else if (!strcmp(f, "left"))  facing = 2;
                        else if (!strcmp(f, "right")) facing = 3;
                        else printf("[amberscript] block_def_load: static_encounter bad "
                                    "facing '%s' (want up|down|left|right): '%s'\n", f, p);
                    } else {
                        printf("[amberscript] block_def_load: static_encounter unknown "
                               "option '%s' (want cry|sprite:NAME|facing:DIR): '%s'\n", opt, p);
                    }
                }
                if (!AmberScript_MapAddStaticEncounter(name, sp, atoi(lv), atoi(xs), atoi(ys),
                                                       ev, text, cry, sprite, facing))
                    printf("[amberscript] block_def_load: static_encounter line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed static_encounter line "
                       "(want: static_encounter <name> <SPECIES> <level> <x> <y> "
                       "<EVENT_FLAG> \"<text>\" [cry] [sprite:NAME] [facing:DIR]): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "slot_machine ", 13) == 0) {

            char name[32] = {0}, xs[16] = {0}, ys[16] = {0}, kinds[24] = {0};
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, xs, sizeof(xs)) &&
                AmberScript_ParseArg(p, 3, ys, sizeof(ys))) {

                int kind = 0;
                if (AmberScript_ParseArg(p, 4, kinds, sizeof(kinds)) && kinds[0]) {
                    if      (!strcmp(kinds, "out_of_order"))  kind = 0xFD;
                    else if (!strcmp(kinds, "out_to_lunch"))  kind = 0xFE;
                    else if (!strcmp(kinds, "someones_keys")) kind = 0xFF;
                    else if (strcmp(kinds, "ok") && strcmp(kinds, "any")) {
                        printf("[amberscript] block_def_load: slot_machine unknown kind "
                               "'%s' (want: ok|out_of_order|out_to_lunch|someones_keys): '%s'\n",
                               kinds, p);
                    }
                }
                if (!AmberScript_MapAddSlotMachine(name, atoi(xs), atoi(ys), kind))
                    printf("[amberscript] block_def_load: slot_machine line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed slot_machine line "
                       "(want: slot_machine <name> <x> <y> [kind]): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "hidden_event ", 13) == 0) {

            char name[32] = {0}, xs[16] = {0}, ys[16] = {0}, text[PKS_MAX_TEXT] = {0};
            char face[16] = {0};
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, xs, sizeof(xs)) &&
                AmberScript_ParseArg(p, 3, ys, sizeof(ys)) &&
                AmberScript_ParseArg(p, 4, text, sizeof(text))) {

                int facing = 0;
                if (AmberScript_ParseArg(p, 5, face, sizeof(face)) && face[0]) {
                    if      (!strcmp(face, "down"))  facing = 1;
                    else if (!strcmp(face, "up"))    facing = 2;
                    else if (!strcmp(face, "left"))  facing = 3;
                    else if (!strcmp(face, "right")) facing = 4;
                    else if (!strcmp(face, "any"))   facing = 0;
                    else printf("[amberscript] block_def_load: hidden_event bad facing "
                                "'%s' (want up|down|left|right|any): '%s'\n", face, p);
                }
                if (!AmberScript_MapAddHiddenEvent(name, atoi(xs), atoi(ys), text, facing))
                    printf("[amberscript] block_def_load: hidden_event line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed hidden_event line "
                       "(want: hidden_event <name> <x> <y> \"<text>\" [facing]): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "after_battle ", 13) == 0) {

            char abs_scene[64] = {0};
            if (AmberScript_ParseArg(p, 1, abs_scene, sizeof(abs_scene))) {
                if (!AmberScript_AddAfterBattle(abs_scene))
                    printf("[amberscript] block_def_load: after_battle line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed after_battle line "
                       "(want: after_battle <scene>): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "zone_latch ", 11) == 0) {

            char zmap[32] = {0}, zev[96] = {0};
            if (AmberScript_ParseArg(p, 1, zmap, sizeof(zmap)) &&
                AmberScript_ParseArg(p, 2, zev, sizeof(zev))) {
                int xs[8], ys[8], n = 0;
                char ax[16] = {0}, ay[16] = {0};
                while (n < 8 &&
                       AmberScript_ParseArg(p, 3 + n * 2, ax, sizeof(ax)) &&
                       AmberScript_ParseArg(p, 4 + n * 2, ay, sizeof(ay))) {
                    xs[n] = atoi(ax);
                    ys[n] = atoi(ay);
                    n++;
                }
                if (n == 0) {
                    printf("[amberscript] block_def_load: zone_latch needs at least one "
                           "<x> <y> pair: '%s'\n", p);
                } else if (!AmberScript_MapAddZoneLatch(zmap, zev, xs, ys, n)) {
                    printf("[amberscript] block_def_load: zone_latch line failed: '%s'\n", p);
                }
            } else {
                printf("[amberscript] block_def_load: malformed zone_latch line "
                       "(want: zone_latch <map> <EVENT> <x> <y> [<x> <y> ...]): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "scene_trigger ", 14) == 0) {

            {
                char nameW[32] = {0}, sceneW[64] = {0}, tokW[16] = {0};
                if (AmberScript_ParseArg(p, 1, nameW, sizeof(nameW)) &&
                    AmberScript_ParseArg(p, 2, tokW, sizeof(tokW)) &&
                    strcmp(tokW, "watch") == 0 &&
                    AmberScript_ParseArg(p, 3, sceneW, sizeof(sceneW))) {
                    char tok4[16] = {0}, ckW[16] = {0}, ceW[96] = {0};
                    char tok7[16] = {0}, ck2W[16] = {0}, ce2W[96] = {0};
                    int okW = 0;
                    if (AmberScript_ParseArg(p, 4, tok4, sizeof(tok4)) && strcmp(tok4, "when") == 0 &&
                        AmberScript_ParseArg(p, 5, ckW, sizeof(ckW)) &&
                        AmberScript_ParseArg(p, 6, ceW, sizeof(ceW))) {
                        if (AmberScript_ParseArg(p, 7, tok7, sizeof(tok7)) && strcmp(tok7, "and") == 0 &&
                            AmberScript_ParseArg(p, 8, ck2W, sizeof(ck2W)) &&
                            AmberScript_ParseArg(p, 9, ce2W, sizeof(ce2W))) {
                            okW = AmberScript_MapAddSceneTriggerWatch(nameW, sceneW, ckW, ceW, ck2W, ce2W);
                        } else {
                            okW = AmberScript_MapAddSceneTriggerWatch(nameW, sceneW, ckW, ceW, NULL, NULL);
                        }
                    } else {
                        printf("[amberscript] block_def_load: scene_trigger watch needs a "
                               "`when event_set|event_clear <EVENT>` gate: '%s'\n", p);
                    }
                    if (!okW)
                        printf("[amberscript] block_def_load: scene_trigger watch line failed: '%s'\n", p);
                    continue;
                }
            }
            {
                char name0[32] = {0}, tok0[16] = {0};
                if (AmberScript_ParseArg(p, 1, name0, sizeof(name0)) &&
                    AmberScript_ParseArg(p, 2, tok0, sizeof(tok0)) &&
                    strcmp(tok0, "onload") == 0) {
                    char scene0[64] = {0};
                    char tok3[16] = {0}, condkind0[16] = {0}, condevent0[96] = {0};
                    char tok6[16] = {0}, condkind02[16] = {0}, condevent02[96] = {0};
                    int ok0 = 0;
                    if (AmberScript_ParseArg(p, 3, scene0, sizeof(scene0))) {
                        if (AmberScript_ParseArg(p, 4, tok3, sizeof(tok3)) && strcmp(tok3, "when") == 0) {
                            if (AmberScript_ParseArg(p, 5, condkind0, sizeof(condkind0)) &&
                                AmberScript_ParseArg(p, 6, condevent0, sizeof(condevent0))) {
                                if (AmberScript_ParseArg(p, 7, tok6, sizeof(tok6)) && strcmp(tok6, "and") == 0) {
                                    if (AmberScript_ParseArg(p, 8, condkind02, sizeof(condkind02)) &&
                                        AmberScript_ParseArg(p, 9, condevent02, sizeof(condevent02))) {
                                        ok0 = AmberScript_MapAddSceneTriggerOnLoad(name0, scene0, condkind0, condevent0, condkind02, condevent02);
                                    } else {
                                        printf("[amberscript] block_def_load: malformed scene_trigger onload 'and' clause: '%s'\n", p);
                                    }
                                } else {
                                    ok0 = AmberScript_MapAddSceneTriggerOnLoad(name0, scene0, condkind0, condevent0, NULL, NULL);
                                }
                            } else {
                                printf("[amberscript] block_def_load: malformed scene_trigger onload 'when' clause: '%s'\n", p);
                            }
                        } else {
                            ok0 = AmberScript_MapAddSceneTriggerOnLoad(name0, scene0, NULL, NULL, NULL, NULL);
                        }
                        if (!ok0)
                            printf("[amberscript] block_def_load: scene_trigger onload line failed: '%s'\n", p);
                    } else {
                        printf("[amberscript] block_def_load: malformed scene_trigger onload line "
                               "(want: scene_trigger <map> onload <scene> "
                               "[when event_set|event_clear <event> [and event_set|event_clear <event>]]): '%s'\n", p);
                    }
                    continue;
                }
            }

            char name[32] = {0}, xs[16] = {0}, ys[16] = {0}, scene[64] = {0};
            char tok5[16] = {0}, condkind[16] = {0}, condevent[96] = {0};
            char tok8[16] = {0}, condkind2[16] = {0}, condevent2[96] = {0};
            int x = 0, y = 0;
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, xs, sizeof(xs)) &&
                AmberScript_ParseArg(p, 3, ys, sizeof(ys)) &&
                AmberScript_ParseArg(p, 4, scene, sizeof(scene)) &&
                AmberScript_ParseCoordExprOrAny(xs, 1, &x) &&
                AmberScript_ParseCoordExprOrAny(ys, 0, &y)) {
                int ok;
                if (AmberScript_ParseArg(p, 5, tok5, sizeof(tok5)) && strcmp(tok5, "when") == 0) {
                    if (!AmberScript_ParseArg(p, 6, condkind, sizeof(condkind)) ||
                        !AmberScript_ParseArg(p, 7, condevent, sizeof(condevent))) {
                        printf("[amberscript] block_def_load: malformed scene_trigger 'when' clause: '%s'\n", p);
                        continue;
                    }

                    if (AmberScript_ParseArg(p, 8, tok8, sizeof(tok8)) && strcmp(tok8, "and") == 0) {
                        if (!AmberScript_ParseArg(p, 9, condkind2, sizeof(condkind2)) ||
                            !AmberScript_ParseArg(p, 10, condevent2, sizeof(condevent2))) {
                            printf("[amberscript] block_def_load: malformed scene_trigger 'and' clause: '%s'\n", p);
                            continue;
                        }
                        ok = AmberScript_MapAddSceneTrigger(name, x, y, scene, condkind, condevent, condkind2, condevent2);
                    } else {
                        ok = AmberScript_MapAddSceneTrigger(name, x, y, scene, condkind, condevent, NULL, NULL);
                    }
                } else {
                    ok = AmberScript_MapAddSceneTrigger(name, x, y, scene, NULL, NULL, NULL, NULL);
                }
                if (!ok)
                    printf("[amberscript] block_def_load: scene_trigger line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed scene_trigger line "
                       "(want: scene_trigger <map> <x_or_any> <y_or_any> <scene> "
                       "[when event_set|event_clear <event> [and event_set|event_clear <event>]]): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "scene_npc ", 10) == 0) {

            char name[32] = {0}, scene[64] = {0}, xs[16] = {0}, ys[16] = {0};
            int x = 0, y = 0;
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, scene, sizeof(scene)) &&
                AmberScript_ParseArg(p, 3, xs, sizeof(xs)) &&
                AmberScript_ParseArg(p, 4, ys, sizeof(ys))) {
                x = atoi(xs); y = atoi(ys);
                if (!AmberScript_MapAddSceneNpc(name, scene, x, y))
                    printf("[amberscript] block_def_load: scene_npc line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed scene_npc line "
                       "(want: scene_npc <map> <scene> <x> <y>): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "warp_walk_into ", 15) == 0) {

            char name[32] = {0}, xs[16] = {0}, ys[16] = {0}, ds[16] = {0};
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, xs, sizeof(xs)) &&
                AmberScript_ParseArg(p, 3, ys, sizeof(ys))) {
                int dir = 0;
                if (AmberScript_ParseArg(p, 4, ds, sizeof(ds)) && ds[0]) {
                    if      (strcmp(ds, "down")  == 0) dir = PKS_FACE_DOWN;
                    else if (strcmp(ds, "up")    == 0) dir = PKS_FACE_UP;
                    else if (strcmp(ds, "left")  == 0) dir = PKS_FACE_LEFT;
                    else if (strcmp(ds, "right") == 0) dir = PKS_FACE_RIGHT;
                    else printf("[amberscript] block_def_load: warp_walk_into: unknown direction '%s'\n", ds);
                }
                if (!AmberScript_MapSetWarpWalkInto(name, atoi(xs), atoi(ys), dir))
                    printf("[amberscript] block_def_load: warp_walk_into line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed warp_walk_into line "
                       "(want: warp_walk_into <map> <x> <y> [down|up|left|right]): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "warp_stair ", 11) == 0) {

            char name[32] = {0}, xs[16] = {0}, ys[16] = {0};
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, xs, sizeof(xs)) &&
                AmberScript_ParseArg(p, 3, ys, sizeof(ys))) {
                if (!AmberScript_MapSetWarpStair(name, atoi(xs), atoi(ys)))
                    printf("[amberscript] block_def_load: warp_stair line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed warp_stair line "
                       "(want: warp_stair <map> <x> <y>): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "scene_tile ", 11) == 0) {

            char name[32] = {0}, scene[64] = {0}, xs[16] = {0}, ys[16] = {0};
            int x = 0, y = 0;
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, scene, sizeof(scene)) &&
                AmberScript_ParseArg(p, 3, xs, sizeof(xs)) &&
                AmberScript_ParseArg(p, 4, ys, sizeof(ys))) {
                x = atoi(xs); y = atoi(ys);
                if (!AmberScript_MapAddSceneTile(name, scene, x, y))
                    printf("[amberscript] block_def_load: scene_tile line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed scene_tile line "
                       "(want: scene_tile <map> <scene> <x> <y>): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "text_if ", 8) == 0) {

            char event_name[48] = {0}, text[PKS_MAX_TEXT] = {0};
            if (AmberScript_ParseArg(p, 1, event_name, sizeof(event_name)) &&
                AmberScript_ParseArg(p, 2, text, sizeof(text))) {
                if (!AmberScript_AddTextVariant(event_name, text))
                    printf("[amberscript] block_def_load: text_if line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed text_if line "
                       "(want: text_if <EVENT_NAME> \"<text>\"): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "badge_if ", 9) == 0) {

            char badge_name[32] = {0}, text[PKS_MAX_TEXT] = {0};
            if (AmberScript_ParseArg(p, 1, badge_name, sizeof(badge_name)) &&
                AmberScript_ParseArg(p, 2, text, sizeof(text))) {
                if (!AmberScript_AddBadgeTextVariant(badge_name, text))
                    printf("[amberscript] block_def_load: badge_if line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed badge_if line "
                       "(want: badge_if <BADGE_NAME> \"<text>\"): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "text_random ", 12) == 0) {

            char weight_s[16] = {0}, text[PKS_MAX_TEXT] = {0};
            if (AmberScript_ParseArg(p, 1, weight_s, sizeof(weight_s)) &&
                AmberScript_ParseArg(p, 2, text, sizeof(text))) {
                if (!AmberScript_AddTextRandom(atoi(weight_s), text))
                    printf("[amberscript] block_def_load: text_random line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed text_random line "
                       "(want: text_random <weight> \"<text>\"): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "hide_if ", 8) == 0) {

            char event_name[48] = {0};
            if (AmberScript_ParseArg(p, 1, event_name, sizeof(event_name))) {
                if (!AmberScript_AddHideIf(event_name))
                    printf("[amberscript] block_def_load: hide_if line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed hide_if line "
                       "(want: hide_if <EVENT_NAME>): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && (strcmp(p, "hidden") == 0 ||
                          strncmp(p, "hidden ", 7) == 0)) {

            if (!AmberScript_AddStartsHidden())
                printf("[amberscript] block_def_load: hidden line failed: '%s'\n", p);
            continue;
        }
        if (mode == 0 && strncmp(p, "show_if ", 8) == 0) {

            char event_name[48] = {0};
            if (AmberScript_ParseArg(p, 1, event_name, sizeof(event_name))) {
                if (!AmberScript_AddShowIf(event_name))
                    printf("[amberscript] block_def_load: show_if line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed show_if line "
                       "(want: show_if <EVENT_NAME>): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "no_face_until ", 14) == 0) {

            char event_name[48] = {0};
            if (AmberScript_ParseArg(p, 1, event_name, sizeof(event_name))) {
                if (!AmberScript_AddNoFaceUntil(event_name))
                    printf("[amberscript] block_def_load: no_face_until line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed no_face_until line "
                       "(want: no_face_until <EVENT_NAME>): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "wild_rate ", 10) == 0) {

            char name[32] = {0}, kind[16] = {0}, rate_s[16] = {0};
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, kind, sizeof(kind)) &&
                AmberScript_ParseArg(p, 3, rate_s, sizeof(rate_s))) {
                int is_water = strcmp(kind, "water") == 0 ? 1
                             : strcmp(kind, "grass") == 0 ? 0
                             : -1;
                if (is_water < 0)
                    printf("[amberscript] block_def_load: wild_rate '%s': unrecognized kind '%s' (want grass|water)\n", name, kind);
                else if (!AmberScript_MapSetWildRate(name, is_water, atoi(rate_s)))
                    printf("[amberscript] block_def_load: wild_rate line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed wild_rate line (want: wild_rate <name> grass|water <rate>): '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "wild_encounter ", 15) == 0) {

            char name[32] = {0}, kind[16] = {0}, slot_s[16] = {0}, species[32] = {0}, level_s[16] = {0};
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, kind, sizeof(kind)) &&
                AmberScript_ParseArg(p, 3, slot_s, sizeof(slot_s)) &&
                AmberScript_ParseArg(p, 4, species, sizeof(species)) &&
                AmberScript_ParseArg(p, 5, level_s, sizeof(level_s))) {
                int is_water = strcmp(kind, "water") == 0 ? 1
                             : strcmp(kind, "grass") == 0 ? 0
                             : -1;
                if (is_water < 0)
                    printf("[amberscript] block_def_load: wild_encounter '%s': unrecognized kind '%s' (want grass|water)\n", name, kind);
                else if (!AmberScript_MapSetWildSlot(name, is_water, atoi(slot_s), species, atoi(level_s)))
                    printf("[amberscript] block_def_load: wild_encounter line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed wild_encounter line "
                       "(want: wild_encounter <name> grass|water <slot 1-10> <species> <level>): '%s'\n", p);
            }
            continue;
        }

        #define pks_parse_tod(s) (!strcmp((s), "morn") ? 0 : \
                                  !strcmp((s), "day")  ? 1 : \
                                  !strcmp((s), "nite") ? 2 : -1)
        if (mode == 0 && (strncmp(p, "wild_grass_rate ", 16) == 0 || strncmp(p, "wild_water_rate ", 16) == 0)) {

            int is_water = strncmp(p, "wild_water_rate ", 16) == 0;
            char name[32] = {0}, rate_s[16] = {0};
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, rate_s, sizeof(rate_s))) {
                char tod_s[16] = {0};
                int tod = -1;
                if (AmberScript_ParseArg(p, 3, tod_s, sizeof(tod_s)) && tod_s[0])
                    tod = pks_parse_tod(tod_s);
                if (!AmberScript_MapSetJohtoWildRate(name, is_water, atoi(rate_s), tod))
                    printf("[amberscript] block_def_load: wild_%s_rate line failed: '%s'\n", is_water ? "water" : "grass", p);
            } else {
                printf("[amberscript] block_def_load: malformed wild_%s_rate line (want: wild_%s_rate <name> <rate> [morn|day|nite]): '%s'\n",
                       is_water ? "water" : "grass", is_water ? "water" : "grass", p);
            }
            continue;
        }
        if (mode == 0 && (strncmp(p, "wild_grass_slot ", 16) == 0 || strncmp(p, "wild_water_slot ", 16) == 0)) {

            int is_water = strncmp(p, "wild_water_slot ", 16) == 0;
            char name[32] = {0}, slot_s[16] = {0}, species[32] = {0}, level_s[16] = {0};
            if (AmberScript_ParseArg(p, 1, name, sizeof(name)) &&
                AmberScript_ParseArg(p, 2, slot_s, sizeof(slot_s)) &&
                AmberScript_ParseArg(p, 3, species, sizeof(species)) &&
                AmberScript_ParseArg(p, 4, level_s, sizeof(level_s))) {
                char tod_s[16] = {0};
                int tod = -1;
                if (AmberScript_ParseArg(p, 5, tod_s, sizeof(tod_s)) && tod_s[0])
                    tod = pks_parse_tod(tod_s);
                if (!AmberScript_MapSetJohtoWildSlot(name, is_water, atoi(slot_s), species, atoi(level_s), tod))
                    printf("[amberscript] block_def_load: wild_%s_slot line failed: '%s'\n", is_water ? "water" : "grass", p);
            } else {
                printf("[amberscript] block_def_load: malformed wild_%s_slot line "
                       "(want: wild_%s_slot <name> <slot> <species> <level> [morn|day|nite]): '%s'\n",
                       is_water ? "water" : "grass", is_water ? "water" : "grass", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "tile_sign ", 10) == 0) {

            char xs[16] = {0}, ys[16] = {0}, text[PKS_MAX_TEXT] = {0};
            if (AmberScript_ParseArg(p, 1, xs, sizeof(xs)) &&
                AmberScript_ParseArg(p, 2, ys, sizeof(ys)) &&
                AmberScript_ParseArg(p, 3, text, sizeof(text))) {
                if (!AmberScript_TileSetSign(atoi(xs), atoi(ys), text))
                    printf("[amberscript] block_def_load: tile_sign line failed (no placed cell at that position?): '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed tile_sign line: '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "tile_ledge ", 11) == 0) {

            char xs[16] = {0}, ys[16] = {0}, dirstr[32] = {0};
            if (sscanf(p + 11, "%15s %15s %31s", xs, ys, dirstr) == 3) {
                int dirs = 0;
                char *tok;
                for (tok = strtok(dirstr, "|"); tok; tok = strtok(NULL, "|")) {
                    if (strcmp(tok, "down") == 0) dirs |= PKS_FACE_DOWN;
                    else if (strcmp(tok, "up") == 0) dirs |= PKS_FACE_UP;
                    else if (strcmp(tok, "left") == 0) dirs |= PKS_FACE_LEFT;
                    else if (strcmp(tok, "right") == 0) dirs |= PKS_FACE_RIGHT;
                    else printf("[amberscript] block_def_load: tile_ledge: unrecognized direction '%s'\n", tok);
                }
                if (!dirs || !AmberScript_TileSetLedgeAt(atoi(xs), atoi(ys), dirs))
                    printf("[amberscript] block_def_load: tile_ledge line failed (no placed cell at that position, or no valid directions?): '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed tile_ledge line: '%s'\n", p);
            }
            continue;
        }

        if (mode == 0 && (strncmp(p, "tile_if ", 8) == 0 ||
                          strncmp(p, "tile_if_not ", 12) == 0)) {

            int negate = (strncmp(p, "tile_if_not ", 12) == 0);
            const char *kw = negate ? "tile_if_not" : "tile_if";
            const char *rest = p + (negate ? 12 : 8);
            char event_name[64] = {0}, alt_block[64] = {0}, xs[16] = {0}, ys[16] = {0};
            if (sscanf(rest, "%63s %63s %15s %15s", event_name, alt_block, xs, ys) == 4) {
                if (!AmberScript_TileSetConditionalEx(atoi(xs), atoi(ys), event_name,
                                                      alt_block, negate))
                    printf("[amberscript] block_def_load: %s line failed: '%s'\n", kw, p);
            } else {
                printf("[amberscript] block_def_load: malformed %s line "
                       "(want: %s <EVENT> <alt_block> <x> <y>): '%s'\n", kw, kw, p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "connect ", 8) == 0) {
            char from[32] = {0}, dirs[16] = {0}, to[32] = {0}, pc[16] = {0}, adj[16] = {0};
            int direction = -1;
            if (sscanf(p + 8, "%31s %15s %31s %15s %15s", from, dirs, to, pc, adj) == 5) {
                if (strcmp(dirs, "north") == 0) direction = 0;
                else if (strcmp(dirs, "south") == 0) direction = 1;
                else if (strcmp(dirs, "west") == 0) direction = 2;
                else if (strcmp(dirs, "east") == 0) direction = 3;
                if (direction < 0 ||
                    !AmberScript_ConnectionDefine(from, direction, to, atoi(pc), atoi(adj)))
                    printf("[amberscript] block_def_load: connect line failed: '%s'\n", p);
            } else {
                printf("[amberscript] block_def_load: malformed connect line: '%s'\n", p);
            }
            continue;
        }
        if (mode == 0 && strncmp(p, "block ", 6) == 0) {
            snprintf(cur_name, sizeof(cur_name), "%s", p + 6);
            mode = 1;
            blocks_seen++;
            continue;
        }
        if (mode == 0 && strncmp(p, "tileset ", 8) == 0) {
            snprintf(cur_name, sizeof(cur_name), "%s", p + 8);
            mode = 2;
            tilesets_seen++;
            continue;
        }
        if (strcmp(p, "end") == 0) {
            if (mode == 2) {
                if (!AmberScript_TilesetApply(cur_name))
                    printf("[amberscript] block_def_load: tileset '%s' apply failed (empty, or no subtiles added?)\n",
                           cur_name);
            }
            mode = 0;
            cur_name[0] = '\0';
            continue;
        }
        if (mode == 0) {
            printf("[amberscript] block_def_load: line outside any stanza, skipped: '%s'\n", p);
            continue;
        }

        if (mode == 2) {
            char t0[32] = {0}, t1[32] = {0};
            if (sscanf(p, "%31s %31s", t0, t1) == 2 && strcmp(t0, "add") == 0) {
                if (!AmberScript_SubtileTilesetAdd(cur_name, t1))
                    printf("[amberscript] block_def_load: tileset '%s' add '%s' failed (unknown subtile, or "
                           "tileset already full/mixed)\n", cur_name, t1);
            } else {
                printf("[amberscript] block_def_load: unrecognized line in tileset '%s': '%s'\n", cur_name, p);
            }
            continue;
        }

        {
            char t0[32] = {0}, t1[32] = {0}, t2[PKS_MAX_TEXT] = {0}, t3[32] = {0}, t4[32] = {0}, t5[32] = {0};
            int nf = sscanf(p, "%31s %31s %199s %31s %31s %31s", t0, t1, t2, t3, t4, t5);
            if (nf == 3 && strcmp(t0, "source") == 0 && strcmp(t1, "custom") == 0) {
                if (!AmberScript_LoadCustomTileArt(cur_name, t2))
                    printf("[amberscript] block_def_load: '%s' source custom failed: %s\n", cur_name, t2);
            } else if (nf == 3 && strcmp(t0, "source") == 0 && strcmp(t1, "original") == 0) {
                long id = strtol(t2, NULL, 0);
                if (id < 0 || id > 255 || !AmberScript_DefineOriginalTile(cur_name, (uint8_t)id))
                    printf("[amberscript] block_def_load: '%s' source original failed: %s\n", cur_name, t2);
            } else if (nf == 6 && strcmp(t0, "source") == 0 && strcmp(t1, "quad") == 0) {
                if (!AmberScript_DefineQuadTile(cur_name, t2, t3, t4, t5))
                    printf("[amberscript] block_def_load: '%s' source quad failed (unknown/unapplied subtile "
                           "among %s %s %s %s)\n", cur_name, t2, t3, t4, t5);
            } else if (nf == 2 && strcmp(t0, "passable") == 0 && strcmp(t1, "inherit") == 0) {
                if (!AmberScript_TileClearPassable(cur_name))
                    printf("[amberscript] block_def_load: '%s' passable inherit failed (no source yet?)\n", cur_name);
            } else if (nf == 2 && strcmp(t0, "passable") == 0) {
                if (!AmberScript_TileSetPassable(cur_name, pks_parse_bool(t1)))
                    printf("[amberscript] block_def_load: '%s' passable failed (no source yet?)\n", cur_name);
            } else if (nf == 2 && strcmp(t0, "surfable") == 0 && strcmp(t1, "inherit") == 0) {
                if (!AmberScript_TileClearSurfable(cur_name))
                    printf("[amberscript] block_def_load: '%s' surfable inherit failed (no source yet?)\n", cur_name);
            } else if (nf == 2 && strcmp(t0, "surfable") == 0) {
                if (!AmberScript_TileSetSurfable(cur_name, pks_parse_bool(t1)))
                    printf("[amberscript] block_def_load: '%s' surfable failed (no source yet?)\n", cur_name);
            } else if (nf == 2 && strcmp(t0, "cuttable") == 0 && strcmp(t1, "inherit") == 0) {
                if (!AmberScript_TileClearCuttable(cur_name))
                    printf("[amberscript] block_def_load: '%s' cuttable inherit failed (no source yet?)\n", cur_name);
            } else if (nf == 2 && strcmp(t0, "cuttable") == 0) {
                if (!AmberScript_TileSetCuttable(cur_name, pks_parse_bool(t1)))
                    printf("[amberscript] block_def_load: '%s' cuttable failed (no source yet?)\n", cur_name);
            } else if (nf == 2 && strcmp(t0, "counter") == 0 && strcmp(t1, "inherit") == 0) {
                if (!AmberScript_TileClearCounter(cur_name))
                    printf("[amberscript] block_def_load: '%s' counter inherit failed (no source yet?)\n", cur_name);
            } else if (nf == 2 && strcmp(t0, "counter") == 0) {
                if (!AmberScript_TileSetCounter(cur_name, pks_parse_bool(t1)))
                    printf("[amberscript] block_def_load: '%s' counter failed (no source yet?)\n", cur_name);
            } else if (nf == 2 && strcmp(t0, "cut_replacement") == 0) {
                if (!AmberScript_TileSetCutReplacement(cur_name, t1))
                    printf("[amberscript] block_def_load: '%s' cut_replacement failed (no source yet?)\n", cur_name);
            } else if (nf == 2 && strcmp(t0, "grass") == 0 && strcmp(t1, "inherit") == 0) {
                if (!AmberScript_TileClearGrass(cur_name))
                    printf("[amberscript] block_def_load: '%s' grass inherit failed (no source yet?)\n", cur_name);
            } else if (nf == 2 && strcmp(t0, "grass") == 0) {
                if (!AmberScript_TileSetGrass(cur_name, pks_parse_bool(t1)))
                    printf("[amberscript] block_def_load: '%s' grass failed (no source yet?)\n", cur_name);
            } else if (nf == 2 && strcmp(t0, "cut_span") == 0) {

                if (!AmberScript_TileSetCutSpanBlock(cur_name, strcmp(t1, "block") == 0))
                    printf("[amberscript] block_def_load: '%s' cut_span failed (no source yet?)\n", cur_name);
            } else if (nf == 2 && strcmp(t0, "grass_rustle") == 0 && strcmp(t1, "inherit") == 0) {
                if (!AmberScript_TileClearGrassRustle(cur_name))
                    printf("[amberscript] block_def_load: '%s' grass_rustle inherit failed (no source yet?)\n", cur_name);
            } else if (nf == 2 && strcmp(t0, "grass_rustle") == 0) {
                if (!AmberScript_TileSetGrassRustle(cur_name, pks_parse_bool(t1)))
                    printf("[amberscript] block_def_load: '%s' grass_rustle failed (no source yet?)\n", cur_name);
            } else if (nf == 2 && strcmp(t0, "pair_block") == 0) {
                if (!AmberScript_TileSetPairBlockGroup(cur_name, t1))
                    printf("[amberscript] block_def_load: '%s' pair_block failed (no source yet?)\n", cur_name);
            } else if (nf == 2 && strcmp(t0, "warp") == 0 && strcmp(t1, "none") == 0) {
                if (!AmberScript_TileClearWarp(cur_name))
                    printf("[amberscript] block_def_load: '%s' warp none failed (no source yet?)\n", cur_name);
            } else if (nf == 3 && strcmp(t0, "warp") == 0 && (t1[0] >= '0' && t1[0] <= '9')) {
                if (!AmberScript_TileSetWarp(cur_name, atoi(t1), atoi(t2)))
                    printf("[amberscript] block_def_load: '%s' warp failed (no source yet, or bad map id?)\n", cur_name);
            } else if (nf == 3 && strcmp(t0, "warp") == 0 && strcmp(t1, "last") == 0) {

                if (!AmberScript_TileSetWarpLast(cur_name, atoi(t2)))
                    printf("[amberscript] block_def_load: '%s' warp last failed (no source yet?)\n", cur_name);
            } else if (nf == 3 && strcmp(t0, "warp") == 0) {

                if (!AmberScript_TileSetWarpNamed(cur_name, t1, atoi(t2)))
                    printf("[amberscript] block_def_load: '%s' warp (named dest '%s') failed "
                           "(no source yet?)\n", cur_name, t1);
            } else {
                printf("[amberscript] block_def_load: unrecognized line in block '%s': '%s'\n", cur_name, p);
            }
        }
    }
    fclose(f);
    printf("[amberscript] block_def_load: '%s' -> %d block(s), %d tileset(s)\n",
           resolved, blocks_seen, tilesets_seen);
    return blocks_seen;
}

int AmberScript_TileMod_TryHandle(const char *cmd, const char *verb, int n) {
    (void)n;

    if (strcmp(verb, "tile_art_load") == 0) {
        char name[32] = {0};
        char path[256] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name)) ||
            !AmberScript_ParseArg(cmd, 2, path, sizeof(path))) {
            printf("[amberscript] tile_art_load usage: tile_art_load <name> <path-to-16x16.png>\n");
            AmberScript_WriteState();
            return 1;
        }
        if (AmberScript_LoadCustomTileArt(name, path)) {
            printf("[amberscript] tile_art_load: '%s' ready -- place with tile_place_custom %s <x> <y>\n", name, name);
        } else {
            printf("[amberscript] tile_art_load: failed for '%s'\n", name);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tileset_add") == 0) {
        char tsname[32] = {0}, asset[32] = {0};
        if (!AmberScript_ParseArg(cmd, 1, tsname, sizeof(tsname)) ||
            !AmberScript_ParseArg(cmd, 2, asset, sizeof(asset))) {
            printf("[amberscript] tileset_add usage: tileset_add <tileset_name> <asset_name>\n");
        } else if (AmberScript_TilesetAdd(tsname, asset)) {
            printf("[amberscript] tileset_add: '%s' <- '%s'\n", tsname, asset);
        } else {
            printf("[amberscript] tileset_add: failed (asset must be tile_art_load'd first, not already a subtile "
                   "tileset, and tileset caps at %d assets)\n", PKS_TILESET_ASSET_MODE_MAX);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "subtile_load") == 0) {
        char name[32] = {0}, path[256] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name)) ||
            !AmberScript_ParseArg(cmd, 2, path, sizeof(path))) {
            printf("[amberscript] subtile_load usage: subtile_load <name> <path-to-8x8.png>\n");
        } else if (AmberScript_LoadSubtileArt(name, path)) {
            printf("[amberscript] subtile_load: '%s' <- %s\n", name, path);
        } else {
            printf("[amberscript] subtile_load: failed for '%s'\n", name);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "subtile_tileset_add") == 0) {
        char tsname[32] = {0}, sub[32] = {0};
        if (!AmberScript_ParseArg(cmd, 1, tsname, sizeof(tsname)) ||
            !AmberScript_ParseArg(cmd, 2, sub, sizeof(sub))) {
            printf("[amberscript] subtile_tileset_add usage: subtile_tileset_add <tileset_name> <subtile_name>\n");
        } else if (AmberScript_SubtileTilesetAdd(tsname, sub)) {
            printf("[amberscript] subtile_tileset_add: '%s' <- '%s'\n", tsname, sub);
        } else {
            printf("[amberscript] subtile_tileset_add: failed (unknown subtile, tileset already holds 16x16 "
                   "assets, or full at %d subtiles)\n", PKS_TILESET_ASSET_MAX);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_define_quad") == 0) {
        char name[32] = {0}, s0[32] = {0}, s1[32] = {0}, s2[32] = {0}, s3[32] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name)) ||
            !AmberScript_ParseArg(cmd, 2, s0, sizeof(s0)) || !AmberScript_ParseArg(cmd, 3, s1, sizeof(s1)) ||
            !AmberScript_ParseArg(cmd, 4, s2, sizeof(s2)) || !AmberScript_ParseArg(cmd, 5, s3, sizeof(s3))) {
            printf("[amberscript] tile_define_quad usage: tile_define_quad <name> <tl> <tr> <bl> <br>\n");
        } else if (AmberScript_DefineQuadTile(name, s0, s1, s2, s3)) {
            printf("[amberscript] tile_define_quad: '%s' <- %s %s %s %s\n", name, s0, s1, s2, s3);
        } else {
            printf("[amberscript] tile_define_quad: failed for '%s' (a subtile is unknown or not yet applied "
                   "to a tileset)\n", name);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tileset_apply") == 0) {
        char tsname[32] = {0};
        if (!AmberScript_ParseArg(cmd, 1, tsname, sizeof(tsname))) {
            printf("[amberscript] tileset_apply usage: tileset_apply <tileset_name>\n");
        } else if (AmberScript_TilesetApply(tsname)) {
            int ts_slot = pks_tileset_find(tsname);
            int per_entry = (ts_slot >= 0 && s_tilesets[ts_slot].is_subtile) ? 1 : 4;
            int tile_count = (ts_slot >= 0) ? s_tilesets[ts_slot].asset_count * per_entry : 0;
            printf("[amberscript] tileset_apply: '%s' stamped into map %d (tiles 0-%d), bound for auto-reapply\n",
                   tsname, (int)wCurMap, tile_count > 0 ? tile_count - 1 : 0);
        } else {
            printf("[amberscript] tileset_apply: failed for '%s' (empty or unknown tileset?)\n", tsname);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tileset_clear") == 0) {
        char tsname[32] = {0};
        if (!AmberScript_ParseArg(cmd, 1, tsname, sizeof(tsname))) {
            printf("[amberscript] tileset_clear usage: tileset_clear <tileset_name>\n");
        } else if (AmberScript_TilesetClear(tsname)) {
            printf("[amberscript] tileset_clear: '%s' cleared\n", tsname);
        } else {
            printf("[amberscript] tileset_clear: unknown tileset '%s'\n", tsname);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_set_passable") == 0) {
        char name[32] = {0}, arg[16] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name)) ||
            !AmberScript_ParseArg(cmd, 2, arg, sizeof(arg))) {
            printf("[amberscript] tile_set_passable usage: tile_set_passable <name> <0|1>\n");
        } else if (AmberScript_TileSetPassable(name, atoi(arg))) {
            printf("[amberscript] tile_set_passable: '%s' -> %s\n", name, atoi(arg) ? "passable" : "solid");
        } else {
            printf("[amberscript] tile_set_passable: failed for '%s' (must be a tile_art_load'd asset)\n", name);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_set_sign") == 0) {
        char xs[16] = {0}, ys[16] = {0}, text[PKS_MAX_TEXT] = {0};
        if (!AmberScript_ParseArg(cmd, 1, xs, sizeof(xs)) ||
            !AmberScript_ParseArg(cmd, 2, ys, sizeof(ys)) ||
            !AmberScript_ParseArg(cmd, 3, text, sizeof(text))) {
            printf("[amberscript] tile_set_sign usage: tile_set_sign <x> <y> \"<text>\"\n");
        } else if (AmberScript_TileSetSign(atoi(xs), atoi(ys), text)) {
            printf("[amberscript] tile_set_sign: (%s,%s) -> \"%s\"\n", xs, ys, text);
        } else {
            printf("[amberscript] tile_set_sign: failed at (%s,%s) (no placed cell there)\n", xs, ys);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_clear_sign") == 0) {
        char xs[16] = {0}, ys[16] = {0};
        if (!AmberScript_ParseArg(cmd, 1, xs, sizeof(xs)) ||
            !AmberScript_ParseArg(cmd, 2, ys, sizeof(ys))) {
            printf("[amberscript] tile_clear_sign usage: tile_clear_sign <x> <y>\n");
        } else if (AmberScript_TileSetSign(atoi(xs), atoi(ys), "")) {
            printf("[amberscript] tile_clear_sign: (%s,%s) cleared\n", xs, ys);
        } else {
            printf("[amberscript] tile_clear_sign: failed at (%s,%s) (no placed cell there)\n", xs, ys);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_set_warp") == 0) {
        char name[32] = {0}, m[16] = {0}, w[16] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name)) ||
            !AmberScript_ParseArg(cmd, 2, m, sizeof(m)) ||
            !AmberScript_ParseArg(cmd, 3, w, sizeof(w))) {
            printf("[amberscript] tile_set_warp usage: tile_set_warp <name> <dest_map> <dest_warp_idx>\n");
        } else if (AmberScript_TileSetWarp(name, atoi(m), atoi(w))) {
            printf("[amberscript] tile_set_warp: '%s' -> map %d warp %d\n", name, atoi(m), atoi(w));
        } else {
            printf("[amberscript] tile_set_warp: failed for '%s' (must be a tile_art_load'd asset; dest_map must be < %d)\n",
                   name, NUM_MAPS);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_clear_warp") == 0) {
        char name[32] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name))) {
            printf("[amberscript] tile_clear_warp usage: tile_clear_warp <name>\n");
        } else if (AmberScript_TileClearWarp(name)) {
            printf("[amberscript] tile_clear_warp: '%s' cleared\n", name);
        } else {
            printf("[amberscript] tile_clear_warp: failed for '%s' (must be a tile_art_load'd asset)\n", name);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_clear_passable") == 0) {
        char name[32] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name))) {
            printf("[amberscript] tile_clear_passable usage: tile_clear_passable <name>\n");
        } else if (AmberScript_TileClearPassable(name)) {
            printf("[amberscript] tile_clear_passable: '%s' now inherits the real tileset's passability\n", name);
        } else {
            printf("[amberscript] tile_clear_passable: unknown name '%s'\n", name);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_set_surfable") == 0) {
        char name[32] = {0}, arg[16] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name)) ||
            !AmberScript_ParseArg(cmd, 2, arg, sizeof(arg))) {
            printf("[amberscript] tile_set_surfable usage: tile_set_surfable <name> <0|1>\n");
        } else if (AmberScript_TileSetSurfable(name, atoi(arg))) {
            printf("[amberscript] tile_set_surfable: '%s' -> %s\n", name, atoi(arg) ? "water (surfable)" : "not water");
        } else {
            printf("[amberscript] tile_set_surfable: failed for '%s' (must be a tile_art_load'd asset)\n", name);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_clear_surfable") == 0) {
        char name[32] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name))) {
            printf("[amberscript] tile_clear_surfable usage: tile_clear_surfable <name>\n");
        } else if (AmberScript_TileClearSurfable(name)) {
            printf("[amberscript] tile_clear_surfable: '%s' no longer explicitly marked\n", name);
        } else {
            printf("[amberscript] tile_clear_surfable: unknown name '%s'\n", name);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_set_cuttable") == 0) {
        char name[32] = {0}, arg[16] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name)) ||
            !AmberScript_ParseArg(cmd, 2, arg, sizeof(arg))) {
            printf("[amberscript] tile_set_cuttable usage: tile_set_cuttable <name> <0|1>\n");
        } else if (AmberScript_TileSetCuttable(name, atoi(arg))) {
            printf("[amberscript] tile_set_cuttable: '%s' -> %s\n", name, atoi(arg) ? "cuttable" : "not cuttable");
        } else {
            printf("[amberscript] tile_set_cuttable: failed for '%s' (must be a tile_art_load'd asset)\n", name);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_clear_cuttable") == 0) {
        char name[32] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name))) {
            printf("[amberscript] tile_clear_cuttable usage: tile_clear_cuttable <name>\n");
        } else if (AmberScript_TileClearCuttable(name)) {
            printf("[amberscript] tile_clear_cuttable: '%s' no longer explicitly marked\n", name);
        } else {
            printf("[amberscript] tile_clear_cuttable: unknown name '%s'\n", name);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_set_cut_replacement") == 0) {
        char name[32] = {0}, repl[32] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name)) ||
            !AmberScript_ParseArg(cmd, 2, repl, sizeof(repl))) {
            printf("[amberscript] tile_set_cut_replacement usage: tile_set_cut_replacement <name> <replacement_quad_name>\n");
        } else if (AmberScript_TileSetCutReplacement(name, repl)) {
            printf("[amberscript] tile_set_cut_replacement: '%s' -> becomes '%s' once cut\n", name, repl);
        } else {
            printf("[amberscript] tile_set_cut_replacement: failed for '%s' (must be a tile_art_load'd asset)\n", name);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "map_export") == 0) {
        char name[32] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name))) {
            printf("[amberscript] map_export usage: map_export <name>\n");
        } else if (!AmberScript_MapExport(name)) {
            printf("[amberscript] map_export: failed (couldn't write to mod_runtime/map_export/)\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "block_tiles") == 0) {

        char id_s[16] = {0};
        int bid;
        if (!AmberScript_ParseArg(cmd, 1, id_s, sizeof(id_s))) {
            printf("[amberscript] block_tiles usage: block_tiles <metatile_id>\n");
        } else if (wCurMapTileset >= NUM_TILESETS) {
            printf("[amberscript] block_tiles: no tileset loaded\n");
        } else {
            bid = atoi(id_s);
            if (bid < 0 || bid > 255) {
                printf("[amberscript] block_tiles: metatile id must be 0-255\n");
            } else {
                const uint8_t *mt = &gTilesets[wCurMapTileset].blocks[bid * 16];
                char out_path[300];
                printf("[amberscript] block_tiles %d: tl=%d,%d,%d,%d tr=%d,%d,%d,%d "
                       "bl=%d,%d,%d,%d br=%d,%d,%d,%d\n", bid,
                       mt[0], mt[1], mt[4], mt[5],
                       mt[2], mt[3], mt[6], mt[7],
                       mt[8], mt[9], mt[12], mt[13],
                       mt[10], mt[11], mt[14], mt[15]);

                if (pks_resolve_output_path("map_export", "block_tiles", "txt", out_path, sizeof(out_path))) {
                    FILE *bf = fopen(out_path, "w");
                    if (bf) {
                        fprintf(bf, "%d %d,%d,%d,%d %d,%d,%d,%d %d,%d,%d,%d %d,%d,%d,%d\n", bid,
                                mt[0], mt[1], mt[4], mt[5],
                                mt[2], mt[3], mt[6], mt[7],
                                mt[8], mt[9], mt[12], mt[13],
                                mt[10], mt[11], mt[14], mt[15]);
                        fclose(bf);
                    }
                }
            }
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "map_edits_apply") == 0) {
        int applied = AmberScript_MapEditsApply();
        if (applied < 0) {
            printf("[amberscript] map_edits_apply: no edits file for map %d (mod_runtime/map_edits/map_%d.txt)\n",
                   (int)wCurMap, (int)wCurMap);
        } else {
            printf("[amberscript] map_edits_apply: %d edit(s) applied to map %d\n", applied, (int)wCurMap);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "block_def_load") == 0) {
        char path[256] = {0};
        if (!AmberScript_ParseArg(cmd, 1, path, sizeof(path))) {
            printf("[amberscript] block_def_load usage: block_def_load <path-to.block>\n");
        } else {
            int n = AmberScript_BlockDefLoad(path);
            if (n < 0) printf("[amberscript] block_def_load: failed to open '%s'\n", path);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_is_passable") == 0) {
        char xs[16] = {0}, ys[16] = {0};
        if (!AmberScript_ParseArg(cmd, 1, xs, sizeof(xs)) || !AmberScript_ParseArg(cmd, 2, ys, sizeof(ys))) {
            printf("[amberscript] tile_is_passable usage: tile_is_passable <x> <y>\n");
        } else {
            int gx = atoi(xs), gy = atoi(ys);
            int passable = Map_IsTilePassableAt(gx, gy);
            FILE *f = fopen("bugs/tile_is_passable.txt", "w");
            if (f) {
                fprintf(f, "(%d,%d) tile=%d passable=%d\n", gx, gy, (int)Map_GetGameTile(gx, gy), passable);
                fclose(f);
            }
            printf("[amberscript] tile_is_passable (%d,%d): %s\n", gx, gy, passable ? "passable" : "solid");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_copy") == 0 || strcmp(verb, "copy_tile") == 0) {
        char norm[192], x1[32], y1[32], x2[32], y2[32];
        int sx = 0, sy = 0, dx = 0, dy = 0;
        AmberScript_NormalizeCoordArgs(cmd + strlen(verb), norm, sizeof(norm));
        if (sscanf(norm, "%31s %31s %31s %31s", x1, y1, x2, y2) != 4 ||
            !AmberScript_ParseCoordExpr(x1, 1, &sx) || !AmberScript_ParseCoordExpr(y1, 0, &sy) ||
            !AmberScript_ParseCoordExpr(x2, 1, &dx) || !AmberScript_ParseCoordExpr(y2, 0, &dy)) {
            printf("[amberscript] tile_copy usage: tile_copy <src_x> <src_y> <dst_x> <dst_y>\n");
        } else if (AmberScript_TileCopy(sx, sy, dx, dy)) {
            printf("[amberscript] tile_copy: (%d,%d) -> (%d,%d)\n", sx, sy, dx, dy);
        } else {
            printf("[amberscript] tile_copy: failed\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_save") == 0) {
        char name[32] = {0};
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name))) {
            printf("[amberscript] tile_save usage: tile_save <name>\n");
        } else if (AmberScript_TileSaveRightOfPlayer(name)) {
            printf("[amberscript] tile_save: saved '%s' from (%d,%d)\n", name, (int)wXCoord + 1, (int)wYCoord);
        } else {
            printf("[amberscript] tile_save: failed\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "tile_place_custom") == 0 || strcmp(verb, "tile_place") == 0) {
        char name[32] = {0};
        int x = 0, y = 0;
        if (!AmberScript_ParseNamedCoordArgs(cmd + strlen(verb), name, sizeof(name), &x, &y)) {
            printf("[amberscript] tile_place_custom usage: tile_place_custom <name> <x> <y>\n");
        } else if (AmberScript_TilePlaceCustom(name, x, y)) {
            printf("[amberscript] tile_place_custom: '%s' -> (%d,%d)\n", name, x, y);
        } else if (AmberScript_BlockPlaceCustom(name, x, y)) {
            int slot = AmberScript_SavedBlockFind(name);
            int count = (slot >= 0) ? s_saved_blocks[slot].cell_count : 0;
            printf("[amberscript] tile_place_custom: block '%s' -> (%d,%d), cells=%d\n", name, x, y, count);
        } else {
            printf("[amberscript] tile_place_custom: failed for '%s'\n", name);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "block_save") == 0) {
        char name[32] = {0};
        int sx = 0, sy = 0, ex = 0, ey = 0;
        if (!AmberScript_ParseBlockSaveArgs(cmd + strlen(verb), name, sizeof(name), &sx, &sy, &ex, &ey)) {
            printf("[amberscript] block_save usage: block_save <name> start <x> <y> end <x> <y>\n");
        } else if (AmberScript_BlockSave(name, sx, sy, ex, ey)) {
            printf("[amberscript] block_save: saved '%s' from (%d,%d) to (%d,%d)\n", name, sx, sy, ex, ey);
        } else {
            printf("[amberscript] block_save: failed for '%s'\n", name);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "block_place_custom") == 0 || strcmp(verb, "block_place") == 0) {
        char name[32] = {0};
        int x = 0, y = 0;
        if (!AmberScript_ParseNamedCoordArgs(cmd + strlen(verb), name, sizeof(name), &x, &y)) {
            printf("[amberscript] block_place_custom usage: block_place_custom <name> <x> <y>\n");
        } else if (AmberScript_BlockPlaceCustom(name, x, y)) {
            int slot = AmberScript_SavedBlockFind(name);
            int count = (slot >= 0) ? s_saved_blocks[slot].cell_count : 0;
            printf("[amberscript] block_place_custom: '%s' -> (%d,%d), cells=%d\n", name, x, y, count);
        } else {
            printf("[amberscript] block_place_custom: failed for '%s'\n", name);
        }
        AmberScript_WriteState();
        return 1;
    }

    return 0;
}
