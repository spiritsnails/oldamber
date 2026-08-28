#pragma once
#include <stdint.h>

#define PRIZE_KIND_MON  0
#define PRIZE_KIND_ITEM 1

void PrizeListChoice_Open(int kind, const uint8_t entries[3], const uint16_t prices[3]);
int  PrizeListChoice_IsOpen(void);
void PrizeListChoice_Tick(void);

int  PrizeListChoice_GetResult(void);
