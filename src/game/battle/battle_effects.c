
#include "battle_effects.h"
#include "battle.h"
#include "battle_core.h"
#include "battle_switch.h"
#include "../../platform/hardware.h"
#include "../constants.h"
#include "../types.h"
#include "game/battle/battle_probe.h"

extern uint8_t BattleRandom(void);

#define PCT10   26
#define PCT20   52
#define PCT25   64
#define PCT30   77
#define PCT33   85
#define PCT40  103

#define SET_BIT(v, b)  ((v) |=  (1u << (b)))
#define RES_BIT(v, b)  ((v) &= ~(1u << (b)))
#define TST_BIT(v, b)  (((v) >> (b)) & 1u)

static const char *bfx_status_name(uint8_t status) {
    if (status & STATUS_PSN) return "PSN";
    if (status & STATUS_BRN) return "BRN";
    if (status & STATUS_FRZ) return "FRZ";
    if (status & STATUS_PAR) return "PAR";
    if (status & STATUS_SLP_MASK) return "SLP";
    return "OK";
}

static void bfx_log_status_change(const char *tag, const char *target_side,
                                  uint8_t before, uint8_t after) {
    BLOG("%s target=%s status 0x%02X(%s) -> 0x%02X(%s)",
         tag, target_side,
         before, bfx_status_name(before),
         after, bfx_status_name(after));
}

static int CheckTargetSubstitute(void) {
    if (hWhoseTurn == 0)
        return TST_BIT(wEnemyBattleStatus2, BSTAT2_HAS_SUBSTITUTE);
    else
        return TST_BIT(wPlayerBattleStatus2, BSTAT2_HAS_SUBSTITUTE);
}

static void ClearHyperBeam(void) {
    if (hWhoseTurn == 0)
        RES_BIT(wEnemyBattleStatus2, BSTAT2_NEEDS_TO_RECHARGE);
    else
        RES_BIT(wPlayerBattleStatus2, BSTAT2_NEEDS_TO_RECHARGE);
}

static uint16_t QuarterSpeed16(uint16_t v) {
    v >>= 2;
    return v ? v : 1;
}

static uint16_t HalveVal16(uint16_t v) {
    v >>= 1;
    return v ? v : 1;
}

static void AddBCD3(uint8_t *dst, const uint8_t *src) {

    int carry = 0;
    for (int i = 2; i >= 0; i--) {
        int lo = (dst[i] & 0xF) + (src[i] & 0xF) + carry;
        carry = lo >= 10 ? 1 : 0;
        if (lo >= 10) lo -= 10;
        int hi = (dst[i] >> 4) + (src[i] >> 4) + carry;
        carry = hi >= 10 ? 1 : 0;
        if (hi >= 10) hi -= 10;
        dst[i] = (uint8_t)((hi << 4) | lo);
    }
    if (carry) {

        dst[0] = dst[1] = dst[2] = 0x99;
    }
}

void Battle_QuarterSpeedDueToParalysis(void) {
    if (hWhoseTurn == 0) {

        if (!(wEnemyMon.status & STATUS_PAR)) return;
        wEnemyMon.spd = QuarterSpeed16(wEnemyMon.spd);
    } else {

        if (!(wBattleMon.status & STATUS_PAR)) return;
        wBattleMon.spd = QuarterSpeed16(wBattleMon.spd);
    }
}

void Battle_HalveAttackDueToBurn(void) {
    if (hWhoseTurn == 0) {

        if (!(wEnemyMon.status & STATUS_BRN)) return;
        wEnemyMon.atk = HalveVal16(wEnemyMon.atk);
    } else {

        if (!(wBattleMon.status & STATUS_BRN)) return;
        wBattleMon.atk = HalveVal16(wBattleMon.atk);
    }
}

static void Effect_Sleep(void) {
    uint8_t *target_status;
    uint8_t *target_bstat2;
    const char *target_side = (hWhoseTurn == 0) ? "enemy" : "player";
    if (hWhoseTurn == 0) {
        target_status = &wEnemyMon.status;
        target_bstat2 = &wEnemyBattleStatus2;
    } else {
        target_status = &wBattleMon.status;
        target_bstat2 = &wPlayerBattleStatus2;
    }

    int was_recharging = TST_BIT(*target_bstat2, BSTAT2_NEEDS_TO_RECHARGE);
    RES_BIT(*target_bstat2, BSTAT2_NEEDS_TO_RECHARGE);

    if (!was_recharging) {

        if (IS_ASLEEP(*target_status)) {
            BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_ALREADY_ASLEEP);
            return;
        }
        if (*target_status != 0) {
            BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_DIDNT_AFFECT);
            return;
        }

        Battle_MoveHitTest();
        if (wMoveMissed) {
            BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_DIDNT_AFFECT);
            return;
        }
    }

    uint8_t before = *target_status;
    uint8_t counter;
    do { counter = BattleRandom() & STATUS_SLP_MASK; } while (counter == 0);
    *target_status = counter;
    bfx_log_status_change("sleep-applied", target_side, before, *target_status);
}

static int CheckDefrost(uint8_t target_status) {
    if (!(target_status & STATUS_FRZ)) return 0;

    uint8_t move_type = (hWhoseTurn == 0) ? wPlayerMoveType : wEnemyMoveType;
    if (move_type != TYPE_FIRE) return 0;

    if (hWhoseTurn == 0) {
        wEnemyMon.status = 0;
    } else {
        wBattleMon.status = 0;
    }

    BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_FIRE_DEFROSTED,
                              (uint8_t)((hWhoseTurn == 0) ? 1u : 0u), 0u);
    return 1;
}

