
#include "pokemon.h"
#include "type_mod.h"
#include "species_mod.h"
#include "gen2_species.h"
#include "gen2_pokedex.h"
#include "constants.h"
#include "../data/base_stats.h"
#include "../data/moves_data.h"
#include "../data/evos_moves_data.h"
#include "../data/pokemon_names_gen.h"
#include "../platform/hardware.h"

#include <string.h>
#include <stdlib.h>

extern uint16_t wPlayerID;

static uint8_t encode_name_char(char c) {

    if ((unsigned char)c >= 0x80) return (uint8_t)(unsigned char)c;
    if (c >= 'A' && c <= 'Z') return (uint8_t)(0x80 + (c - 'A'));
    if (c >= 'a' && c <= 'z') return (uint8_t)(0xA0 + (c - 'a'));
    if (c >= '0' && c <= '9') return (uint8_t)(0xF6 + (c - '0'));
    if (c == ' ')  return 0x7F;
    if (c == '.')  return 0xE8;
    if (c == '-')  return 0xE3;
    if (c == '\'') return 0xE0;
    return 0x7F;
}

void Pokemon_EncodeNameString(const char *src, uint8_t *dst) {
    int i;
    for (i = 0; i < NAME_LENGTH - 1 && src[i]; i++)
        dst[i] = encode_name_char(src[i]);
    dst[i] = 0x50;
    for (i = i + 1; i < NAME_LENGTH; i++)
        dst[i] = 0x50;
}

static uint32_t exp_bytes_to_u32(const uint8_t exp[3]) {
    return ((uint32_t)exp[0] << 16) | ((uint32_t)exp[1] << 8) | (uint32_t)exp[2];
}

static uint8_t infer_level_from_exp(uint8_t species, const uint8_t exp[3]) {
    uint8_t dex = gSpeciesToDex[species];
    base_stats_t bs = {0};
    uint8_t growth_rate;
    uint32_t cur_exp;
    uint8_t best = 1;

    if (SpeciesMod_GetBaseStats(species, &bs)) growth_rate = bs.growth_rate;
    else if (dex >= 1 && dex <= NUM_POKEMON) growth_rate = gBaseStats[dex].growth_rate;
    else return 1;
    cur_exp = exp_bytes_to_u32(exp);

    for (uint8_t lv = 1; lv <= 100; lv++) {
        uint32_t need = CalcExpForLevel(growth_rate, lv);
        if (need > cur_exp) break;
        best = lv;
    }
    return best;
}

static uint8_t hp_dv_from_dvs(uint16_t dvs) {
    uint8_t atk = (uint8_t)((dvs >> 12) & 0xF);
    uint8_t def = (uint8_t)((dvs >> 8) & 0xF);
    uint8_t spd = (uint8_t)((dvs >> 4) & 0xF);
    uint8_t spc = (uint8_t)(dvs & 0xF);
    return (uint8_t)(((atk & 1) << 3) | ((def & 1) << 2) | ((spd & 1) << 1) | (spc & 1));
}

