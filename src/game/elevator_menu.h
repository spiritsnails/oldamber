#pragma once
#include <stdint.h>

void ElevatorMenu_OpenRocketHideout(void);
void ElevatorMenu_QueueOpenRocketHideout(void);
void ElevatorMenu_QueueOpenSilphCo(void);
void ElevatorMenu_QueueOpenVending(void);

void ElevatorMenu_QueueOpenCeladonMart(void);

void CeladonMartElevator_PanelInteract(void);

void SilphCoElevator_PanelInteract(void);
void ElevatorMenu_TryOpenQueued(void);
void ElevatorMenu_Tick(void);
int  ElevatorMenu_IsOpen(void);
int  ElevatorMenu_IsBusy(void);

int  ElevatorMenu_ConsumeTeleport(uint8_t *map_out, int *x_out, int *y_out);
