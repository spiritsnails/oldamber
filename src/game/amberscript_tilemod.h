#pragma once

#include <stdint.h>
#include <stddef.h>

int AmberScript_TileMod_TryHandle(const char *cmd, const char *verb, int n);

int  AmberScript_TileCopy(int sx, int sy, int dx, int dy);
int  AmberScript_TileSaveRightOfPlayer(const char *name);
int  AmberScript_TilePlaceCustom(const char *name, int x, int y);
int  AmberScript_BlockSave(const char *name, int sx, int sy, int ex, int ey);
int  AmberScript_BlockPlaceCustom(const char *name, int x, int y);
int  AmberScript_SavedBlockFind(const char *name);
int  AmberScript_SavedBlockCellCount(int slot);

int  AmberScript_PlaceSwapBlock(const char *prefix, const char *state,
                                int bx, int by);

int  AmberScript_ParseCoordExpr(const char *tok, int is_x, int *out);

int  AmberScript_ParseCoordExprOrAny(const char *tok, int is_x, int *out);
void AmberScript_NormalizeCoordArgs(const char *src, char *dst, size_t dst_sz);
int  AmberScript_ParseBlockSaveArgs(const char *args, char *name, size_t name_sz,
                                    int *sx, int *sy, int *ex, int *ey);
int  AmberScript_ParseNamedCoordArgs(const char *args, char *name, size_t name_sz,
                                     int *x, int *y);

int  AmberScript_GetTileOverrideAt(int tx, int ty, uint8_t *tile_id);
int  AmberScript_GetWarpOverrideAt(int x, int y, uint8_t *has_warp, uint8_t *dest_map, uint8_t *dest_warp_idx);

int  AmberScript_GetWarpOverrideDestNameAt(int x, int y, char *out_name, size_t out_cap);

int  AmberScript_TileSetSign(int x, int y, const char *text);

const char *AmberScript_GetSignTextAt(int x, int y);

int  AmberScript_TileSetConditional(int x, int y, const char *event_name, const char *alt_block_name);

int  AmberScript_TileSetConditionalEx(int x, int y, const char *event_name,
                                      const char *alt_block_name, int negate);

void AmberScript_ClearTileOverrides(uint8_t map_id);

void AmberScript_TickTileAnimations(void);

void AmberScript_SetSubtilePixels(const char *name, const uint8_t pixels[16]);
void AmberScript_DebugTilePropCount(uint8_t map_id, int *counter_value, int *real_count, int *total_used);
extern long g_subtile_cache_hits;
extern long g_subtile_cache_misses;

int  AmberScript_GetTileOverrideAtForMap(uint8_t map_id, int tx, int ty, uint8_t *tile_id);

int  AmberScript_ResolveNamedBlock(const char *name, uint8_t tiles[4], uint8_t *passable_out);

int  AmberScript_GetPassableOverrideAt(int tx, int ty, uint8_t *passable);

void AmberScript_DebugDumpTilePropAt(int x, int y, char *out, size_t out_sz);

int  AmberScript_GetPassableOverrideAtForMap(uint8_t map_id, int tx, int ty, uint8_t *passable);

int  AmberScript_GetSurfableOverrideAt(int tx, int ty, uint8_t *surfable);

int  AmberScript_GetSurfableOverrideAtForMap(uint8_t map_id, int tx, int ty, uint8_t *surfable);
int  AmberScript_TileSetSurfable(const char *name, int surfable);
int  AmberScript_TileClearSurfable(const char *name);

int  AmberScript_GetCuttableOverrideAt(int tx, int ty, uint8_t *cuttable);
int  AmberScript_GetCutReplacementAt(int tx, int ty, char *out_name, size_t out_cap);
int  AmberScript_TileSetCuttable(const char *name, int cuttable);
int  AmberScript_TileClearCuttable(const char *name);
int  AmberScript_TileSetCutReplacement(const char *name, const char *replacement_name);

int  AmberScript_TileSetCutSpanBlock(const char *name, int block_wide);
int  AmberScript_GetCutSpanBlockAt(int tx, int ty);

int  AmberScript_GetCounterOverrideAt(int tx, int ty, uint8_t *counter);
int  AmberScript_TileSetCounter(const char *name, int counter);
int  AmberScript_TileClearCounter(const char *name);

int  AmberScript_GetGrassOverrideAt(int tx, int ty, uint8_t *grass);
int  AmberScript_TileSetGrass(const char *name, int grass);
int  AmberScript_TileClearGrass(const char *name);

int  AmberScript_GetGrassRustleOverrideAt(int tx, int ty, uint8_t *rustle);
int  AmberScript_TileSetGrassRustle(const char *name, int rustle);
int  AmberScript_TileClearGrassRustle(const char *name);

int  AmberScript_IsPairBlockedAt(int from_tx, int from_ty, int to_tx, int to_ty);
int  AmberScript_TileSetPairBlockGroup(const char *name, const char *group_name);

#define PKS_FACE_DOWN  (1 << 0)
#define PKS_FACE_UP    (1 << 1)
#define PKS_FACE_LEFT  (1 << 2)
#define PKS_FACE_RIGHT (1 << 3)
int  AmberScript_GetLedgeOverrideAt(int tx, int ty, int *ledge_dirs);
int  AmberScript_TileSetLedgeAt(int x, int y, int ledge_dirs);

int AmberScript_LoadCustomTileArt(const char *name, const char *png_path);

int AmberScript_TileSetPassable(const char *name, int passable);
int AmberScript_TileSetWarp(const char *name, int dest_map, int dest_warp_idx);

int AmberScript_TileSetWarpNamed(const char *name, const char *dest_vmap_name, int dest_warp_idx);

int AmberScript_TileSetWarpLast(const char *name, int dest_warp_idx);
int AmberScript_TileClearWarp(const char *name);

int AmberScript_TileClearPassable(const char *name);

int AmberScript_DefineOriginalTile(const char *name, uint8_t tile_id);

int AmberScript_LoadSubtileArt(const char *name, const char *png_path);

int AmberScript_SubtileSetPalette(const char *name, int pal);

int AmberScript_SubtileBlitPixels(const char *name, const uint8_t *pixels);
int AmberScript_SubtileReadPixels(const char *name, uint8_t *out);
int AmberScript_SubtileTilesetAdd(const char *tileset_name, const char *subtile_name);
int AmberScript_DefineQuadTile(const char *name, const char *sub_tl, const char *sub_tr,
                               const char *sub_bl, const char *sub_br);

int AmberScript_BlockDefLoad(const char *path);

int AmberScript_TilesetAdd(const char *tileset_name, const char *asset_name);
int AmberScript_TilesetApply(const char *tileset_name);
int AmberScript_TilesetClear(const char *tileset_name);

void AmberScript_TilesetUnbindMap(uint8_t map_id);

void AmberScript_TileMod_ReapplyCurrentMapNow(void);

void AmberScript_TileMod_ForceReapplyGfx(void);

void AmberScript_TileMod_InvalidateSubtileCache(void);

void AmberScript_TileMod_ResetAllMapBindings(void);

void AmberScript_TileMod_Tick(void);

void AmberScript_TileMod_PrewarmNeighbors(void);

void AmberScript_TileMod_PreloadIndoorSubtiles(void);

void AmberScript_TileMod_BumpCacheGeneration(void);

int AmberScript_TilePlaceRawTile(uint8_t tile_id, int x, int y);
int AmberScript_MapExport(const char *name);
int AmberScript_MapEditsApply(void);
