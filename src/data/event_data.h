#pragma once

#include <stdint.h>

typedef struct {
    uint16_t x, y;
    uint8_t  dest_map;
    uint8_t  dest_warp_idx;
} map_warp_t;

typedef void (*npc_script_fn)(void);

typedef struct {
    uint16_t      x, y;
    uint8_t       sprite_id;
    uint8_t       movement;
    const char   *text;
    npc_script_fn script;

    uint8_t       facing;

    uint8_t       src_idx;

    uint8_t       crystal_pal;

    uint8_t       starts_hidden;
} npc_event_t;

typedef struct {
    uint8_t      npc_idx;
    uint8_t      facing;
    uint8_t      trainer_class;
    uint8_t      trainer_no;
    uint8_t      sight_dist;
    uint16_t     flag_bit;
    const char  *before_text;
    const char  *after_text;
    const char  *end_text;

    const char  *defeat_text;

    const char  *after_battle_scene;

    uint16_t     johto_party;
} map_trainer_t;

typedef struct {
    uint16_t    x, y;
    const char *text;
} sign_event_t;

typedef struct {
    uint16_t x, y;
    uint8_t  item_id;
    uint8_t  src_idx;
} item_event_t;

typedef void (*hidden_script_fn)(void);
typedef struct {
    int16_t           x, y;
    const char       *text;
    hidden_script_fn  script;
    uint8_t           facing;
} hidden_event_t;

typedef struct {
    const map_warp_t      *warps;
    uint8_t                num_warps;
    const npc_event_t     *npcs;
    uint8_t                num_npcs;
    const sign_event_t    *signs;
    uint8_t                num_signs;
    const item_event_t    *items;
    uint8_t                num_items;
    uint8_t                border_block;
    const map_trainer_t   *trainers;
    uint8_t                num_trainers;
    const hidden_event_t  *hidden_events;
    uint8_t                num_hidden_events;
} map_events_t;

#define NUM_MAPS 256

extern map_events_t gMapEvents[NUM_MAPS];
void MapEvents_LoadFromPack(void);
