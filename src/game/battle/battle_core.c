
#include "battle_core.h"

#include "../../data/moves_data.h"
#include "battle.h"
#include "battle_effects.h"
#include "battle_exp.h"
#include "battle_switch.h"
#include "battle_trainer.h"
#include "battle_loop.h"
#include "../../platform/hardware.h"
#include "game/battle/battle_probe.h"
#include "../inventory.h"
#include "../amberscript_mapbank.h"
#include "../../data/map_data.h"
#include <string.h>

#define MOVE_ANIM_ABSORB_ID   0x47u
#define MOVE_ANIM_BURN_PSN_ID 0xBAu

static const uint8_t kResidualEffects1[] = {
    EFFECT_CONVERSION,
    EFFECT_HAZE,
    EFFECT_SWITCH_TELEPORT,
    EFFECT_MIST,
    EFFECT_FOCUS_ENERGY,
    EFFECT_CONFUSION,
    EFFECT_HEAL,
    EFFECT_TRANSFORM,
    EFFECT_LIGHT_SCREEN,
    EFFECT_REFLECT,
    EFFECT_POISON,
    EFFECT_PARALYZE,
    EFFECT_SUBSTITUTE,
    EFFECT_MIMIC,
    EFFECT_LEECH_SEED,
    EFFECT_SPLASH,
    0xFF
};

static const uint8_t kSpecialEffectsCont[] = {
    EFFECT_THRASH,
    EFFECT_TRAPPING,
    0xFF
};

static const uint8_t kSetDamageEffects[] = {
    EFFECT_SUPER_FANG,
    EFFECT_SPECIAL_DAMAGE,
    0xFF
};

static const uint8_t kAlwaysHappenSideEffects[] = {
    EFFECT_DRAIN_HP,
    EFFECT_EXPLODE,
    EFFECT_DREAM_EATER,
    EFFECT_PAY_DAY,
    EFFECT_TWO_TO_FIVE_ATTACKS,
    EFFECT_1E,
    EFFECT_ATTACK_TWICE,
    EFFECT_RECOIL,
    EFFECT_TWINEEDLE,
    EFFECT_RAGE,
    0xFF
};

static const uint8_t kResidualEffects2[] = {
    EFFECT_01,
    EFFECT_ATTACK_UP1,
    EFFECT_DEFENSE_UP1,
    EFFECT_SPEED_UP1,
    EFFECT_SPECIAL_UP1,
    EFFECT_ACCURACY_UP1,
    EFFECT_EVASION_UP1,
    EFFECT_ATTACK_DOWN1,
    EFFECT_DEFENSE_DOWN1,
    EFFECT_SPEED_DOWN1,
    EFFECT_SPECIAL_DOWN1,
    EFFECT_ACCURACY_DOWN1,
    EFFECT_EVASION_DOWN1,
    EFFECT_BIDE,
    EFFECT_SLEEP,
    EFFECT_ATTACK_UP2,
    EFFECT_DEFENSE_UP2,
    EFFECT_SPEED_UP2,
    EFFECT_SPECIAL_UP2,
    EFFECT_ACCURACY_UP2,
    EFFECT_EVASION_UP2,
    EFFECT_ATTACK_DOWN2,
    EFFECT_DEFENSE_DOWN2,
    EFFECT_SPEED_DOWN2,
    EFFECT_SPECIAL_DOWN2,
    EFFECT_ACCURACY_DOWN2,
    EFFECT_EVASION_DOWN2,
    0xFF
};

static const uint8_t kSpecialEffects[] = {
    EFFECT_DRAIN_HP,
    EFFECT_EXPLODE,
    EFFECT_DREAM_EATER,
    EFFECT_PAY_DAY,
    EFFECT_SWIFT,
    EFFECT_TWO_TO_FIVE_ATTACKS,
    EFFECT_1E,
    EFFECT_CHARGE,
    EFFECT_SUPER_FANG,
    EFFECT_SPECIAL_DAMAGE,
    EFFECT_FLY,
    EFFECT_ATTACK_TWICE,
    EFFECT_JUMP_KICK,
    EFFECT_RECOIL,

    EFFECT_THRASH,
    EFFECT_TRAPPING,
    0xFF
};

static void battle_event_push_hit_sfx(uint8_t dmg_mult);
static void battle_event_push_hp_target(uint8_t target_side);
static void battle_event_push_move_result(uint8_t result);
static void battle_event_push_residual_msg(uint8_t side, uint8_t msg);
static uint8_t battle_classify_move_failure_result(void);

#define BATTLE_HIT_HP_LOG_MAX 8
static uint16_t s_hit_hp_log_player[BATTLE_HIT_HP_LOG_MAX];
static uint8_t  s_hit_hp_log_player_n;
static uint16_t s_hit_hp_log_enemy[BATTLE_HIT_HP_LOG_MAX];
static uint8_t  s_hit_hp_log_enemy_n;

static uint16_t s_multihit_dmg_player;
static uint16_t s_multihit_dmg_enemy;

void (*gCombatLogSink)(const char *line) = NULL;

static int is_in_array(uint8_t val, const uint8_t *arr) {
    for (; *arr != 0xFF; arr++) {
        if (*arr == val) return 1;
    }
    return 0;
}

static void swap_player_enemy_levels(void) {
    uint8_t tmp = wBattleMon.level;
    wBattleMon.level = wEnemyMon.level;
    wEnemyMon.level = tmp;
}

static void attack_substitute(void) {
    uint8_t  *sub_hp;
    uint8_t  *bstat2;
    uint8_t  *atk_effect;

    if (hWhoseTurn == 0) {
        sub_hp     = &wEnemySubstituteHP;
        bstat2     = &wEnemyBattleStatus2;
        atk_effect = &wPlayerMoveEffect;
    } else {
        sub_hp     = &wPlayerSubstituteHP;
        bstat2     = &wPlayerBattleStatus2;
        atk_effect = &wEnemyMoveEffect;
    }

    BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_SUBSTITUTE_TOOK_DAMAGE,
                              (uint8_t)(hWhoseTurn == 0 ? 1u : 0u), 0u);

    if ((wDamage >> 8) != 0) goto substitute_broke;

    {

        uint8_t dmg = (uint8_t)wDamage;
        uint8_t before = *sub_hp;
        *sub_hp = (uint8_t)(before - dmg);
        if (before >= dmg)
            return;
    }

substitute_broke:
    *bstat2 &= ~(1u << BSTAT2_HAS_SUBSTITUTE);

    BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_SUBSTITUTE_BROKE,
                              (uint8_t)(hWhoseTurn == 0 ? 1u : 0u), 0u);

    *atk_effect = 0;
}

static void apply_damage_to_enemy_pokemon(void) {
    if (wDamage == 0) return;

    if (wEnemyBattleStatus2 & (1u << BSTAT2_HAS_SUBSTITUTE)) {
        attack_substitute();
        return;
    }

    if (wEnemyMon.hp <= wDamage) {
        wDamage = wEnemyMon.hp;
        wEnemyMon.hp = 0;
    } else {
        wEnemyMon.hp -= wDamage;
    }
    battle_event_push_hp_target(0u);
    BLOG("  %s took %d dmg -> %d/%d HP", BMON_E(), wDamage, wEnemyMon.hp, wEnemyMon.max_hp);
}

static void apply_damage_to_player_pokemon(void) {
    if (wDamage == 0) return;

    if (wPlayerBattleStatus2 & (1u << BSTAT2_HAS_SUBSTITUTE)) {
        attack_substitute();
        return;
    }

    if (wBattleMon.hp <= wDamage) {
        wDamage = wBattleMon.hp;
        wBattleMon.hp = 0;
    } else {
        wBattleMon.hp -= wDamage;
    }
    battle_event_push_hp_target(1u);
    BLOG("  %s took %d dmg -> %d/%d HP", BMON_P(), wDamage, wBattleMon.hp, wBattleMon.max_hp);
}

static void apply_attack_to_enemy_pokemon(void) {
    BPROBE("ApplyAttackToEnemyPokemon");
    uint8_t effect = wPlayerMoveEffect;

    if (effect == EFFECT_OHKO) {

        goto do_apply;
    }
    if (effect == EFFECT_SUPER_FANG) {

        uint16_t half = wEnemyMon.hp >> 1;
        wDamage = (half == 0) ? 1 : half;
        goto do_apply;
    }
    if (effect == EFFECT_SPECIAL_DAMAGE) {

        uint8_t level = wBattleMon.level;
        uint8_t move  = wPlayerMoveNum;
        uint8_t dmg;
        if (move == MOVE_SEISMIC_TOSS || move == MOVE_NIGHT_SHADE) {
            dmg = level;
        } else if (move == MOVE_SONICBOOM) {
            dmg = SONICBOOM_DAMAGE;
        } else if (move == MOVE_DRAGON_RAGE) {
            dmg = DRAGON_RAGE_DAMAGE;
        } else {

            uint8_t max_dmg = (uint8_t)(level + (level >> 1));
            uint8_t r;
            do {
                r = BattleRandom();
            } while (r == 0 || r >= max_dmg);
            dmg = r;
        }
        wDamage = dmg;
        goto do_apply;
    }

    if (wPlayerMovePower == 0) return;

do_apply:
    apply_damage_to_enemy_pokemon();
}

