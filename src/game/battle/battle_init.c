
#include "battle_init.h"
#include "battle_trainer.h"
#include <stdio.h>
#include "../../platform/hardware.h"
#include "../../data/base_stats.h"
#include "../../data/moves_data.h"
#include "../../data/trainer_data.h"
#include "../constants.h"
#include "../pokemon.h"
#include "../type_mod.h"
#include "../music.h"
#include "../pokedex.h"
#include "../rival_starter.h"
#include "../safari_zone_scripts.h"
#include <string.h>

static int s_pending_old_man_type = 0;

void Battle_RequestOldManType(void) {
    s_pending_old_man_type = 1;
}

void Battle_Start(void) {

    wPlayerBattleStatus1 = wPlayerBattleStatus2 = wPlayerBattleStatus3 = 0;
    wEnemyBattleStatus1  = wEnemyBattleStatus2  = wEnemyBattleStatus3  = 0;
    memset(wPlayerMonStatMods, STAT_STAGE_NORMAL, sizeof(wPlayerMonStatMods));
    memset(wEnemyMonStatMods,  STAT_STAGE_NORMAL, sizeof(wEnemyMonStatMods));
    wPlayerConfusedCounter = wEnemyConfusedCounter = 0;
    wPlayerToxicCounter    = wEnemyToxicCounter    = 0;
    wPlayerDisabledMove    = wEnemyDisabledMove    = 0;
    wPlayerNumAttacksLeft  = wEnemyNumAttacksLeft  = 0;
    wPlayerNumHits         = wEnemyNumHits         = 0;
    wPlayerBideAccumulatedDamage = wEnemyBideAccumulatedDamage = 0;
    wPlayerSubstituteHP    = wEnemySubstituteHP    = 0;
    wPlayerMonMinimized    = wEnemyMonMinimized    = 0;
    wPlayerDisabledMoveNumber = wEnemyDisabledMoveNumber = 0;
    wPlayerSelectedMove    = wEnemySelectedMove    = 0;
    wEscapedFromBattle     = 0;
    wMoveDidntMiss         = 0;
    wDamage                = 0;
    wDamageMultipliers     = DAMAGE_MULT_EFFECTIVE;
    wFirstMonsNotOutYet    = 1;
    wIsInBattle            = 1;
    wBattleType            = 0;
    hWhoseTurn             = 0;

    if (SafariZoneScripts_MapHasWildEncounters(wCurMap))
        wBattleType = 2;
    if (s_pending_old_man_type) {
        wBattleType = 1;
        s_pending_old_man_type = 0;
    }

    printf("[OMDBG] Battle_Start: wBattleType=%d (safari_map=%d)\n",
           (int)wBattleType, SafariZoneScripts_MapHasWildEncounters(wCurMap));
    fflush(stdout);

    wPlayerMonNumber = 0;
    for (uint8_t i = 0; i < wPartyCount; i++) {
        if (wPartyMons[i].base.hp > 0) {
            wPlayerMonNumber = i;
            break;
        }
    }

    wPartyGainExpFlags            = (uint8_t)(1u << wPlayerMonNumber);
    wPartyFoughtCurrentEnemyFlags = (uint8_t)(1u << wPlayerMonNumber);

    for (int i = 0; i < wPartyCount && i < PARTY_LENGTH; i++)
        wPartySpecies[i] = wPartyMons[i].base.species;
    wPartySpecies[wPartyCount < PARTY_LENGTH ? wPartyCount : PARTY_LENGTH] = 0xFF;

    const party_mon_t *p = &wPartyMons[wPlayerMonNumber];

    wBattleMon.species    = p->base.species;
    wBattleMon.hp         = p->base.hp;
    wBattleMon.party_pos  = 0;
    wBattleMon.status     = p->base.status;
    TypeMod_GetSpeciesTypes(p->base.species, &wBattleMon.type1, &wBattleMon.type2);
    wBattleMon.catch_rate = p->base.catch_rate;
    wEnemyMonActualCatchRate = 0;
    memcpy(wBattleMon.moves, p->base.moves, 4);
    wBattleMon.dvs        = p->base.dvs;
    wBattleMon.level      = p->level;
    wBattleMon.max_hp     = p->max_hp;
    wBattleMon.atk        = p->atk;
    wBattleMon.def        = p->def;
    wBattleMon.spd        = p->spd;
    wBattleMon.spc        = p->spc;

    memcpy(wBattleMon.pp, p->base.pp, 4);

    wPlayerMonUnmodifiedAttack  = wBattleMon.atk;
    wPlayerMonUnmodifiedDefense = wBattleMon.def;
    wPlayerMonUnmodifiedSpeed   = wBattleMon.spd;
    wPlayerMonUnmodifiedSpecial = wBattleMon.spc;

    uint8_t dex = gSpeciesToDex[wCurPartySpecies];
    const base_stats_t *b = &gBaseStats[dex];
    uint8_t lv = wCurEnemyLevel;

    uint8_t dv_atk = BattleRandom() & 0x0F;
    uint8_t dv_def = BattleRandom() & 0x0F;
    uint8_t dv_spd = BattleRandom() & 0x0F;
    uint8_t dv_spc = BattleRandom() & 0x0F;

    uint8_t dv_hp = (uint8_t)(((dv_atk & 1) << 3) | ((dv_def & 1) << 2) |
                               ((dv_spd & 1) << 1) |  (dv_spc & 1));

    wEnemyMon.species    = wCurPartySpecies;
    Pokedex_SetSeen(wCurPartySpecies);
    wEnemyMon.hp         = CalcStat(b->hp,  dv_hp,  0, lv, 1);
    wEnemyMon.party_pos  = 0;
    wEnemyMon.status     = 0;
    TypeMod_GetSpeciesTypes(wEnemyMon.species, &wEnemyMon.type1, &wEnemyMon.type2);
    wEnemyMon.catch_rate = b->catch_rate;
    wEnemyMonActualCatchRate = wEnemyMon.catch_rate;
    wEnemyMon.dvs        = (uint16_t)((dv_atk << 12) | (dv_def << 8) | (dv_spd << 4) | dv_spc);
    wEnemyMon.level      = lv;
    wEnemyMon.max_hp     = CalcStat(b->hp,  dv_hp,  0, lv, 1);
    wEnemyMon.atk        = CalcStat(b->atk, dv_atk, 0, lv, 0);
    wEnemyMon.def        = CalcStat(b->def, dv_def, 0, lv, 0);
    wEnemyMon.spd        = CalcStat(b->spd, dv_spd, 0, lv, 0);
    wEnemyMon.spc        = CalcStat(b->spc, dv_spc, 0, lv, 0);

    for (int i = 0; i < 4; i++) {
        uint8_t mid = b->start_moves[i];
        wEnemyMon.moves[i] = mid;
        wEnemyMon.pp[i] = (mid && mid < NUM_MOVE_DEFS) ? gMoves[mid].pp : 0;
    }
    Pokemon_WriteMovesForLevel(wEnemyMon.moves, wEnemyMon.pp, wCurPartySpecies, lv);

    wEnemyMonUnmodifiedAttack  = wEnemyMon.atk;
    wEnemyMonUnmodifiedDefense = wEnemyMon.def;
    wEnemyMonUnmodifiedSpeed   = wEnemyMon.spd;
    wEnemyMonUnmodifiedSpecial = wEnemyMon.spc;

}

