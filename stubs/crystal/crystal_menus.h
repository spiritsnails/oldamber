
#pragma once
#include <stdint.h>

#define CRYSTAL_START_ITEMS 9

typedef struct {
    uint8_t flags, x1, y1, x2, y2, def_selection;
    uint8_t data_flags, rows, cols;
} crystal_menu_box_t;

typedef struct {
    const char *id;
    const char *label;
    const char *desc;
} crystal_menu_item_t;

extern const crystal_menu_box_t gCrystalStartMenuBox;
extern const crystal_menu_item_t gCrystalStartMenu[CRYSTAL_START_ITEMS];
