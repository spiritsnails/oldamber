#pragma once

#include <stdint.h>

int MonPic_Exists(int dex);

const uint8_t *MonPic_FrontTile(int dex, int slot);

int MonPic_CrystalExists(int dex);
const uint8_t *MonPic_CrystalFrontTile(int dex, int slot);

int MonPic_CrystalPalette(int dex, uint16_t out[4]);

const uint8_t *MonPic_CrystalAnimTile(int dex, int slot, const uint8_t *framemap);

const uint8_t *MonPic_BackTile(int dex, int slot);

int MonPic_FrontW(int dex);
int MonPic_FrontH(int dex);

const uint16_t *MonPic_Palette(int dex);
