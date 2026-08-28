#pragma once

#include <stdint.h>

void FlyAnim_Start(uint8_t dest_map, int dest_x, int dest_y);
int FlyAnim_IsActive(void);
void FlyAnim_Tick(void);
