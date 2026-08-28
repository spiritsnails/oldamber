
#include "gen2_species.h"
#include "species_mod.h"
#include "constants.h"
#include "../data/base_stats.h"
#include "gen2_pokedex.h"
#include "gen2_evos_moves.h"
#include <stdio.h>
#include <string.h>

#define GEN2_FIRST_DEX 152
#define GEN2_LAST_DEX  251
#define GEN2_COUNT     (GEN2_LAST_DEX - GEN2_FIRST_DEX + 1)

static uint8_t s_dex_to_internal[GEN2_COUNT];
static uint8_t s_internal_to_dex[256];
static int s_ready = 0;

void Gen2Species_Init(void) {
    int next = 0;
    if (s_ready) return;
    memset(s_dex_to_internal, 0, sizeof(s_dex_to_internal));
    memset(s_internal_to_dex, 0, sizeof(s_internal_to_dex));

    for (int id = 1; id < 256 && next < GEN2_COUNT; id++) {
        if (gSpeciesToDex[id] != 0) continue;
        s_dex_to_internal[next] = (uint8_t)id;
        s_internal_to_dex[id] = (uint8_t)(GEN2_FIRST_DEX + next);
        next++;
    }
    s_ready = 1;

    if (next < GEN2_COUNT) {

        printf("[gen2] only %d of %d Gen 2 species could be assigned an internal "
               "id -- dex %d and up are unavailable\n",
               next, GEN2_COUNT, GEN2_FIRST_DEX + next);
    }
}

uint8_t Gen2Species_DexToInternal(uint8_t dex) {
    if (dex < GEN2_FIRST_DEX || dex > GEN2_LAST_DEX) return 0;
    Gen2Species_Init();
    return s_dex_to_internal[dex - GEN2_FIRST_DEX];
}

uint8_t Gen2Species_InternalToDex(uint8_t species) {
    Gen2Species_Init();
    return s_internal_to_dex[species];
}

uint8_t Gen2Species_AnyDexToInternal(uint8_t dex) {
    if (dex >= 1 && dex <= 151) return gDexToSpecies[dex];
    return Gen2Species_DexToInternal(dex);
}

const char *Gen2Species_GetName(uint8_t species) {
    uint8_t dex = Gen2Species_InternalToDex(species);
    if (dex == 0) return NULL;
    return gGen2BaseStats[dex - 1].name;
}

int Gen2Species_GetTypes(uint8_t species, uint8_t *out_t1, uint8_t *out_t2) {
    uint8_t dex = Gen2Species_InternalToDex(species);
    if (dex == 0) return 0;

    if (out_t1) *out_t1 = gGen2BaseStats[dex - 1].type1;
    if (out_t2) *out_t2 = gGen2BaseStats[dex - 1].type2;
    return 1;
}

int Gen2Species_GetBaseStats(uint8_t species, base_stats_t *out_bs) {
    uint8_t dex = Gen2Species_InternalToDex(species);
    const gen2_base_stats_t *g;
    if (dex == 0 || !out_bs) return 0;
    g = &gGen2BaseStats[dex - 1];
    memset(out_bs, 0, sizeof(*out_bs));
    out_bs->dex_id = dex;
    out_bs->hp = g->hp;
    out_bs->atk = g->atk;
    out_bs->def = g->def;
    out_bs->spd = g->spd;

    out_bs->spc = g->sat;
    out_bs->type1 = g->type1;
    out_bs->type2 = g->type2;
    out_bs->catch_rate = g->catch_rate;
    out_bs->base_exp = g->base_exp;

    out_bs->sprite_dim = g->pic_dimensions;
    out_bs->growth_rate = g->growth_rate;
    memcpy(out_bs->tmhm, g->tmhm,
           sizeof(out_bs->tmhm) < sizeof(g->tmhm) ? sizeof(out_bs->tmhm)
                                                  : sizeof(g->tmhm));

    Gen2EvosMoves_MovesAtLevel(dex, 1, out_bs->start_moves);

    return 1;
}

uint8_t Species_Dex(uint8_t species) {
    uint8_t dex = gSpeciesToDex[species];
    if (dex) return dex;
    return Gen2Species_InternalToDex(species);
}

int Species_GetBaseStats(uint8_t species, base_stats_t *out_bs) {
    uint8_t dex;
    if (!out_bs) return 0;

    if (SpeciesMod_GetBaseStats(species, out_bs)) return 1;
    if (Gen2Species_GetBaseStats(species, out_bs)) return 1;
    dex = gSpeciesToDex[species];
    if (dex >= 1 && dex <= NUM_POKEMON) {
        *out_bs = gBaseStats[dex];
        return 1;
    }
    return 0;
}