static void Effect_Poison(void) {
    uint8_t effect = (hWhoseTurn == 0) ? wPlayerMoveEffect : wEnemyMoveEffect;
    uint8_t *target_status;
    uint8_t *target_type1;
    uint8_t *target_type2;
    uint8_t *target_bstat3;
    uint8_t *target_toxic_ctr;
    if (hWhoseTurn == 0) {
        target_status    = &wEnemyMon.status;
        target_type1     = &wEnemyMon.type1;
        target_type2     = &wEnemyMon.type2;
        target_bstat3    = &wEnemyBattleStatus3;
        target_toxic_ctr = &wEnemyToxicCounter;
    } else {
        target_status    = &wBattleMon.status;
        target_type1     = &wBattleMon.type1;
        target_type2     = &wBattleMon.type2;
        target_bstat3    = &wPlayerBattleStatus3;
        target_toxic_ctr = &wPlayerToxicCounter;
    }

    if (CheckTargetSubstitute()) {
        BLOG("status-blocked target=%s substitute", (hWhoseTurn == 0) ? "enemy" : "player");
        if (effect == EFFECT_POISON) {
            BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_DIDNT_AFFECT);
            goto noEffect;
        }
        return;
    }

    if (*target_status != 0) {
        if (effect == EFFECT_POISON) {
            BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_DIDNT_AFFECT);
            goto noEffect;
        }
        return;
    }

    if (*target_type1 == TYPE_POISON || *target_type2 == TYPE_POISON) {
        if (effect == EFFECT_POISON) {
            BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_DIDNT_AFFECT);
            goto noEffect;
        }
        return;
    }

    if (effect == EFFECT_POISON_SIDE1) {
        if (BattleRandom() >= PCT20) return;
    } else if (effect == EFFECT_POISON_SIDE2) {
        if (BattleRandom() >= PCT40) return;
    } else {

        Battle_MoveHitTest();
        if (wMoveMissed) {
            BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_DIDNT_AFFECT);
            goto noEffect;
        }
    }

    {
        uint8_t before = *target_status;
        *target_status |= STATUS_PSN;
        bfx_log_status_change("poison-applied",
                              (hWhoseTurn == 0) ? "enemy" : "player",
                              before, *target_status);
    }

    uint8_t move_num = (hWhoseTurn == 0) ? wPlayerMoveNum : wEnemyMoveNum;
    if (move_num == MOVE_TOXIC) {
        SET_BIT(*target_bstat3, BSTAT3_BADLY_POISONED);
        *target_toxic_ctr = 0;
    }

    if (effect != EFFECT_POISON) {
        BattleEvent_PushPlayAnim((uint8_t)((hWhoseTurn == 0) ? 169u : 199u),
                                 (uint8_t)hWhoseTurn);
    }
    return;

noEffect:

    return;
}

static void Effect_FreezeBurnParalyze(void) {
    uint8_t effect  = (hWhoseTurn == 0) ? wPlayerMoveEffect : wEnemyMoveEffect;
    uint8_t mv_type = (hWhoseTurn == 0) ? wPlayerMoveType   : wEnemyMoveType;
    uint8_t *target_status;
    uint8_t *target_type1;
    uint8_t *target_type2;
    if (hWhoseTurn == 0) {
        target_status = &wEnemyMon.status;
        target_type1  = &wEnemyMon.type1;
        target_type2  = &wEnemyMon.type2;
    } else {
        target_status = &wBattleMon.status;
        target_type1  = &wBattleMon.type1;
        target_type2  = &wBattleMon.type2;
    }

    if (CheckTargetSubstitute()) return;

    if (*target_status != 0) {
        CheckDefrost(*target_status);
        BLOG("status-blocked target=%s already statused 0x%02X(%s)",
             (hWhoseTurn == 0) ? "enemy" : "player",
             *target_status, bfx_status_name(*target_status));
        return;
    }

    if (*target_type1 == mv_type || *target_type2 == mv_type) {
        BLOG("status-blocked target=%s immune to move type=%u",
             (hWhoseTurn == 0) ? "enemy" : "player", (unsigned)mv_type);
        return;
    }

    uint8_t base_effect = effect;
    uint8_t threshold   = PCT10;
    if (effect >= EFFECT_BURN_SIDE2 && effect <= EFFECT_PARALYZE_SIDE2) {

        threshold   = PCT30;
        base_effect = effect - (EFFECT_BURN_SIDE2 - EFFECT_BURN_SIDE);
    }

    if (BattleRandom() >= threshold) {
        BLOG("status-miss target=%s effect=%u threshold=%u",
             (hWhoseTurn == 0) ? "enemy" : "player",
             (unsigned)effect, (unsigned)threshold);
        return;
    }

    {
        uint8_t before = *target_status;
        if (base_effect == EFFECT_BURN_SIDE) {
            *target_status = STATUS_BRN;

            if (hWhoseTurn == 0) {
                wEnemyMon.atk = HalveVal16(wEnemyMon.atk);
            } else {
                wBattleMon.atk = HalveVal16(wBattleMon.atk);
            }
        } else if (base_effect == EFFECT_FREEZE_SIDE) {

            ClearHyperBeam();
            *target_status = STATUS_FRZ;
        } else {

            *target_status = STATUS_PAR;

            if (hWhoseTurn == 0) {
                wEnemyMon.spd = QuarterSpeed16(wEnemyMon.spd);
            } else {
                wBattleMon.spd = QuarterSpeed16(wBattleMon.spd);
            }
        }
        bfx_log_status_change("status-applied",
                              (hWhoseTurn == 0) ? "enemy" : "player",
                              before, *target_status);
    }

    if (hWhoseTurn == 0) {
        BattleEvent_PushPlayAnim(169u , 0u);
    }
}

static void Effect_Explode(void) {
    if (hWhoseTurn == 0) {
        wBattleMon.hp = 0;
        wBattleMon.status = 0;
        RES_BIT(wPlayerBattleStatus2, BSTAT2_SEEDED);
    } else {
        wEnemyMon.hp = 0;
        wEnemyMon.status = 0;
        RES_BIT(wEnemyBattleStatus2, BSTAT2_SEEDED);
    }
}

static void Effect_DrainHP(void) {

    uint16_t drain = wDamage >> 1;
    if (drain == 0) drain = 1;

    wDamage = drain;

    uint16_t *attacker_hp;
    uint16_t  attacker_max;
    if (hWhoseTurn == 0) {
        attacker_hp  = &wBattleMon.hp;
        attacker_max =  wBattleMon.max_hp;
    } else {
        attacker_hp  = &wEnemyMon.hp;
        attacker_max =  wEnemyMon.max_hp;
    }
    uint32_t new_hp = (uint32_t)(*attacker_hp) + drain;
    *attacker_hp = (new_hp > attacker_max) ? attacker_max : (uint16_t)new_hp;

    BattleEvent_PushHPTarget((uint8_t)((hWhoseTurn == 0) ? 1u : 0u));
}

static void Effect_StatModifierUp(void);
void Battle_StatModifierUpEffect(void) { Effect_StatModifierUp(); }