static void apply_attack_to_player_pokemon(void) {
    BPROBE("ApplyAttackToPlayerPokemon");
    uint8_t effect = wEnemyMoveEffect;

    if (effect == EFFECT_OHKO) {
        goto do_apply;
    }
    if (effect == EFFECT_SUPER_FANG) {
        uint16_t half = wBattleMon.hp >> 1;
        wDamage = (half == 0) ? 1 : half;
        goto do_apply;
    }
    if (effect == EFFECT_SPECIAL_DAMAGE) {

        uint8_t level = wEnemyMon.level;
        uint8_t move  = wEnemyMoveNum;
        uint8_t dmg;
        if (move == MOVE_SEISMIC_TOSS || move == MOVE_NIGHT_SHADE) {
            dmg = level;
        } else if (move == MOVE_SONICBOOM) {
            dmg = SONICBOOM_DAMAGE;
        } else if (move == MOVE_DRAGON_RAGE) {
            dmg = DRAGON_RAGE_DAMAGE;
        } else {

            uint8_t max_dmg = (uint8_t)(level + (level >> 1));
            uint8_t r;
            do {
                r = BattleRandom();
            } while (r >= max_dmg);
            dmg = r;
        }
        wDamage = dmg;
        goto do_apply;
    }
    if (wEnemyMovePower == 0) return;

do_apply:
    apply_damage_to_player_pokemon();
}

static void handle_building_rage(void) {
    uint8_t  *bstat2;
    uint8_t  *stat_mods;
    uint8_t  *move_num;
    uint8_t  *move_effect;

    if (hWhoseTurn == 0) {
        bstat2      = &wEnemyBattleStatus2;
        stat_mods   = wEnemyMonStatMods;
        move_num    = &wEnemyMoveNum;
        move_effect = &wEnemyMoveEffect;
    } else {
        bstat2      = &wPlayerBattleStatus2;
        stat_mods   = wPlayerMonStatMods;
        move_num    = &wPlayerMoveNum;
        move_effect = &wPlayerMoveEffect;
    }

    if (!(*bstat2 & (1u << BSTAT2_USING_RAGE))) return;
    if (stat_mods[MOD_ATTACK] >= STAT_STAGE_MAX) return;

    BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_RAGE_BUILDING,
                              (uint8_t)(hWhoseTurn == 0 ? 1u : 0u), 0u);

    uint8_t saved_num    = *move_num;
    uint8_t saved_effect = *move_effect;
    *move_num    = 0;
    *move_effect = EFFECT_ATTACK_UP1;

    uint8_t atk_mod_before = stat_mods[MOD_ATTACK];
    hWhoseTurn ^= 1;
    Battle_StatModifierUpEffect();
    hWhoseTurn ^= 1;

    if (stat_mods[MOD_ATTACK] == atk_mod_before) {
        BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_NOTHING_HAPPENED,
                                  (uint8_t)(hWhoseTurn == 0 ? 1u : 0u), 0u);
    }

    *move_num    = MOVE_RAGE;
    *move_effect = saved_effect;
    (void)saved_num;
}

static void handle_self_confusion_damage(void) {
    uint16_t saved_enemy_def = wEnemyMon.def;
    wEnemyMon.def = wBattleMon.def;

    uint8_t saved_effect = wPlayerMoveEffect;
    uint8_t saved_power  = wPlayerMovePower;
    uint8_t saved_type   = wPlayerMoveType;
    wPlayerMoveEffect = 0;
    wPlayerMovePower  = 40;
    wPlayerMoveType   = 0;
    wCriticalHitOrOHKO = 0;

    Battle_GetDamageVarsForPlayerAttack();

    wPlayerMoveEffect = saved_effect;
    wPlayerMovePower  = saved_power;
    wPlayerMoveType   = saved_type;
    wEnemyMon.def = saved_enemy_def;

    apply_damage_to_player_pokemon();
}

static void clear_charging_up_for_disabled_move(uint8_t side) {
    if (side == 0u)
        wPlayerBattleStatus1 &= ~(1u << BSTAT1_CHARGING_UP);
    else
        wEnemyBattleStatus1 &= ~(1u << BSTAT1_CHARGING_UP);
}

static int handle_counter_move(void) {
    BPROBE("HandleCounterMove");
    uint8_t cur_move;
    uint8_t opp_selected;
    uint8_t opp_power;
    uint8_t opp_type;

    if (hWhoseTurn == 0) {
        cur_move     = wPlayerMoveNum;
        opp_selected = wEnemySelectedMove;
        opp_power    = wEnemyMovePower;
        opp_type     = wEnemyMoveType;
    } else {
        cur_move     = wEnemyMoveNum;
        opp_selected = wPlayerSelectedMove;
        opp_power    = wPlayerMovePower;
        opp_type     = wPlayerMoveType;
    }

    if (cur_move != MOVE_COUNTER) return 1;

    wMoveMissed = 1;

    if (opp_selected == MOVE_COUNTER) return 0;
    if (opp_power == 0)               return 0;

    if (opp_type != TYPE_NORMAL && opp_type != TYPE_FIGHTING) return 0;

    if (wDamage == 0) return 0;

    uint32_t doubled = (uint32_t)wDamage * 2;
    wDamage = (doubled > 0xFFFF) ? 0xFFFF : (uint16_t)doubled;
    wMoveMissed = 0;

    Battle_MoveHitTest();

    return 0;
}

#define PSTAT_CAN_MOVE    1
#define PSTAT_DONE        0
#define PSTAT_MISSED      2
#define PSTAT_CALC_DMG    3
#define PSTAT_GET_ANIM    4
#define PSTAT_RAGE        5

#define BATTLE_EVENTQ_MAX 24
static battle_event_t s_battle_event_q[BATTLE_EVENTQ_MAX];
static uint8_t s_battle_event_q_head = 0;
static uint8_t s_battle_event_q_tail = 0;
static uint8_t s_battle_event_q_count = 0;

void BattleEvent_ResetTurnQueue(void) {
    s_battle_event_q_head = 0;
    s_battle_event_q_tail = 0;
    s_battle_event_q_count = 0;
}

static void battle_event_push(uint8_t type, uint8_t arg0, uint8_t arg1, uint8_t arg2) {
    if (s_battle_event_q_count >= BATTLE_EVENTQ_MAX) return;
    s_battle_event_q[s_battle_event_q_head].type = type;
    s_battle_event_q[s_battle_event_q_head].arg0 = arg0;
    s_battle_event_q[s_battle_event_q_head].arg1 = arg1;
    s_battle_event_q[s_battle_event_q_head].arg2 = arg2;
    s_battle_event_q_head = (uint8_t)((s_battle_event_q_head + 1u) % BATTLE_EVENTQ_MAX);
    s_battle_event_q_count++;
}

static void battle_event_push_status_msg(uint8_t side, uint8_t is_pre, battle_status_msg_t msg) {
    if (msg == BSTAT_MSG_NONE) return;
    battle_event_push(BATTLE_EVENT_STATUS_MSG, (uint8_t)msg, is_pre ? 1u : 0u, side);
}

static void battle_event_push_hit_sfx(uint8_t dmg_mult) {
    if (dmg_mult == 0u) return;

    battle_event_push(BATTLE_EVENT_HIT_SFX, (uint8_t)(dmg_mult & 0x7Fu),
                      wCriticalHitOrOHKO, 0u);
}

static void battle_event_push_hp_target(uint8_t target_side) {
    if (target_side > 1u) return;

    uint16_t hp = (target_side == 1u) ? wBattleMon.hp : wEnemyMon.hp;
    battle_event_push(BATTLE_EVENT_HP_TARGET, target_side,
                      (uint8_t)(hp & 0xFFu),
                      (uint8_t)(0x80u | (uint8_t)(hp >> 8)));
}

static void battle_event_push_move_result(uint8_t result) {
    if (result == BATTLE_MOVE_RESULT_NONE) return;
    battle_event_push(BATTLE_EVENT_MOVE_RESULT, result, 0u, 0u);
}

void BattleEvent_PushMoveResult(uint8_t result) {
    battle_event_push_move_result(result);
}

static void battle_event_push_residual_msg(uint8_t side, uint8_t msg) {
    if (msg == BATTLE_RESIDUAL_MSG_NONE) return;
    if (side > 1u) return;
    battle_event_push(BATTLE_EVENT_RESIDUAL_MSG, msg, side, 0u);
}

void BattleEvent_PushPlayAnim(uint8_t anim_id, uint8_t forced_turn) {
    battle_event_push(BATTLE_EVENT_PLAY_ANIM, anim_id, forced_turn, 0u);
}

void BattleEvent_PushHPTarget(uint8_t side) {
    battle_event_push_hp_target(side);
}

void BattleEvent_PushEffectMsg(uint8_t msg, uint8_t side, uint8_t extra) {
    if (msg == BATTLE_EFFECT_MSG_NONE || side > 1u) return;
    battle_event_push(BATTLE_EVENT_EFFECT_MSG, msg, side, extra);
}