static void add_enemy_mon_to_party(uint8_t slot, uint8_t species, uint8_t level) {
    if (slot >= PARTY_LENGTH) return;
    uint8_t dex = gSpeciesToDex[species];
    if (dex == 0) return;
    const base_stats_t *b = &gBaseStats[dex];

    party_mon_t *m = &wEnemyMons[slot];
    memset(m, 0, sizeof(*m));

    uint8_t dv_atk = TRAINER_ATK_DV;
    uint8_t dv_def = TRAINER_DEF_DV;
    uint8_t dv_spd = TRAINER_SPD_DV;
    uint8_t dv_spc = TRAINER_SPC_DV;
    uint8_t dv_hp  = (uint8_t)(((dv_atk & 1) << 3) | ((dv_def & 1) << 2) |
                                ((dv_spd & 1) << 1) |  (dv_spc & 1));

    m->base.species    = species;
    TypeMod_GetSpeciesTypes(species, &m->base.type1, &m->base.type2);
    m->base.catch_rate = b->catch_rate;
    m->base.dvs        = (uint16_t)((dv_atk << 12) | (dv_def << 8) | (dv_spd << 4) | dv_spc);
    m->level           = level;

    for (int i = 0; i < 4; i++) {
        uint8_t mid = b->start_moves[i];
        m->base.moves[i] = mid;
        m->base.pp[i] = (mid && mid < NUM_MOVE_DEFS) ? gMoves[mid].pp : 0;
    }
    Pokemon_WriteMovesForLevel(m->base.moves, m->base.pp, species, level);

    uint16_t max_hp    = CalcStat(b->hp,  dv_hp,  0, level, 1);
    m->base.hp         = max_hp;
    m->max_hp          = max_hp;
    m->atk             = CalcStat(b->atk, dv_atk, 0, level, 0);
    m->def             = CalcStat(b->def, dv_def, 0, level, 0);
    m->spd             = CalcStat(b->spd, dv_spd, 0, level, 0);
    m->spc             = CalcStat(b->spc, dv_spc, 0, level, 0);
}