static void Effect_StatModifierUp(void) {
    uint8_t effect   = (hWhoseTurn == 0) ? wPlayerMoveEffect : wEnemyMoveEffect;
    uint8_t move_num = (hWhoseTurn == 0) ? wPlayerMoveNum    : wEnemyMoveNum;
    uint8_t *stat_mods = (hWhoseTurn == 0) ? wPlayerMonStatMods : wEnemyMonStatMods;

    uint8_t a = effect - EFFECT_ATTACK_UP1;
    if (a >= 8u) a -= (uint8_t)(EFFECT_ATTACK_UP2 - EFFECT_ATTACK_UP1);
    uint8_t idx = a;

    uint8_t new_mod = stat_mods[idx] + 1;
    if (new_mod > STAT_STAGE_MAX) {

        return;
    }

    int is_plus2 = (effect >= EFFECT_ATTACK_UP2);
    if (is_plus2) {
        new_mod++;
        if (new_mod > STAT_STAGE_MAX) new_mod = STAT_STAGE_MAX;
    }
    stat_mods[idx] = new_mod;

    if (idx < 4u) {
        uint16_t cur_stat = 0u;
        if (hWhoseTurn == 0) {
            switch (idx) {
                case 0u: cur_stat = wBattleMon.atk; break;
                case 1u: cur_stat = wBattleMon.def; break;
                case 2u: cur_stat = wBattleMon.spd; break;
                default: cur_stat = wBattleMon.spc; break;
            }
        } else {
            switch (idx) {
                case 0u: cur_stat = wEnemyMon.atk; break;
                case 1u: cur_stat = wEnemyMon.def; break;
                case 2u: cur_stat = wEnemyMon.spd; break;
                default: cur_stat = wEnemyMon.spc; break;
            }
        }
        if (cur_stat >= MAX_STAT_VALUE && stat_mods[idx] > STAT_STAGE_MIN) {
            stat_mods[idx]--;
            return;
        }
    }

    if (idx < 4) {
        wCalculateWhoseStats = hWhoseTurn ? 1 : 0;
        Battle_RecalculateStat(idx);
    }

    if (move_num == MOVE_MINIMIZE) {
        if (hWhoseTurn == 0) wPlayerMonMinimized = 1;
        else                 wEnemyMonMinimized  = 1;
    }

    if (hWhoseTurn == 0) {
        Battle_ApplyBadgeStatBoosts();
    }

    BattleEvent_PushStatModText(idx, hWhoseTurn ? 1u : 0u, 0u, (uint8_t)is_plus2);

    Battle_QuarterSpeedDueToParalysis();
    Battle_HalveAttackDueToBurn();
}

static int stat_is_already_at_one(uint8_t idx) {
    const battle_mon_t *t = (hWhoseTurn == 0) ? &wEnemyMon : &wBattleMon;
    uint16_t v;
    switch (idx) {
        case 0:  v = t->atk; break;
        case 1:  v = t->def; break;
        case 2:  v = t->spd; break;
        case 3:  v = t->spc; break;
        default: return 0;
    }
    return v == 1u;
}

static void Effect_StatModifierDown(void) {
    uint8_t effect  = (hWhoseTurn == 0) ? wPlayerMoveEffect : wEnemyMoveEffect;
    uint8_t *target_mods;
    uint8_t  target_bstat1_val;

    target_mods       = wEnemyMonStatMods;
    target_bstat1_val = wEnemyBattleStatus1;
    if (hWhoseTurn != 0) {
        target_mods       = wPlayerMonStatMods;
        target_bstat1_val = wPlayerBattleStatus1;
        if (wLinkState != LINK_STATE_BATTLING) {

            if (BattleRandom() < PCT25) {

                if (effect < EFFECT_ATTACK_DOWN_SIDE)
                    BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
                return;
            }
        }
    }

    if (CheckTargetSubstitute()) {

        if (effect < EFFECT_ATTACK_DOWN_SIDE)
            BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
        return;
    }

    if (effect >= EFFECT_ATTACK_DOWN_SIDE) {
        if (BattleRandom() >= PCT33) return;
        uint8_t idx = effect - EFFECT_ATTACK_DOWN_SIDE;

        if (target_mods[idx] > STAT_STAGE_MIN) {
            target_mods[idx]--;
            if (stat_is_already_at_one(idx)) {
                target_mods[idx]++;
                return;
            }
            wCalculateWhoseStats = (hWhoseTurn == 0) ? 1u : 0u;
            Battle_RecalculateStat(idx);

            if (hWhoseTurn != 0) {
                Battle_ApplyBadgeStatBoosts();
            }
            BattleEvent_PushStatModText(idx, (hWhoseTurn == 0) ? 1u : 0u, 1u, 0u);
            Battle_QuarterSpeedDueToParalysis();
            Battle_HalveAttackDueToBurn();
        }
        return;
    }

    Battle_MoveHitTest();
    if (wMoveMissed) {
        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
        return;
    }

    if (TST_BIT(target_bstat1_val, BSTAT1_INVULNERABLE)) {
        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
        return;
    }

    uint8_t a = effect - EFFECT_ATTACK_DOWN1;
    if (a >= 8u) a -= (uint8_t)(EFFECT_ATTACK_DOWN2 - EFFECT_ATTACK_DOWN1);
    uint8_t idx = a;

    int is_minus2 = (effect >= (uint8_t)(EFFECT_ATTACK_DOWN2 - 0x16u) &&
                     effect <  EFFECT_ATTACK_DOWN_SIDE);

    if (target_mods[idx] <= STAT_STAGE_MIN) return;
    target_mods[idx]--;

    if (is_minus2 && target_mods[idx] > STAT_STAGE_MIN) {
        target_mods[idx]--;
    }

    if (stat_is_already_at_one(idx)) {
        target_mods[idx]++;
        return;
    }

    if (idx < 4) {
        wCalculateWhoseStats = (hWhoseTurn == 0) ? 1u : 0u;
        Battle_RecalculateStat(idx);
    }

    if (hWhoseTurn != 0) {
        Battle_ApplyBadgeStatBoosts();
    }

    BattleEvent_PushStatModText(idx, (hWhoseTurn == 0) ? 1u : 0u, 1u, (uint8_t)is_minus2);

    Battle_QuarterSpeedDueToParalysis();
    Battle_HalveAttackDueToBurn();
}

static void Effect_Bide(void) {
    uint8_t  *bstat1;
    uint16_t *bide_dmg;
    uint8_t  *ctr;
    if (hWhoseTurn == 0) {
        bstat1   = &wPlayerBattleStatus1;
        bide_dmg = &wPlayerBideAccumulatedDamage;
        ctr      = &wPlayerNumAttacksLeft;
    } else {
        bstat1   = &wEnemyBattleStatus1;
        bide_dmg = &wEnemyBideAccumulatedDamage;
        ctr      = &wEnemyNumAttacksLeft;
    }
    SET_BIT(*bstat1, BSTAT1_STORING_ENERGY);
    *bide_dmg = 0;
    wPlayerMoveEffect = 0;
    wEnemyMoveEffect  = 0;

    *ctr = (BattleRandom() & 1u) + 2u;

    BattleEvent_PushPlayAnim((uint8_t)(174u  + (hWhoseTurn ? 1u : 0u)),
                             (uint8_t)(hWhoseTurn ? 1u : 0u));
}

static void Effect_ThrashPetalDance(void) {
    uint8_t *bstat1;
    uint8_t *ctr;
    if (hWhoseTurn == 0) {
        bstat1 = &wPlayerBattleStatus1;
        ctr    = &wPlayerNumAttacksLeft;
    } else {
        bstat1 = &wEnemyBattleStatus1;
        ctr    = &wEnemyNumAttacksLeft;
    }
    SET_BIT(*bstat1, BSTAT1_THRASHING_ABOUT);
    *ctr = (BattleRandom() & 1u) + 2u;

    BattleEvent_PushPlayAnim((uint8_t)(176u  + (hWhoseTurn ? 1u : 0u)),
                             (uint8_t)(hWhoseTurn ? 1u : 0u));
    BattleEvent_PushPlayAnim((hWhoseTurn == 0) ? wPlayerMoveNum : wEnemyMoveNum,
                             (uint8_t)(hWhoseTurn ? 1u : 0u));
}

