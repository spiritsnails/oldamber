#pragma once
#include <stdint.h>

#define ITEM_NONE        0x00
#define ITEM_MASTER_BALL 0x01
#define ITEM_ULTRA_BALL  0x02
#define ITEM_GREAT_BALL  0x03
#define ITEM_POKE_BALL   0x04
#define ITEM_TOWN_MAP    0x05
#define ITEM_BICYCLE     0x06
#define ITEM_POKEDEX     0x09
#define ITEM_MOON_STONE  0x0A
#define ITEM_ANTIDOTE    0x0B
#define ITEM_BURN_HEAL   0x0C
#define ITEM_ICE_HEAL    0x0D
#define ITEM_AWAKENING   0x0E
#define ITEM_PARLYZ_HEAL 0x0F
#define ITEM_FULL_RESTORE 0x10
#define ITEM_MAX_POTION  0x11
#define ITEM_HYPER_POTION 0x12
#define ITEM_SUPER_POTION 0x13
#define ITEM_POTION      0x14
#define ITEM_ESCAPE_ROPE 0x1D
#define ITEM_REPEL       0x1E
#define ITEM_FIRE_STONE  0x20
#define ITEM_THUNDER_STONE 0x21
#define ITEM_WATER_STONE 0x22
#define ITEM_HP_UP       0x23
#define ITEM_PROTEIN     0x24
#define ITEM_IRON        0x25
#define ITEM_CARBOS      0x26
#define ITEM_CALCIUM     0x27
#define ITEM_RARE_CANDY   0x28
#define ITEM_LEAF_STONE  0x2F
#define ITEM_PP_UP       0x4F
#define ITEM_ETHER       0x50
#define ITEM_MAX_ETHER   0x51
#define ITEM_ELIXER      0x52
#define ITEM_MAX_ELIXER  0x53
#define ITEM_ITEMFINDER   0x47
#define ITEM_DOME_FOSSIL  0x29
#define ITEM_HELIX_FOSSIL 0x2A
#define ITEM_SECRET_KEY   0x2B
#define ITEM_POKE_DOLL   0x33
#define ITEM_FULL_HEAL   0x34
#define ITEM_REVIVE      0x35
#define ITEM_MAX_REVIVE  0x36
#define ITEM_SUPER_REPEL 0x38
#define ITEM_MAX_REPEL   0x39
#define ITEM_NUGGET      0x31
#define ITEM_SS_TICKET   0x3F
#define ITEM_OAKS_PARCEL 0x46
#define ITEM_POKE_FLUTE  0x49

#define ITEM_EXP_ALL     0x4B
#define ITEM_OLD_ROD     0x4C
#define ITEM_GOOD_ROD    0x4D
#define ITEM_SUPER_ROD   0x4E
#define HM01             0xC4
#define TM01             0xC9
#define TM_THUNDER_WAVE  0xF5

int Inventory_Add(uint8_t item_id, uint8_t qty);

int Inventory_Remove(uint8_t item_id, uint8_t qty);

int Inventory_AddTo(uint8_t *num, uint8_t *items, int cap, uint8_t item_id, uint8_t qty);
int Inventory_RemoveFrom(uint8_t *num, uint8_t *items, uint8_t item_id, uint8_t qty);

int Inventory_GetQty(uint8_t item_id);

int Inventory_IsKeyItem(uint8_t item_id);

int Inventory_IsHM(uint8_t item_id);

const uint8_t *Inventory_GetName(uint8_t item_id);

int Inventory_TmHmIdFromName(const char *name);

void Inventory_DecodeASCII(uint8_t item_id, char *buf, int buf_size);
