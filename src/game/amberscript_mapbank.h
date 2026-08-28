#pragma once

#ifndef PKS_MAX_TEXT
#define PKS_MAX_TEXT 512
#endif

#include <stdint.h>
#include <stddef.h>
#include "../data/event_data.h"
#include "../data/wild_data.h"

int AmberScript_MapWarp(const char *name, int x, int y);

int AmberScript_MapSave(const char *name);

void AmberScript_MapList(void);

const char *AmberScript_MapBank_NameForRealId(int real_id);
int AmberScript_MapBank_HasStreamedForRealId(int real_id);

unsigned AmberScript_MapBank_StreamGeneration(void);

int AmberScript_MapSetDims(const char *name, int width_blocks, int height_blocks);

int AmberScript_MapBank_GetDimsForRealId(int real_id, int *width_blocks, int *height_blocks);

int AmberScript_MapSetMusic(const char *name, const char *track_name);

int AmberScript_MapBank_GetMusicForRealId(int real_id, char *out_track, size_t out_cap);

int AmberScript_MapAddNpc(const char *name, const char *sprite_or_class, int x, int y,
                          int movement, int facing, const char *text);

void AmberScript_MapNpcSaveRuntime(int real_id, int key_x, int key_y,
                                  int x, int y, int facing, int hidden, int has_pos);

int AmberScript_MapNpcResolveRuntime(int real_id, int key_x, int key_y,
                                    int *out_x, int *out_y);

#define PKS_TRAINER_DECL_IDX_BASE 64

#define PKS_STATIC_DECL_IDX_BASE 128

int AmberScript_MapAddStaticEncounter(const char *name, const char *species_str,
                                      int level, int x, int y,
                                      const char *event_name,
                                      const char *text, int cry,
                                      const char *sprite_str, int facing);
int AmberScript_GetStaticEncounterAt(uint8_t real_id, int x, int y, int *out_index);
int AmberScript_GetStaticEncounterInfo(uint8_t real_id, int index,
                                       int *out_species, int *out_level,
                                       uint16_t *out_flag, int *out_cry,
                                       const char **out_text);

void AmberScript_StaticEncounterInteract(void);

int AmberScript_MapFindLiveNpcByDeclaredTile(int real_id, int key_x, int key_y);
int AmberScript_MapAddTrainer(const char *name, const char *class_name, int trainer_no,
                              int x, int y, int dir, int sight_dist,
                              const char *before_text, const char *after_text,
                              const char *defeat_text, const char *flag_name,
                              const char *sprite_override);

#define PKS_JOHTO_PLACEHOLDER_CLASS 1
int AmberScript_MapAddJohtoTrainer(const char *name, const char *class_name, int trainer_no,
                                   int x, int y, int dir, int sight_dist,
                                   const char *before_text, const char *after_text,
                                   const char *defeat_text, const char *flag_name,
                                   const char *sprite_override);

int AmberScript_MapAddItemBall(const char *name, int x, int y, const char *item_name_or_id);

int AmberScript_MapAddHiddenItem(const char *name, int x, int y, const char *item_name_or_id);

int AmberScript_GetHiddenItemAt(uint8_t real_id, int x, int y, uint8_t *out_item, uint16_t *out_flag);

int AmberScript_MapAddHiddenCoin(const char *name, int x, int y, int amount);

int AmberScript_GetHiddenCoinAt(uint8_t real_id, int x, int y, uint16_t *out_amount, uint16_t *out_flag);

int AmberScript_MapAddSlotMachine(const char *name, int x, int y, int kind);

int AmberScript_GetSlotMachineAt(uint8_t real_id, int x, int y, int *out_index, int *out_kind);

int AmberScript_MapAddHiddenEvent(const char *name, int x, int y, const char *text, int facing);

int AmberScript_AddTextVariant(const char *event_name, const char *text);

int AmberScript_AddBadgeTextVariant(const char *badge_name, const char *text);

int AmberScript_AddTextRandom(int weight, const char *text);
void AmberScript_ResetLastDecl(void);
const char *AmberScript_ResolveNpcText(uint8_t map_id, int npc_idx);

const char *AmberScript_ResolveNpcTextByDecl(uint8_t map_id, int decl_idx);

int AmberScript_GetNpcDeclaredPos(uint8_t map_id, int decl_idx, int *out_x, int *out_y);
const char *AmberScript_ResolveHiddenEventText(uint8_t map_id, int hidden_idx);

int AmberScript_AddHideIf(const char *event_name);

int AmberScript_AddShowIf(const char *event_name);

int AmberScript_AddStartsHidden(void);

int AmberScript_AddNoFaceUntil(const char *event_name);

int AmberScript_NpcSuppressesFacePlayerByDecl(uint8_t map_id, int decl_idx);

int AmberScript_MapSetWildRate(const char *name, int is_water, int rate);
int AmberScript_MapSetWildSlot(const char *name, int is_water, int slot_no,
                               const char *species_name_or_id, int level);

const wild_mons_t *AmberScript_GetWildMonsFor(uint8_t map_id, int is_water);

