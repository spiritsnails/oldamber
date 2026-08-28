
#include "battle_exp.h"
#include "../../platform/hardware.h"
#include "../../data/base_stats.h"
#include "../../data/moves_data.h"
#include "../../data/evos_moves_data.h"
#include "../pokemon.h"
#include "../type_mod.h"
#include "../species_mod.h"
#include "../gen2_species.h"
#include "../constants.h"
#include "../inventory.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int gDebugExpRate = 100;

#define EXP_QUEUE_MAX 32
#define EXP_TEXT_LEN  80

static battleexp_event_t s_exp_queue[EXP_QUEUE_MAX];
static int               s_exp_queue_count = 0;
static int               s_exp_queue_idx   = 0;

static battleexp_barcue_t s_bar_cue = {0, BEXP_ANIM_NONE, 0xFF, 0};
static int                s_bar_cue_open = 0;

void BattleExp_GetBarCue(battleexp_barcue_t *out) {
    if (out) *out = s_bar_cue;
}

static void bar_cue_publish(uint8_t kind, uint8_t slot, uint8_t to_full) {
    s_bar_cue.seq++;
    s_bar_cue.kind    = kind;
    s_bar_cue.slot    = slot;
    s_bar_cue.to_full = to_full;
}

static void exp_queue_clear(void) {
    s_exp_queue_count = 0;
    s_exp_queue_idx   = 0;
}

static int exp_queue_push(const char *text) {
    if (s_exp_queue_count < EXP_QUEUE_MAX) {
        battleexp_event_t *e = &s_exp_queue[s_exp_queue_count];
        memset(e, 0, sizeof(*e));
        e->type = BEXP_EVENT_TEXT;
        strncpy(e->text, text, EXP_TEXT_LEN - 1);
        e->text[EXP_TEXT_LEN - 1] = '\0';
        e->stats.valid = 0;
        return s_exp_queue_count++;
    }
    return -1;
}

static void exp_queue_push_levelup(const char *text, uint16_t atk, uint16_t def,
                                   uint16_t spd, uint16_t spc) {
    if (s_exp_queue_count < EXP_QUEUE_MAX) {
        battleexp_event_t *e = &s_exp_queue[s_exp_queue_count];
        memset(e, 0, sizeof(*e));
        e->type = BEXP_EVENT_TEXT;
        strncpy(e->text, text, EXP_TEXT_LEN - 1);
        e->text[EXP_TEXT_LEN - 1] = '\0';
        e->stats = (levelup_stats_t){ 1, atk, def, spd, spc };
        e->slot     = (uint8_t)wWhichPokemon;
        e->exp_anim = BEXP_ANIM_LEVEL;
        s_exp_queue_count++;
    }
}

static void exp_queue_push_learn_move(uint8_t slot, uint8_t move_id) {
    if (s_exp_queue_count < EXP_QUEUE_MAX) {
        battleexp_event_t *e = &s_exp_queue[s_exp_queue_count];
        memset(e, 0, sizeof(*e));
        e->type = BEXP_EVENT_LEARN_MOVE;
        e->slot = slot;
        e->move_id = move_id;
        s_exp_queue_count++;
    }
}

static int exp_nick_is_default_species(uint8_t slot, uint8_t species) {
    uint8_t enc_default[NAME_LENGTH];
    const uint8_t *nick;
    if (slot >= PARTY_LENGTH) return 0;
    Pokemon_EncodeNameString(Pokemon_GetNameBySpecies(species), enc_default);

    nick = wPartyMonNicks[slot];
    for (int i = 0; i < NAME_LENGTH; i++) {
        if (nick[i] != enc_default[i]) return 0;
        if (nick[i] == 0x50) return 1;
    }
    return 1;
}

int BattleExp_TakeNextEvent(battleexp_event_t *out) {
    if (!out) return 0;
    if (s_exp_queue_idx >= s_exp_queue_count) {

        if (s_bar_cue_open) {
            s_bar_cue_open = 0;
            bar_cue_publish(BEXP_ANIM_SETTLE, s_bar_cue.slot, 0);
        }
        return 0;
    }
    *out = s_exp_queue[s_exp_queue_idx++];
    if (out->exp_anim != BEXP_ANIM_NONE) {
        s_bar_cue_open = 1;
        bar_cue_publish(out->exp_anim, out->slot, out->exp_to_full);
    }
    return 1;
}

uint8_t Battle_CalcLevelFromExp(uint8_t growth_rate, uint32_t exp) {
    for (uint8_t d = 2; d <= MAX_LEVEL; d++) {
        if (CalcExpForLevel(growth_rate, d) > exp) {
            return (uint8_t)(d - 1);
        }
    }
    return MAX_LEVEL;
}

void Battle_LearnMoveFromLevelUp(uint8_t slot, uint8_t new_level) {
    uint8_t species_id = wPartyMons[slot].base.species;
    if (species_id == 0 || species_id >= EVOS_MOVES_TABLE_SIZE) return;

    const uint8_t *data = gEvosMoves[species_id];
    if (!data) return;

    while (*data != 0) data++;
    data++;

    while (*data != 0) {
        uint8_t learn_level = *data++;
        uint8_t move_id     = *data++;

        if (learn_level != new_level) continue;

        uint8_t *moves = wPartyMons[slot].base.moves;
        uint8_t *pp    = wPartyMons[slot].base.pp;
        int already = 0;
        for (int i = 0; i < NUM_MOVES; i++) {
            if (moves[i] == move_id) { already = 1; break; }
        }
        if (already) return;

        int learn_slot = -1;
        for (int i = 0; i < NUM_MOVES; i++) {
            if (moves[i] == 0) { learn_slot = i; break; }
        }
        if (learn_slot < 0) {

            exp_queue_push_learn_move(slot, move_id);
            return;
        }

        moves[learn_slot] = move_id;
        pp[learn_slot]    = (move_id < NUM_MOVE_DEFS) ? gMoves[move_id].pp : 0;

        if (slot == wPlayerMonNumber) {
            wBattleMon.moves[learn_slot] = move_id;
            wBattleMon.pp[learn_slot]    = pp[learn_slot];
        }

        {
            char _mv_buf[EXP_TEXT_LEN];
            const char *mn = (move_id < NUM_MOVE_DEFS && gMoveNames[move_id])
                             ? gMoveNames[move_id] : "a new move";
            snprintf(_mv_buf, sizeof(_mv_buf), "%s learned\n%s!",
                     Pokemon_GetNameBySpecies(species_id), mn);
            exp_queue_push(_mv_buf);
        }
        return;
    }
}

static uint8_t s_exp_all_halve;

static uint8_t s_modern_share_pass;

static uint8_t s_exp_queue_keep;
static int     s_modern_share_on;

void BattleExp_SetModernShare(int on) { s_modern_share_on = on ? 1 : 0; }
int  BattleExp_ModernShare(void)      { return s_modern_share_on; }

