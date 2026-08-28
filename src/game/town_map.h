#pragma once

#include <stdint.h>

void TownMap_Open(void);
void TownMap_OpenFly(void);
int  TownMap_IsOpen(void);
void TownMap_Tick(void);
void TownMap_MarkVisited(uint8_t map_id);

int  TownMap_GetFlyDest(uint8_t map_id, int *x, int *y);

void TownMap_GetVisited(uint8_t out[2]);
void TownMap_SetVisited(const uint8_t in[2]);
