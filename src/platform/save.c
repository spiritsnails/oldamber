
#include "save.h"
#include "compiler.h"
#include "game_version.h"
#include "hardware.h"
#include "../game/constants.h"
#include "../game/types.h"
#include "../game/player.h"
#include "../game/overworld.h"
#include "../game/npc.h"
#include "../game/trainer_sight.h"
#include "../game/town_map.h"
#include "../game/pokecenter.h"
#include "../game/amberscript_mapbank.h"
#include "../game/amberscript_core.h"
#include "../data/map_data.h"
#include "../data/event_constants.h"

static void save_vmap_sidecar_load(void);
static void save_npc_rt_sidecar_load(void);
#include <stdio.h>
#include <string.h>
#include <stddef.h>

extern int  Game_GetScene(void);
extern void Game_SetScene(int);

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

#define SAVE_EVENT_FLAGS_BYTES     352
#define SAVE_EVENT_FLAGS_EXT_BYTES (EVENT_FLAGS_BYTES - SAVE_EVENT_FLAGS_BYTES)

typedef struct PACKED {
    uint8_t     player_name[NAME_LENGTH];

    uint8_t     pokedex_owned[19];
    uint8_t     pokedex_seen[19];
    uint8_t     num_bag_items;
    uint8_t     bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t     player_money[3];
    uint8_t     rival_name[NAME_LENGTH];
    uint8_t     options;
    uint8_t     badges;
    uint8_t     _pad1;
    uint8_t     letter_delay_flags;
    uint16_t    player_id;
    uint8_t     cur_map;
    uint8_t     last_map;
    uint8_t     y_coord;
    uint8_t     x_coord;
    uint8_t     event_flags[SAVE_EVENT_FLAGS_BYTES];
    uint8_t     game_progress_flags[256];
    uint16_t    picked_up_items[248];

    uint8_t     party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t     party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     party_nicks[PARTY_LENGTH][NAME_LENGTH];

    uint8_t     checksum;
} save_block_v1_t;

typedef struct PACKED {
    uint8_t     player_name[NAME_LENGTH];

    uint8_t     pokedex_owned[19];
    uint8_t     pokedex_seen[19];
    uint8_t     num_bag_items;
    uint8_t     bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t     player_money[3];
    uint8_t     rival_name[NAME_LENGTH];
    uint8_t     options;
    uint8_t     badges;
    uint8_t     _pad1;
    uint8_t     letter_delay_flags;
    uint16_t    player_id;
    uint8_t     cur_map;
    uint8_t     last_map;
    uint8_t     y_coord;
    uint8_t     x_coord;
    uint8_t     event_flags[SAVE_EVENT_FLAGS_BYTES];
    uint8_t     game_progress_flags[256];
    uint16_t    picked_up_items[248];

    uint8_t     party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t     party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     party_nicks[PARTY_LENGTH][NAME_LENGTH];

    uint8_t     current_box_num;
    uint8_t     box_count[NUM_BOXES];
    uint8_t     box_species[NUM_BOXES][BOX_CAPACITY + 1];
    box_mon_t   box_mons[NUM_BOXES][BOX_CAPACITY];
    uint8_t     box_ot[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t     box_nicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];

    uint8_t     checksum;
} save_block_v2_t;