void Battle_GainExperience(void) {

    if (!s_exp_queue_keep) exp_queue_clear();

    if (wLinkState == LINK_STATE_BATTLING) return;

    int num_gaining = 0;
    for (int i = 0; i < PARTY_LENGTH; i++) {
        if (wPartyGainExpFlags & (1u << i)) num_gaining++;
    }
    if (num_gaining == 0) return;

    base_stats_t enemy_bs;
    if (!Species_GetBaseStats(wEnemyMon.species, &enemy_bs)) goto done;
    const base_stats_t *eb = &enemy_bs;

    uint8_t e_stats[5] = {eb->hp, eb->atk, eb->def, eb->spd, eb->spc};
    uint8_t base_exp   = eb->base_exp;

    if (s_exp_all_halve) {
        for (int i = 0; i < 5; i++) e_stats[i] >>= 1;
        base_exp >>= 1;
    }

    if (s_modern_share_pass) {
        for (int i = 0; i < 5; i++) e_stats[i] >>= 1;
        base_exp >>= 1;
    } else if (num_gaining > 1) {

        for (int i = 0; i < 5; i++) e_stats[i] /= (uint8_t)num_gaining;
        base_exp /= (uint8_t)num_gaining;
    }

    for (wWhichPokemon = 0; wWhichPokemon < wPartyCount; wWhichPokemon++) {

        if (wPartyMons[wWhichPokemon].base.hp == 0) continue;

        if (!(wPartyGainExpFlags & (1u << wWhichPokemon))) continue;

        party_mon_t *p = &wPartyMons[wWhichPokemon];

        uint16_t *sexp[5] = {
            &p->base.stat_exp_hp,  &p->base.stat_exp_atk,
            &p->base.stat_exp_def, &p->base.stat_exp_spd,
            &p->base.stat_exp_spc
        };
        for (int i = 0; i < 5; i++) {
            uint32_t v = (uint32_t)*sexp[i] + e_stats[i];
            *sexp[i]   = (v > 0xFFFF) ? 0xFFFF : (uint16_t)v;
        }

        uint32_t gained = (uint32_t)base_exp * wEnemyMon.level / 7;

        wGainBoostedExp = 0;
        if (p->base.ot_id != 0 && p->base.ot_id != wPlayerID) {
            gained += gained / 2;
            wGainBoostedExp = 1;
        }

        if (wIsInBattle != 1) {
            gained += gained / 2;
        }

        if (gDebugExpRate != 100)
            gained = gained * (uint32_t)gDebugExpRate / 100;

        wExpAmountGained = (uint16_t)(gained > 0xFFFF ? 0xFFFF : gained);

        uint32_t cur_exp = exp_to_u32(p->base.exp);
        cur_exp += gained;

        base_stats_t bs_live = {0};
        uint8_t gr;
        if (!SpeciesMod_GetBaseStats(p->base.species, &bs_live)) continue;
        gr  = bs_live.growth_rate;
        uint32_t max_exp = CalcExpForLevel(gr, MAX_LEVEL);
        if (cur_exp > max_exp) cur_exp = max_exp;
        u32_to_exp(cur_exp, p->base.exp);

        int gained_idx;
        {

            char _exp_buf[EXP_TEXT_LEN];
            const char *nm = Pokemon_GetNameBySpecies(p->base.species);
            if (wBoostExpByExpAll)
                snprintf(_exp_buf, sizeof(_exp_buf), "%s gained\nwith EXP.ALL,\n%u EXP. Points!",
                         nm, (unsigned)gained);
            else if (wGainBoostedExp)
                snprintf(_exp_buf, sizeof(_exp_buf), "%s gained\na boosted\n%u EXP. Points!",
                         nm, (unsigned)gained);
            else
                snprintf(_exp_buf, sizeof(_exp_buf), "%s gained\n%u EXP. Points!",
                         nm, (unsigned)gained);
            gained_idx = exp_queue_push(_exp_buf);
            if (gained_idx >= 0) {
                s_exp_queue[gained_idx].slot     = (uint8_t)wWhichPokemon;
                s_exp_queue[gained_idx].exp_anim = BEXP_ANIM_GAIN;
            }
        }

        uint8_t old_level = p->level;
        uint8_t new_level = Battle_CalcLevelFromExp(gr, cur_exp);
        if (new_level <= old_level) continue;

        if (gained_idx >= 0) s_exp_queue[gained_idx].exp_to_full = 1;

        p->level         = new_level;
        p->base.box_level = new_level;

        const base_stats_t *bs = &bs_live;
        uint8_t atk_dv  = (uint8_t)((p->base.dvs >> 12) & 0x0F);
        uint8_t def_dv  = (uint8_t)((p->base.dvs >>  8) & 0x0F);
        uint8_t spd_dv  = (uint8_t)((p->base.dvs >>  4) & 0x0F);
        uint8_t spc_dv  = (uint8_t)( p->base.dvs        & 0x0F);
        uint8_t hp_dv   = (uint8_t)(((atk_dv & 1) << 3) | ((def_dv & 1) << 2) |
                                     ((spd_dv & 1) << 1) |  (spc_dv & 1));

        uint16_t old_max_hp = p->max_hp;
        p->max_hp = CalcStat(bs->hp,  hp_dv,  p->base.stat_exp_hp,  new_level, 1);
        p->atk    = CalcStat(bs->atk, atk_dv, p->base.stat_exp_atk, new_level, 0);
        p->def    = CalcStat(bs->def, def_dv, p->base.stat_exp_def, new_level, 0);
        p->spd    = CalcStat(bs->spd, spd_dv, p->base.stat_exp_spd, new_level, 0);
        p->spc    = CalcStat(bs->spc, spc_dv, p->base.stat_exp_spc, new_level, 0);

        uint16_t cur_hp  = (wWhichPokemon == wPlayerMonNumber) ? wBattleMon.hp : p->base.hp;
        uint16_t hp_gain = (p->max_hp > old_max_hp) ? (p->max_hp - old_max_hp) : 0;
        uint32_t new_hp  = (uint32_t)cur_hp + hp_gain;
        p->base.hp = (uint16_t)(new_hp > p->max_hp ? p->max_hp : new_hp);

        if (wWhichPokemon == wPlayerMonNumber) {
            wBattleMon.hp     = p->base.hp;
            wBattleMon.level  = new_level;
            wBattleMon.max_hp = p->max_hp;
            wBattleMon.atk    = p->atk;
            wBattleMon.def    = p->def;
            wBattleMon.spd    = p->spd;
            wBattleMon.spc    = p->spc;
        }

        {
            char _lvl_buf[EXP_TEXT_LEN];
            snprintf(_lvl_buf, sizeof(_lvl_buf), "%s grew\nto level %d!",
                     Pokemon_GetNameBySpecies(p->base.species), new_level);
            exp_queue_push_levelup(_lvl_buf, p->atk, p->def, p->spd, p->spc);
        }

        Battle_LearnMoveFromLevelUp((uint8_t)wWhichPokemon, new_level);

        wCanEvolveFlags |= (uint8_t)(1u << wWhichPokemon);
    }

done:

    wPartyGainExpFlags = (uint8_t)(1u << wPlayerMonNumber);
    wPartyFoughtCurrentEnemyFlags = (uint8_t)(1u << wPlayerMonNumber);
}

