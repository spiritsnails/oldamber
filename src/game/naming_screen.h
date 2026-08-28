#pragma once
#include <stdint.h>

enum {
    NAME_PLAYER_SCREEN = 0,
    NAME_RIVAL_SCREEN  = 1,
    NAME_MON_SCREEN    = 2,
};

void NamingScreen_Open(uint8_t type, uint8_t species, uint8_t *name_buf);
int  NamingScreen_IsOpen(void);
void NamingScreen_Tick(void);
