#include "type_mod.h"

#include "../data/base_stats.h"
#include "constants.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "gen2_species.h"

#define TYPEMOD_ALIAS_MAX 32
#define TYPEMOD_EFFECT_MAX 256
#define TYPEMOD_SPECIES_MAX 256
#define TYPEMOD_BANK_PATH "bugs/type_bank.txt"

typedef struct {
    int used;
    char name[24];
    uint8_t id;
} type_alias_t;

typedef struct {
    int used;
    uint8_t atk, def, eff;
} type_eff_t;

typedef struct {
    int used;
    uint8_t species, type1, type2;
} type_species_t;

static type_alias_t s_aliases[TYPEMOD_ALIAS_MAX];
static type_eff_t s_effects[TYPEMOD_EFFECT_MAX];
static type_species_t s_species[TYPEMOD_SPECIES_MAX];
static int s_bank_enabled = 0;

static void typemod_norm(const char *in, char *out, size_t out_sz) {
    size_t n = 0;
    if (!in || !out || out_sz == 0) return;
    for (size_t i = 0; in[i] && n + 1 < out_sz; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isalnum(c)) out[n++] = (char)toupper(c);
        else if (c == '_' || c == '-') out[n++] = '_';
    }
    out[n] = '\0';
}

static int typemod_builtin(const char *tok, uint8_t *out_type) {
    char n[24];
    typemod_norm(tok, n, sizeof(n));
    if (strcmp(n, "NORMAL") == 0) { *out_type = TYPE_NORMAL; return 1; }
    if (strcmp(n, "FIGHTING") == 0) { *out_type = TYPE_FIGHTING; return 1; }
    if (strcmp(n, "FLYING") == 0) { *out_type = TYPE_FLYING; return 1; }
    if (strcmp(n, "POISON") == 0) { *out_type = TYPE_POISON; return 1; }
    if (strcmp(n, "GROUND") == 0) { *out_type = TYPE_GROUND; return 1; }
    if (strcmp(n, "ROCK") == 0) { *out_type = TYPE_ROCK; return 1; }
    if (strcmp(n, "BIRD") == 0) { *out_type = TYPE_BIRD; return 1; }
    if (strcmp(n, "BUG") == 0) { *out_type = TYPE_BUG; return 1; }
    if (strcmp(n, "GHOST") == 0) { *out_type = TYPE_GHOST; return 1; }
    if (strcmp(n, "FIRE") == 0) { *out_type = TYPE_FIRE; return 1; }
    if (strcmp(n, "WATER") == 0) { *out_type = TYPE_WATER; return 1; }
    if (strcmp(n, "GRASS") == 0) { *out_type = TYPE_GRASS; return 1; }
    if (strcmp(n, "ELECTRIC") == 0) { *out_type = TYPE_ELECTRIC; return 1; }
    if (strcmp(n, "PSYCHIC") == 0) { *out_type = TYPE_PSYCHIC; return 1; }
    if (strcmp(n, "ICE") == 0) { *out_type = TYPE_ICE; return 1; }
    if (strcmp(n, "DRAGON") == 0) { *out_type = TYPE_DRAGON; return 1; }
    return 0;
}

int TypeMod_ResolveTypeToken(const char *tok, uint8_t *out_type) {
    char *end = NULL;
    long v;
    char n[24];
    if (!tok || !*tok || !out_type) return 0;
    v = strtol(tok, &end, 0);
    if (end != tok && *end == '\0' && v >= 0 && v <= 255) {
        *out_type = (uint8_t)v;
        return 1;
    }
    if (typemod_builtin(tok, out_type)) return 1;
    typemod_norm(tok, n, sizeof(n));
    for (int i = 0; i < TYPEMOD_ALIAS_MAX; i++) {
        if (s_aliases[i].used && strcmp(s_aliases[i].name, n) == 0) {
            *out_type = s_aliases[i].id;
            return 1;
        }
    }
    return 0;
}

void TypeMod_SetTypeAlias(const char *name, uint8_t type_id) {
    char n[24];
    int slot = -1;
    typemod_norm(name, n, sizeof(n));
    if (!n[0]) return;
    for (int i = 0; i < TYPEMOD_ALIAS_MAX; i++) {
        if (s_aliases[i].used && strcmp(s_aliases[i].name, n) == 0) { slot = i; break; }
        if (!s_aliases[i].used && slot < 0) slot = i;
    }
    if (slot < 0) return;
    s_aliases[slot].used = 1;
    s_aliases[slot].id = type_id;
    snprintf(s_aliases[slot].name, sizeof(s_aliases[slot].name), "%s", n);
    if (s_bank_enabled) TypeMod_BankSave();
}

const char *TypeMod_GetTypeName(uint8_t type_id) {
    for (int i = 0; i < TYPEMOD_ALIAS_MAX; i++) {
        if (s_aliases[i].used && s_aliases[i].id == type_id) {
            return s_aliases[i].name;
        }
    }
    return NULL;
}

void TypeMod_SetEffect(uint8_t atk, uint8_t def, uint8_t eff) {
    int slot = -1;
    for (int i = 0; i < TYPEMOD_EFFECT_MAX; i++) {
        if (s_effects[i].used && s_effects[i].atk == atk && s_effects[i].def == def) { slot = i; break; }
        if (!s_effects[i].used && slot < 0) slot = i;
    }
    if (slot < 0) return;
    s_effects[slot].used = 1;
    s_effects[slot].atk = atk;
    s_effects[slot].def = def;
    s_effects[slot].eff = eff;
    if (s_bank_enabled) TypeMod_BankSave();
}

