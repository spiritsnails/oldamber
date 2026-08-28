#include "species_mod.h"

#include "../data/base_stats.h"
#include "constants.h"
#include "pokemon.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPECIESMOD_ALIAS_MAX 64
#define SPECIESMOD_NAME_MAX 128
#define SPECIESMOD_STATS_MAX 128
#define SPECIESMOD_LEVEL_MOVES_MAX 512
#define SPECIESMOD_BANK_PATH "bugs/species_bank.txt"

typedef struct {
    int used;
    char name[24];
    uint8_t species;
} species_alias_t;

typedef struct {
    int used;
    uint8_t species;
    char name[24];
} species_name_t;

typedef struct {
    int used;
    uint8_t species;
    base_stats_t bs;
} species_stats_t;

typedef struct {
    int used;
    uint8_t species;
    uint8_t level;
    uint8_t move;
} species_level_move_t;

static species_alias_t s_aliases[SPECIESMOD_ALIAS_MAX];
static species_name_t s_names[SPECIESMOD_NAME_MAX];
static species_stats_t s_stats[SPECIESMOD_STATS_MAX];
static species_level_move_t s_level_moves[SPECIESMOD_LEVEL_MOVES_MAX];
static int s_bank_enabled = 0;

static void speciesmod_norm(const char *in, char *out, size_t out_sz) {
    size_t n = 0;
    if (!in || !out || out_sz == 0) return;
    for (size_t i = 0; in[i] && n + 1 < out_sz; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isalnum(c)) out[n++] = (char)toupper(c);
        else if (c == '_' || c == '-' || c == ' ') out[n++] = '_';
    }
    out[n] = '\0';
}

static void speciesmod_upper_ascii(const char *in, char *out, size_t out_sz) {
    size_t n = 0;
    if (!in || !out || out_sz == 0) return;
    for (size_t i = 0; in[i] && n + 1 < out_sz; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c < 32 || c > 126) continue;
        out[n++] = (char)toupper(c);
    }
    out[n] = '\0';
}

int SpeciesMod_ResolveSpeciesToken(const char *tok, uint8_t *out_species) {
    char n[24];
    char *end = NULL;
    long v;
    if (!tok || !*tok || !out_species) return 0;
    v = strtol(tok, &end, 0);
    if (end != tok && *end == '\0' && v >= 0 && v <= 255) {
        *out_species = (uint8_t)v;
        return 1;
    }
    speciesmod_norm(tok, n, sizeof(n));
    for (int i = 0; i < SPECIESMOD_ALIAS_MAX; i++) {
        if (!s_aliases[i].used) continue;
        if (strcmp(s_aliases[i].name, n) == 0) {
            *out_species = s_aliases[i].species;
            return 1;
        }
    }
    return 0;
}

void SpeciesMod_DefineAlias(const char *name, uint8_t species_id) {
    char n[24];
    int slot = -1;
    speciesmod_norm(name, n, sizeof(n));
    if (!n[0]) return;
    for (int i = 0; i < SPECIESMOD_ALIAS_MAX; i++) {
        if (s_aliases[i].used && strcmp(s_aliases[i].name, n) == 0) { slot = i; break; }
        if (!s_aliases[i].used && slot < 0) slot = i;
    }
    if (slot < 0) return;
    s_aliases[slot].used = 1;
    s_aliases[slot].species = species_id;
    snprintf(s_aliases[slot].name, sizeof(s_aliases[slot].name), "%s", n);
    if (s_bank_enabled) SpeciesMod_BankSave();
}

void SpeciesMod_SetName(uint8_t species, const char *name) {
    char up[24];
    int slot = -1;
    speciesmod_upper_ascii(name, up, sizeof(up));
    if (!up[0]) return;
    for (int i = 0; i < SPECIESMOD_NAME_MAX; i++) {
        if (s_names[i].used && s_names[i].species == species) { slot = i; break; }
        if (!s_names[i].used && slot < 0) slot = i;
    }
    if (slot < 0) return;
    s_names[slot].used = 1;
    s_names[slot].species = species;
    snprintf(s_names[slot].name, sizeof(s_names[slot].name), "%s", up);
    if (s_bank_enabled) SpeciesMod_BankSave();
}

const char *SpeciesMod_GetName(uint8_t species) {
    for (int i = 0; i < SPECIESMOD_NAME_MAX; i++) {
        if (s_names[i].used && s_names[i].species == species) return s_names[i].name;
    }
    return NULL;
}