int AmberScript_MapSetJohtoWildRate(const char *name, int is_water, int rate,
                                    int tod);
int AmberScript_MapSetJohtoWildSlot(const char *name, int is_water, int slot_no,
                                    const char *species_name_or_id, int level,
                                    int tod);
int AmberScript_HasJohtoWildTable(uint8_t map_id, int is_water);
int AmberScript_TryJohtoWildEncounter(uint8_t map_id, int is_water,
                                      uint8_t *out_species, uint8_t *out_level);

const map_events_t *AmberScript_GetMapEventsForCurrentMap(uint8_t real_id);

const map_events_t *AmberScript_GetMapEventsFor(uint8_t map_id);

const map_events_t *AmberScript_GetMapEventsForFreshLoad(uint8_t map_id);

uint16_t AmberScript_GetItemFlagBitAt(uint8_t real_id, int item_index);

void AmberScript_ScrubStaleFlagBits(void);

int AmberScript_IsTrainerBeatenAt(uint8_t real_id, int x, int y);

void AmberScript_MarkAllTrainersDefeated(const char *map_name);

int AmberScript_MapBank_EnsureResidentForRealId(uint8_t real_id);

int AmberScript_MapSetBorderSide(const char *name, int side, const char *tl, const char *tr,
                                 const char *bl, const char *br);
int AmberScript_MapSetBorder(const char *name, const char *tl, const char *tr,
                             const char *bl, const char *br);

int AmberScript_MapBank_GetBorderTileForRealId(int real_id, int tx, int ty,
                                               uint8_t *tile_id, uint8_t *passable_out);

int AmberScript_ConnectionDefine(const char *from, int direction, const char *to,
                                  int player_coord, int adjust);

int AmberScript_GetConnectionOverride(uint8_t cur_real_id, int direction,
                                       uint8_t *dest_real_id, int16_t *player_coord, int16_t *adjust);

int AmberScript_GetConnectionOverridePassive(uint8_t cur_real_id, int direction,
                                             uint8_t *dest_real_id, int16_t *player_coord, int16_t *adjust);

int AmberScript_MapSetWarpSpot(const char *name, int spot_idx, int x, int y);

int AmberScript_MapBank_GetWarpSpotForRealId(int real_id, int spot_idx, int *x, int *y);
int AmberScript_MapBank_GetWarpSpotForName(const char *name, int spot_idx, int *x, int *y);

int AmberScript_MapBank_RegisterName(const char *name);

int AmberScript_MapBank_EnsureResidentByName(const char *name);

int AmberScript_MapBank_GetOrAssignRealId(const char *name);

int AmberScript_MapSetIndoor(const char *name);

int AmberScript_MapBank_IsIndoorForRealId(int real_id);

int AmberScript_MapSetDark(const char *name);

int AmberScript_MapSetGbcTileset(const char *name, int tileset_id);

int AmberScript_MapBank_GetGbcTilesetForRealId(int real_id);

void AmberScript_SetPendingNpcPalette(int pal_plus_one);

int AmberScript_MapSetCrystalEnv(const char *name, int environment, int map_group);
int AmberScript_MapBank_GetCrystalEnvForRealId(int real_id, int *out_group);

int AmberScript_MapSetCrystalAnim(const char *name, int tileset_id, const char *slug);
int AmberScript_MapBank_GetCrystalAnimForRealId(int real_id, const char **out_slug);

int AmberScript_MapBank_GetGbcTilesetForName(const char *name);

int AmberScript_MapBank_IsDarkForRealId(int real_id);

void AmberScript_MapBank_TouchRealId(uint8_t real_id);

#define PKS_VMAP_BIND_NAME_LEN 32
void AmberScript_MapBank_SnapshotBindings(char out[][PKS_VMAP_BIND_NAME_LEN], int count);
void AmberScript_MapBank_RestoreBindings(const char in[][PKS_VMAP_BIND_NAME_LEN], int count);

void AmberScript_MapBank_ResetAll(void);

#define PKS_NPC_RT_BLOB_MAX 2048

int  AmberScript_MapBank_SerializeNpcRt(uint8_t *buf, int cap);
void AmberScript_MapBank_DeserializeNpcRt(const uint8_t *buf, int len);

int AmberScript_MapSetNoDoorStep(const char *name);

int AmberScript_MapBank_GetNoDoorStepForRealId(int real_id);

int AmberScript_MapSetWarpWalkInto(const char *name, int x, int y, int dir);
int AmberScript_MapBank_IsWarpWalkIntoAt(int real_id, int x, int y);

int AmberScript_MapBank_GetWarpWalkIntoDirAt(int real_id, int x, int y);

int AmberScript_MapSetWarpStair(const char *name, int x, int y);
int AmberScript_MapBank_IsWarpStairAt(int real_id, int x, int y);

int AmberScript_RunService(const char *name);

int AmberScript_AddAfterBattle(const char *scene);

int AmberScript_MapBank_TryHandle(const char *cmd, const char *verb, int n);
