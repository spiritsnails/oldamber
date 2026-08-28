
#include "test_runner.h"
#include "../src/game/battle/battle.h"
#include "../src/game/battle/battle_core.h"
#include "../src/game/battle/battle_effects.h"
#include "../src/platform/hardware.h"
#include "../src/game/constants.h"

static void battle_reset(void) {
    extern void WRAMClear(void);
    WRAMClear();
    hRandomAdd = 0x00;
    hRandomSub = 0xFD;
    hWhoseTurn = 0;

    wBattleMon.atk = 100;  wBattleMon.def = 80;
    wBattleMon.spd = 0;    wBattleMon.spc = 70;
    wBattleMon.max_hp = 200; wBattleMon.hp = 200;
    wBattleMon.level = 50;

    wEnemyMon.atk = 100;  wEnemyMon.def = 80;
    wEnemyMon.spd = 0;    wEnemyMon.spc = 70;
    wEnemyMon.max_hp = 200; wEnemyMon.hp = 200;
    wEnemyMon.level = 50;

    wPlayerSelectedMove = 1;
    wPlayerMoveNum      = 1;
    wPlayerMoveEffect   = EFFECT_NONE;
    wPlayerMovePower    = 80;
    wPlayerMoveType     = TYPE_NORMAL;
    wPlayerMoveAccuracy = 255;
    wPlayerMoveMaxPP    = 35;

    wEnemySelectedMove  = 1;
    wEnemyMoveNum       = 1;
    wEnemyMoveEffect    = EFFECT_NONE;
    wEnemyMovePower     = 80;
    wEnemyMoveType      = TYPE_NORMAL;
    wEnemyMoveAccuracy  = 255;
    wEnemyMoveMaxPP     = 35;

    wDamageMultipliers  = DAMAGE_MULT_EFFECTIVE;
}

static void enable_x_accuracy(void) {
    wPlayerBattleStatus2 |= (1u << BSTAT2_USING_X_ACCURACY);
}

static void seed_never_trigger(void) {
    hRandomAdd = 0x00;
    hRandomSub = 0xFD;
}

static void seed_always_trigger(void) {
    hRandomAdd = 0xFB;
    hRandomSub = 0x08;

    hRandomAdd = 0x00;
    hRandomSub = 0x07;
}

TEST(HandlePoisonBurnLeechSeed, no_status_no_damage) {
    battle_reset();
    hWhoseTurn = 0;
    int alive = Battle_HandlePoisonBurnLeechSeed();
    EXPECT_EQ(alive, 1);
    EXPECT_EQ((int)wBattleMon.hp, 200);
}

TEST(HandlePoisonBurnLeechSeed, poison_reduces_hp_by_sixteenth) {
    battle_reset();
    hWhoseTurn = 0;
    wBattleMon.status = STATUS_PSN;
    wBattleMon.max_hp = 160;
    wBattleMon.hp = 160;
    int alive = Battle_HandlePoisonBurnLeechSeed();

    EXPECT_EQ((int)wBattleMon.hp, 150);
    EXPECT_EQ(alive, 1);
}

TEST(HandlePoisonBurnLeechSeed, burn_reduces_hp_by_sixteenth) {
    battle_reset();
    hWhoseTurn = 0;
    wBattleMon.status = STATUS_BRN;
    wBattleMon.max_hp = 160;
    wBattleMon.hp = 160;
    Battle_HandlePoisonBurnLeechSeed();
    EXPECT_EQ((int)wBattleMon.hp, 150);
}

TEST(HandlePoisonBurnLeechSeed, min_damage_one) {
    battle_reset();
    hWhoseTurn = 0;
    wBattleMon.status = STATUS_PSN;
    wBattleMon.max_hp = 15;
    wBattleMon.hp = 15;
    Battle_HandlePoisonBurnLeechSeed();
    EXPECT_EQ((int)wBattleMon.hp, 14);
}

TEST(HandlePoisonBurnLeechSeed, toxic_counter_increments_and_scales_damage) {
    battle_reset();
    hWhoseTurn = 0;
    wBattleMon.status = STATUS_PSN;
    wPlayerBattleStatus3 |= (1u << BSTAT3_BADLY_POISONED);
    wPlayerToxicCounter = 1;
    wBattleMon.max_hp = 160;
    wBattleMon.hp = 160;
    Battle_HandlePoisonBurnLeechSeed();

    EXPECT_EQ((int)wPlayerToxicCounter, 2);
    EXPECT_EQ((int)wBattleMon.hp, 140);
}

