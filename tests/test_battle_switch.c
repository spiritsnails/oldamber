
#include "test_runner.h"
#include "../src/game/battle/battle_switch.h"
#include "../src/game/battle/battle.h"
#include "../src/platform/hardware.h"
#include "../src/game/constants.h"
#include <string.h>

static void reset_party(void) {
    memset(wPartyMons, 0, sizeof(wPartyMons));
    wPartyCount = 0;
}

static void reset_battle_state(void) {
    memset(&wBattleMon, 0, sizeof(wBattleMon));
    wPlayerMonNumber          = 0;
    wPartyGainExpFlags        = 0;
    wPartyFoughtCurrentEnemyFlags = 0;
    wPlayerBattleStatus1      = 0;
    wPlayerBattleStatus2      = 0;
    wPlayerBattleStatus3      = 0;
    wPlayerDisabledMove       = 0;
    wPlayerDisabledMoveNumber = 0;
    wPlayerMonMinimized       = 0;
    wPlayerConfusedCounter    = 0;
    wPlayerToxicCounter       = 0;
    wPlayerBideAccumulatedDamage = 0;
    wEnemyBattleStatus1       = 0;
    wActionResultOrTookBattleTurn = 0;
    wDamageMultipliers        = 0;
    wPlayerMoveNum            = 0;
    wPlayerUsedMove           = 0;
    wEnemyUsedMove            = 0;
    wBoostExpByExpAll         = 0;
    wLinkState                = 0;
    wObtainedBadges           = 0;
    for (int i = 0; i < NUM_STAT_MODS; i++) wPlayerMonStatMods[i] = 7;
}

TEST(Switch, AnyPartyAlive_AllFainted) {
    reset_party();
    wPartyCount = 3;

    wPartyMons[0].base.hp = 0;
    wPartyMons[1].base.hp = 0;
    wPartyMons[2].base.hp = 0;
    EXPECT_EQ(Battle_AnyPartyAlive(), 0);
}

TEST(Switch, AnyPartyAlive_OneAlive) {
    reset_party();
    wPartyCount = 3;
    wPartyMons[0].base.hp = 0;
    wPartyMons[1].base.hp = 30;
    wPartyMons[2].base.hp = 0;
    EXPECT_NE(Battle_AnyPartyAlive(), 0);
}

TEST(Switch, AnyPartyAlive_AllAlive) {
    reset_party();
    wPartyCount = 2;
    wPartyMons[0].base.hp = 50;
    wPartyMons[1].base.hp = 1;
    EXPECT_NE(Battle_AnyPartyAlive(), 0);
}

TEST(Switch, HasMonFainted_HP0) {
    reset_party();
    wPartyMons[2].base.hp = 0;
    EXPECT_EQ(Battle_HasMonFainted(2), 1);
}

TEST(Switch, HasMonFainted_HPNonzero) {
    reset_party();
    wPartyMons[2].base.hp = 1;
    EXPECT_EQ(Battle_HasMonFainted(2), 0);
}

TEST(Switch, LoadBattleMon_CopiesFields) {
    reset_party();
    reset_battle_state();
    wPartyCount = 2;
    wPlayerMonNumber = 1;

    party_mon_t *p = &wPartyMons[1];
    p->base.species    = 6;
    p->base.hp         = 120;
    p->base.box_level  = 0;
    p->base.status     = 0;
    p->base.type1      = TYPE_FIRE;
    p->base.type2      = TYPE_FLYING;
    p->base.catch_rate = 45;
    p->base.moves[0]   = MOVE_SLASH;
    p->base.moves[1]   = MOVE_RAZOR_LEAF;
    p->base.moves[2]   = 0;
    p->base.moves[3]   = 0;
    p->base.dvs        = 0xFFFF;
    p->base.pp[0]      = 15;
    p->base.pp[1]      = 20;
    p->level           = 50;
    p->max_hp          = 160;
    p->atk             = 110;
    p->def             = 90;
    p->spd             = 100;
    p->spc             = 85;

    Battle_LoadBattleMonFromParty();

    EXPECT_EQ(wBattleMon.species,    6);
    EXPECT_EQ(wBattleMon.hp,         120);
    EXPECT_EQ(wBattleMon.type1,      TYPE_FIRE);
    EXPECT_EQ(wBattleMon.type2,      TYPE_FLYING);
    EXPECT_EQ(wBattleMon.catch_rate, 45);
    EXPECT_EQ(wBattleMon.moves[0],   MOVE_SLASH);
    EXPECT_EQ(wBattleMon.moves[1],   MOVE_RAZOR_LEAF);
    EXPECT_EQ(wBattleMon.dvs,        0xFFFF);
    EXPECT_EQ(wBattleMon.pp[0],      15);
    EXPECT_EQ(wBattleMon.pp[1],      20);
    EXPECT_EQ(wBattleMon.level,      50);
    EXPECT_EQ(wBattleMon.max_hp,     160);
    EXPECT_EQ(wBattleMon.atk,        110);
    EXPECT_EQ(wBattleMon.def,        90);
    EXPECT_EQ(wBattleMon.spd,        100);
    EXPECT_EQ(wBattleMon.spc,        85);
}

