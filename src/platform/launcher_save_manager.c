#include "launcher_save_manager.h"

#include "data_dir.h"
#include "game_version.h"
#include "launcher_browse.h"
#include "launcher_draw.h"
#include "launcher_dropdown.h"
#include "launcher_location_preview.h"
#include "launcher_save_editor.h"
#include "assetpack.h"
#include "save.h"
#include "../data/map_data.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define sm_mkdir(p) _mkdir(p)
#define SM_SEP '\\'
#else
#include <unistd.h>
#define sm_mkdir(p) mkdir((p), 0755)
#define SM_SEP '/'
#endif

#define SM_MAX_SAVES 64
#define SM_PATH_MAX 1200
#define SM_ROWS 8
#define SM_ROW_H 30
#define SM_LIST_TOP 86

static const char *const kSaveExts[] = { "sav", NULL };

typedef struct {
    char path[SM_PATH_MAX];
    char file[160];
    char summary[128];
    int active;
    int valid;
    int blank;
    int new_action;
    char player[16];
    char location[64];
    char location_vmap[64];
    uint8_t location_map;
    uint8_t location_x;
    uint8_t location_y;
    uint16_t trainer_id;
    uint32_t money;
    uint8_t badge_bits;
    int caught;
    int seen;
    int party_count;
    uint8_t party_species[PARTY_LENGTH];
    char party_name[PARTY_LENGTH][20];
    uint8_t party_level[PARTY_LENGTH];
    uint16_t party_hp[PARTY_LENGTH];
    uint16_t party_max_hp[PARTY_LENGTH];
} sm_entry_t;

typedef struct {
    int available;
    uint8_t tiles[8][4][16];
    uint16_t palettes[4][4];
    SDL_Texture *earned[8];
    SDL_Texture *silhouette[8];
    SDL_Texture *money_glyph;
    uint8_t species_to_dex[256];
    SDL_Texture *front_sprite[152];
} sm_badge_art_t;

typedef struct {
    const char *id;
    const char *label;
    sm_entry_t entries[SM_MAX_SAVES];
    int count;
    int selected;
    int top;
    sm_badge_art_t badge_art;
    SDL_Texture *location_preview;
    char location_preview_key[160];
} sm_column_t;

static Uint32 sm_rgb555(SDL_PixelFormat *fmt, uint16_t c);

static void sm_badge_art_destroy(sm_badge_art_t *art) {
    for (int i = 0; i < 8; i++) {
        if (art->earned[i]) SDL_DestroyTexture(art->earned[i]);
        if (art->silhouette[i]) SDL_DestroyTexture(art->silhouette[i]);
    }
    if (art->money_glyph) SDL_DestroyTexture(art->money_glyph);
    for (int i = 0; i < 152; i++)
        if (art->front_sprite[i]) SDL_DestroyTexture(art->front_sprite[i]);
    memset(art, 0, sizeof *art);
}

static SDL_Texture *sm_font_glyph_texture(SDL_Renderer *r,
                                          const uint8_t tile[16]) {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
        0, 8, 8, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!surface) return NULL;
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
    for (int py = 0; py < 8; py++) {
        uint8_t lo = tile[py * 2];
        uint8_t hi = tile[py * 2 + 1];
        for (int px = 0; px < 8; px++) {
            int bit = 7 - px;
            if (((lo | hi) >> bit) & 1) {
                ((Uint32 *)surface->pixels)
                    [py * (surface->pitch / 4) + px] =
                    SDL_MapRGBA(surface->format, 0, 0, 0, 255);
            }
        }
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(r, surface);
    SDL_FreeSurface(surface);
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 12)
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
#endif
    }
    return texture;
}

static SDL_Texture *sm_front_sprite_texture(SDL_Renderer *r,
                                            const uint8_t *tiles,
                                            const uint16_t palette[4]) {
    uint8_t pixels[56][56] = {{0}};
    int min_x = 56, min_y = 56, max_x = -1, max_y = -1;
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
        0, 56, 56, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!surface) return NULL;
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
    for (int tile = 0; tile < 49; tile++) {
        int tx = (tile % 7) * 8;
        int ty = (tile / 7) * 8;
        const uint8_t *src = tiles + tile * 16;
        for (int py = 0; py < 8; py++) {
            uint8_t lo = src[py * 2];
            uint8_t hi = src[py * 2 + 1];
            for (int px = 0; px < 8; px++) {
                int bit = 7 - px;
                int shade = ((hi >> bit) & 1) * 2 + ((lo >> bit) & 1);
                if (!shade) continue;
                int sx = tx + px, sy = ty + py;
                pixels[sy][sx] = (uint8_t)shade;
                if (sx < min_x) min_x = sx;
                if (sx > max_x) max_x = sx;
                if (sy < min_y) min_y = sy;
                if (sy > max_y) max_y = sy;
            }
        }
    }
    if (max_x >= min_x && max_y >= min_y) {
        int offset_x = (56 - (max_x - min_x + 1)) / 2 - min_x;
        int offset_y = (56 - (max_y - min_y + 1)) / 2 - min_y;
        for (int sy = min_y; sy <= max_y; sy++) {
            for (int sx = min_x; sx <= max_x; sx++) {
                int shade = pixels[sy][sx];
                if (!shade) continue;
                int dx = sx + offset_x, dy = sy + offset_y;
                ((Uint32 *)surface->pixels)
                    [dy * (surface->pitch / 4) + dx] =
                    sm_rgb555(surface->format, palette[shade]);
            }
        }
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(r, surface);
    SDL_FreeSurface(surface);
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 12)
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
#endif
    }
    return texture;
}

static int sm_mount_version(const char *id) {
    char dir[256], err[512];
    Pkg_UnmountAll();
    snprintf(dir, sizeof dir, "packages/%s", id);
    if (Pkg_MountList(dir, err, sizeof err)) return 1;
    Pkg_UnmountAll();
    snprintf(dir, sizeof dir, "../packages/%s", id);
    if (Pkg_MountList(dir, err, sizeof err)) return 1;
    Pkg_UnmountAll();
    return 0;
}

static Uint32 sm_rgb555(SDL_PixelFormat *fmt, uint16_t c) {
    Uint8 r = (Uint8)(((c & 31u) * 255u + 15u) / 31u);
    Uint8 g = (Uint8)((((c >> 5) & 31u) * 255u + 15u) / 31u);
    Uint8 b = (Uint8)((((c >> 10) & 31u) * 255u + 15u) / 31u);
    return SDL_MapRGBA(fmt, r, g, b, 255);
}

static int sm_badge_tile_palette(int badge, int tile) {
    static const uint8_t map[8][4] = {
        { 0, 0, 0, 0 }, { 1, 1, 1, 1 },
        { 3, 3, 3, 3 }, { 0, 2, 1, 3 },
        { 2, 2, 2, 2 }, { 3, 3, 3, 3 },
        { 2, 2, 2, 2 }, { 1, 1, 1, 1 }
    };
    return map[badge][tile];
}

