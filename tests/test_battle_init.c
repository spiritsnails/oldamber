
#include "test_runner.h"
#include "../src/game/battle/battle_init.h"
#include "../src/game/pokemon.h"
#include "../src/platform/hardware.h"
#include "../src/game/constants.h"

static void init_reset(void) {
    extern void WRAMClear(void);
    WRAMClear();
    wIsInBattle  = 1;
    wPartyCount  = 1;
    wPartyMons[0].base.species  = SPECIES_BULBASAUR;
    wPartyMons[0].base.hp       = 45;
    wPartyMons[0].level         = 5;
    wPartyMons[0].max_hp        = 45;
    wPartyMons[0].base.moves[0] = 1;
    wPartyMons[0].base.pp[0]    = 35;
    wPlayerMonNumber = 0;
}

TEST(WriteMovesForLevel, level7_populates_one_move) {
    uint8_t moves[4] = {0};
    uint8_t pp[4]    = {0};
    Pokemon_WriteMovesForLevel(moves, pp, SPECIES_RATTATA, 7);

    EXPECT_TRUE(moves[0] != 0);
    EXPECT_TRUE(pp[0] > 0);

    EXPECT_EQ(moves[1], 0);
    EXPECT_EQ(moves[2], 0);
    EXPECT_EQ(moves[3], 0);
}

TEST(WriteMovesForLevel, higher_level_more_moves) {
    uint8_t moves_lv1[4] = {0}; uint8_t pp_lv1[4] = {0};
    uint8_t moves_lv20[4] = {0}; uint8_t pp_lv20[4] = {0};
    Pokemon_WriteMovesForLevel(moves_lv1,  pp_lv1,  SPECIES_RATTATA, 1);
    Pokemon_WriteMovesForLevel(moves_lv20, pp_lv20, SPECIES_RATTATA, 20);

    int cnt1 = 0, cnt20 = 0;
    for (int i = 0; i < 4; i++) {
        if (moves_lv1[i])  cnt1++;
        if (moves_lv20[i]) cnt20++;
    }
    EXPECT_TRUE(cnt20 >= cnt1);
}

TEST(WriteMovesForLevel, pp_populated) {
    uint8_t moves[4] = {0};
    uint8_t pp[4]    = {0};
    Pokemon_WriteMovesForLevel(moves, pp, SPECIES_RATTATA, 20);
    for (int i = 0; i < 4; i++) {
        if (moves[i]) {
            EXPECT_TRUE(pp[i] > 0);
        }
    }
}

TEST(WriteMovesForLevel, species_zero_noop) {
    uint8_t moves[4] = {0};
    uint8_t pp[4]    = {0};
    Pokemon_WriteMovesForLevel(moves, pp, 0, 50);
    int any = 0;
    for (int i = 0; i < 4; i++) if (moves[i]) any = 1;
    EXPECT_TRUE(!any);
}

TEST(ReadTrainer, brock_party_count) {
    init_reset();
    Battle_ReadTrainer(34, 1);
    EXPECT_EQ(wEnemyPartyCount, 2);
}

TEST(ReadTrainer, brock_geodude_level) {
    init_reset();
    Battle_ReadTrainer(34, 1);
    EXPECT_EQ(wEnemyMons[0].base.species, SPECIES_GEODUDE);
    EXPECT_EQ(wEnemyMons[0].level, 12);
}

TEST(ReadTrainer, brock_onix_level) {
    init_reset();
    Battle_ReadTrainer(34, 1);
    EXPECT_EQ(wEnemyMons[1].base.species, SPECIES_ONIX);
    EXPECT_EQ(wEnemyMons[1].level, 14);
}