void BattleEvent_PushStatModText(uint8_t stat_idx, uint8_t side, uint8_t down, uint8_t greatly) {
    if (stat_idx >= NUM_STAT_MODS) return;
    if (side > 1u) return;
    battle_event_push(BATTLE_EVENT_STAT_MOD_TEXT, stat_idx, side,
                      (uint8_t)((down ? 1u : 0u) | (greatly ? 2u : 0u)));
}

static uint8_t battle_classify_move_failure_result(void) {
    if ((wDamageMultipliers & 0x7Fu) == 0u)
        return BATTLE_MOVE_RESULT_NO_EFFECT;
    if (wCriticalHitOrOHKO == 0xFFu)
        return BATTLE_MOVE_RESULT_UNAFFECTED;
    return BATTLE_MOVE_RESULT_MISS;
}

int BattleEvent_Pop(battle_event_t *out) {
    if (!out || s_battle_event_q_count == 0) return 0;
    *out = s_battle_event_q[s_battle_event_q_tail];
    s_battle_event_q_tail = (uint8_t)((s_battle_event_q_tail + 1u) % BATTLE_EVENTQ_MAX);
    s_battle_event_q_count--;
    return 1;
}

int BattleEvent_HasPending(void) {
    return s_battle_event_q_count != 0;
}

static uint8_t s_player_replaced_move = 0;
static uint8_t s_enemy_replaced_move  = 0;
uint8_t Battle_GetPlayerReplacedMove(void) { return s_player_replaced_move; }
uint8_t Battle_GetEnemyReplacedMove(void)  { return s_enemy_replaced_move;  }

static uint8_t s_player_executed_move = 0;
static uint8_t s_enemy_executed_move  = 0;
uint8_t Battle_SideExecutedMove(int whose) {
    return (uint8_t)(whose == 0 ? s_player_executed_move : s_enemy_executed_move);
}

static battle_announce_t s_player_announce = BATTLE_ANNOUNCE_USED_MOVE;
static battle_announce_t s_enemy_announce  = BATTLE_ANNOUNCE_USED_MOVE;
battle_announce_t Battle_GetPlayerAnnounce(void) { return s_player_announce; }
battle_announce_t Battle_GetEnemyAnnounce(void)  { return s_enemy_announce;  }

static uint8_t s_last_crit;
uint8_t Battle_GetLastCrit(void) { return s_last_crit; }

static void print_critical_ohko_text(void) {
    s_last_crit = wCriticalHitOrOHKO;
    wCriticalHitOrOHKO = 0;
}

static battle_status_msg_t s_player_status_msg = BSTAT_MSG_NONE;
static battle_status_msg_t s_player_pre_msg    = BSTAT_MSG_NONE;
static battle_status_msg_t s_enemy_status_msg  = BSTAT_MSG_NONE;
static battle_status_msg_t s_enemy_pre_msg     = BSTAT_MSG_NONE;
static uint8_t s_player_status_affected_anim_pending = 0;
static uint8_t s_enemy_status_affected_anim_pending  = 0;
static uint8_t s_player_status_anim_id = 0;
static uint8_t s_enemy_status_anim_id  = 0;
static uint8_t s_player_confusion_selfhit_anim_pending = 0;
static uint8_t s_enemy_confusion_selfhit_anim_pending  = 0;
static uint8_t  s_last_player_hit_count = 1;
static uint16_t s_last_player_first_target_hp = 0;
static uint8_t  s_last_enemy_hit_count = 1;
static uint16_t s_last_enemy_first_target_hp = 0;

#define STATUS_ANIM_SLP_PLAYER 188u
#define STATUS_ANIM_SLP_ENEMY  189u
#define STATUS_ANIM_CONF_PLAYER 190u
#define STATUS_ANIM_CONF_ENEMY  191u

static int check_player_status_conditions(void) {
    BPROBE("CheckPlayerStatusConditions");
    s_player_status_msg = BSTAT_MSG_NONE;
    s_player_pre_msg    = BSTAT_MSG_NONE;
    BLOG("STATUSCHECK player start status=0x%02X par=%u confused=%u disabled=%u recharge=%u",
         (unsigned)wBattleMon.status,
         (unsigned)((wBattleMon.status & STATUS_PAR) != 0u),
         (unsigned)((wPlayerBattleStatus1 & (1u << BSTAT1_CONFUSED)) != 0u),
         (unsigned)wPlayerDisabledMove,
         (unsigned)((wPlayerBattleStatus2 & (1u << BSTAT2_NEEDS_TO_RECHARGE)) != 0u));

    uint8_t slp = wBattleMon.status & STATUS_SLP_MASK;
    if (slp) {
        slp--;
        wBattleMon.status = (uint8_t)((wBattleMon.status & ~STATUS_SLP_MASK) | slp);
        s_player_status_msg = (slp == 0) ? BSTAT_MSG_WOKE_UP : BSTAT_MSG_FAST_ASLEEP;
        battle_event_push_status_msg(0u, 0u, s_player_status_msg);
        if (slp != 0) {
            s_player_status_anim_id = STATUS_ANIM_SLP_PLAYER;
            battle_event_push(BATTLE_EVENT_PLAY_ANIM, STATUS_ANIM_SLP_PLAYER, 0u, 0u);
        }
        wPlayerUsedMove = 0;
        return PSTAT_DONE;
    }

    if (wBattleMon.status & STATUS_FRZ) {
        s_player_status_msg = BSTAT_MSG_FROZEN;
        battle_event_push_status_msg(0u, 0u, s_player_status_msg);
        wPlayerUsedMove = 0;
        return PSTAT_DONE;
    }

    if (wEnemyBattleStatus1 & (1u << BSTAT1_USING_TRAPPING)) {
        s_player_status_msg = BSTAT_MSG_CANT_MOVE;
        battle_event_push_status_msg(0u, 0u, s_player_status_msg);
        return PSTAT_DONE;
    }

    if (wPlayerBattleStatus1 & (1u << BSTAT1_FLINCHED)) {
        wPlayerBattleStatus1 &= ~(1u << BSTAT1_FLINCHED);
        s_player_status_msg = BSTAT_MSG_FLINCHED;
        battle_event_push_status_msg(0u, 0u, s_player_status_msg);
        return PSTAT_DONE;
    }

    if (wPlayerBattleStatus2 & (1u << BSTAT2_NEEDS_TO_RECHARGE)) {
        wPlayerBattleStatus2 &= ~(1u << BSTAT2_NEEDS_TO_RECHARGE);
        s_player_status_msg = BSTAT_MSG_MUST_RECHARGE;
        battle_event_push_status_msg(0u, 0u, s_player_status_msg);
        return PSTAT_DONE;
    }

    if (wPlayerDisabledMove) {
        wPlayerDisabledMove--;
        if ((wPlayerDisabledMove & 0x0F) == 0) {
            wPlayerDisabledMove = 0;
            wPlayerDisabledMoveNumber = 0;
            s_player_pre_msg = BSTAT_MSG_DISABLED_NO_MORE;
            battle_event_push_status_msg(0u, 1u, s_player_pre_msg);
        }
    }

    if (wPlayerBattleStatus1 & (1u << BSTAT1_CONFUSED)) {
        wPlayerConfusedCounter--;
        if (wPlayerConfusedCounter == 0) {
            wPlayerBattleStatus1 &= ~(1u << BSTAT1_CONFUSED);
            s_player_pre_msg = BSTAT_MSG_CONFUSED_NO_MORE;
            battle_event_push_status_msg(0u, 1u, s_player_pre_msg);
        } else {

            s_player_pre_msg = BSTAT_MSG_IS_CONFUSED;
            battle_event_push_status_msg(0u, 1u, s_player_pre_msg);
            s_player_status_anim_id = STATUS_ANIM_CONF_PLAYER;
            battle_event_push(BATTLE_EVENT_PLAY_ANIM, STATUS_ANIM_CONF_PLAYER, 0u, 0u);

            if (BattleRandom() >= 128) {

                wPlayerBattleStatus1 &= (1u << BSTAT1_CONFUSED);
                s_player_confusion_selfhit_anim_pending = 1u;
                battle_event_push(BATTLE_EVENT_PLAY_ANIM, 1u , 1u, 0u);
                s_player_status_msg = BSTAT_MSG_HURT_ITSELF;
                battle_event_push_status_msg(0u, 0u, s_player_status_msg);
                handle_self_confusion_damage();
                goto mon_hurt_itself;
            }
        }
    }

    if (wPlayerDisabledMoveNumber &&
        wPlayerDisabledMoveNumber == wPlayerSelectedMove) {
        clear_charging_up_for_disabled_move(0u);
        s_player_status_msg = BSTAT_MSG_MOVE_DISABLED;
        battle_event_push_status_msg(0u, 0u, s_player_status_msg);
        return PSTAT_DONE;
    }

    if (wBattleMon.status & STATUS_PAR) {

        uint8_t roll = BattleRandom();
        BLOG("PARCHECK player status=0x%02X roll=%u threshold=63 result=%s",
             (unsigned)wBattleMon.status, (unsigned)roll,
             (roll < 63) ? "FULLY_PARALYZED" : "acts");
        if (roll < 63) {
            s_player_status_msg = BSTAT_MSG_FULLY_PARALYZED;
            battle_event_push_status_msg(0u, 0u, s_player_status_msg);
            goto mon_hurt_itself;
        }
    }

    if (wPlayerBattleStatus1 & (1u << BSTAT1_STORING_ENERGY)) {
        wPlayerMoveNum = 0;
        wPlayerBideAccumulatedDamage += wDamage;
        wPlayerNumAttacksLeft--;
        if (wPlayerNumAttacksLeft > 0) return PSTAT_DONE;

        wPlayerBattleStatus1 &= ~(1u << BSTAT1_STORING_ENERGY);
        s_player_announce = BATTLE_ANNOUNCE_UNLEASHED_ENERGY;

        wPlayerMovePower = 1;
        uint16_t bide_dmg = wPlayerBideAccumulatedDamage * 2;
        wPlayerBideAccumulatedDamage = 0;
        wDamage = bide_dmg;
        if (bide_dmg == 0) wMoveMissed = 1;
        wPlayerMoveNum = MOVE_BIDE;
        return PSTAT_MISSED;
    }

    if (wPlayerBattleStatus1 & (1u << BSTAT1_THRASHING_ABOUT)) {
        wPlayerMoveNum = MOVE_THRASH;
        s_player_announce = BATTLE_ANNOUNCE_THRASHING;
        wPlayerNumAttacksLeft--;
        if (wPlayerNumAttacksLeft > 0) return PSTAT_CALC_DMG;

        wPlayerBattleStatus1 &= ~(1u << BSTAT1_THRASHING_ABOUT);
        wPlayerBattleStatus1 |= (1u << BSTAT1_CONFUSED);
        uint8_t ctr = (BattleRandom() & 3) + 2;
        wPlayerConfusedCounter = ctr;
        return PSTAT_CALC_DMG;
    }

    if (wPlayerBattleStatus1 & (1u << BSTAT1_USING_TRAPPING)) {
        s_player_announce = BATTLE_ANNOUNCE_ATTACK_CONTINUES;
        wPlayerNumAttacksLeft--;
        return PSTAT_GET_ANIM;
    }

    if (wPlayerBattleStatus2 & (1u << BSTAT2_USING_RAGE)) {
        wPlayerMoveEffect = 0;
        return PSTAT_RAGE;
    }

    return PSTAT_CAN_MOVE;

mon_hurt_itself:

    wPlayerBattleStatus1 &= ~((1u << BSTAT1_STORING_ENERGY) |
                               (1u << BSTAT1_THRASHING_ABOUT) |
                               (1u << BSTAT1_CHARGING_UP) |
                               (1u << BSTAT1_USING_TRAPPING));

    if (wPlayerMoveEffect == EFFECT_FLY || wPlayerMoveEffect == EFFECT_CHARGE) {
        s_player_status_affected_anim_pending = 1u;
        battle_event_push(BATTLE_EVENT_PLAY_ANIM, 167u , 0u, 0u);
    }

    return PSTAT_DONE;
}