static void Effect_SwitchAndTeleport(void) {
    if (wIsInBattle != 1) {

        uint8_t move = (hWhoseTurn == 0) ? wPlayerMoveNum : wEnemyMoveNum;
        BattleEvent_PushMoveResult(move == MOVE_TELEPORT
            ? BATTLE_MOVE_RESULT_BUT_IT_FAILED
            : BATTLE_MOVE_RESULT_UNAFFECTED);
        return;
    }
    uint8_t player_level = wBattleMon.level;

    uint8_t enemy_level  = wCurEnemyLevel;

    if (hWhoseTurn == 0) {

        if (player_level >= enemy_level) {
            wEscapedFromBattle = 1;
            BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_RAN_FROM_BATTLE, 0u, 0u);
            return;
        }
        uint8_t range = player_level + enemy_level + 1;
        uint8_t r;
        do { r = BattleRandom(); } while (r >= range);
        if (r >= (enemy_level >> 2)) {
            wEscapedFromBattle = 1;
            BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_RAN_FROM_BATTLE, 0u, 0u);
        } else {

            BattleEvent_PushMoveResult(wPlayerMoveNum == MOVE_TELEPORT
                ? BATTLE_MOVE_RESULT_BUT_IT_FAILED
                : BATTLE_MOVE_RESULT_DIDNT_AFFECT);
        }
    } else {

        if (enemy_level >= player_level) {
            wEscapedFromBattle = 1;
            BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_RAN_FROM_BATTLE, 1u, 0u);
            return;
        }
        uint8_t range = enemy_level + player_level + 1;
        uint8_t r;
        do { r = BattleRandom(); } while (r >= range);
        if (r >= (player_level >> 2)) {
            wEscapedFromBattle = 1;
            BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_RAN_FROM_BATTLE, 1u, 0u);
        } else {

            BattleEvent_PushMoveResult(wEnemyMoveNum == MOVE_TELEPORT
                ? BATTLE_MOVE_RESULT_BUT_IT_FAILED
                : BATTLE_MOVE_RESULT_DIDNT_AFFECT);
        }
    }
}

static void Effect_TwoToFiveAttacks(void) {
    uint8_t *bstat1;
    uint8_t *ctr;
    uint8_t *num_hits;
    uint8_t *eff_ptr;
    if (hWhoseTurn == 0) {
        bstat1   = &wPlayerBattleStatus1;
        ctr      = &wPlayerNumAttacksLeft;
        num_hits = &wPlayerNumHits;
        eff_ptr  = &wPlayerMoveEffect;
    } else {
        bstat1   = &wEnemyBattleStatus1;
        ctr      = &wEnemyNumAttacksLeft;
        num_hits = &wEnemyNumHits;
        eff_ptr  = &wEnemyMoveEffect;
    }

    if (TST_BIT(*bstat1, BSTAT1_ATTACKING_MULTIPLE)) return;
    SET_BIT(*bstat1, BSTAT1_ATTACKING_MULTIPLE);

    uint8_t effect = *eff_ptr;
    uint8_t n;

    if (effect == EFFECT_TWINEEDLE) {
        *eff_ptr = EFFECT_POISON_SIDE1;
        n = 2;
    } else if (effect == EFFECT_ATTACK_TWICE) {
        n = 2;
    } else {

        uint8_t r = BattleRandom() & 3u;
        if (r >= 2) r = BattleRandom() & 3u;
        n = r + 2u;
    }
    *ctr      = n;
    *num_hits = n;
}

static void Effect_FlinchSide(void) {
    if (CheckTargetSubstitute()) return;

    uint8_t effect = (hWhoseTurn == 0) ? wPlayerMoveEffect : wEnemyMoveEffect;
    uint8_t threshold = (effect == EFFECT_FLINCH_SIDE1) ? PCT10 : PCT30;
    if (BattleRandom() >= threshold) return;

    uint8_t *target_bstat1 = (hWhoseTurn == 0) ? &wEnemyBattleStatus1
                                                 : &wPlayerBattleStatus1;
    SET_BIT(*target_bstat1, BSTAT1_FLINCHED);
    ClearHyperBeam();
}

static void Effect_OHKO(void) {
    wDamage           = 0;
    wCriticalHitOrOHKO = 0xFF;
    uint16_t atk_spd, def_spd;
    if (hWhoseTurn == 0) {
        atk_spd = wBattleMon.spd;
        def_spd = wEnemyMon.spd;
    } else {
        atk_spd = wEnemyMon.spd;
        def_spd = wBattleMon.spd;
    }

    if (atk_spd >= def_spd) {
        wDamage            = 0xFFFF;
        wCriticalHitOrOHKO = 2;
    } else {
        wMoveMissed = 1;
    }
}

static void Effect_Charge(void) {
    uint8_t effect   = (hWhoseTurn == 0) ? wPlayerMoveEffect : wEnemyMoveEffect;
    uint8_t move_num = (hWhoseTurn == 0) ? wPlayerMoveNum    : wEnemyMoveNum;
    uint8_t *bstat1  = (hWhoseTurn == 0) ? &wPlayerBattleStatus1
                                          : &wEnemyBattleStatus1;
    SET_BIT(*bstat1, BSTAT1_CHARGING_UP);

    if (effect == EFFECT_FLY || move_num == MOVE_DIG) {
        SET_BIT(*bstat1, BSTAT1_INVULNERABLE);
    }
    wChargeMoveNum = move_num;
}

static void Effect_Trapping(void) {
    uint8_t *bstat1;
    uint8_t *ctr;
    if (hWhoseTurn == 0) {
        bstat1 = &wPlayerBattleStatus1;
        ctr    = &wPlayerNumAttacksLeft;
    } else {
        bstat1 = &wEnemyBattleStatus1;
        ctr    = &wEnemyNumAttacksLeft;
    }
    if (TST_BIT(*bstat1, BSTAT1_USING_TRAPPING)) return;

    ClearHyperBeam();
    SET_BIT(*bstat1, BSTAT1_USING_TRAPPING);

    uint8_t r = BattleRandom() & 3u;
    if (r >= 2) r = BattleRandom() & 3u;

    *ctr = (uint8_t)(r + 1u);
}

static int EffectCore_ApplyConfusion(void) {

    uint8_t *target_bstat1;
    uint8_t *target_ctr;
    if (hWhoseTurn == 0) {
        target_bstat1 = &wEnemyBattleStatus1;
        target_ctr    = &wEnemyConfusedCounter;
    } else {
        target_bstat1 = &wPlayerBattleStatus1;
        target_ctr    = &wPlayerConfusedCounter;
    }
    if (TST_BIT(*target_bstat1, BSTAT1_CONFUSED)) return 0;
    SET_BIT(*target_bstat1, BSTAT1_CONFUSED);
    *target_ctr = (BattleRandom() & 3u) + 2u;
    return 1;
}

