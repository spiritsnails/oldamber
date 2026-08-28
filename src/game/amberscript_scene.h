#pragma once
#include <stdint.h>

int AmberScript_Scene_TryHandle(const char *cmd, const char *verb, int n);

void AmberScript_Scene_Tick(void);

int AmberScript_SceneWantsFullRate(void);

void AmberScript_SetMarchDebug(int on);
int  AmberScript_GetMarchDebug(void);

int AmberScript_GetMarchActorLabelForNpcIdx(int npc_idx, char *out_ch);

int AmberScript_Scene_IsActive(void);

int AmberScript_SceneDisasm(const char *path);

int AmberScript_OnNpcInteracted(int npc_idx);

void AmberScript_Scene_OnBattleOutcome(int outcome);

int AmberScript_MapAddSceneTriggerWatch(const char *map_name, const char *scene,
                                       const char *cond_kind_name, const char *cond_event_name,
                                       const char *cond_kind2_name, const char *cond_event2_name);

int pks_resolve_trainer_class_id(const char *tok);
uint8_t pks_trainer_class_to_overworld_sprite(int trainer_class);
int pks_parse_sprite(const char *tok);

int AmberScript_MapAddSceneTrigger(const char *map_name, int x, int y, const char *scene,
                                   const char *cond_kind_name, const char *cond_event_name,
                                   const char *cond_kind2_name, const char *cond_event2_name);

void AmberScript_SceneTriggerSuppressAt(int map_id, int x, int y);

int AmberScript_MapAddSceneTriggerOnLoad(const char *map_name, const char *scene,
                                         const char *cond_kind_name, const char *cond_event_name,
                                         const char *cond_kind2_name, const char *cond_event2_name);

void AmberScript_Scene_NotifyMapLoaded(void);

void AmberScript_Scene_NotifyBattleEnded(void);

void AmberScript_Scene_Abort(void);

int AmberScript_MapAddSceneNpc(const char *map_name, const char *scene, int x, int y);

int AmberScript_MapAddZoneLatch(const char *map_name, const char *event_name,
                               const int *xs, const int *ys, int ntiles);

int AmberScript_MapAddSceneTile(const char *map_name, const char *scene, int x, int y);
int AmberScript_OnTileInteracted(int fx, int fy);

void AmberScript_Scene_ClearNpcBindingsForMap(uint8_t map_id);

void AmberScript_Scene_ClearTriggersForMap(uint8_t map_id);

void AmberScript_Scene_ClearAllMapBindings(void);

int AmberScript_SceneShowingDex(void);

int AmberScript_SceneShowingBillsDexList(void);

int AmberScript_SceneShowingBadgeHouseMenu(void);

int AmberScript_SceneShowingDiploma(void);