static int check_enemy_status_conditions(void) {
    BPROBE("CheckEnemyStatusConditions");
    s_enemy_status_msg = BSTAT_MSG_NONE;
    s_enemy_pre_msg    = BSTAT_MSG_NONE;
    BLOG("STATUSCHECK enemy start status=0x%02X par=%u confused=%u disabled=%u recharge=%u",
         (unsigned)wEnemyMon.status,
         (unsigned)((wEnemyMon.status & STATUS_PAR) != 0u),
         (unsigned)((wEnemyBattleStatus1 & (1u << BSTAT1_CONFUSED)) != 0u),
         (unsigned)wEnemyDisabledMove,
         (unsigned)((wEnemyBattleStatus2 & (1u << BSTAT2_NEEDS_TO_RECHARGE)) != 0u));

    uint8_t slp = wEnemyMon.status & STATUS_SLP_MASK;
    if (slp) {
        slp--;
        wEnemyMon.status = (uint8_t)((wEnemyMon.status & ~STATUS_SLP_MASK) | slp);
        s_enemy_status_msg = (slp == 0) ? BSTAT_MSG_WOKE_UP : BSTAT_MSG_FAST_ASLEEP;
        battle_event_push_status_msg(1u, 0u, s_enemy_status_msg);
        if (slp != 0) {
            s_enemy_status_anim_id = STATUS_ANIM_SLP_ENEMY;
            battle_event_push(BATTLE_EVENT_PLAY_ANIM, STATUS_ANIM_SLP_ENEMY, 1u, 0u);
        }
        wEnemyUsedMove = 0;
        return PSTAT_DONE;
    }

    if (wEnemyMon.status & STATUS_FRZ) {
        s_enemy_status_msg = BSTAT_MSG_FROZEN;
        battle_event_push_status_msg(1u, 0u, s_enemy_status_msg);
        wEnemyUsedMove = 0;
        return PSTAT_DONE;
    }

    if (wPlayerBattleStatus1 & (1u << BSTAT1_USING_TRAPPING)) {
        s_enemy_status_msg = BSTAT_MSG_CANT_MOVE;
        battle_event_push_status_msg(1u, 0u, s_enemy_status_msg);
        return PSTAT_DONE;
    }

    if (wEnemyBattleStatus1 & (1u << BSTAT1_FLINCHED)) {
        wEnemyBattleStatus1 &= ~(1u << BSTAT1_FLINCHED);
        s_enemy_status_msg = BSTAT_MSG_FLINCHED;
        battle_event_push_status_msg(1u, 0u, s_enemy_status_msg);
        return PSTAT_DONE;
    }

    if (wEnemyBattleStatus2 & (1u << BSTAT2_NEEDS_TO_RECHARGE)) {
        wEnemyBattleStatus2 &= ~(1u << BSTAT2_NEEDS_TO_RECHARGE);
        s_enemy_status_msg = BSTAT_MSG_MUST_RECHARGE;
        battle_event_push_status_msg(1u, 0u, s_enemy_status_msg);
        return PSTAT_DONE;
    }

    if (wEnemyDisabledMove) {
        wEnemyDisabledMove--;
        if ((wEnemyDisabledMove & 0x0F) == 0) {
            wEnemyDisabledMove = 0;
            wEnemyDisabledMoveNumber = 0;
            s_enemy_pre_msg = BSTAT_MSG_DISABLED_NO_MORE;
            battle_event_push_status_msg(1u, 1u, s_enemy_pre_msg);
        }
    }

    if (wEnemyBattleStatus1 & (1u << BSTAT1_CONFUSED)) {
        wEnemyConfusedCounter--;
        if (wEnemyConfusedCounter == 0) {
            wEnemyBattleStatus1 &= ~(1u << BSTAT1_CONFUSED);
            s_enemy_pre_msg = BSTAT_MSG_CONFUSED_NO_MORE;
            battle_event_push_status_msg(1u, 1u, s_enemy_pre_msg);
        } else {

            s_enemy_pre_msg = BSTAT_MSG_IS_CONFUSED;
            battle_event_push_status_msg(1u, 1u, s_enemy_pre_msg);
            s_enemy_status_anim_id = STATUS_ANIM_CONF_ENEMY;
            battle_event_push(BATTLE_EVENT_PLAY_ANIM, STATUS_ANIM_CONF_ENEMY, 1u, 0u);

            if (BattleRandom() >= 0x80) {

                uint16_t saved_player_def = wBattleMon.def;
                wBattleMon.def = wEnemyMon.def;
                uint8_t saved_effect = wEnemyMoveEffect;
                uint8_t saved_power  = wEnemyMovePower;
                uint8_t saved_type   = wEnemyMoveType;
                wEnemyMoveEffect = 0;
                wEnemyMovePower  = 40;
                wEnemyMoveType   = 0;
                wCriticalHitOrOHKO = 0;
                Battle_GetDamageVarsForEnemyAttack();
                wEnemyMoveEffect = saved_effect;
                wEnemyMovePower  = saved_power;
                wEnemyMoveType   = saved_type;
                wBattleMon.def = saved_player_def;

                s_enemy_confusion_selfhit_anim_pending = 1u;
                battle_event_push(BATTLE_EVENT_PLAY_ANIM, 1u , 0u, 0u);
                s_enemy_status_msg = BSTAT_MSG_HURT_ITSELF;
                battle_event_push_status_msg(1u, 0u, s_enemy_status_msg);
                apply_damage_to_enemy_pokemon();
                goto enemy_hurt_itself;
            }
        }
    }

    if (wEnemyDisabledMoveNumber &&
        wEnemyDisabledMoveNumber == wEnemySelectedMove) {
        clear_charging_up_for_disabled_move(1u);
        s_enemy_status_msg = BSTAT_MSG_MOVE_DISABLED;
        battle_event_push_status_msg(1u, 0u, s_enemy_status_msg);
        return PSTAT_DONE;
    }

    if (wEnemyMon.status & STATUS_PAR) {

        uint8_t roll = BattleRandom();
        BLOG("PARCHECK enemy status=0x%02X roll=%u threshold=63 result=%s",
             (unsigned)wEnemyMon.status, (unsigned)roll,
             (roll < 63) ? "FULLY_PARALYZED" : "acts");
        if (roll < 63) {
            s_enemy_status_msg = BSTAT_MSG_FULLY_PARALYZED;
            battle_event_push_status_msg(1u, 0u, s_enemy_status_msg);
            goto enemy_hurt_itself;
        }
    }

    if (wEnemyBattleStatus1 & (1u << BSTAT1_STORING_ENERGY)) {
        wEnemyMoveNum = 0;
        wEnemyBideAccumulatedDamage += wDamage;
        wEnemyNumAttacksLeft--;
        if (wEnemyNumAttacksLeft > 0) return PSTAT_DONE;
        wEnemyBattleStatus1 &= ~(1u << BSTAT1_STORING_ENERGY);
        s_enemy_announce = BATTLE_ANNOUNCE_UNLEASHED_ENERGY;
        wEnemyMovePower = 1;
        uint16_t bide_dmg = wEnemyBideAccumulatedDamage * 2;
        wEnemyBideAccumulatedDamage = 0;
        wDamage = bide_dmg;
        if (bide_dmg == 0) wMoveMissed = 1;
        wEnemyMoveNum = MOVE_BIDE;
        swap_player_enemy_levels();
        return PSTAT_MISSED;
    }

    if (wEnemyBattleStatus1 & (1u << BSTAT1_THRASHING_ABOUT)) {
        wEnemyMoveNum = MOVE_THRASH;
        s_enemy_announce = BATTLE_ANNOUNCE_THRASHING;
        wEnemyNumAttacksLeft--;
        if (wEnemyNumAttacksLeft > 0) return PSTAT_CALC_DMG;
        wEnemyBattleStatus1 &= ~(1u << BSTAT1_THRASHING_ABOUT);
        wEnemyBattleStatus1 |= (1u << BSTAT1_CONFUSED);
        wEnemyConfusedCounter = (BattleRandom() & 3) + 2;
        return PSTAT_CALC_DMG;
    }

    if (wEnemyBattleStatus1 & (1u << BSTAT1_USING_TRAPPING)) {
        s_enemy_announce = BATTLE_ANNOUNCE_ATTACK_CONTINUES;
        wEnemyNumAttacksLeft--;
        return PSTAT_GET_ANIM;
    }

    if (wEnemyBattleStatus2 & (1u << BSTAT2_USING_RAGE)) {
        wEnemyMoveEffect = 0;
        return PSTAT_RAGE;
    }

    return PSTAT_CAN_MOVE;

enemy_hurt_itself:
    wEnemyBattleStatus1 &= ~((1u << BSTAT1_STORING_ENERGY) |
                              (1u << BSTAT1_THRASHING_ABOUT) |
                              (1u << BSTAT1_CHARGING_UP) |
                              (1u << BSTAT1_USING_TRAPPING));

    if (wEnemyMoveEffect == EFFECT_FLY || wEnemyMoveEffect == EFFECT_CHARGE) {
        s_enemy_status_affected_anim_pending = 1u;
        battle_event_push(BATTLE_EVENT_PLAY_ANIM, 167u , 1u, 0u);
    }
    return PSTAT_DONE;
}

