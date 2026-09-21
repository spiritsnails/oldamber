#include "launcher_location_preview.h"

#include "data_dir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LP_W 160
#define LP_H 144
#define LP_MAX_SUBTILES 192
#define LP_MAX_BLOCKS 256

typedef struct {
    char name[48];
    char path[256];
    uint8_t pixels[16];
    int loaded;
} lp_subtile_t;

typedef struct {
    char name[48];
    char subtile[4][48];
} lp_block_t;

static int lp_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0;
}

static int lp_resolve(char *out, size_t n, const char *relative) {
    char data[1200];
    while (relative[0] == '.' && relative[1] == '.' &&
           (relative[2] == '/' || relative[2] == '\\'))
        relative += 3;
    if (DataDir_Get(data, sizeof data) &&
        (size_t)snprintf(out, n, "%s%s", data, relative) < n &&
        lp_exists(out)) return 1;
    if ((size_t)snprintf(out, n, "%s", relative) < n && lp_exists(out))
        return 1;
    if ((size_t)snprintf(out, n, "../%s", relative) < n && lp_exists(out))
        return 1;
    out[0] = '\0';
    return 0;
}

static int lp_subtile_index(lp_subtile_t *items, int count, const char *name) {
    for (int i = 0; i < count; i++)
        if (strcmp(items[i].name, name) == 0) return i;
    return -1;
}

static int lp_block_index(lp_block_t *items, int count, const char *name) {
    for (int i = 0; i < count; i++)
        if (strcmp(items[i].name, name) == 0) return i;
    return -1;
}

static int lp_load_subtile(lp_subtile_t *tile) {
    char bin_relative[300], path[1400];
    FILE *f;
    if (tile->loaded) return tile->loaded > 0;
    snprintf(bin_relative, sizeof bin_relative, "%s", tile->path);
    char *dot = strrchr(bin_relative, '.');
    if (dot) snprintf(dot, (size_t)(bin_relative + sizeof bin_relative - dot), ".bin");
    else if (strlen(bin_relative) + 4 < sizeof bin_relative)
        strcat(bin_relative, ".bin");
    if (!lp_resolve(path, sizeof path, bin_relative)) {
        tile->loaded = -1;
        return 0;
    }
    f = fopen(path, "rb");
    if (!f || fread(tile->pixels, 1, sizeof tile->pixels, f) != sizeof tile->pixels) {
        if (f) fclose(f);
        tile->loaded = -1;
        return 0;
    }
    fclose(f);
    tile->loaded = 1;
    return 1;
}

static void lp_draw_subtile(uint32_t *pixels, int dx, int dy,
                            const uint8_t *tile) {
    static const uint32_t colors[4] = {
        0xffc8f5d0u, 0xff75c98au, 0xff34775du, 0xff123a38u
    };
    for (int y = 0; y < 8; y++) {
        uint8_t lo = tile[y * 2], hi = tile[y * 2 + 1];
        for (int x = 0; x < 8; x++) {
            int px = dx + x, py = dy + y, bit = 7 - x;
            int shade;
            if (px < 0 || py < 0 || px >= LP_W || py >= LP_H) continue;
            shade = (((hi >> bit) & 1) << 1) | ((lo >> bit) & 1);
            pixels[py * LP_W + px] = colors[shade];
        }
    }
}