TEST(HandlePoisonBurnLeechSeed, faint_returns_zero) {
    battle_reset();
    hWhoseTurn = 0;
    wBattleMon.status = STATUS_PSN;
    wBattleMon.max_hp = 16;
    wBattleMon.hp = 1;
    int alive = Battle_HandlePoisonBurnLeechSeed();
    EXPECT_EQ((int)wBattleMon.hp, 0);
    EXPECT_EQ(alive, 0);
}

TEST(HandlePoisonBurnLeechSeed, leech_seed_damages_and_heals_enemy) {
    battle_reset();
    hWhoseTurn = 0;

    wPlayerBattleStatus2 |= (1u << BSTAT2_SEEDED);
    wBattleMon.max_hp = 160;
    wBattleMon.hp = 160;
    wEnemyMon.max_hp = 200;
    wEnemyMon.hp = 100;
    Battle_HandlePoisonBurnLeechSeed();

    EXPECT_EQ((int)wBattleMon.hp, 150);
    EXPECT_EQ((int)wEnemyMon.hp, 110);
}

TEST(HandlePoisonBurnLeechSeed, leech_seed_heal_capped_at_max) {
    battle_reset();
    hWhoseTurn = 0;
    wPlayerBattleStatus2 |= (1u << BSTAT2_SEEDED);
    wBattleMon.max_hp = 160;
    wBattleMon.hp = 160;
    wEnemyMon.max_hp = 200;
    wEnemyMon.hp = 195;
    Battle_HandlePoisonBurnLeechSeed();

    EXPECT_EQ((int)wEnemyMon.hp, 200);
}

TEST(HandlePoisonBurnLeechSeed, enemy_side_hwhosturn_1) {
    battle_reset();
    hWhoseTurn = 1;
    wEnemyMon.status = STATUS_PSN;
    wEnemyMon.max_hp = 160;
    wEnemyMon.hp = 160;
    Battle_HandlePoisonBurnLeechSeed();
    EXPECT_EQ((int)wEnemyMon.hp, 150);
    EXPECT_EQ((int)wBattleMon.hp, 200);
}

TEST(HandleEnemyMonFainted, clears_enemy_battle_statuses) {
    battle_reset();
    wEnemyBattleStatus1 = 0xFF;
    wEnemyBattleStatus2 = 0xFF;
    wEnemyBattleStatus3 = 0xFF;
    Battle_HandleEnemyMonFainted();
    EXPECT_EQ((int)wEnemyBattleStatus1, 0);
    EXPECT_EQ((int)wEnemyBattleStatus2, 0);
    EXPECT_EQ((int)wEnemyBattleStatus3, 0);
}

TEST(HandleEnemyMonFainted, clears_disabled_and_minimized) {
    battle_reset();
    wEnemyDisabledMove       = 0x35;
    wEnemyDisabledMoveNumber = 0x02;
    wEnemyMonMinimized       = 1;
    Battle_HandleEnemyMonFainted();
    EXPECT_EQ((int)wEnemyDisabledMove, 0);
    EXPECT_EQ((int)wEnemyDisabledMoveNumber, 0);
    EXPECT_EQ((int)wEnemyMonMinimized, 0);
}

TEST(HandleEnemyMonFainted, sets_wInHandlePlayerMonFainted_to_zero) {
    battle_reset();
    wInHandlePlayerMonFainted = 1;
    Battle_HandleEnemyMonFainted();
    EXPECT_EQ((int)wInHandlePlayerMonFainted, 0);
}

TEST(HandleEnemyMonFainted, clears_player_multi_hit_flag) {
    battle_reset();
    wPlayerBattleStatus1 |= (1u << BSTAT1_ATTACKING_MULTIPLE);
    Battle_HandleEnemyMonFainted();
    EXPECT_EQ((int)(wPlayerBattleStatus1 & (1u << BSTAT1_ATTACKING_MULTIPLE)), 0);
}

TEST(HandleEnemyMonFainted, bide_bug_only_zeros_high_byte) {
    battle_reset();

    wPlayerBideAccumulatedDamage = 0x01FF;
    Battle_HandleEnemyMonFainted();

    EXPECT_EQ((int)wPlayerBideAccumulatedDamage, 0x00FF);
}

TEST(HandleEnemyMonFainted, clears_used_move_tracking) {
    battle_reset();
    wPlayerUsedMove = 0x12;
    wEnemyUsedMove  = 0x34;
    Battle_HandleEnemyMonFainted();
    EXPECT_EQ((int)wPlayerUsedMove, 0);
    EXPECT_EQ((int)wEnemyUsedMove,  0);
}

