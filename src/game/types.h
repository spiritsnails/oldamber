#pragma once
#include <stdint.h>
#include "../platform/compiler.h"

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

typedef struct PACKED {
    uint8_t  species;
    uint16_t hp;
    uint8_t  box_level;
    uint8_t  status;
    uint8_t  type1;
    uint8_t  type2;
    uint8_t  catch_rate;
    uint8_t  moves[4];
    uint16_t ot_id;
    uint8_t  exp[3];
    uint16_t stat_exp_hp;
    uint16_t stat_exp_atk;
    uint16_t stat_exp_def;
    uint16_t stat_exp_spd;
    uint16_t stat_exp_spc;
    uint16_t dvs;
    uint8_t  pp[4];
} box_mon_t;

typedef struct PACKED {
    box_mon_t base;
    uint8_t   level;
    uint16_t  max_hp;
    uint16_t  atk;
    uint16_t  def;
    uint16_t  spd;
    uint16_t  spc;
} party_mon_t;

typedef struct PACKED {
    uint8_t  species;
    uint16_t hp;
    uint8_t  party_pos;
    uint8_t  status;
    uint8_t  type1;
    uint8_t  type2;
    uint8_t  catch_rate;
    uint8_t  moves[4];
    uint16_t dvs;
    uint8_t  level;
    uint16_t max_hp;
    uint16_t atk;
    uint16_t def;
    uint16_t spd;
    uint16_t spc;
    uint8_t  pp[4];
} battle_mon_t;

typedef struct PACKED {
    uint8_t anim;
    uint8_t effect;
    uint8_t power;
    uint8_t type;
    uint8_t accuracy;
    uint8_t pp;
} move_t;

typedef struct PACKED {
    uint8_t picture_id;
    uint8_t movement_status;
    uint8_t image_index;
    uint8_t y_disp;
    uint8_t x_disp;
    uint8_t map_y;
    uint8_t map_x;
    uint8_t movement_byte_2;
    uint8_t grass_priority;
    uint8_t y_pixels;
    uint8_t x_pixels;
    uint8_t intra_anim_frame;
    uint8_t collision_data;
    uint8_t facing_direction;
    uint8_t _pad[2];
} sprite_state_data1_t;

typedef struct PACKED {
    uint8_t walk_animation_counter;
    uint8_t movement_byte1;
    uint8_t _unk1;
    uint8_t text_id;
    uint8_t trainer_class_or_item;
    uint8_t trainer_set_id;
    uint8_t _unk2[2];
    uint8_t y_displacement;
    uint8_t x_displacement;
    uint8_t _unk3[6];
} sprite_state_data2_t;

typedef struct PACKED {
    uint8_t y;

    int16_t x;
    uint8_t tile;
    uint8_t flags;
} oam_entry_t;

typedef struct PACKED {
    uint8_t  tileset;
    uint8_t  height;
    uint8_t  width;
    uint16_t blocks_ptr;
    uint16_t text_ptr;
    uint16_t script_ptr;
    uint8_t  connections;
} map_header_t;

typedef struct PACKED {
    uint8_t  map_id;
    uint16_t strip_src_ptr;
    uint16_t strip_dst_ptr;
    uint8_t  strip_width;
    uint8_t  window;
    uint8_t  map_pos;
    uint8_t  _pad[2];
} map_connection_t;

typedef struct PACKED {
    uint8_t y;
    uint8_t x;
    uint8_t dest_warp_id;
    uint8_t dest_map;
} warp_event_t;

typedef struct PACKED {
    uint8_t y;
    uint8_t x;
    uint8_t text_id;
} bg_event_t;

typedef struct PACKED {
    uint8_t  bank;
    uint16_t blocks_ptr;
    uint16_t gfx_ptr;
    uint16_t coll_ptr;
    uint8_t  counter[3];
    uint8_t  grass_tile;
    uint8_t  anim;
} tileset_header_t;

#define NUM_TM_HM_BYTES  7

typedef struct PACKED {
    uint8_t  dex_id;
    uint8_t  hp, atk, def, spd, spc;
    uint8_t  type1, type2;
    uint8_t  catch_rate;
    uint8_t  base_exp;
    uint8_t  sprite_dim;
    uint16_t front_ptr;
    uint16_t back_ptr;
    uint8_t  start_moves[4];
    uint8_t  growth_rate;
    uint8_t  tmhm[NUM_TM_HM_BYTES];
} base_stats_t;

#define NUM_WILD_SLOTS  10

typedef struct PACKED {
    uint8_t encounter_rate;
    struct { uint8_t level; uint8_t species; } slots[NUM_WILD_SLOTS];
} wild_data_t;

typedef struct PACKED {
    uint8_t item_id;
    uint8_t quantity;
} item_slot_t;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif
