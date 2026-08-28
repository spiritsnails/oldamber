
#include "mon_pic.h"
#include "../data/pokemon_sprites.h"
#include "crystal_mon_pics.h"

#define GEN1_DEX_MAX 151

static const uint8_t kBlankTile[16] = {0};

static int is_gen1(int dex) { return dex >= 1 && dex <= GEN1_DEX_MAX; }
static int is_gen2(int dex) { return dex > GEN1_DEX_MAX && dex < CRYSTAL_MON_COUNT; }

int MonPic_Exists(int dex) { return is_gen1(dex) || is_gen2(dex); }

const uint8_t *MonPic_FrontTile(int dex, int slot) {
    if (slot < 0 || slot >= POKEMON_FRONT_CANVAS_TILES) return kBlankTile;
    if (is_gen1(dex)) return gPokemonFrontSprite[dex][slot];

    if (is_gen2(dex))
        return gCrystalPicTiles[gCrystalMonPic[dex].tile_base + slot];
    return kBlankTile;
}

int MonPic_CrystalExists(int dex) {
    return dex >= 1 && dex < CRYSTAL_MON_COUNT;
}

const uint8_t *MonPic_CrystalFrontTile(int dex, int slot) {
    if (slot < 0 || slot >= POKEMON_FRONT_CANVAS_TILES) return kBlankTile;
    if (!MonPic_CrystalExists(dex)) return kBlankTile;
    return gCrystalPicTiles[gCrystalMonPic[dex].tile_base + slot];
}

int MonPic_CrystalPalette(int dex, uint16_t out[4]) {
    if (!MonPic_CrystalExists(dex) || !out) return 0;
    out[0] = 0x7FFF;
    out[1] = gCrystalMonPalette[dex][0];
    out[2] = gCrystalMonPalette[dex][1];
    out[3] = 0x0000;
    return 1;
}

const uint8_t *MonPic_CrystalAnimTile(int dex, int slot, const uint8_t *framemap) {
    if (slot < 0 || slot >= POKEMON_FRONT_CANVAS_TILES) return kBlankTile;
    if (!MonPic_CrystalExists(dex)) return kBlankTile;
    int off = framemap ? framemap[slot] : slot;
    return gCrystalPicTiles[gCrystalMonPic[dex].tile_base + off];
}

const uint8_t *MonPic_BackTile(int dex, int slot) {
    if (slot < 0 || slot >= POKEMON_BACK_TILES) return kBlankTile;
    if (is_gen1(dex)) return gPokemonBackSprite[dex][slot];
    if (is_gen2(dex)) return gCrystalMonBackPic[dex][slot];
    return kBlankTile;
}

int MonPic_FrontW(int dex) {
    if (is_gen1(dex)) return gPokemonFrontSpriteW[dex];
    if (is_gen2(dex)) return gCrystalMonPic[dex].dim;
    return 0;
}

int MonPic_FrontH(int dex) {
    if (is_gen1(dex)) return gPokemonFrontSpriteH[dex];
    if (is_gen2(dex)) return gCrystalMonPic[dex].dim;
    return 0;
}

const uint16_t *MonPic_Palette(int dex) {

    if (dex >= 1 && dex < CRYSTAL_MON_COUNT) return gCrystalMonPalette[dex];
    return 0;
}