static SDL_Texture *sm_badge_texture(SDL_Renderer *r,
                                     const sm_badge_art_t *art,
                                     int badge, int silhouette) {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
        0, 16, 16, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!surface) return NULL;
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
    for (int tile = 0; tile < 4; tile++) {
        int tx = (tile & 1) * 8;
        int ty = (tile >> 1) * 8;
        int pal = sm_badge_tile_palette(badge, tile);
        for (int py = 0; py < 8; py++) {
            uint8_t lo = art->tiles[badge][tile][py * 2];
            uint8_t hi = art->tiles[badge][tile][py * 2 + 1];
            for (int px = 0; px < 8; px++) {
                int bit = 7 - px;
                int shade = ((hi >> bit) & 1) * 2 + ((lo >> bit) & 1);
                if (!shade) continue;
                Uint32 pixel = silhouette
                    ? SDL_MapRGBA(surface->format, 0x58, 0x58, 0x58, 255)
                    : sm_rgb555(surface->format, art->palettes[pal][shade]);
                ((Uint32 *)surface->pixels)[(ty + py) * (surface->pitch / 4) + tx + px] = pixel;
            }
        }
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(r, surface);
    SDL_FreeSurface(surface);
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 12)
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
#endif
    }
    return texture;
}

static int sm_badge_art_load(SDL_Renderer *r, const char *id,
                             sm_badge_art_t *art) {
    static const int palette_ids[4] = { 0x10, 0x22, 0x12, 0x18 };
    AssetPack_Entry tiles, palettes, font, fronts, mon_palettes, species_to_dex;
    sm_badge_art_destroy(art);
    if (!sm_mount_version(id)) return 0;
    if (!AssetPack_Find("kTrainerBadgeTiles", &tiles) ||
        !AssetPack_Find("gGbcRedSgbPalettes", &palettes) ||
        !AssetPack_Find("gFontTiles", &font) ||
        !AssetPack_Find("gPokemonFrontSprite", &fronts) ||
        !AssetPack_Find("gGbcYellowMonPaletteId", &mon_palettes) ||
        !AssetPack_Find("gSpeciesToDex", &species_to_dex) ||
        tiles.size < sizeof art->tiles || palettes.stride < 8 ||
        palettes.count <= 0x22 || font.stride < 16 || font.count <= 0x70 ||
        fronts.stride < 16 || fronts.size < 152u * 49u * 16u ||
        mon_palettes.count < 152 || species_to_dex.count < 256) {
        Pkg_UnmountAll();
        return 0;
    }
    memcpy(art->tiles, tiles.data, sizeof art->tiles);
    for (int p = 0; p < 4; p++)
        memcpy(art->palettes[p],
               (const uint8_t *)palettes.data + palette_ids[p] * palettes.stride,
               sizeof art->palettes[p]);
    art->money_glyph = sm_font_glyph_texture(
        r, (const uint8_t *)font.data + 0x70 * font.stride);
    memcpy(art->species_to_dex, species_to_dex.data,
           sizeof art->species_to_dex);
    for (int dex = 1; dex < 152; dex++) {
        int palette_id = ((const uint8_t *)mon_palettes.data)[dex];
        if (palette_id < 0 || (uint32_t)palette_id >= palettes.count)
            palette_id = 0x10;
        art->front_sprite[dex] = sm_front_sprite_texture(
            r, (const uint8_t *)fronts.data + dex * 49u * 16u,
            (const uint16_t *)((const uint8_t *)palettes.data +
                               palette_id * palettes.stride));
        if (!art->front_sprite[dex]) {
            Pkg_UnmountAll();
            sm_badge_art_destroy(art);
            return 0;
        }
    }
    Pkg_UnmountAll();
    if (!art->money_glyph) {
        sm_badge_art_destroy(art);
        return 0;
    }
    for (int i = 0; i < 8; i++) {
        art->earned[i] = sm_badge_texture(r, art, i, 0);
        art->silhouette[i] = sm_badge_texture(r, art, i, 1);
        if (!art->earned[i] || !art->silhouette[i]) {
            sm_badge_art_destroy(art);
            return 0;
        }
    }
    art->available = 1;
    return 1;
}

static int sm_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0;
}

static void sm_join(char *out, size_t n, const char *a, const char *b) {
    size_t len = strlen(a);
    snprintf(out, n, "%s%s%s", a,
             len && (a[len - 1] == '/' || a[len - 1] == '\\') ? "" :
#ifdef _WIN32
             "\\",
#else
             "/",
#endif
             b);
}

static int sm_copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    FILE *out;
    char buf[8192];
    size_t got;
    int ok = 1;
    if (!in) return 0;
    out = fopen(dst, "wb");
    if (!out) { fclose(in); return 0; }
    while ((got = fread(buf, 1, sizeof buf, in)) != 0)
        if (fwrite(buf, 1, got, out) != got) { ok = 0; break; }
    if (ferror(in)) ok = 0;
    fclose(in);
    if (fclose(out) != 0) ok = 0;
    if (!ok) remove(dst);
    return ok;
}

static void sm_companion(char *out, size_t n, const char *save,
                         const char *suffix) {
    snprintf(out, n, "%s%s", save, suffix);
}

static int sm_copy_unit(const char *src, const char *dst) {
    static const char *const extra[] = { ".vmaps", ".npcrt", NULL };
    char from[SM_PATH_MAX], to[SM_PATH_MAX];
    if (!sm_copy_file(src, dst)) return 0;
    for (int i = 0; extra[i]; i++) {
        sm_companion(from, sizeof from, src, extra[i]);
        sm_companion(to, sizeof to, dst, extra[i]);
        if (sm_exists(from) && !sm_copy_file(from, to)) {
            remove(dst);
            for (int k = 0; k < i; k++) {
                sm_companion(to, sizeof to, dst, extra[k]);
                remove(to);
            }
            return 0;
        }
    }
    return 1;
}

static void sm_remove_unit(const char *base) {
    char path[SM_PATH_MAX];
    remove(base);
    sm_companion(path, sizeof path, base, ".vmaps"); remove(path);
    sm_companion(path, sizeof path, base, ".npcrt"); remove(path);
}

static int sm_make_dir(const char *path) {
    if (sm_exists(path)) return 1;
    return sm_mkdir(path) == 0 || sm_exists(path);
}

static int sm_library_dir(const char *ver, char *out, size_t n) {
    char root[SM_PATH_MAX];
    if (!UserDataPath("saves", root, sizeof root) || !sm_make_dir(root)) return 0;
    sm_join(out, n, root, ver);
    return sm_make_dir(out);
}

static char sm_pokechar(uint8_t c) {
    if (c >= 0x80 && c <= 0x99) return (char)('A' + c - 0x80);
    if (c >= 0xA0 && c <= 0xB9) return (char)('a' + c - 0xA0);
    if (c >= 0xF6) return (char)('0' + c - 0xF6);
    if (c == 0x7f) return ' ';
    return 0;
}

static int sm_bits(const uint8_t *p, size_t n) {
    int total = 0;
    for (size_t i = 0; i < n; i++)
        for (uint8_t b = p[i]; b; b >>= 1) total += b & 1;
    return total;
}

static int sm_dex_bits(const uint8_t bits[19]) {
    return sm_bits(bits, 18) + sm_bits(&((uint8_t){ bits[18] & 0x7f }), 1);
}