TEST(Switch, LoadBattleMon_ResetsStatMods) {
    reset_party();
    reset_battle_state();
    wPartyCount = 1;
    wPlayerMonNumber = 0;
    wPartyMons[0].base.hp = 100;
    wPartyMons[0].level   = 20;
    wPartyMons[0].max_hp  = 100;

    wPlayerMonStatMods[0] = 13;
    wPlayerMonStatMods[3] = 1;

    Battle_LoadBattleMonFromParty();

    for (int i = 0; i < NUM_STAT_MODS; i++) {
        EXPECT_EQ(wPlayerMonStatMods[i], 7);
    }
}

TEST(Switch, LoadBattleMon_SetsUnmodifiedStats) {
    reset_party();
    reset_battle_state();
    wPartyCount = 1;
    wPlayerMonNumber = 0;
    wPartyMons[0].base.hp = 100;
    wPartyMons[0].level   = 30;
    wPartyMons[0].max_hp  = 100;
    wPartyMons[0].atk     = 80;
    wPartyMons[0].def     = 60;
    wPartyMons[0].spd     = 70;
    wPartyMons[0].spc     = 55;

    Battle_LoadBattleMonFromParty();

    EXPECT_EQ(wPlayerMonUnmodifiedAttack,  80);
    EXPECT_EQ(wPlayerMonUnmodifiedDefense, 60);
    EXPECT_EQ(wPlayerMonUnmodifiedSpeed,   70);
    EXPECT_EQ(wPlayerMonUnmodifiedSpecial, 55);
}

TEST(Switch, LoadBattleMon_ParalysisPenalty) {

    reset_party();
    reset_battle_state();
    wPartyCount = 1;
    wPlayerMonNumber = 0;
    wPartyMons[0].base.hp     = 100;
    wPartyMons[0].base.status = STATUS_PAR;
    wPartyMons[0].level       = 30;
    wPartyMons[0].max_hp      = 100;
    wPartyMons[0].atk         = 80;
    wPartyMons[0].def         = 60;
    wPartyMons[0].spd         = 100;
    wPartyMons[0].spc         = 55;

    Battle_LoadBattleMonFromParty();

    EXPECT_EQ(wBattleMon.spd, 25);

    EXPECT_EQ(wBattleMon.atk, 80);
}

TEST(Switch, LoadBattleMon_BurnPenalty) {

    reset_party();
    reset_battle_state();
    wPartyCount = 1;
    wPlayerMonNumber = 0;
    wPartyMons[0].base.hp     = 100;
    wPartyMons[0].base.status = STATUS_BRN;
    wPartyMons[0].level       = 30;
    wPartyMons[0].max_hp      = 100;
    wPartyMons[0].atk         = 100;
    wPartyMons[0].def         = 60;
    wPartyMons[0].spd         = 80;
    wPartyMons[0].spc         = 55;

    Battle_LoadBattleMonFromParty();

    EXPECT_EQ(wBattleMon.atk, 50);
    EXPECT_EQ(wBattleMon.spd, 80);
}