static void fill_party_from_box(party_mon_t *dst, const box_mon_t *src) {
    uint8_t dex;
    base_stats_t bs = {0};
    const base_stats_t *bs_ptr;
    uint8_t level;
    uint8_t atk_dv, def_dv, spd_dv, spc_dv, hp_dv;

    memset(dst, 0, sizeof(*dst));
    memcpy(&dst->base, src, sizeof(*src));

    dex = gSpeciesToDex[src->species];
    if (!SpeciesMod_GetBaseStats(src->species, &bs)) {
        if (dex == 0 || dex > NUM_POKEMON) return;
        bs = gBaseStats[dex];
    }
    bs_ptr = &bs;
    level = src->box_level;
    if (level == 0) {

        level = infer_level_from_exp(src->species, src->exp);
        dst->base.box_level = level;
    }
    atk_dv = (uint8_t)((src->dvs >> 12) & 0xF);
    def_dv = (uint8_t)((src->dvs >> 8) & 0xF);
    spd_dv = (uint8_t)((src->dvs >> 4) & 0xF);
    spc_dv = (uint8_t)(src->dvs & 0xF);
    hp_dv = hp_dv_from_dvs(src->dvs);

    dst->level  = level;
    dst->max_hp = CalcStat(bs_ptr->hp,  hp_dv,  src->stat_exp_hp,  level, 1);
    dst->atk    = CalcStat(bs_ptr->atk, atk_dv, src->stat_exp_atk, level, 0);
    dst->def    = CalcStat(bs_ptr->def, def_dv, src->stat_exp_def, level, 0);
    dst->spd    = CalcStat(bs_ptr->spd, spd_dv, src->stat_exp_spd, level, 0);
    dst->spc    = CalcStat(bs_ptr->spc, spc_dv, src->stat_exp_spc, level, 0);

    dst->base.hp     = dst->max_hp;
    dst->base.status = 0;
}

static void shift_party_left(int start_slot) {
    for (int i = start_slot; i < PARTY_LENGTH - 1; i++) {
        wPartyMons[i] = wPartyMons[i + 1];
        memcpy(wPartyMonOT[i], wPartyMonOT[i + 1], NAME_LENGTH);
        memcpy(wPartyMonNicks[i], wPartyMonNicks[i + 1], NAME_LENGTH);
    }
    memset(&wPartyMons[PARTY_LENGTH - 1], 0, sizeof(wPartyMons[PARTY_LENGTH - 1]));
    memset(wPartyMonOT[PARTY_LENGTH - 1], 0x50, NAME_LENGTH);
    memset(wPartyMonNicks[PARTY_LENGTH - 1], 0x50, NAME_LENGTH);
}

static void shift_box_left(uint8_t box, int start_slot) {
    for (int i = start_slot; i < BOX_CAPACITY - 1; i++) {
        wBoxMons[box][i] = wBoxMons[box][i + 1];
        memcpy(wBoxMonOT[box][i], wBoxMonOT[box][i + 1], NAME_LENGTH);
        memcpy(wBoxMonNicks[box][i], wBoxMonNicks[box][i + 1], NAME_LENGTH);
        wBoxSpecies[box][i] = wBoxSpecies[box][i + 1];
    }
    memset(&wBoxMons[box][BOX_CAPACITY - 1], 0, sizeof(wBoxMons[box][BOX_CAPACITY - 1]));
    memset(wBoxMonOT[box][BOX_CAPACITY - 1], 0x50, NAME_LENGTH);
    memset(wBoxMonNicks[box][BOX_CAPACITY - 1], 0x50, NAME_LENGTH);
    wBoxSpecies[box][BOX_CAPACITY - 1] = 0xFF;
    wBoxSpecies[box][BOX_CAPACITY] = 0xFF;
}

uint32_t CalcExpForLevel(uint8_t growth_rate, uint8_t level) {
    if (level < 2) return 0;
    uint32_t n  = level;
    uint32_t n2 = n * n;
    uint32_t n3 = n2 * n;
    switch (growth_rate) {
        case GROWTH_MEDIUM_FAST:   return n3;
        case GROWTH_SLIGHTLY_FAST: return (3 * n3 / 4) + 10 * n2 - 30;
        case GROWTH_SLIGHTLY_SLOW: return (3 * n3 / 4) + 20 * n2 - 70;
        case GROWTH_MEDIUM_SLOW:   return (6 * n3 / 5) - 15 * n2 + 100 * n - 140;
        case GROWTH_FAST:          return 4 * n3 / 5;
        case GROWTH_SLOW:          return 5 * n3 / 4;
        default:                   return n3;
    }
}