static int sm_preview(const char *path, char *summary, size_t n,
                      char *player, size_t player_n) {
    save_peek_t p;
    int j = 0;
    if (Save_PeekFrom(path, &p) != 0) {
        snprintf(summary, n, "UNREADABLE OR INCOMPATIBLE SAVE");
        if (player_n) player[0] = '\0';
        return 0;
    }
    for (int i = 0; i < NAME_LENGTH - 1 && j + 1 < (int)player_n; i++) {
        char c = sm_pokechar(p.player_name[i]);
        if (!c) break;
        player[j++] = c;
    }
    player[j] = '\0';
    snprintf(summary, n, "%s - %d BADGE%s - %d SEEN",
             player[0] ? player : "PLAYER", sm_bits(&p.badges, 1),
             sm_bits(&p.badges, 1) == 1 ? "" : "S",
             sm_dex_bits(p.pokedex_owned));
    return 1;
}

static void sm_decode_name(const uint8_t *src, char *dst, size_t n) {
    size_t j = 0;
    if (!dst || !n) return;
    for (int i = 0; i < NAME_LENGTH - 1 && j + 1 < n; i++) {
        char c = sm_pokechar(src[i]);
        if (!c) break;
        dst[j++] = c;
    }
    dst[j] = '\0';
}

static void sm_load_details(sm_entry_t *e) {
    save_editor_data_t d;
    if (!e || !e->valid || Save_EditorRead(e->path, &d) != 0) return;
    sm_decode_name(d.player_name, e->player, sizeof e->player);
    e->trainer_id = d.player_id;
    e->money = d.money;
    e->badge_bits = d.badges;
    e->caught = sm_dex_bits(d.pokedex_owned);
    e->seen = sm_dex_bits(d.pokedex_seen);
    e->party_count = d.party_count > PARTY_LENGTH ? PARTY_LENGTH : d.party_count;
    if (d.cur_map >= PKS_VIRTUAL_MAP_FIRST) {
        int slot = d.cur_map - PKS_VIRTUAL_MAP_FIRST;
        if (slot >= 0 && slot < SAVE_EDITOR_VMAP_SLOT_COUNT &&
            d.vmap_bindings[slot][0]) {
            char display[64];
            snprintf(e->location_vmap, sizeof e->location_vmap, "%s",
                     d.vmap_bindings[slot]);
            LauncherLocation_DisplayName(e->location_vmap,
                                         display, sizeof display);
            snprintf(e->location, sizeof e->location, "%s", display);
        }
    }
    if (!e->location[0])
        snprintf(e->location, sizeof e->location, "MAP %u", d.cur_map);
    e->location_map = d.cur_map;
    e->location_x = d.x_coord;
    e->location_y = d.y_coord;
    for (int i = 0; i < e->party_count; i++) {
        e->party_species[i] = d.party_mons[i].base.species;
        sm_decode_name(d.party_nicks[i], e->party_name[i],
                       sizeof e->party_name[i]);
        if (!e->party_name[i][0])
            snprintf(e->party_name[i], sizeof e->party_name[i], "SPECIES %u",
                     (unsigned)d.party_mons[i].base.species);
        e->party_level[i] = d.party_mons[i].level;
        e->party_hp[i] = d.party_mons[i].base.hp;
        e->party_max_hp[i] = d.party_mons[i].max_hp;
    }
}

static int sm_sav_name(const char *name) {
    size_t n = strlen(name);
    return n > 4 && name[n - 4] == '.' &&
           (name[n - 3] == 's' || name[n - 3] == 'S') &&
           (name[n - 2] == 'a' || name[n - 2] == 'A') &&
           (name[n - 1] == 'v' || name[n - 1] == 'V');
}

static int sm_entry_cmp(const void *aa, const void *bb) {
    const sm_entry_t *a = (const sm_entry_t *)aa;
    const sm_entry_t *b = (const sm_entry_t *)bb;
    return strcmp(a->file, b->file);
}

static void sm_add_entry(sm_column_t *c, const char *path, const char *file,
                         int active) {
    sm_entry_t *e;
    char player[32];
    if (c->count >= SM_MAX_SAVES) return;
    e = &c->entries[c->count++];
    memset(e, 0, sizeof *e);
    snprintf(e->path, sizeof e->path, "%s", path);
    snprintf(e->file, sizeof e->file, "%s", file);
    e->active = active;
    e->valid = sm_preview(path, e->summary, sizeof e->summary,
                          player, sizeof player);
    sm_load_details(e);
}

static void sm_add_blank_active(sm_column_t *c) {
    sm_entry_t *e;
    if (c->count >= SM_MAX_SAVES) return;
    e = &c->entries[c->count++];
    memset(e, 0, sizeof *e);
    e->active = 1;
    e->blank = 1;
    snprintf(e->file, sizeof e->file, "NO SAVE DATA WRITTEN");
    snprintf(e->summary, sizeof e->summary, "NEW GAME - NOT SAVED YET");
}

static void sm_add_new_action(sm_column_t *c) {
    sm_entry_t *e;
    if (c->count >= SM_MAX_SAVES) return;
    e = &c->entries[c->count++];
    memset(e, 0, sizeof *e);
    e->new_action = 1;
    e->valid = 1;
    snprintf(e->file, sizeof e->file, "PRESERVES THE CURRENT ACTIVE SAVE");
    snprintf(e->summary, sizeof e->summary, "+ CREATE NEW SAVE");
}

static void sm_seed_legacy_backup(const char *ver, const char *dir) {
    char rel[128], old[SM_PATH_MAX], dst[SM_PATH_MAX];
    snprintf(rel, sizeof rel, "saves_backup/%s_prev.sav", ver);
    if (!UserDataPath(rel, old, sizeof old) || !sm_exists(old)) return;
    sm_join(dst, sizeof dst, dir, "previous.sav");
    if (!sm_exists(dst)) (void)sm_copy_unit(old, dst);
}

static void sm_scan(sm_column_t *c) {
    char dir[SM_PATH_MAX];
    DIR *d;
    struct dirent *de;
    c->count = 0;
    if (!sm_library_dir(c->id, dir, sizeof dir)) return;
    sm_seed_legacy_backup(c->id, dir);

    const char *active = GameVersion_SavePath(c->id);
    int has_active = sm_exists(active);
    if (has_active) sm_add_entry(c, active, "ACTIVE SAVE", 1);
    else sm_add_blank_active(c);

    d = opendir(dir);
    if (d) {
        while ((de = readdir(d)) != NULL) {
            char path[SM_PATH_MAX];
            if (de->d_name[0] == '.' || !sm_sav_name(de->d_name)) continue;
            sm_join(path, sizeof path, dir, de->d_name);
            sm_add_entry(c, path, de->d_name, 0);
        }
        closedir(d);
    }
    {
        int first_stored = 1;
        if (c->count - first_stored > 1)
            qsort(c->entries + first_stored,
                  (size_t)(c->count - first_stored),
                  sizeof c->entries[0], sm_entry_cmp);
    }
    if (has_active) sm_add_new_action(c);
    if (c->selected >= c->count) c->selected = c->count ? c->count - 1 : 0;
    if (c->selected < 0) c->selected = 0;
    if (c->selected < c->top) c->top = c->selected;
    if (c->selected >= c->top + SM_ROWS) c->top = c->selected - SM_ROWS + 1;
}

