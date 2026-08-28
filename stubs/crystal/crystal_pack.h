
#pragma once
#include <stdint.h>

typedef struct {
    const char *name;
    uint8_t flags, x1, y1, x2, y2, def_selection;
    uint8_t data_flags, rows, cols, item_format;
} crystal_pocket_t;

#define CRYSTAL_NUM_POCKETS 3
#define CRYSTAL_ITEM_DESC_COUNT 255

extern const crystal_pocket_t gCrystalPockets[CRYSTAL_NUM_POCKETS];

extern const char *const gCrystalItemDesc[CRYSTAL_ITEM_DESC_COUNT];

extern const char *const gCrystalItemName[CRYSTAL_ITEM_DESC_COUNT];

#define CRYSTAL_PACK_IMG_TILES 15
#define CRYSTAL_PACK_IMG_W 5
#define CRYSTAL_PACK_IMG_H 3

extern const uint8_t gCrystalPackGFX[4][CRYSTAL_PACK_IMG_TILES][16];
extern const uint8_t gCrystalPackGFXOrder[4];

#define CRYSTAL_PACK_MENU_TILES 0x60
#define CRYSTAL_PACK_BG_TILE    0x24
#define CRYSTAL_PACK_HDR_TILE   0x28
#define CRYSTAL_PACK_NAME_ROW   7
#define CRYSTAL_PACK_LIST_COL   5

extern const uint8_t gCrystalPackMenuGFX[CRYSTAL_PACK_MENU_TILES][16];

extern const uint8_t gCrystalPocketNameTilemap[4][15];

extern const uint16_t gCrystalPackPals[8][4];
