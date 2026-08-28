
#include "battle_items.h"
#include "battle_catch.h"
#include "battle_effects.h"
#include "../../platform/hardware.h"
#include "../constants.h"
#include "../pokedex.h"
#include <stdio.h>

static item_use_result_t use_ball(uint8_t item_id);
static item_use_result_t use_medicine(uint8_t item_id, uint8_t slot);
static item_use_result_t use_x_stat(uint8_t item_id);

item_use_result_t Battle_UseItem(uint8_t item_id, uint8_t target_slot) {

    if (item_id >= ITEM_HM01) return ITEM_USE_CANNOT_USE;

    switch (item_id) {

    case ITEM_MASTER_BALL:
    case ITEM_ULTRA_BALL:
    case ITEM_GREAT_BALL:
    case ITEM_POKE_BALL:
    case ITEM_SAFARI_BALL:
        return use_ball(item_id);

    case ITEM_ANTIDOTE:
    case ITEM_BURN_HEAL:
    case ITEM_ICE_HEAL:
    case ITEM_AWAKENING:
    case ITEM_PARLYZ_HEAL:
    case ITEM_FULL_HEAL:

    case ITEM_FULL_RESTORE:
    case ITEM_MAX_POTION:
    case ITEM_HYPER_POTION:
    case ITEM_SUPER_POTION:
    case ITEM_POTION:
    case ITEM_FRESH_WATER:
    case ITEM_SODA_POP:
    case ITEM_LEMONADE:

    case ITEM_REVIVE:
    case ITEM_MAX_REVIVE:
        return use_medicine(item_id, target_slot);

    case ITEM_X_ACCURACY:

        wPlayerBattleStatus2 |= (1 << BSTAT2_USING_X_ACCURACY);
        printf("[item]   Used X Accuracy! Player attacks can't miss.\n");
        return ITEM_USE_OK;

    case ITEM_GUARD_SPEC:

        wPlayerBattleStatus2 |= (1 << BSTAT2_PROTECTED_BY_MIST);
        printf("[item]   Used Guard Spec.! Player protected from stat reductions.\n");
        return ITEM_USE_OK;

    case ITEM_DIRE_HIT:

        wPlayerBattleStatus2 |= (1 << BSTAT2_GETTING_PUMPED);
        printf("[item]   Used Dire Hit! Player is getting pumped.\n");
        return ITEM_USE_OK;

    case ITEM_X_ATTACK:
    case ITEM_X_DEFEND:
    case ITEM_X_SPEED:
    case ITEM_X_SPECIAL:
        return use_x_stat(item_id);

    case ITEM_POKE_DOLL:

        if (wIsInBattle != 1) return ITEM_USE_CANNOT_USE;
        wEscapedFromBattle = 1;
        printf("[item]   Used Poké Doll! Fled from battle.\n");
        return ITEM_USE_FLED;

    case ITEM_POKE_FLUTE: {

        int any_asleep = 0;
        for (int i = 0; i < wPartyCount; i++) {
            if (wPartyMons[i].base.status & STATUS_SLP_MASK) any_asleep = 1;
            wPartyMons[i].base.status &= (uint8_t)~STATUS_SLP_MASK;
        }
        if (wIsInBattle == 2) {
            for (int i = 0; i < PARTY_LENGTH; i++) {
                if (wEnemyMons[i].base.status & STATUS_SLP_MASK) any_asleep = 1;
                wEnemyMons[i].base.status &= (uint8_t)~STATUS_SLP_MASK;
            }
        }

        wBattleMon.status &= (uint8_t)~STATUS_SLP_MASK;
        wEnemyMon.status  &= (uint8_t)~STATUS_SLP_MASK;
        return any_asleep ? ITEM_USE_OK : ITEM_USE_FAILED;
    }

    default:
        return ITEM_USE_CANNOT_USE;
    }
}

