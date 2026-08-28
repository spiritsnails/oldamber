
#define BATTLE_DEBUG 1
#include "battle.h"
#include "../gen2_species.h"
#include "../../data/base_stats.h"
#include <string.h>
#if BATTLE_DEBUG
#include <stdio.h>
#endif
#include "../../data/type_chart.h"
#include "game/battle/battle_probe.h"
#include "game/battle/battle_effects.h"

static battle_hittrace_t s_last_hittrace = {.enabled = 1};
static uint32_t s_hittrace_seq = 0;

void Battle_HitTraceEnable(uint8_t enable) {
    s_last_hittrace.enabled = enable ? 1u : 0u;
}

uint8_t Battle_HitTraceIsEnabled(void) {
    return s_last_hittrace.enabled;
}

void Battle_HitTraceReset(void) {
    uint8_t en = s_last_hittrace.enabled;
    memset(&s_last_hittrace, 0, sizeof(s_last_hittrace));
    s_last_hittrace.enabled = en;
    s_hittrace_seq = 0;
}

battle_hittrace_t Battle_GetLastHitTrace(void) {
    return s_last_hittrace;
}

const uint8_t kBattleStatModRatios[13][2] = {
    {25, 100}, {28, 100}, {33, 100}, {40, 100}, {50, 100}, {66, 100},
    { 1,   1}, {15,  10}, { 2,   1}, {25,  10}, { 3,   1}, {35,  10}, { 4, 1}
};

static const uint8_t kHighCritMoves[] = {
    MOVE_KARATE_CHOP,
    MOVE_RAZOR_LEAF,
    MOVE_CRABHAMMER,
    MOVE_SLASH,
    0xFF
};

int Battle_CalcDamage(uint8_t attack, uint8_t defense, uint8_t power, uint8_t level) {
    BPROBE("CalculateDamage");
    uint8_t effect = hWhoseTurn ? wEnemyMoveEffect : wPlayerMoveEffect;

    if (effect == EFFECT_EXPLODE) {
        defense >>= 1;
        if (defense == 0) defense = 1;
    }

    int skip_power_check = (effect == EFFECT_TWO_TO_FIVE_ATTACKS ||
                            effect == EFFECT_1E);

    if (!skip_power_check) {

        if (effect == EFFECT_OHKO) {
            Battle_JumpMoveEffect();
            return wMoveMissed ? 0 : 1;
        }
        if (power == 0) return 0;
    }

    uint32_t d = (uint32_t)level * 2 / 5 + 2;
    d = d * (uint32_t)power;
    d = d * (uint32_t)attack;
    d = d / (uint32_t)(defense ? defense : 1);
    d = d / 50;

    uint32_t total = (uint32_t)wDamage + d;
    if (total > (uint32_t)(MAX_NEUTRAL_DAMAGE - MIN_NEUTRAL_DAMAGE)) {
        total = MAX_NEUTRAL_DAMAGE - MIN_NEUTRAL_DAMAGE;
    }

    total += MIN_NEUTRAL_DAMAGE;
    wDamage = (uint16_t)total;
    return 1;
}

void Battle_CriticalHitTest(void) {
    BPROBE("CriticalHitTest");
    wCriticalHitOrOHKO = 0;

    uint8_t species = hWhoseTurn ? wEnemyMon.species : wBattleMon.species;

    uint8_t dex = gSpeciesToDex[species];
    base_stats_t crit_bs;
    (void)dex;
    if (!Species_GetBaseStats(species, &crit_bs)) return;
    uint8_t base_speed = crit_bs.spd;

    uint8_t b = base_speed >> 1;

    uint8_t move_power  = hWhoseTurn ? wEnemyMovePower  : wPlayerMovePower;
    uint8_t move_num    = hWhoseTurn ? wEnemyMoveNum    : wPlayerMoveNum;
    uint8_t bstat2      = hWhoseTurn ? wEnemyBattleStatus2 : wPlayerBattleStatus2;

    if (move_power == 0) return;

    if (bstat2 & (1 << BSTAT2_GETTING_PUMPED)) {
        b >>= 1;
    } else {

        b = (b & 0x80) ? 0xFF : (uint8_t)(b << 1);
    }

    int is_high_crit = 0;
    for (int i = 0; kHighCritMoves[i] != 0xFF; i++) {
        if (kHighCritMoves[i] == move_num) { is_high_crit = 1; break; }
    }

    if (is_high_crit) {

        b = (b & 0x80) ? 0xFF : (uint8_t)(b << 1);
        b = (b & 0x80) ? 0xFF : (uint8_t)(b << 1);
    } else {

        b >>= 1;
    }

    uint8_t r = BattleRandom();
    r = (uint8_t)((r << 3) | (r >> 5));

    if (r < b) {
        wCriticalHitOrOHKO = 1;
    }
}