static void sm_safe_stem(const char *in, char *out, size_t n) {
    size_t j = 0;
    for (size_t i = 0; in && in[i] && j + 1 < n; i++) {
        unsigned char ch = (unsigned char)in[i];
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9')) out[j++] = (char)ch;
        else if (ch == ' ' || ch == '-' || ch == '_') out[j++] = '_';
    }
    if (!j) snprintf(out, n, "save");
    else out[j] = '\0';
}

static int sm_archive_path(const char *ver, const char *active,
                           char *out, size_t n) {
    char dir[SM_PATH_MAX], summary[128], player[32], stem[64], file[96];
    if (!sm_library_dir(ver, dir, sizeof dir)) return 0;
    (void)sm_preview(active, summary, sizeof summary, player, sizeof player);
    sm_safe_stem(player, stem, sizeof stem);
    for (int i = 1; i < 10000; i++) {
        snprintf(file, sizeof file, i == 1 ? "%s.sav" : "%s_%d.sav", stem, i);
        sm_join(out, n, dir, file);
        if (!sm_exists(out)) return 1;
    }
    return 0;
}

static int sm_activate(sm_column_t *c, int index) {
    char archive[SM_PATH_MAX] = "";
    char temp[SM_PATH_MAX];
    const char *active = GameVersion_SavePath(c->id);
    sm_entry_t *picked;
    if (index < 0 || index >= c->count) return 0;
    picked = &c->entries[index];
    if (picked->active || !picked->valid) return 0;

    snprintf(temp, sizeof temp, "%s.switching", active);
    sm_remove_unit(temp);
    if (!sm_copy_unit(picked->path, temp)) return 0;

    if (sm_exists(active)) {
        if (!sm_archive_path(c->id, active, archive, sizeof archive) ||
            !sm_copy_unit(active, archive)) {
            sm_remove_unit(temp);
            return 0;
        }
    }

    sm_remove_unit(active);
    if (!sm_copy_unit(temp, active)) {
        if (archive[0]) (void)sm_copy_unit(archive, active);
        sm_remove_unit(temp);
        return 0;
    }
    sm_remove_unit(temp);
    sm_remove_unit(picked->path);
    return 1;
}

static int sm_create_blank(sm_column_t *c) {
    char archive[SM_PATH_MAX];
    const char *active = GameVersion_SavePath(c->id);
    if (!sm_exists(active)) return 1;
    if (!sm_archive_path(c->id, active, archive, sizeof archive) ||
        !sm_copy_unit(active, archive)) return 0;
    sm_remove_unit(active);
    return 1;
}

static int sm_import(SDL_Renderer *r, SDL_Window *win, launcher_nav_t *nav,
                     sm_column_t *c) {
    char picked[SM_PATH_MAX], dir[SM_PATH_MAX], name[180], dst[SM_PATH_MAX];
    char summary[128], player[32];
    const char *base;
    if (!LauncherBrowse_Run(r, win, nav, "IMPORT A SAVE", kSaveExts,
                            NULL, picked, sizeof picked)) return 0;
    if (!sm_preview(picked, summary, sizeof summary, player, sizeof player)) return -1;
    if (!sm_library_dir(c->id, dir, sizeof dir)) return -1;
    base = strrchr(picked, '/');
    {
        const char *b = strrchr(picked, '\\');
        if (!base || (b && b > base)) base = b;
    }
    base = base ? base + 1 : picked;
    snprintf(name, sizeof name, "%s", base[0] ? base : "imported.sav");
    for (int i = 1; ; i++) {
        if (i > 1) {
            char stem[128];
            snprintf(stem, sizeof stem, "%s", base);
            char *dot = strrchr(stem, '.');
            if (dot) *dot = '\0';
            snprintf(name, sizeof name, "%s_%d.sav", stem, i);
        }
        sm_join(dst, sizeof dst, dir, name);
        if (!sm_exists(dst)) break;
    }
    return sm_copy_unit(picked, dst) ? 1 : -1;
}

static SDL_Rect sm_game_field(void) {
    return (SDL_Rect){ 96, 44, LDRAW_W - 112, 28 };
}

static SDL_Rect sm_list_box(void) {
    int w = (LDRAW_W - 40) * 22 / 100;
    return (SDL_Rect){ 16, 82, w, SM_ROWS * SM_ROW_H + 8 };
}

static SDL_Rect sm_detail_box(void) {
    SDL_Rect list = sm_list_box();
    return (SDL_Rect){ list.x + list.w + 8, 82,
                       LDRAW_W - (list.x + list.w + 8) - 16,
                       LDRAW_H - 116 };
}

static SDL_Rect sm_row_rect(int row) {
    SDL_Rect box = sm_list_box();
    return (SDL_Rect){ box.x + 4, SM_LIST_TOP + row * SM_ROW_H,
                       box.w - 8, SM_ROW_H - 2 };
}

static const char *sm_version_label(void *ctx, int index) {
    sm_column_t *versions = (sm_column_t *)ctx;
    return versions[index].label;
}

static void sm_tint_field(SDL_Renderer *r, const char *id, SDL_Rect field,
                          const char *label, int focused, int open) {
    if (id && strcmp(id, "red") == 0)
        LauncherDraw_SetChromeColors(0xff, 0xb9, 0xaa,
                                     0xff, 0xd8, 0xd0,
                                     0xa0, 0x50, 0x48);
    else if (id && strcmp(id, "blue") == 0)
        LauncherDraw_SetChromeColors(0xc8, 0xda, 0xe8,
                                     0xe8, 0xf2, 0xf8,
                                     0x58, 0x70, 0x88);
    LauncherDropdown_DrawField(r, field, label, focused, open);
    LauncherDraw_ResetChromeColors();
}

static const char *sm_primary_label(const sm_entry_t *e) {
    if (!e) return "SELECT";
    if (e->new_action) return "CREATE";
    if (e->active && !e->blank) return "EDIT";
    if (e->blank) return "READY";
    return "ACTIVATE";
}

static int sm_footer_buttons(const sm_entry_t *e,
                             ldraw_footer_btn_t buttons[5]) {
    buttons[0] = (ldraw_footer_btn_t){ "BACK" };
    buttons[1] = (ldraw_footer_btn_t){ "DELETE" };
    buttons[2] = (ldraw_footer_btn_t){ "IMPORT" };
    if (e && e->active && !e->blank) {
        buttons[3] = (ldraw_footer_btn_t){ sm_primary_label(e) };
        return 4;
    }
    buttons[3] = (ldraw_footer_btn_t){ "EDIT" };
    buttons[4] = (ldraw_footer_btn_t){ sm_primary_label(e) };
    return 5;
}