static int check_for_disobedience(void) {
    BPROBE("CheckForDisobedience");
    wMonIsDisobedient = 0;

    if (wLinkState == LINK_STATE_BATTLING) return 1;

    if (wPartyMons[wPlayerMonNumber].base.ot_id == wPlayerID) return 1;

    uint8_t threshold;
    if (wObtainedBadges & (1u << BIT_EARTHBADGE))   { threshold = 101; }
    else if (wObtainedBadges & (1u << BIT_MARSHBADGE))   { threshold = 70;  }
    else if (wObtainedBadges & (1u << BIT_RAINBOWBADGE)) { threshold = 50;  }
    else if (wObtainedBadges & (1u << BIT_CASCADEBADGE)) { threshold = 30;  }
    else                                                   { threshold = 10;  }

    uint8_t level = wBattleMon.level;
    if (level < threshold) return 1;

    uint8_t b = (uint8_t)((threshold + (uint16_t)level > 0xFF) ? 0xFF
                            : (threshold + level));

    uint8_t r1;
    do { r1 = (uint8_t)((BattleRandom() >> 4) | (BattleRandom() << 4)); }
    while (r1 >= b);

    if (r1 < threshold) return 1;

    uint8_t r2;
    do { r2 = BattleRandom(); } while (r2 >= b);

    if (r2 < threshold) {

        wMonIsDisobedient = 1;
        return 0;
    }

    uint8_t diff = level - threshold;
    uint8_t r3   = (uint8_t)((BattleRandom() >> 4) | (BattleRandom() << 4));
    if ((int8_t)(r3 - diff) < 0) {

        uint8_t sleep_ctr;
        do {
            sleep_ctr = (uint8_t)(((BattleRandom() << 1) >> 4) & STATUS_SLP_MASK);
        } while (sleep_ctr == 0);
        wBattleMon.status = sleep_ctr;
        return 0;
    }
    if (r3 >= (uint8_t)(diff + b)) {

        return 0;
    }

    handle_self_confusion_damage();
    return 0;
}

static void get_current_move(void);

void Battle_GetCurrentMove(void) { get_current_move(); }

#define GLITCH_MOVE0_EFFECT   116u
#define GLITCH_MOVE0_POWER    102u
#define GLITCH_MOVE0_TYPE     122u
#define GLITCH_MOVE0_ACCURACY  80u
#define GLITCH_MOVE0_PP        13u
#define GLITCH_MOVE0_MOVENUM   90u

static void get_current_move(void) {
    if (hWhoseTurn == 0) {
        uint8_t move_id = wPlayerSelectedMove;
        if (move_id == 0) {
            wPlayerMoveNum      = GLITCH_MOVE0_MOVENUM;
            wPlayerMoveEffect   = GLITCH_MOVE0_EFFECT;
            wPlayerMovePower    = GLITCH_MOVE0_POWER;
            wPlayerMoveType     = GLITCH_MOVE0_TYPE;
            wPlayerMoveAccuracy = GLITCH_MOVE0_ACCURACY;
            wPlayerMoveMaxPP    = GLITCH_MOVE0_PP;
            return;
        }
        wPlayerMoveNum      = move_id;
        wPlayerMoveEffect   = gMoves[move_id].effect;
        wPlayerMovePower    = gMoves[move_id].power;
        wPlayerMoveType     = gMoves[move_id].type;
        wPlayerMoveAccuracy = gMoves[move_id].accuracy;
        wPlayerMoveMaxPP    = gMoves[move_id].pp;
    } else {
        uint8_t move_id = wEnemySelectedMove;
        if (move_id == 0) {
            wEnemyMoveNum      = GLITCH_MOVE0_MOVENUM;
            wEnemyMoveEffect   = GLITCH_MOVE0_EFFECT;
            wEnemyMovePower    = GLITCH_MOVE0_POWER;
            wEnemyMoveType     = GLITCH_MOVE0_TYPE;
            wEnemyMoveAccuracy = GLITCH_MOVE0_ACCURACY;
            wEnemyMoveMaxPP    = GLITCH_MOVE0_PP;
            return;
        }
        wEnemyMoveNum      = move_id;
        wEnemyMoveEffect   = gMoves[move_id].effect;
        wEnemyMovePower    = gMoves[move_id].power;
        wEnemyMoveType     = gMoves[move_id].type;
        wEnemyMoveAccuracy = gMoves[move_id].accuracy;
        wEnemyMoveMaxPP    = gMoves[move_id].pp;
    }
}

static void decrement_pp(void) {
    BPROBE("DecrementPP");

    uint8_t move = wPlayerSelectedMove;
    if (move == MOVE_STRUGGLE) return;

    if (wPlayerBattleStatus1 & ((1u << BSTAT1_STORING_ENERGY) |
                                (1u << BSTAT1_THRASHING_ABOUT) |
                                (1u << BSTAT1_ATTACKING_MULTIPLE))) {
        return;
    }
    if (wPlayerBattleStatus2 & (1u << BSTAT2_USING_RAGE)) return;

    {
        uint8_t slot = wPlayerMoveListIndex;
        if (slot >= 4) return;

        wBattleMon.pp[slot]--;

        if (!(wPlayerBattleStatus3 & (1u << BSTAT3_TRANSFORMED))) {
            wPartyMons[wPlayerMonNumber].base.pp[slot]--;
        }
    }
}

static void increment_move_pp(void) {
    BPROBE("IncrementMovePP");
    if (hWhoseTurn == 0) {
        uint8_t slot = wPlayerMoveListIndex;
        if (slot >= NUM_MOVES) return;
        wBattleMon.pp[slot]++;
        wPartyMons[wPlayerMonNumber].base.pp[slot]++;
    } else {
        uint8_t slot = wEnemyMoveListIndex;
        if (slot >= NUM_MOVES) return;
        wEnemyMon.pp[slot]++;
    }
}

#define MAP_POKEMON_TOWER_1F 0x8E
#define MAP_POKEMON_TOWER_7F 0x94
#define ITEM_SILPH_SCOPE     0x48

