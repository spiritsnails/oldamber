
#include "debug_fields.h"
#include "../platform/hardware.h"
#include "../game/constants.h"
#include "../game/types.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

typedef enum { FT_U8, FT_I8, FT_U16, FT_I16, FT_U32 } fieldtype_t;

typedef struct {
    char        group[16];
    char        name[28];
    void       *ptr;
    fieldtype_t type;
} field_t;

#define FIELDS_MAX 320
static field_t s_fields[FIELDS_MAX];
static int     s_nfields = 0;
static int     s_inited  = 0;

static void reg(const char *group, const char *name, void *ptr, fieldtype_t t) {
    field_t *f;
    if (s_nfields >= FIELDS_MAX || !ptr) return;
    f = &s_fields[s_nfields++];
    snprintf(f->group, sizeof(f->group), "%s", group);
    snprintf(f->name, sizeof(f->name), "%s", name);
    f->ptr = ptr;
    f->type = t;
}

static long field_read(const field_t *f) {
    switch (f->type) {
        case FT_U8:  return *(uint8_t  *)f->ptr;
        case FT_I8:  return *(int8_t   *)f->ptr;
        case FT_U16: return *(uint16_t *)f->ptr;
        case FT_I16: return *(int16_t  *)f->ptr;
        case FT_U32: return (long)*(uint32_t *)f->ptr;
    }
    return 0;
}

static void field_write(const field_t *f, long v) {
    switch (f->type) {
        case FT_U8:  *(uint8_t  *)f->ptr = (uint8_t)v;  break;
        case FT_I8:  *(int8_t   *)f->ptr = (int8_t)v;   break;
        case FT_U16: *(uint16_t *)f->ptr = (uint16_t)v; break;
        case FT_I16: *(int16_t  *)f->ptr = (int16_t)v;  break;
        case FT_U32: *(uint32_t *)f->ptr = (uint32_t)v; break;
    }
}

static void reg_battlemon(const char *group, battle_mon_t *m) {
    reg(group, "species", &m->species, FT_U8);
    reg(group, "hp",      &m->hp,      FT_U16);
    reg(group, "max_hp",  &m->max_hp,  FT_U16);
    reg(group, "level",   &m->level,   FT_U8);
    reg(group, "status",  &m->status,  FT_U8);
    reg(group, "atk",     &m->atk,     FT_U16);
    reg(group, "def",     &m->def,     FT_U16);
    reg(group, "spd",     &m->spd,     FT_U16);
    reg(group, "spc",     &m->spc,     FT_U16);
    reg(group, "dvs",     &m->dvs,     FT_U16);
}

void DebugFields_Init(void) {
    if (s_inited) return;
    s_inited = 1;

    reg("player", "cur_map",     &wCurMap,            FT_U8);
    reg("player", "last_map",    &wLastMap,           FT_U8);
    reg("player", "x",           &wXCoord,            FT_I16);
    reg("player", "y",           &wYCoord,            FT_I16);
    reg("player", "direction",   &wPlayerDirection,   FT_U8);
    reg("player", "walkbikesurf",&wWalkBikeSurfState, FT_U8);
    reg("player", "badges",      &wObtainedBadges,    FT_U8);
    reg("player", "player_id",   &wPlayerID,          FT_U16);
    reg("player", "step_counter",&wStepCounter,       FT_U8);
    reg("player", "repel_steps", &wRepelRemainingSteps, FT_U8);
    reg("player", "party_count", &wPartyCount,        FT_U8);

    reg("rng", "rand_add",   &hRandomAdd,   FT_U8);
    reg("rng", "rand_sub",   &hRandomSub,   FT_U8);
    reg("rng", "frame_ctr",  &hFrameCounter,FT_U8);

    reg("battle", "in_battle",      &wIsInBattle,        FT_U8);
    reg("battle", "battle_type",    &wBattleType,        FT_U8);
    reg("battle", "enemy_species",  &wEnemyMonSpecies,   FT_U8);
    reg("battle", "cur_enemy_level",&wCurEnemyLevel,     FT_U8);
    reg("battle", "player_mon_num", &wPlayerMonNumber,   FT_U8);
    reg("battle", "crit",           &wCriticalHitOrOHKO, FT_U8);
    reg("battle", "damage",         &wDamage,            FT_U16);
    reg("battle", "move_missed",    &wMoveMissed,        FT_U8);
    reg_battlemon("battle_you",   &wBattleMon);
    reg_battlemon("battle_enemy", &wEnemyMon);
    for (int i = 0; i < NUM_STAT_MODS; i++) {
        char nm[28];
        snprintf(nm, sizeof(nm), "statmod%d", i);
        reg("battle_you",   nm, &wPlayerMonStatMods[i], FT_U8);
        reg("battle_enemy", nm, &wEnemyMonStatMods[i],  FT_U8);
    }

    for (int i = 0; i < PARTY_LENGTH; i++) {
        char g[16];
        party_mon_t *m = &wPartyMons[i];
        snprintf(g, sizeof(g), "party%d", i + 1);
        reg(g, "species", &m->base.species, FT_U8);
        reg(g, "hp",      &m->base.hp,      FT_U16);
        reg(g, "max_hp",  &m->max_hp,       FT_U16);
        reg(g, "level",   &m->level,        FT_U8);
        reg(g, "status",  &m->base.status,  FT_U8);
        reg(g, "dvs",     &m->base.dvs,     FT_U16);
        reg(g, "atk",     &m->atk,          FT_U16);
        reg(g, "def",     &m->def,          FT_U16);
        reg(g, "spd",     &m->spd,          FT_U16);
        reg(g, "spc",     &m->spc,          FT_U16);
        for (int j = 0; j < 4; j++) {
            char nm[28];
            snprintf(nm, sizeof(nm), "move%d", j + 1);
            reg(g, nm, &m->base.moves[j], FT_U8);
            snprintf(nm, sizeof(nm), "pp%d", j + 1);
            reg(g, nm, &m->base.pp[j], FT_U8);
        }
    }
}

int DebugFields_Set(const char *name, long value) {
    if (!s_inited) DebugFields_Init();
    for (int i = 0; i < s_nfields; i++) {
        char full[48];
        snprintf(full, sizeof(full), "%s.%s", s_fields[i].group, s_fields[i].name);
        if (strcasecmp(full, name) == 0) {
            field_write(&s_fields[i], value);
            return 1;
        }
    }
    return 0;
}

void DebugFields_Dump(void) {
    FILE *fp;
    if (!s_inited) DebugFields_Init();
    fp = fopen("bugs/fields.json.tmp", "w");
    if (!fp) return;
    fprintf(fp, "[");
    for (int i = 0; i < s_nfields; i++) {
        fprintf(fp, "%s{\"g\":\"%s\",\"n\":\"%s\",\"v\":%ld}",
                i ? "," : "", s_fields[i].group, s_fields[i].name,
                field_read(&s_fields[i]));
    }
    fprintf(fp, "]\n");
    fclose(fp);

    remove("bugs/fields.json");
    rename("bugs/fields.json.tmp", "bugs/fields.json");
}