void Battle_RandomizeDamage(void) {
    BPROBE("RandomizeDamage");
    uint16_t dmg = wDamage;

    if (dmg <= 1) return;

    uint8_t r;
    do {
        r = BattleRandom();
        r = (uint8_t)((r >> 1) | (r << 7));
    } while (r < 217);

    uint32_t product = (uint32_t)dmg * r;
    wDamage = (uint16_t)(product / 255);
}

void Battle_CalcHitChance(void) {
    uint8_t *acc_ptr;
    uint8_t  acc_mod;
    uint8_t  eva_mod;

    if (hWhoseTurn == 0) {

        acc_ptr = &wPlayerMoveAccuracy;
        acc_mod = wPlayerMonStatMods[MOD_ACCURACY];
        eva_mod = wEnemyMonStatMods[MOD_EVASION];
    } else {

        acc_ptr = &wEnemyMoveAccuracy;
        acc_mod = wEnemyMonStatMods[MOD_ACCURACY];
        eva_mod = wPlayerMonStatMods[MOD_EVASION];
    }

    uint8_t eff_eva = (uint8_t)(14 - eva_mod);

    uint32_t val = *acc_ptr;

    uint8_t stages[2] = {acc_mod, eff_eva};
    for (int i = 0; i < 2; i++) {
        uint8_t stage = stages[i];
        if (stage < 1)  stage = 1;
        if (stage > 13) stage = 13;

        uint8_t num = kBattleStatModRatios[stage - 1][0];
        uint8_t den = kBattleStatModRatios[stage - 1][1];

        val = val * num / den;

        if (val == 0) val = 1;
    }

    if (val > 255) val = 255;
    *acc_ptr = (uint8_t)val;
}