static item_use_result_t use_ball(uint8_t item_id) {
    if (wIsInBattle != 1) {

        printf("[item]   Can't use a ball against a trainer's Monster!\n");
        return ITEM_USE_CANNOT_USE;
    }

    catch_result_t result = Battle_CatchAttempt(item_id);

    switch (result) {
    case CATCH_RESULT_SUCCESS:
        Pokedex_SetOwned(wEnemyMon.species);
        printf("[item]   Gotcha! Enemy Monster was caught!\n");
        return ITEM_USE_CAUGHT;
    case CATCH_RESULT_0_SHAKES:
        printf("[item]   Oh no! The Monster broke free! (0 shakes)\n");
        break;
    case CATCH_RESULT_1_SHAKE:
        printf("[item]   Darn! Almost had it! (1 shake)\n");
        break;
    case CATCH_RESULT_2_SHAKES:
        printf("[item]   Argh! So close! (2 shakes)\n");
        break;
    case CATCH_RESULT_3_SHAKES:
        printf("[item]   Shoot! It was so close too! (3 shakes)\n");
        break;
    case CATCH_RESULT_CANNOT_CATCH:
        printf("[item]   The Monster can't be caught!\n");
        break;
    }

    return ITEM_USE_OK;
}

static uint8_t  s_med_msg_id;
static uint16_t s_med_old_hp;
static uint16_t s_med_new_hp;

uint8_t  Battle_GetMedicineMsg(void)   { return s_med_msg_id; }
uint16_t Battle_GetMedicineOldHP(void) { return s_med_old_hp; }
uint16_t Battle_GetMedicineNewHP(void) { return s_med_new_hp; }

static item_use_result_t cure_status(uint8_t item_id, uint8_t slot, party_mon_t *p) {
    uint8_t mask, msg;
    switch (item_id) {
    case ITEM_ANTIDOTE:    mask = STATUS_PSN;      msg = MEDICINE_MSG_ANTIDOTE;    break;
    case ITEM_BURN_HEAL:   mask = STATUS_BRN;      msg = MEDICINE_MSG_BURN_HEAL;   break;
    case ITEM_ICE_HEAL:    mask = STATUS_FRZ;      msg = MEDICINE_MSG_ICE_HEAL;    break;
    case ITEM_AWAKENING:   mask = STATUS_SLP_MASK; msg = MEDICINE_MSG_AWAKENING;   break;
    case ITEM_PARLYZ_HEAL: mask = STATUS_PAR;      msg = MEDICINE_MSG_PARLYZ_HEAL; break;
    default:
                           mask = 0xFF;            msg = MEDICINE_MSG_FULL_HEAL;   break;
    }

    if (!(p->base.status & mask)) {
        printf("[item]   It won't have any effect.\n");
        return ITEM_USE_FAILED;
    }

    p->base.status = 0;
    s_med_msg_id   = msg;

    if (slot == wPlayerMonNumber) {

        wBattleMon.status = 0;
        wPlayerBattleStatus3 &= (uint8_t)~(1 << BSTAT3_BADLY_POISONED);
        wBattleMon.max_hp = p->max_hp;
        wBattleMon.atk    = p->atk;
        wBattleMon.def    = p->def;
        wBattleMon.spd    = p->spd;
        wBattleMon.spc    = p->spc;
    }
    printf("[item]   Status cured.\n");
    return ITEM_USE_OK;
}

