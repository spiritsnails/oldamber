
#include "test_runner.h"
#include "../src/game/battle/battle_items.h"
#include "../src/game/battle/battle_catch.h"
#include "../src/platform/hardware.h"
#include "../src/game/constants.h"

static void item_reset(void) {
    extern void WRAMClear(void);
    WRAMClear();
    hRandomAdd = 0x00;
    hRandomSub = 0xFF;
    wIsInBattle = 1;
    wPartyCount = 1;
    wPartyMons[0].base.hp     = 50;
    wPartyMons[0].max_hp      = 100;
    wPartyMons[0].base.status = 0;
    wPlayerMonNumber          = 0;
    wBattleMon.hp             = 50;
    wBattleMon.max_hp         = 100;
    wBattleMon.status         = 0;

    wEnemyMon.max_hp    = 100;
    wEnemyMon.hp        = 50;
    wEnemyMon.catch_rate= 255;
    wEnemyMon.status    = 0;

    for (int i = 0; i < NUM_STAT_MODS; i++)
        wPlayerMonStatMods[i] = STAT_STAGE_NORMAL;
}

TEST(BattleItems, Ball_TrainerBattle_CannotUse) {
    item_reset();
    wIsInBattle = 2;
    item_use_result_t r = Battle_UseItem(ITEM_POKE_BALL, 0);
    EXPECT_EQ((int)r, (int)ITEM_USE_CANNOT_USE);
}

TEST(BattleItems, MasterBall_AlwaysCatches) {
    item_reset();
    item_use_result_t r = Battle_UseItem(ITEM_MASTER_BALL, 0);
    EXPECT_EQ((int)r, (int)ITEM_USE_CAUGHT);
}

TEST(BattleItems, PokeBall_FailedCatch_ConsumesTurn) {
    item_reset();
    wEnemyMon.catch_rate = 1;
    wEnemyMon.hp = wEnemyMon.max_hp;

    hRandomAdd = 0x00;
    hRandomSub = (uint8_t)((255 ^ 0x05) + 3);
    item_use_result_t r = Battle_UseItem(ITEM_POKE_BALL, 0);

    EXPECT_EQ((int)r, (int)ITEM_USE_OK);
}

TEST(BattleItems, Potion_HealsBy20) {
    item_reset();
    wPartyMons[0].base.hp = 50;
    Battle_UseItem(ITEM_POTION, 0);
    EXPECT_EQ((int)wPartyMons[0].base.hp, 70);
}

TEST(BattleItems, Potion_CapsAtMaxHP) {
    item_reset();
    wPartyMons[0].base.hp = 95;
    Battle_UseItem(ITEM_POTION, 0);
    EXPECT_EQ((int)wPartyMons[0].base.hp, 100);
}

TEST(BattleItems, Potion_FailsAtFullHP) {
    item_reset();
    wPartyMons[0].base.hp = 100;
    item_use_result_t r = Battle_UseItem(ITEM_POTION, 0);
    EXPECT_EQ((int)r, (int)ITEM_USE_FAILED);
    EXPECT_EQ((int)wPartyMons[0].base.hp, 100);
}

TEST(BattleItems, SuperPotion_HealsBy50) {
    item_reset();
    wPartyMons[0].base.hp = 10;
    Battle_UseItem(ITEM_SUPER_POTION, 0);
    EXPECT_EQ((int)wPartyMons[0].base.hp, 60);
}

TEST(BattleItems, HyperPotion_HealsBy200) {
    item_reset();
    wPartyMons[0].base.hp  = 10;
    wPartyMons[0].max_hp   = 300;
    wBattleMon.max_hp      = 300;
    Battle_UseItem(ITEM_HYPER_POTION, 0);
    EXPECT_EQ((int)wPartyMons[0].base.hp, 210);
}

TEST(BattleItems, MaxPotion_RestoresToMax) {
    item_reset();
    wPartyMons[0].base.hp = 1;
    Battle_UseItem(ITEM_MAX_POTION, 0);
    EXPECT_EQ((int)wPartyMons[0].base.hp, 100);
}

TEST(BattleItems, MaxPotion_FailsAtFullHP) {
    item_reset();
    item_use_result_t r = Battle_UseItem(ITEM_MAX_POTION, 0);

    wPartyMons[0].base.hp = 100;
    r = Battle_UseItem(ITEM_MAX_POTION, 0);
    EXPECT_EQ((int)r, (int)ITEM_USE_FAILED);
}

