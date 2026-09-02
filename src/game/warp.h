#pragma once

#include <stdint.h>

int Warp_Check(void);

void Warp_SetForced(int on);
int  Warp_IsForced(void);

int Warp_CheckAtMapBoundary(void);

int Warp_CheckDungeonHole(void);
void Warp_QueueTeleportPadVmap(const char *vmap_name, int tile_x, int tile_y);

int Warp_CheckCollision(void);

void Warp_Execute(void);

void Warp_Reset(void);

const char *Warp_GetLastFromMapName(void);

int Warp_JustHappened(void);

int Warp_IsPending(void);

int Warp_IsPendingDungeonHole(void);
int Warp_IsPendingTeleportPad(void);

int Warp_HasDoorStep(void);

int Warp_ConsumeDoorStepFaceUp(void);

int Warp_IsDoorTile(uint8_t tile_id);

int Warp_HasEventAt(int x, int y);

void Warp_ForceTeleport(uint8_t map_id, int tile_x, int tile_y);

void Warp_QueueTeleport(uint8_t map_id, int tile_x, int tile_y);

void Warp_QueueTeleportVmap(const char *vmap_name, int tile_x, int tile_y);

void Warp_PlayMapChangeSound(void);

void Warp_SetSilphElevatorDestination(uint8_t map_id, int tile_x, int tile_y);

void Warp_SetVmapElevatorDestination(const char *vmap_name, int tile_x, int tile_y);