TEST(HandlePlayerMonFainted, sets_wInHandlePlayerMonFainted_to_one) {
    battle_reset();
    wInHandlePlayerMonFainted = 0;
    Battle_HandlePlayerMonFainted();
    EXPECT_EQ((int)wInHandlePlayerMonFainted, 1);
}

TEST(HandlePlayerMonFainted, clears_player_status) {
    battle_reset();
    wBattleMon.status = STATUS_PSN;
    Battle_HandlePlayerMonFainted();
    EXPECT_EQ((int)wBattleMon.status, 0);
}

TEST(HandlePlayerMonFainted, clears_enemy_bide_both_bytes) {
    battle_reset();
    wEnemyBideAccumulatedDamage = 0x01FF;
    Battle_HandlePlayerMonFainted();

    EXPECT_EQ((int)wEnemyBideAccumulatedDamage, 0);
}

TEST(HandlePlayerMonFainted, clears_enemy_attacking_multiple) {
    battle_reset();
    wEnemyBattleStatus1 |= (1u << BSTAT1_ATTACKING_MULTIPLE);
    Battle_HandlePlayerMonFainted();
    EXPECT_EQ((int)(wEnemyBattleStatus1 & (1u << BSTAT1_ATTACKING_MULTIPLE)), 0);
}

TEST(HandlePlayerMonFainted, simultaneous_faint_triggers_enemy_faint_state) {
    battle_reset();
    wEnemyMon.hp = 0;
    wEnemyBattleStatus1 = 0xFF;
    wEnemyBattleStatus2 = 0xFF;
    Battle_HandlePlayerMonFainted();

    EXPECT_EQ((int)wInHandlePlayerMonFainted, 1);

    EXPECT_EQ((int)wEnemyBattleStatus1, 0);
    EXPECT_EQ((int)wEnemyBattleStatus2, 0);
}

TEST(HandlePlayerMonFainted, no_enemy_faint_if_enemy_alive) {
    battle_reset();
    wEnemyMon.hp = 50;
    wEnemyBattleStatus1 = 0xFF;
    Battle_HandlePlayerMonFainted();

    EXPECT_NE((int)wEnemyBattleStatus1, 0);
}

TEST(ExecutePlayerMove, cannot_move_skips_execution) {
    battle_reset();
    wPlayerSelectedMove = CANNOT_MOVE;
    Battle_ExecutePlayerMove();
    EXPECT_EQ((int)wEnemyMon.hp, 200);
}

TEST(ExecutePlayerMove, action_taken_flag_skips_execution) {
    battle_reset();
    enable_x_accuracy();
    wActionResultOrTookBattleTurn = 1;
    Battle_ExecutePlayerMove();
    EXPECT_EQ((int)wEnemyMon.hp, 200);

    EXPECT_EQ((int)wActionResultOrTookBattleTurn, 0);
}

TEST(ExecutePlayerMove, sleep_prevents_move) {
    battle_reset();
    wBattleMon.status = 3;
    wPlayerUsedMove = 0x42;
    Battle_ExecutePlayerMove();
    EXPECT_EQ((int)wEnemyMon.hp, 200);
    EXPECT_EQ((int)wPlayerUsedMove, 0);

    EXPECT_EQ((int)(wBattleMon.status & STATUS_SLP_MASK), 2);
}

TEST(ExecutePlayerMove, frozen_prevents_move) {
    battle_reset();
    wBattleMon.status = STATUS_FRZ;
    wPlayerUsedMove = 0x01;
    Battle_ExecutePlayerMove();
    EXPECT_EQ((int)wEnemyMon.hp, 200);
    EXPECT_EQ((int)wPlayerUsedMove, 0);
}

TEST(ExecutePlayerMove, flinch_prevents_move_and_clears_flag) {
    battle_reset();
    wPlayerBattleStatus1 |= (1u << BSTAT1_FLINCHED);
    Battle_ExecutePlayerMove();
    EXPECT_EQ((int)wEnemyMon.hp, 200);
    EXPECT_EQ((int)(wPlayerBattleStatus1 & (1u << BSTAT1_FLINCHED)), 0);
}

TEST(ExecutePlayerMove, hyper_beam_recharge_prevents_move_and_clears_flag) {
    battle_reset();
    wPlayerBattleStatus2 |= (1u << BSTAT2_NEEDS_TO_RECHARGE);
    Battle_ExecutePlayerMove();
    EXPECT_EQ((int)wEnemyMon.hp, 200);
    EXPECT_EQ((int)(wPlayerBattleStatus2 & (1u << BSTAT2_NEEDS_TO_RECHARGE)), 0);
}