int Battle_MapIsPokemonTower(void) {
    if (wCurMap >= MAP_POKEMON_TOWER_1F && wCurMap <= MAP_POKEMON_TOWER_7F) return 1;

    if (wCurMap < PKS_VIRTUAL_MAP_FIRST || wCurMap > PKS_VIRTUAL_MAP_LAST) return 0;
    {
        const char *n = AmberScript_MapBank_NameForRealId(wCurMap);
        return n && strncmp(n, "PokemonTower", 12) == 0;
    }
}

int Battle_IsGhostBattle(void) {
    if (wIsInBattle != 1) return 0;
    if (!Battle_MapIsPokemonTower()) return 0;

    return Inventory_GetQty(ITEM_SILPH_SCOPE) == 0;
}

void Battle_ExecutePlayerMove(void) {
    hWhoseTurn = 0;
    BLOG("EXEC player start move=%s(0x%02X) effect=0x%02X target_pre=%s target_hp_pre=%u/%u",
         BMOVE(wPlayerSelectedMove), (unsigned)wPlayerSelectedMove, (unsigned)wPlayerMoveEffect,
         BMON_E(), (unsigned)wEnemyMon.hp, (unsigned)wEnemyMon.max_hp);
    BattleEvent_ResetTurnQueue();
    s_player_executed_move = 0;
    s_player_status_affected_anim_pending = 0;
    s_player_status_anim_id = 0;
    s_player_replaced_move = 0;
    s_player_announce = BATTLE_ANNOUNCE_USED_MOVE;
    s_last_crit = 0;
    s_player_confusion_selfhit_anim_pending = 0;
    s_last_player_hit_count = 1;
    s_hit_hp_log_player_n = 0;
    s_last_player_first_target_hp = 0;

    if (wPlayerSelectedMove == CANNOT_MOVE) goto execute_done;

    wMoveMissed       = 0;
    wMonIsDisobedient = 0;
    wMoveDidntMiss    = 0;
    wDamageMultipliers = DAMAGE_MULT_EFFECTIVE;

    if (wActionResultOrTookBattleTurn) goto execute_done;

    if (Battle_IsGhostBattle() &&
        !(wBattleMon.status & (STATUS_FRZ | STATUS_SLP_MASK))) {
        s_player_status_msg = BSTAT_MSG_TOO_SCARED;
        battle_event_push_status_msg(0u, 0u, s_player_status_msg);
        wPlayerUsedMove = 0;
        goto execute_done;
    }

    {
        int sr = check_player_status_conditions();
        if (sr == PSTAT_DONE)     goto execute_done;
        if (sr == PSTAT_MISSED)   goto handle_if_player_move_missed;
        if (sr == PSTAT_CALC_DMG) goto player_calc_damage;
        if (sr == PSTAT_GET_ANIM) {

            apply_attack_to_enemy_pokemon();
            wMoveDidntMiss = 1;
            goto execute_after_apply;
        }
        if (sr == PSTAT_RAGE)     goto player_can_execute_move;

    }

    get_current_move();

    if (wPlayerBattleStatus1 & (1u << BSTAT1_CHARGING_UP)) {
        wPlayerBattleStatus1 &= ~((1u << BSTAT1_CHARGING_UP) |
                                   (1u << BSTAT1_INVULNERABLE));
        goto player_can_execute_move;
    }

    if (!check_for_disobedience()) goto execute_done;

check_charge:
    if (wPlayerMoveEffect == EFFECT_CHARGE || wPlayerMoveEffect == EFFECT_FLY) {

        s_player_executed_move = 1;
        Battle_JumpMoveEffect();
        goto execute_done;
    }

player_can_execute_move:

    wPlayerUsedMove = wPlayerMoveNum;
    s_player_executed_move = 1;

    decrement_pp();

    if (is_in_array(wPlayerMoveEffect, kResidualEffects1)) {
        Battle_JumpMoveEffect();
        goto execute_done;
    }

    if (is_in_array(wPlayerMoveEffect, kSpecialEffectsCont)) {
        Battle_JumpMoveEffect();
    }

player_calc_damage:

    if (is_in_array(wPlayerMoveEffect, kSetDamageEffects)) {
        goto player_move_hit_test;
    }

    Battle_CriticalHitTest();

    if (!handle_counter_move()) goto handle_if_player_move_missed;

    if (!Battle_GetDamageVarsForPlayerAttack()) {
        goto player_check_fly_charge;
    }
    Battle_AdjustDamageForMoveType();
    Battle_RandomizeDamage();

player_move_hit_test:
    Battle_MoveHitTest();

handle_if_player_move_missed:
    if (wMoveMissed && wPlayerMoveEffect != EFFECT_EXPLODE) {

        goto player_check_fly_charge;
    }

player_check_fly_charge:

player_mirror_move_check:
    if (wPlayerMoveEffect == EFFECT_MIRROR_MOVE) {

        wPlayerSelectedMove = wEnemyUsedMove;
        if (wEnemyUsedMove != 0 && wEnemyUsedMove != MOVE_MIRROR_MOVE) {
            s_player_replaced_move = wPlayerMoveNum;
            get_current_move();
            increment_move_pp();
            wMonIsDisobedient = 0;
            goto check_charge;
        }

        BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_MIRROR_MOVE_FAILED, 0u, 0u);
        goto execute_done;
    }
    if (wPlayerMoveEffect == EFFECT_METRONOME) {

        BattleEvent_PushPlayAnim(wPlayerMoveNum , 0u);

        uint8_t pick;
        do { pick = BattleRandom(); }
        while (pick == 0 || pick >= MOVE_STRUGGLE || pick == 118 );
        BattleEvent_PushPlayAnim(pick, 0u);
        s_player_replaced_move = wPlayerMoveNum;
        wPlayerSelectedMove = pick;
        get_current_move();
        increment_move_pp();
        wMonIsDisobedient = 0;
        goto check_charge;
    }

    if (is_in_array(wPlayerMoveEffect, kResidualEffects2)) {
        Battle_JumpMoveEffect();
        goto execute_done;
    }

    if (wMoveMissed) {
        battle_event_push_move_result(battle_classify_move_failure_result());

        wCriticalHitOrOHKO = 0;
        BLOG("  %s used %s -- missed!", BMON_P(), BMOVE(wPlayerMoveNum));

        if (wPlayerMoveEffect == EFFECT_JUMP_KICK) {

            wDamage >>= 3;
            if (wDamage == 0u) wDamage = 1u;
            BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_CRASHED, 0u, 0u);

            apply_damage_to_player_pokemon();
        }
        if (wPlayerMoveEffect != EFFECT_EXPLODE) goto execute_done;
    } else {
        BLOG("  %s used %s", BMON_P(), BMOVE(wPlayerMoveNum));

        if (wPlayerMovePower != 0u)
            battle_event_push_hit_sfx((uint8_t)(wDamageMultipliers & 0x7Fu));
        s_multihit_dmg_player = wDamage;
        apply_attack_to_enemy_pokemon();
        if (s_hit_hp_log_player_n < BATTLE_HIT_HP_LOG_MAX)
            s_hit_hp_log_player[s_hit_hp_log_player_n++] = wEnemyMon.hp;
        print_critical_ohko_text();
        wMoveDidntMiss = 1;
    }

execute_after_apply:

    if (is_in_array(wPlayerMoveEffect, kAlwaysHappenSideEffects)) {
        Battle_JumpMoveEffect();
    }

    if (wEnemyMon.hp == 0) goto execute_done;

    handle_building_rage();

    if (wPlayerBattleStatus1 & (1u << BSTAT1_ATTACKING_MULTIPLE)) {
        wPlayerNumAttacksLeft--;
        if (wPlayerNumAttacksLeft > 0) {

            wCriticalHitOrOHKO = 0;

            if (s_last_player_hit_count == 1)
                s_last_player_first_target_hp = wEnemyMon.hp;
            s_last_player_hit_count++;
            if (wPlayerMovePower != 0u)
                battle_event_push_hit_sfx((uint8_t)(wDamageMultipliers & 0x7Fu));
            wDamage = s_multihit_dmg_player;
            apply_attack_to_enemy_pokemon();
            if (s_hit_hp_log_player_n < BATTLE_HIT_HP_LOG_MAX)
                s_hit_hp_log_player[s_hit_hp_log_player_n++] = wEnemyMon.hp;
            if (wEnemyMon.hp == 0) {
                wPlayerBattleStatus1 &= ~(1u << BSTAT1_ATTACKING_MULTIPLE);
                goto execute_done;
            }

            goto execute_after_apply;
        }
        wPlayerBattleStatus1 &= ~(1u << BSTAT1_ATTACKING_MULTIPLE);

        BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_HIT_N_TIMES, 0u, wPlayerNumHits);
        wPlayerNumHits = 0;
    }

    if (wPlayerMoveEffect == 0) goto execute_done;

    if (!is_in_array(wPlayerMoveEffect, kSpecialEffects)) {
        Battle_JumpMoveEffect();
    }

    goto execute_done;

execute_done:
    BLOG("EXEC player done move=%s player_status=0x%02X enemy_status=0x%02X missed=%u damage=%u took_turn=%u",
         BMOVE(wPlayerSelectedMove), (unsigned)wBattleMon.status, (unsigned)wEnemyMon.status,
         (unsigned)wMoveMissed, (unsigned)wDamage, (unsigned)wActionResultOrTookBattleTurn);
    wActionResultOrTookBattleTurn = 0;
}