void Pokemon_InitMon(party_mon_t *mon, uint8_t species, uint8_t level) {
    base_stats_t bs = {0};
    const base_stats_t *bs_ptr;

    if (!Species_GetBaseStats(species, &bs)) return;
    bs_ptr = &bs;

    memset(mon, 0, sizeof(*mon));

    mon->base.species    = species;
    mon->base.box_level  = level;
    mon->base.ot_id      = wPlayerID;
    TypeMod_GetSpeciesTypes(species, &mon->base.type1, &mon->base.type2);
    mon->base.catch_rate = bs_ptr->catch_rate;

    for (int i = 0; i < NUM_MOVES; i++) {
        uint8_t move_id = bs_ptr->start_moves[i];
        mon->base.moves[i] = move_id;
        if (move_id && move_id < NUM_MOVE_DEFS)
            mon->base.pp[i] = gMoves[move_id].pp;
    }

    uint32_t exp = CalcExpForLevel(bs_ptr->growth_rate, level);
    u32_to_exp(exp, mon->base.exp);

    mon->base.dvs = 0;

    mon->level  = level;
    mon->max_hp = CalcStat(bs_ptr->hp,  0, 0, level, 1);
    mon->atk    = CalcStat(bs_ptr->atk, 0, 0, level, 0);
    mon->def    = CalcStat(bs_ptr->def, 0, 0, level, 0);
    mon->spd    = CalcStat(bs_ptr->spd, 0, 0, level, 0);
    mon->spc    = CalcStat(bs_ptr->spc, 0, 0, level, 0);

    mon->base.hp = mon->max_hp;
}

const char *Pokemon_GetName(uint8_t dex) {

    if (dex >= NUM_POKEMON_NAMES && dex <= GEN2_NUM_SPECIES)
        return gGen2BaseStats[dex - 1].name;
    if (dex == 0 || dex >= NUM_POKEMON_NAMES) return "";
    return kPokemonNames[dex];
}

const char *Pokemon_GetNameBySpecies(uint8_t species) {
    const char *mod = SpeciesMod_GetName(species);
    uint8_t dex;
    if (mod && *mod) return mod;

    dex = Species_Dex(species);
    return Pokemon_GetName(dex);
}

void Pokemon_WriteMovesForLevel(uint8_t *moves, uint8_t *pp,
                                uint8_t species_id, uint8_t level) {
    uint8_t lm[32] = {0};
    uint8_t lm_count = 0;
    if (SpeciesMod_GetLevelMoves(species_id, level, lm, &lm_count)) {
        for (int li = 0; li < (int)lm_count; li++) {
            uint8_t move_id = lm[li];
            int already = 0;
            for (int i = 0; i < NUM_MOVES; i++) if (moves[i] == move_id) { already = 1; break; }
            if (already) continue;
            int slot = -1;
            for (int i = 0; i < NUM_MOVES; i++) if (moves[i] == 0) { slot = i; break; }
            if (slot >= 0) {
                moves[slot] = move_id;
                pp[slot] = (move_id < NUM_MOVE_DEFS) ? gMoves[move_id].pp : 0;
            } else {
                moves[0] = moves[1]; pp[0] = pp[1];
                moves[1] = moves[2]; pp[1] = pp[2];
                moves[2] = moves[3]; pp[2] = pp[3];
                moves[3] = move_id;
                pp[3] = (move_id < NUM_MOVE_DEFS) ? gMoves[move_id].pp : 0;
            }
        }
        return;
    }
    if (species_id == 0 || species_id >= EVOS_MOVES_TABLE_SIZE) return;
    const uint8_t *data = gEvosMoves[species_id];
    if (!data) return;

    while (*data != 0) data++;
    data++;

    while (*data != 0) {
        uint8_t learn_level = *data++;
        uint8_t move_id     = *data++;
        if (learn_level > level) break;

        int already = 0;
        for (int i = 0; i < NUM_MOVES; i++) {
            if (moves[i] == move_id) { already = 1; break; }
        }
        if (already) continue;

        int slot = -1;
        for (int i = 0; i < NUM_MOVES; i++) {
            if (moves[i] == 0) { slot = i; break; }
        }

        if (slot >= 0) {
            moves[slot] = move_id;
            pp[slot] = (move_id < NUM_MOVE_DEFS) ? gMoves[move_id].pp : 0;
        } else {

            moves[0] = moves[1]; pp[0] = pp[1];
            moves[1] = moves[2]; pp[1] = pp[2];
            moves[2] = moves[3]; pp[2] = pp[3];
            moves[3] = move_id;
            pp[3] = (move_id < NUM_MOVE_DEFS) ? gMoves[move_id].pp : 0;
        }
    }
}

