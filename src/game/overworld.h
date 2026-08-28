#pragma once
#include <stdint.h>
#include "../game/constants.h"

void    Map_Load(uint8_t map_id);

void    AmberScript_PlayMapMusic(uint8_t map_id);

void    Map_RefreshVirtualDims(void);

void    Map_RefreshVirtualAnim(void);

void    Map_ReloadGfx(void);
void    Map_BuildView(void);

void    Overworld_ArmCloseWhiteout(int frames);

int     Map_IsDarkMap(int real_id);

void    Map_ApplyDarknessForWarp(int prev_was_dark, uint8_t prev_offset, int dest_real_id);

extern int gOverworldCloseWhiteout;
void    Map_BuildScrollView(void);

void    Map_SuppressScrollRebuild(int suppress);

void    Map_HoldForBootScreen(int hold);
int     Map_IsHeldForBootScreen(void);
void    Map_UpdateCamera(void);
uint8_t Map_GetTile(int tx, int ty);

void    Map_SetBlock(int bx, int by, uint8_t block_id);

uint8_t Map_GetGameTile(int gx, int gy);

uint8_t Map_GetBlockAt(int gx, int gy);

void    Map_SetBlockAt(int gx, int gy, uint8_t block_id);
int     Tile_IsPassable(uint8_t tile_id);

int     Map_IsTilePassableAt(int gx, int gy);

int     Map_GetSurfableOverrideAt(int gx, int gy, uint8_t *surfable);

int     Map_CurrentRealId(void);

int     Map_RealIdForName(const char *name);

int     Map_GetBlockIdRaw(int bx, int by);
int     Tile_IsPairBlocked(uint8_t a, uint8_t b);
int     Connection_Check(int dx, int dy);
void    Map_PreBuildScrollStep(int dx, int dy);
void    Map_ResetScrollState(void);

#define SCREEN_WIDTH_MAX 32
#define SCROLL_MAP_W  (SCREEN_WIDTH_MAX + 4)
#define SCROLL_MAP_H  (SCREEN_HEIGHT + 4)
extern uint8_t gScrollTileMap[SCROLL_MAP_W * SCROLL_MAP_H];

int Map_ViewTilesW(void);
int Map_ScrollCols(void);

int Map_CamHalfX(void);

int Map_UiColOfs(void);

int Map_UiColOfsRight(void);

extern int gCamX;
extern int gCamY;