void Battle_ExecuteEnemyMove(void) {
    BLOG("EXEC enemy start move=%s(0x%02X) effect=0x%02X target_pre=%s target_hp_pre=%u/%u",
         BMOVE(wEnemySelectedMove), (unsigned)wEnemySelectedMove, (unsigned)wEnemyMoveEffect,
         BMON_P(), (unsigned)wBattleMon.hp, (unsigned)wBattleMon.max_hp);
    BattleEvent_ResetTurnQueue();
    s_enemy_executed_move = 0;
    s_enemy_status_affected_anim_pending = 0;
    s_enemy_status_anim_id = 0;
    s_enemy_replaced_move = 0;
    s_enemy_announce = BATTLE_ANNOUNCE_USED_MOVE;
    s_last_crit = 0;
    s_enemy_confusion_selfhit_anim_pending = 0;
    s_last_enemy_hit_count = 1;
    s_hit_hp_log_enemy_n = 0;
    s_last_enemy_first_target_hp = 0;

    if (wEnemySelectedMove == CANNOT_MOVE) goto enemy_execute_done;

    if (Battle_IsGhostBattle()) {
        s_enemy_status_msg = BSTAT_MSG_GET_OUT;
        battle_event_push_status_msg(1u, 0u, s_enemy_status_msg);
        wEnemyUsedMove = 0;
        goto enemy_execute_done;
    }

    wAILayer2Encouragement++;

    wMoveMissed    = 0;
    wMoveDidntMiss = 0;
    wDamageMultipliers = DAMAGE_MULT_EFFECTIVE;

    {
        int sr = check_enemy_status_conditions();
        if (sr == PSTAT_DONE)     goto enemy_execute_done;
        if (sr == PSTAT_MISSED)   goto enemy_handle_if_missed;
        if (sr == PSTAT_CALC_DMG) goto enemy_calc_damage;
        if (sr == PSTAT_GET_ANIM) {

            apply_attack_to_player_pokemon();
            wMoveDidntMiss = 1;
            goto enemy_after_apply;
        }
        if (sr == PSTAT_RAGE)     goto enemy_can_execute_move;

    }

    if (wEnemyBattleStatus1 & (1u << BSTAT1_CHARGING_UP)) {
        wEnemyBattleStatus1 &= ~((1u << BSTAT1_CHARGING_UP) |
                                  (1u << BSTAT1_INVULNERABLE));
        goto enemy_can_execute_move;
    }

    get_current_move();

enemy_check_charge:
    if (wEnemyMoveEffect == EFFECT_CHARGE || wEnemyMoveEffect == EFFECT_FLY) {

        s_enemy_executed_move = 1;
        Battle_JumpMoveEffect();
        goto enemy_execute_done;
    }

enemy_can_execute_move:
    wMonIsDisobedient = 0;

    wEnemyUsedMove = wEnemyMoveNum;
    s_enemy_executed_move = 1;

    if (is_in_array(wEnemyMoveEffect, kResidualEffects1)) {
        Battle_JumpMoveEffect();
        goto enemy_execute_done;
    }

    if (is_in_array(wEnemyMoveEffect, kSpecialEffectsCont)) {
        Battle_JumpMoveEffect();
    }

enemy_calc_damage:
    swap_player_enemy_levels();

    if (is_in_array(wEnemyMoveEffect, kSetDamageEffects)) {
        goto enemy_move_hit_test;
    }

    Battle_CriticalHitTest();

    if (!handle_counter_move()) goto enemy_handle_if_missed;

    swap_player_enemy_levels();
    Battle_GetDamageVarsForEnemyAttack();
    swap_player_enemy_levels();

    if (!wDamage) {
        goto enemy_check_fly_charge;
    }
    Battle_AdjustDamageForMoveType();
    Battle_RandomizeDamage();

enemy_move_hit_test:
    Battle_MoveHitTest();

enemy_handle_if_missed:
    if (wMoveMissed) {
        if (wEnemyMoveEffect == EFFECT_EXPLODE) {

            swap_player_enemy_levels();
            goto enemy_mirror_move_check;
        }

        goto enemy_check_fly_charge;
    }

    swap_player_enemy_levels();
    goto enemy_mirror_move_check;

enemy_check_fly_charge:

    swap_player_enemy_levels();

enemy_mirror_move_check:

    if (wEnemyMoveEffect == EFFECT_MIRROR_MOVE) {

        wEnemySelectedMove = wPlayerUsedMove;
        if (wPlayerUsedMove != 0 && wPlayerUsedMove != MOVE_MIRROR_MOVE) {

            s_enemy_replaced_move = wEnemyMoveNum;
            get_current_move();
            increment_move_pp();
            goto enemy_check_charge;
        }
        BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_MIRROR_MOVE_FAILED, 1u, 0u);
        goto enemy_execute_done;
    }
    if (wEnemyMoveEffect == EFFECT_METRONOME) {

        BattleEvent_PushPlayAnim(wEnemyMoveNum , 1u);
        uint8_t pick;
        do { pick = BattleRandom(); }
        while (pick == 0 || pick >= MOVE_STRUGGLE || pick == 118 );
        BattleEvent_PushPlayAnim(pick, 1u);
        s_enemy_replaced_move = wEnemyMoveNum;
        wEnemySelectedMove = pick;
        get_current_move();
        increment_move_pp();
        goto enemy_check_charge;
    }

    if (is_in_array(wEnemyMoveEffect, kResidualEffects2)) {
        Battle_JumpMoveEffect();
        goto enemy_execute_done;
    }

    if (wMoveMissed) {
        battle_event_push_move_result(battle_classify_move_failure_result());

        wCriticalHitOrOHKO = 0;
        BLOG("  %s used %s -- missed!", BMON_E(), BMOVE(wEnemyMoveNum));
        if (wEnemyMoveEffect == EFFECT_JUMP_KICK) {

            wDamage >>= 3;
            if (wDamage == 0u) wDamage = 1u;
            BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_CRASHED, 1u, 0u);
            apply_damage_to_enemy_pokemon();
        }
        if (wEnemyMoveEffect != EFFECT_EXPLODE) goto enemy_execute_done;
    } else {
        BLOG("  %s used %s", BMON_E(), BMOVE(wEnemyMoveNum));

        if (wEnemyMovePower != 0u)
            battle_event_push_hit_sfx((uint8_t)(wDamageMultipliers & 0x7Fu));
        s_multihit_dmg_enemy = wDamage;
        apply_attack_to_player_pokemon();
        if (s_hit_hp_log_enemy_n < BATTLE_HIT_HP_LOG_MAX)
            s_hit_hp_log_enemy[s_hit_hp_log_enemy_n++] = wBattleMon.hp;
        print_critical_ohko_text();
        wMoveDidntMiss = 1;
    }

enemy_after_apply:

    if (is_in_array(wEnemyMoveEffect, kAlwaysHappenSideEffects)) {
        Battle_JumpMoveEffect();
    }

    if (wBattleMon.hp == 0) goto enemy_execute_done;

    handle_building_rage();

    if (wEnemyBattleStatus1 & (1u << BSTAT1_ATTACKING_MULTIPLE)) {
        wEnemyNumAttacksLeft--;
        if (wEnemyNumAttacksLeft > 0) {

            wCriticalHitOrOHKO = 0;

            if (s_last_enemy_hit_count == 1)
                s_last_enemy_first_target_hp = wBattleMon.hp;
            s_last_enemy_hit_count++;
            if (wEnemyMovePower != 0u)
                battle_event_push_hit_sfx((uint8_t)(wDamageMultipliers & 0x7Fu));
            wDamage = s_multihit_dmg_enemy;
            apply_attack_to_player_pokemon();
            if (s_hit_hp_log_enemy_n < BATTLE_HIT_HP_LOG_MAX)
                s_hit_hp_log_enemy[s_hit_hp_log_enemy_n++] = wBattleMon.hp;
            if (wBattleMon.hp == 0) {
                wEnemyBattleStatus1 &= ~(1u << BSTAT1_ATTACKING_MULTIPLE);
                goto enemy_execute_done;
            }

            goto enemy_after_apply;
        }
        wEnemyBattleStatus1 &= ~(1u << BSTAT1_ATTACKING_MULTIPLE);
        BattleEvent_PushEffectMsg(BATTLE_EFFECT_MSG_HIT_N_TIMES, 1u, wEnemyNumHits);
        wEnemyNumHits = 0;
    }

    if (wEnemyMoveEffect == 0) goto enemy_execute_done;

    if (!is_in_array(wEnemyMoveEffect, kSpecialEffects)) {
        Battle_JumpMoveEffect();
    }

enemy_execute_done:
    BLOG("EXEC enemy done move=%s player_status=0x%02X enemy_status=0x%02X missed=%u damage=%u took_turn=%u",
         BMOVE(wEnemySelectedMove), (unsigned)wBattleMon.status, (unsigned)wEnemyMon.status,
         (unsigned)wMoveMissed, (unsigned)wDamage, (unsigned)wActionResultOrTookBattleTurn);

}