void Battle_MoveHitTest(void) {
    BPROBE("MoveHitTest");
    int player_turn     = (hWhoseTurn == 0);
    uint8_t move_effect = player_turn ? wPlayerMoveEffect : wEnemyMoveEffect;
    uint8_t tgt_status  = player_turn ? wEnemyMon.status  : wBattleMon.status;
    uint8_t tgt_bstat1  = player_turn ? wEnemyBattleStatus1 : wPlayerBattleStatus1;
    uint8_t tgt_bstat2  = player_turn ? wEnemyBattleStatus2 : wPlayerBattleStatus2;
    uint8_t att_bstat2  = player_turn ? wPlayerBattleStatus2 : wEnemyBattleStatus2;
    uint8_t base_acc    = player_turn ? wPlayerMoveAccuracy : wEnemyMoveAccuracy;
    uint8_t move_num    = player_turn ? wPlayerMoveNum : wEnemyMoveNum;
    uint8_t roll        = 0;
    uint8_t reason      = BHTR_HIT;

    if (move_effect == EFFECT_DREAM_EATER) {
        if (!(tgt_status & STATUS_SLP_MASK)) {
            reason = BHTR_MISS_DREAM_EATER;
            goto move_missed;
        }
    }

    if (move_effect == EFFECT_SWIFT) {
        reason = BHTR_HIT_SWIFT;
        goto move_hit;
    }

    {
        int has_sub = (tgt_bstat2 >> BSTAT2_HAS_SUBSTITUTE) & 1;
        if (has_sub) {
            uint8_t bug_a = (uint8_t)has_sub;
            if (bug_a == EFFECT_DRAIN_HP)    goto move_missed;
            if (bug_a == EFFECT_DREAM_EATER) goto move_missed;

        }
    }

    if (tgt_bstat1 & (1 << BSTAT1_INVULNERABLE)) {
        reason = BHTR_MISS_INVULNERABLE;
        goto move_missed;
    }

    {
        int mist_blocked = (move_effect >= EFFECT_ATTACK_DOWN1 &&
                            move_effect <= EFFECT_HAZE) ||
                           (move_effect >= EFFECT_ATTACK_DOWN2 &&
                            move_effect <= EFFECT_REFLECT);
        if (mist_blocked && (tgt_bstat2 & (1 << BSTAT2_PROTECTED_BY_MIST))) {
            reason = BHTR_MISS_MIST;
            goto move_missed;
        }
    }

    if (att_bstat2 & (1 << BSTAT2_USING_X_ACCURACY)) {
        reason = BHTR_HIT_XACCURACY;
        goto move_hit;
    }

    Battle_CalcHitChance();
    {
        uint8_t acc  = player_turn ? wPlayerMoveAccuracy : wEnemyMoveAccuracy;
        roll = BattleRandom();
        if (roll >= acc) {
            reason = BHTR_MISS_ACCURACY_ROLL;
            goto move_missed;
        }
    }
move_hit:
    if (s_last_hittrace.enabled) {
        s_last_hittrace.seq = ++s_hittrace_seq;
        s_last_hittrace.player_turn = player_turn ? 1u : 0u;
        s_last_hittrace.move_num = move_num;
        s_last_hittrace.move_effect = move_effect;
        s_last_hittrace.base_acc = base_acc;
        s_last_hittrace.scaled_acc = player_turn ? wPlayerMoveAccuracy : wEnemyMoveAccuracy;
        s_last_hittrace.roll = roll;
        s_last_hittrace.missed = 0;
        s_last_hittrace.reason = reason;
        BLOG("  HITTRACE seq=%lu turn=%s move=%s(0x%02X) eff=0x%02X base_acc=%u scaled_acc=%u roll=%u missed=0 reason=%u",
             (unsigned long)s_last_hittrace.seq,
             s_last_hittrace.player_turn ? "player" : "enemy",
             BMOVE(s_last_hittrace.move_num), s_last_hittrace.move_num,
             s_last_hittrace.move_effect,
             s_last_hittrace.base_acc, s_last_hittrace.scaled_acc,
             s_last_hittrace.roll, s_last_hittrace.reason);
    }
    return;

move_missed:
    wDamage     = 0;
    wMoveMissed = 1;

    if (player_turn) {
        wPlayerBattleStatus1 &= (uint8_t)~(1 << BSTAT1_USING_TRAPPING);
    } else {
        wEnemyBattleStatus1  &= (uint8_t)~(1 << BSTAT1_USING_TRAPPING);
    }
    if (s_last_hittrace.enabled) {
        s_last_hittrace.seq = ++s_hittrace_seq;
        s_last_hittrace.player_turn = player_turn ? 1u : 0u;
        s_last_hittrace.move_num = move_num;
        s_last_hittrace.move_effect = move_effect;
        s_last_hittrace.base_acc = base_acc;
        s_last_hittrace.scaled_acc = player_turn ? wPlayerMoveAccuracy : wEnemyMoveAccuracy;
        s_last_hittrace.roll = roll;
        s_last_hittrace.missed = 1;
        s_last_hittrace.reason = reason;
        BLOG("  HITTRACE seq=%lu turn=%s move=%s(0x%02X) eff=0x%02X base_acc=%u scaled_acc=%u roll=%u missed=1 reason=%u",
             (unsigned long)s_last_hittrace.seq,
             s_last_hittrace.player_turn ? "player" : "enemy",
             BMOVE(s_last_hittrace.move_num), s_last_hittrace.move_num,
             s_last_hittrace.move_effect,
             s_last_hittrace.base_acc, s_last_hittrace.scaled_acc,
             s_last_hittrace.roll, s_last_hittrace.reason);
    }
}