void SpeciesMod_SetBaseStats(uint8_t species, const base_stats_t *bs) {
    int slot = -1;
    if (!bs) return;
    for (int i = 0; i < SPECIESMOD_STATS_MAX; i++) {
        if (s_stats[i].used && s_stats[i].species == species) { slot = i; break; }
        if (!s_stats[i].used && slot < 0) slot = i;
    }
    if (slot < 0) return;
    s_stats[slot].used = 1;
    s_stats[slot].species = species;
    s_stats[slot].bs = *bs;
    if (s_bank_enabled) SpeciesMod_BankSave();
}

int SpeciesMod_GetBaseStats(uint8_t species, base_stats_t *out_bs) {
    uint8_t dex;
    if (!out_bs) return 0;
    for (int i = 0; i < SPECIESMOD_STATS_MAX; i++) {
        if (s_stats[i].used && s_stats[i].species == species) {
            *out_bs = s_stats[i].bs;
            return 1;
        }
    }
    dex = gSpeciesToDex[species];
    if (dex >= 1 && dex <= NUM_POKEMON) {
        *out_bs = gBaseStats[dex];
        return 1;
    }
    return 0;
}

void SpeciesMod_SetStartMoves(uint8_t species, uint8_t m1, uint8_t m2, uint8_t m3, uint8_t m4) {
    base_stats_t bs;
    if (!SpeciesMod_GetBaseStats(species, &bs)) return;
    bs.start_moves[0] = m1;
    bs.start_moves[1] = m2;
    bs.start_moves[2] = m3;
    bs.start_moves[3] = m4;
    SpeciesMod_SetBaseStats(species, &bs);
}

void SpeciesMod_AddLevelMove(uint8_t species, uint8_t level, uint8_t move) {
    int slot = -1;
    for (int i = 0; i < SPECIESMOD_LEVEL_MOVES_MAX; i++) {
        if (!s_level_moves[i].used && slot < 0) slot = i;
    }
    if (slot < 0) return;
    s_level_moves[slot].used = 1;
    s_level_moves[slot].species = species;
    s_level_moves[slot].level = level;
    s_level_moves[slot].move = move;
    if (s_bank_enabled) SpeciesMod_BankSave();
}

int SpeciesMod_GetLevelMoves(uint8_t species, uint8_t level, uint8_t out_moves[32], uint8_t *out_count) {
    uint8_t count = 0;
    if (!out_moves || !out_count) return 0;
    for (int i = 0; i < SPECIESMOD_LEVEL_MOVES_MAX; i++) {
        if (!s_level_moves[i].used) continue;
        if (s_level_moves[i].species != species) continue;
        if (s_level_moves[i].level > level) continue;
        if (count < 32) out_moves[count++] = s_level_moves[i].move;
    }
    *out_count = count;
    return count > 0 ? 1 : 0;
}

void SpeciesMod_BankSetEnabled(int enabled) { s_bank_enabled = enabled ? 1 : 0; }
int SpeciesMod_BankIsEnabled(void) { return s_bank_enabled; }

int SpeciesMod_BankSave(void) {
    FILE *fp = fopen(SPECIESMOD_BANK_PATH, "w");
    if (!fp) return 0;
    fprintf(fp, "enabled %d\n", s_bank_enabled);
    for (int i = 0; i < SPECIESMOD_ALIAS_MAX; i++)
        if (s_aliases[i].used) fprintf(fp, "alias %s %u\n", s_aliases[i].name, (unsigned)s_aliases[i].species);
    for (int i = 0; i < SPECIESMOD_NAME_MAX; i++)
        if (s_names[i].used) fprintf(fp, "name %u %s\n", (unsigned)s_names[i].species, s_names[i].name);
    for (int i = 0; i < SPECIESMOD_STATS_MAX; i++) {
        const base_stats_t *b;
        if (!s_stats[i].used) continue;
        b = &s_stats[i].bs;
        fprintf(fp, "stats %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
                (unsigned)s_stats[i].species,
                (unsigned)b->hp, (unsigned)b->atk, (unsigned)b->def, (unsigned)b->spd, (unsigned)b->spc,
                (unsigned)b->type1, (unsigned)b->type2, (unsigned)b->catch_rate, (unsigned)b->base_exp,
                (unsigned)b->start_moves[0], (unsigned)b->start_moves[1], (unsigned)b->start_moves[2]);
        fprintf(fp, "stats_tail %u %u %u\n",
                (unsigned)s_stats[i].species, (unsigned)b->start_moves[3], (unsigned)b->growth_rate);
    }
    for (int i = 0; i < SPECIESMOD_LEVEL_MOVES_MAX; i++)
        if (s_level_moves[i].used)
            fprintf(fp, "lm %u %u %u\n", (unsigned)s_level_moves[i].species, (unsigned)s_level_moves[i].level, (unsigned)s_level_moves[i].move);
    fclose(fp);
    return 1;
}