static void build_pallet_test_trainer_party(void) {
    static const uint8_t kTestSpecies[PARTY_LENGTH] = {
        SPECIES_CHANSEY, SPECIES_SNORLAX, SPECIES_CLOYSTER,
        SPECIES_MUK, SPECIES_WEEZING, SPECIES_TANGELA
    };
    static const uint8_t kTestMoves[4] = {
        MOVE_REST, MOVE_REST, MOVE_REST, MOVE_REST
    };

    wEnemyPartyCount = PARTY_LENGTH;
    for (uint8_t i = 0; i < PARTY_LENGTH; i++) {
        add_enemy_mon_to_party(i, kTestSpecies[i], 100);
        for (uint8_t j = 0; j < 4; j++) {
            uint8_t move = kTestMoves[j];
            wEnemyMons[i].base.moves[j] = move;
            wEnemyMons[i].base.pp[j] = (move && move < NUM_MOVE_DEFS) ? gMoves[move].pp : 0;
        }
    }
}

uint8_t Battle_PeekTrainerLevelForTransition(uint8_t trainer_class, uint8_t trainer_no) {
    if (trainer_class < 1 || trainer_class > NUM_TRAINERS) return 0;
    if (trainer_no < 1) return 0;
    if (trainer_class == 27 && trainer_no == 255) return 100;

    const uint8_t *data = gTrainerPartyData[trainer_class - 1];
    for (uint8_t skip = 1; skip < trainer_no; skip++) {
        if (*data == TRAINER_PARTY_FMT_B) {
            data++;
            while (*data != 0) data += 2;
        } else {
            data++;
            while (*data != 0) data++;
        }
        data++;
    }

    if (*data == TRAINER_PARTY_FMT_B) {

        data++;
        uint8_t level = 0;
        while (*data != 0) {
            level = *data++;
            data++;
        }
        return level;
    }

    return *data;
}

