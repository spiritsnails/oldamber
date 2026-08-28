
#include "amberscript_checkpoint.h"
#include "amberscript_core.h"
#include "amberscript_saveops.h"
#include "amberscript_story.h"

#include "overworld.h"
#include "npc.h"
#include "text.h"
#include "trainer_sight.h"
#include "player.h"
#include "pokemon.h"
#include "inventory.h"
#include "badge.h"
#include "battle/battle_loop.h"
#include "../platform/hardware.h"
#include "../platform/save.h"
#include "../data/event_constants.h"
#include "constants.h"
#include "rockethideout_b4f_scripts.h"

#include <stdio.h>
#include <string.h>

extern int Game_WarpToRealMap(uint8_t real_id, int x, int y);

static void pks_force_interrupt_runtime(void) {
    extern void Game_SetScene(int);
    if (Text_IsOpen())
        Text_Close();
    wJoyIgnore = 0;
    hJoyHeld = 0;
    gScriptedMovement = 0;
    wIsInBattle = 0;
    wBattleResult = BATTLE_OUTCOME_NONE;
    Game_SetScene(0);
}

static void pks_checkpoint_apply(const char *cp) {
    if (strcmp(cp, "parcel_ready") == 0) {
        SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
        SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
        SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
        SetEvent(EVENT_GOT_STARTER);
        SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 5);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(0x2a, 3, 6);
        printf("[amberscript] checkpoint: parcel_ready -- at Viridian Mart\n");
    } else if (strcmp(cp, "pokedex_ready") == 0) {
        SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
        SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
        SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
        SetEvent(EVENT_GOT_STARTER);
        SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
        SetEvent(EVENT_GOT_OAKS_PARCEL);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 5);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(0x28, 6, 8);
        if (Inventory_GetQty(ITEM_OAKS_PARCEL) == 0)
            Inventory_Add(ITEM_OAKS_PARCEL, 1);
        printf("[amberscript] checkpoint: pokedex_ready -- at Oak's Lab with parcel\n");
    } else if (strcmp(cp, "mt_moon") == 0) {
        SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
        SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
        SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
        SetEvent(EVENT_GOT_STARTER);
        SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
        SetEvent(EVENT_GOT_OAKS_PARCEL);
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 25);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(0x3d, 13, 10);
        printf("[amberscript] checkpoint: mt_moon -- at Mt. Moon B2F, south of fossils\n");
    } else if (strcmp(cp, "cerulean") == 0) {
        SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
        SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
        SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
        SetEvent(EVENT_GOT_STARTER);
        SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
        SetEvent(EVENT_GOT_OAKS_PARCEL);
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 25);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(3, 20, 8);
        printf("[amberscript] checkpoint: cerulean -- at Cerulean City bridge (rival trigger at y=6), beat_rival=%d\n",
               CheckEvent(EVENT_BEAT_CERULEAN_RIVAL));
    } else if (strcmp(cp, "route22") == 0) {
        SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
        SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
        SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
        SetEvent(EVENT_GOT_STARTER);
        SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
        SetEvent(EVENT_GOT_OAKS_PARCEL);
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_1ST_ROUTE22_RIVAL_BATTLE);
        SetEvent(EVENT_ROUTE22_RIVAL_WANTS_BATTLE);
        ClearEvent(EVENT_BEAT_ROUTE22_RIVAL_1ST_BATTLE);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 10);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(0x21, 31, 5);
        printf("[amberscript] checkpoint: route22 -- on Route 22, walk left to trigger rival\n");
    } else if (strcmp(cp, "brock") == 0) {
        SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
        SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
        SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
        SetEvent(EVENT_GOT_STARTER);
        SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
        SetEvent(EVENT_GOT_OAKS_PARCEL);
        SetEvent(EVENT_GOT_POKEDEX);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 15);
            wPartyCount = 1;
        }
        ClearEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        ClearEvent(EVENT_BEAT_BROCK);
        Game_WarpToRealMap(0x36, 4, 2);
        printf("[amberscript] checkpoint: brock -- inside Pewter Gym\n");
    } else if (strcmp(cp, "misty") == 0) {
        SetEvent(EVENT_GOT_OAKS_PARCEL);
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
        ClearEvent(EVENT_BEAT_MISTY);
        ClearEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        ClearEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        ClearEvent(EVENT_GOT_TM11);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 25);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(0x41, 4, 8);
        printf("[amberscript] checkpoint: misty -- inside Cerulean Gym\n");
    } else if (strcmp(cp, "cerulean_rocket") == 0) {
        SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
        SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
        SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
        SetEvent(EVENT_GOT_STARTER);
        SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
        SetEvent(EVENT_GOT_OAKS_PARCEL);
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        SetEvent(EVENT_GOT_SS_TICKET);
        ClearEvent(EVENT_BEAT_CERULEAN_ROCKET_THIEF);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 25);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(3, 30, 9);
        printf("[amberscript] checkpoint: cerulean_rocket -- at Rocket thief trigger (30,9) in Cerulean City\n");
    } else if (strcmp(cp, "ss_anne_hm") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        ClearEvent(EVENT_SS_ANNE_LEFT);
        Inventory_Add(ITEM_SS_TICKET, 1);
        Inventory_Add(HM01, 1);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 25);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(0x5F, 26, 1);
        printf("[amberscript] checkpoint: ss_anne_hm -- inside SS Anne 1F, walk north to dock then departure\n");
    } else if (strcmp(cp, "liftkey_reset") == 0) {
        ClearEvent(EVENT_BEAT_ROCKET_HIDEOUT_4_TRAINER_2);
        ClearEvent(EVENT_ROCKET_DROPPED_LIFT_KEY);
        if (0xCA < 248)
            wPickedUpItems[0xCA] &= (uint16_t)~(1u << 4);
        if (wCurMap == 0xCA) {
            NPC_Load();
            RocketHideoutB4FScripts_OnMapLoad();
        }
        printf("[amberscript] checkpoint: liftkey_reset -- cleared flags 441/715 and B4F Lift Key pickup bit\n");
    } else if (strcmp(cp, "giovanni_reset") == 0) {
        ClearEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
        if (0xCA < 248)
            wPickedUpItems[0xCA] &= (uint16_t)~(1u << 3);
        if (wCurMap == 0xCA) {
            NPC_Load();
            RocketHideoutB4FScripts_OnMapLoad();
        }
        printf("[amberscript] checkpoint: giovanni_reset -- cleared Giovanni flag and B4F Silph Scope pickup bit\n");
    } else if (strcmp(cp, "giovanni_ready") == 0) {
        SetEvent(EVENT_FOUND_ROCKET_HIDEOUT);
        SetEvent(EVENT_ROCKET_DROPPED_LIFT_KEY);
        SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_4_TRAINER_0);
        SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_4_TRAINER_1);
        ClearEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
        if (Inventory_GetQty(0x4A) == 0)
            Inventory_Add(0x4A, 1);
        if (0xCA < 248)
            wPickedUpItems[0xCA] &= (uint16_t)~(1u << 3);
        Game_WarpToRealMap(0xCA, 25, 6);
        printf("[amberscript] checkpoint: giovanni_ready -- teleported to B4F in front of Giovanni, Lift Key granted\n");
    } else if (strcmp(cp, "silph_ready") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        SetEvent(EVENT_SS_ANNE_LEFT);
        SetEvent(EVENT_2ND_LOCK_OPENED);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_GOT_TM24);
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_GOT_TM21);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
        SetEvent(EVENT_RESCUED_MR_FUJI);
        SetEvent(EVENT_GOT_POKE_FLUTE);
        SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);

        wObtainedBadges |= (1u << BADGE_BOULDER);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        wObtainedBadges |= (1u << BADGE_RAINBOW);

        if (Inventory_GetQty(0x4A) == 0)
            Inventory_Add(0x4A, 1);
        if (Inventory_GetQty(0x30) == 0)
            Inventory_Add(0x30, 1);
        if (Inventory_GetQty(HM01) == 0)
            Inventory_Add(HM01, 1);
        if (Inventory_GetQty(ITEM_POKE_FLUTE) == 0)
            Inventory_Add(ITEM_POKE_FLUTE, 1);

        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 42);
            wPartyCount = 1;
        }

        Game_WarpToRealMap(0xCF, 19, 0);
        printf("[amberscript] checkpoint: silph_ready -- inside Silph Co 2F with Card Key + expected pre-Silph flags\n");
    } else if (strcmp(cp, "silph_rival_ready") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        SetEvent(EVENT_SS_ANNE_LEFT);
        SetEvent(EVENT_2ND_LOCK_OPENED);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_GOT_TM24);
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_GOT_TM21);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
        SetEvent(EVENT_RESCUED_MR_FUJI);
        SetEvent(EVENT_GOT_POKE_FLUTE);
        SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);
        ClearEvent(EVENT_BEAT_SILPH_CO_RIVAL);
        ClearEvent(EVENT_BEAT_SILPH_CO_GIOVANNI);
        ClearEvent(EVENT_GOT_MASTER_BALL);

        wObtainedBadges |= (1u << BADGE_BOULDER);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        wObtainedBadges |= (1u << BADGE_RAINBOW);

        if (Inventory_GetQty(0x30) == 0)
            Inventory_Add(0x30, 1);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 45);
            wPartyCount = 1;
        }

        Game_WarpToRealMap(0xD4, 3, 3);
        printf("[amberscript] checkpoint: silph_rival_ready -- Silph Co 7F, one step before rival trigger\n");
    } else if (strcmp(cp, "silph_giovanni_ready") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        SetEvent(EVENT_SS_ANNE_LEFT);
        SetEvent(EVENT_2ND_LOCK_OPENED);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_GOT_TM24);
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_GOT_TM21);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
        SetEvent(EVENT_RESCUED_MR_FUJI);
        SetEvent(EVENT_GOT_POKE_FLUTE);
        SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);
        SetEvent(EVENT_BEAT_SILPH_CO_RIVAL);
        ClearEvent(EVENT_BEAT_SILPH_CO_GIOVANNI);
        ClearEvent(EVENT_GOT_MASTER_BALL);

        wObtainedBadges |= (1u << BADGE_BOULDER);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        wObtainedBadges |= (1u << BADGE_RAINBOW);

        if (Inventory_GetQty(0x30) == 0)
            Inventory_Add(0x30, 1);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 47);
            wPartyCount = 1;
        }

        Game_WarpToRealMap(0xEB, 6, 14);
        printf("[amberscript] checkpoint: silph_giovanni_ready -- Silph Co 11F, one step before Giovanni trigger\n");
    } else if (strcmp(cp, "silph_lapras_ready") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        SetEvent(EVENT_SS_ANNE_LEFT);
        SetEvent(EVENT_2ND_LOCK_OPENED);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_GOT_TM24);
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_GOT_TM21);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
        SetEvent(EVENT_RESCUED_MR_FUJI);
        SetEvent(EVENT_GOT_POKE_FLUTE);
        SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);
        ClearEvent(EVENT_BEAT_SILPH_CO_GIOVANNI);
        ClearEvent(EVENT_GOT_MASTER_BALL);
        ClearEvent(EVENT_GOT_LAPRAS);

        wObtainedBadges |= (1u << BADGE_BOULDER);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        wObtainedBadges |= (1u << BADGE_RAINBOW);

        if (Inventory_GetQty(0x30) == 0)
            Inventory_Add(0x30, 1);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 44);
            wPartyCount = 1;
        }

        Game_WarpToRealMap(0xD4, 1, 6);
        printf("[amberscript] checkpoint: silph_lapras_ready -- Silph Co 7F, one step before Lapras worker\n");
    } else if (strcmp(cp, "silph_entry_locked") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        SetEvent(EVENT_SS_ANNE_LEFT);
        SetEvent(EVENT_2ND_LOCK_OPENED);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_GOT_TM24);
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_GOT_TM21);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
        SetEvent(EVENT_GOT_POKE_FLUTE);
        SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);
        ClearEvent(EVENT_RESCUED_MR_FUJI);
        ClearEvent(EVENT_RESCUED_MR_FUJI_2);
        ClearEvent(EVENT_BEAT_SILPH_CO_GIOVANNI);
        ClearEvent(EVENT_BEAT_SILPH_CO_RIVAL);
        ClearEvent(EVENT_GOT_MASTER_BALL);
        ClearEvent(EVENT_GOT_LAPRAS);

        wObtainedBadges |= (1u << BADGE_BOULDER);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        wObtainedBadges |= (1u << BADGE_RAINBOW);

        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 45);
            wPartyCount = 1;
        }

        Game_WarpToRealMap(0x0A, 18, 23);
        printf("[amberscript] checkpoint: silph_entry_locked -- Saffron, outside Silph with blocker active\n");
    } else if (strcmp(cp, "silph_entry_unlocked") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        SetEvent(EVENT_SS_ANNE_LEFT);
        SetEvent(EVENT_2ND_LOCK_OPENED);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_GOT_TM24);
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_GOT_TM21);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
        SetEvent(EVENT_GOT_POKE_FLUTE);
        SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);
        SetEvent(EVENT_RESCUED_MR_FUJI);
        SetEvent(EVENT_RESCUED_MR_FUJI_2);
        ClearEvent(EVENT_BEAT_SILPH_CO_GIOVANNI);
        ClearEvent(EVENT_BEAT_SILPH_CO_RIVAL);
        ClearEvent(EVENT_GOT_MASTER_BALL);
        ClearEvent(EVENT_GOT_LAPRAS);

        wObtainedBadges |= (1u << BADGE_BOULDER);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        wObtainedBadges |= (1u << BADGE_RAINBOW);

        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 45);
            wPartyCount = 1;
        }

        Game_WarpToRealMap(0x0A, 18, 23);
        printf("[amberscript] checkpoint: silph_entry_unlocked -- Saffron, Silph entrance unlocked\n");
    } else if (strcmp(cp, "surge") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        SetEvent(EVENT_SS_ANNE_LEFT);
        SetEvent(EVENT_2ND_LOCK_OPENED);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        ClearEvent(EVENT_BEAT_LT_SURGE);
        ClearEvent(EVENT_GOT_TM24);
        Inventory_Add(HM01, 1);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 35);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(0x5C, 5, 3);
        printf("[amberscript] checkpoint: surge -- inside Vermilion Gym, facing Lt. Surge\n");
    } else if (strcmp(cp, "sabrina") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        SetEvent(EVENT_SS_ANNE_LEFT);
        SetEvent(EVENT_2ND_LOCK_OPENED);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_GOT_TM24);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_GOT_TM21);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        wObtainedBadges |= (1u << BADGE_RAINBOW);
        SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
        SetEvent(EVENT_BEAT_SILPH_CO_GIOVANNI);
        SetEvent(EVENT_RESCUED_MR_FUJI);
        SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);
        SetEvent(EVENT_GOT_POKE_FLUTE);
        ClearEvent(EVENT_BEAT_SABRINA);
        ClearEvent(EVENT_GOT_TM46);
        ClearEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_0);
        ClearEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_1);
        ClearEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_2);
        ClearEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_3);
        ClearEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_4);
        ClearEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_5);
        ClearEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_6);

        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 48);
            wPartyCount = 1;
        }

        Game_WarpToRealMap(0xB2, 9, 9);
        printf("[amberscript] checkpoint: sabrina -- inside Saffron Gym, facing Sabrina\n");
    } else if (strcmp(cp, "erika") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        SetEvent(EVENT_SS_ANNE_LEFT);
        SetEvent(EVENT_2ND_LOCK_OPENED);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_GOT_TM24);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        ClearEvent(EVENT_BEAT_ERIKA);
        ClearEvent(EVENT_GOT_TM21);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 40);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(0x86, 4, 4);
        printf("[amberscript] checkpoint: erika -- inside Celadon Gym, facing Erika\n");
    } else if (strcmp(cp, "koga") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        SetEvent(EVENT_SS_ANNE_LEFT);
        SetEvent(EVENT_2ND_LOCK_OPENED);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_GOT_TM24);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_GOT_TM21);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        wObtainedBadges |= (1u << BADGE_RAINBOW);
        ClearEvent(EVENT_BEAT_KOGA);
        ClearEvent(EVENT_GOT_TM06);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_0);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_1);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_2);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_3);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_4);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_5);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 45);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(0x9D, 4, 11);
        printf("[amberscript] checkpoint: koga -- inside Fuchsia Gym, facing Koga\n");
    } else if (strcmp(cp, "blaine") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        SetEvent(EVENT_SS_ANNE_LEFT);
        SetEvent(EVENT_2ND_LOCK_OPENED);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_GOT_TM24);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_GOT_TM21);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        wObtainedBadges |= (1u << BADGE_RAINBOW);
        SetEvent(EVENT_BEAT_KOGA);
        SetEvent(EVENT_GOT_TM06);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_5);
        wObtainedBadges |= (1u << BADGE_SOUL);
        ClearEvent(EVENT_BEAT_BLAINE);
        ClearEvent(EVENT_GOT_TM38);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 52);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(0xA6, 3, 4);
        printf("[amberscript] checkpoint: blaine -- inside Cinnabar Gym, facing Blaine\n");
    } else if (strcmp(cp, "erika_post") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        SetEvent(EVENT_SS_ANNE_LEFT);
        SetEvent(EVENT_2ND_LOCK_OPENED);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_GOT_TM24);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        ClearEvent(EVENT_GOT_TM21);
        wObtainedBadges |= (1u << BADGE_RAINBOW);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 45);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(0x86, 4, 4);
        printf("[amberscript] checkpoint: erika_post -- Erika beaten, TM21 not obtained\n");
    } else if (strcmp(cp, "koga_post") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        SetEvent(EVENT_SS_ANNE_LEFT);
        SetEvent(EVENT_2ND_LOCK_OPENED);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_GOT_TM24);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_GOT_TM21);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        wObtainedBadges |= (1u << BADGE_RAINBOW);
        SetEvent(EVENT_BEAT_KOGA);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_5);
        ClearEvent(EVENT_GOT_TM06);
        wObtainedBadges |= (1u << BADGE_SOUL);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 50);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(0x9D, 4, 11);
        printf("[amberscript] checkpoint: koga_post -- Koga beaten, TM06 not obtained\n");
    } else if (strcmp(cp, "blaine_post") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        SetEvent(EVENT_SS_ANNE_LEFT);
        SetEvent(EVENT_2ND_LOCK_OPENED);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_GOT_TM24);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_GOT_TM21);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        wObtainedBadges |= (1u << BADGE_RAINBOW);
        SetEvent(EVENT_BEAT_KOGA);
        SetEvent(EVENT_GOT_TM06);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_5);
        wObtainedBadges |= (1u << BADGE_SOUL);
        SetEvent(EVENT_BEAT_BLAINE);
        ClearEvent(EVENT_GOT_TM38);
        wObtainedBadges |= (1u << BADGE_VOLCANO);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 56);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(0xA6, 3, 4);
        printf("[amberscript] checkpoint: blaine_post -- Blaine beaten, TM38 not obtained\n");
    } else if (strcmp(cp, "post_giovanni_victory") == 0 ||
               strcmp(cp, "post-giovanni-victory") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_BEAT_KOGA);
        SetEvent(EVENT_BEAT_SABRINA);
        SetEvent(EVENT_BEAT_BLAINE);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_GIOVANNI);

        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_6);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_7);

        wObtainedBadges |= (1u << BADGE_BOULDER);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        wObtainedBadges |= (1u << BADGE_RAINBOW);
        wObtainedBadges |= (1u << BADGE_SOUL);
        wObtainedBadges |= (1u << BADGE_MARSH);
        wObtainedBadges |= (1u << BADGE_VOLCANO);
        wObtainedBadges |= (1u << BADGE_EARTH);

        SetEvent(EVENT_BEAT_ROUTE22_RIVAL_1ST_BATTLE);
        ClearEvent(EVENT_1ST_ROUTE22_RIVAL_BATTLE);
        SetEvent(EVENT_2ND_ROUTE22_RIVAL_BATTLE);
        SetEvent(EVENT_ROUTE22_RIVAL_WANTS_BATTLE);
        ClearEvent(EVENT_BEAT_ROUTE22_RIVAL_2ND_BATTLE);

        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 55);
            wPartyCount = 1;
        }

        Game_WarpToRealMap(0x21, 31, 5);
        printf("[amberscript] checkpoint: post_giovanni_victory -- Route 22 rival-2 armed; walk left to trigger\n");
    } else if (strcmp(cp, "route23_guard_reset") == 0 ||
               strcmp(cp, "badge_guard_reset") == 0) {
        ClearEvent(EVENT_PASSED_CASCADEBADGE_CHECK);
        ClearEvent(EVENT_PASSED_THUNDERBADGE_CHECK);
        ClearEvent(EVENT_PASSED_RAINBOWBADGE_CHECK);
        ClearEvent(EVENT_PASSED_SOULBADGE_CHECK);
        ClearEvent(EVENT_PASSED_MARSHBADGE_CHECK);
        ClearEvent(EVENT_PASSED_VOLCANOBADGE_CHECK);
        ClearEvent(EVENT_PASSED_EARTHBADGE_CHECK);
        wObtainedBadges &= (uint8_t)~(1u << BADGE_BOULDER);
        ClearEvent(EVENT_BEAT_BROCK);
        printf("[amberscript] checkpoint: route23_guard_reset -- cleared Route 23 pass flags + Boulder badge/Brock win\n");
    } else if (strcmp(cp, "sabrina_post") == 0) {
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        SetEvent(EVENT_GOT_SS_TICKET);
        SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
        SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
        SetEvent(EVENT_GOT_HM01);
        SetEvent(EVENT_SS_ANNE_LEFT);
        SetEvent(EVENT_2ND_LOCK_OPENED);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_GOT_TM24);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_GOT_TM21);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        wObtainedBadges |= (1u << BADGE_RAINBOW);
        SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
        SetEvent(EVENT_BEAT_SILPH_CO_GIOVANNI);
        SetEvent(EVENT_RESCUED_MR_FUJI);
        SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);
        SetEvent(EVENT_GOT_POKE_FLUTE);
        SetEvent(EVENT_BEAT_SABRINA);
        SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_6);
        ClearEvent(EVENT_GOT_TM46);
        wObtainedBadges |= (1u << BADGE_MARSH);

        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 50);
            wPartyCount = 1;
        }

        Game_WarpToRealMap(0xB2, 9, 9);
        printf("[amberscript] checkpoint: sabrina_post -- Sabrina beaten, TM46 not obtained\n");
    } else if (strcmp(cp, "gym_badges4") == 0) {
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_BEAT_ERIKA);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        wObtainedBadges |= (1u << BADGE_RAINBOW);
        ClearEvent(EVENT_BEAT_KOGA);
        ClearEvent(EVENT_BEAT_BLAINE);
        printf("[amberscript] checkpoint: gym_badges4 -- set first 4 gym wins/badges\n");
    } else if (strcmp(cp, "gym_badges5") == 0) {
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_BEAT_KOGA);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        wObtainedBadges |= (1u << BADGE_RAINBOW);
        wObtainedBadges |= (1u << BADGE_SOUL);
        ClearEvent(EVENT_BEAT_BLAINE);
        printf("[amberscript] checkpoint: gym_badges5 -- set first 5 gym wins/badges\n");
    } else if (strcmp(cp, "gym_badges1") == 0) {
        SetEvent(EVENT_BEAT_BROCK);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        ClearEvent(EVENT_BEAT_MISTY);
        ClearEvent(EVENT_BEAT_LT_SURGE);
        ClearEvent(EVENT_BEAT_ERIKA);
        ClearEvent(EVENT_BEAT_KOGA);
        ClearEvent(EVENT_BEAT_BLAINE);
        printf("[amberscript] checkpoint: gym_badges1 -- set Brock win/badge only\n");
    } else if (strcmp(cp, "gym_badges2") == 0) {
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_MISTY);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        ClearEvent(EVENT_BEAT_LT_SURGE);
        ClearEvent(EVENT_BEAT_ERIKA);
        ClearEvent(EVENT_BEAT_KOGA);
        ClearEvent(EVENT_BEAT_BLAINE);
        printf("[amberscript] checkpoint: gym_badges2 -- set Brock+Misty wins/badges\n");
    } else if (strcmp(cp, "gym_badges3") == 0) {
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_LT_SURGE);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        wObtainedBadges |= (1u << BADGE_THUNDER);
        ClearEvent(EVENT_BEAT_ERIKA);
        ClearEvent(EVENT_BEAT_KOGA);
        ClearEvent(EVENT_BEAT_BLAINE);
        printf("[amberscript] checkpoint: gym_badges3 -- set Brock+Misty+Surge wins/badges\n");
    } else if (strcmp(cp, "brock_post") == 0) {
        SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
        SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
        SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
        SetEvent(EVENT_GOT_STARTER);
        SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
        SetEvent(EVENT_GOT_OAKS_PARCEL);
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        ClearEvent(EVENT_GOT_TM34);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 18);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(0x36, 4, 2);
        printf("[amberscript] checkpoint: brock_post -- Brock beaten, TM34 not obtained\n");
    } else if (strcmp(cp, "misty_post") == 0) {
        SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
        SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
        SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
        SetEvent(EVENT_GOT_STARTER);
        SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
        SetEvent(EVENT_GOT_OAKS_PARCEL);
        SetEvent(EVENT_GOT_POKEDEX);
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        wObtainedBadges |= (1u << BADGE_BOULDER);
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        ClearEvent(EVENT_GOT_TM11);
        wObtainedBadges |= (1u << BADGE_CASCADE);
        if (wPartyCount == 0) {
            Pokemon_InitMon(&wPartyMons[0], STARTER1, 26);
            wPartyCount = 1;
        }
        Game_WarpToRealMap(0x41, 4, 8);
        printf("[amberscript] checkpoint: misty_post -- Misty beaten, TM11 not obtained\n");
    } else if (strcmp(cp, "list") == 0 || strcmp(cp, "help") == 0) {
        printf("[amberscript] checkpoint list:\n");
        printf("  parcel_ready, pokedex_ready, route22, brock, mt_moon, cerulean,\n");
        printf("  misty, cerulean_rocket, ss_anne_hm, surge, erika, koga, sabrina, blaine,\n");
        printf("  brock_post, misty_post, erika_post, koga_post, sabrina_post, blaine_post,\n");
        printf("  post_giovanni_victory,\n");
        printf("  route23_guard_reset (alias: badge_guard_reset),\n");
        printf("  gym_badges1, gym_badges2, gym_badges3, gym_badges4, gym_badges5,\n");
        printf("  liftkey_reset, giovanni_reset, giovanni_ready, silph_ready,\n");
        printf("  silph_entry_locked, silph_entry_unlocked,\n");
        printf("  silph_rival_ready, silph_giovanni_ready, silph_lapras_ready\n");
    } else {
        printf("[amberscript] Unknown checkpoint: %s\n"
               "      Use: checkpoint list\n", cp);
    }
}