TEST(ExecutePlayerMove, splash_does_no_damage) {
    battle_reset();

    wPlayerSelectedMove = 0x60;
    Battle_ExecutePlayerMove();
    EXPECT_EQ((int)wEnemyMon.hp, 200);
}

TEST(ExecutePlayerMove, normal_move_deals_damage) {
    battle_reset();
    enable_x_accuracy();

    wPlayerMoveEffect = EFFECT_NONE;
    wPlayerMovePower  = 80;
    wPlayerMoveType   = TYPE_NORMAL;
    Battle_ExecutePlayerMove();
    EXPECT_LT((int)wEnemyMon.hp, 200);
}

TEST(ExecutePlayerMove, zero_power_move_no_damage) {
    battle_reset();

    wPlayerBattleStatus1 |= (1u << BSTAT1_THRASHING_ABOUT);
    wPlayerNumAttacksLeft = 2;
    wPlayerMoveEffect = EFFECT_NONE;
    wPlayerMovePower  = 0;
    Battle_ExecutePlayerMove();
    EXPECT_EQ((int)wEnemyMon.hp, 200);
}

TEST(ExecutePlayerMove, resets_action_result_at_end) {
    battle_reset();
    enable_x_accuracy();
    wActionResultOrTookBattleTurn = 0;
    Battle_ExecutePlayerMove();

    EXPECT_EQ((int)wActionResultOrTookBattleTurn, 0);
}

TEST(ExecutePlayerMove, paralysis_triggers_with_low_rng) {
    battle_reset();

    seed_always_trigger();
    wBattleMon.status = STATUS_PAR;
    wPlayerMoveEffect = EFFECT_NONE;
    wPlayerMovePower  = 80;
    Battle_ExecutePlayerMove();

    EXPECT_EQ((int)wEnemyMon.hp, 200);
}

TEST(ExecutePlayerMove, paralysis_does_not_trigger_with_high_rng) {
    battle_reset();

    seed_never_trigger();
    enable_x_accuracy();
    wBattleMon.status = STATUS_PAR;
    wPlayerMoveEffect = EFFECT_NONE;
    wPlayerMovePower  = 80;
    Battle_ExecutePlayerMove();

    EXPECT_LT((int)wEnemyMon.hp, 200);
}

TEST(ExecutePlayerMove, bide_accumulates_and_does_not_deal_damage) {
    battle_reset();
    wPlayerBattleStatus1 |= (1u << BSTAT1_STORING_ENERGY);
    wPlayerNumAttacksLeft = 2;
    wDamage = 30;
    Battle_ExecutePlayerMove();

    EXPECT_EQ((int)wEnemyMon.hp, 200);
    EXPECT_GT((int)wPlayerBideAccumulatedDamage, 0);
    EXPECT_EQ((int)wPlayerNumAttacksLeft, 1);
}

TEST(ExecutePlayerMove, stat_up_effect_dispatches_via_residual2) {
    battle_reset();

    wPlayerBattleStatus1 |= (1u << BSTAT1_THRASHING_ABOUT);
    wPlayerNumAttacksLeft = 2;
    wPlayerMoveEffect = EFFECT_ATTACK_UP1;
    wPlayerMovePower  = 0;
    uint8_t initial_stage = wPlayerMonStatMods[MOD_ATTACK];
    Battle_ExecutePlayerMove();
    EXPECT_GT((int)wPlayerMonStatMods[MOD_ATTACK], (int)initial_stage);
    EXPECT_EQ((int)wEnemyMon.hp, 200);
}

TEST(ExecutePlayerMove, leer_emits_stat_mod_text_event) {
    battle_reset();
    enable_x_accuracy();
    wPlayerSelectedMove = MOVE_LEER;

    uint8_t initial_stage = wEnemyMonStatMods[MOD_DEFENSE];
    Battle_ExecutePlayerMove();

    EXPECT_LT((int)wEnemyMonStatMods[MOD_DEFENSE], (int)initial_stage);

    battle_event_t ev;
    int found = 0;
    while (BattleEvent_Pop(&ev)) {
        if (ev.type == BATTLE_EVENT_STAT_MOD_TEXT) {
            found = 1;
            EXPECT_EQ(ev.arg0, MOD_DEFENSE);
            EXPECT_EQ(ev.arg1, 1);
            EXPECT_EQ(ev.arg2 & 1, 1);
            EXPECT_EQ(ev.arg2 & 2, 0);
        }
    }
    EXPECT_TRUE(found);
}