static uint16_t enemy_raw_stat(uint8_t stat_idx) {
    uint8_t dex = gSpeciesToDex[wEnemyMon.species];
    base_stats_t enemy_bs;
    (void)dex;
    if (!Species_GetBaseStats(wEnemyMon.species, &enemy_bs)) return 1;
    const base_stats_t *bs = &enemy_bs;
    uint16_t dvs = wEnemyMon.dvs;
    uint8_t base, dv;
    switch (stat_idx) {
        default: return 1;
        case STAT_ATTACK:
            base = bs->atk;  dv = (uint8_t)((dvs >> 12) & 0xF); break;
        case STAT_DEFENSE:
            base = bs->def;  dv = (uint8_t)((dvs >> 8) & 0xF);  break;
        case STAT_SPEED:
            base = bs->spd;  dv = (uint8_t)((dvs >> 4) & 0xF);  break;
        case STAT_SPECIAL_STAT:
            base = bs->spc;  dv = (uint8_t)(dvs & 0xF);         break;
    }
    return CalcStat(base, dv, 0, wEnemyMon.level, 0);
}

static void scale_for_damage(uint16_t *offense, uint16_t *defense) {
    if ((*offense >> 8) | (*defense >> 8)) {
        *defense >>= 2;
        *offense >>= 2;
        if (*offense == 0) *offense = 1;
    }
}

void Battle_RecalculateStat(uint8_t stat_idx) {
    if (stat_idx >= NUM_STATS) return;

    battle_mon_t *mon;
    uint8_t *mods;
    uint16_t unmod;

    if (wCalculateWhoseStats == 0) {
        mon = &wBattleMon;
        mods = wPlayerMonStatMods;
        unmod = (stat_idx == 0) ? wPlayerMonUnmodifiedAttack
              : (stat_idx == 1) ? wPlayerMonUnmodifiedDefense
              : (stat_idx == 2) ? wPlayerMonUnmodifiedSpeed
                                : wPlayerMonUnmodifiedSpecial;
    } else {
        mon = &wEnemyMon;
        mods = wEnemyMonStatMods;
        unmod = (stat_idx == 0) ? wEnemyMonUnmodifiedAttack
              : (stat_idx == 1) ? wEnemyMonUnmodifiedDefense
              : (stat_idx == 2) ? wEnemyMonUnmodifiedSpeed
                                : wEnemyMonUnmodifiedSpecial;
    }

    uint8_t stage = mods[stat_idx];
    if (stage < 1)  stage = 1;
    if (stage > 13) stage = 13;

    uint32_t val = (uint32_t)unmod * kBattleStatModRatios[stage - 1][0]
                                   / kBattleStatModRatios[stage - 1][1];
    if (val > MAX_NEUTRAL_DAMAGE) val = MAX_NEUTRAL_DAMAGE;
    if (val == 0) val = 1;

    uint16_t *stat_ptrs[NUM_STATS] = { &mon->atk, &mon->def, &mon->spd, &mon->spc };
    *stat_ptrs[stat_idx] = (uint16_t)val;
}