TEST(BattleItems, FreshWater_HealsBy50) {
    item_reset();
    wPartyMons[0].base.hp = 20;
    Battle_UseItem(ITEM_FRESH_WATER, 0);
    EXPECT_EQ((int)wPartyMons[0].base.hp, 70);
}

TEST(BattleItems, SodaPop_HealsBy60) {
    item_reset();
    wPartyMons[0].base.hp = 20;
    Battle_UseItem(ITEM_SODA_POP, 0);
    EXPECT_EQ((int)wPartyMons[0].base.hp, 80);
}

TEST(BattleItems, Lemonade_HealsBy80) {
    item_reset();
    wPartyMons[0].base.hp = 10;
    Battle_UseItem(ITEM_LEMONADE, 0);
    EXPECT_EQ((int)wPartyMons[0].base.hp, 90);
}

TEST(BattleItems, Medicine_SyncsActiveMon) {
    item_reset();
    wPlayerMonNumber = 0;
    wBattleMon.hp    = 50;
    Battle_UseItem(ITEM_POTION, 0);
    EXPECT_EQ((int)wBattleMon.hp, 70);
}

TEST(BattleItems, Medicine_NoSyncForBenchedMon) {
    item_reset();
    wPartyCount = 2;
    wPartyMons[1].base.hp  = 50;
    wPartyMons[1].max_hp   = 100;
    wPartyMons[1].base.status = 0;
    wPlayerMonNumber = 0;
    wBattleMon.hp    = 50;
    Battle_UseItem(ITEM_POTION, 1);
    EXPECT_EQ((int)wPartyMons[1].base.hp, 70);
    EXPECT_EQ((int)wBattleMon.hp, 50);
}

TEST(BattleItems, Antidote_CuresPoisoned) {
    item_reset();
    wPartyMons[0].base.status = STATUS_PSN;
    Battle_UseItem(ITEM_ANTIDOTE, 0);
    EXPECT_EQ((int)wPartyMons[0].base.status, 0);
}

TEST(BattleItems, Antidote_FailsIfNotPoisoned) {
    item_reset();
    item_use_result_t r = Battle_UseItem(ITEM_ANTIDOTE, 0);
    EXPECT_EQ((int)r, (int)ITEM_USE_FAILED);
}

TEST(BattleItems, Awakening_CuresSleep) {
    item_reset();
    wPartyMons[0].base.status = 0x03;
    Battle_UseItem(ITEM_AWAKENING, 0);
    EXPECT_EQ((int)wPartyMons[0].base.status, 0);
}

TEST(BattleItems, ParlyzHeal_CuresParalysis) {
    item_reset();
    wPartyMons[0].base.status = STATUS_PAR;
    Battle_UseItem(ITEM_PARLYZ_HEAL, 0);
    EXPECT_EQ((int)wPartyMons[0].base.status, 0);
}

TEST(BattleItems, FullHeal_CuresAnyStatus) {
    item_reset();
    wPartyMons[0].base.status = STATUS_BRN;
    Battle_UseItem(ITEM_FULL_HEAL, 0);
    EXPECT_EQ((int)wPartyMons[0].base.status, 0);
}

TEST(BattleItems, StatusCure_SyncsActiveMon) {
    item_reset();
    wPartyMons[0].base.status = STATUS_PSN;
    wBattleMon.status         = STATUS_PSN;
    wPlayerMonNumber = 0;
    Battle_UseItem(ITEM_ANTIDOTE, 0);
    EXPECT_EQ((int)wBattleMon.status, 0);
}

TEST(BattleItems, FullRestore_HealsAndCuresStatus) {
    item_reset();
    wPartyMons[0].base.status = STATUS_PAR;
    wPartyMons[0].base.hp     = 10;
    Battle_UseItem(ITEM_FULL_RESTORE, 0);
    EXPECT_EQ((int)wPartyMons[0].base.hp, 100);
    EXPECT_EQ((int)wPartyMons[0].base.status, 0);
}

TEST(BattleItems, FullRestore_FailsIfFullHPNoStatus) {
    item_reset();
    wPartyMons[0].base.hp     = 100;
    wPartyMons[0].base.status = 0;
    item_use_result_t r = Battle_UseItem(ITEM_FULL_RESTORE, 0);
    EXPECT_EQ((int)r, (int)ITEM_USE_FAILED);
}

