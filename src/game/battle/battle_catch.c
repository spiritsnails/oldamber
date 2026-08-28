
#include "battle_catch.h"
#include "battle_probe.h"
#include "../../platform/hardware.h"
#include "../constants.h"
#include "battle_core.h"

catch_result_t Battle_CatchAttempt(uint8_t ball_id) {

    if (Battle_IsGhostBattle())
        return CATCH_RESULT_CANNOT_CATCH;
    if (wIsInBattle == 1 && Battle_MapIsPokemonTower() &&
        wEnemyMon.species == SPECIES_MAROWAK)
        return CATCH_RESULT_CANNOT_CATCH;

    BPROBE("ItemUseBall.loop");

    uint8_t rand1;
    for (;;) {
        rand1 = BattleRandom();
        if (ball_id == ITEM_MASTER_BALL) return CATCH_RESULT_SUCCESS;
        if (ball_id == ITEM_POKE_BALL)  break;
        if (rand1 > 200)                continue;
        if (ball_id == ITEM_GREAT_BALL) break;
        if (rand1 > 150)                continue;
        break;
    }

    uint8_t status = wEnemyMon.status;
    if (status) {
        uint8_t sub;
        if (status & ((1 << 5) | 0x07))
            sub = 25;
        else
            sub = 12;
        if (sub > rand1) return CATCH_RESULT_SUCCESS;
        rand1 -= sub;
    }

    uint8_t ball_factor = (ball_id == ITEM_GREAT_BALL) ? 8 : 12;

    uint32_t num   = (uint32_t)wEnemyMon.max_hp * 255 / ball_factor;
    uint8_t  hp4   = (uint8_t)(wEnemyMon.hp >> 2);
    if (hp4 == 0) hp4 = 1;
    uint32_t W     = num / hp4;

    uint8_t  X     = (W > 255) ? 255 : (uint8_t)W;

    uint8_t catch_rate = wEnemyMonActualCatchRate;
    if (rand1 > catch_rate) goto fail;

    if (W > 255) return CATCH_RESULT_SUCCESS;

    {
        uint8_t rand2 = BattleRandom();
        if (rand2 > X) goto fail;
    }

    return CATCH_RESULT_SUCCESS;

fail:;

    uint8_t ball_factor2;
    if      (ball_id == ITEM_POKE_BALL)  ball_factor2 = 255;
    else if (ball_id == ITEM_GREAT_BALL) ball_factor2 = 200;
    else                                  ball_factor2 = 150;

    uint32_t Y = (uint32_t)catch_rate * 100 / ball_factor2;

    if (Y > 255) return CATCH_RESULT_3_SHAKES;

    uint32_t Z_raw = (uint32_t)X * (uint8_t)Y / 255;
    uint8_t  Z     = (Z_raw > 255) ? 255 : (uint8_t)Z_raw;

    if (status) {
        uint8_t bonus;
        if (status & ((1 << 5) | 0x07))
            bonus = 10;
        else
            bonus = 5;
        uint16_t z2 = (uint16_t)Z + bonus;
        Z = (z2 > 255) ? 255 : (uint8_t)z2;
    }

    if (Z <  10) return CATCH_RESULT_0_SHAKES;
    if (Z <  30) return CATCH_RESULT_1_SHAKE;
    if (Z <  70) return CATCH_RESULT_2_SHAKES;
    return CATCH_RESULT_3_SHAKES;
}
