#pragma once
#include <stdint.h>

void SafariZoneScripts_OnMapLoad(void);
void SafariZoneScripts_StepCheck(void);
void SafariZoneScripts_Tick(void);
void SafariZoneScripts_PostRender(void);
int SafariZoneScripts_IsActive(void);
void SafariZoneScripts_GateStepCheck(void);
void SafariZoneScripts_DebugGetState(uint16_t *steps, uint8_t *balls, uint8_t *script_state);
void SafariZoneScripts_DebugSetState(uint16_t steps);

void SafariZoneScripts_Enter(void);
void SafariZoneScripts_Leave(void);

int SafariZoneScripts_MapHasWildEncounters(uint8_t map_id);

int SafariZoneScripts_MapShowsStepCounter(uint8_t map_id);