void Battle_ReadTrainer(uint8_t trainer_class, uint8_t trainer_no) {
    if (trainer_class < 1 || trainer_class > NUM_TRAINERS) return;
    if (trainer_no < 1) return;

    if (trainer_class == 27 && trainer_no == 255) {
        build_pallet_test_trainer_party();
        wAmountMoneyWon = 0;
        return;
    }

    const uint8_t *data = gTrainerPartyData[trainer_class - 1];

    for (uint8_t skip = 1; skip < trainer_no; skip++) {

        if (*data == TRAINER_PARTY_FMT_B) {
            data++;
            while (*data != 0) data += 2;
        } else {
            data++;
            while (*data != 0) data++;
        }
        data++;
    }

    wEnemyPartyCount = 0;
    int is_fmt_b = 0;
    if (*data == TRAINER_PARTY_FMT_B) {
        is_fmt_b = 1;

        data++;
        while (*data != 0 && wEnemyPartyCount < PARTY_LENGTH) {
            uint8_t lv      = *data++;
            uint8_t species = *data++;
            add_enemy_mon_to_party(wEnemyPartyCount, species, lv);
            wEnemyPartyCount++;
        }
    } else {

        uint8_t lv = *data++;
        while (*data != 0 && wEnemyPartyCount < PARTY_LENGTH) {
            uint8_t species = *data++;
            add_enemy_mon_to_party(wEnemyPartyCount, species, lv);
            wEnemyPartyCount++;
        }
    }

    {
        uint16_t base = (trainer_class >= 1 && trainer_class <= NUM_TRAINERS)
                        ? gTrainerBaseMoney[trainer_class - 1] : 0;

        wAmountMoneyWon = (uint32_t)(base / 100) * wEnemyMons[wEnemyPartyCount - 1].level;
    }

    if (!is_fmt_b) return;

    if (wLoneAttackNo != 0) {

        static const uint8_t lone_moves[8][2] = {
            {1, MOVE_BIDE},
            {1, MOVE_BUBBLEBEAM},
            {2, MOVE_THUNDERBOLT},
            {2, MOVE_MEGA_DRAIN},
            {3, MOVE_TOXIC},
            {3, MOVE_PSYWAVE},
            {3, MOVE_FIRE_BLAST},
            {4, MOVE_FISSURE},
        };
        uint8_t idx = wLoneAttackNo - 1;
        if (idx < 8) {
            uint8_t slot = lone_moves[idx][0];
            uint8_t mid  = lone_moves[idx][1];
            if (slot < wEnemyPartyCount)
                wEnemyMons[slot].base.moves[2] = mid;
        }
        return;
    }

    static const uint8_t team_moves[5][2] = {
        {OPP_LORELEI - OPP_ID_OFFSET, MOVE_BLIZZARD},
        {OPP_BRUNO   - OPP_ID_OFFSET, MOVE_FISSURE},
        {OPP_AGATHA  - OPP_ID_OFFSET, MOVE_TOXIC},
        {OPP_LANCE   - OPP_ID_OFFSET, MOVE_BARRIER},
        {0xFF, 0},
    };
    for (int i = 0; team_moves[i][0] != 0xFF; i++) {
        if (team_moves[i][0] == trainer_class) {
            if (4 < wEnemyPartyCount)
                wEnemyMons[4].base.moves[2] = team_moves[i][1];
            return;
        }
    }

    if (trainer_class == (OPP_RIVAL3 - OPP_ID_OFFSET)) {
        if (wEnemyPartyCount > 0)
            wEnemyMons[0].base.moves[2] = MOVE_SKY_ATTACK;
        uint8_t rival_move;
        uint8_t rival_starter = RivalStarter_Get();
        if      (rival_starter == STARTER3) rival_move = MOVE_MEGA_DRAIN;
        else if (rival_starter == STARTER1) rival_move = MOVE_FIRE_BLAST;
        else                                rival_move = MOVE_BLIZZARD;
        if (wEnemyPartyCount > 5)
            wEnemyMons[5].base.moves[2] = rival_move;
    }
}

