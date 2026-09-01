#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../game/types.h"
#include "../game/constants.h"

#define SAVE_FILE "pokered.sav"

int  Save_Load(void);

int  Save_LoadFrom(const char *path);

typedef struct {
    uint8_t player_name[11];
    uint8_t badges;
    uint8_t pokedex_owned[19];
    int     valid;
} save_peek_t;

int  Save_PeekFrom(const char *path, save_peek_t *out);

typedef struct {
    uint8_t player_name[NAME_LENGTH];
    uint8_t rival_name[NAME_LENGTH];
    uint16_t player_id;
    uint32_t money;
    uint16_t coins;
    uint8_t badges;
    uint8_t cur_map, last_map, x_coord, y_coord;
    uint8_t pokedex_owned[19];
    uint8_t pokedex_seen[19];
    uint8_t num_bag_items;
    uint8_t bag_items[BAG_ITEM_CAPACITY * 2 + 1];
    uint8_t num_box_items;
    uint8_t box_items[PC_ITEM_CAPACITY * 2 + 1];
    uint8_t party_count;
    party_mon_t party_mons[PARTY_LENGTH];
    uint8_t party_ot[PARTY_LENGTH][NAME_LENGTH];
    uint8_t party_nicks[PARTY_LENGTH][NAME_LENGTH];
    uint8_t current_box_num;
    uint8_t box_count[NUM_BOXES];
    uint8_t box_species[NUM_BOXES][BOX_CAPACITY + 1];
    box_mon_t box_mons[NUM_BOXES][BOX_CAPACITY];
    uint8_t box_ot[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t box_nicks[NUM_BOXES][BOX_CAPACITY][NAME_LENGTH];
    uint8_t event_flags[448];
    uint8_t hand_authored_flags[4];
} save_editor_data_t;

int Save_EditorRead(const char *path, save_editor_data_t *out);
int Save_EditorWrite(const char *path, const save_editor_data_t *data);

int  Save_Write(void);

int  Save_ValidateChecksum(void);

int  Save_StateWrite(const char *path);
int  Save_StateLoad(const char *path);
size_t Save_StateSize(void);
int  Save_StateCaptureToBuffer(void *dst, size_t dst_size);
int  Save_StateLoadFromBuffer(const void *src, size_t src_size);

int  Save_StateWasBattle(void);

enum {
    SAVE_STATE_ERR_NONE = 0,
    SAVE_STATE_ERR_EMPTY,
    SAVE_STATE_ERR_CORRUPT,
    SAVE_STATE_ERR_VERSION,
};
int  Save_StateLastError(void);

int  Save_StatePeek(const char *path, int *out_version);