static int lp_parse_blocks(const char *path, lp_subtile_t *subtiles,
                           int *subtile_count, lp_block_t *blocks,
                           int *block_count) {
    FILE *f = fopen(path, "rb");
    char line[1024], current[48] = "";
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        char a[48], b[300], c[48], d[48];
        if (sscanf(line, "subtile %47s %299s", a, b) == 2 &&
            *subtile_count < LP_MAX_SUBTILES) {
            lp_subtile_t *s = &subtiles[(*subtile_count)++];
            memset(s, 0, sizeof *s);
            snprintf(s->name, sizeof s->name, "%s", a);
            snprintf(s->path, sizeof s->path, "%s", b);
        } else if (sscanf(line, "block %47s", a) == 1) {
            snprintf(current, sizeof current, "%s", a);
        } else if (current[0] &&
                   sscanf(line, " source quad %47s %47s %47s %47s",
                          a, b, c, d) == 4 &&
                   *block_count < LP_MAX_BLOCKS) {
            lp_block_t *block = &blocks[(*block_count)++];
            memset(block, 0, sizeof *block);
            snprintf(block->name, sizeof block->name, "%s", current);
            snprintf(block->subtile[0], sizeof block->subtile[0], "%s", a);
            snprintf(block->subtile[1], sizeof block->subtile[1], "%s", b);
            snprintf(block->subtile[2], sizeof block->subtile[2], "%s", c);
            snprintf(block->subtile[3], sizeof block->subtile[3], "%s", d);
            current[0] = '\0';
        }
    }
    fclose(f);
    return *block_count > 0 && *subtile_count > 0;
}

SDL_Texture *LauncherLocationPreview_Create(SDL_Renderer *renderer,
                                            const char *version,
                                            const char *vmap,
                                            uint8_t player_x,
                                            uint8_t player_y) {
    lp_subtile_t *subtiles = calloc(LP_MAX_SUBTILES, sizeof *subtiles);
    lp_block_t *blocks = calloc(LP_MAX_BLOCKS, sizeof *blocks);
    uint32_t *pixels = malloc(LP_W * LP_H * sizeof *pixels);
    SDL_Texture *texture = NULL;
    char relative[512], blocks_path[1400], edits_path[1400], line[256];
    int subtile_count = 0, block_count = 0;
    FILE *edits = NULL;
    if (!renderer || !version || !vmap || !subtiles || !blocks || !pixels)
        goto done;
    snprintf(relative, sizeof relative,
             "mod_runtime/generatedmaps/%s/blocks/%s.block", version, vmap);
    if (!lp_resolve(blocks_path, sizeof blocks_path, relative)) goto done;
    snprintf(relative, sizeof relative,
             "mod_runtime/generatedmaps/%s/map_edits/vmap_%s.txt", version, vmap);
    if (!lp_resolve(edits_path, sizeof edits_path, relative)) goto done;
    if (!lp_parse_blocks(blocks_path, subtiles, &subtile_count,
                         blocks, &block_count)) goto done;
    for (int i = 0; i < LP_W * LP_H; i++) pixels[i] = 0xffc8f5d0u;
    edits = fopen(edits_path, "rb");
    if (!edits) goto done;
    {
        int origin_x = (int)player_x * 16 + 8 - LP_W / 2;
        int origin_y = (int)player_y * 16 + 8 - LP_H / 2;
        while (fgets(line, sizeof line, edits)) {
            int cell_x, cell_y;
            char kind[24], block_name[48];
            if (sscanf(line, "%d %d %23s %47s",
                       &cell_x, &cell_y, kind, block_name) != 4 ||
                strcmp(kind, "custom") != 0) continue;
            int dx = cell_x * 16 - origin_x;
            int dy = cell_y * 16 - origin_y;
            if (dx <= -16 || dy <= -16 || dx >= LP_W || dy >= LP_H) continue;
            int bi = lp_block_index(blocks, block_count, block_name);
            if (bi < 0) continue;
            for (int q = 0; q < 4; q++) {
                int si = lp_subtile_index(subtiles, subtile_count,
                                          blocks[bi].subtile[q]);
                if (si < 0 || !lp_load_subtile(&subtiles[si])) continue;
                lp_draw_subtile(pixels, dx + (q & 1) * 8,
                                dy + (q >> 1) * 8, subtiles[si].pixels);
            }
        }
    }
    fclose(edits);
    edits = NULL;
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STATIC, LP_W, LP_H);
    if (texture) {
        SDL_UpdateTexture(texture, NULL, pixels, LP_W * (int)sizeof *pixels);
#if SDL_VERSION_ATLEAST(2, 0, 12)
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
#endif
    }
done:
    if (edits) fclose(edits);
    free(pixels);
    free(blocks);
    free(subtiles);
    return texture;
}
