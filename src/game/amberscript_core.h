#pragma once

#include <stdint.h>
#include <stddef.h>

#define PKS_BTN_A      0x01
#define PKS_BTN_B      0x02
#define PKS_BTN_SELECT 0x04
#define PKS_BTN_START  0x08
#define PKS_BTN_RIGHT  0x10
#define PKS_BTN_LEFT   0x20
#define PKS_BTN_UP     0x40
#define PKS_BTN_DOWN   0x80

void AmberScript_SetEnabled(int enabled);
int  AmberScript_IsEnabled(void);

int  AmberScript_Dispatch(const char *cmd);

void AmberScript_Tick(void);

void AmberScript_SeqClear(void);
void AmberScript_SeqPush(uint8_t btn, int press_frames, int gap_frames);
int  AmberScript_SeqPending(void);

void AmberScript_SeqBattleMenu(int pos);

void AmberScript_SeqMoveSelect(int n);

void AmberScript_RequestDeferredWrite(int wait_frames);

void AmberScript_WriteState(void);

int AmberScript_ParseArg(const char *src, int arg_index, char *out, size_t out_sz);
