#pragma once

#include <stdint.h>

#include "assetpack_bind.h"

#define TRADE_GAME_BOY_TILE_BASE   0x31
#define TRADE_LINK_CABLE_TILE_BASE 0x53
#define TRADE_CABLE_BALL_TILE_BASE 0x7C

#define TRADE_GAME_BOY_TILES   34
#define TRADE_LINK_CABLE_TILES 15
#define TRADE_CABLE_BALL_TILES 4

#define TRADE_GLYPH_ID_TILE 98
#define TRADE_GLYPH_NO_TILE 99

#define TRADE_GAME_BOY_MAP_W   6
#define TRADE_GAME_BOY_MAP_H   8
#define TRADE_LINK_CABLE_MAP_W 12
#define TRADE_LINK_CABLE_MAP_H 3

void TradeGfx_LoadTiles(void);