static void Effect_ConfusionSide(void) {

    if (BattleRandom() >= PCT10) return;
    (void)EffectCore_ApplyConfusion();
}

static void Effect_Confusion(void) {

    if (CheckTargetSubstitute()) {
        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
        return;
    }
    Battle_MoveHitTest();
    if (wMoveMissed) {
        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
        return;
    }
    if (!EffectCore_ApplyConfusion())
        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
}

static void Effect_HyperBeam(void) {
    uint8_t *bstat2 = (hWhoseTurn == 0) ? &wPlayerBattleStatus2
                                         : &wEnemyBattleStatus2;
    SET_BIT(*bstat2, BSTAT2_NEEDS_TO_RECHARGE);
}

static void Effect_Rage(void) {
    uint8_t *bstat2 = (hWhoseTurn == 0) ? &wPlayerBattleStatus2
                                         : &wEnemyBattleStatus2;
    SET_BIT(*bstat2, BSTAT2_USING_RAGE);
}

static void Effect_Mimic(void) {

    Battle_MoveHitTest();
    if (wMoveMissed) {
        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
        return;
    }

    uint8_t *src_moves;
    uint8_t *dst_moves;
    uint8_t  dst_slot;
    uint8_t  attacker_bstat1;
    uint8_t  target_bstat1;
    if (hWhoseTurn == 0) {
        attacker_bstat1 = wPlayerBattleStatus1;
        target_bstat1   = wEnemyBattleStatus1;
        src_moves = wEnemyMon.moves;
        dst_moves = wBattleMon.moves;
        dst_slot  = wPlayerMoveListIndex;
    } else {
        attacker_bstat1 = wEnemyBattleStatus1;
        target_bstat1   = wPlayerBattleStatus1;
        src_moves = wBattleMon.moves;
        dst_moves = wEnemyMon.moves;
        dst_slot  = wEnemyMoveListIndex;
    }

    if (TST_BIT(target_bstat1, BSTAT1_INVULNERABLE)) { wMoveMissed = 1; return; }
    (void)attacker_bstat1;

    uint8_t picked;
    if (hWhoseTurn == 0) {
        picked = src_moves[wPlayerMimicChoice & 3u];
        if (picked == 0) return;
    } else {
        do {
            uint8_t r = BattleRandom() & 3u;
            picked = src_moves[r];
        } while (picked == 0);
    }

    if (dst_slot < 4) {
        dst_moves[dst_slot] = picked;
        BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_LEARNED_MOVE, (uint8_t)(hWhoseTurn == 0 ? 0u : 1u), picked);
    }
}

static void Effect_Splash(void) {

    BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_NO_EFFECT_PLAIN);
}

static void Effect_Disable(void) {

    Battle_MoveHitTest();
    if (wMoveMissed) {
        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
        return;
    }

    uint8_t *target_disabled;
    uint8_t *target_moves;
    uint8_t *target_pp;
    uint8_t *target_disnum;
    if (hWhoseTurn == 0) {
        target_disabled = &wEnemyDisabledMove;
        target_moves    = wEnemyMon.moves;
        target_pp       = wEnemyMon.pp;
        target_disnum   = &wEnemyDisabledMoveNumber;
    } else {
        target_disabled = &wPlayerDisabledMove;
        target_moves    = wBattleMon.moves;
        target_pp       = wBattleMon.pp;
        target_disnum   = &wPlayerDisabledMoveNumber;
    }

    if (*target_disabled != 0) {
        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
        return;
    }

    uint8_t slot;
    for (;;) {
        slot = BattleRandom() & 3u;
        if (target_moves[slot] == 0) continue;

        if (hWhoseTurn == 0 && wLinkState != LINK_STATE_BATTLING) {
            break;
        }

        uint8_t pp = target_pp[slot] & PP_MASK;
        if (pp == 0) {

            uint8_t any_pp = (target_pp[0] | target_pp[1] | target_pp[2] | target_pp[3]) & PP_MASK;
            if (!any_pp) {
                BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
                return;
            }
            continue;
        }
        break;
    }

    uint8_t turns = (BattleRandom() & 7u) + 1u;
    *target_disabled = (uint8_t)(((slot + 1u) << 4) | turns);
    *target_disnum   = target_moves[slot];

    BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_MOVE_DISABLED, (uint8_t)(hWhoseTurn == 0 ? 1u : 0u),
                              target_moves[slot]);
}

static void Effect_PayDay(void) {
    uint8_t level = (hWhoseTurn == 0) ? wBattleMon.level : wEnemyMon.level;
    uint8_t val   = level * 2u;

    uint8_t pay[3];
    pay[0] = val / 100u;
    pay[1] = (uint8_t)(((val % 100u) / 10u) << 4) |
             (uint8_t)(val % 10u);
    pay[2] = 0;

    uint8_t bcd_src[3];
    bcd_src[0] = 0;
    bcd_src[1] = pay[0];
    bcd_src[2] = pay[1];
    AddBCD3(wTotalPayDayMoney, bcd_src);
    BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_COINS_SCATTERED, (uint8_t)(hWhoseTurn == 0 ? 0u : 1u), 0u);
}

static void Effect_Mist(void) {
    uint8_t *bstat2 = (hWhoseTurn == 0) ? &wPlayerBattleStatus2
                                         : &wEnemyBattleStatus2;
    if (TST_BIT(*bstat2, BSTAT2_PROTECTED_BY_MIST)) {

        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
        return;
    }
    SET_BIT(*bstat2, BSTAT2_PROTECTED_BY_MIST);
}

static void Effect_FocusEnergy(void) {
    uint8_t *bstat2 = (hWhoseTurn == 0) ? &wPlayerBattleStatus2
                                         : &wEnemyBattleStatus2;
    if (TST_BIT(*bstat2, BSTAT2_GETTING_PUMPED)) {

        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
        return;
    }
    SET_BIT(*bstat2, BSTAT2_GETTING_PUMPED);
}

static void Effect_Recoil(void) {
    uint8_t move_num = (hWhoseTurn == 0) ? wPlayerMoveNum : wEnemyMoveNum;
    uint16_t recoil  = (move_num == MOVE_STRUGGLE) ? (wDamage >> 1)
                                                    : (wDamage >> 2);
    if (recoil == 0) recoil = 1;

    uint16_t *hp = (hWhoseTurn == 0) ? &wBattleMon.hp : &wEnemyMon.hp;
    *hp = (*hp > recoil) ? (uint16_t)(*hp - recoil) : 0u;

    BattleEvent_PushHPTarget((uint8_t)(hWhoseTurn == 0 ? 1u : 0u));
    BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_HIT_WITH_RECOIL, (uint8_t)(hWhoseTurn == 0 ? 0u : 1u), 0u);
}