TEST(Switch, LoadBattleMon_BadgeBoost_Boulder) {

    reset_party();
    reset_battle_state();
    wPartyCount = 1;
    wPlayerMonNumber = 0;
    wPartyMons[0].base.hp = 100;
    wPartyMons[0].level   = 30;
    wPartyMons[0].max_hp  = 100;
    wPartyMons[0].atk     = 80;
    wPartyMons[0].def     = 80;
    wPartyMons[0].spd     = 80;
    wPartyMons[0].spc     = 80;

    wObtainedBadges = (1u << BIT_BOULDERBADGE);

    Battle_LoadBattleMonFromParty();

    EXPECT_EQ(wBattleMon.atk, 90);
    EXPECT_EQ(wBattleMon.def, 80);
    EXPECT_EQ(wBattleMon.spd, 80);
    EXPECT_EQ(wBattleMon.spc, 80);
}

TEST(Switch, LoadBattleMon_BadgeBoost_AllFour) {
    reset_party();
    reset_battle_state();
    wPartyCount = 1;
    wPlayerMonNumber = 0;
    wPartyMons[0].base.hp = 100;
    wPartyMons[0].level   = 50;
    wPartyMons[0].max_hp  = 100;
    wPartyMons[0].atk     = 128;
    wPartyMons[0].def     = 128;
    wPartyMons[0].spd     = 128;
    wPartyMons[0].spc     = 128;

    wObtainedBadges = (1u << BIT_BOULDERBADGE) | (1u << BIT_THUNDERBADGE)
                    | (1u << BIT_SOULBADGE)    | (1u << BIT_VOLCANOBADGE);

    Battle_LoadBattleMonFromParty();

    EXPECT_EQ(wBattleMon.atk, 144);
    EXPECT_EQ(wBattleMon.def, 144);
    EXPECT_EQ(wBattleMon.spd, 144);
    EXPECT_EQ(wBattleMon.spc, 144);
}

TEST(Switch, LoadBattleMon_BadgeBoost_CapAt999) {
    reset_party();
    reset_battle_state();
    wPartyCount = 1;
    wPlayerMonNumber = 0;
    wPartyMons[0].base.hp = 100;
    wPartyMons[0].level   = 100;
    wPartyMons[0].max_hp  = 100;
    wPartyMons[0].atk     = 999;
    wObtainedBadges = (1u << BIT_BOULDERBADGE);

    Battle_LoadBattleMonFromParty();

    EXPECT_EQ(wBattleMon.atk, 999);
}

TEST(Switch, LoadBattleMon_NoBadgeBoostInLinkBattle) {
    reset_party();
    reset_battle_state();
    wPartyCount = 1;
    wPlayerMonNumber = 0;
    wPartyMons[0].base.hp = 100;
    wPartyMons[0].level   = 50;
    wPartyMons[0].max_hp  = 100;
    wPartyMons[0].atk     = 100;
    wObtainedBadges = 0xFF;
    wLinkState      = LINK_STATE_BATTLING;

    Battle_LoadBattleMonFromParty();

    EXPECT_EQ(wBattleMon.atk, 100);
}

TEST(Switch, ReadCurHPAndStatus_SyncsToParty) {
    reset_party();
    reset_battle_state();
    wPartyCount = 3;
    wPlayerMonNumber = 2;

    wBattleMon.hp     = 44;
    wBattleMon.status = STATUS_PSN;

    wPartyMons[2].base.hp     = 100;
    wPartyMons[2].base.status = 0;

    Battle_ReadPlayerMonCurHPAndStatus();

    EXPECT_EQ(wPartyMons[2].base.hp,     44);
    EXPECT_EQ(wPartyMons[2].base.status, STATUS_PSN);
}

TEST(Switch, ReadCurHPAndStatus_OnlyUpdatesActiveSlot) {
    reset_party();
    reset_battle_state();
    wPartyCount = 3;
    wPlayerMonNumber = 0;

    wBattleMon.hp     = 55;
    wPartyMons[0].base.hp = 100;
    wPartyMons[1].base.hp = 200;
    wPartyMons[2].base.hp = 300;

    Battle_ReadPlayerMonCurHPAndStatus();

    EXPECT_EQ(wPartyMons[0].base.hp, 55);
    EXPECT_EQ(wPartyMons[1].base.hp, 200);
    EXPECT_EQ(wPartyMons[2].base.hp, 300);
}