static void sm_location_preview_update(SDL_Renderer *r, sm_column_t *c,
                                       const sm_entry_t *e) {
    char key[160] = "";
    if (e && e->valid && e->location_vmap[0])
        snprintf(key, sizeof key, "%s|%s|%u|%u", c->id,
                 e->location_vmap, (unsigned)e->location_x,
                 (unsigned)e->location_y);
    if (strcmp(key, c->location_preview_key) == 0) return;
    if (c->location_preview) SDL_DestroyTexture(c->location_preview);
    c->location_preview = NULL;
    snprintf(c->location_preview_key, sizeof c->location_preview_key,
             "%s", key);
    if (key[0])
        c->location_preview = LauncherLocationPreview_Create(
            r, c->id, e->location_vmap, e->location_x, e->location_y);
}

static void sm_draw_details(SDL_Renderer *r, const char *version,
                            const sm_entry_t *e,
                            const sm_badge_art_t *badge_art,
                            SDL_Texture *location_preview) {
    static const char *const badges[8] = {
        "BLDR", "CASC", "THND", "RAIN",
        "SOUL", "MRSH", "VOLC", "ERTH"
    };
    SDL_Rect box = sm_detail_box();
    LauncherDraw_Bevel(r, box, 0);
    int x = box.x + 6, y = box.y + 5;
    int inner = box.w - 12;
    if (!e) return;
    if (e->new_action) {
        LauncherDraw_TextClippedBold(r, x, y, 2, LCOL_TEXT,
                                     "CREATE NEW SAVE", inner);
        LauncherDraw_TextClippedBold(r, x, y + 34, 1, LCOL_TEXT_DIM,
                                     "YOUR CURRENT ACTIVE SAVE WILL BE", inner);
        LauncherDraw_TextClippedBold(r, x, y + 50, 1, LCOL_TEXT_DIM,
                                     "PRESERVED IN THIS LIST.", inner);
        LauncherDraw_TextClippedBold(r, x, y + 82, 1, LCOL_TEXT_DIM,
                                     "THE NEXT GAME STARTS FROM THE BEGINNING.", inner);
        return;
    }
    if (e->blank) {
        LauncherDraw_TextClippedBold(r, x, y, 2, LCOL_TEXT,
                                     "NEW GAME", inner);
        LauncherDraw_TextClippedBold(r, x, y + 34, 1, LCOL_TEXT_DIM,
                                     "NO SAVE DATA HAS BEEN WRITTEN YET.", inner);
        LauncherDraw_TextClippedBold(r, x, y + 50, 1, LCOL_TEXT_DIM,
                                     "PLAY THIS VERSION TO BEGIN.", inner);
        return;
    }
    if (!e->valid) {
        LauncherDraw_TextClippedBold(r, x, y, 2, LCOL_ERROR,
                                     "UNREADABLE SAVE", inner);
        LauncherDraw_TextClippedBold(r, x, y + 34, 1, LCOL_TEXT_DIM,
                                     e->file, inner);
        return;
    }

    SDL_Rect identity = { x, y, inner, 29 };
    if (version && strcmp(version, "red") == 0)
        LauncherDraw_SetChromeColors(0xff, 0xb9, 0xaa,
                                     0xff, 0xe0, 0xd8,
                                     0xa0, 0x50, 0x48);
    else if (version && strcmp(version, "blue") == 0)
        LauncherDraw_SetChromeColors(0xc8, 0xda, 0xe8,
                                     0xe8, 0xf2, 0xf8,
                                     0x58, 0x70, 0x88);
    else
        LauncherDraw_ResetChromeColors();
    LauncherDraw_Bevel(r, identity, 0);
    LauncherDraw_ResetChromeColors();
    LauncherDraw_TextClippedBold(r, identity.x + 12,
                                 LDRAW_TEXT_Y(identity.y, identity.h, 2),
                                 2, LCOL_TEXT,
                                 e->player[0] ? e->player : "PLAYER",
                                 identity.w - 112);
    const char *state = e->active ? "ACTIVE" : "STORED";
    int state_w = LauncherDraw_TextWidthBold(1, state);
    if (e->active)
        LauncherDraw_TextBold(r, identity.x + identity.w - state_w - 12,
                              LDRAW_TEXT_Y(identity.y, identity.h, 1),
                              1, LCOL_OK, state);
    else
        LauncherDraw_TextBold(r, identity.x + identity.w - state_w - 12,
                              LDRAW_TEXT_Y(identity.y, identity.h, 1),
                              1, LCOL_TEXT_DIM, state);

    char line[128];
    int gap = 6;
    int left_w = (inner - gap) / 2;
    int right_x = x + left_w + gap;
    int right_w = inner - left_w - gap;

    SDL_Rect trainer = { x, y + 34, left_w, 94 };
    LauncherDraw_Bevel(r, trainer, 0);
    LauncherDraw_TextBold(r, trainer.x + 6, trainer.y + 5, 1,
                          LCOL_TEXT, "TRAINER INFORMATION");
    const char *labels[3] = { "TRAINER ID", "MONEY", "POKEDEX" };
    char values[3][48];
    snprintf(values[0], sizeof values[0], "%u", (unsigned)e->trainer_id);
    snprintf(values[1], sizeof values[1], "%u", (unsigned)e->money);
    snprintf(values[2], sizeof values[2], "%d SEEN  %d OWNED",
             e->seen, e->caught);
    int trainer_label_w = LauncherDraw_TextWidth(1, "TRAINER ID");
    for (int i = 0; i < 3; i++) {
        SDL_Rect field = { trainer.x + 5, trainer.y + 21 + i * 23,
                           trainer.w - 10, 22 };
        LauncherDraw_Bevel(r, field, 0);
        int row_y = LDRAW_TEXT_Y(field.y, field.h, 1);
        int trainer_value_x = field.x + 4 + trainer_label_w + 10;
        LauncherDraw_TextClipped(r, field.x + 4, row_y, 1,
                                 LCOL_TEXT_DIM, labels[i], trainer_label_w);
        if (i == 1 && badge_art && badge_art->money_glyph) {
            SDL_Rect glyph = { trainer_value_x, row_y, 8, 8 };
            SDL_RenderCopy(r, badge_art->money_glyph, NULL, &glyph);
        }
        int value_x = trainer_value_x +
            (i == 1 && badge_art && badge_art->money_glyph
                 ? LDRAW_ADVANCE(1) + 1 : 0);
        LauncherDraw_TextClippedBold(r, value_x,
                                     row_y, 1, LCOL_TEXT, values[i],
                                     field.x + field.w - value_x - 4);
    }

    SDL_Rect location = { right_x, y + 34, right_w, 140 };
    LauncherDraw_Bevel(r, location, 0);
    LauncherDraw_TextBold(r, location.x + 6, location.y + 5, 1,
                          LCOL_TEXT, "CURRENT LOCATION");
    snprintf(line, sizeof line, "%s  (%u,%u)", e->location,
             (unsigned)e->location_x, (unsigned)e->location_y);
    LauncherDraw_TextClippedBold(r, location.x + 6, location.y + 18,
                                 1, LCOL_TEXT, line, location.w - 12);
    if (location_preview) {
        SDL_Rect preview = { location.x + 6, location.y + 33, 112, 101 };
        SDL_RenderCopy(r, location_preview, NULL, &preview);
        SDL_SetRenderDrawColor(r, 0x38, 0x38, 0x38, 0xff);
        SDL_RenderDrawRect(r, &preview);
        int meta_x = preview.x + preview.w + 7;
        int meta_w = location.x + location.w - meta_x - 5;
        const char *meta_labels[4] = { "MAP ID", "GROUP", "X", "Y" };
        char meta_values[4][16];
        snprintf(meta_values[0], sizeof meta_values[0], "%03u",
                 (unsigned)e->location_map);
        snprintf(meta_values[1], sizeof meta_values[1], "KANTO");
        snprintf(meta_values[2], sizeof meta_values[2], "%u",
                 (unsigned)e->location_x);
        snprintf(meta_values[3], sizeof meta_values[3], "%u",
                 (unsigned)e->location_y);
        for (int i = 0; i < 4; i++) {
            SDL_Rect meta = { meta_x, location.y + 34 + i * 21,
                              meta_w, 20 };
            LauncherDraw_Bevel(r, meta, 0);
            int value_w = LauncherDraw_TextWidthBold(1, meta_values[i]);
            int value_x = meta.x + meta.w - value_w - 5;
            LauncherDraw_TextClipped(r, meta.x + 4,
                                     LDRAW_TEXT_Y(meta.y, meta.h, 1),
                                     1, LCOL_TEXT_DIM, meta_labels[i],
                                     value_x - meta.x - 8);
            LauncherDraw_TextBold(r, value_x,
                                         LDRAW_TEXT_Y(meta.y, meta.h, 1),
                                         1, LCOL_TEXT, meta_values[i]);
        }
    }

    SDL_Rect badge_panel = { x, y + 132, left_w, 45 };
    LauncherDraw_Bevel(r, badge_panel, 0);
    int badges_y = 137;
    snprintf(line, sizeof line, "BADGES  %d / 8", sm_bits(&e->badge_bits, 1));
    LauncherDraw_TextBold(r, badge_panel.x + 6, y + badges_y,
                          1, LCOL_TEXT, line);
    int badge_slot_w = (badge_panel.w - 8) / 8;
    for (int i = 0; i < 8; i++) {
        SDL_Rect badge = { badge_panel.x + 4 + i * badge_slot_w,
                           y + badges_y + 13,
                           badge_slot_w,
                           16 };
        if (badge_art && badge_art->available) {
            SDL_Texture *icon = (e->badge_bits & (1u << i))
                ? badge_art->earned[i] : badge_art->silhouette[i];
            SDL_Rect dst = { badge.x + (badge.w - 16) / 2,
                             badge.y + (badge.h - 16) / 2, 16, 16 };
            SDL_RenderCopy(r, icon, NULL, &dst);
        } else {
            Uint8 c = (e->badge_bits & (1u << i)) ? 0x00 : 0x78;
            int bw = LauncherDraw_TextWidth(1, badges[i]);
            LauncherDraw_Text(r, badge.x + (badge.w - bw) / 2,
                              LDRAW_TEXT_Y(badge.y, badge.h, 1),
                              1, c, c, c, badges[i]);
        }
    }

    int party_y = 181;
    LauncherDraw_TextBold(r, x, y + party_y, 1, LCOL_TEXT, "PARTY");
    if (e->party_count == 0)
        LauncherDraw_TextClipped(r, x, y + party_y + 18, 1,
                                 LCOL_TEXT_DIM, "NO POKEMON", inner);
    int party_gap = 4;
    int party_w = (inner - party_gap * 2) / 3;
    for (int i = 0; i < e->party_count; i++) {
        int col = i % 3, row = i / 3;
        SDL_Rect mon = { x + col * (party_w + party_gap),
                         y + party_y + 13 + row * 82, party_w, 78 };
        LauncherDraw_Bevel(r, mon, 0);
        int info_x = mon.x + 5;
        if (badge_art) {
            int dex = badge_art->species_to_dex[e->party_species[i]];
            if (dex > 0 && dex < 152 && badge_art->front_sprite[dex]) {
                SDL_Rect sprite = { mon.x + 5, mon.y + 18, 56, 56 };
                SDL_RenderCopy(r, badge_art->front_sprite[dex], NULL, &sprite);
                info_x = mon.x + 64;
            }
        }
        snprintf(line, sizeof line, "%s  L%u",
                 e->party_name[i][0] ? e->party_name[i] : "?",
                 (unsigned)e->party_level[i]);
        LauncherDraw_TextClippedBold(r, mon.x + 5, mon.y + 5,
                                     1, LCOL_TEXT, line,
                                     mon.w - 10);
        snprintf(line, sizeof line, "HP %u/%u",
                 (unsigned)e->party_hp[i], (unsigned)e->party_max_hp[i]);
        LauncherDraw_TextClipped(r, info_x, mon.y + 31,
                                 1, LCOL_TEXT_DIM, line,
                                 mon.x + mon.w - info_x - 5);
        int bar_x = info_x;
        int bar_w = mon.x + mon.w - bar_x - 6;
        if (bar_w > 12) {
            SDL_Rect bar = { bar_x, mon.y + 47, bar_w, 5 };
            SDL_SetRenderDrawColor(r, 0x28, 0x28, 0x28, 0xff);
            SDL_RenderFillRect(r, &bar);
            SDL_Rect fill = { bar.x + 1, bar.y + 1, 0, bar.h - 2 };
            if (e->party_max_hp[i])
                fill.w = (bar.w - 2) * e->party_hp[i] / e->party_max_hp[i];
            SDL_SetRenderDrawColor(r, 0x00, 0xc8, 0x78, 0xff);
            SDL_RenderFillRect(r, &fill);
        }
    }
}