int AmberScript_Checkpoint_TryHandle(const char *cmd, const char *verb, int n) {
    (void)n;
    if (strcmp(verb, "checkpoint") != 0) return 0;

    char cp[32] = {0};
    sscanf(cmd, "%*s %31s", cp);

    pks_force_interrupt_runtime();

    if (strcmp(cp, "verify") == 0) {
        char target[32] = {0};
        char temp_path[160] = "bugs/cli_checkpoint_verify_tmp.state";
        if (!AmberScript_ParseArg(cmd, 2, target, sizeof(target)) || target[0] == '\0') {
            printf("[amberscript] checkpoint verify usage: checkpoint verify <name>\n");
            AmberScript_WriteState();
            return 1;
        }
        if (strcmp(target, "verify") == 0) {
            printf("[amberscript] checkpoint verify: refusing recursive target 'verify'\n");
            AmberScript_WriteState();
            return 1;
        }
        if (Save_StateWrite(temp_path) != 0) {
            printf("[amberscript] checkpoint verify: failed to save temp state\n");
            AmberScript_WriteState();
            return 1;
        }
        AmberScript_EventDiff_Snapshot();

        pks_checkpoint_apply(target);

        printf("[amberscript] checkpoint verify %s:\n", target);
        AmberScript_EventDiff_PrintDiff();

        if (Save_StateLoad(temp_path) == 0) AmberScript_ReloadAfterStateLoad();
        remove(temp_path);
        AmberScript_WriteState();
        return 1;
    }

    pks_checkpoint_apply(cp);
    AmberScript_WriteState();
    return 1;
}