static item_use_result_t heal_hp(uint8_t item_id, uint8_t slot, party_mon_t *p) {
    s_med_old_hp = p->base.hp;

    if (p->base.hp == 0) {

        if (item_id != ITEM_REVIVE && item_id != ITEM_MAX_REVIVE) {
            printf("[item]   It won't have any effect.\n");
            return ITEM_USE_FAILED;
        }

        if (wIsInBattle && (wPartyFoughtCurrentEnemyFlags & (1u << slot)))
            wPartyGainExpFlags |= (uint8_t)(1u << slot);
    } else {

        if (item_id == ITEM_REVIVE || item_id == ITEM_MAX_REVIVE) {
            printf("[item]   It won't have any effect.\n");
            return ITEM_USE_FAILED;
        }
    }

    if (p->base.hp == p->max_hp) {
        if (item_id != ITEM_FULL_RESTORE) {
            printf("[item]   It won't have any effect.\n");
            return ITEM_USE_FAILED;
        }
        if (p->base.status == 0) {
            printf("[item]   It won't have any effect.\n");
            return ITEM_USE_FAILED;
        }

        return cure_status(ITEM_FULL_HEAL, slot, p);
    }

    if (item_id == ITEM_REVIVE) {

        p->base.hp = (uint16_t)(p->max_hp >> 1);
    } else if (item_id == ITEM_FULL_RESTORE || item_id == ITEM_MAX_POTION ||
               item_id == ITEM_MAX_REVIVE) {

        p->base.hp = p->max_hp;
    } else {

        uint8_t b;
        if      (item_id == ITEM_SODA_POP)     b = 60;
        else if (item_id >  ITEM_SODA_POP)     b = 80;
        else if (item_id == ITEM_FRESH_WATER)  b = 50;
        else if (item_id <  ITEM_SUPER_POTION) b = 200;
        else if (item_id == ITEM_SUPER_POTION) b = 50;
        else                                    b = 20;
        uint32_t nh = (uint32_t)p->base.hp + b;
        p->base.hp = (uint16_t)(nh > p->max_hp ? p->max_hp : nh);
    }

    if (item_id == ITEM_FULL_RESTORE)
        p->base.status = 0;

    s_med_new_hp = p->base.hp;
    s_med_msg_id = (item_id == ITEM_REVIVE || item_id == ITEM_MAX_REVIVE)
                   ? MEDICINE_MSG_REVIVE : MEDICINE_MSG_POTION;

    if (slot == wPlayerMonNumber) {
        wBattleMon.hp = p->base.hp;
        if (item_id == ITEM_FULL_RESTORE)
            wBattleMon.status = 0;
    }

    printf("[item]   HP now %d/%d.\n", (int)p->base.hp, (int)p->max_hp);
    return ITEM_USE_OK;
}

static item_use_result_t use_medicine(uint8_t item_id, uint8_t slot) {
    if (slot >= wPartyCount) return ITEM_USE_FAILED;
    party_mon_t *p = &wPartyMons[slot];

    if (wIsInBattle && slot == (uint8_t)wPlayerMonNumber) {
        p->base.hp     = wBattleMon.hp;
        p->base.status = wBattleMon.status;
    }

    if ((item_id >= ITEM_ANTIDOTE && item_id <= ITEM_PARLYZ_HEAL) ||
        item_id == ITEM_FULL_HEAL)
        return cure_status(item_id, slot, p);
    return heal_hp(item_id, slot, p);
}

static item_use_result_t use_x_stat(uint8_t item_id) {
    uint8_t stat_idx = (uint8_t)(item_id - ITEM_X_ATTACK);
    uint8_t before = wPlayerMonStatMods[stat_idx];

    uint8_t saved_move_num    = wPlayerMoveNum;
    uint8_t saved_move_effect = wPlayerMoveEffect;

    hWhoseTurn = 0;
    wPlayerMoveNum    = 0;
    wPlayerMoveEffect = (uint8_t)(EFFECT_ATTACK_UP1 + stat_idx);
    Battle_StatModifierUpEffect();

    wPlayerMoveNum    = saved_move_num;
    wPlayerMoveEffect = saved_move_effect;

    if (wPlayerMonStatMods[stat_idx] == before) {
        printf("[item]   It won't have any effect.\n");
        return ITEM_USE_FAILED;
    }
    printf("[item]   Player's stat (idx %d) rose! Stage now %d.\n",
           (int)stat_idx, (int)wPlayerMonStatMods[stat_idx]);
    return ITEM_USE_OK;
}
