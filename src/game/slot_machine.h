#pragma once

#include <stdint.h>

#define SLOTS_MACHINE_OK           0
#define SLOTS_MACHINE_OUTOFORDER   0xFD
#define SLOTS_MACHINE_OUTTOLUNCH   0xFE
#define SLOTS_MACHINE_SOMEONESKEYS 0xFF

void SlotMachine_SelectLucky(void);

void SlotMachine_Start(int machine_index, int kind);

int  SlotMachine_IsOpen(void);
void SlotMachine_Tick(void);

void SlotMachine_Reset(void);