void Pokemon_AddToParty(uint8_t species, uint8_t level) {
    if (wPartyCount >= PARTY_LENGTH) return;
    base_stats_t bs = {0};
    const base_stats_t *bs_ptr;

    if (!Species_GetBaseStats(species, &bs)) return;
    bs_ptr = &bs;

    party_mon_t *m = &wPartyMons[wPartyCount];
    memset(m, 0, sizeof(*m));

    uint8_t dv1 = BattleRandom();
    uint8_t dv2 = BattleRandom();
    uint8_t dv_atk = (uint8_t)(dv1 >> 4);
    uint8_t dv_def = (uint8_t)(dv1 & 0xF);
    uint8_t dv_spd = (uint8_t)(dv2 >> 4);
    uint8_t dv_spc = (uint8_t)(dv2 & 0xF);
    uint8_t dv_hp  = (uint8_t)(((dv_atk & 1) << 3) | ((dv_def & 1) << 2) |
                                ((dv_spd & 1) << 1) |  (dv_spc & 1));

    m->base.species    = species;
    m->base.box_level  = level;
    m->base.ot_id      = wPlayerID;
    TypeMod_GetSpeciesTypes(species, &m->base.type1, &m->base.type2);
    m->base.catch_rate = bs_ptr->catch_rate;
    m->base.dvs        = (uint16_t)((dv_atk << 12) | (dv_def << 8) | (dv_spd << 4) | dv_spc);

    uint32_t exp = CalcExpForLevel(bs_ptr->growth_rate, level);
    u32_to_exp(exp, m->base.exp);

    for (int i = 0; i < 4; i++) {
        uint8_t mid = bs_ptr->start_moves[i];
        m->base.moves[i] = mid;
        m->base.pp[i]    = (mid && mid < NUM_MOVE_DEFS) ? gMoves[mid].pp : 0;
    }
    Pokemon_WriteMovesForLevel(m->base.moves, m->base.pp, species, level);

    m->level  = level;
    m->max_hp = CalcStat(bs_ptr->hp,  dv_hp,  0, level, 1);
    m->atk    = CalcStat(bs_ptr->atk, dv_atk, 0, level, 0);
    m->def    = CalcStat(bs_ptr->def, dv_def, 0, level, 0);
    m->spd    = CalcStat(bs_ptr->spd, dv_spd, 0, level, 0);
    m->spc    = CalcStat(bs_ptr->spc, dv_spc, 0, level, 0);
    m->base.hp = m->max_hp;

    memcpy(wPartyMonOT[wPartyCount], wPlayerName, NAME_LENGTH);
    {
        const char *name = Pokemon_GetNameBySpecies(species);
        Pokemon_EncodeNameString(name ? name : "", wPartyMonNicks[wPartyCount]);
    }
    wPartySpecies[wPartyCount] = species;
    wPartySpecies[wPartyCount + 1] = 0xFF;

    wPartyCount++;
}