void Battle_CalculateModifiedStats(void) {
    battle_mon_t *mon;
    uint8_t *mods;
    uint16_t unmod[NUM_STATS];

    if (wCalculateWhoseStats == 0) {
        mon = &wBattleMon;
        mods = wPlayerMonStatMods;
        unmod[0] = wPlayerMonUnmodifiedAttack;
        unmod[1] = wPlayerMonUnmodifiedDefense;
        unmod[2] = wPlayerMonUnmodifiedSpeed;
        unmod[3] = wPlayerMonUnmodifiedSpecial;
    } else {
        mon = &wEnemyMon;
        mods = wEnemyMonStatMods;
        unmod[0] = wEnemyMonUnmodifiedAttack;
        unmod[1] = wEnemyMonUnmodifiedDefense;
        unmod[2] = wEnemyMonUnmodifiedSpeed;
        unmod[3] = wEnemyMonUnmodifiedSpecial;
    }

    uint16_t *stat_ptrs[NUM_STATS] = { &mon->atk, &mon->def, &mon->spd, &mon->spc };

    static const char *stat_names[4] = {"ATK","DEF","SPD","SPC"};
    for (int c = 0; c < NUM_STATS; c++) {
        uint8_t stage = mods[c];
        if (stage < 1)  stage = 1;
        if (stage > 13) stage = 13;

        uint8_t  num = kBattleStatModRatios[stage - 1][0];
        uint8_t  den = kBattleStatModRatios[stage - 1][1];
        uint32_t val = (uint32_t)unmod[c] * num / den;

        if (val > MAX_NEUTRAL_DAMAGE) val = MAX_NEUTRAL_DAMAGE;
        if (val == 0) val = 1;

#if BATTLE_DEBUG
        printf("[DBG] CalcModStats %s side %s: unmod=%u stage=%d -> %u\n",
               (wCalculateWhoseStats==0)?"player":"enemy",
               stat_names[c], unmod[c], (int)stage-7, (unsigned)val);
#endif
        *stat_ptrs[c] = (uint16_t)val;
    }
}

void Battle_ApplyBurnAndParalysisPenalties(void) {
    battle_mon_t *mon = (hWhoseTurn != 0) ? &wBattleMon : &wEnemyMon;

    if (mon->status & STATUS_PAR) {
        uint16_t spd = (uint16_t)(mon->spd >> 2);
        if (spd == 0) spd = 1;
        mon->spd = spd;
    }

    if (mon->status & STATUS_BRN) {
        uint16_t atk = (uint16_t)(mon->atk >> 1);
        if (atk == 0) atk = 1;
        mon->atk = atk;
    }
}

int Battle_GetDamageVarsForPlayerAttack(void) {
    BPROBE("GetDamageVarsForPlayerAttack");
    wDamage = 0;

    uint8_t power = wPlayerMovePower;
    uint8_t move_type = wPlayerMoveType;
    uint16_t offense, defense;

    if (move_type < TYPE_SPECIAL_THRESHOLD) {

        defense = wEnemyMon.def;
        if (wEnemyBattleStatus3 & (1 << BSTAT3_HAS_REFLECT))
            defense <<= 1;

        if (wCriticalHitOrOHKO) {

            offense = wPartyMons[wPlayerMonNumber].atk;
            defense = enemy_raw_stat(STAT_DEFENSE);
        } else {
            offense = wBattleMon.atk;
        }
    } else {

        defense = wEnemyMon.spc;
        if (wEnemyBattleStatus3 & (1 << BSTAT3_HAS_LIGHT_SCREEN))
            defense <<= 1;

        if (wCriticalHitOrOHKO) {
            offense = wPartyMons[wPlayerMonNumber].spc;
            defense = enemy_raw_stat(STAT_SPECIAL_STAT);
        } else {
            offense = wBattleMon.spc;
        }
    }

#if BATTLE_DEBUG
    printf("[DBG] PlayerAtk: offense(atk)=%u defense(def)=%u power=%u level=%u crit=%d\n",
           offense, defense, power, wBattleMon.level, wCriticalHitOrOHKO);
#endif
    scale_for_damage(&offense, &defense);

    uint8_t level = wBattleMon.level;
    if (wCriticalHitOrOHKO) level = (uint8_t)(level << 1);

    return Battle_CalcDamage((uint8_t)offense, (uint8_t)defense, power, level);
}

