
#pragma once
#include <stdint.h>

typedef enum {
    CRYSTAL_ANIM_ANIMATE_FLOWER_TILE = 0,
    CRYSTAL_ANIM_ANIMATE_FOUNTAIN_TILE = 1,
    CRYSTAL_ANIM_ANIMATE_LAVA_BUBBLE_TILE1 = 2,
    CRYSTAL_ANIM_ANIMATE_LAVA_BUBBLE_TILE2 = 3,
    CRYSTAL_ANIM_ANIMATE_TOWER_PILLAR_TILE = 4,
    CRYSTAL_ANIM_ANIMATE_WATER_PALETTE = 5,
    CRYSTAL_ANIM_ANIMATE_WATER_TILE = 6,
    CRYSTAL_ANIM_ANIMATE_WHIRLPOOL_TILE = 7,
    CRYSTAL_ANIM_DONE_TILE_ANIMATION = 8,
    CRYSTAL_ANIM_FLICKERING_CAVE_ENTRANCE_PALETTE = 9,
    CRYSTAL_ANIM_FOREST_TREE_LEFT_ANIMATION = 10,
    CRYSTAL_ANIM_FOREST_TREE_LEFT_ANIMATION2 = 11,
    CRYSTAL_ANIM_FOREST_TREE_RIGHT_ANIMATION = 12,
    CRYSTAL_ANIM_FOREST_TREE_RIGHT_ANIMATION2 = 13,
    CRYSTAL_ANIM_READ_TILE_TO_ANIM_BUFFER = 14,
    CRYSTAL_ANIM_SCROLL_TILE_DOWN = 15,
    CRYSTAL_ANIM_SCROLL_TILE_RIGHT_LEFT = 16,
    CRYSTAL_ANIM_STANDING_TILE_FRAME = 17,
    CRYSTAL_ANIM_STANDING_TILE_FRAME8 = 18,
    CRYSTAL_ANIM_WAIT_TILE_ANIMATION = 19,
    CRYSTAL_ANIM_WRITE_TILE_FROM_ANIM_BUFFER = 20,
    CRYSTAL_ANIM_COUNT = 21
} crystal_anim_routine_t;

typedef struct {
    uint8_t  routine;
    uint8_t  arg_kind;
    uint16_t arg;
} crystal_anim_frame_t;

typedef struct {
    uint16_t first;
    uint8_t  count;
} crystal_anim_script_t;

typedef struct {
    uint16_t dest_tile;
    uint8_t  num_frames;
    uint8_t  frames[8][16];
} crystal_anim_descriptor_t;

#define CRYSTAL_ANIM_NUM_SCRIPTS 10
#define CRYSTAL_ANIM_NUM_FRAMES 126
#define CRYSTAL_ANIM_NUM_DESCRIPTORS 14
#define CRYSTAL_NUM_TILESETS 37

#define CRYSTAL_ANIM_SCRIPT_TILESET0_ANIM 0
#define CRYSTAL_ANIM_SCRIPT_TILESET_AERODACTYL_WORD_ROOM_ANIM 1
#define CRYSTAL_ANIM_SCRIPT_TILESET_CAVE_ANIM 2
#define CRYSTAL_ANIM_SCRIPT_TILESET_ELITE_FOUR_ROOM_ANIM 3
#define CRYSTAL_ANIM_SCRIPT_TILESET_FOREST_ANIM 4
#define CRYSTAL_ANIM_SCRIPT_TILESET_ICE_PATH_ANIM 5
#define CRYSTAL_ANIM_SCRIPT_TILESET_JOHTO_ANIM 6
#define CRYSTAL_ANIM_SCRIPT_TILESET_PARK_ANIM 7
#define CRYSTAL_ANIM_SCRIPT_TILESET_PORT_ANIM 8
#define CRYSTAL_ANIM_SCRIPT_TILESET_TOWER_ANIM 9

extern const crystal_anim_frame_t gCrystalAnimFrames[CRYSTAL_ANIM_NUM_FRAMES];
extern const crystal_anim_script_t gCrystalAnimScripts[CRYSTAL_ANIM_NUM_SCRIPTS];

extern const uint8_t gCrystalTilesetAnim[CRYSTAL_NUM_TILESETS];
extern const crystal_anim_descriptor_t gCrystalAnimDescriptors[CRYSTAL_ANIM_NUM_DESCRIPTORS];

#define CRYSTAL_ANIM_FLOWER_FRAMES 4
extern const uint8_t gCrystalAnimFlower[4][16];
#define CRYSTAL_ANIM_FOREST_LEFT_FRAMES 2
extern const uint8_t gCrystalAnimForestLeft[2][16];
#define CRYSTAL_ANIM_FOREST_RIGHT_FRAMES 2
extern const uint8_t gCrystalAnimForestRight[2][16];
#define CRYSTAL_ANIM_LAVA_FRAMES 4
extern const uint8_t gCrystalAnimLava[4][16];
#define CRYSTAL_ANIM_WATER_FRAMES 4
extern const uint8_t gCrystalAnimWater[4][16];
#define CRYSTAL_ANIM_FOUNTAIN_FRAMES 8
extern const uint8_t gCrystalAnimFountain[8][16];

extern const uint8_t gCrystalAnimPillarSeq[8];