int Pokemon_AddToBox(uint8_t species, uint8_t level) {
    uint8_t box, slot, dex;
    base_stats_t bs = {0};
    const base_stats_t *bs_ptr;
    const char *name;
    box_mon_t *dst;
    uint32_t exp;
    uint8_t dv1, dv2, dv_atk, dv_def, dv_spd, dv_spc, dv_hp;
    uint8_t type1 = 0, type2 = 0;

    box = (uint8_t)(wCurrentBoxNum % NUM_BOXES);
    if (box >= NUM_BOXES) return 0;
    if (wBoxCount[box] >= BOX_CAPACITY) return 0;

    dex = gSpeciesToDex[species];
    if (!SpeciesMod_GetBaseStats(species, &bs)) {
        if (dex == 0 || dex > NUM_POKEMON) return 0;
        bs = gBaseStats[dex];
    }
    bs_ptr = &bs;

    dv1 = BattleRandom();
    dv2 = BattleRandom();
    dv_atk = (uint8_t)(dv1 >> 4); dv_def = (uint8_t)(dv1 & 0xF);
    dv_spd = (uint8_t)(dv2 >> 4); dv_spc = (uint8_t)(dv2 & 0xF);
    dv_hp  = (uint8_t)(((dv_atk & 1) << 3) | ((dv_def & 1) << 2) |
                        ((dv_spd & 1) << 1) |  (dv_spc & 1));

    slot = wBoxCount[box];
    dst = &wBoxMons[box][slot];
    memset(dst, 0, sizeof(*dst));

    TypeMod_GetSpeciesTypes(species, &type1, &type2);
    dst->species    = species;
    dst->box_level  = level;
    dst->type1      = type1;
    dst->type2      = type2;
    dst->catch_rate = bs_ptr->catch_rate;
    dst->ot_id      = wPlayerID;
    dst->dvs        = (uint16_t)((dv_atk << 12) | (dv_def << 8) | (dv_spd << 4) | dv_spc);

    exp = CalcExpForLevel(bs_ptr->growth_rate, level);
    u32_to_exp(exp, dst->exp);

    for (int i = 0; i < 4; i++) {
        uint8_t mid = bs_ptr->start_moves[i];
        dst->moves[i] = mid;
        dst->pp[i]    = (mid && mid < NUM_MOVE_DEFS) ? gMoves[mid].pp : 0;
    }
    Pokemon_WriteMovesForLevel(dst->moves, dst->pp, species, level);
    dst->hp = CalcStat(bs_ptr->hp, dv_hp, 0, level, 1);

    memcpy(wBoxMonOT[box][slot], wPlayerName, NAME_LENGTH);
    name = Pokemon_GetNameBySpecies(species);
    Pokemon_EncodeNameString(name ? name : "", wBoxMonNicks[box][slot]);

    wBoxCount[box]++;
    wBoxSpecies[box][slot] = species;
    wBoxSpecies[box][slot + 1] = 0xFF;
    return 1;
}

int Pokemon_SendBattleMonToBox(const battle_mon_t *mon) {
    uint8_t box;
    uint8_t slot;
    uint8_t dex;
    base_stats_t bs = {0};
    const base_stats_t *bs_ptr;
    const char *name;
    box_mon_t *dst;
    uint32_t exp;

    if (!mon) return 0;

    box = (uint8_t)(wCurrentBoxNum % NUM_BOXES);
    if (box >= NUM_BOXES) return 0;
    if (wBoxCount[box] >= BOX_CAPACITY) return 0;

    dex = gSpeciesToDex[mon->species];
    if (!SpeciesMod_GetBaseStats(mon->species, &bs)) {
        if (dex == 0 || dex > NUM_POKEMON) return 0;
        bs = gBaseStats[dex];
    }
    bs_ptr = &bs;

    slot = wBoxCount[box];
    dst = &wBoxMons[box][slot];
    memset(dst, 0, sizeof(*dst));

    dst->species    = mon->species;
    dst->hp         = mon->hp;
    dst->box_level  = mon->level;
    dst->status     = mon->status;
    dst->type1      = mon->type1;
    dst->type2      = mon->type2;
    dst->catch_rate = mon->catch_rate;
    memcpy(dst->moves, mon->moves, sizeof(dst->moves));
    dst->ot_id      = wPlayerID;
    exp = CalcExpForLevel(bs_ptr->growth_rate, mon->level);
    u32_to_exp(exp, dst->exp);
    dst->dvs        = mon->dvs;
    memcpy(dst->pp, mon->pp, sizeof(dst->pp));

    memcpy(wBoxMonOT[box][slot], wPlayerName, NAME_LENGTH);
    name = Pokemon_GetNameBySpecies(mon->species);
    Pokemon_EncodeNameString(name ? name : "", wBoxMonNicks[box][slot]);

    wBoxCount[box]++;
    wBoxSpecies[box][slot] = mon->species;
    wBoxSpecies[box][slot + 1] = 0xFF;
    return 1;
}