TEST(ExecutePlayerMove, thunder_wave_sets_enemy_paralysis_status) {
    battle_reset();
    enable_x_accuracy();
    wPlayerSelectedMove = 0x56;
    wEnemyMon.type1 = TYPE_NORMAL;
    wEnemyMon.type2 = TYPE_NORMAL;

    Battle_ExecutePlayerMove();

    EXPECT_TRUE(IS_PARALYZED(wEnemyMon.status));
}

TEST(ExecuteEnemyMove, cannot_move_skips) {
    battle_reset();
    hWhoseTurn = 1;
    wEnemySelectedMove = CANNOT_MOVE;
    Battle_ExecuteEnemyMove();
    EXPECT_EQ((int)wBattleMon.hp, 200);
}

TEST(ExecuteEnemyMove, sleep_prevents_move) {
    battle_reset();
    hWhoseTurn = 1;
    wEnemyMon.status = 2;
    Battle_ExecuteEnemyMove();
    EXPECT_EQ((int)wBattleMon.hp, 200);
    EXPECT_EQ((int)wEnemyUsedMove, 0);
    EXPECT_EQ((int)(wEnemyMon.status & STATUS_SLP_MASK), 1);
}

TEST(ExecuteEnemyMove, normal_move_deals_damage_to_player) {
    battle_reset();
    hWhoseTurn = 1;

    wEnemyBattleStatus2 |= (1u << BSTAT2_USING_X_ACCURACY);
    wEnemyMoveEffect = EFFECT_NONE;
    wEnemyMovePower  = 80;
    wEnemyMoveType   = TYPE_NORMAL;
    wEnemyMon.spd    = 0;
    Battle_ExecuteEnemyMove();
    EXPECT_LT((int)wBattleMon.hp, 200);
}

TEST(ExecuteEnemyMove, frozen_prevents_move) {
    battle_reset();
    hWhoseTurn = 1;
    wEnemyMon.status = STATUS_FRZ;
    Battle_ExecuteEnemyMove();
    EXPECT_EQ((int)wBattleMon.hp, 200);
    EXPECT_EQ((int)wEnemyUsedMove, 0);
}

TEST(HandlePoisonBurnLeechSeed, poison_and_leech_both_trigger) {
    battle_reset();
    hWhoseTurn = 0;
    wBattleMon.status = STATUS_PSN;
    wPlayerBattleStatus2 |= (1u << BSTAT2_SEEDED);
    wBattleMon.max_hp = 160;
    wBattleMon.hp = 160;
    wEnemyMon.hp = 50;
    wEnemyMon.max_hp = 200;
    Battle_HandlePoisonBurnLeechSeed();

    EXPECT_EQ((int)wBattleMon.hp, 140);
    EXPECT_EQ((int)wEnemyMon.hp, 60);
}

TEST(DecrementPP, enemy_pp_decrements) {
    battle_reset();
    hWhoseTurn = 1;
    wEnemyMon.pp[0]    = 10;
    wEnemyMoveListIndex = 0;
    wEnemyBattleStatus2 |= (1u << BSTAT2_USING_X_ACCURACY);
    Battle_ExecuteEnemyMove();
    EXPECT_EQ((int)wEnemyMon.pp[0], 9);
}

TEST(DecrementPP, player_pp_decrements) {
    battle_reset();
    hWhoseTurn = 0;
    wBattleMon.pp[0]     = 10;
    wPlayerMoveListIndex  = 0;
    wPlayerBattleStatus2 |= (1u << BSTAT2_USING_X_ACCURACY);
    Battle_ExecutePlayerMove();
    EXPECT_EQ((int)wBattleMon.pp[0], 9);
}

TEST(DecrementPP, pp_up_bits_preserved) {
    battle_reset();
    hWhoseTurn = 1;

    wEnemyMon.pp[0]    = (1u << 6) | 5;
    wEnemyMoveListIndex = 0;
    wEnemyBattleStatus2 |= (1u << BSTAT2_USING_X_ACCURACY);
    Battle_ExecuteEnemyMove();

    EXPECT_EQ((int)(wEnemyMon.pp[0] >> 6), 1);
    EXPECT_EQ((int)(wEnemyMon.pp[0] & 0x3F), 4);
}

TEST(DecrementPP, struggle_no_decrement) {
    battle_reset();
    hWhoseTurn = 1;
    wEnemyMon.pp[0]     = 5;
    wEnemyMoveListIndex  = 0;
    wEnemySelectedMove   = MOVE_STRUGGLE;
    wEnemyBattleStatus2 |= (1u << BSTAT2_USING_X_ACCURACY);
    Battle_ExecuteEnemyMove();
    EXPECT_EQ((int)wEnemyMon.pp[0], 5);
}