static uint8_t evo_check_one_mon(uint8_t i) {
    party_mon_t *p = &wPartyMons[i];
    uint8_t old_species = p->base.species;
    if (old_species == 0 || old_species >= EVOS_MOVES_TABLE_SIZE) return 0;
    const uint8_t *data = gEvosMoves[old_species];
    if (!data) return 0;

    uint8_t new_species = 0;
    int skip_mon = 0;
    while (!skip_mon && *data != 0) {
        uint8_t evo_type = *data++;
        if (evo_type == EVOLVE_TRADE) {
            if (wLinkState != LINK_STATE_TRADING) { data += 2; continue; }
            uint8_t lr = *data++; uint8_t ns = *data++;
            if (p->level < lr) { skip_mon = 1; break; }
            new_species = ns; break;
        }
        if (wLinkState == LINK_STATE_TRADING) { skip_mon = 1; break; }
        if (evo_type == EVOLVE_ITEM) {
            uint8_t item_id = *data++;
            if (wCurPartySpecies != item_id) { data += 2; continue; }
            uint8_t lr = *data++; uint8_t ns = *data++;
            if (p->level < lr) continue;
            new_species = ns; break;
        }
        if (wForceEvolution) { skip_mon = 1; break; }
        if (evo_type == EVOLVE_LEVEL) {
            uint8_t lr = *data++; uint8_t ns = *data++;
            if (p->level < lr) continue;
            new_species = ns; break;
        }
        data += 2;
    }
    return new_species;
}

int Battle_CheckNextEvolution(uint8_t *slot_out, uint8_t *new_species_out) {
    for (uint8_t i = 0; i < wPartyCount; i++) {
        if (!(wCanEvolveFlags & (1u << i))) continue;
        uint8_t ns = evo_check_one_mon(i);
        if (ns) {
            *slot_out        = i;
            *new_species_out = ns;
            return 1;
        }
    }
    return 0;
}

void Battle_ApplyEvolution(uint8_t slot, uint8_t new_species) {
    wEvolutionOccurred = 1;
    wWhichPokemon      = slot;
    party_mon_t *p     = &wPartyMons[slot];
    uint8_t old_species = p->base.species;
    wEvoOldSpecies     = old_species;
    wEvoNewSpecies     = new_species;

    uint8_t new_dex = gSpeciesToDex[new_species];
    base_stats_t bs_live = {0};
    if (!SpeciesMod_GetBaseStats(new_species, &bs_live)) {
        wCanEvolveFlags &= (uint8_t)~(1u << slot);
        return;
    }
    const base_stats_t *bs = &bs_live;

    p->base.species      = new_species;
    wPartySpecies[slot]  = new_species;

    if (exp_nick_is_default_species(slot, old_species))
        Pokemon_EncodeNameString(Pokemon_GetNameBySpecies(new_species), wPartyMonNicks[slot]);

    uint8_t atk_dv = (uint8_t)((p->base.dvs >> 12) & 0x0F);
    uint8_t def_dv = (uint8_t)((p->base.dvs >>  8) & 0x0F);
    uint8_t spd_dv = (uint8_t)((p->base.dvs >>  4) & 0x0F);
    uint8_t spc_dv = (uint8_t)( p->base.dvs        & 0x0F);
    uint8_t hp_dv  = (uint8_t)(((atk_dv & 1) << 3) | ((def_dv & 1) << 2) |
                                ((spd_dv & 1) << 1) |  (spc_dv & 1));

    uint16_t old_max_hp = p->max_hp;
    p->max_hp = CalcStat(bs->hp,  hp_dv,  p->base.stat_exp_hp,  p->level, 1);
    p->atk    = CalcStat(bs->atk, atk_dv, p->base.stat_exp_atk, p->level, 0);
    p->def    = CalcStat(bs->def, def_dv, p->base.stat_exp_def, p->level, 0);
    p->spd    = CalcStat(bs->spd, spd_dv, p->base.stat_exp_spd, p->level, 0);
    p->spc    = CalcStat(bs->spc, spc_dv, p->base.stat_exp_spc, p->level, 0);

    uint16_t cur_hp  = (slot == wPlayerMonNumber) ? wBattleMon.hp : p->base.hp;
    uint16_t hp_gain = (p->max_hp > old_max_hp) ? (p->max_hp - old_max_hp) : 0;
    uint32_t new_hp  = (uint32_t)cur_hp + hp_gain;
    p->base.hp = (uint16_t)(new_hp > p->max_hp ? p->max_hp : new_hp);

    TypeMod_GetSpeciesTypes(new_species, &p->base.type1, &p->base.type2);

    if (slot == wPlayerMonNumber) {
        wBattleMon.species = new_species;
        wBattleMon.hp      = p->base.hp;
        wBattleMon.max_hp  = p->max_hp;
        wBattleMon.atk     = p->atk;
        wBattleMon.def     = p->def;
        wBattleMon.spd     = p->spd;
        wBattleMon.spc     = p->spc;
        wBattleMon.type1   = p->base.type1;
        wBattleMon.type2   = p->base.type2;
    }

    Battle_LearnMoveFromLevelUp(slot, p->level);

    if (new_dex >= 1 && new_dex <= NUM_POKEMON) {
        uint8_t bidx = (uint8_t)((new_dex - 1) % 8);
        uint8_t boff = (uint8_t)((new_dex - 1) / 8);
        if (boff < 19) {
            wPokedexOwned[boff] |= (uint8_t)(1u << bidx);
            wPokedexSeen[boff]  |= (uint8_t)(1u << bidx);
        }
    }

    printf("[battle]   %s evolved into %s!\n",
           Pokemon_GetNameBySpecies(old_species),
           Pokemon_GetNameBySpecies(new_species));

    wCanEvolveFlags &= (uint8_t)~(1u << slot);
}