int Pokemon_DepositPartyMonToBox(int party_slot) {
    uint8_t box;
    uint8_t slot;

    if (party_slot < 0 || party_slot >= wPartyCount) return 0;

    box = (uint8_t)(wCurrentBoxNum % NUM_BOXES);
    if (wBoxCount[box] >= BOX_CAPACITY) return 0;

    slot = wBoxCount[box];
    memcpy(&wBoxMons[box][slot], &wPartyMons[party_slot].base, sizeof(box_mon_t));

    wBoxMons[box][slot].box_level = wPartyMons[party_slot].level;
    memcpy(wBoxMonOT[box][slot], wPartyMonOT[party_slot], NAME_LENGTH);
    memcpy(wBoxMonNicks[box][slot], wPartyMonNicks[party_slot], NAME_LENGTH);
    wBoxCount[box]++;
    wBoxSpecies[box][slot] = wPartyMons[party_slot].base.species;
    wBoxSpecies[box][slot + 1] = 0xFF;

    shift_party_left(party_slot);
    if (wPartyCount > 0) wPartyCount--;
    return 1;
}

int Pokemon_WithdrawBoxMonToParty(int box_slot) {
    uint8_t box;
    int party_slot;

    if (wPartyCount >= PARTY_LENGTH) return 0;

    box = (uint8_t)(wCurrentBoxNum % NUM_BOXES);
    if (box_slot < 0 || box_slot >= wBoxCount[box]) return 0;

    party_slot = wPartyCount;
    fill_party_from_box(&wPartyMons[party_slot], &wBoxMons[box][box_slot]);
    memcpy(wPartyMonOT[party_slot], wBoxMonOT[box][box_slot], NAME_LENGTH);
    memcpy(wPartyMonNicks[party_slot], wBoxMonNicks[box][box_slot], NAME_LENGTH);
    wPartyCount++;

    shift_box_left(box, box_slot);
    if (wBoxCount[box] > 0) wBoxCount[box]--;
    wBoxSpecies[box][wBoxCount[box]] = 0xFF;
    return 1;
}

int Pokemon_ReleaseBoxMon(int box_slot) {
    uint8_t box = (uint8_t)(wCurrentBoxNum % NUM_BOXES);

    if (box_slot < 0 || box_slot >= wBoxCount[box]) return 0;
    shift_box_left(box, box_slot);
    if (wBoxCount[box] > 0) wBoxCount[box]--;
    wBoxSpecies[box][wBoxCount[box]] = 0xFF;
    return 1;
}

void Pokemon_RemoveFromParty(int slot) {
    if (slot < 0 || slot >= (int)wPartyCount) return;

    for (int i = slot; i < (int)wPartyCount - 1; i++) {
        wPartyMons[i]     = wPartyMons[i + 1];
        wPartySpecies[i]  = wPartySpecies[i + 1];
        memcpy(wPartyMonOT[i],    wPartyMonOT[i + 1],    NAME_LENGTH);
        memcpy(wPartyMonNicks[i], wPartyMonNicks[i + 1], NAME_LENGTH);
    }

    wPartyCount--;
    memset(&wPartyMons[wPartyCount], 0, sizeof(wPartyMons[0]));
    memset(wPartyMonOT[wPartyCount],    0, NAME_LENGTH);
    memset(wPartyMonNicks[wPartyCount], 0, NAME_LENGTH);
    wPartySpecies[wPartyCount] = 0xFF;
}