TEST(BattleItems, Revive_RestoresToHalfMaxHP) {
    item_reset();
    wPartyMons[0].base.hp = 0;
    wPartyMons[0].max_hp  = 100;
    Battle_UseItem(ITEM_REVIVE, 0);
    EXPECT_EQ((int)wPartyMons[0].base.hp, 50);
}

TEST(BattleItems, MaxRevive_RestoresToMaxHP) {
    item_reset();
    wPartyMons[0].base.hp = 0;
    wPartyMons[0].max_hp  = 100;
    Battle_UseItem(ITEM_MAX_REVIVE, 0);
    EXPECT_EQ((int)wPartyMons[0].base.hp, 100);
}

TEST(BattleItems, Revive_FailsIfNotFainted) {
    item_reset();
    item_use_result_t r = Battle_UseItem(ITEM_REVIVE, 0);
    EXPECT_EQ((int)r, (int)ITEM_USE_FAILED);
}

TEST(BattleItems, XAttack_RaisesAtkStage) {
    item_reset();
    Battle_UseItem(ITEM_X_ATTACK, 0);
    EXPECT_EQ((int)wPlayerMonStatMods[0], STAT_STAGE_NORMAL + 1);
}

TEST(BattleItems, XDefend_RaisesDefStage) {
    item_reset();
    Battle_UseItem(ITEM_X_DEFEND, 0);
    EXPECT_EQ((int)wPlayerMonStatMods[1], STAT_STAGE_NORMAL + 1);
}

TEST(BattleItems, XSpeed_RaisesSpeedStage) {
    item_reset();
    Battle_UseItem(ITEM_X_SPEED, 0);
    EXPECT_EQ((int)wPlayerMonStatMods[2], STAT_STAGE_NORMAL + 1);
}

TEST(BattleItems, XSpecial_RaisesSpecialStage) {
    item_reset();
    Battle_UseItem(ITEM_X_SPECIAL, 0);
    EXPECT_EQ((int)wPlayerMonStatMods[3], STAT_STAGE_NORMAL + 1);
}

TEST(BattleItems, XAttack_FailsAtMax) {
    item_reset();
    wPlayerMonStatMods[0] = STAT_STAGE_MAX;
    item_use_result_t r = Battle_UseItem(ITEM_X_ATTACK, 0);
    EXPECT_EQ((int)r, (int)ITEM_USE_FAILED);
    EXPECT_EQ((int)wPlayerMonStatMods[0], STAT_STAGE_MAX);
}

TEST(BattleItems, XAccuracy_SetsFlag) {
    item_reset();
    Battle_UseItem(ITEM_X_ACCURACY, 0);
    EXPECT_TRUE(wPlayerBattleStatus2 & (1 << BSTAT2_USING_X_ACCURACY));
}

TEST(BattleItems, GuardSpec_SetsMistFlag) {
    item_reset();
    Battle_UseItem(ITEM_GUARD_SPEC, 0);
    EXPECT_TRUE(wPlayerBattleStatus2 & (1 << BSTAT2_PROTECTED_BY_MIST));
}

TEST(BattleItems, DireHit_SetsFocusEnergyFlag) {
    item_reset();
    Battle_UseItem(ITEM_DIRE_HIT, 0);
    EXPECT_TRUE(wPlayerBattleStatus2 & (1 << BSTAT2_GETTING_PUMPED));
}

TEST(BattleItems, PokeDoll_SetsEscapeFlag) {
    item_reset();
    item_use_result_t r = Battle_UseItem(ITEM_POKE_DOLL, 0);
    EXPECT_EQ((int)r, (int)ITEM_USE_FLED);
    EXPECT_EQ((int)wEscapedFromBattle, 1);
}

TEST(BattleItems, PokeDoll_CannotUseInTrainerBattle) {
    item_reset();
    wIsInBattle = 2;
    item_use_result_t r = Battle_UseItem(ITEM_POKE_DOLL, 0);
    EXPECT_EQ((int)r, (int)ITEM_USE_CANNOT_USE);
}

TEST(BattleItems, TownMap_CannotUseInBattle) {
    item_reset();
    item_use_result_t r = Battle_UseItem(0x05 , 0);
    EXPECT_EQ((int)r, (int)ITEM_USE_CANNOT_USE);
}

TEST(BattleItems, HM01_CannotUseInBattle) {
    item_reset();
    item_use_result_t r = Battle_UseItem(ITEM_HM01, 0);
    EXPECT_EQ((int)r, (int)ITEM_USE_CANNOT_USE);
}