void Battle_StartTrainer(uint8_t trainer_class, uint8_t trainer_no) {

    wPlayerBattleStatus1 = wPlayerBattleStatus2 = wPlayerBattleStatus3 = 0;
    wEnemyBattleStatus1  = wEnemyBattleStatus2  = wEnemyBattleStatus3  = 0;
    memset(wPlayerMonStatMods, STAT_STAGE_NORMAL, sizeof(wPlayerMonStatMods));
    memset(wEnemyMonStatMods,  STAT_STAGE_NORMAL, sizeof(wEnemyMonStatMods));
    wPlayerConfusedCounter = wEnemyConfusedCounter = 0;
    wPlayerToxicCounter    = wEnemyToxicCounter    = 0;
    wPlayerDisabledMove    = wEnemyDisabledMove    = 0;
    wPlayerNumAttacksLeft  = wEnemyNumAttacksLeft  = 0;
    wPlayerNumHits         = wEnemyNumHits         = 0;
    wPlayerBideAccumulatedDamage = wEnemyBideAccumulatedDamage = 0;
    wPlayerSubstituteHP    = wEnemySubstituteHP    = 0;
    wPlayerMonMinimized    = wEnemyMonMinimized    = 0;
    wPlayerDisabledMoveNumber = wEnemyDisabledMoveNumber = 0;
    wPlayerSelectedMove    = wEnemySelectedMove    = 0;
    wEscapedFromBattle     = 0;
    wMoveDidntMiss         = 0;
    wDamage                = 0;
    wDamageMultipliers     = DAMAGE_MULT_EFFECTIVE;
    wFirstMonsNotOutYet    = 1;
    wBattleType            = 0;
    hWhoseTurn             = 0;

    wIsInBattle   = 2;
    wAICount      = 0xFF;
    wTrainerClass = trainer_class;

    wPlayerMonNumber = 0;
    for (uint8_t i = 0; i < wPartyCount; i++) {
        if (wPartyMons[i].base.hp > 0) {
            wPlayerMonNumber = i;
            break;
        }
    }

    wPartyGainExpFlags            = (uint8_t)(1u << wPlayerMonNumber);
    wPartyFoughtCurrentEnemyFlags = (uint8_t)(1u << wPlayerMonNumber);

    for (int i = 0; i < wPartyCount && i < PARTY_LENGTH; i++)
        wPartySpecies[i] = wPartyMons[i].base.species;
    wPartySpecies[wPartyCount < PARTY_LENGTH ? wPartyCount : PARTY_LENGTH] = 0xFF;

    const party_mon_t *p = &wPartyMons[wPlayerMonNumber];
    wBattleMon.species    = p->base.species;
    wBattleMon.hp         = p->base.hp;
    wBattleMon.party_pos  = 0;
    wBattleMon.status     = p->base.status;
    wBattleMon.type1      = p->base.type1;
    wBattleMon.type2      = p->base.type2;
    wBattleMon.catch_rate = p->base.catch_rate;
    memcpy(wBattleMon.moves, p->base.moves, 4);
    wBattleMon.dvs        = p->base.dvs;
    wBattleMon.level      = p->level;
    wBattleMon.max_hp     = p->max_hp;
    wBattleMon.atk        = p->atk;
    wBattleMon.def        = p->def;
    wBattleMon.spd        = p->spd;
    wBattleMon.spc        = p->spc;
    memcpy(wBattleMon.pp, p->base.pp, 4);

    wPlayerMonUnmodifiedAttack  = wBattleMon.atk;
    wPlayerMonUnmodifiedDefense = wBattleMon.def;
    wPlayerMonUnmodifiedSpeed   = wBattleMon.spd;
    wPlayerMonUnmodifiedSpecial = wBattleMon.spc;

    Battle_ReadTrainer(trainer_class, trainer_no);

    wEnemyMonPartyPos = 0xFF;
    Battle_EnemySendOut_State();
}

void Battle_StartTrainerCustomDebug(uint8_t trainer_class,
                                    const uint8_t species[6],
                                    const uint8_t level[6],
                                    const uint8_t moves[6][4],
                                    uint8_t count) {

    if (trainer_class < 1 || trainer_class > NUM_TRAINERS) trainer_class = 34;
    Battle_StartTrainer(trainer_class, 1);

    memset(wEnemyMons, 0, sizeof(wEnemyMons));
    wEnemyPartyCount = 0;

    if (count > PARTY_LENGTH) count = PARTY_LENGTH;
    for (uint8_t i = 0; i < count; i++) {
        uint8_t sp = species ? species[i] : 0;
        uint8_t lv = level ? level[i] : 0;
        if (sp == 0 || lv == 0) continue;
        Pokemon_InitMon(&wEnemyMons[wEnemyPartyCount], sp, lv);
        if (moves) {
            for (int m = 0; m < 4; m++) {
                uint8_t mv = moves[i][m];
                wEnemyMons[wEnemyPartyCount].base.moves[m] = mv;
                wEnemyMons[wEnemyPartyCount].base.pp[m] =
                    (mv && mv < NUM_MOVE_DEFS) ? gMoves[mv].pp : 0;
            }
        }
        wEnemyPartyCount++;
    }

    if (wEnemyPartyCount == 0) {
        Pokemon_InitMon(&wEnemyMons[0], STARTER1, 5);
        wEnemyPartyCount = 1;
    }

    wEnemyMonPartyPos = 0xFF;
    Battle_EnemySendOut_State();
    wAmountMoneyWon = 0;
}