void Pokemon_HealParty(void) {
    for (int i = 0; i < wPartyCount && i < PARTY_LENGTH; i++) {
        party_mon_t *mon = &wPartyMons[i];
        mon->base.status = 0;
        for (int m = 0; m < 4; m++) {
            uint8_t move_id = mon->base.moves[m];
            if (move_id == 0 || move_id >= NUM_MOVE_DEFS) continue;
            uint8_t pp_ups  = (mon->base.pp[m] >> 6) & 0x03;
            uint8_t base_pp = gMoves[move_id].pp;
            uint16_t new_pp = (uint16_t)base_pp + (uint16_t)(pp_ups * (base_pp / 5));
            if (new_pp > 63) new_pp = 63;
            mon->base.pp[m] = (uint8_t)((pp_ups << 6) | (uint8_t)new_pp);
        }
        mon->base.hp = mon->max_hp;
    }
}

uint8_t Pokemon_LevelFromExp(uint8_t species, const uint8_t exp[3]) {
    return infer_level_from_exp(species, exp);
}

uint8_t Pokemon_DaycareCheckedLevel(uint8_t species, uint8_t exp[3]) {
    uint8_t dex;
    base_stats_t bs = {0};
    uint8_t growth_rate = GROWTH_MEDIUM_FAST;
    uint8_t level = infer_level_from_exp(species, exp);
    if (level < MAX_LEVEL) return level;

    dex = gSpeciesToDex[species];
    if (SpeciesMod_GetBaseStats(species, &bs)) growth_rate = bs.growth_rate;
    else if (dex >= 1 && dex <= NUM_POKEMON) growth_rate = gBaseStats[dex].growth_rate;
    u32_to_exp(CalcExpForLevel(growth_rate, MAX_LEVEL), exp);
    return MAX_LEVEL;
}

int Pokemon_DepositPartyMonToDaycare(int party_slot) {
    if (party_slot < 0 || party_slot >= (int)wPartyCount) return 0;

    memcpy(&wDayCareMon, &wPartyMons[party_slot].base, sizeof(box_mon_t));
    wDayCareMon.box_level = wPartyMons[party_slot].level;
    memcpy(wDayCareMonOT,   wPartyMonOT[party_slot],    NAME_LENGTH);
    memcpy(wDayCareMonName, wPartyMonNicks[party_slot], NAME_LENGTH);

    Pokemon_RemoveFromParty(party_slot);
    return 1;
}

int Pokemon_WithdrawDaycareMonToParty(void) {
    int slot;
    party_mon_t *mon;

    if (wPartyCount >= PARTY_LENGTH) return 0;

    slot = wPartyCount;
    fill_party_from_box(&wPartyMons[slot], &wDayCareMon);
    memcpy(wPartyMonOT[slot],    wDayCareMonOT,   NAME_LENGTH);
    memcpy(wPartyMonNicks[slot], wDayCareMonName, NAME_LENGTH);
    wPartyCount++;

    mon = &wPartyMons[slot];
    memset(mon->base.moves, 0, sizeof(mon->base.moves));
    memset(mon->base.pp,    0, sizeof(mon->base.pp));
    Pokemon_WriteMovesForLevel(mon->base.moves, mon->base.pp, mon->base.species, mon->level);

    mon->base.hp = mon->max_hp;

    wDayCareInUse = 0;
    memset(&wDayCareMon, 0, sizeof(wDayCareMon));
    memset(wDayCareMonOT, 0x50, NAME_LENGTH);
    memset(wDayCareMonName, 0x50, NAME_LENGTH);
    return 1;
}