TEST(ReadTrainer, youngster1_format_a) {
    init_reset();
    Battle_ReadTrainer(1, 1);
    EXPECT_EQ(wEnemyPartyCount, 2);
    EXPECT_EQ(wEnemyMons[0].base.species, SPECIES_RATTATA);
    EXPECT_EQ(wEnemyMons[0].level, 11);
    EXPECT_EQ(wEnemyMons[1].base.species, SPECIES_EKANS);
    EXPECT_EQ(wEnemyMons[1].level, 11);
}

TEST(ReadTrainer, youngster2_trainer_no) {
    init_reset();
    Battle_ReadTrainer(1, 2);
    EXPECT_EQ(wEnemyPartyCount, 1);
    EXPECT_EQ(wEnemyMons[0].base.species, SPECIES_SPEAROW);
    EXPECT_EQ(wEnemyMons[0].level, 14);
}

TEST(ReadTrainer, rival1_oakslab_party_1_squirtle) {
    init_reset();
    Battle_ReadTrainer(25, 1);
    EXPECT_EQ(wEnemyPartyCount, 1);
    EXPECT_EQ(wEnemyMons[0].base.species, SPECIES_SQUIRTLE);
    EXPECT_EQ(wEnemyMons[0].level, 5);
}

TEST(ReadTrainer, rival1_oakslab_party_2_bulbasaur) {
    init_reset();
    Battle_ReadTrainer(25, 2);
    EXPECT_EQ(wEnemyPartyCount, 1);
    EXPECT_EQ(wEnemyMons[0].base.species, SPECIES_BULBASAUR);
    EXPECT_EQ(wEnemyMons[0].level, 5);
}

TEST(ReadTrainer, rival1_oakslab_party_3_charmander) {
    init_reset();
    Battle_ReadTrainer(25, 3);
    EXPECT_EQ(wEnemyPartyCount, 1);
    EXPECT_EQ(wEnemyMons[0].base.species, SPECIES_CHARMANDER);
    EXPECT_EQ(wEnemyMons[0].level, 5);
}

TEST(ReadTrainer, rival1_route22_party_1_uses_squirtle) {
    init_reset();
    Battle_ReadTrainer(25, 4);
    EXPECT_EQ(wEnemyPartyCount, 2);
    EXPECT_EQ(wEnemyMons[0].base.species, SPECIES_PIDGEY);
    EXPECT_EQ(wEnemyMons[0].level, 9);
    EXPECT_EQ(wEnemyMons[1].base.species, SPECIES_SQUIRTLE);
    EXPECT_EQ(wEnemyMons[1].level, 8);
}

TEST(ReadTrainer, rival1_cerulean_party_1_uses_squirtle) {
    init_reset();
    Battle_ReadTrainer(25, 7);
    EXPECT_EQ(wEnemyPartyCount, 4);
    EXPECT_EQ(wEnemyMons[0].base.species, SPECIES_PIDGEOTTO);
    EXPECT_EQ(wEnemyMons[0].level, 18);
    EXPECT_EQ(wEnemyMons[3].base.species, SPECIES_SQUIRTLE);
    EXPECT_EQ(wEnemyMons[3].level, 17);
}

TEST(ReadTrainer, mons_start_at_full_hp) {
    init_reset();
    Battle_ReadTrainer(35, 1);
    EXPECT_TRUE(wEnemyMons[0].base.hp > 0);
    EXPECT_EQ(wEnemyMons[0].base.hp, wEnemyMons[0].max_hp);
    EXPECT_TRUE(wEnemyMons[1].base.hp > 0);
    EXPECT_EQ(wEnemyMons[1].base.hp, wEnemyMons[1].max_hp);
}

TEST(StartTrainer, wIsInBattle_is_2) {
    init_reset();
    Battle_StartTrainer(34, 1);
    EXPECT_EQ(wIsInBattle, 2);
}

TEST(StartTrainer, wAICount_is_ff) {
    init_reset();
    Battle_StartTrainer(34, 1);
    EXPECT_EQ(wAICount, 0xFF);
}