typedef struct PACKED {
    uint8_t     player_name[NAME_LENGTH];
    uint8_t     pokedex_owned[19];
    uint8_t     pokedex_seen[19];
    uint8_t     num_bag_items;
    uint8_t     bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t     player_money[3];
    uint8_t     rival_name[NAME_LENGTH];
    uint8_t     options;
    uint8_t     badges;
    uint8_t     _pad1;
    uint8_t     letter_delay_flags;
    uint16_t    player_id;
    uint8_t     cur_map;
    uint8_t     last_map;
    uint8_t     y_coord;
    uint8_t     x_coord;
    uint8_t     event_flags[SAVE_EVENT_FLAGS_BYTES];
    uint8_t     game_progress_flags[256];
    uint16_t    picked_up_items[248];
    uint8_t     party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t     party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     party_nicks[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     current_box_num;
    uint8_t     box_count[NUM_BOXES];
    uint8_t     box_species[NUM_BOXES][BOX_CAPACITY + 1];
    box_mon_t   box_mons[NUM_BOXES][BOX_CAPACITY];
    uint8_t     box_ot[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t     box_nicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint16_t    completed_trade_flags;
    uint8_t     checksum;
} save_block_v3_t;

typedef struct PACKED {
    uint8_t     player_name[NAME_LENGTH];
    uint8_t     pokedex_owned[19];
    uint8_t     pokedex_seen[19];
    uint8_t     num_bag_items;
    uint8_t     bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t     player_money[3];
    uint8_t     rival_name[NAME_LENGTH];
    uint8_t     options;
    uint8_t     badges;
    uint8_t     _pad1;
    uint8_t     letter_delay_flags;
    uint16_t    player_id;
    uint8_t     cur_map;
    uint8_t     last_map;
    uint8_t     y_coord;
    uint8_t     x_coord;
    uint8_t     event_flags[SAVE_EVENT_FLAGS_BYTES];
    uint8_t     game_progress_flags[256];
    uint16_t    picked_up_items[248];
    uint8_t     party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t     party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     party_nicks[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     current_box_num;
    uint8_t     box_count[NUM_BOXES];
    uint8_t     box_species[NUM_BOXES][BOX_CAPACITY + 1];
    box_mon_t   box_mons[NUM_BOXES][BOX_CAPACITY];
    uint8_t     box_ot[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t     box_nicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint16_t    completed_trade_flags;
    uint8_t     num_box_items;
    uint8_t     box_items[PC_ITEM_CAPACITY * 2 + 1];
    uint8_t     checksum;
} save_block_v4_t;

typedef struct PACKED {
    uint8_t     player_name[NAME_LENGTH];
    uint8_t     pokedex_owned[19];
    uint8_t     pokedex_seen[19];
    uint8_t     num_bag_items;
    uint8_t     bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t     player_money[3];
    uint8_t     rival_name[NAME_LENGTH];
    uint8_t     options;
    uint8_t     badges;
    uint8_t     _pad1;
    uint8_t     letter_delay_flags;
    uint16_t    player_id;
    uint8_t     cur_map;
    uint8_t     last_map;
    uint8_t     y_coord;
    uint8_t     x_coord;
    uint8_t     event_flags[SAVE_EVENT_FLAGS_BYTES];
    uint8_t     game_progress_flags[256];
    uint16_t    picked_up_items[248];
    uint8_t     party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t     party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     party_nicks[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     current_box_num;
    uint8_t     box_count[NUM_BOXES];
    uint8_t     box_species[NUM_BOXES][BOX_CAPACITY + 1];
    box_mon_t   box_mons[NUM_BOXES][BOX_CAPACITY];
    uint8_t     box_ot[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t     box_nicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint16_t    completed_trade_flags;
    uint8_t     num_box_items;
    uint8_t     box_items[PC_ITEM_CAPACITY * 2 + 1];
    uint8_t     used_pokecenter;
    uint8_t     checksum;
} save_block_v5_t;

typedef struct PACKED {
    uint8_t     player_name[NAME_LENGTH];
    uint8_t     pokedex_owned[19];
    uint8_t     pokedex_seen[19];
    uint8_t     num_bag_items;
    uint8_t     bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t     player_money[3];
    uint8_t     rival_name[NAME_LENGTH];
    uint8_t     options;
    uint8_t     badges;
    uint8_t     _pad1;
    uint8_t     letter_delay_flags;
    uint16_t    player_id;
    uint8_t     cur_map;
    uint8_t     last_map;
    uint8_t     y_coord;
    uint8_t     x_coord;
    uint8_t     event_flags[SAVE_EVENT_FLAGS_BYTES];
    uint8_t     game_progress_flags[256];
    uint16_t    picked_up_items[248];
    uint8_t     party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t     party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     party_nicks[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     current_box_num;
    uint8_t     box_count[NUM_BOXES];
    uint8_t     box_species[NUM_BOXES][BOX_CAPACITY + 1];
    box_mon_t   box_mons[NUM_BOXES][BOX_CAPACITY];
    uint8_t     box_ot[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t     box_nicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];

    uint16_t    completed_trade_flags;

    uint8_t     num_box_items;
    uint8_t     box_items[PC_ITEM_CAPACITY * 2 + 1];

    uint8_t     used_pokecenter;

    uint16_t    safari_steps;
    uint8_t     safari_balls;

    uint8_t     checksum;
} save_block_v6_t;

typedef struct PACKED {
    uint8_t     player_name[NAME_LENGTH];
    uint8_t     pokedex_owned[19];
    uint8_t     pokedex_seen[19];
    uint8_t     num_bag_items;
    uint8_t     bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t     player_money[3];
    uint8_t     rival_name[NAME_LENGTH];
    uint8_t     options;
    uint8_t     badges;
    uint8_t     _pad1;
    uint8_t     letter_delay_flags;
    uint16_t    player_id;
    uint8_t     cur_map;
    uint8_t     last_map;
    uint8_t     y_coord;
    uint8_t     x_coord;
    uint8_t     event_flags[SAVE_EVENT_FLAGS_BYTES];
    uint8_t     game_progress_flags[256];
    uint16_t    picked_up_items[248];
    uint8_t     party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t     party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     party_nicks[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     current_box_num;
    uint8_t     box_count[NUM_BOXES];
    uint8_t     box_species[NUM_BOXES][BOX_CAPACITY + 1];
    box_mon_t   box_mons[NUM_BOXES][BOX_CAPACITY];
    uint8_t     box_ot[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t     box_nicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint16_t    completed_trade_flags;
    uint8_t     num_box_items;
    uint8_t     box_items[PC_ITEM_CAPACITY * 2 + 1];
    uint8_t     used_pokecenter;
    uint16_t    safari_steps;
    uint8_t     safari_balls;

    uint8_t     daycare_in_use;
    box_mon_t   daycare_mon;
    uint8_t     daycare_mon_ot[NAME_LENGTH];
    uint8_t     daycare_mon_name[NAME_LENGTH];

    uint8_t     checksum;
} save_block_v7_t;

typedef struct PACKED {
    uint8_t     player_name[NAME_LENGTH];
    uint8_t     pokedex_owned[19];
    uint8_t     pokedex_seen[19];
    uint8_t     num_bag_items;
    uint8_t     bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t     player_money[3];
    uint8_t     rival_name[NAME_LENGTH];
    uint8_t     options;
    uint8_t     badges;
    uint8_t     _pad1;
    uint8_t     letter_delay_flags;
    uint16_t    player_id;
    uint8_t     cur_map;
    uint8_t     last_map;
    uint8_t     y_coord;
    uint8_t     x_coord;
    uint8_t     event_flags[SAVE_EVENT_FLAGS_BYTES];
    uint8_t     game_progress_flags[256];
    uint16_t    picked_up_items[248];
    uint8_t     party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t     party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     party_nicks[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     current_box_num;
    uint8_t     box_count[NUM_BOXES];
    uint8_t     box_species[NUM_BOXES][BOX_CAPACITY + 1];
    box_mon_t   box_mons[NUM_BOXES][BOX_CAPACITY];
    uint8_t     box_ot[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t     box_nicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint16_t    completed_trade_flags;
    uint8_t     num_box_items;
    uint8_t     box_items[PC_ITEM_CAPACITY * 2 + 1];
    uint8_t     used_pokecenter;
    uint16_t    safari_steps;
    uint8_t     safari_balls;
    uint8_t     daycare_in_use;
    box_mon_t   daycare_mon;
    uint8_t     daycare_mon_ot[NAME_LENGTH];
    uint8_t     daycare_mon_name[NAME_LENGTH];

    uint8_t     first_lock_can;
    uint8_t     second_lock_can;

    uint8_t     checksum;
} save_block_v8_t;

typedef struct PACKED {
    uint8_t     player_name[NAME_LENGTH];
    uint8_t     pokedex_owned[19];
    uint8_t     pokedex_seen[19];
    uint8_t     num_bag_items;
    uint8_t     bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t     player_money[3];
    uint8_t     rival_name[NAME_LENGTH];
    uint8_t     options;
    uint8_t     badges;
    uint8_t     _pad1;
    uint8_t     letter_delay_flags;
    uint16_t    player_id;
    uint8_t     cur_map;
    uint8_t     last_map;
    uint8_t     y_coord;
    uint8_t     x_coord;
    uint8_t     event_flags[SAVE_EVENT_FLAGS_BYTES];
    uint8_t     game_progress_flags[256];
    uint16_t    picked_up_items[248];
    uint8_t     party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t     party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     party_nicks[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     current_box_num;
    uint8_t     box_count[NUM_BOXES];
    uint8_t     box_species[NUM_BOXES][BOX_CAPACITY + 1];
    box_mon_t   box_mons[NUM_BOXES][BOX_CAPACITY];
    uint8_t     box_ot[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t     box_nicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint16_t    completed_trade_flags;
    uint8_t     num_box_items;
    uint8_t     box_items[PC_ITEM_CAPACITY * 2 + 1];
    uint8_t     used_pokecenter;
    uint16_t    safari_steps;
    uint8_t     safari_balls;
    uint8_t     daycare_in_use;
    box_mon_t   daycare_mon;
    uint8_t     daycare_mon_ot[NAME_LENGTH];
    uint8_t     daycare_mon_name[NAME_LENGTH];
    uint8_t     first_lock_can;
    uint8_t     second_lock_can;
    uint8_t     player_coins[2];
    uint8_t     checksum;
} save_block_v9_t;

typedef struct PACKED {
    uint8_t     player_name[NAME_LENGTH];
    uint8_t     pokedex_owned[19];
    uint8_t     pokedex_seen[19];
    uint8_t     num_bag_items;
    uint8_t     bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t     player_money[3];
    uint8_t     rival_name[NAME_LENGTH];
    uint8_t     options;
    uint8_t     badges;
    uint8_t     _pad1;
    uint8_t     letter_delay_flags;
    uint16_t    player_id;
    uint8_t     cur_map;
    uint8_t     last_map;
    uint8_t     y_coord;
    uint8_t     x_coord;
    uint8_t     event_flags[SAVE_EVENT_FLAGS_BYTES];
    uint8_t     game_progress_flags[256];
    uint16_t    picked_up_items[248];
    uint8_t     party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t     party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     party_nicks[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     current_box_num;
    uint8_t     box_count[NUM_BOXES];
    uint8_t     box_species[NUM_BOXES][BOX_CAPACITY + 1];
    box_mon_t   box_mons[NUM_BOXES][BOX_CAPACITY];
    uint8_t     box_ot[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t     box_nicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint16_t    completed_trade_flags;
    uint8_t     num_box_items;
    uint8_t     box_items[PC_ITEM_CAPACITY * 2 + 1];
    uint8_t     used_pokecenter;
    uint16_t    safari_steps;
    uint8_t     safari_balls;
    uint8_t     daycare_in_use;
    box_mon_t   daycare_mon;
    uint8_t     daycare_mon_ot[NAME_LENGTH];
    uint8_t     daycare_mon_name[NAME_LENGTH];
    uint8_t     first_lock_can;
    uint8_t     second_lock_can;
    uint8_t     player_coins[2];

    uint8_t     hand_authored_flags[PKS_HANDAUTHORED_EVENT_BYTES];

    uint8_t     checksum;
} save_block_v10_t;

typedef struct PACKED {
    uint8_t     player_name[NAME_LENGTH];
    uint8_t     pokedex_owned[19];
    uint8_t     pokedex_seen[19];
    uint8_t     num_bag_items;
    uint8_t     bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t     player_money[3];
    uint8_t     rival_name[NAME_LENGTH];
    uint8_t     options;
    uint8_t     badges;
    uint8_t     _pad1;
    uint8_t     letter_delay_flags;
    uint16_t    player_id;
    uint8_t     cur_map;
    uint8_t     last_map;
    uint8_t     y_coord;
    uint8_t     x_coord;
    uint8_t     event_flags[SAVE_EVENT_FLAGS_BYTES];
    uint8_t     game_progress_flags[256];
    uint16_t    picked_up_items[248];
    uint8_t     party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t     party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     party_nicks[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     current_box_num;
    uint8_t     box_count[NUM_BOXES];
    uint8_t     box_species[NUM_BOXES][BOX_CAPACITY + 1];
    box_mon_t   box_mons[NUM_BOXES][BOX_CAPACITY];
    uint8_t     box_ot[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t     box_nicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint16_t    completed_trade_flags;
    uint8_t     num_box_items;
    uint8_t     box_items[PC_ITEM_CAPACITY * 2 + 1];
    uint8_t     used_pokecenter;
    uint16_t    safari_steps;
    uint8_t     safari_balls;
    uint8_t     daycare_in_use;
    box_mon_t   daycare_mon;
    uint8_t     daycare_mon_ot[NAME_LENGTH];
    uint8_t     daycare_mon_name[NAME_LENGTH];
    uint8_t     first_lock_can;
    uint8_t     second_lock_can;
    uint8_t     player_coins[2];
    uint8_t     hand_authored_flags[PKS_HANDAUTHORED_EVENT_BYTES];

    uint8_t     map_pal_offset;

    uint8_t     checksum;
} save_block_v11_t;

typedef struct PACKED {
    uint8_t     player_name[NAME_LENGTH];
    uint8_t     pokedex_owned[19];
    uint8_t     pokedex_seen[19];
    uint8_t     num_bag_items;
    uint8_t     bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t     player_money[3];
    uint8_t     rival_name[NAME_LENGTH];
    uint8_t     options;
    uint8_t     badges;
    uint8_t     _pad1;
    uint8_t     letter_delay_flags;
    uint16_t    player_id;
    uint8_t     cur_map;
    uint8_t     last_map;
    uint8_t     y_coord;
    uint8_t     x_coord;
    uint8_t     event_flags[SAVE_EVENT_FLAGS_BYTES];
    uint8_t     game_progress_flags[256];
    uint16_t    picked_up_items[248];
    uint8_t     party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t     party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     party_nicks[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     current_box_num;
    uint8_t     box_count[NUM_BOXES];
    uint8_t     box_species[NUM_BOXES][BOX_CAPACITY + 1];
    box_mon_t   box_mons[NUM_BOXES][BOX_CAPACITY];
    uint8_t     box_ot[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t     box_nicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint16_t    completed_trade_flags;
    uint8_t     num_box_items;
    uint8_t     box_items[PC_ITEM_CAPACITY * 2 + 1];
    uint8_t     used_pokecenter;
    uint16_t    safari_steps;
    uint8_t     safari_balls;
    uint8_t     daycare_in_use;
    box_mon_t   daycare_mon;
    uint8_t     daycare_mon_ot[NAME_LENGTH];
    uint8_t     daycare_mon_name[NAME_LENGTH];
    uint8_t     first_lock_can;
    uint8_t     second_lock_can;
    uint8_t     player_coins[2];
    uint8_t     hand_authored_flags[PKS_HANDAUTHORED_EVENT_BYTES];

    uint8_t     map_pal_offset;

    uint8_t     event_numbering_rev;

    uint8_t     checksum;
} save_block_v12_t;

typedef struct PACKED {
    uint8_t     player_name[NAME_LENGTH];
    uint8_t     pokedex_owned[19];
    uint8_t     pokedex_seen[19];
    uint8_t     num_bag_items;
    uint8_t     bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t     player_money[3];
    uint8_t     rival_name[NAME_LENGTH];
    uint8_t     options;
    uint8_t     badges;
    uint8_t     _pad1;
    uint8_t     letter_delay_flags;
    uint16_t    player_id;
    uint8_t     cur_map;
    uint8_t     last_map;
    uint8_t     y_coord;
    uint8_t     x_coord;
    uint8_t     event_flags[SAVE_EVENT_FLAGS_BYTES];
    uint8_t     game_progress_flags[256];
    uint16_t    picked_up_items[248];
    uint8_t     party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t     party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     party_nicks[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     current_box_num;
    uint8_t     box_count[NUM_BOXES];
    uint8_t     box_species[NUM_BOXES][BOX_CAPACITY + 1];
    box_mon_t   box_mons[NUM_BOXES][BOX_CAPACITY];
    uint8_t     box_ot[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t     box_nicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint16_t    completed_trade_flags;
    uint8_t     num_box_items;
    uint8_t     box_items[PC_ITEM_CAPACITY * 2 + 1];
    uint8_t     used_pokecenter;
    uint16_t    safari_steps;
    uint8_t     safari_balls;
    uint8_t     daycare_in_use;
    box_mon_t   daycare_mon;
    uint8_t     daycare_mon_ot[NAME_LENGTH];
    uint8_t     daycare_mon_name[NAME_LENGTH];
    uint8_t     first_lock_can;
    uint8_t     second_lock_can;
    uint8_t     player_coins[2];
    uint8_t     hand_authored_flags[PKS_HANDAUTHORED_EVENT_BYTES];

    uint8_t     map_pal_offset;

    uint8_t     event_numbering_rev;

    uint8_t     town_visited[2];

    uint8_t     checksum;
} save_block_v13_t;

typedef struct PACKED {
    uint8_t     player_name[NAME_LENGTH];
    uint8_t     pokedex_owned[19];
    uint8_t     pokedex_seen[19];
    uint8_t     num_bag_items;
    uint8_t     bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t     player_money[3];
    uint8_t     rival_name[NAME_LENGTH];
    uint8_t     options;
    uint8_t     badges;
    uint8_t     _pad1;
    uint8_t     letter_delay_flags;
    uint16_t    player_id;
    uint8_t     cur_map;
    uint8_t     last_map;
    uint8_t     y_coord;
    uint8_t     x_coord;
    uint8_t     event_flags[SAVE_EVENT_FLAGS_BYTES];
    uint8_t     game_progress_flags[256];
    uint16_t    picked_up_items[248];
    uint8_t     party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t     party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     party_nicks[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     current_box_num;
    uint8_t     box_count[NUM_BOXES];
    uint8_t     box_species[NUM_BOXES][BOX_CAPACITY + 1];
    box_mon_t   box_mons[NUM_BOXES][BOX_CAPACITY];
    uint8_t     box_ot[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t     box_nicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint16_t    completed_trade_flags;
    uint8_t     num_box_items;
    uint8_t     box_items[PC_ITEM_CAPACITY * 2 + 1];
    uint8_t     used_pokecenter;
    uint16_t    safari_steps;
    uint8_t     safari_balls;
    uint8_t     daycare_in_use;
    box_mon_t   daycare_mon;
    uint8_t     daycare_mon_ot[NAME_LENGTH];
    uint8_t     daycare_mon_name[NAME_LENGTH];
    uint8_t     first_lock_can;
    uint8_t     second_lock_can;
    uint8_t     player_coins[2];
    uint8_t     hand_authored_flags[PKS_HANDAUTHORED_EVENT_BYTES];

    uint8_t     map_pal_offset;

    uint8_t     event_numbering_rev;

    uint8_t     town_visited[2];

    uint8_t     event_flags_ext[SAVE_EVENT_FLAGS_EXT_BYTES];

    uint8_t     checksum;
} save_block_v14_t;

typedef struct PACKED {
    uint8_t     player_name[NAME_LENGTH];
    uint8_t     pokedex_owned[19];
    uint8_t     pokedex_seen[19];
    uint8_t     num_bag_items;
    uint8_t     bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t     player_money[3];
    uint8_t     rival_name[NAME_LENGTH];
    uint8_t     options;
    uint8_t     badges;
    uint8_t     _pad1;
    uint8_t     letter_delay_flags;
    uint16_t    player_id;
    uint8_t     cur_map;
    uint8_t     last_map;
    uint8_t     y_coord;
    uint8_t     x_coord;
    uint8_t     event_flags[SAVE_EVENT_FLAGS_BYTES];
    uint8_t     game_progress_flags[256];
    uint16_t    picked_up_items[248];
    uint8_t     party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t     party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     party_nicks[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     current_box_num;
    uint8_t     box_count[NUM_BOXES];
    uint8_t     box_species[NUM_BOXES][BOX_CAPACITY + 1];
    box_mon_t   box_mons[NUM_BOXES][BOX_CAPACITY];
    uint8_t     box_ot[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t     box_nicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint16_t    completed_trade_flags;
    uint8_t     num_box_items;
    uint8_t     box_items[PC_ITEM_CAPACITY * 2 + 1];
    uint8_t     used_pokecenter;
    uint16_t    safari_steps;
    uint8_t     safari_balls;
    uint8_t     daycare_in_use;
    box_mon_t   daycare_mon;
    uint8_t     daycare_mon_ot[NAME_LENGTH];
    uint8_t     daycare_mon_name[NAME_LENGTH];
    uint8_t     first_lock_can;
    uint8_t     second_lock_can;
    uint8_t     player_coins[2];
    uint8_t     hand_authored_flags[PKS_HANDAUTHORED_EVENT_BYTES];

    uint8_t     map_pal_offset;

    uint8_t     event_numbering_rev;

    uint8_t     town_visited[2];

    uint8_t     event_flags_ext[SAVE_EVENT_FLAGS_EXT_BYTES];

    uint8_t     fossil_item;
    uint8_t     fossil_mon;

    uint32_t    play_time_frames;

    uint8_t     walk_bike_surf_state;

    uint8_t     last_blackout_map;
    uint8_t     last_heal_town_map;
    char        last_heal_town_name[24];

    uint8_t     num_hof_teams;
    hall_of_fame_team_t hall_of_fame_teams[HOF_TEAM_CAPACITY];

    uint8_t     checksum;
} save_block_t;

#define SAVE_V18_PAYLOAD  offsetof(save_block_t, num_hof_teams)
#define SAVE_V18_SIZE     (SAVE_V18_PAYLOAD + 1u)
typedef char save_v18_appends_hall_of_fame_and_nothing_else[
    (offsetof(save_block_t, checksum)
        == offsetof(save_block_t, num_hof_teams) + 1u
           + sizeof(((save_block_t *)0)->hall_of_fame_teams)) ? 1 : -1];

#define SAVE_V17_PAYLOAD  offsetof(save_block_t, last_blackout_map)
#define SAVE_V17_SIZE     (SAVE_V17_PAYLOAD + 1u)
typedef char save_v17_appends_blackout_town_and_nothing_else[
    (offsetof(save_block_t, num_hof_teams)
        == offsetof(save_block_t, last_blackout_map) + 2u
           + sizeof(((save_block_t *)0)->last_heal_town_name)) ? 1 : -1];

#define SAVE_V16_PAYLOAD  offsetof(save_block_t, walk_bike_surf_state)
#define SAVE_V16_SIZE     (SAVE_V16_PAYLOAD + 1u)
typedef char save_v16_appends_walkbike_and_nothing_else[
    (offsetof(save_block_t, last_blackout_map)
        == offsetof(save_block_t, walk_bike_surf_state) + 1u) ? 1 : -1];

#define SAVE_V15_PAYLOAD  offsetof(save_block_t, play_time_frames)
#define SAVE_V15_SIZE     (SAVE_V15_PAYLOAD + 1u)

typedef char save_v15_appends_play_time_and_nothing_else[
    (offsetof(save_block_t, walk_bike_surf_state)
        == offsetof(save_block_t, play_time_frames) + sizeof(uint32_t)) ? 1 : -1];

typedef char save_v11_appends_exactly_one_byte[
    (sizeof(save_block_v11_t) == sizeof(save_block_v10_t) + 1) ? 1 : -1];
typedef char save_v12_appends_exactly_one_byte[
    (sizeof(save_block_v12_t) == sizeof(save_block_v11_t) + 1) ? 1 : -1];

typedef char save_v14_appends_exactly_the_flag_ext[
    (sizeof(save_block_v14_t) == sizeof(save_block_v13_t)
                             + SAVE_EVENT_FLAGS_EXT_BYTES) ? 1 : -1];

typedef char save_cur_appends_the_fossil_bytes_and_play_time[
    (offsetof(save_block_t, fossil_item)
        == sizeof(save_block_v14_t) - 1u) ? 1 : -1];

typedef char save_v13_appends_exactly_two_bytes[
    (sizeof(save_block_v13_t) == sizeof(save_block_v12_t) + 2) ? 1 : -1];

#define SAVE_PREFIX_PINNED(field) \
    (offsetof(save_block_t, field) == offsetof(save_block_v11_t, field) && \
     offsetof(save_block_t, field) == offsetof(save_block_v12_t, field) && \
     offsetof(save_block_t, field) == offsetof(save_block_v13_t, field))
typedef char save_prefix_is_stable_v11[
    (SAVE_PREFIX_PINNED(event_flags)   &&
     SAVE_PREFIX_PINNED(party_count)   &&
     SAVE_PREFIX_PINNED(party_mons)    &&
     SAVE_PREFIX_PINNED(party_ot)      &&
     SAVE_PREFIX_PINNED(box_mons)      &&
     SAVE_PREFIX_PINNED(num_box_items) &&
     SAVE_PREFIX_PINNED(map_pal_offset)) ? 1 : -1];

static save_block_t save;
static int write_file_atomic(const char *path, const void *data, size_t len);

#define SAVE_EVENT_NUMBERING_REV 3

static void save_migrate_event_numbering_v11(uint8_t *flags) {
    enum { OLD_LO = 240, OLD_HI = 335, SHIFT = 2 };
    uint8_t bits[OLD_HI - OLD_LO + 1];
    int i;
    if (!flags) return;
    for (i = 0; i <= OLD_HI - OLD_LO; i++) {
        int id = OLD_LO + i;
        bits[i] = (uint8_t)((flags[id >> 3] >> (id & 7)) & 1);
    }
    for (i = 0; i <= OLD_HI - OLD_LO; i++) {
        int dst = OLD_LO + i - SHIFT;
        if (bits[i]) flags[dst >> 3] |= (uint8_t)(1u << (dst & 7));
        else         flags[dst >> 3] &= (uint8_t)~(1u << (dst & 7));
    }

    for (i = OLD_HI - SHIFT + 1; i <= OLD_HI; i++)
        flags[i >> 3] &= (uint8_t)~(1u << (i & 7));

    {
        static const struct { int lo, hi; } tower_trainers[] = {
            { 241, 243 },
            { 249, 251 },
            { 258, 261 },
            { 265, 267 },
            { 273, 275 },
        };
        size_t r;
        for (r = 0; r < sizeof(tower_trainers) / sizeof(tower_trainers[0]); r++)
            for (i = tower_trainers[r].lo; i <= tower_trainers[r].hi; i++)
                flags[i >> 3] &= (uint8_t)~(1u << (i & 7));
    }
}

static void save_migrate_purified_zone_v1(uint8_t *flags) {
    if (!flags) return;
    flags[EVENT_IN_PURIFIED_ZONE >> 3] &=
        (uint8_t)~(1u << (EVENT_IN_PURIFIED_ZONE & 7));
}

static void log_party_moves(const char *tag, const party_mon_t *p, int count) {
    int i;
    if (count > PARTY_LENGTH) count = PARTY_LENGTH;
    for (i = 0; i < count; i++) {
        printf("[savemoves] %s slot%d species=%u moves=%u,%u,%u,%u\n",
               tag, i + 1, (unsigned)p[i].base.species,
               (unsigned)p[i].base.moves[0], (unsigned)p[i].base.moves[1],
               (unsigned)p[i].base.moves[2], (unsigned)p[i].base.moves[3]);
    }
    fflush(stdout);
}

static void pack_save(void) {
    memcpy(save.player_name,    wPlayerName,     NAME_LENGTH);
    memcpy(save.pokedex_owned,  wPokedexOwned,   sizeof(wPokedexOwned));
    memcpy(save.pokedex_seen,   wPokedexSeen,    sizeof(wPokedexSeen));
    save.num_bag_items = wNumBagItems;
    memcpy(save.bag_items,      wBagItems,       sizeof(wBagItems));
    save.num_box_items = wNumBoxItems;
    memcpy(save.box_items,      wBoxItems,       sizeof(wBoxItems));
    save.used_pokecenter = (uint8_t)Pokecenter_GetUsedFlag();

    save.map_pal_offset = gMapPalOffset;

    save.event_numbering_rev = SAVE_EVENT_NUMBERING_REV;
    TownMap_GetVisited(save.town_visited);
    save.safari_steps = wSafariSteps;
    save.fossil_item  = wFossilItem;
    save.fossil_mon   = wFossilMon;
    {
        extern unsigned long gPlayTimeFrames;
        save.play_time_frames = (uint32_t)gPlayTimeFrames;
    save.walk_bike_surf_state = wWalkBikeSurfState;
    save.last_blackout_map  = wLastBlackoutMap;
    save.last_heal_town_map = wLastHealTownMap;
    snprintf(save.last_heal_town_name, sizeof(save.last_heal_town_name),
             "%s", wLastHealTownName);
    save.num_hof_teams = wNumHoFTeams;
    memcpy(save.hall_of_fame_teams, wHallOfFameTeams,
           sizeof(wHallOfFameTeams));
    }
    save.safari_balls = wNumSafariBalls;
    save.daycare_in_use = wDayCareInUse;
    memcpy(&save.daycare_mon,      &wDayCareMon,     sizeof(box_mon_t));
    memcpy(save.daycare_mon_ot,    wDayCareMonOT,    NAME_LENGTH);
    memcpy(save.daycare_mon_name,  wDayCareMonName,  NAME_LENGTH);
    save.first_lock_can  = wFirstLockTrashCanIndex;
    save.second_lock_can = wSecondLockTrashCanIndex;
    memcpy(save.player_money,   wPlayerMoney,    3);
    memcpy(save.player_coins,   wPlayerCoins,    2);
    memcpy(save.rival_name,     wRivalName,      NAME_LENGTH);
    save.options    = wOptions;
    save.badges     = wObtainedBadges;
    save.player_id  = wPlayerID;
    save.cur_map    = wCurMap;
    save.last_map   = wLastMap;
    save.y_coord    = wYCoord;
    save.x_coord    = wXCoord;
    memcpy(save.event_flags,     wEventFlags, SAVE_EVENT_FLAGS_BYTES);
    memcpy(save.event_flags_ext, wEventFlags + SAVE_EVENT_FLAGS_BYTES,
           SAVE_EVENT_FLAGS_EXT_BYTES);
    memcpy(save.hand_authored_flags, wHandAuthoredEventFlags, PKS_HANDAUTHORED_EVENT_BYTES);
    save.completed_trade_flags = wCompletedInGameTradeFlags;
    memcpy(save.picked_up_items, wPickedUpItems, sizeof(wPickedUpItems));
    save.party_count = wPartyCount;
    memcpy(save.party_mons,     wPartyMons,      sizeof(wPartyMons));
    memcpy(save.party_ot,       wPartyMonOT,     sizeof(wPartyMonOT));
    memcpy(save.party_nicks,    wPartyMonNicks,  sizeof(wPartyMonNicks));
    save.current_box_num = wCurrentBoxNum;
    memcpy(save.box_count,      wBoxCount,       sizeof(wBoxCount));
    memcpy(save.box_species,    wBoxSpecies,     sizeof(wBoxSpecies));
    memcpy(save.box_mons,       wBoxMons,        sizeof(wBoxMons));
    memcpy(save.box_ot,         wBoxMonOT,       sizeof(wBoxMonOT));
    memcpy(save.box_nicks,      wBoxMonNicks,    sizeof(wBoxMonNicks));

    save.checksum = CalcCheckSum((uint8_t *)&save,
                                  (uint16_t)(sizeof(save) - 1));
    log_party_moves("write", save.party_mons, save.party_count);
}

#define SAVE_NICK_LEGACY_DOT 0xE8u
static const uint8_t kNickNidoran[7] = {0x8D,0x88,0x83,0x8E,0x91,0x80,0x8D};

static void save_fix_one_gender_nick(uint8_t *nick) {
    uint8_t glyph;
    if (memcmp(nick, kNickNidoran, sizeof kNickNidoran) != 0) return;
    if (nick[7] != SAVE_NICK_LEGACY_DOT) return;
    if      (nick[8] == 0x8Cu) glyph = 0xEFu;
    else if (nick[8] == 0x85u) glyph = 0xF5u;
    else return;
    nick[7] = glyph;
    for (int i = 8; i < NAME_LENGTH; i++) nick[i] = 0x50u;
}

static void save_migrate_gender_glyph_nicks_v2(void) {
    for (int i = 0; i < PARTY_LENGTH; i++)
        save_fix_one_gender_nick(save.party_nicks[i]);
    for (int b = 0; b < NUM_BOXES; b++)
        for (int i = 0; i < BOX_CAPACITY; i++)
            save_fix_one_gender_nick(save.box_nicks[b][i]);
}

static int s_peek_only = 0;

static void unpack_save(void) {

    if (save.event_numbering_rev < 1) save_migrate_event_numbering_v11(save.event_flags);
    if (save.event_numbering_rev < 2) save_migrate_purified_zone_v1(save.event_flags);
    if (save.event_numbering_rev < 3) save_migrate_gender_glyph_nicks_v2();
    save.event_numbering_rev = SAVE_EVENT_NUMBERING_REV;
    if (s_peek_only) return;
    memcpy(wPlayerName,    save.player_name,   NAME_LENGTH);
    memcpy(wPokedexOwned,  save.pokedex_owned, sizeof(wPokedexOwned));
    memcpy(wPokedexSeen,   save.pokedex_seen,  sizeof(wPokedexSeen));
    wNumBagItems = save.num_bag_items;
    memcpy(wBagItems,      save.bag_items,     sizeof(wBagItems));
    wNumBoxItems = save.num_box_items;
    memcpy(wBoxItems,      save.box_items,     sizeof(wBoxItems));
    Pokecenter_SetUsedFlag(save.used_pokecenter);
    gMapPalOffset = save.map_pal_offset;
    wSafariSteps = save.safari_steps;
    wFossilItem  = save.fossil_item;
    wFossilMon   = save.fossil_mon;
    {
        extern unsigned long gPlayTimeFrames;
        gPlayTimeFrames = (unsigned long)save.play_time_frames;
    wWalkBikeSurfState = save.walk_bike_surf_state;
    wLastBlackoutMap  = save.last_blackout_map;
    wLastHealTownMap  = save.last_heal_town_map;
    snprintf(wLastHealTownName, sizeof(wLastHealTownName),
             "%s", save.last_heal_town_name);
    wNumHoFTeams = save.num_hof_teams;
    memcpy(wHallOfFameTeams, save.hall_of_fame_teams,
           sizeof(wHallOfFameTeams));
    }
    wNumSafariBalls = save.safari_balls;
    wDayCareInUse = save.daycare_in_use;
    memcpy(&wDayCareMon,     &save.daycare_mon,      sizeof(box_mon_t));
    memcpy(wDayCareMonOT,    save.daycare_mon_ot,    NAME_LENGTH);
    memcpy(wDayCareMonName,  save.daycare_mon_name,  NAME_LENGTH);
    wFirstLockTrashCanIndex  = save.first_lock_can;
    wSecondLockTrashCanIndex = save.second_lock_can;
    memcpy(wPlayerMoney,   save.player_money,  3);
    memcpy(wPlayerCoins,   save.player_coins,  2);
    memcpy(wRivalName,     save.rival_name,    NAME_LENGTH);
    wOptions       = save.options;
    wObtainedBadges = save.badges;
    wPlayerID       = save.player_id;
    wCurMap         = save.cur_map;
    wLastMap        = save.last_map;
    wYCoord         = save.y_coord;
    wXCoord         = save.x_coord;
    wCompletedInGameTradeFlags = save.completed_trade_flags;
    memcpy(wEventFlags,                          save.event_flags,
           SAVE_EVENT_FLAGS_BYTES);
    memcpy(wEventFlags + SAVE_EVENT_FLAGS_BYTES, save.event_flags_ext,
           SAVE_EVENT_FLAGS_EXT_BYTES);
    memcpy(wHandAuthoredEventFlags, save.hand_authored_flags, PKS_HANDAUTHORED_EVENT_BYTES);

    AmberScript_ScrubStaleFlagBits();
    memcpy(wPickedUpItems, save.picked_up_items, sizeof(wPickedUpItems));
    wPartyCount = save.party_count;
    memcpy(wPartyMons,    save.party_mons,     sizeof(wPartyMons));
    memcpy(wPartyMonOT,   save.party_ot,       sizeof(wPartyMonOT));
    memcpy(wPartyMonNicks,save.party_nicks,    sizeof(wPartyMonNicks));
    wCurrentBoxNum = save.current_box_num;
    memcpy(wBoxCount,     save.box_count,      sizeof(wBoxCount));
    memcpy(wBoxSpecies,   save.box_species,    sizeof(wBoxSpecies));
    memcpy(wBoxMons,      save.box_mons,       sizeof(wBoxMons));
    memcpy(wBoxMonOT,     save.box_ot,         sizeof(wBoxMonOT));
    memcpy(wBoxMonNicks,  save.box_nicks,      sizeof(wBoxMonNicks));

    if (wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST)
        AmberScript_SetEnabled(1);

    save_vmap_sidecar_load();
    save_npc_rt_sidecar_load();
    TownMap_SetVisited(save.town_visited);
    log_party_moves("read ", wPartyMons, wPartyCount);
}

static const char *save_sidecar(const char *ext) {
    static char path[1280];
    snprintf(path, sizeof(path), "%s%s", GameVersion_SavePath(NULL), ext);
    return path;
}
#define SAVE_VMAP_SIDECAR save_sidecar(".vmaps")

static void save_vmap_sidecar_write(void) {
    char names[PKS_VIRTUAL_MAP_COUNT][PKS_VMAP_BIND_NAME_LEN];
    FILE *f;
    AmberScript_MapBank_SnapshotBindings(names, PKS_VIRTUAL_MAP_COUNT);
    f = fopen(SAVE_VMAP_SIDECAR, "w");
    if (!f) return;
    for (int i = 0; i < PKS_VIRTUAL_MAP_COUNT; i++)
        fprintf(f, "%s\n", names[i]);
    fclose(f);
}

static void save_vmap_sidecar_load(void) {
    char names[PKS_VIRTUAL_MAP_COUNT][PKS_VMAP_BIND_NAME_LEN];
    char line[64];
    FILE *f = fopen(SAVE_VMAP_SIDECAR, "r");
    memset(names, 0, sizeof(names));
    if (!f) return;
    for (int i = 0; i < PKS_VIRTUAL_MAP_COUNT &&
                    fgets(line, sizeof(line), f); i++) {
        line[strcspn(line, "\r\n")] = '\0';
        snprintf(names[i], PKS_VMAP_BIND_NAME_LEN, "%s", line);
    }
    fclose(f);
    AmberScript_MapBank_RestoreBindings(
        (const char (*)[PKS_VMAP_BIND_NAME_LEN])names, PKS_VIRTUAL_MAP_COUNT);
}

#define SAVE_NPC_RT_SIDECAR save_sidecar(".npcrt")

static void save_npc_rt_sidecar_write(void) {
    uint8_t blob[PKS_NPC_RT_BLOB_MAX];
    int len = AmberScript_MapBank_SerializeNpcRt(blob, (int)sizeof(blob));
    FILE *f = fopen(SAVE_NPC_RT_SIDECAR, "wb");
    if (!f) return;
    if (len > 0) fwrite(blob, 1, (size_t)len, f);
    fclose(f);
}

static void save_npc_rt_sidecar_load(void) {
    uint8_t blob[PKS_NPC_RT_BLOB_MAX];
    FILE *f = fopen(SAVE_NPC_RT_SIDECAR, "rb");
    if (!f) { AmberScript_MapBank_DeserializeNpcRt(NULL, 0); return; }
    size_t n = fread(blob, 1, sizeof(blob), f);
    fclose(f);
    AmberScript_MapBank_DeserializeNpcRt(blob, (int)n);
}

int Save_ValidateChecksum(void) {
    uint8_t calc = CalcCheckSum((uint8_t *)&save,
                                 (uint16_t)(sizeof(save) - 1));
    return calc == save.checksum ? 0 : -1;
}

int Save_Load(void) {
    return Save_LoadFrom(GameVersion_SavePath(NULL));
}

int Save_PeekFrom(const char *path, save_peek_t *out) {
    save_block_t keep = save;
    int rc;

    if (out) memset(out, 0, sizeof(*out));
    s_peek_only = 1;
    rc = Save_LoadFrom(path);
    s_peek_only = 0;

    if (rc == 0 && out) {
        memcpy(out->player_name, save.player_name, sizeof(out->player_name));
        out->badges = save.badges;
        memcpy(out->pokedex_owned, save.pokedex_owned,
               sizeof(out->pokedex_owned));
        out->valid = 1;
    }
    save = keep;
    return rc;
}

static unsigned bcd_read(const uint8_t *p, size_t n) {
    unsigned v = 0;
    for (size_t i = 0; i < n; i++)
        v = v * 100u + ((p[i] >> 4) & 0xFu) * 10u + (p[i] & 0xFu);
    return v;
}

static void bcd_write(uint8_t *p, size_t n, unsigned v) {
    for (size_t i = n; i-- > 0;) {
        unsigned pair = v % 100u;
        p[i] = (uint8_t)(((pair / 10u) << 4) | (pair % 10u));
        v /= 100u;
    }
}

static int editor_sidecar_path(const char *save_path, char *out, size_t out_size) {
    int n;
    if (!save_path || !out || out_size == 0) return 0;
    n = snprintf(out, out_size, "%s.vmaps", save_path);
    return n > 0 && (size_t)n < out_size;
}

static int editor_copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    FILE *out;
    char buf[8192];
    size_t n;
    int rc = 0;
    if (!in) return -1;
    out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { rc = -1; break; }
    }
    if (ferror(in)) rc = -1;
    fclose(in);
    if (fclose(out) != 0) rc = -1;
    return rc;
}

static void editor_read_vmap_bindings(const char *save_path,
                                      save_editor_data_t *out) {
    char path[1280];
    char line[128];
    FILE *f;
    if (!out || !editor_sidecar_path(save_path, path, sizeof(path))) return;
    f = fopen(path, "r");
    if (!f) return;
    for (int i = 0; i < SAVE_EDITOR_VMAP_SLOT_COUNT &&
                    fgets(line, sizeof(line), f); i++) {
        line[strcspn(line, "\r\n")] = '\0';
        snprintf(out->vmap_bindings[i], sizeof(out->vmap_bindings[i]),
                 "%s", line);
    }
    fclose(f);
    out->vmap_bindings[SAVE_EDITOR_VMAP_SLOT_COUNT - 1][0] = '\0';
}

static int editor_write_vmap_bindings(const char *save_path,
                                      const save_editor_data_t *data,
                                      int *had_old_sidecar) {
    char path[1280];
    char backup[1280];
    char text[SAVE_EDITOR_VMAP_SLOT_COUNT * SAVE_EDITOR_VMAP_NAME_LEN];
    size_t used = 0;
    FILE *probe;
    if (!data || !had_old_sidecar ||
        !editor_sidecar_path(save_path, path, sizeof(path))) return -1;
    *had_old_sidecar = 0;
    probe = fopen(path, "rb");
    if (probe) {
        fclose(probe);
        *had_old_sidecar = 1;
        if (snprintf(backup, sizeof(backup), "%s.editor.bak", path) <= 0 ||
            editor_copy_file(path, backup) != 0) return -1;
    }
    for (int i = 0; i < SAVE_EDITOR_VMAP_SLOT_COUNT; i++) {
        const char *name = i == SAVE_EDITOR_VMAP_SLOT_COUNT - 1
                         ? "" : data->vmap_bindings[i];
        int n = snprintf(text + used, sizeof(text) - used, "%s\n", name);
        if (n < 0 || (size_t)n >= sizeof(text) - used) return -1;
        used += (size_t)n;
    }
    return write_file_atomic(path, text, used);
}

static void editor_restore_vmap_bindings(const char *save_path,
                                         int had_old_sidecar) {
    char path[1280];
    char backup[1280];
    if (!editor_sidecar_path(save_path, path, sizeof(path))) return;
    if (!had_old_sidecar) {
        remove(path);
        return;
    }
    if (snprintf(backup, sizeof(backup), "%s.editor.bak", path) <= 0) return;
    if (editor_copy_file(backup, path) != 0)
        fprintf(stderr, "save editor: could not restore %s after save failure\n", path);
}

int Save_EditorRead(const char *path, save_editor_data_t *out) {
    save_block_t keep = save;
    int rc;
    if (!path || !out) return -1;
    memset(out, 0, sizeof(*out));
    s_peek_only = 1;
    rc = Save_LoadFrom(path);
    s_peek_only = 0;
    if (rc == 0) {
        memcpy(out->player_name, save.player_name, sizeof(out->player_name));
        memcpy(out->rival_name, save.rival_name, sizeof(out->rival_name));
        out->player_id = save.player_id;
        out->money = bcd_read(save.player_money, sizeof(save.player_money));
        out->coins = (uint16_t)bcd_read(save.player_coins, sizeof(save.player_coins));
        out->badges = save.badges;
        out->cur_map = save.cur_map;
        out->last_map = save.last_map;
        out->x_coord = save.x_coord;
        out->y_coord = save.y_coord;
        memcpy(out->pokedex_owned, save.pokedex_owned, sizeof(out->pokedex_owned));
        memcpy(out->pokedex_seen, save.pokedex_seen, sizeof(out->pokedex_seen));
        out->num_bag_items = save.num_bag_items;
        memcpy(out->bag_items, save.bag_items, sizeof(out->bag_items));
        out->num_box_items = save.num_box_items;
        memcpy(out->box_items, save.box_items, sizeof(out->box_items));
        out->party_count = save.party_count;
        memcpy(out->party_mons, save.party_mons, sizeof(out->party_mons));
        memcpy(out->party_ot, save.party_ot, sizeof(out->party_ot));
        memcpy(out->party_nicks, save.party_nicks, sizeof(out->party_nicks));
        out->current_box_num = save.current_box_num;
        memcpy(out->box_count, save.box_count, sizeof(out->box_count));
        memcpy(out->box_species, save.box_species, sizeof(out->box_species));
        memcpy(out->box_mons, save.box_mons, sizeof(out->box_mons));
        memcpy(out->box_ot, save.box_ot, sizeof(out->box_ot));
        memcpy(out->box_nicks, save.box_nicks, sizeof(out->box_nicks));
        memcpy(out->event_flags, save.event_flags, SAVE_EVENT_FLAGS_BYTES);
        memcpy(out->event_flags + SAVE_EVENT_FLAGS_BYTES, save.event_flags_ext,
               SAVE_EVENT_FLAGS_EXT_BYTES);
        memcpy(out->hand_authored_flags, save.hand_authored_flags,
               sizeof(out->hand_authored_flags));
        editor_read_vmap_bindings(path, out);
        out->location_changed = 0;
    }
    save = keep;
    return rc;
}

int Save_EditorWrite(const char *path, const save_editor_data_t *data) {
    char backup[1280];
    save_block_t keep = save;
    int had_old_sidecar = 0;
    int rc;
    if (!path || !data || data->money > 999999u || data->coins > 9999u)
        return -1;
    if (data->location_changed &&
        (data->cur_map < PKS_VIRTUAL_MAP_FIRST ||
         data->cur_map >= PKS_VIRTUAL_MAP_LAST ||
         !data->vmap_bindings[data->cur_map - PKS_VIRTUAL_MAP_FIRST][0]))
        return -1;

    s_peek_only = 1;
    rc = Save_LoadFrom(path);
    s_peek_only = 0;
    if (rc != 0) { save = keep; return -1; }

    memcpy(save.player_name, data->player_name, sizeof(save.player_name));
    memcpy(save.rival_name, data->rival_name, sizeof(save.rival_name));
    save.player_id = data->player_id;
    bcd_write(save.player_money, sizeof(save.player_money), data->money);
    bcd_write(save.player_coins, sizeof(save.player_coins), data->coins);
    save.badges = data->badges;
    save.cur_map = data->cur_map;
    save.last_map = data->last_map;
    save.x_coord = data->x_coord;
    save.y_coord = data->y_coord;
    memcpy(save.pokedex_seen, data->pokedex_seen, sizeof(save.pokedex_seen));
    memcpy(save.pokedex_owned, data->pokedex_owned, sizeof(save.pokedex_owned));
    for (int i = 0; i < 19; i++)
        save.pokedex_seen[i] |= save.pokedex_owned[i];
    save.num_bag_items = data->num_bag_items <= BAG_ITEM_CAPACITY
                       ? data->num_bag_items : BAG_ITEM_CAPACITY;
    memcpy(save.bag_items, data->bag_items, sizeof(save.bag_items));
    save.bag_items[save.num_bag_items * 2] = 0xFF;
    save.num_box_items = data->num_box_items <= PC_ITEM_CAPACITY
                       ? data->num_box_items : PC_ITEM_CAPACITY;
    memcpy(save.box_items, data->box_items, sizeof(save.box_items));
    save.box_items[save.num_box_items * 2] = 0xFF;
    save.party_count = data->party_count <= PARTY_LENGTH ? data->party_count : PARTY_LENGTH;
    memcpy(save.party_mons, data->party_mons, sizeof(save.party_mons));
    memcpy(save.party_ot, data->party_ot, sizeof(save.party_ot));
    memcpy(save.party_nicks, data->party_nicks, sizeof(save.party_nicks));
    save.current_box_num = data->current_box_num < NUM_BOXES ? data->current_box_num : 0;
    memcpy(save.box_count, data->box_count, sizeof(save.box_count));
    memcpy(save.box_species, data->box_species, sizeof(save.box_species));
    memcpy(save.box_mons, data->box_mons, sizeof(save.box_mons));
    memcpy(save.box_ot, data->box_ot, sizeof(save.box_ot));
    memcpy(save.box_nicks, data->box_nicks, sizeof(save.box_nicks));
    for (int b = 0; b < NUM_BOXES; b++) {
        if (save.box_count[b] > BOX_CAPACITY) save.box_count[b] = BOX_CAPACITY;
        for (int i = 0; i < save.box_count[b]; i++)
            save.box_species[b][i] = save.box_mons[b][i].species;
        save.box_species[b][save.box_count[b]] = 0xFF;
    }
    memcpy(save.event_flags, data->event_flags, SAVE_EVENT_FLAGS_BYTES);
    memcpy(save.event_flags_ext, data->event_flags + SAVE_EVENT_FLAGS_BYTES,
           SAVE_EVENT_FLAGS_EXT_BYTES);
    memcpy(save.hand_authored_flags, data->hand_authored_flags,
           sizeof(save.hand_authored_flags));
    save.checksum = CalcCheckSum((uint8_t *)&save, (uint16_t)(sizeof(save) - 1));

    snprintf(backup, sizeof(backup), "%s.editor.bak", path);
    if (editor_copy_file(path, backup) != 0) { save = keep; return -1; }
    if (data->location_changed &&
        editor_write_vmap_bindings(path, data, &had_old_sidecar) != 0) {
        save = keep;
        return -1;
    }
    rc = write_file_atomic(path, &save, sizeof(save));
    if (rc != 0 && data->location_changed)
        editor_restore_vmap_bindings(path, had_old_sidecar);
    save = keep;
    return rc;
}

int Save_LoadFrom(const char *path) {
    FILE *f = fopen(path, "rb");
    long sz;
    size_t n;
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    sz = ftell(f);
    if (sz < 0) { fclose(f); return -1; }
    rewind(f);

    if ((size_t)sz == sizeof(save_block_t)) {
        n = fread(&save, 1, sizeof(save), f);
        fclose(f);
        if (n != sizeof(save)) return -1;
        if (Save_ValidateChecksum() != 0) return -1;
        unpack_save();
        return 0;
    }

    if ((size_t)sz == SAVE_V18_SIZE) {
        uint8_t v18[SAVE_V18_SIZE];
        uint8_t calc18;
        n = fread(v18, 1, sizeof(v18), f);
        fclose(f);
        if (n != sizeof(v18)) return -1;
        calc18 = CalcCheckSum(v18, (uint16_t)SAVE_V18_PAYLOAD);
        if (calc18 != v18[SAVE_V18_PAYLOAD]) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, v18, SAVE_V18_PAYLOAD);
        unpack_save();
        return 0;
    }

    if ((size_t)sz == SAVE_V17_SIZE) {

        uint8_t v17[SAVE_V17_SIZE];
        uint8_t calc17;
        n = fread(v17, 1, sizeof(v17), f);
        fclose(f);
        if (n != sizeof(v17)) return -1;
        calc17 = CalcCheckSum(v17, (uint16_t)SAVE_V17_PAYLOAD);
        if (calc17 != v17[SAVE_V17_PAYLOAD]) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, v17, SAVE_V17_PAYLOAD);
        save.last_blackout_map     = 0xFF;
        save.last_heal_town_map    = 0x00;
        save.last_heal_town_name[0] = '\0';
        unpack_save();
        return 0;
    }

    if ((size_t)sz == SAVE_V16_SIZE) {

        uint8_t v16[SAVE_V16_SIZE];
        uint8_t calc16;
        n = fread(v16, 1, sizeof(v16), f);
        fclose(f);
        if (n != sizeof(v16)) return -1;
        calc16 = CalcCheckSum(v16, (uint16_t)SAVE_V16_PAYLOAD);
        if (calc16 != v16[SAVE_V16_PAYLOAD]) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, v16, SAVE_V16_PAYLOAD);
        save.walk_bike_surf_state = 0;
        save.last_blackout_map      = 0xFF;
        save.last_heal_town_map     = 0x00;
        save.last_heal_town_name[0] = '\0';
        unpack_save();
        return 0;
    }

    if ((size_t)sz == SAVE_V15_SIZE) {

        uint8_t v15[SAVE_V15_SIZE];
        uint8_t calc15;
        n = fread(v15, 1, sizeof(v15), f);
        fclose(f);
        if (n != sizeof(v15)) return -1;
        calc15 = CalcCheckSum(v15, (uint16_t)SAVE_V15_PAYLOAD);
        if (calc15 != v15[SAVE_V15_PAYLOAD]) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, v15, SAVE_V15_PAYLOAD);
        save.play_time_frames = 0;
        save.last_blackout_map      = 0xFF;
        save.last_heal_town_map     = 0x00;
        save.last_heal_town_name[0] = '\0';
        unpack_save();
        return 0;
    }

    if ((size_t)sz == sizeof(save_block_v14_t)) {

        save_block_v14_t v14;
        uint8_t calc14;
        memset(&v14, 0, sizeof(v14));
        n = fread(&v14, 1, sizeof(v14), f);
        fclose(f);
        if (n != sizeof(v14)) return -1;
        calc14 = CalcCheckSum((uint8_t *)&v14, (uint16_t)(sizeof(v14) - 1));
        if (calc14 != v14.checksum) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, &v14, offsetof(save_block_v14_t, checksum));
        save.fossil_item = 0;
        save.fossil_mon  = 0;
        unpack_save();
        return 0;
    }

    if ((size_t)sz == sizeof(save_block_v13_t)) {

        save_block_v13_t v13;
        uint8_t calc13;
        memset(&v13, 0, sizeof(v13));
        n = fread(&v13, 1, sizeof(v13), f);
        fclose(f);
        if (n != sizeof(v13)) return -1;
        calc13 = CalcCheckSum((uint8_t *)&v13, (uint16_t)(sizeof(v13) - 1));
        if (calc13 != v13.checksum) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, &v13, offsetof(save_block_v13_t, checksum));
        memset(save.event_flags_ext, 0, sizeof(save.event_flags_ext));
        unpack_save();
        return 0;
    }

    if ((size_t)sz == sizeof(save_block_v12_t)) {

        save_block_v12_t v12;
        uint8_t calc12;
        memset(&v12, 0, sizeof(v12));
        n = fread(&v12, 1, sizeof(v12), f);
        fclose(f);
        if (n != sizeof(v12)) return -1;
        calc12 = CalcCheckSum((uint8_t *)&v12, (uint16_t)(sizeof(v12) - 1));
        if (calc12 != v12.checksum) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, &v12, offsetof(save_block_v12_t, checksum));
        save.town_visited[0] = 0;
        save.town_visited[1] = 0;
        unpack_save();
        return 0;
    }

    if ((size_t)sz == sizeof(save_block_v11_t)) {

        save_block_v11_t v11;
        uint8_t calc11;
        memset(&v11, 0, sizeof(v11));
        n = fread(&v11, 1, sizeof(v11), f);
        fclose(f);
        if (n != sizeof(v11)) return -1;
        calc11 = CalcCheckSum((uint8_t *)&v11, (uint16_t)(sizeof(v11) - 1));
        if (calc11 != v11.checksum) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, &v11, offsetof(save_block_v11_t, checksum));

        unpack_save();
        return 0;
    }

    if ((size_t)sz == sizeof(save_block_v10_t)) {

        save_block_v10_t v10;
        uint8_t calc10;
        memset(&v10, 0, sizeof(v10));
        n = fread(&v10, 1, sizeof(v10), f);
        fclose(f);
        if (n != sizeof(v10)) return -1;
        calc10 = CalcCheckSum((uint8_t *)&v10, (uint16_t)(sizeof(v10) - 1));
        if (calc10 != v10.checksum) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, &v10, offsetof(save_block_v10_t, checksum));
        save.map_pal_offset = 0;
        unpack_save();
        return 0;
    }

    if ((size_t)sz == sizeof(save_block_v9_t)) {

        save_block_v9_t v9;
        uint8_t calc9;
        memset(&v9, 0, sizeof(v9));
        n = fread(&v9, 1, sizeof(v9), f);
        fclose(f);
        if (n != sizeof(v9)) return -1;
        calc9 = CalcCheckSum((uint8_t *)&v9, (uint16_t)(sizeof(v9) - 1));
        if (calc9 != v9.checksum) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, &v9, offsetof(save_block_v9_t, checksum));
        memset(save.hand_authored_flags, 0, sizeof(save.hand_authored_flags));
        unpack_save();
        return 0;
    }

    if ((size_t)sz == sizeof(save_block_v8_t)) {

        save_block_v8_t v8;
        uint8_t calc;
        memset(&v8, 0, sizeof(v8));
        n = fread(&v8, 1, sizeof(v8), f);
        fclose(f);
        if (n != sizeof(v8)) return -1;
        calc = CalcCheckSum((uint8_t *)&v8, (uint16_t)(sizeof(v8) - 1));
        if (calc != v8.checksum) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, &v8, offsetof(save_block_v8_t, checksum));
        save.player_coins[0] = 0;
        save.player_coins[1] = 0;
        unpack_save();
        return 0;
    }

    if ((size_t)sz == sizeof(save_block_v7_t)) {

        save_block_v7_t v7;
        uint8_t calc;
        memset(&v7, 0, sizeof(v7));
        n = fread(&v7, 1, sizeof(v7), f);
        fclose(f);
        if (n != sizeof(v7)) return -1;
        calc = CalcCheckSum((uint8_t *)&v7, (uint16_t)(sizeof(v7) - 1));
        if (calc != v7.checksum) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, &v7, offsetof(save_block_v7_t, checksum));
        save.first_lock_can  = 0;
        save.second_lock_can = 0;
        unpack_save();
        return 0;
    }

    if ((size_t)sz == sizeof(save_block_v6_t)) {

        save_block_v6_t v6;
        uint8_t calc;
        memset(&v6, 0, sizeof(v6));
        n = fread(&v6, 1, sizeof(v6), f);
        fclose(f);
        if (n != sizeof(v6)) return -1;
        calc = CalcCheckSum((uint8_t *)&v6, (uint16_t)(sizeof(v6) - 1));
        if (calc != v6.checksum) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, &v6, offsetof(save_block_v6_t, checksum));
        save.daycare_in_use = 0;
        memset(&save.daycare_mon, 0, sizeof(save.daycare_mon));
        memset(save.daycare_mon_ot, 0x50, NAME_LENGTH);
        memset(save.daycare_mon_name, 0x50, NAME_LENGTH);
        unpack_save();
        return 0;
    }

    if ((size_t)sz == sizeof(save_block_v5_t)) {

        save_block_v5_t v5;
        uint8_t calc;
        memset(&v5, 0, sizeof(v5));
        n = fread(&v5, 1, sizeof(v5), f);
        fclose(f);
        if (n != sizeof(v5)) return -1;
        calc = CalcCheckSum((uint8_t *)&v5, (uint16_t)(sizeof(v5) - 1));
        if (calc != v5.checksum) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, &v5, offsetof(save_block_v5_t, checksum));
        save.safari_steps = 0;
        save.safari_balls = 0;
        unpack_save();
        return 0;
    }

    if ((size_t)sz == sizeof(save_block_v4_t)) {

        save_block_v4_t v4;
        uint8_t calc;
        memset(&v4, 0, sizeof(v4));
        n = fread(&v4, 1, sizeof(v4), f);
        fclose(f);
        if (n != sizeof(v4)) return -1;
        calc = CalcCheckSum((uint8_t *)&v4, (uint16_t)(sizeof(v4) - 1));
        if (calc != v4.checksum) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, &v4, offsetof(save_block_v4_t, checksum));
        save.used_pokecenter = 0;
        unpack_save();
        return 0;
    }

    if ((size_t)sz == sizeof(save_block_v3_t)) {

        save_block_v3_t v3;
        uint8_t calc;
        memset(&v3, 0, sizeof(v3));
        n = fread(&v3, 1, sizeof(v3), f);
        fclose(f);
        if (n != sizeof(v3)) return -1;
        calc = CalcCheckSum((uint8_t *)&v3, (uint16_t)(sizeof(v3) - 1));
        if (calc != v3.checksum) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, &v3, offsetof(save_block_v3_t, checksum));
        save.num_box_items = 0;
        save.box_items[0]  = 0xFF;
        unpack_save();
        return 0;
    }

    if ((size_t)sz == sizeof(save_block_v2_t)) {

        save_block_v2_t v2;
        uint8_t calc;
        memset(&v2, 0, sizeof(v2));
        n = fread(&v2, 1, sizeof(v2), f);
        fclose(f);
        if (n != sizeof(v2)) return -1;
        calc = CalcCheckSum((uint8_t *)&v2, (uint16_t)(sizeof(v2) - 1));
        if (calc != v2.checksum) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(&save, &v2, offsetof(save_block_v2_t, checksum));
        save.completed_trade_flags = 0;
        save.num_box_items = 0;
        save.box_items[0]  = 0xFF;
        unpack_save();
        return 0;
    }

    if ((size_t)sz == sizeof(save_block_v1_t)) {
        save_block_v1_t old_save;
        uint8_t calc;
        memset(&old_save, 0, sizeof(old_save));
        n = fread(&old_save, 1, sizeof(old_save), f);
        fclose(f);
        if (n != sizeof(old_save)) return -1;
        calc = CalcCheckSum((uint8_t *)&old_save, (uint16_t)(sizeof(old_save) - 1));
        if (calc != old_save.checksum) return -1;

        memset(&save, 0, sizeof(save));
        memcpy(save.player_name,   old_save.player_name,   sizeof(old_save.player_name));
        memcpy(save.pokedex_owned, old_save.pokedex_owned, sizeof(old_save.pokedex_owned));
        memcpy(save.pokedex_seen,  old_save.pokedex_seen,  sizeof(old_save.pokedex_seen));
        save.num_bag_items = old_save.num_bag_items;
        memcpy(save.bag_items,     old_save.bag_items,     sizeof(old_save.bag_items));
        memcpy(save.player_money,  old_save.player_money,  sizeof(old_save.player_money));
        memcpy(save.rival_name,    old_save.rival_name,    sizeof(old_save.rival_name));
        save.options = old_save.options;
        save.badges  = old_save.badges;
        save._pad1   = old_save._pad1;
        save.letter_delay_flags = old_save.letter_delay_flags;
        save.player_id = old_save.player_id;
        save.cur_map   = old_save.cur_map;
        save.last_map  = old_save.last_map;
        save.y_coord   = old_save.y_coord;
        save.x_coord   = old_save.x_coord;
        memcpy(save.event_flags,   old_save.event_flags,   sizeof(old_save.event_flags));
        memcpy(save.game_progress_flags, old_save.game_progress_flags, sizeof(old_save.game_progress_flags));
        memcpy(save.picked_up_items, old_save.picked_up_items, sizeof(old_save.picked_up_items));
        save.party_count = old_save.party_count;
        memcpy(save.party_mons,    old_save.party_mons,    sizeof(old_save.party_mons));
        memcpy(save.party_ot,      old_save.party_ot,      sizeof(old_save.party_ot));
        memcpy(save.party_nicks,   old_save.party_nicks,   sizeof(old_save.party_nicks));
        save.current_box_num = 0;
        memset(save.box_count, 0, sizeof(save.box_count));
        memset(save.box_species, 0xFF, sizeof(save.box_species));
        memset(save.box_mons, 0, sizeof(save.box_mons));
        memset(save.box_ot, 0, sizeof(save.box_ot));
        memset(save.box_nicks, 0, sizeof(save.box_nicks));
        save.num_box_items = 0;
        save.box_items[0]  = 0xFF;
        unpack_save();
        return 0;
    }

    fclose(f);
    return -1;
}

static int write_file_atomic(const char *path, const void *data, size_t len) {
    char tmp[512];
    FILE *f;
    size_t n;
    if (!path || !data) return -1;
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    f = fopen(tmp, "wb");
    if (!f) return -1;
    n = fwrite(data, 1, len, f);
    if (fclose(f) != 0 || n != len) { remove(tmp); return -1; }
    remove(path);
    if (rename(tmp, path) != 0) { remove(tmp); return -1; }
    return 0;
}

int Save_Write(void) {
    pack_save();
    if (write_file_atomic(GameVersion_SavePath(NULL), &save, sizeof(save)) != 0)
        return -1;
    save_vmap_sidecar_write();
    save_npc_rt_sidecar_write();
    return 0;
}

#define STATE_MAGIC   0x504B5354u

#define STATE_VERSION 8u

typedef struct PACKED {
    uint32_t magic;
    uint32_t version;

    uint8_t  wCurMap, wLastMap;
    uint16_t wYCoord, wXCoord;
    uint8_t  wYBlockCoord, wXBlockCoord;
    uint8_t  wDestinationWarpID, wMapBackgroundTile;
    uint8_t  wPlayerMovingDirection, wPlayerLastStopDirection, wPlayerDirection;
    uint8_t  wWalkBikeSurfState, wWalkCounter, wStepCounter;
    uint8_t  wRepelRemainingSteps;
    int8_t   gPlayerFacing;
    int8_t   gScene;
    int16_t  gScrollPxX, gScrollPxY;
    int16_t  gCamX, gCamY;

    uint8_t  hRandomAdd, hRandomSub, hFrameCounter;

    uint8_t  wPlayerName[NAME_LENGTH];
    uint8_t  wRivalName[NAME_LENGTH];
    uint16_t wPlayerID;
    uint8_t  wObtainedBadges;
    uint8_t  wRivalStarter;

    uint8_t  wPlayerMoney[3];
    uint32_t wAmountMoneyWon;
    uint8_t  wNumBagItems;
    uint8_t  wBagItems[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t  wNumBoxItems;
    uint8_t  wBoxItems[PC_ITEM_CAPACITY * 2 + 1];

    uint8_t     wPartyCount;
    party_mon_t wPartyMons[PARTY_LENGTH];
    uint8_t     wPartyMonOT[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     wPartyMonNicks[PARTY_LENGTH][NAME_LENGTH];
    uint8_t     wPartySpecies[PARTY_LENGTH + 1];
    uint8_t     wNumHoFTeams;
    hall_of_fame_team_t wHallOfFameTeams[HOF_TEAM_CAPACITY];

    uint8_t  wPokedexOwned[19];
    uint8_t  wPokedexSeen[19];
    uint8_t  wEventFlags[EVENT_FLAGS_BYTES];
    uint8_t  wHandAuthoredEventFlags[PKS_HANDAUTHORED_EVENT_BYTES];
    uint16_t wCompletedInGameTradeFlags;
    uint16_t wPickedUpItems[248];

    uint8_t  wIsInBattle, wBattleType;
    uint8_t  wCurEnemyLevel, wTrainerClass, wLoneAttackNo;
    uint8_t  hWhoseTurn;
    uint8_t  wPlayerMonNumber, wCalculateWhoseStats;
    battle_mon_t wBattleMon, wEnemyMon;
    uint8_t  wPlayerMonStatMods[NUM_STAT_MODS];
    uint8_t  wEnemyMonStatMods[NUM_STAT_MODS];
    uint8_t  wPlayerBattleStatus1, wPlayerBattleStatus2, wPlayerBattleStatus3;
    uint8_t  wEnemyBattleStatus1,  wEnemyBattleStatus2,  wEnemyBattleStatus3;
    uint8_t  wPlayerConfusedCounter, wPlayerToxicCounter, wPlayerDisabledMove;
    uint8_t  wEnemyConfusedCounter,  wEnemyToxicCounter,  wEnemyDisabledMove;
    uint8_t  wPlayerSelectedMove,  wEnemySelectedMove;
    uint8_t  wPlayerMoveNum,       wEnemyMoveNum;
    uint8_t  wCriticalHitOrOHKO;
    uint16_t wDamage;
    uint8_t  wMoveMissed;
    uint8_t  wActionResultOrTookBattleTurn;
    uint8_t  wPlayerUsedMove,      wEnemyUsedMove;
    uint8_t  wPlayerNumAttacksLeft, wEnemyNumAttacksLeft;
    uint8_t  wPlayerNumHits,       wEnemyNumHits;
    uint16_t wPlayerBideAccumulatedDamage, wEnemyBideAccumulatedDamage;
    uint8_t  wPlayerSubstituteHP,  wEnemySubstituteHP;
    uint8_t  wPlayerMonMinimized,  wEnemyMonMinimized;
    uint8_t  wPlayerDisabledMoveNumber, wEnemyDisabledMoveNumber;
    uint16_t wPlayerMonUnmodifiedAttack,  wPlayerMonUnmodifiedDefense;
    uint16_t wPlayerMonUnmodifiedSpeed,   wPlayerMonUnmodifiedSpecial;
    uint16_t wEnemyMonUnmodifiedAttack,   wEnemyMonUnmodifiedDefense;
    uint16_t wEnemyMonUnmodifiedSpeed,    wEnemyMonUnmodifiedSpecial;
    uint16_t wTransformedEnemyMonOriginalDVs;
    uint8_t  wFirstMonsNotOutYet, wBattleResult;

    uint8_t     wEnemyPartyCount;
    party_mon_t wEnemyMons[PARTY_LENGTH];
    uint8_t     wEnemyMonPartyPos;
    uint8_t     wNumRunAttempts, wForcePlayerToChooseMon;
    uint8_t     wAICount, wAILayer2Encouragement;
    uint16_t    wLastSwitchInEnemyMonHP;
    uint8_t     gEngagedTrainerClass;

    uint8_t  wPartyGainExpFlags, wPartyFoughtCurrentEnemyFlags;
    uint8_t  wGainBoostedExp, wCanEvolveFlags, wEvolutionOccurred;

    npc_state_t npc_state;

    char vmap_owner_names[PKS_VIRTUAL_MAP_COUNT][PKS_VMAP_BIND_NAME_LEN];

    uint64_t div_cycles;

    uint32_t ow_gate_accum;
    uint8_t  ow_gate_latch;

    uint16_t npc_rt_len;
    uint8_t  npc_rt_blob[PKS_NPC_RT_BLOB_MAX];
} state_block_t;

static state_block_t st;
static int s_last_load_in_battle = 0;
static int s_last_load_err;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

static void pack_state(void) {
    st.magic   = STATE_MAGIC;
    st.version = STATE_VERSION;

    st.wCurMap = wCurMap;   st.wLastMap = wLastMap;
    st.wYCoord = wYCoord;   st.wXCoord  = wXCoord;
    st.wYBlockCoord = wYBlockCoord; st.wXBlockCoord = wXBlockCoord;
    st.wDestinationWarpID = wDestinationWarpID;
    st.wMapBackgroundTile = wMapBackgroundTile;
    st.wPlayerMovingDirection    = wPlayerMovingDirection;
    st.wPlayerLastStopDirection  = wPlayerLastStopDirection;
    st.wPlayerDirection          = wPlayerDirection;
    st.wWalkBikeSurfState = wWalkBikeSurfState;
    st.wWalkCounter       = wWalkCounter;
    st.wStepCounter       = wStepCounter;
    st.wRepelRemainingSteps = wRepelRemainingSteps;
    st.gPlayerFacing = (int8_t)gPlayerFacing;
    st.gScene        = (int8_t)Game_GetScene();
    st.gScrollPxX    = (int16_t)gScrollPxX;
    st.gScrollPxY    = (int16_t)gScrollPxY;
    st.gCamX         = (int16_t)gCamX;
    st.gCamY         = (int16_t)gCamY;

    st.hRandomAdd     = hRandomAdd;
    st.hRandomSub     = hRandomSub;
    st.hFrameCounter  = hFrameCounter;

    memcpy(st.wPlayerName, wPlayerName, NAME_LENGTH);
    memcpy(st.wRivalName,  wRivalName,  NAME_LENGTH);
    st.wPlayerID      = wPlayerID;
    st.wObtainedBadges = wObtainedBadges;
    st.wRivalStarter   = wRivalStarter;

    memcpy(st.wPlayerMoney, wPlayerMoney, 3);
    st.wAmountMoneyWon = wAmountMoneyWon;
    st.wNumBagItems    = wNumBagItems;
    memcpy(st.wBagItems, wBagItems, sizeof(wBagItems));
    st.wNumBoxItems    = wNumBoxItems;
    memcpy(st.wBoxItems, wBoxItems, sizeof(wBoxItems));

    st.wPartyCount = wPartyCount;
    memcpy(st.wPartyMons,    wPartyMons,    sizeof(wPartyMons));
    memcpy(st.wPartyMonOT,   wPartyMonOT,   sizeof(wPartyMonOT));
    memcpy(st.wPartyMonNicks,wPartyMonNicks,sizeof(wPartyMonNicks));
    memcpy(st.wPartySpecies, wPartySpecies, sizeof(wPartySpecies));
    st.wNumHoFTeams = wNumHoFTeams;
    memcpy(st.wHallOfFameTeams, wHallOfFameTeams, sizeof(wHallOfFameTeams));

    memcpy(st.wPokedexOwned, wPokedexOwned, sizeof(wPokedexOwned));
    memcpy(st.wPokedexSeen,  wPokedexSeen,  sizeof(wPokedexSeen));
    memcpy(st.wEventFlags,   wEventFlags,   EVENT_FLAGS_BYTES);
    memcpy(st.wHandAuthoredEventFlags, wHandAuthoredEventFlags, PKS_HANDAUTHORED_EVENT_BYTES);
    st.wCompletedInGameTradeFlags = wCompletedInGameTradeFlags;
    memcpy(st.wPickedUpItems,wPickedUpItems,sizeof(wPickedUpItems));

    st.wIsInBattle = wIsInBattle; st.wBattleType = wBattleType;
    st.wCurEnemyLevel = wCurEnemyLevel; st.wTrainerClass = wTrainerClass;
    st.wLoneAttackNo  = wLoneAttackNo;
    st.hWhoseTurn = hWhoseTurn;
    st.wPlayerMonNumber = wPlayerMonNumber;
    st.wCalculateWhoseStats = wCalculateWhoseStats;
    st.wBattleMon = wBattleMon; st.wEnemyMon = wEnemyMon;
    memcpy(st.wPlayerMonStatMods, wPlayerMonStatMods, NUM_STAT_MODS);
    memcpy(st.wEnemyMonStatMods,  wEnemyMonStatMods,  NUM_STAT_MODS);
    st.wPlayerBattleStatus1 = wPlayerBattleStatus1;
    st.wPlayerBattleStatus2 = wPlayerBattleStatus2;
    st.wPlayerBattleStatus3 = wPlayerBattleStatus3;
    st.wEnemyBattleStatus1  = wEnemyBattleStatus1;
    st.wEnemyBattleStatus2  = wEnemyBattleStatus2;
    st.wEnemyBattleStatus3  = wEnemyBattleStatus3;
    st.wPlayerConfusedCounter = wPlayerConfusedCounter;
    st.wPlayerToxicCounter    = wPlayerToxicCounter;
    st.wPlayerDisabledMove    = wPlayerDisabledMove;
    st.wEnemyConfusedCounter  = wEnemyConfusedCounter;
    st.wEnemyToxicCounter     = wEnemyToxicCounter;
    st.wEnemyDisabledMove     = wEnemyDisabledMove;
    st.wPlayerSelectedMove = wPlayerSelectedMove;
    st.wEnemySelectedMove  = wEnemySelectedMove;
    st.wPlayerMoveNum = wPlayerMoveNum; st.wEnemyMoveNum = wEnemyMoveNum;
    st.wCriticalHitOrOHKO = wCriticalHitOrOHKO;
    st.wDamage = wDamage; st.wMoveMissed = wMoveMissed;
    st.wActionResultOrTookBattleTurn = wActionResultOrTookBattleTurn;
    st.wPlayerUsedMove = wPlayerUsedMove; st.wEnemyUsedMove = wEnemyUsedMove;
    st.wPlayerNumAttacksLeft = wPlayerNumAttacksLeft;
    st.wEnemyNumAttacksLeft  = wEnemyNumAttacksLeft;
    st.wPlayerNumHits = wPlayerNumHits; st.wEnemyNumHits = wEnemyNumHits;
    st.wPlayerBideAccumulatedDamage = wPlayerBideAccumulatedDamage;
    st.wEnemyBideAccumulatedDamage  = wEnemyBideAccumulatedDamage;
    st.wPlayerSubstituteHP = wPlayerSubstituteHP;
    st.wEnemySubstituteHP  = wEnemySubstituteHP;
    st.wPlayerMonMinimized = wPlayerMonMinimized;
    st.wEnemyMonMinimized  = wEnemyMonMinimized;
    st.wPlayerDisabledMoveNumber = wPlayerDisabledMoveNumber;
    st.wEnemyDisabledMoveNumber  = wEnemyDisabledMoveNumber;
    st.wPlayerMonUnmodifiedAttack  = wPlayerMonUnmodifiedAttack;
    st.wPlayerMonUnmodifiedDefense = wPlayerMonUnmodifiedDefense;
    st.wPlayerMonUnmodifiedSpeed   = wPlayerMonUnmodifiedSpeed;
    st.wPlayerMonUnmodifiedSpecial = wPlayerMonUnmodifiedSpecial;
    st.wEnemyMonUnmodifiedAttack   = wEnemyMonUnmodifiedAttack;
    st.wEnemyMonUnmodifiedDefense  = wEnemyMonUnmodifiedDefense;
    st.wEnemyMonUnmodifiedSpeed    = wEnemyMonUnmodifiedSpeed;
    st.wEnemyMonUnmodifiedSpecial  = wEnemyMonUnmodifiedSpecial;
    st.wTransformedEnemyMonOriginalDVs = wTransformedEnemyMonOriginalDVs;
    st.wFirstMonsNotOutYet = wFirstMonsNotOutYet;
    st.wBattleResult = wBattleResult;

    st.wEnemyPartyCount = wEnemyPartyCount;
    memcpy(st.wEnemyMons, wEnemyMons, sizeof(wEnemyMons));
    st.wEnemyMonPartyPos       = wEnemyMonPartyPos;
    st.wNumRunAttempts         = wNumRunAttempts;
    st.wForcePlayerToChooseMon = wForcePlayerToChooseMon;
    st.wAICount                = wAICount;
    st.wAILayer2Encouragement  = wAILayer2Encouragement;
    st.wLastSwitchInEnemyMonHP = wLastSwitchInEnemyMonHP;
    st.gEngagedTrainerClass    = gEngagedTrainerClass;

    st.wPartyGainExpFlags             = wPartyGainExpFlags;
    st.wPartyFoughtCurrentEnemyFlags  = wPartyFoughtCurrentEnemyFlags;
    st.wGainBoostedExp   = wGainBoostedExp;
    st.wCanEvolveFlags   = wCanEvolveFlags;
    st.wEvolutionOccurred = wEvolutionOccurred;
    {
        npc_state_t tmp_npc;
        NPC_StateCapture(&tmp_npc);
        memcpy(&st.npc_state, &tmp_npc, sizeof(tmp_npc));
    }

    {
        extern void Game_GetOwGateState(uint32_t *accum, uint8_t *latch);
        AmberScript_MapBank_SnapshotBindings(st.vmap_owner_names,
                                            PKS_VIRTUAL_MAP_COUNT);
        st.div_cycles = Random_GetDivCycles();
        Game_GetOwGateState(&st.ow_gate_accum, &st.ow_gate_latch);
        st.npc_rt_len = (uint16_t)AmberScript_MapBank_SerializeNpcRt(
            st.npc_rt_blob, (int)sizeof(st.npc_rt_blob));
    }
}

static void unpack_state(void) {

    AmberScript_MapBank_RestoreBindings(
        (const char (*)[PKS_VMAP_BIND_NAME_LEN])st.vmap_owner_names,
        PKS_VIRTUAL_MAP_COUNT);
    Random_SetDivCycles(st.div_cycles);
    {
        extern void Game_SetOwGateState(uint32_t accum, uint8_t latch);
        Game_SetOwGateState(st.ow_gate_accum, st.ow_gate_latch);
    }
    AmberScript_MapBank_DeserializeNpcRt(st.npc_rt_blob, (int)st.npc_rt_len);

    wCurMap = st.wCurMap; wLastMap = st.wLastMap;

    if (wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST)
        AmberScript_SetEnabled(1);
    wYCoord = st.wYCoord; wXCoord  = st.wXCoord;
    wYBlockCoord = st.wYBlockCoord; wXBlockCoord = st.wXBlockCoord;
    wDestinationWarpID = st.wDestinationWarpID;
    wMapBackgroundTile = st.wMapBackgroundTile;
    wPlayerMovingDirection   = st.wPlayerMovingDirection;
    wPlayerLastStopDirection = st.wPlayerLastStopDirection;
    wPlayerDirection         = st.wPlayerDirection;
    wWalkBikeSurfState = st.wWalkBikeSurfState;
    wWalkCounter       = st.wWalkCounter;
    wStepCounter       = st.wStepCounter;
    wRepelRemainingSteps = st.wRepelRemainingSteps;
    gPlayerFacing = (int)st.gPlayerFacing;
    Game_SetScene((int)st.gScene);
    gScrollPxX = (int)st.gScrollPxX;
    gScrollPxY = (int)st.gScrollPxY;
    gCamX      = (int)st.gCamX;
    gCamY      = (int)st.gCamY;

    hRandomAdd    = st.hRandomAdd;
    hRandomSub    = st.hRandomSub;
    hFrameCounter = st.hFrameCounter;

    memcpy(wPlayerName, st.wPlayerName, NAME_LENGTH);
    memcpy(wRivalName,  st.wRivalName,  NAME_LENGTH);
    wPlayerID       = st.wPlayerID;
    wObtainedBadges = st.wObtainedBadges;
    wRivalStarter   = st.wRivalStarter;

    memcpy(wPlayerMoney, st.wPlayerMoney, 3);
    wAmountMoneyWon = st.wAmountMoneyWon;
    wNumBagItems    = st.wNumBagItems;
    memcpy(wBagItems, st.wBagItems, sizeof(wBagItems));
    wNumBoxItems    = st.wNumBoxItems;
    memcpy(wBoxItems, st.wBoxItems, sizeof(wBoxItems));

    wPartyCount = st.wPartyCount;
    memcpy(wPartyMons,    st.wPartyMons,    sizeof(wPartyMons));
    memcpy(wPartyMonOT,   st.wPartyMonOT,   sizeof(wPartyMonOT));
    memcpy(wPartyMonNicks,st.wPartyMonNicks,sizeof(wPartyMonNicks));
    memcpy(wPartySpecies, st.wPartySpecies, sizeof(wPartySpecies));
    wNumHoFTeams = st.wNumHoFTeams;
    memcpy(wHallOfFameTeams, st.wHallOfFameTeams, sizeof(wHallOfFameTeams));

    memcpy(wPokedexOwned, st.wPokedexOwned, sizeof(wPokedexOwned));
    memcpy(wPokedexSeen,  st.wPokedexSeen,  sizeof(wPokedexSeen));
    memcpy(wEventFlags,   st.wEventFlags,   EVENT_FLAGS_BYTES);
    memcpy(wHandAuthoredEventFlags, st.wHandAuthoredEventFlags, PKS_HANDAUTHORED_EVENT_BYTES);
    wCompletedInGameTradeFlags = st.wCompletedInGameTradeFlags;

    memcpy(wPickedUpItems,st.wPickedUpItems,sizeof(wPickedUpItems));

    wIsInBattle = st.wIsInBattle; wBattleType = st.wBattleType;
    wCurEnemyLevel = st.wCurEnemyLevel; wTrainerClass = st.wTrainerClass;
    wLoneAttackNo  = st.wLoneAttackNo;
    hWhoseTurn = st.hWhoseTurn;
    wPlayerMonNumber    = st.wPlayerMonNumber;
    wCalculateWhoseStats = st.wCalculateWhoseStats;
    wBattleMon = st.wBattleMon; wEnemyMon = st.wEnemyMon;
    memcpy(wPlayerMonStatMods, st.wPlayerMonStatMods, NUM_STAT_MODS);
    memcpy(wEnemyMonStatMods,  st.wEnemyMonStatMods,  NUM_STAT_MODS);
    wPlayerBattleStatus1 = st.wPlayerBattleStatus1;
    wPlayerBattleStatus2 = st.wPlayerBattleStatus2;
    wPlayerBattleStatus3 = st.wPlayerBattleStatus3;
    wEnemyBattleStatus1  = st.wEnemyBattleStatus1;
    wEnemyBattleStatus2  = st.wEnemyBattleStatus2;
    wEnemyBattleStatus3  = st.wEnemyBattleStatus3;
    wPlayerConfusedCounter = st.wPlayerConfusedCounter;
    wPlayerToxicCounter    = st.wPlayerToxicCounter;
    wPlayerDisabledMove    = st.wPlayerDisabledMove;
    wEnemyConfusedCounter  = st.wEnemyConfusedCounter;
    wEnemyToxicCounter     = st.wEnemyToxicCounter;
    wEnemyDisabledMove     = st.wEnemyDisabledMove;
    wPlayerSelectedMove = st.wPlayerSelectedMove;
    wEnemySelectedMove  = st.wEnemySelectedMove;
    wPlayerMoveNum = st.wPlayerMoveNum; wEnemyMoveNum = st.wEnemyMoveNum;
    wCriticalHitOrOHKO = st.wCriticalHitOrOHKO;
    wDamage = st.wDamage; wMoveMissed = st.wMoveMissed;
    wActionResultOrTookBattleTurn = st.wActionResultOrTookBattleTurn;
    wPlayerUsedMove = st.wPlayerUsedMove; wEnemyUsedMove = st.wEnemyUsedMove;
    wPlayerNumAttacksLeft = st.wPlayerNumAttacksLeft;
    wEnemyNumAttacksLeft  = st.wEnemyNumAttacksLeft;
    wPlayerNumHits = st.wPlayerNumHits; wEnemyNumHits = st.wEnemyNumHits;
    wPlayerBideAccumulatedDamage = st.wPlayerBideAccumulatedDamage;
    wEnemyBideAccumulatedDamage  = st.wEnemyBideAccumulatedDamage;
    wPlayerSubstituteHP = st.wPlayerSubstituteHP;
    wEnemySubstituteHP  = st.wEnemySubstituteHP;
    wPlayerMonMinimized = st.wPlayerMonMinimized;
    wEnemyMonMinimized  = st.wEnemyMonMinimized;
    wPlayerDisabledMoveNumber = st.wPlayerDisabledMoveNumber;
    wEnemyDisabledMoveNumber  = st.wEnemyDisabledMoveNumber;
    wPlayerMonUnmodifiedAttack  = st.wPlayerMonUnmodifiedAttack;
    wPlayerMonUnmodifiedDefense = st.wPlayerMonUnmodifiedDefense;
    wPlayerMonUnmodifiedSpeed   = st.wPlayerMonUnmodifiedSpeed;
    wPlayerMonUnmodifiedSpecial = st.wPlayerMonUnmodifiedSpecial;
    wEnemyMonUnmodifiedAttack   = st.wEnemyMonUnmodifiedAttack;
    wEnemyMonUnmodifiedDefense  = st.wEnemyMonUnmodifiedDefense;
    wEnemyMonUnmodifiedSpeed    = st.wEnemyMonUnmodifiedSpeed;
    wEnemyMonUnmodifiedSpecial  = st.wEnemyMonUnmodifiedSpecial;
    wTransformedEnemyMonOriginalDVs = st.wTransformedEnemyMonOriginalDVs;
    wFirstMonsNotOutYet = st.wFirstMonsNotOutYet;
    wBattleResult = st.wBattleResult;

    wEnemyPartyCount = st.wEnemyPartyCount;
    memcpy(wEnemyMons, st.wEnemyMons, sizeof(wEnemyMons));
    wEnemyMonPartyPos       = st.wEnemyMonPartyPos;
    wNumRunAttempts         = st.wNumRunAttempts;
    wForcePlayerToChooseMon = st.wForcePlayerToChooseMon;
    wAICount               = st.wAICount;
    wAILayer2Encouragement = st.wAILayer2Encouragement;
    wLastSwitchInEnemyMonHP = st.wLastSwitchInEnemyMonHP;
    gEngagedTrainerClass    = st.gEngagedTrainerClass;

    wPartyGainExpFlags            = st.wPartyGainExpFlags;
    wPartyFoughtCurrentEnemyFlags = st.wPartyFoughtCurrentEnemyFlags;
    wGainBoostedExp    = st.wGainBoostedExp;
    wCanEvolveFlags    = st.wCanEvolveFlags;
    wEvolutionOccurred = st.wEvolutionOccurred;
    {
        npc_state_t tmp_npc;
        memcpy(&tmp_npc, &st.npc_state, sizeof(tmp_npc));
        NPC_StateRestore(&tmp_npc);
    }
}

static size_t state_payload_size(uint32_t v) {
    if (v == STATE_VERSION) return sizeof(state_block_t);

    return 0;
}

static int state_migrate_in_place(size_t n) {
    size_t want;
    if (n < sizeof(uint32_t) * 2u)  return SAVE_STATE_ERR_CORRUPT;
    if (st.magic != STATE_MAGIC)    return SAVE_STATE_ERR_CORRUPT;

    want = state_payload_size(st.version);
    if (want == 0)                  return SAVE_STATE_ERR_VERSION;

    if (n != want)                  return SAVE_STATE_ERR_CORRUPT;

    if (want < sizeof(st)) {

        memset((char *)&st + want, 0, sizeof(st) - want);
        st.version = STATE_VERSION;
    }
    return SAVE_STATE_ERR_NONE;
}

int Save_StateWrite(const char *path) {
    pack_state();
    return write_file_atomic(path, &st, sizeof(st));
}

int Save_StateLoad(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    size_t n = fread(&st, 1, sizeof(st), f);
    fclose(f);
    s_last_load_err = state_migrate_in_place(n);
    if (s_last_load_err != SAVE_STATE_ERR_NONE) return -1;
    unpack_state();
    s_last_load_in_battle = (st.wIsInBattle != 0);
    s_last_load_err = SAVE_STATE_ERR_NONE;
    return 0;
}

int Save_StateLastError(void) { return s_last_load_err; }

int Save_StateWasBattle(void) { return s_last_load_in_battle; }

int Save_StatePeek(const char *path, int *out_version) {
    uint32_t hdr[2];
    size_t n;
    FILE *f = fopen(path, "rb");
    if (!f) return SAVE_STATE_ERR_EMPTY;
    n = fread(hdr, 1, sizeof hdr, f);
    fclose(f);
    if (n != sizeof hdr || hdr[0] != STATE_MAGIC) return SAVE_STATE_ERR_CORRUPT;
    if (out_version) *out_version = (int)hdr[1];

    if (state_payload_size(hdr[1]) == 0) return SAVE_STATE_ERR_VERSION;
    return SAVE_STATE_ERR_NONE;
}

size_t Save_StateSize(void) {
    return sizeof(st);
}

int Save_StateCaptureToBuffer(void *dst, size_t dst_size) {
    if (!dst || dst_size < sizeof(st)) return -1;
    pack_state();
    memcpy(dst, &st, sizeof(st));
    return 0;
}

int Save_StateLoadFromBuffer(const void *src, size_t src_size) {

    size_t n;
    if (!src) return -1;
    n = (src_size < sizeof(st)) ? src_size : sizeof(st);
    memcpy(&st, src, n);
    if (state_migrate_in_place(n) != SAVE_STATE_ERR_NONE) return -1;
    unpack_state();
    s_last_load_in_battle = (st.wIsInBattle != 0);
    return 0;
}
