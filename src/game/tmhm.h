#pragma once
#include <stdint.h>

void TMHM_Use(uint8_t item_id);

void TMHM_BeginLevelUpLearn(int party_slot, uint8_t move_id);

int  TMHM_IsActive(void);

int  TMHM_CanLearnActive(int party_slot);

int  TMHM_ShowingMenuBackdrop(void);

void TMHM_Tick(void);

void TMHM_PostRender(void);
