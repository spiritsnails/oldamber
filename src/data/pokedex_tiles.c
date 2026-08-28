
#include "pokedex_tiles.h"
#include "assetpack_bind.h"
#include "../platform/display.h"
#include <stdint.h>

void PokedexTiles_Load(void) {
    for (int i = 0; i < 18; i++) {
        Display_LoadTile((uint8_t)(0x60 + i), gPokedexTiles[i]);
    }
    Display_LoadTile(0x72, gPokedexOwnedBallTile);
}