static void sm_draw(SDL_Renderer *r, launcher_nav_t *nav, sm_column_t *c,
                    int version_index, int field_focus,
                    launcher_dropdown_t *dropdown,
                    const char *status, int status_err) {
    SDL_SetRenderDrawColor(r, LCOL_BG, 0xff);
    SDL_RenderClear(r);
    LauncherDraw_TextBold(r, 16, 16, 2, LCOL_TEXT, "SAVE MANAGER");
    if (status && status[0]) {
        if (status_err)
            LauncherDraw_TextClippedBold(r, 190, 22, 1, LCOL_ERROR,
                                         status, LDRAW_W - 206);
        else
            LauncherDraw_TextClippedBold(r, 190, 22, 1, LCOL_OK,
                                         status, LDRAW_W - 206);
    }

    SDL_Rect field = sm_game_field();
    LauncherDraw_TextBold(r, 16, LDRAW_TEXT_Y(field.y, field.h, 1),
                          1, LCOL_TEXT_DIM, "GAME");
    sm_tint_field(r, c->id, field, c->label, field_focus,
                  dropdown && dropdown->open);

    SDL_Rect box = sm_list_box();
    LauncherDraw_Bevel(r, box, 0);
    for (int row = 0; row < SM_ROWS; row++) {
        int idx = c->top + row;
        SDL_Rect rr = sm_row_rect(row);
        if (idx >= c->count) break;
        sm_entry_t *e = &c->entries[idx];
        int focused = !field_focus && idx == c->selected;
        int hover = LauncherNav_HoverHighlight(nav) &&
                    LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, rr);
        if (focused || hover) LauncherDraw_FocusBar(r, rr);
        Uint8 tc = (focused || hover) ? 0xff : 0x00;
        char first[180];
        if (e->new_action)
            snprintf(first, sizeof first, "+ CREATE NEW SAVE");
        else if (e->blank)
            snprintf(first, sizeof first, "NEW GAME");
        else
            snprintf(first, sizeof first, "%s", e->player[0] ? e->player : "PLAYER");
        LauncherDraw_TextClippedBold(r, rr.x + 7, rr.y + 4, 1,
                                     tc, tc, tc, first, rr.w - 14);
        const char *second = e->active ? "ACTIVE SAVE" : e->file;
        if (e->new_action) second = "PRESERVES THE CURRENT ACTIVE SAVE";
        else if (e->blank) second = "NOT SAVED YET";
        LauncherDraw_TextClipped(r, rr.x + 7, rr.y + 17, 1,
                                 focused || hover ? 0xd8 : 0x60,
                                 focused || hover ? 0xd8 : 0x60,
                                 focused || hover ? 0xd8 : 0x60,
                                 second, rr.w - 14);
    }
    sm_entry_t *detail_entry = c->count ? &c->entries[c->selected] : NULL;
    sm_location_preview_update(r, c, detail_entry);
    sm_draw_details(r, c->id, detail_entry, &c->badge_art,
                    c->location_preview);

    int pointer = LauncherNav_Device(nav) == LNAV_INPUT_POINTER;
    sm_entry_t *selected = c->count ? &c->entries[c->selected] : NULL;
    if (pointer) {
        ldraw_footer_btn_t buttons[5];
        int button_count = sm_footer_buttons(selected, buttons);
        LauncherDraw_FooterLayout(buttons, button_count);
        int hover = -1;
        for (int i = 0; i < button_count; i++)
            if (LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, buttons[i].rect))
                hover = i;
        LauncherDraw_FooterButtons(r, buttons, button_count, hover);
    } else {
        LauncherDraw_PromptBar(r, sm_primary_label(selected), "BACK",
                               "DELETE", "IMPORT");
    }
    if (dropdown && dropdown->open) LauncherDropdown_Draw(r, dropdown);
    SDL_RenderPresent(r);
}

