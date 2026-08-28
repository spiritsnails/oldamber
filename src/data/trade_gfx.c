
#include "trade_gfx.h"
#include "../platform/display.h"

void TradeGfx_LoadTiles(void) {
    for (int i = 0; i < TRADE_GAME_BOY_TILES; i++)
        Display_LoadTile((uint8_t)(TRADE_GAME_BOY_TILE_BASE + i), gTradeGameBoyTiles[i]);
    for (int i = 0; i < TRADE_LINK_CABLE_TILES; i++)
        Display_LoadTile((uint8_t)(TRADE_LINK_CABLE_TILE_BASE + i), gTradeLinkCableTiles[i]);
    for (int i = 0; i < TRADE_CABLE_BALL_TILES; i++)
        Display_LoadSpriteTile((uint8_t)(TRADE_CABLE_BALL_TILE_BASE + i), gTradeCableBallTiles[i]);
    Display_LoadTile(TRADE_GLYPH_ID_TILE, gTradeGlyphTiles[0]);
    Display_LoadTile(TRADE_GLYPH_NO_TILE, gTradeGlyphTiles[1]);
}
