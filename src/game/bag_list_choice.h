#pragma once
#include <stdint.h>

void    BagListChoice_Open(const uint8_t *candidates, int count, int interior_w);

int     BagListChoice_IsOpen(void);
void    BagListChoice_Tick(void);

int     BagListChoice_IsHeld(void);
void    BagListChoice_Refresh(void);
void    BagListChoice_ClearHeld(void);

uint8_t BagListChoice_GetResult(void);