static int sm_confirm_delete(SDL_Renderer *r, launcher_nav_t *nav,
                             const sm_column_t *c, const sm_entry_t *e) {
    int choice = 0;
    for (;;) {
        SDL_Event ev;
        nav->ptr_moved = nav->ptr_pressed = nav->ptr_released = 0;
        while (SDL_PollEvent(&ev)) LauncherNav_HandleEvent(nav, &ev, r);
        unsigned in = LauncherNav_Poll(nav);
        if (in & (LNAV_LEFT | LNAV_RIGHT)) choice = !choice;
        if (in & (LNAV_BACK | LNAV_CANCEL | LNAV_QUIT)) return 0;

        SDL_Rect panel = { 42, 92, LDRAW_W - 84, 174 };
        SDL_Rect no = { panel.x + panel.w / 2 + 8, panel.y + 118,
                        panel.w / 2 - 22, 32 };
        SDL_Rect yes = { panel.x + 14, panel.y + 118,
                         panel.w / 2 - 22, 32 };
        if (nav->ptr_pressed) {
            if (LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, no)) return 0;
            if (LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, yes)) return 1;
        }
        if (in & LNAV_ACCEPT) return choice == 1;

        SDL_SetRenderDrawColor(r, LCOL_BG, 0xff);
        SDL_RenderClear(r);
        LauncherDraw_Bevel(r, panel, 1);
        const char *question = "DO YOU REALLY WANT TO DELETE THIS SAVE?";
        int qw = LauncherDraw_TextWidthBold(1, question);
        LauncherDraw_TextBold(r, panel.x + (panel.w - qw) / 2,
                              panel.y + 22, 1, LCOL_TEXT, question);
        char line[180];
        snprintf(line, sizeof line, "%s - %s", c->label, e->summary);
        LauncherDraw_TextClippedBold(r, panel.x + 18, panel.y + 52,
                                     1, LCOL_TEXT_DIM, line, panel.w - 36);
        LauncherDraw_TextBold(r, panel.x + 18, panel.y + 78, 1,
                              LCOL_ERROR, "THIS CANNOT BE UNDONE.");
        LauncherDraw_Bevel(r, yes, 1);
        LauncherDraw_Bevel(r, no, 1);
        if (choice == 1) LauncherDraw_FocusBar(r, (SDL_Rect){yes.x+3,yes.y+3,yes.w-6,yes.h-6});
        else LauncherDraw_FocusBar(r, (SDL_Rect){no.x+3,no.y+3,no.w-6,no.h-6});
        Uint8 yc = choice == 1 ? 0xff : 0x00;
        Uint8 nc = choice == 0 ? 0xff : 0x00;
        LauncherDraw_TextBold(r, yes.x + (yes.w - LauncherDraw_TextWidthBold(1,"YES, DELETE"))/2,
                              LDRAW_TEXT_Y(yes.y,yes.h,1),1,yc,yc,yc,"YES, DELETE");
        LauncherDraw_TextBold(r, no.x + (no.w - LauncherDraw_TextWidthBold(1,"NO"))/2,
                              LDRAW_TEXT_Y(no.y,no.h,1),1,nc,nc,nc,"NO");
        SDL_RenderPresent(r);
        SDL_Delay(16);
    }
}