TEST(Switch, SendOutMon_ClearsBattleStatus) {
    reset_battle_state();
    wPlayerBattleStatus1 = 0xFF;
    wPlayerBattleStatus2 = 0xFF;
    wPlayerBattleStatus3 = 0xFF;
    wEnemyBattleStatus1  = (uint8_t)(1u << BSTAT1_USING_TRAPPING);

    Battle_SendOutMon_State();

    EXPECT_EQ(wPlayerBattleStatus1, 0);
    EXPECT_EQ(wPlayerBattleStatus2, 0);
    EXPECT_EQ(wPlayerBattleStatus3, 0);

    EXPECT_EQ(wEnemyBattleStatus1 & (1u << BSTAT1_USING_TRAPPING), 0);
}

TEST(Switch, SendOutMon_ClearsDisabledMove) {
    reset_battle_state();
    wPlayerDisabledMove       = 0x23;
    wPlayerDisabledMoveNumber = 5;
    wPlayerMonMinimized       = 1;

    Battle_SendOutMon_State();

    EXPECT_EQ(wPlayerDisabledMove, 0);
    EXPECT_EQ(wPlayerDisabledMoveNumber, 0);
    EXPECT_EQ(wPlayerMonMinimized, 0);
}

TEST(Switch, SendOutMon_ClearsCounters) {
    reset_battle_state();
    wPlayerConfusedCounter       = 3;
    wPlayerToxicCounter          = 5;
    wPlayerBideAccumulatedDamage = 200;

    Battle_SendOutMon_State();

    EXPECT_EQ(wPlayerConfusedCounter, 0);
    EXPECT_EQ(wPlayerToxicCounter, 0);
    EXPECT_EQ(wPlayerBideAccumulatedDamage, 0);
}

TEST(Switch, SendOutMon_SetsWhoseTurn) {
    reset_battle_state();
    hWhoseTurn = 0;
    Battle_SendOutMon_State();
    EXPECT_EQ(hWhoseTurn, 1);
}

TEST(Switch, SendOutMon_PreservesOtherEnemyStatus) {

    reset_battle_state();
    wEnemyBattleStatus1 = 0xFF;

    Battle_SendOutMon_State();

    uint8_t expected = (uint8_t)(0xFF & ~(1u << BSTAT1_USING_TRAPPING));
    EXPECT_EQ(wEnemyBattleStatus1, expected);
}

TEST(Switch, SwitchPlayerMon_UpdatesMonNumber) {
    reset_party();
    reset_battle_state();
    wPartyCount      = 3;
    wPlayerMonNumber = 0;

    wBattleMon.hp     = 80;
    wBattleMon.status = 0;
    wPartyMons[0].base.hp = 80;
    wPartyMons[0].level   = 20;
    wPartyMons[0].max_hp  = 80;

    wPartyMons[2].base.hp = 60;
    wPartyMons[2].level   = 25;
    wPartyMons[2].max_hp  = 100;

    Battle_SwitchPlayerMon(2);

    EXPECT_EQ(wPlayerMonNumber, 2);
}

TEST(Switch, SwitchPlayerMon_SetsExpAndFoughtFlags) {
    reset_party();
    reset_battle_state();
    wPartyCount = 3;
    wPlayerMonNumber = 0;
    wPartyMons[0].base.hp = 80;
    wPartyMons[0].level   = 20;
    wPartyMons[0].max_hp  = 80;
    wPartyMons[2].base.hp = 60;
    wPartyMons[2].level   = 25;
    wPartyMons[2].max_hp  = 100;

    wPartyGainExpFlags            = 0;
    wPartyFoughtCurrentEnemyFlags = 0;

    Battle_SwitchPlayerMon(2);

    EXPECT_NE(wPartyGainExpFlags & (1u << 2), 0);
    EXPECT_NE(wPartyFoughtCurrentEnemyFlags & (1u << 2), 0);
}

TEST(Switch, SwitchPlayerMon_SyncsOldMonHP) {

    reset_party();
    reset_battle_state();
    wPartyCount      = 2;
    wPlayerMonNumber = 0;

    wBattleMon.hp              = 33;
    wBattleMon.status          = 0;
    wPartyMons[0].base.hp      = 100;
    wPartyMons[0].level        = 20;
    wPartyMons[0].max_hp       = 100;
    wPartyMons[1].base.hp      = 50;
    wPartyMons[1].level        = 25;
    wPartyMons[1].max_hp       = 80;

    Battle_SwitchPlayerMon(1);

    EXPECT_EQ(wPartyMons[0].base.hp, 33);
}