static uint16_t poison_decrease_own_hp(battle_mon_t *mon) {
    uint8_t  *bstat3;
    uint8_t  *toxic;

    if (hWhoseTurn == 0) {
        bstat3 = &wPlayerBattleStatus3;
        toxic  = &wPlayerToxicCounter;
    } else {
        bstat3 = &wEnemyBattleStatus3;
        toxic  = &wEnemyToxicCounter;
    }

    uint16_t damage = mon->max_hp / 16;
    if (damage == 0) damage = 1;

    if (*bstat3 & (1u << BSTAT3_BADLY_POISONED)) {
        (*toxic)++;
        damage *= *toxic;
    }

    if (mon->hp <= damage) {
        damage = mon->hp;
        mon->hp = 0;
    } else {
        mon->hp -= (uint16_t)damage;
    }
    return damage;
}

static uint16_t poison_increase_enemy_hp(uint16_t damage) {
    battle_mon_t *opp = (hWhoseTurn == 0) ? &wEnemyMon : &wBattleMon;
    uint16_t old_hp = opp->hp;
    uint16_t new_hp = opp->hp + damage;
    if (new_hp > opp->max_hp) new_hp = opp->max_hp;
    opp->hp = new_hp;
    return (uint16_t)(new_hp - old_hp);
}

int Battle_HandlePoisonBurnLeechSeed(void) {
    BPROBE("HandlePoisonBurnLeechSeed");
    battle_mon_t *mon = (hWhoseTurn == 0) ? &wBattleMon : &wEnemyMon;
    uint8_t side = hWhoseTurn ? 1u : 0u;
    uint8_t opp_side = (uint8_t)(side ^ 1u);

    BLOG("RESIDUAL start turn=%s mon=%s status=0x%02X hp=%u/%u bstat2=0x%02X",
         hWhoseTurn == 0 ? "player" : "enemy",
         side == 0u ? "player" : "enemy",
         (unsigned)mon->status,
         (unsigned)mon->hp, (unsigned)mon->max_hp,
         (unsigned)((hWhoseTurn == 0) ? wPlayerBattleStatus2 : wEnemyBattleStatus2));

    if (mon->status & (STATUS_BRN | STATUS_PSN)) {
        uint16_t dmg = poison_decrease_own_hp(mon);
        BLOG("RESIDUAL status dmg mon=%s dmg=%u hp_now=%u/%u",
             side == 0u ? "player" : "enemy",
             (unsigned)dmg, (unsigned)mon->hp, (unsigned)mon->max_hp);
        if (dmg > 0u) {
            if (mon->status & STATUS_BRN)
                battle_event_push_residual_msg(side, BATTLE_RESIDUAL_MSG_BURN);
            else
                battle_event_push_residual_msg(side, BATTLE_RESIDUAL_MSG_POISON);
            battle_event_push(BATTLE_EVENT_PLAY_ANIM, MOVE_ANIM_BURN_PSN_ID, side, 0u);

            battle_event_push(BATTLE_EVENT_HP_TARGET, (uint8_t)(side ^ 1u),
                              (uint8_t)(mon->hp & 0xFFu),
                              (uint8_t)(0x80u | (uint8_t)(mon->hp >> 8)));
        }
    }

    uint8_t bstat2 = (hWhoseTurn == 0) ? wPlayerBattleStatus2 : wEnemyBattleStatus2;
    if (bstat2 & (1u << BSTAT2_SEEDED)) {

        battle_event_push(BATTLE_EVENT_PLAY_ANIM, MOVE_ANIM_ABSORB_ID, opp_side, 0u);
        uint16_t dmg = poison_decrease_own_hp(mon);
        uint16_t healed = poison_increase_enemy_hp(dmg);
        BLOG("RESIDUAL leech seed mon=%s dmg=%u healed=%u hp_now=%u/%u opp_hp_now=%u/%u",
             side == 0u ? "player" : "enemy",
             (unsigned)dmg, (unsigned)healed,
             (unsigned)mon->hp, (unsigned)mon->max_hp,
             (unsigned)((opp_side == 0u ? wBattleMon.hp : wEnemyMon.hp)),
             (unsigned)((opp_side == 0u ? wBattleMon.max_hp : wEnemyMon.max_hp)));

        if (dmg > 0u)
            battle_event_push(BATTLE_EVENT_HP_TARGET, (uint8_t)(side ^ 1u),
                              (uint8_t)(mon->hp & 0xFFu),
                              (uint8_t)(0x80u | (uint8_t)(mon->hp >> 8)));
        if (healed > 0u) {
            uint16_t ohp = (opp_side == 0u) ? wBattleMon.hp : wEnemyMon.hp;
            battle_event_push(BATTLE_EVENT_HP_TARGET, (uint8_t)(opp_side ^ 1u),
                              (uint8_t)(ohp & 0xFFu),
                              (uint8_t)(0x80u | (uint8_t)(ohp >> 8)));
        }
        battle_event_push_residual_msg(side, BATTLE_RESIDUAL_MSG_LEECH_SEED);
    }

    return (mon->hp != 0) ? 1 : 0;
}

static void faint_enemy_pokemon_state(void) {

    if (wIsInBattle == 2) {
        wEnemyMons[wEnemyMonPartyPos].base.hp = 0;
    }

    wPlayerBideAccumulatedDamage &= 0x00FF;

    wPlayerBattleStatus1 &= ~(1u << BSTAT1_ATTACKING_MULTIPLE);

    wEnemyBattleStatus1  = 0;
    wEnemyBattleStatus2  = 0;
    wEnemyBattleStatus3  = 0;

    wEnemyDisabledMove        = 0;
    wEnemyDisabledMoveNumber  = 0;
    wEnemyMonMinimized        = 0;

    wPlayerUsedMove = 0;
    wEnemyUsedMove  = 0;
}

static void remove_fainted_player_mon_state(void) {

    wPartyMons[wPlayerMonNumber].base.hp     = wBattleMon.hp;
    wPartyMons[wPlayerMonNumber].base.status = wBattleMon.status;

    wEnemyBattleStatus1 &= ~(1u << BSTAT1_ATTACKING_MULTIPLE);

    wEnemyBideAccumulatedDamage = 0;

    wBattleMon.status = 0;
    wPartyMons[wPlayerMonNumber].base.status = 0;
}

void Battle_HandleEnemyMonFainted(void) {
    BLOG("  %s fainted!", BMON_E());
    wInHandlePlayerMonFainted = 0;
    faint_enemy_pokemon_state();

    Battle_AwardExpForFaintedEnemy();

    if (!Battle_AnyPartyAlive()) {
        Battle_HandlePlayerBlackOut();
        return;
    }

    if (wIsInBattle != 2) {
        wBattleResult = BATTLE_OUTCOME_WILD_VICTORY;
        return;
    }

    if (!Battle_AnyEnemyPokemonAliveCheck()) {
        Battle_TrainerBattleVictory();
        return;
    }

    if (wBattleMon.hp == 0) {
        wForcePlayerToChooseMon = 1;
    }

    Battle_ReplaceFaintedEnemyMon();
}

battle_status_msg_t Battle_GetPlayerStatusMsg(void)    { return s_player_status_msg; }
battle_status_msg_t Battle_GetPlayerPreStatusMsg(void) { return s_player_pre_msg;    }
battle_status_msg_t Battle_GetEnemyStatusMsg(void)     { return s_enemy_status_msg;  }
battle_status_msg_t Battle_GetEnemyPreStatusMsg(void)  { return s_enemy_pre_msg;     }

uint8_t Battle_GetHitHpLog(int whose, const uint16_t **out) {
    if (whose == 0) { if (out) *out = s_hit_hp_log_player; return s_hit_hp_log_player_n; }
    if (out) *out = s_hit_hp_log_enemy;
    return s_hit_hp_log_enemy_n;
}
uint8_t  Battle_GetLastPlayerHitCount(void)            { return s_last_player_hit_count; }
uint16_t Battle_GetLastPlayerFirstTargetHP(void)       { return s_last_player_first_target_hp; }
uint8_t  Battle_GetLastEnemyHitCount(void)             { return s_last_enemy_hit_count; }
uint16_t Battle_GetLastEnemyFirstTargetHP(void)        { return s_last_enemy_first_target_hp; }
uint8_t Battle_GetPlayerStatusAffectedAnimPending(void){ return s_player_status_affected_anim_pending; }
uint8_t Battle_GetEnemyStatusAffectedAnimPending(void) { return s_enemy_status_affected_anim_pending;  }
uint8_t Battle_GetPlayerStatusAnimId(void)             { return s_player_status_anim_id; }
uint8_t Battle_GetEnemyStatusAnimId(void)              { return s_enemy_status_anim_id;  }
uint8_t Battle_GetPlayerConfusionSelfHitAnimPending(void){ return s_player_confusion_selfhit_anim_pending; }
uint8_t Battle_GetEnemyConfusionSelfHitAnimPending(void) { return s_enemy_confusion_selfhit_anim_pending;  }

void Battle_HandlePlayerMonFainted(void) {
    BLOG("  %s fainted!", BMON_P());
    wInHandlePlayerMonFainted = 1;
    remove_fainted_player_mon_state();

    if (wEnemyMon.hp == 0) {
        faint_enemy_pokemon_state();
    }
}