static void Effect_Conversion(void) {
    uint8_t target_bstat1;
    uint8_t *user_type1, *user_type2;
    uint8_t src_type1,    src_type2;
    if (hWhoseTurn == 0) {
        target_bstat1 = wEnemyBattleStatus1;
        user_type1    = &wBattleMon.type1;
        user_type2    = &wBattleMon.type2;
        src_type1     = wEnemyMon.type1;
        src_type2     = wEnemyMon.type2;
    } else {
        target_bstat1 = wPlayerBattleStatus1;
        user_type1    = &wEnemyMon.type1;
        user_type2    = &wEnemyMon.type2;
        src_type1     = wBattleMon.type1;
        src_type2     = wBattleMon.type2;
    }
    if (TST_BIT(target_bstat1, BSTAT1_INVULNERABLE)) {

        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
        return;
    }
    *user_type1 = src_type1;
    *user_type2 = src_type2;
    BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_CONVERTED_TYPE, (uint8_t)(hWhoseTurn == 0 ? 1u : 0u), 0u);
}

static void Effect_Haze(void) {

    for (int i = 0; i < NUM_STAT_MODS; i++) {
        wPlayerMonStatMods[i] = STAT_STAGE_NORMAL;
        wEnemyMonStatMods[i]  = STAT_STAGE_NORMAL;
    }

    wBattleMon.atk = wPlayerMonUnmodifiedAttack;
    wBattleMon.def = wPlayerMonUnmodifiedDefense;
    wBattleMon.spd = wPlayerMonUnmodifiedSpeed;
    wBattleMon.spc = wPlayerMonUnmodifiedSpecial;
    wEnemyMon.atk  = wEnemyMonUnmodifiedAttack;
    wEnemyMon.def  = wEnemyMonUnmodifiedDefense;
    wEnemyMon.spd  = wEnemyMonUnmodifiedSpeed;
    wEnemyMon.spc  = wEnemyMonUnmodifiedSpecial;

    uint8_t *target_status;
    uint8_t *target_selected;
    if (hWhoseTurn == 0) {

        target_status   = &wEnemyMon.status;
        target_selected = &wEnemySelectedMove;
    } else {
        target_status   = &wBattleMon.status;
        target_selected = &wPlayerSelectedMove;
    }
    uint8_t old_status = *target_status;
    *target_status = 0;
    if (old_status & (STATUS_FRZ | STATUS_SLP_MASK)) {
        *target_selected = 0xFF;
    }

    RES_BIT(wPlayerBattleStatus1, BSTAT1_CONFUSED);
    wPlayerBattleStatus2 &= ~((1u << BSTAT2_USING_X_ACCURACY) |
                               (1u << BSTAT2_PROTECTED_BY_MIST) |
                               (1u << BSTAT2_GETTING_PUMPED) |
                               (1u << BSTAT2_SEEDED));
    wPlayerBattleStatus3 &= ~((1u << BSTAT3_BADLY_POISONED) |
                               (1u << BSTAT3_HAS_LIGHT_SCREEN) |
                               (1u << BSTAT3_HAS_REFLECT));
    wPlayerDisabledMove        = 0;
    wPlayerDisabledMoveNumber  = 0;

    RES_BIT(wEnemyBattleStatus1, BSTAT1_CONFUSED);
    wEnemyBattleStatus2 &= ~((1u << BSTAT2_USING_X_ACCURACY) |
                              (1u << BSTAT2_PROTECTED_BY_MIST) |
                              (1u << BSTAT2_GETTING_PUMPED) |
                              (1u << BSTAT2_SEEDED));
    wEnemyBattleStatus3 &= ~((1u << BSTAT3_BADLY_POISONED) |
                              (1u << BSTAT3_HAS_LIGHT_SCREEN) |
                              (1u << BSTAT3_HAS_REFLECT));
    wEnemyDisabledMove        = 0;
    wEnemyDisabledMoveNumber  = 0;
    BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_HAZE, (uint8_t)(hWhoseTurn == 0 ? 0u : 1u), 0u);
}

static void Effect_Heal(void) {
    uint8_t move_num = (hWhoseTurn == 0) ? wPlayerMoveNum : wEnemyMoveNum;
    uint16_t *hp;
    uint16_t  max_hp;
    if (hWhoseTurn == 0) {
        hp     = &wBattleMon.hp;
        max_hp =  wBattleMon.max_hp;
    } else {
        hp     = &wEnemyMon.hp;
        max_hp =  wEnemyMon.max_hp;
    }

    {
        uint8_t hp_hi   = (uint8_t)(*hp >> 8);
        uint8_t hp_lo   = (uint8_t)(*hp & 0xFF);
        uint8_t max_hi  = (uint8_t)(max_hp >> 8);
        uint8_t max_lo  = (uint8_t)(max_hp & 0xFF);
        int borrow = (hp_hi < max_hi) ? 1 : 0;
        int diff   = (int)hp_lo - (int)max_lo - borrow;
        if (diff == 0) {

            BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
            return;
        }
    }

    uint8_t user_side = (uint8_t)((hWhoseTurn == 0) ? 0u : 1u);

    if (move_num == MOVE_REST) {

        uint8_t *status = (hWhoseTurn == 0) ? &wBattleMon.status : &wEnemyMon.status;
        uint8_t had_status = *status;
        *status = 2;
        BattleEvent_PushEffectMsg(had_status ? BATTLE_EFFECT_MSG_FELL_ASLEEP_HEALTHY
                                             : BATTLE_EFFECT_MSG_STARTED_SLEEPING,
                                  user_side, 0u);
        *hp = max_hp;

        BattleEvent_PushHPTarget((uint8_t)((hWhoseTurn == 0) ? 1u : 0u));
        BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_REGAINED_HEALTH, user_side, 0u);
        return;
    }

    uint16_t heal_amt = max_hp >> 1;

    uint32_t new_hp = (uint32_t)(*hp) + heal_amt;
    *hp = (new_hp > max_hp) ? max_hp : (uint16_t)new_hp;
    BattleEvent_PushHPTarget((uint8_t)((hWhoseTurn == 0) ? 1u : 0u));
    BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_REGAINED_HEALTH, user_side, 0u);
}

