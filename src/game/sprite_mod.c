#include "sprite_mod.h"

#include "../data/pokemon_sprites.h"
#include <stdio.h>
#include <string.h>

static uint8_t s_front_override[256][POKEMON_FRONT_CANVAS_TILES][16];
static uint8_t s_front_override_used[256];
static uint8_t s_back_override[256][POKEMON_BACK_TILES][16];
static uint8_t s_back_override_used[256];

int SpriteMod_LoadFrontFromFile(uint8_t species, const char *path) {
    FILE *fp = NULL;
    size_t need = (size_t)POKEMON_FRONT_CANVAS_TILES * 16u;
    size_t got;
    char pbuf[320];
    if (!path || !*path) return 0;
    fp = fopen(path, "rb");
    if (!fp) {
        snprintf(pbuf, sizeof(pbuf), "../%s", path);
        fp = fopen(pbuf, "rb");
    }
    if (!fp) return 0;
    got = fread(s_front_override[species], 1, need, fp);
    fclose(fp);
    if (got != need) return 0;
    s_front_override_used[species] = 1;
    return 1;
}

const uint8_t *SpriteMod_GetFrontTile(uint8_t species, int tile_idx) {
    if (tile_idx < 0 || tile_idx >= POKEMON_FRONT_CANVAS_TILES) return NULL;
    if (!s_front_override_used[species]) return NULL;
    return s_front_override[species][tile_idx];
}

int SpriteMod_LoadBackFromFile(uint8_t species, const char *path) {
    FILE *fp = NULL;
    size_t need = (size_t)POKEMON_BACK_TILES * 16u;
    size_t got;
    char pbuf[320];
    if (!path || !*path) return 0;
    fp = fopen(path, "rb");
    if (!fp) {
        snprintf(pbuf, sizeof(pbuf), "../%s", path);
        fp = fopen(pbuf, "rb");
    }
    if (!fp) return 0;
    got = fread(s_back_override[species], 1, need, fp);
    fclose(fp);
    if (got != need) return 0;
    s_back_override_used[species] = 1;
    return 1;
}

const uint8_t *SpriteMod_GetBackTile(uint8_t species, int tile_idx) {
    if (tile_idx < 0 || tile_idx >= POKEMON_BACK_TILES) return NULL;
    if (!s_back_override_used[species]) return NULL;
    return s_back_override[species][tile_idx];
}