int LauncherSaveManager_Run(SDL_Renderer *r, SDL_Window *win,
                            launcher_nav_t *nav) {
    sm_column_t versions[GAMEVER_MAX];
    int n_versions = GameVersion_SupportedCount();
    if (n_versions > GAMEVER_MAX) n_versions = GAMEVER_MAX;
    memset(versions, 0, sizeof versions);
    for (int i = 0; i < n_versions; i++) {
        versions[i].id = GameVersion_IdAt(i);
        versions[i].label = GameVersion_Label(versions[i].id);
        sm_scan(&versions[i]);
        (void)sm_badge_art_load(r, versions[i].id, &versions[i].badge_art);
    }
    if (n_versions <= 0) return 0;

    int old_lw = LDRAW_W, old_lh = LDRAW_H, old_ww = 0, old_wh = 0;
    SDL_bool old_integer_scale = SDL_RenderGetIntegerScale(r);
    SDL_GetWindowSize(win, &old_ww, &old_wh);
    if (!(SDL_GetWindowFlags(win) &
          (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP))) {
        SDL_SetWindowSize(win, 1280, 960);
        SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED);
    }
    LauncherDraw_SetSize(640, 480);
    SDL_RenderSetLogicalSize(r, 640, 480);
    SDL_RenderSetIntegerScale(r, SDL_TRUE);

    char status[160] = "";
    int status_err = 0, version_index = 0, field_focus = 0;
    int changed = 0, running = 1;
    launcher_dropdown_t dropdown;
    memset(&dropdown, 0, sizeof dropdown);

    while (running) {
        SDL_Event ev;
        int key_edit = 0, key_import = 0, key_delete = 0;
        int pad_delete = 0, pad_import = 0;
        int wheel = 0;
        nav->ptr_moved = nav->ptr_pressed = nav->ptr_released = 0;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_e) key_edit = 1;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_i) key_import = 1;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_DELETE) key_delete = 1;
            if (!dropdown.open && ev.type == SDL_CONTROLLERBUTTONDOWN &&
                ev.cbutton.button == SDL_CONTROLLER_BUTTON_X) pad_delete = 1;
            if (!dropdown.open && ev.type == SDL_CONTROLLERBUTTONDOWN &&
                ev.cbutton.button == SDL_CONTROLLER_BUTTON_Y) pad_import = 1;
            if (ev.type == SDL_MOUSEWHEEL) wheel = ev.wheel.y < 0 ? 1 : -1;
            LauncherNav_HandleEvent(nav, &ev, r);
        }
        unsigned in = LauncherNav_Poll(nav);
        if (pad_delete) in &= ~LNAV_CANCEL;
        if (in & LNAV_QUIT) break;

        if (dropdown.open) {
            if (wheel) LauncherDropdown_Wheel(&dropdown, wheel);
            int picked = LauncherDropdown_Tick(&dropdown, nav, in);
            if (picked >= 0 && picked < n_versions) {
                version_index = picked;
                status[0] = '\0';
            }
            sm_draw(r, nav, &versions[version_index], version_index,
                    1, &dropdown, status, status_err);
            SDL_Delay(16);
            continue;
        }

        sm_column_t *c = &versions[version_index];
        SDL_Rect field = sm_game_field();
        if (nav->ptr_pressed &&
            LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, field)) {
            field_focus = 1;
            LauncherDropdown_Open(&dropdown, field, n_versions, version_index,
                                  1, 0, sm_version_label, versions);
        }
        if (!field_focus && (in & LNAV_UP) && c->selected == 0) field_focus = 1;
        else if (field_focus && (in & LNAV_DOWN)) field_focus = 0;
        else if (!field_focus && (in & LNAV_UP) && c->selected > 0) c->selected--;
        else if (!field_focus && (in & LNAV_DOWN) && c->selected + 1 < c->count) c->selected++;
        if (field_focus && (in & LNAV_ACCEPT))
            LauncherDropdown_Open(&dropdown, field, n_versions, version_index,
                                  0, 0, sm_version_label, versions);
        if (c->selected < c->top) c->top = c->selected;
        if (c->selected >= c->top + SM_ROWS) c->top = c->selected - SM_ROWS + 1;

        int primary = !field_focus && (in & LNAV_ACCEPT) != 0;
        int edit = key_edit || (in & LNAV_PAGE_UP);
        int import = key_import || pad_import || (in & LNAV_PAGE_DOWN);
        int deleting = key_delete || pad_delete;

        if (nav->ptr_pressed) {
            int hit_row = -1;
            for (int row = 0; row < SM_ROWS; row++) {
                int idx = c->top + row;
                if (idx < c->count &&
                    LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y,
                                             sm_row_rect(row))) {
                    hit_row = idx;
                }
            }
            if (hit_row >= 0) {
                primary = !field_focus && hit_row == c->selected;
                field_focus = 0;
                c->selected = hit_row;
            }

            sm_entry_t *selected = c->count ? &c->entries[c->selected] : NULL;
            int active_primary = selected && selected->active && !selected->blank;
            ldraw_footer_btn_t buttons[5];
            int button_count = sm_footer_buttons(selected, buttons);
            LauncherDraw_FooterLayout(buttons, button_count);
            if (LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, buttons[0].rect)) running = 0;
            if (LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, buttons[1].rect)) deleting = 1;
            if (LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, buttons[2].rect)) import = 1;
            if (LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, buttons[3].rect)) {
                if (active_primary) primary = 1;
                else edit = 1;
            }
            if (!active_primary &&
                LauncherDraw_PointInRect(nav->ptr_x, nav->ptr_y, buttons[4].rect))
                primary = 1;
        }

        if (in & (LNAV_BACK | LNAV_CANCEL)) running = 0;
        if (!running) break;

        sm_entry_t *e = c->count ? &c->entries[c->selected] : NULL;
        if (deleting && e && !e->blank && !e->new_action) {
            if (sm_confirm_delete(r, nav, c, e)) {
                sm_remove_unit(e->path);
                changed = 1;
                sm_scan(c);
                snprintf(status, sizeof status, "%s SAVE DELETED", c->label);
                status_err = 0;
            }
        } else if (import) {
            int rc = sm_import(r, win, nav, c);
            if (rc > 0) {
                snprintf(status, sizeof status, "%s SAVE IMPORTED", c->label);
                status_err = 0;
                sm_scan(c);
            } else if (rc < 0) {
                snprintf(status, sizeof status, "SAVE COULD NOT BE IMPORTED");
                status_err = 1;
            }
        } else if (edit && e && e->valid && !e->new_action) {
            (void)LauncherSaveEditor_Run(r, win, nav, e->path, c->label);
            changed = 1;
            sm_scan(c);
            snprintf(status, sizeof status, "%s SAVE EDITOR CLOSED", c->label);
            status_err = 0;
        } else if (primary && e && e->new_action) {
            if (sm_create_blank(c)) {
                changed = 1;
                sm_scan(c);
                c->selected = 0;
                c->top = 0;
                snprintf(status, sizeof status, "%s NEW SAVE READY", c->label);
                status_err = 0;
            } else {
                snprintf(status, sizeof status, "COULD NOT CREATE NEW SAVE");
                status_err = 1;
            }
        } else if (primary && e && e->valid) {
            if (e->active && !e->blank) {
                (void)LauncherSaveEditor_Run(r, win, nav, e->path, c->label);
                changed = 1;
                sm_scan(c);
                snprintf(status, sizeof status, "%s SAVE EDITOR CLOSED", c->label);
                status_err = 0;
            } else if (sm_activate(c, c->selected)) {
                changed = 1;
                sm_scan(c);
                c->selected = 0;
                c->top = 0;
                snprintf(status, sizeof status, "%s ACTIVE SAVE CHANGED", c->label);
                status_err = 0;
            } else {
                snprintf(status, sizeof status, "COULD NOT ACTIVATE SAVE");
                status_err = 1;
            }
        }

        sm_draw(r, nav, c, version_index, field_focus,
                &dropdown, status, status_err);
        SDL_Delay(16);
    }
    for (int i = 0; i < n_versions; i++)
        sm_badge_art_destroy(&versions[i].badge_art);
    for (int i = 0; i < n_versions; i++)
        if (versions[i].location_preview)
            SDL_DestroyTexture(versions[i].location_preview);
    Pkg_UnmountAll();
    LauncherDraw_SetSize(old_lw, old_lh);
    SDL_RenderSetLogicalSize(r, old_lw, old_lh);
    SDL_RenderSetIntegerScale(r, old_integer_scale);
    SDL_SetWindowSize(win, old_ww, old_wh);
    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);
    return changed;
}
