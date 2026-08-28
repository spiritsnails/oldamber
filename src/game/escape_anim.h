#pragma once
#include <stdint.h>

void EscapeAnim_Start(uint8_t dest_map, int dest_x, int dest_y);
int  EscapeAnim_IsActive(void);
void EscapeAnim_Tick(void);

void EscapeAnim_StartToLastHealTown(void);

void EscapeAnim_StartToLastHealTownAfter(int predelay);

int  EscapeAnim_CanEscapeHere(void);
int  EscapeAnim_IsOutsideMap(void);