static void Effect_Transform(void) {

    battle_mon_t *attacker;
    battle_mon_t *target;
    uint8_t      *attacker_bstat3;
    uint16_t     *attacker_unmod_atk;
    uint16_t     *target_unmod_atk;
    uint8_t      *attacker_stat_mods;
    uint8_t      *target_stat_mods;

    if (hWhoseTurn == 0) {
        attacker           = &wBattleMon;
        target             = &wEnemyMon;
        attacker_bstat3    = &wPlayerBattleStatus3;
        attacker_unmod_atk = &wPlayerMonUnmodifiedAttack;
        target_unmod_atk   = &wEnemyMonUnmodifiedAttack;
        attacker_stat_mods = wPlayerMonStatMods;
        target_stat_mods   = wEnemyMonStatMods;
    } else {
        attacker           = &wEnemyMon;
        target             = &wBattleMon;
        attacker_bstat3    = &wEnemyBattleStatus3;
        attacker_unmod_atk = &wEnemyMonUnmodifiedAttack;
        target_unmod_atk   = &wPlayerMonUnmodifiedAttack;
        attacker_stat_mods = wEnemyMonStatMods;
        target_stat_mods   = wPlayerMonStatMods;
    }

    attacker->species   = target->species;

    attacker->type1     = target->type1;
    attacker->type2     = target->type2;
    attacker->catch_rate = target->catch_rate;
    for (int i = 0; i < 4; i++) attacker->moves[i] = target->moves[i];

    if (hWhoseTurn != 0) {

        wTransformedEnemyMonOriginalDVs = wEnemyMon.dvs;
    }
    attacker->dvs = target->dvs;

    attacker->atk = target->atk;
    attacker->def = target->def;
    attacker->spd = target->spd;
    attacker->spc = target->spc;

    for (int i = 0; i < 4; i++)
        attacker->pp[i] = attacker->moves[i] ? 5 : 0;

    attacker_unmod_atk[0] = target_unmod_atk[0];
    attacker_unmod_atk[1] = target_unmod_atk[1];
    attacker_unmod_atk[2] = target_unmod_atk[2];
    attacker_unmod_atk[3] = target_unmod_atk[3];
    for (int i = 0; i < NUM_STAT_MODS; i++)
        attacker_stat_mods[i] = target_stat_mods[i];

    SET_BIT(*attacker_bstat3, BSTAT3_TRANSFORMED);
    BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_TRANSFORMED,
                              (uint8_t)(hWhoseTurn == 0 ? 0u : 1u), 0u);
}

static void Effect_ReflectLightScreen(void) {
    uint8_t effect  = (hWhoseTurn == 0) ? wPlayerMoveEffect : wEnemyMoveEffect;
    uint8_t *bstat3 = (hWhoseTurn == 0) ? &wPlayerBattleStatus3
                                         : &wEnemyBattleStatus3;
    uint8_t bit = (effect == EFFECT_REFLECT) ? BSTAT3_HAS_REFLECT
                                             : BSTAT3_HAS_LIGHT_SCREEN;
    if (TST_BIT(*bstat3, bit)) {
        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_BUT_IT_FAILED);
        return;
    }
    SET_BIT(*bstat3, bit);
    BattleEvent_PushEffectMsg((effect == EFFECT_REFLECT)
                                  ? BATTLE_EFFECT_MSG_GAINED_ARMOR
                                  : BATTLE_EFFECT_MSG_LIGHT_SCREEN,
                              (uint8_t)(hWhoseTurn == 0 ? 0u : 1u), 0u);
}

static void Effect_Paralyze(void) {
    uint8_t mv_type = (hWhoseTurn == 0) ? wPlayerMoveType : wEnemyMoveType;
    uint8_t *target_status, *target_type1, *target_type2;
    const char *target_side = (hWhoseTurn == 0) ? "enemy" : "player";
    if (hWhoseTurn == 0) {
        target_status = &wEnemyMon.status;
        target_type1  = &wEnemyMon.type1;
        target_type2  = &wEnemyMon.type2;
    } else {
        target_status = &wBattleMon.status;
        target_type1  = &wBattleMon.type1;
        target_type2  = &wBattleMon.type2;
    }

    if (*target_status != 0) {
        BLOG("paralyze-blocked target=%s already statused 0x%02X(%s)",
             target_side, *target_status, bfx_status_name(*target_status));
        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_DIDNT_AFFECT);
        return;
    }

    if (mv_type == TYPE_ELECTRIC &&
        (*target_type1 == TYPE_GROUND || *target_type2 == TYPE_GROUND)) {
        BLOG("paralyze-blocked target=%s ground-immune move_type=%u target_types=%u/%u",
             target_side, (unsigned)mv_type, (unsigned)*target_type1, (unsigned)*target_type2);

        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_NO_EFFECT);
        return;
    }

    Battle_MoveHitTest();
    if (wMoveMissed) {
        BLOG("paralyze-miss target=%s move_type=%u", target_side, (unsigned)mv_type);

        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_DIDNT_AFFECT);
        return;
    }

    *target_status = STATUS_PAR;

    if (hWhoseTurn == 0) wEnemyMon.spd = QuarterSpeed16(wEnemyMon.spd);
    else                 wBattleMon.spd = QuarterSpeed16(wBattleMon.spd);
    BLOG("paralyze-applied target=%s status=0x%02X(%s)", target_side, *target_status, bfx_status_name(*target_status));
}

static void Effect_Substitute(void) {
    uint16_t *hp;
    uint16_t  max_hp;
    uint8_t  *bstat2;
    uint8_t  *sub_hp_var;
    if (hWhoseTurn == 0) {
        hp         = &wBattleMon.hp;
        max_hp     =  wBattleMon.max_hp;
        bstat2     = &wPlayerBattleStatus2;
        sub_hp_var = &wPlayerSubstituteHP;
    } else {
        hp         = &wEnemyMon.hp;
        max_hp     =  wEnemyMon.max_hp;
        bstat2     = &wEnemyBattleStatus2;
        sub_hp_var = &wEnemySubstituteHP;
    }

    if (TST_BIT(*bstat2, BSTAT2_HAS_SUBSTITUTE)) {
        BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_HAS_SUBSTITUTE,
                                  (uint8_t)(hWhoseTurn == 0 ? 0u : 1u), 0u);
        return;
    }

    uint8_t sub_cost = (uint8_t)(max_hp >> 2);

    *sub_hp_var = sub_cost;

    uint8_t hp_lo = (uint8_t)(*hp & 0xFF);
    uint8_t hp_hi = (uint8_t)(*hp >> 8);
    if (sub_cost > hp_lo && hp_hi == 0) {
        BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_SUBSTITUTE_TOO_WEAK, (uint8_t)(hWhoseTurn == 0 ? 0u : 1u), 0u);
        return;
    }
    if (sub_cost > hp_lo && hp_hi > 0) {
        hp_hi--;
        hp_lo = (uint8_t)(256u - (sub_cost - hp_lo));
    } else {
        hp_lo -= sub_cost;
    }
    *hp = ((uint16_t)hp_hi << 8) | hp_lo;

    SET_BIT(*bstat2, BSTAT2_HAS_SUBSTITUTE);
    BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_SUBSTITUTE_MADE, (uint8_t)(hWhoseTurn == 0 ? 0u : 1u), 0u);
}

static void Effect_LeechSeed(void) {

    Battle_MoveHitTest();
    if (wMoveMissed) {
        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_EVADED);
        return;
    }

    uint8_t *target_type1, *target_type2, *target_bstat2;
    if (hWhoseTurn == 0) {
        target_type1  = &wEnemyMon.type1;
        target_type2  = &wEnemyMon.type2;
        target_bstat2 = &wEnemyBattleStatus2;
    } else {
        target_type1  = &wBattleMon.type1;
        target_type2  = &wBattleMon.type2;
        target_bstat2 = &wPlayerBattleStatus2;
    }

    if (*target_type1 == TYPE_GRASS || *target_type2 == TYPE_GRASS) {
        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_EVADED);
        return;
    }

    if (TST_BIT(*target_bstat2, BSTAT2_SEEDED)) {
        BattleEvent_PushMoveResult(BATTLE_MOVE_RESULT_EVADED);
        return;
    }

    SET_BIT(*target_bstat2, BSTAT2_SEEDED);
}

