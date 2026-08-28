
#include "battle_loop.h"
#include "battle_core.h"
#include "battle.h"
#include "battle_ai.h"
#include "../../platform/hardware.h"
#include "game/battle/battle_probe.h"

static int s_turn_player_first = 1;

battle_result_t Battle_TurnPrepare(void) {
    wFirstMonsNotOutYet = 0;

    if ((uint8_t)wPlayerMonNumber < wPartyCount) {
        wPartyMons[wPlayerMonNumber].base.hp     = wBattleMon.hp;
        wPartyMons[wPlayerMonNumber].base.status = wBattleMon.status;
    }

    if (wBattleMon.hp == 0) {
        Battle_HandlePlayerMonFainted();
        return BATTLE_RESULT_PLAYER_FAINTED;
    }
    if (wEnemyMon.hp == 0) {
        Battle_HandleEnemyMonFainted();
        return BATTLE_RESULT_ENEMY_FAINTED;
    }

    if (!(wPlayerBattleStatus2 & ((1u << BSTAT2_NEEDS_TO_RECHARGE) |
                                  (1u << BSTAT2_USING_RAGE)))) {
        wEnemyBattleStatus1  &= ~(1u << BSTAT1_FLINCHED);
        wPlayerBattleStatus1 &= ~(1u << BSTAT1_FLINCHED);
    }

    if (!(wPlayerBattleStatus2 & ((1u << BSTAT2_NEEDS_TO_RECHARGE) |
                                  (1u << BSTAT2_USING_RAGE))) &&
        !(wPlayerBattleStatus1 & ((1u << BSTAT1_THRASHING_ABOUT) |
                                  (1u << BSTAT1_CHARGING_UP))) &&
        !(wBattleMon.status & (STATUS_FRZ | STATUS_SLP_MASK)) &&
        !(wPlayerBattleStatus1 & ((1u << BSTAT1_STORING_ENERGY) |
                                  (1u << BSTAT1_USING_TRAPPING))) &&
         (wEnemyBattleStatus1 & (1u << BSTAT1_USING_TRAPPING)))
        wPlayerSelectedMove = CANNOT_MOVE;

    Battle_SelectEnemyMove();

    BLOG("--- Turn: %s Lv%d %d/%d HP vs %s Lv%d %d/%d HP",
         BMON_P(), wBattleMon.level, wBattleMon.hp, wBattleMon.max_hp,
         BMON_E(), wEnemyMon.level,  wEnemyMon.hp,  wEnemyMon.max_hp);
    BLOG("    Player -> %-12s | Enemy -> %s",
         BMOVE(wPlayerSelectedMove), BMOVE(wEnemySelectedMove));

    {
        int p_quick   = (wPlayerSelectedMove == MOVE_QUICK_ATTACK);
        int e_quick   = (wEnemySelectedMove  == MOVE_QUICK_ATTACK);
        int p_counter = (wPlayerSelectedMove == MOVE_COUNTER);
        int e_counter = (wEnemySelectedMove  == MOVE_COUNTER);

        if      (p_quick   && !e_quick)   s_turn_player_first = 1;
        else if (e_quick   && !p_quick)   s_turn_player_first = 0;
        else if (p_counter && !e_counter) s_turn_player_first = 0;
        else if (e_counter && !p_counter) s_turn_player_first = 1;
        else {
            if      (wBattleMon.spd > wEnemyMon.spd) s_turn_player_first = 1;
            else if (wEnemyMon.spd > wBattleMon.spd) s_turn_player_first = 0;
            else    s_turn_player_first = (BattleRandom() < 128);
        }
    }

    return BATTLE_RESULT_CONTINUE;
}

int Battle_TurnPlayerFirst(void) {
    return s_turn_player_first;
}

