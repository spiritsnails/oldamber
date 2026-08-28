
#pragma once
#include <stdint.h>

#define CRYSTAL_TRAINER_PIC_TILES 49
#define CRYSTAL_TRAINER_PIC_COUNT 48

extern const uint8_t gCrystalTrainerPic[CRYSTAL_TRAINER_PIC_COUNT][CRYSTAL_TRAINER_PIC_TILES][16];

extern const uint8_t gCrystalTrainerPicValid[CRYSTAL_TRAINER_PIC_COUNT];
extern const uint8_t gCrystalTrainerPalValid[CRYSTAL_TRAINER_PIC_COUNT];

extern const uint16_t gCrystalTrainerPalette[CRYSTAL_TRAINER_PIC_COUNT][2];

#define CRYSTAL_TRAINER_CLASS_COUNT 68
extern const uint8_t gCrystalTrainerPicByClass
    [CRYSTAL_TRAINER_CLASS_COUNT][CRYSTAL_TRAINER_PIC_TILES][16];
extern const uint8_t gCrystalTrainerPicByClassValid[CRYSTAL_TRAINER_CLASS_COUNT];
extern const uint16_t gCrystalTrainerPaletteByClass[CRYSTAL_TRAINER_CLASS_COUNT][2];
