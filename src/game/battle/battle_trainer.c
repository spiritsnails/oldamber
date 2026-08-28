
#include "battle_trainer.h"
#include "battle_core.h"
#include "battle_loop.h"
#include "battle.h"
#include "battle_probe.h"
#include "../../platform/hardware.h"
#include "../constants.h"
#include "../pokedex.h"
#include <string.h>
#include <stdio.h>

int Battle_AnyEnemyPokemonAliveCheck(void) {
    for (uint8_t i = 0; i < wEnemyPartyCount; i++) {
        if (wEnemyMons[i].base.hp != 0) return 1;
    }
    return 0;
}

void Battle_LoadEnemyMonFromParty(void) {
    party_mon_t *p = &wEnemyMons[wEnemyMonPartyPos];

    wEnemyMon.species    = p->base.species;
    wEnemyMon.hp         = p->base.hp;
    wEnemyMon.party_pos  = p->base.box_level;
    wEnemyMon.status     = p->base.status;
    wEnemyMon.type1      = p->base.type1;
    wEnemyMon.type2      = p->base.type2;
    wEnemyMon.catch_rate = p->base.catch_rate;
    memcpy(wEnemyMon.moves, p->base.moves, sizeof(wEnemyMon.moves));

    wEnemyMon.dvs = p->base.dvs;

    memcpy(wEnemyMon.pp, p->base.pp, sizeof(wEnemyMon.pp));

    wEnemyMon.level  = p->level;
    wEnemyMon.max_hp = p->max_hp;
    wEnemyMon.atk    = p->atk;
    wEnemyMon.def    = p->def;
    wEnemyMon.spd    = p->spd;
    wEnemyMon.spc    = p->spc;

    wEnemyMonUnmodifiedAttack  = p->atk;
    wEnemyMonUnmodifiedDefense = p->def;
    wEnemyMonUnmodifiedSpeed   = p->spd;
    wEnemyMonUnmodifiedSpecial = p->spc;

    hWhoseTurn = 0;
    Battle_ApplyBurnAndParalysisPenalties();

    for (int i = 0; i < NUM_STAT_MODS; i++) {
        wEnemyMonStatMods[i] = 7;
    }
}

int Battle_EnemySendOut_State(void) {

    uint8_t new_slot = 0xFF;
    for (uint8_t b = 0; b < wEnemyPartyCount; b++) {
        if (b == wEnemyMonPartyPos) continue;
        if (wEnemyMons[b].base.hp == 0) continue;
        new_slot = b;
        break;
    }

    if (new_slot == 0xFF) return 0;

    wEnemyMonPartyPos = new_slot;
    Battle_LoadEnemyMonFromParty();
    Pokedex_SetSeen(wEnemyMon.species);

    wEnemyBattleStatus1  = 0;
    wEnemyBattleStatus2  = 0;
    wEnemyBattleStatus3  = 0;

    wEnemyDisabledMove        = 0;
    wEnemyDisabledMoveNumber  = 0;
    wEnemyMonMinimized        = 0;
    wEnemyConfusedCounter     = 0;
    wEnemyToxicCounter        = 0;
    wEnemyBideAccumulatedDamage = 0;
    wEnemyNumAttacksLeft      = 0;

    wAICount = 0xFF;

    wPlayerBattleStatus1 &= (uint8_t)~(1u << BSTAT1_USING_TRAPPING);

    wLastSwitchInEnemyMonHP = wEnemyMon.hp;

    printf("[trainer] Enemy sent out new Monster (party slot %d).\n",
           (int)wEnemyMonPartyPos);
    return 1;
}

int Battle_ReplaceFaintedEnemyMon(void) {
    if (!Battle_EnemySendOut_State()) return 0;

    wEnemyMoveNum                  = 0;
    wActionResultOrTookBattleTurn  = 0;
    wAILayer2Encouragement         = 0;

    return 1;
}

void Battle_TrainerBattleVictory(void) {
    wBattleResult = BATTLE_OUTCOME_TRAINER_VICTORY;
    wIsInBattle   = 0;
    printf("[trainer] All enemy Monster fainted. Player wins!\n");
}

void Battle_HandlePlayerBlackOut(void) {
    wBattleResult = BATTLE_OUTCOME_BLACKOUT;
    wIsInBattle   = (uint8_t)0xFF;
    printf("[trainer] Player blacked out!\n");
}

int gBattleNoBlackoutOnLoss = 0;

void Battle_HandlePlayerLossNoBlackOut(void) {
    wBattleResult = BATTLE_OUTCOME_LOSS_NO_BLACKOUT;
    wIsInBattle   = 0;
    printf("[trainer] Player lost, no blackout (rival 1 in Oak's Lab).\n");
}

static int run_escapes(void) {
    wEscapedFromBattle = 1;
    return 1;
}

int Battle_TryRunningFromBattle(void) {
    BPROBE("TryRunningFromBattle");

    if (Battle_IsGhostBattle())
        return run_escapes();
    if (wBattleType == 2 )
        return run_escapes();

    if (wIsInBattle != 1)
        return 0;

    wNumRunAttempts++;

    uint16_t player_spd = wBattleMon.spd;
    uint16_t enemy_spd  = wEnemyMon.spd;

    if (player_spd >= enemy_spd)
        return run_escapes();

    uint16_t dividend = (uint16_t)((uint32_t)player_spd * 32u);

    uint8_t divisor = (uint8_t)((enemy_spd >> 2) & 0xFFu);
    if (divisor == 0)
        return run_escapes();

    uint16_t quotient = (uint16_t)(dividend / divisor);
    if (quotient > 255u)
        return run_escapes();

    uint16_t odds = (uint16_t)(quotient + 30u * (uint16_t)(wNumRunAttempts - 1u));
    if (odds > 255u)
        return run_escapes();

    if ((uint8_t)odds >= BattleRandom())
        return run_escapes();

    wActionResultOrTookBattleTurn = 1;
    printf("[battle] Can't escape!\n");
    return 0;
}
