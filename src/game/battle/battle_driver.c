
#include "battle_driver.h"
#include "battle_loop.h"
#include "battle_exp.h"
#include "battle_switch.h"
#include "battle_trainer.h"
#include "../../platform/hardware.h"
#include "../../data/base_stats.h"
#include "../../data/moves_data.h"
#include "../constants.h"
#include "../pokemon.h"
#include <stdio.h>

static int find_next_alive_party_slot(void) {
    for (uint8_t i = 0; i < wPartyCount; i++) {
        if (i != wPlayerMonNumber && wPartyMons[i].base.hp > 0) {
            return (int)i;
        }
    }
    return -1;
}

void Battle_RunLoop(void) {
    uint8_t p_dex = gSpeciesToDex[wBattleMon.species];
    uint8_t e_dex = gSpeciesToDex[wEnemyMon.species];

    if (wIsInBattle == 2) {
        printf("[battle] Trainer sent out %s Lv.%d!\n",
               Pokemon_GetName(e_dex), wEnemyMon.level);
    } else {
        printf("[battle] Wild %s Lv.%d (HP %d/%d) appeared!\n",
               Pokemon_GetName(e_dex), wEnemyMon.level,
               wEnemyMon.hp, wEnemyMon.max_hp);
    }
    printf("[battle] Go! %s Lv.%d (HP %d/%d)\n",
           Pokemon_GetName(p_dex), wBattleMon.level,
           wBattleMon.hp, wBattleMon.max_hp);

    int turn = 0;
    for (;;) {
        turn++;

        wPlayerSelectedMove = MOVE_STRUGGLE;
        for (int i = 0; i < 4; i++) {
            if (wBattleMon.moves[i]) {
                wPlayerSelectedMove = wBattleMon.moves[i];
                break;
            }
        }

        printf("[battle] --- Turn %d ---\n", turn);

        battle_result_t r = Battle_RunTurn();

        p_dex = gSpeciesToDex[wBattleMon.species];
        e_dex = gSpeciesToDex[wEnemyMon.species];
        printf("[battle]   %s HP: %d/%d  |  %s HP: %d/%d\n",
               Pokemon_GetName(p_dex), wBattleMon.hp, wBattleMon.max_hp,
               Pokemon_GetName(e_dex), wEnemyMon.hp,  wEnemyMon.max_hp);

        if (r == BATTLE_RESULT_ENEMY_FAINTED) {

            if (wBattleResult == BATTLE_OUTCOME_WILD_VICTORY) {
                printf("[battle] Wild %s fainted!\n", Pokemon_GetName(e_dex));
                break;
            }
            if (wBattleResult == BATTLE_OUTCOME_TRAINER_VICTORY) {
                printf("[battle] Trainer is out of usable Monster!\n");
                break;
            }
            if (wBattleResult == BATTLE_OUTCOME_BLACKOUT) {
                printf("[battle] You have no more usable Monster!\n");
                break;
            }

            if (wForcePlayerToChooseMon) {
                wForcePlayerToChooseMon = 0;
                int next = find_next_alive_party_slot();
                if (next < 0) {
                    Battle_HandlePlayerBlackOut();
                    printf("[battle] You have no more usable Monster!\n");
                    break;
                }
                Battle_ChooseNextMon((uint8_t)next);
                p_dex = gSpeciesToDex[wBattleMon.species];
                printf("[battle] Go! %s Lv.%d!\n",
                       Pokemon_GetName(p_dex), wBattleMon.level);
            }
            e_dex = gSpeciesToDex[wEnemyMon.species];
            printf("[battle] Trainer sent out %s Lv.%d!\n",
                   Pokemon_GetName(e_dex), wEnemyMon.level);
            continue;
        }

        if (r == BATTLE_RESULT_PLAYER_FAINTED) {

            printf("[battle] %s fainted!\n", Pokemon_GetName(p_dex));

            if (!Battle_AnyPartyAlive()) {
                Battle_HandlePlayerBlackOut();
                printf("[battle] You blacked out!\n");
                break;
            }

            if (wEnemyMon.hp == 0) {

                wBoostExpByExpAll = 0;
                Battle_GainExperience();

                if (wIsInBattle != 2) {

                    printf("[battle] Wild %s fainted too!\n", Pokemon_GetName(e_dex));
                    break;
                }

                if (!Battle_AnyEnemyPokemonAliveCheck()) {
                    Battle_TrainerBattleVictory();
                    printf("[battle] Trainer is out of usable Monster!\n");
                    break;
                }

                Battle_ReplaceFaintedEnemyMon();
                e_dex = gSpeciesToDex[wEnemyMon.species];
                printf("[battle] Trainer sent out %s Lv.%d!\n",
                       Pokemon_GetName(e_dex), wEnemyMon.level);
            }

            int next = find_next_alive_party_slot();
            if (next < 0) {
                Battle_HandlePlayerBlackOut();
                printf("[battle] You blacked out!\n");
                break;
            }
            Battle_ChooseNextMon((uint8_t)next);
            p_dex = gSpeciesToDex[wBattleMon.species];
            printf("[battle] Go! %s Lv.%d!\n",
                   Pokemon_GetName(p_dex), wBattleMon.level);
            continue;
        }

        if (r == BATTLE_RESULT_ESCAPED) {
            printf("[battle] Got away safely!\n");
            break;
        }

        if (turn >= 50) {
            printf("[battle] Turn limit reached — ending battle.\n");
            break;
        }
    }

    wPartyMons[wPlayerMonNumber].base.hp = wBattleMon.hp;
    wIsInBattle = 0;

    Battle_EvolutionAfterBattle();
}
