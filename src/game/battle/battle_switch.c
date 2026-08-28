
#include "battle_switch.h"
#include "battle.h"
#include "../../platform/hardware.h"
#include "../constants.h"
#include <string.h>

static uint16_t apply_boost(uint16_t stat) {
    uint16_t boost = (uint16_t)(stat >> 3);
    uint32_t result = (uint32_t)stat + boost;
    return (uint16_t)(result > MAX_STAT_VALUE ? MAX_STAT_VALUE : result);
}

static void apply_badge_stat_boosts(void) {
    if (wLinkState == LINK_STATE_BATTLING) return;

    uint8_t b = wObtainedBadges;
    if (b & (1u << BIT_BOULDERBADGE)) wBattleMon.atk = apply_boost(wBattleMon.atk);
    if (b & (1u << BIT_THUNDERBADGE)) wBattleMon.def = apply_boost(wBattleMon.def);
    if (b & (1u << BIT_SOULBADGE))   wBattleMon.spd = apply_boost(wBattleMon.spd);
    if (b & (1u << BIT_VOLCANOBADGE)) wBattleMon.spc = apply_boost(wBattleMon.spc);
}

void Battle_ApplyBadgeStatBoosts(void) {
    apply_badge_stat_boosts();
}

int Battle_AnyPartyAlive(void) {
    uint16_t hp_or = 0;
    for (uint8_t i = 0; i < wPartyCount; i++) {
        hp_or |= wPartyMons[i].base.hp;
    }
    return (hp_or != 0) ? 1 : 0;
}

int Battle_HasMonFainted(uint8_t slot) {
    return (wPartyMons[slot].base.hp == 0) ? 1 : 0;
}

void Battle_LoadBattleMonFromParty(void) {
    party_mon_t *p = &wPartyMons[wPlayerMonNumber];

    wBattleMon.species    = p->base.species;
    wBattleMon.hp         = p->base.hp;
    wBattleMon.party_pos  = p->base.box_level;
    wBattleMon.status     = p->base.status;
    wBattleMon.type1      = p->base.type1;
    wBattleMon.type2      = p->base.type2;
    wBattleMon.catch_rate = p->base.catch_rate;
    memcpy(wBattleMon.moves, p->base.moves, sizeof(wBattleMon.moves));

    wBattleMon.dvs = p->base.dvs;

    memcpy(wBattleMon.pp, p->base.pp, sizeof(wBattleMon.pp));

    wBattleMon.level  = p->level;
    wBattleMon.max_hp = p->max_hp;
    wBattleMon.atk    = p->atk;
    wBattleMon.def    = p->def;
    wBattleMon.spd    = p->spd;
    wBattleMon.spc    = p->spc;

    wCurSpecies = wBattleMon.species;

    wPlayerMonUnmodifiedAttack  = p->atk;
    wPlayerMonUnmodifiedDefense = p->def;
    wPlayerMonUnmodifiedSpeed   = p->spd;
    wPlayerMonUnmodifiedSpecial = p->spc;

    hWhoseTurn = 1;
    Battle_ApplyBurnAndParalysisPenalties();

    apply_badge_stat_boosts();

    for (int i = 0; i < NUM_STAT_MODS; i++) {
        wPlayerMonStatMods[i] = 7;
    }
}

void Battle_SendOutMon_State(void) {
    wBoostExpByExpAll               = 0;
    wDamageMultipliers              = 0;
    wPlayerMoveNum                  = 0;
    wPlayerUsedMove                 = 0;
    wEnemyUsedMove                  = 0;

    wPlayerBattleStatus1 = 0;
    wPlayerBattleStatus2 = 0;
    wPlayerBattleStatus3 = 0;

    wPlayerDisabledMove        = 0;
    wPlayerDisabledMoveNumber  = 0;
    wPlayerMonMinimized        = 0;
    wPlayerConfusedCounter     = 0;
    wPlayerToxicCounter        = 0;
    wPlayerBideAccumulatedDamage = 0;
    wPlayerNumAttacksLeft      = 0;

    wEnemyBattleStatus1 &= (uint8_t)~(1u << BSTAT1_USING_TRAPPING);

    hWhoseTurn = 1;
}

void Battle_ReadPlayerMonCurHPAndStatus(void) {
    party_mon_t *p = &wPartyMons[wPlayerMonNumber];
    p->base.hp     = wBattleMon.hp;
    p->base.status = wBattleMon.status;
}

void Battle_SwitchPlayerMon(uint8_t new_slot) {

    Battle_ReadPlayerMonCurHPAndStatus();

    wPlayerMonNumber = new_slot;

    wPartyGainExpFlags |= (uint8_t)(1u << new_slot);
    wPartyFoughtCurrentEnemyFlags |= (uint8_t)(1u << new_slot);

    Battle_LoadBattleMonFromParty();
    Battle_SendOutMon_State();

    wActionResultOrTookBattleTurn = 1;
}

void Battle_ChooseNextMon(uint8_t new_slot) {
    wPlayerMonNumber = new_slot;

    wPartyGainExpFlags |= (uint8_t)(1u << new_slot);
    wPartyFoughtCurrentEnemyFlags |= (uint8_t)(1u << new_slot);

    Battle_LoadBattleMonFromParty();
    Battle_SendOutMon_State();
}