void Battle_CancelEvolution(uint8_t slot) {
    wCanEvolveFlags &= (uint8_t)~(1u << slot);
}

void Battle_EvolutionAfterBattle(void) {
    wEvolutionOccurred = 0;
    uint8_t slot, new_species;
    while (Battle_CheckNextEvolution(&slot, &new_species))
        Battle_ApplyEvolution(slot, new_species);
}

void Battle_AddExpDirect(uint8_t slot, uint32_t amount) {
    if (slot >= wPartyCount) return;
    party_mon_t *p = &wPartyMons[slot];
    base_stats_t bs_live = {0};
    const base_stats_t *bs;
    if (!SpeciesMod_GetBaseStats(p->base.species, &bs_live)) return;
    bs = &bs_live;

    uint32_t cur_exp = exp_to_u32(p->base.exp);
    cur_exp += amount;
    uint32_t max_exp = CalcExpForLevel(bs->growth_rate, MAX_LEVEL);
    if (cur_exp > max_exp) cur_exp = max_exp;
    u32_to_exp(cur_exp, p->base.exp);

    uint8_t old_level = p->level;
    uint8_t new_level = Battle_CalcLevelFromExp(bs->growth_rate, cur_exp);
    if (new_level > MAX_LEVEL) new_level = MAX_LEVEL;

    if (new_level > old_level) {
        p->level          = new_level;
        p->base.box_level = new_level;

        uint8_t atk_dv = (uint8_t)((p->base.dvs >> 12) & 0x0F);
        uint8_t def_dv = (uint8_t)((p->base.dvs >>  8) & 0x0F);
        uint8_t spd_dv = (uint8_t)((p->base.dvs >>  4) & 0x0F);
        uint8_t spc_dv = (uint8_t)( p->base.dvs        & 0x0F);
        uint8_t hp_dv  = (uint8_t)(((atk_dv & 1) << 3) | ((def_dv & 1) << 2) |
                                    ((spd_dv & 1) << 1) |  (spc_dv & 1));

        uint16_t old_max_hp = p->max_hp;
        p->max_hp = CalcStat(bs->hp,  hp_dv,  p->base.stat_exp_hp,  new_level, 1);
        p->atk    = CalcStat(bs->atk, atk_dv, p->base.stat_exp_atk, new_level, 0);
        p->def    = CalcStat(bs->def, def_dv, p->base.stat_exp_def, new_level, 0);
        p->spd    = CalcStat(bs->spd, spd_dv, p->base.stat_exp_spd, new_level, 0);
        p->spc    = CalcStat(bs->spc, spc_dv, p->base.stat_exp_spc, new_level, 0);

        uint16_t hp_gain = (p->max_hp > old_max_hp) ? (p->max_hp - old_max_hp) : 0;
        uint32_t new_hp  = (uint32_t)p->base.hp + hp_gain;
        p->base.hp = (uint16_t)(new_hp > p->max_hp ? p->max_hp : new_hp);

        if (slot == wPlayerMonNumber && wIsInBattle) {
            wBattleMon.max_hp = p->max_hp;
            wBattleMon.hp     = p->base.hp;
            wBattleMon.atk    = p->atk;
            wBattleMon.def    = p->def;
            wBattleMon.spd    = p->spd;
            wBattleMon.spc    = p->spc;
        }

        for (uint8_t lv = (uint8_t)(old_level + 1); lv <= new_level; lv++)
            Battle_LearnMoveFromLevelUp(slot, lv);

        printf("[battle] %s grew to Lv%d!\n", Pokemon_GetNameBySpecies(p->base.species), new_level);
    }
    printf("[cli] addexp: slot %d +%u exp → %u total, Lv%d\n",
           slot + 1, (unsigned)amount, (unsigned)cur_exp, (int)p->level);
}

