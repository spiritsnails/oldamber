
#include "anim.h"
#include "assetpack_bind.h"
#include "../platform/display.h"
#include <stdint.h>

#define ANIM_WATER_TILE   0x14
#define ANIM_FLOWER_TILE  0x03

static uint8_t hTileAnimations        = 0;
static uint8_t hMovingBGTilesCounter1 = 0;
static uint8_t wMovingBGTilesCounter2 = 0;

void Anim_SetTileset(uint8_t anim_type) {
    hTileAnimations        = anim_type;
    hMovingBGTilesCounter1 = 0;

}

static inline uint8_t rrca8(uint8_t v) { return (v >> 1) | (uint8_t)(v << 7); }

static inline uint8_t rlca8(uint8_t v) { return (v << 1) | (v >> 7); }

void Anim_UpdateTiles(void) {
    if (!hTileAnimations) return;

    hMovingBGTilesCounter1++;
    if (hMovingBGTilesCounter1 < 20) return;

    if (hMovingBGTilesCounter1 == 21) {
        hMovingBGTilesCounter1 = 0;
        uint8_t v = wMovingBGTilesCounter2 & 3;
        int f = (v < 2) ? 0 : (v == 2) ? 1 : 2;
        Display_LoadTile(ANIM_FLOWER_TILE, kFlowerFrames[f]);
        return;
    }

    wMovingBGTilesCounter2 = (wMovingBGTilesCounter2 + 1) & 7;

    uint8_t buf[16];
    Display_GetTile(ANIM_WATER_TILE, buf);
    if (wMovingBGTilesCounter2 & 4) {
        for (int i = 0; i < 16; i++) buf[i] = rlca8(buf[i]);
    } else {
        for (int i = 0; i < 16; i++) buf[i] = rrca8(buf[i]);
    }
    Display_LoadTile(ANIM_WATER_TILE, buf);

    if (hTileAnimations == TILEANIM_WATER) {
        hMovingBGTilesCounter1 = 0;
    }
}