int Battle_GetDamageVarsForEnemyAttack(void) {
    BPROBE("GetDamageVarsForEnemyAttack");
    wDamage = 0;

    uint8_t power = wEnemyMovePower;

    uint8_t move_type = wEnemyMoveType;
    uint16_t offense, defense;

    if (move_type < TYPE_SPECIAL_THRESHOLD) {

        defense = wBattleMon.def;
        if (wPlayerBattleStatus3 & (1 << BSTAT3_HAS_REFLECT))
            defense <<= 1;

        if (wCriticalHitOrOHKO) {
            defense = wPartyMons[wPlayerMonNumber].def;
            offense = enemy_raw_stat(STAT_ATTACK);
        } else {
            offense = wEnemyMon.atk;
        }
    } else {

        defense = wBattleMon.spc;
        if (wPlayerBattleStatus3 & (1 << BSTAT3_HAS_LIGHT_SCREEN))
            defense <<= 1;

        if (wCriticalHitOrOHKO) {
            defense = wPartyMons[wPlayerMonNumber].spc;
            offense = enemy_raw_stat(STAT_SPECIAL_STAT);
        } else {
            offense = wEnemyMon.spc;
        }
    }

#if BATTLE_DEBUG
    printf("[DBG] EnemyAtk: offense(atk)=%u defense(def)=%u power=%u level=%u crit=%d\n",
           offense, defense, power, wEnemyMon.level, wCriticalHitOrOHKO);
#endif
    scale_for_damage(&offense, &defense);

    uint8_t level = wEnemyMon.level;
    if (wCriticalHitOrOHKO) level = (uint8_t)(level << 1);

    {
        int r = Battle_CalcDamage((uint8_t)offense, (uint8_t)defense, power, level);
#if BATTLE_DEBUG
        printf("[DBG] EnemyAtk  -> base wDamage=%u (scaled off=%u def=%u)\n",
               (unsigned)wDamage, offense, defense);
#endif
        return r;
    }
}

void Battle_AdjustDamageForMoveType(void) {
    BPROBE("AdjustDamageForMoveType");
    uint8_t atk_type1, atk_type2, def_type1, def_type2, move_type;

    if (hWhoseTurn == 0) {
        atk_type1 = wBattleMon.type1;
        atk_type2 = wBattleMon.type2;
        def_type1 = wEnemyMon.type1;
        def_type2 = wEnemyMon.type2;
        move_type = wPlayerMoveType;
    } else {
        atk_type1 = wEnemyMon.type1;
        atk_type2 = wEnemyMon.type2;
        def_type1 = wBattleMon.type1;
        def_type2 = wBattleMon.type2;
        move_type = wEnemyMoveType;
    }
    wMoveType = move_type;

    if (move_type == atk_type1 || move_type == atk_type2) {
        wDamage += (wDamage >> 1);
        wDamageMultipliers |= (uint8_t)(1 << BIT_STAB_DAMAGE);
    }

    const type_entry_t *tbl = TypeChart_Table();
    for (int i = 0; tbl[i].atk != 0xFFu; i++) {
        if (tbl[i].atk != move_type) continue;
        if (tbl[i].def != def_type1 && tbl[i].def != def_type2) continue;
        uint8_t eff = TypeEffectiveness(move_type, tbl[i].def);
        uint8_t stab_bit = wDamageMultipliers & (uint8_t)(1 << BIT_STAB_DAMAGE);
        wDamageMultipliers = stab_bit + eff;
        wDamage = (uint16_t)((uint32_t)wDamage * eff / 10);
        if (wDamage == 0) wMoveMissed = 1;
#if BATTLE_DEBUG
        printf("[DBG] TypeAdj  -> eff=%u wDamage=%u missed=%u\n",
               (unsigned)eff, (unsigned)wDamage, (unsigned)wMoveMissed);
#endif
    }
#if BATTLE_DEBUG
    printf("[DBG] TypeAdj final wDamage=%u mult=0x%02X movetype=%u deftypes=%u/%u\n",
           (unsigned)wDamage, (unsigned)wDamageMultipliers,
           (unsigned)move_type, (unsigned)def_type1, (unsigned)def_type2);
#endif
}