int Pokemon_ApplyRareCandy(uint8_t slot, uint8_t *new_level_out) {
    if (slot >= wPartyCount) return 0;
    party_mon_t *p = &wPartyMons[slot];
    if (p->level >= MAX_LEVEL) return 0;

    base_stats_t bs_live = {0};
    if (!SpeciesMod_GetBaseStats(p->base.species, &bs_live)) return 0;
    const base_stats_t *bs = &bs_live;

    uint8_t new_level = (uint8_t)(p->level + 1);
    p->level          = new_level;
    p->base.box_level = new_level;
    if (new_level_out) *new_level_out = new_level;

    uint32_t new_exp = CalcExpForLevel(bs->growth_rate, new_level);
    u32_to_exp(new_exp, p->base.exp);

    uint8_t atk_dv = (uint8_t)((p->base.dvs >> 12) & 0x0F);
    uint8_t def_dv = (uint8_t)((p->base.dvs >>  8) & 0x0F);
    uint8_t spd_dv = (uint8_t)((p->base.dvs >>  4) & 0x0F);
    uint8_t spc_dv = (uint8_t)( p->base.dvs        & 0x0F);
    uint8_t hp_dv  = (uint8_t)(((atk_dv & 1) << 3) | ((def_dv & 1) << 2) |
                                ((spd_dv & 1) << 1) |  (spc_dv & 1));

    uint16_t old_max_hp = p->max_hp;
    p->max_hp = CalcStat(bs->hp,  hp_dv,  p->base.stat_exp_hp,  new_level, 1);
    p->atk    = CalcStat(bs->atk, atk_dv, p->base.stat_exp_atk, new_level, 0);
    p->def    = CalcStat(bs->def, def_dv, p->base.stat_exp_def, new_level, 0);
    p->spd    = CalcStat(bs->spd, spd_dv, p->base.stat_exp_spd, new_level, 0);
    p->spc    = CalcStat(bs->spc, spc_dv, p->base.stat_exp_spc, new_level, 0);

    uint16_t hp_gain = (p->max_hp > old_max_hp) ? (uint16_t)(p->max_hp - old_max_hp) : 0;
    uint32_t nh      = (uint32_t)p->base.hp + hp_gain;
    p->base.hp = (uint16_t)(nh > p->max_hp ? p->max_hp : nh);

    Battle_LearnMoveFromLevelUp(slot, new_level);

    wForceEvolution = 0;
    if (evo_check_one_mon(slot)) {
        wCanEvolveFlags = (uint8_t)(1u << slot);
        return 2;
    }
    return 1;
}

const char *Pokemon_VitaminStatName(uint8_t item_id) {
    switch (item_id) {
        case ITEM_HP_UP:   return "HEALTH";
        case ITEM_PROTEIN: return "ATTACK";
        case ITEM_IRON:    return "DEFENSE";
        case ITEM_CARBOS:  return "SPEED";
        case ITEM_CALCIUM: return "SPECIAL";
        default:           return "";
    }
}

