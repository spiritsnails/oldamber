
#pragma once
#include <stdint.h>

#define CRYSTAL_ICON_TILES 8
#define CRYSTAL_NUM_ICONS  39
#define CRYSTAL_ICON_SPECIES 251

extern const uint8_t gCrystalSpeciesIcon[CRYSTAL_ICON_SPECIES];

extern const uint8_t gCrystalIcon[CRYSTAL_NUM_ICONS][CRYSTAL_ICON_TILES][16];
#define CRYSTAL_ICON_OBPALS 8

extern const uint16_t gCrystalIconOBPals[CRYSTAL_ICON_OBPALS][4];

extern const uint16_t gCrystalPartyMenuBGPals[4][4];