int SpeciesMod_BankLoad(void) {
    FILE *fp = fopen(SPECIESMOD_BANK_PATH, "r");
    char line[256];
    if (!fp) return 0;
    memset(s_aliases, 0, sizeof(s_aliases));
    memset(s_names, 0, sizeof(s_names));
    memset(s_stats, 0, sizeof(s_stats));
    memset(s_level_moves, 0, sizeof(s_level_moves));
    while (fgets(line, sizeof(line), fp)) {
        char key[24] = {0};
        if (sscanf(line, "%23s", key) != 1) continue;
        if (strcmp(key, "enabled") == 0) {
            int e = 0;
            if (sscanf(line, "enabled %d", &e) == 1) s_bank_enabled = e ? 1 : 0;
        } else if (strcmp(key, "alias") == 0) {
            char n[24] = {0};
            unsigned s = 0;
            if (sscanf(line, "alias %23s %u", n, &s) == 2) SpeciesMod_DefineAlias(n, (uint8_t)s);
        } else if (strcmp(key, "name") == 0) {
            unsigned s = 0;
            char n[24] = {0};
            if (sscanf(line, "name %u %23s", &s, n) == 2) SpeciesMod_SetName((uint8_t)s, n);
        } else if (strcmp(key, "stats") == 0) {
            unsigned sp = 0, hp = 0, atk = 0, def = 0, spd = 0, spc = 0, t1 = 0, t2 = 0, cr = 0, be = 0, m1 = 0, m2 = 0, m3 = 0;
            base_stats_t b = {0};
            if (sscanf(line, "stats %u %u %u %u %u %u %u %u %u %u %u %u %u",
                       &sp, &hp, &atk, &def, &spd, &spc, &t1, &t2, &cr, &be, &m1, &m2, &m3) == 13) {
                b.hp = (uint8_t)hp; b.atk = (uint8_t)atk; b.def = (uint8_t)def; b.spd = (uint8_t)spd; b.spc = (uint8_t)spc;
                b.type1 = (uint8_t)t1; b.type2 = (uint8_t)t2; b.catch_rate = (uint8_t)cr; b.base_exp = (uint8_t)be;
                b.start_moves[0] = (uint8_t)m1; b.start_moves[1] = (uint8_t)m2; b.start_moves[2] = (uint8_t)m3;
                SpeciesMod_SetBaseStats((uint8_t)sp, &b);
            }
        } else if (strcmp(key, "stats_tail") == 0) {
            unsigned sp = 0, m4 = 0, gr = 0;
            if (sscanf(line, "stats_tail %u %u %u", &sp, &m4, &gr) == 3) {
                base_stats_t b = {0};
                if (SpeciesMod_GetBaseStats((uint8_t)sp, &b)) {
                    b.start_moves[3] = (uint8_t)m4;
                    b.growth_rate = (uint8_t)gr;
                    SpeciesMod_SetBaseStats((uint8_t)sp, &b);
                }
            }
        } else if (strcmp(key, "lm") == 0) {
            unsigned s = 0, lv = 0, mv = 0;
            if (sscanf(line, "lm %u %u %u", &s, &lv, &mv) == 3) SpeciesMod_AddLevelMove((uint8_t)s, (uint8_t)lv, (uint8_t)mv);
        }
    }
    fclose(fp);
    return 1;
}

int SpeciesMod_BankClear(void) {
    memset(s_aliases, 0, sizeof(s_aliases));
    memset(s_names, 0, sizeof(s_names));
    memset(s_stats, 0, sizeof(s_stats));
    memset(s_level_moves, 0, sizeof(s_level_moves));
    remove(SPECIESMOD_BANK_PATH);
    return 1;
}