int Pokemon_ApplyVitamin(uint8_t slot, uint8_t item_id) {
    if (slot >= wPartyCount) return 0;
    party_mon_t *p = &wPartyMons[slot];

    uint16_t *se;
    int which;
    switch (item_id) {
        case ITEM_HP_UP:   se = &p->base.stat_exp_hp;  which = 0; break;
        case ITEM_PROTEIN: se = &p->base.stat_exp_atk; which = 1; break;
        case ITEM_IRON:    se = &p->base.stat_exp_def; which = 2; break;
        case ITEM_CARBOS:  se = &p->base.stat_exp_spd; which = 3; break;
        case ITEM_CALCIUM: se = &p->base.stat_exp_spc; which = 4; break;
        default: return 0;
    }

    if (((*se >> 8) & 0xFF) >= 100) return 0;

    base_stats_t bs_live = {0};
    if (!SpeciesMod_GetBaseStats(p->base.species, &bs_live)) return 0;
    const base_stats_t *bs = &bs_live;

    uint16_t before[5] = { p->max_hp, p->atk, p->def, p->spd, p->spc };
    *se = (uint16_t)(*se + 2560);

    uint8_t atk_dv = (uint8_t)((p->base.dvs >> 12) & 0x0F);
    uint8_t def_dv = (uint8_t)((p->base.dvs >>  8) & 0x0F);
    uint8_t spd_dv = (uint8_t)((p->base.dvs >>  4) & 0x0F);
    uint8_t spc_dv = (uint8_t)( p->base.dvs        & 0x0F);
    uint8_t hp_dv  = (uint8_t)(((atk_dv & 1) << 3) | ((def_dv & 1) << 2) |
                                ((spd_dv & 1) << 1) |  (spc_dv & 1));
    p->max_hp = CalcStat(bs->hp,  hp_dv,  p->base.stat_exp_hp,  p->level, 1);
    p->atk    = CalcStat(bs->atk, atk_dv, p->base.stat_exp_atk, p->level, 0);
    p->def    = CalcStat(bs->def, def_dv, p->base.stat_exp_def, p->level, 0);
    p->spd    = CalcStat(bs->spd, spd_dv, p->base.stat_exp_spd, p->level, 0);
    p->spc    = CalcStat(bs->spc, spc_dv, p->base.stat_exp_spc, p->level, 0);

    uint16_t after[5] = { p->max_hp, p->atk, p->def, p->spd, p->spc };
    printf("[item] vitamin on %s (slot %d): %s %u -> %u  "
           "[stat_exp hi=%u/100]  (HP %u/%u ATK %u DEF %u SPD %u SPC %u)\n",
           Pokemon_GetNameBySpecies(p->base.species), slot + 1,
           Pokemon_VitaminStatName(item_id), before[which], after[which],
           (unsigned)((*se >> 8) & 0xFF),
           p->max_hp, p->base.hp, p->atk, p->def, p->spd, p->spc);
    return 1;
}

static uint8_t pp_move_max(uint8_t move_id, uint8_t pp_ups) {
    if (move_id == 0 || move_id >= NUM_MOVE_DEFS) return 0;
    uint8_t base = gMoves[move_id].pp;
    uint8_t per  = (uint8_t)(base / 5); if (per > 7) per = 7;
    uint16_t maxp = (uint16_t)base + (uint16_t)pp_ups * per;
    return (uint8_t)(maxp > 63 ? 63 : maxp);
}

static int pp_restore_slot(party_mon_t *p, int m, int is_max) {
    uint8_t move_id = p->base.moves[m];
    if (move_id == 0) return 0;
    uint8_t pp_ups = (uint8_t)((p->base.pp[m] >> 6) & 0x03);
    uint8_t maxp   = pp_move_max(move_id, pp_ups);
    uint8_t cur    = (uint8_t)(p->base.pp[m] & PP_MASK);
    if (cur >= maxp) return 0;
    uint8_t neu = is_max ? maxp
                         : (uint8_t)((cur + 10 > maxp) ? maxp : cur + 10);
    p->base.pp[m] = (uint8_t)((pp_ups << 6) | neu);
    return 1;
}

int Pokemon_ApplyPPRestore(uint8_t slot, int move_index, uint8_t item_id) {
    if (slot >= wPartyCount) return 0;
    party_mon_t *p = &wPartyMons[slot];
    int is_max    = (item_id == ITEM_MAX_ETHER || item_id == ITEM_MAX_ELIXER);
    int is_elixer = (item_id == ITEM_ELIXER    || item_id == ITEM_MAX_ELIXER);
    int any = 0;
    if (is_elixer) {
        for (int m = 0; m < 4; m++) {
            uint8_t before = (uint8_t)(p->base.pp[m] & PP_MASK);
            if (pp_restore_slot(p, m, is_max)) {
                any = 1;
                printf("[item] %selixer %s move%d: PP %u -> %u\n",
                       is_max ? "max-" : "",
                       Pokemon_GetNameBySpecies(p->base.species),
                       m + 1, before, (unsigned)(p->base.pp[m] & PP_MASK));
            }
        }
    } else {
        if (move_index < 0 || move_index > 3) return 0;
        uint8_t before = (uint8_t)(p->base.pp[move_index] & PP_MASK);
        any = pp_restore_slot(p, move_index, is_max);
        if (any)
            printf("[item] %sether %s move%d: PP %u -> %u\n",
                   is_max ? "max-" : "",
                   Pokemon_GetNameBySpecies(p->base.species),
                   move_index + 1, before,
                   (unsigned)(p->base.pp[move_index] & PP_MASK));
    }
    return any;
}

