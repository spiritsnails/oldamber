#pragma once

#include <stdint.h>

typedef enum {
    ITEM_USE_FAILED      = 0,
    ITEM_USE_OK          = 1,
    ITEM_USE_CANNOT_USE  = 2,
    ITEM_USE_CAUGHT      = 3,
    ITEM_USE_FLED        = 4,
} item_use_result_t;

item_use_result_t Battle_UseItem(uint8_t item_id, uint8_t target_slot);

#define MEDICINE_MSG_ANTIDOTE     0xF0
#define MEDICINE_MSG_BURN_HEAL    0xF1
#define MEDICINE_MSG_ICE_HEAL     0xF2
#define MEDICINE_MSG_AWAKENING    0xF3
#define MEDICINE_MSG_PARLYZ_HEAL  0xF4
#define MEDICINE_MSG_POTION       0xF5
#define MEDICINE_MSG_FULL_HEAL    0xF6
#define MEDICINE_MSG_REVIVE       0xF7

uint8_t  Battle_GetMedicineMsg(void);
uint16_t Battle_GetMedicineOldHP(void);
uint16_t Battle_GetMedicineNewHP(void);