void Battle_SelectEnemyMove(void) {
    BPROBE("SelectEnemyMove");

    if (wEnemyBattleStatus2 & ((1u << BSTAT2_NEEDS_TO_RECHARGE) |
                                (1u << BSTAT2_USING_RAGE)))
        return;
    if (wEnemyBattleStatus1 & ((1u << BSTAT1_CHARGING_UP) |
                                (1u << BSTAT1_THRASHING_ABOUT)))
        return;
    if (wEnemyMon.status & (STATUS_FRZ | STATUS_SLP_MASK))
        return;
    if (wEnemyBattleStatus1 & ((1u << BSTAT1_USING_TRAPPING) |
                                (1u << BSTAT1_STORING_ENERGY)))
        return;

    if (wPlayerBattleStatus1 & (1u << BSTAT1_USING_TRAPPING)) {
        wEnemySelectedMove = CANNOT_MOVE;
        return;
    }

    if (wEnemyMon.moves[1] == 0 && wEnemyDisabledMove != 0) {
        wEnemySelectedMove = MOVE_STRUGGLE;
        return;
    }

    const uint8_t *movesrc = wEnemyMon.moves;
    uint8_t ai_moves[4];
    if (wIsInBattle == 2)
        movesrc = AI_EnemyTrainerChooseMoves(ai_moves);

    {
        uint8_t slot, move;
        do {
            uint8_t r = BattleRandom();
            if      (r < 63)  slot = 0;
            else if (r < 127) slot = 1;
            else if (r < 190) slot = 2;
            else              slot = 3;

            uint8_t dis = (wEnemyDisabledMove >> 4) & 0x0F;
            if (dis != 0 && dis == (uint8_t)(slot + 1)) continue;

            move = movesrc[slot];
        } while (move == 0);

        wEnemyMoveListIndex = slot;
        wEnemySelectedMove  = move;
    }

}

void Battle_CheckNumAttacksLeft(void) {
    if (wPlayerNumAttacksLeft == 0)
        wPlayerBattleStatus1 &= ~(1u << BSTAT1_USING_TRAPPING);
    if (wEnemyNumAttacksLeft == 0)
        wEnemyBattleStatus1  &= ~(1u << BSTAT1_USING_TRAPPING);
}

battle_result_t Battle_RunTurn(void) {
    battle_result_t prep = Battle_TurnPrepare();
    if (prep != BATTLE_RESULT_CONTINUE) return prep;

    if (!s_turn_player_first) {

        hWhoseTurn = 1;

        if (!AI_TrainerAI()) {
            Battle_ExecuteEnemyMove();
            if (wEscapedFromBattle) return BATTLE_RESULT_ESCAPED;
            if (wBattleMon.hp == 0) {
                Battle_HandlePlayerMonFainted();
                return BATTLE_RESULT_PLAYER_FAINTED;
            }
        }

        if (!Battle_HandlePoisonBurnLeechSeed()) {
            Battle_HandleEnemyMonFainted();
            return BATTLE_RESULT_ENEMY_FAINTED;
        }
        Battle_ExecutePlayerMove();
        if (wEscapedFromBattle) return BATTLE_RESULT_ESCAPED;
        if (wEnemyMon.hp == 0) {
            Battle_HandleEnemyMonFainted();
            return BATTLE_RESULT_ENEMY_FAINTED;
        }

        if (!Battle_HandlePoisonBurnLeechSeed()) {
            Battle_HandlePlayerMonFainted();
            return BATTLE_RESULT_PLAYER_FAINTED;
        }
    } else {

        Battle_ExecutePlayerMove();
        if (wEscapedFromBattle) return BATTLE_RESULT_ESCAPED;
        if (wEnemyMon.hp == 0) {
            Battle_HandleEnemyMonFainted();
            return BATTLE_RESULT_ENEMY_FAINTED;
        }

        if (!Battle_HandlePoisonBurnLeechSeed()) {
            Battle_HandlePlayerMonFainted();
            return BATTLE_RESULT_PLAYER_FAINTED;
        }
        hWhoseTurn = 1;

        if (!AI_TrainerAI()) {
            Battle_ExecuteEnemyMove();
            if (wEscapedFromBattle) return BATTLE_RESULT_ESCAPED;
            if (wBattleMon.hp == 0) {
                Battle_HandlePlayerMonFainted();
                return BATTLE_RESULT_PLAYER_FAINTED;
            }
        }

        if (!Battle_HandlePoisonBurnLeechSeed()) {
            Battle_HandleEnemyMonFainted();
            return BATTLE_RESULT_ENEMY_FAINTED;
        }
    }

    Battle_CheckNumAttacksLeft();
    return BATTLE_RESULT_CONTINUE;
}