int Pokemon_ApplyPPUp(uint8_t slot, int move_index) {
    if (slot >= wPartyCount || move_index < 0 || move_index > 3) return 0;
    party_mon_t *p = &wPartyMons[slot];
    uint8_t move_id = p->base.moves[move_index];
    if (move_id == 0 || move_id >= NUM_MOVE_DEFS) return 0;
    uint8_t pp_ups = (uint8_t)((p->base.pp[move_index] >> 6) & 0x03);
    if (pp_ups >= 3) return 0;

    uint8_t base = gMoves[move_id].pp;
    uint8_t per  = (uint8_t)(base / 5); if (per > 7) per = 7;
    uint8_t cur  = (uint8_t)(p->base.pp[move_index] & PP_MASK);
    uint8_t old_ups = pp_ups;
    pp_ups++;
    uint8_t maxp = pp_move_max(move_id, pp_ups);
    uint16_t neu = (uint16_t)cur + per; if (neu > maxp) neu = maxp;
    p->base.pp[move_index] = (uint8_t)((pp_ups << 6) | (uint8_t)neu);
    printf("[item] pp-up %s move%d: PP-Up %u->%u, PP %u -> %u (max now %u)\n",
           Pokemon_GetNameBySpecies(p->base.species), move_index + 1,
           old_ups, pp_ups, cur, (unsigned)neu, maxp);
    return 1;
}

int Pokemon_CanEvolveWithStone(uint8_t slot, uint8_t stone_item_id) {
    if (slot >= wPartyCount) return 0;
    uint8_t species = wPartyMons[slot].base.species;
    if (species == 0 || species >= EVOS_MOVES_TABLE_SIZE) return 0;
    const uint8_t *d = gEvosMoves[species];
    if (!d) return 0;
    while (*d != 0) {
        if (*d == EVOLVE_ITEM) {
            if (d[1] == stone_item_id) return 1;
            d += 4;
        } else {
            d += 3;
        }
    }
    return 0;
}

int Pokemon_ApplyEvoStone(uint8_t slot, uint8_t stone_item_id) {
    if (slot >= wPartyCount) return 0;
    wCurPartySpecies = stone_item_id;
    wForceEvolution  = 1;
    if (evo_check_one_mon(slot)) {
        wCanEvolveFlags = (uint8_t)(1u << slot);
        return 1;
    }
    wForceEvolution = 0;
    return 0;
}

void Battle_AwardExpForFaintedEnemy(void) {
    int has_exp_all = Inventory_GetQty(ITEM_EXP_ALL) > 0;

    if (s_modern_share_on) {
        uint8_t participants;

        s_exp_all_halve   = 0;
        s_modern_share_pass = 0;
        wBoostExpByExpAll = 0;
        participants = wPartyGainExpFlags;
        Battle_GainExperience();

        {
            uint8_t all = (uint8_t)((wPartyCount >= 8) ? 0xFFu
                                                       : ((1u << wPartyCount) - 1u));
            uint8_t others = (uint8_t)(all & (uint8_t)~participants);
            if (others) {
                wBoostExpByExpAll   = 1;
                wPartyGainExpFlags  = others;
                s_modern_share_pass = 1;
                s_exp_queue_keep    = 1;
                Battle_GainExperience();
                s_exp_queue_keep    = 0;
                s_modern_share_pass = 0;
            }
        }
        return;
    }

    s_exp_all_halve = has_exp_all ? 1u : 0u;

    wBoostExpByExpAll = 0;
    Battle_GainExperience();

    if (!has_exp_all) {
        s_exp_all_halve = 0;
        return;
    }

    wBoostExpByExpAll  = 1;
    wPartyGainExpFlags = (uint8_t)((wPartyCount >= 8) ? 0xFFu
                                                      : ((1u << wPartyCount) - 1u));
    Battle_GainExperience();

    s_exp_all_halve = 0;
}