TEST(StartTrainer, first_enemy_mon_loaded) {
    init_reset();
    Battle_StartTrainer(34, 1);
    EXPECT_EQ(wEnemyMon.species, SPECIES_GEODUDE);
    EXPECT_EQ(wEnemyMon.level, 12);
    EXPECT_TRUE(wEnemyMon.hp > 0);
}

TEST(StartTrainer, player_mon_loaded) {
    init_reset();
    Battle_StartTrainer(34, 1);
    EXPECT_EQ(wBattleMon.species, SPECIES_BULBASAUR);
    EXPECT_EQ(wBattleMon.hp, 45);
}

TEST(StartTrainer, enemy_party_pos_set) {
    init_reset();
    Battle_StartTrainer(34, 1);
    EXPECT_EQ(wEnemyMonPartyPos, 0);
}

TEST(StartTrainer, misty_enemy_party_count) {
    init_reset();
    Battle_StartTrainer(35, 1);
    EXPECT_EQ(wEnemyPartyCount, 2);
    EXPECT_EQ(wEnemyMon.species, SPECIES_STARYU);
}

TEST(ReadTrainer, lonemoves_brock_onix_bide) {
    init_reset();
    wLoneAttackNo = 1;
    Battle_ReadTrainer(34, 1);
    EXPECT_EQ(wEnemyMons[1].base.moves[2], MOVE_BIDE);
}

TEST(ReadTrainer, lonemoves_misty_starmie_bubblebeam) {
    init_reset();
    wLoneAttackNo = 2;
    Battle_ReadTrainer(35, 1);
    EXPECT_EQ(wEnemyMons[1].base.moves[2], MOVE_BUBBLEBEAM);
}

TEST(ReadTrainer, lonemoves_format_a_not_applied) {
    init_reset();
    wLoneAttackNo = 1;
    Battle_ReadTrainer(1, 1);

    EXPECT_TRUE(wEnemyMons[1].base.moves[2] != MOVE_BIDE);
}

TEST(ReadTrainer, lonemoves_zero_not_applied) {
    init_reset();
    wLoneAttackNo = 0;
    Battle_ReadTrainer(34, 1);

    EXPECT_TRUE(wEnemyMons[1].base.moves[2] != MOVE_BIDE);
}

TEST(ReadTrainer, teammoves_lorelei_blizzard) {
    init_reset();
    wLoneAttackNo = 0;
    Battle_ReadTrainer(44, 1);
    EXPECT_TRUE(wEnemyPartyCount >= 5);
    EXPECT_EQ(wEnemyMons[4].base.moves[2], MOVE_BLIZZARD);
}

TEST(ReadTrainer, teammoves_bruno_fissure) {
    init_reset();
    wLoneAttackNo = 0;
    Battle_ReadTrainer(33, 1);
    EXPECT_TRUE(wEnemyPartyCount >= 5);
    EXPECT_EQ(wEnemyMons[4].base.moves[2], MOVE_FISSURE);
}

TEST(ReadTrainer, rival3_bulbasaur_starter) {
    init_reset();
    wLoneAttackNo  = 0;
    wRivalStarter  = STARTER3;
    Battle_ReadTrainer(43, 1);
    EXPECT_TRUE(wEnemyPartyCount >= 6);
    EXPECT_EQ(wEnemyMons[0].base.moves[2], MOVE_SKY_ATTACK);
    EXPECT_EQ(wEnemyMons[5].base.moves[2], MOVE_MEGA_DRAIN);
}

TEST(ReadTrainer, rival3_charmander_starter) {
    init_reset();
    wLoneAttackNo  = 0;
    wRivalStarter  = STARTER1;
    Battle_ReadTrainer(43, 1);
    EXPECT_TRUE(wEnemyPartyCount >= 6);
    EXPECT_EQ(wEnemyMons[0].base.moves[2], MOVE_SKY_ATTACK);
    EXPECT_EQ(wEnemyMons[5].base.moves[2], MOVE_FIRE_BLAST);
}
