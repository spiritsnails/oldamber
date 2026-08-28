#pragma once

#include <stdint.h>
#include "../platform/save.h"
#include <stddef.h>

#define SAVE_SLOT_COUNT 6

#define SAVE_SLOT_THUMB_W 256
#define SAVE_SLOT_THUMB_H 144
#define SAVE_SLOT_THUMB_MAX (SAVE_SLOT_THUMB_W * SAVE_SLOT_THUMB_H)

typedef struct {
    int      occupied;
    int      readable;
    int      err;
    int64_t  when;
    char     map[24];
    uint8_t  badges;
    uint8_t  party_count;
    uint8_t  party_level[6];
    uint16_t thumb_w, thumb_h;
    const uint32_t *thumb;
} save_slot_info_t;

const save_slot_info_t *SaveSlots_Info(int slot);

int SaveSlots_Write(int slot);

int SaveSlots_Read(int slot);

int SaveSlots_Delete(int slot);

const char *SaveSlots_When(int slot);