int TypeMod_GetEffectOverride(uint8_t atk, uint8_t def, uint8_t *out_eff) {
    if (!out_eff) return 0;
    for (int i = 0; i < TYPEMOD_EFFECT_MAX; i++) {
        if (s_effects[i].used && s_effects[i].atk == atk && s_effects[i].def == def) {
            *out_eff = s_effects[i].eff;
            return 1;
        }
    }
    return 0;
}

void TypeMod_SetSpeciesTypes(uint8_t species, uint8_t type1, uint8_t type2) {
    int slot = -1;
    for (int i = 0; i < TYPEMOD_SPECIES_MAX; i++) {
        if (s_species[i].used && s_species[i].species == species) { slot = i; break; }
        if (!s_species[i].used && slot < 0) slot = i;
    }
    if (slot < 0) return;
    s_species[slot].used = 1;
    s_species[slot].species = species;
    s_species[slot].type1 = type1;
    s_species[slot].type2 = type2;
    if (s_bank_enabled) TypeMod_BankSave();
}

void TypeMod_GetSpeciesTypes(uint8_t species, uint8_t *out_type1, uint8_t *out_type2) {
    uint8_t d;
    if (!out_type1 || !out_type2) return;
    for (int i = 0; i < TYPEMOD_SPECIES_MAX; i++) {
        if (s_species[i].used && s_species[i].species == species) {
            *out_type1 = s_species[i].type1;
            *out_type2 = s_species[i].type2;
            return;
        }
    }
    d = gSpeciesToDex[species];
    if (d >= 1 && d <= 151) {
        *out_type1 = gBaseStats[d].type1;
        *out_type2 = gBaseStats[d].type2;
    } else if (Gen2Species_GetTypes(species, out_type1, out_type2)) {

    } else {
        *out_type1 = TYPE_NORMAL;
        *out_type2 = TYPE_NORMAL;
    }
}

void TypeMod_BankSetEnabled(int enabled) { s_bank_enabled = enabled ? 1 : 0; }
int TypeMod_BankIsEnabled(void) { return s_bank_enabled; }

int TypeMod_BankSave(void) {
    FILE *fp = fopen(TYPEMOD_BANK_PATH, "w");
    if (!fp) return 0;
    fprintf(fp, "enabled %d\n", s_bank_enabled);
    for (int i = 0; i < TYPEMOD_ALIAS_MAX; i++)
        if (s_aliases[i].used) fprintf(fp, "alias %s %u\n", s_aliases[i].name, (unsigned)s_aliases[i].id);
    for (int i = 0; i < TYPEMOD_EFFECT_MAX; i++)
        if (s_effects[i].used) fprintf(fp, "eff %u %u %u\n", (unsigned)s_effects[i].atk, (unsigned)s_effects[i].def, (unsigned)s_effects[i].eff);
    for (int i = 0; i < TYPEMOD_SPECIES_MAX; i++)
        if (s_species[i].used) fprintf(fp, "species %u %u %u\n", (unsigned)s_species[i].species, (unsigned)s_species[i].type1, (unsigned)s_species[i].type2);
    fclose(fp);
    return 1;
}

int TypeMod_BankLoad(void) {
    FILE *fp = fopen(TYPEMOD_BANK_PATH, "r");
    char line[128];
    if (!fp) return 0;
    memset(s_aliases, 0, sizeof(s_aliases));
    memset(s_effects, 0, sizeof(s_effects));
    memset(s_species, 0, sizeof(s_species));
    while (fgets(line, sizeof(line), fp)) {
        char key[16] = {0};
        if (sscanf(line, "%15s", key) != 1) continue;
        if (strcmp(key, "enabled") == 0) {
            int e = 0; if (sscanf(line, "enabled %d", &e) == 1) s_bank_enabled = e ? 1 : 0;
        } else if (strcmp(key, "alias") == 0) {
            char n[24] = {0}; unsigned id = 0;
            if (sscanf(line, "alias %23s %u", n, &id) == 2) TypeMod_SetTypeAlias(n, (uint8_t)id);
        } else if (strcmp(key, "eff") == 0) {
            unsigned a = 0, d = 0, e = 10;
            if (sscanf(line, "eff %u %u %u", &a, &d, &e) == 3) TypeMod_SetEffect((uint8_t)a, (uint8_t)d, (uint8_t)e);
        } else if (strcmp(key, "species") == 0) {
            unsigned s = 0, t1 = 0, t2 = 0;
            if (sscanf(line, "species %u %u %u", &s, &t1, &t2) == 3) TypeMod_SetSpeciesTypes((uint8_t)s, (uint8_t)t1, (uint8_t)t2);
        }
    }
    fclose(fp);
    return 1;
}

int TypeMod_BankClear(void) {
    memset(s_aliases, 0, sizeof(s_aliases));
    memset(s_effects, 0, sizeof(s_effects));
    memset(s_species, 0, sizeof(s_species));
    remove(TYPEMOD_BANK_PATH);
    return 1;
}