TEST(Switch, SwitchPlayerMon_ConsumesActionTurn) {
    reset_party();
    reset_battle_state();
    wPartyCount      = 2;
    wPlayerMonNumber = 0;
    wPartyMons[0].base.hp = 80;
    wPartyMons[0].level   = 20;
    wPartyMons[0].max_hp  = 80;
    wPartyMons[1].base.hp = 60;
    wPartyMons[1].level   = 25;
    wPartyMons[1].max_hp  = 100;
    wActionResultOrTookBattleTurn = 0;

    Battle_SwitchPlayerMon(1);

    EXPECT_EQ(wActionResultOrTookBattleTurn, 1);
}

TEST(Switch, SwitchPlayerMon_LoadsNewMonData) {
    reset_party();
    reset_battle_state();
    wPartyCount      = 2;
    wPlayerMonNumber = 0;
    wPartyMons[0].base.hp   = 80;
    wPartyMons[0].level     = 20;
    wPartyMons[0].max_hp    = 80;

    wPartyMons[1].base.hp      = 55;
    wPartyMons[1].base.species = 25;
    wPartyMons[1].base.status  = 0;
    wPartyMons[1].level        = 35;
    wPartyMons[1].max_hp       = 95;
    wPartyMons[1].atk          = 75;

    Battle_SwitchPlayerMon(1);

    EXPECT_EQ(wBattleMon.species, 25);
    EXPECT_EQ(wBattleMon.hp,      55);
    EXPECT_EQ(wBattleMon.level,   35);
    EXPECT_EQ(wBattleMon.atk,     75);
}

TEST(Switch, ChooseNextMon_UpdatesMonNumber) {
    reset_party();
    reset_battle_state();
    wPartyCount = 3;
    wPlayerMonNumber = 0;

    wPartyMons[1].base.hp = 70;
    wPartyMons[1].level   = 22;
    wPartyMons[1].max_hp  = 90;

    Battle_ChooseNextMon(1);

    EXPECT_EQ(wPlayerMonNumber, 1);
}

TEST(Switch, ChooseNextMon_SetsExpAndFoughtFlags) {
    reset_party();
    reset_battle_state();
    wPartyCount = 3;
    wPlayerMonNumber = 0;
    wPartyMons[1].base.hp = 70;
    wPartyMons[1].level   = 22;
    wPartyMons[1].max_hp  = 90;

    wPartyGainExpFlags            = 0;
    wPartyFoughtCurrentEnemyFlags = 0;

    Battle_ChooseNextMon(1);

    EXPECT_NE(wPartyGainExpFlags & (1u << 1), 0);
    EXPECT_NE(wPartyFoughtCurrentEnemyFlags & (1u << 1), 0);
}

TEST(Switch, ChooseNextMon_DoesNotConsumeAction) {

    reset_party();
    reset_battle_state();
    wPartyCount = 2;
    wPlayerMonNumber = 0;
    wPartyMons[1].base.hp = 60;
    wPartyMons[1].level   = 20;
    wPartyMons[1].max_hp  = 80;
    wActionResultOrTookBattleTurn = 0;

    Battle_ChooseNextMon(1);

    EXPECT_EQ(wActionResultOrTookBattleTurn, 0);
}

TEST(Switch, ChooseNextMon_ClearsBattleStatus) {

    reset_party();
    reset_battle_state();
    wPartyCount = 2;
    wPartyMons[1].base.hp = 60;
    wPartyMons[1].level   = 20;
    wPartyMons[1].max_hp  = 80;

    wPlayerBattleStatus1 = 0xFF;
    wEnemyBattleStatus1  = (uint8_t)(1u << BSTAT1_USING_TRAPPING);

    Battle_ChooseNextMon(1);

    EXPECT_EQ(wPlayerBattleStatus1, 0);
    EXPECT_EQ(wEnemyBattleStatus1 & (1u << BSTAT1_USING_TRAPPING), 0);
}