void Battle_JumpMoveEffect(void) {
    BPROBE("JumpMoveEffect");
    uint8_t effect = (hWhoseTurn == 0) ? wPlayerMoveEffect : wEnemyMoveEffect;
    BLOG("JumpMoveEffect turn=%s move=%s(0x%02X) effect=0x%02X pre player=%s enemy=%s",
         hWhoseTurn == 0 ? "player" : "enemy",
         BMOVE(hWhoseTurn == 0 ? wPlayerSelectedMove : wEnemySelectedMove),
         (unsigned)(hWhoseTurn == 0 ? wPlayerSelectedMove : wEnemySelectedMove),
         (unsigned)effect,
         bfx_status_name(wBattleMon.status),
         bfx_status_name(wEnemyMon.status));

    switch (effect) {

    case 0x01:
    case EFFECT_SLEEP:          Effect_Sleep();            break;

    case EFFECT_POISON_SIDE1:
    case EFFECT_POISON_SIDE2:
    case EFFECT_POISON:         Effect_Poison();           break;

    case EFFECT_DRAIN_HP:
    case EFFECT_DREAM_EATER:    Effect_DrainHP();          break;

    case EFFECT_BURN_SIDE:
    case EFFECT_FREEZE_SIDE:
    case EFFECT_PARALYZE_SIDE:
    case EFFECT_BURN_SIDE2:
    case EFFECT_FREEZE_SIDE2:
    case EFFECT_PARALYZE_SIDE2: Effect_FreezeBurnParalyze(); break;

    case EFFECT_EXPLODE:        Effect_Explode();          break;

    case EFFECT_MIRROR_MOVE:     break;

    case EFFECT_ATTACK_UP1:
    case EFFECT_DEFENSE_UP1:
    case EFFECT_SPEED_UP1:
    case EFFECT_SPECIAL_UP1:
    case EFFECT_ACCURACY_UP1:
    case EFFECT_EVASION_UP1:
    case EFFECT_ATTACK_UP2:
    case EFFECT_DEFENSE_UP2:
    case EFFECT_SPEED_UP2:
    case EFFECT_SPECIAL_UP2:
    case EFFECT_ACCURACY_UP2:
    case EFFECT_EVASION_UP2:    Effect_StatModifierUp();   break;

    case EFFECT_PAY_DAY:        Effect_PayDay();           break;

    case EFFECT_SWIFT:           break;

    case EFFECT_ATTACK_DOWN1:
    case EFFECT_DEFENSE_DOWN1:
    case EFFECT_SPEED_DOWN1:
    case EFFECT_SPECIAL_DOWN1:
    case EFFECT_ACCURACY_DOWN1:
    case EFFECT_EVASION_DOWN1:
    case EFFECT_ATTACK_DOWN2:
    case EFFECT_DEFENSE_DOWN2:
    case EFFECT_SPEED_DOWN2:
    case EFFECT_SPECIAL_DOWN2:
    case EFFECT_ACCURACY_DOWN2:
    case EFFECT_EVASION_DOWN2:
    case EFFECT_ATTACK_DOWN_SIDE:
    case EFFECT_DEFENSE_DOWN_SIDE:
    case EFFECT_SPEED_DOWN_SIDE:
    case EFFECT_SPECIAL_DOWN_SIDE: Effect_StatModifierDown(); break;

    case EFFECT_CONVERSION:     Effect_Conversion();       break;
    case EFFECT_HAZE:           Effect_Haze();             break;
    case EFFECT_BIDE:           Effect_Bide();             break;
    case EFFECT_THRASH:         Effect_ThrashPetalDance(); break;
    case EFFECT_SWITCH_TELEPORT: Effect_SwitchAndTeleport(); break;

    case EFFECT_TWO_TO_FIVE_ATTACKS:
    case EFFECT_1E:
    case EFFECT_ATTACK_TWICE:
    case EFFECT_TWINEEDLE:      Effect_TwoToFiveAttacks(); break;

    case EFFECT_FLINCH_SIDE1:
    case EFFECT_FLINCH_SIDE2:   Effect_FlinchSide();       break;

    case EFFECT_OHKO:           Effect_OHKO();             break;
    case EFFECT_CHARGE:
    case EFFECT_FLY:            Effect_Charge();           break;

    case EFFECT_SUPER_FANG:      break;
    case EFFECT_SPECIAL_DAMAGE:  break;

    case EFFECT_TRAPPING:       Effect_Trapping();         break;
    case EFFECT_JUMP_KICK:       break;

    case EFFECT_MIST:           Effect_Mist();             break;
    case EFFECT_FOCUS_ENERGY:   Effect_FocusEnergy();      break;
    case EFFECT_RECOIL:         Effect_Recoil();           break;
    case EFFECT_CONFUSION:      Effect_Confusion();        break;

    case EFFECT_HEAL:           Effect_Heal();             break;
    case EFFECT_TRANSFORM:      Effect_Transform();        break;

    case EFFECT_LIGHT_SCREEN:
    case EFFECT_REFLECT:        Effect_ReflectLightScreen(); break;

    case EFFECT_PARALYZE:
        BLOG("ParalyzeEffect entry turn=%s target=%s pre status=0x%02X(%s) move_type=%u",
             hWhoseTurn == 0 ? "player" : "enemy",
             hWhoseTurn == 0 ? "enemy" : "player",
             (unsigned)((hWhoseTurn == 0) ? wEnemyMon.status : wBattleMon.status),
             bfx_status_name((hWhoseTurn == 0) ? wEnemyMon.status : wBattleMon.status),
             (unsigned)((hWhoseTurn == 0) ? wPlayerMoveType : wEnemyMoveType));
        Effect_Paralyze();
        BLOG("ParalyzeEffect exit turn=%s player=%s enemy=%s",
             hWhoseTurn == 0 ? "player" : "enemy",
             bfx_status_name(wBattleMon.status),
             bfx_status_name(wEnemyMon.status));
        break;
    case EFFECT_CONFUSION_SIDE: Effect_ConfusionSide();    break;
    case EFFECT_SUBSTITUTE:     Effect_Substitute();       break;
    case EFFECT_HYPER_BEAM:     Effect_HyperBeam();        break;
    case EFFECT_RAGE:           Effect_Rage();             break;
    case EFFECT_MIMIC:          Effect_Mimic();            break;
    case EFFECT_METRONOME:       break;
    case EFFECT_LEECH_SEED:     Effect_LeechSeed();        break;
    case EFFECT_SPLASH:         Effect_Splash();           break;
    case EFFECT_DISABLE:        Effect_Disable();          break;

    default: break;
    }
}
