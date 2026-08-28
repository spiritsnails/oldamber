#pragma once
#include <stdint.h>

void Input_Init(void);
void Input_Update(void);

uint8_t Input_RawHeld(void);

int Input_PadMenuPressed(void);
void Input_Quit(void);

#define INPUT_BIND_COUNT 8

const char *Input_BindName(int bit);

int  Input_GetKey(int bit);
int  Input_GetPad(int bit);

void Input_SetKey(int bit, int scancode);
void Input_SetPad(int bit, int button);

const char *Input_KeyLabel(int scancode);
const char *Input_PadLabel(int button);

void Input_ResetBindings(void);

int  Input_PadAnyButton(void);
int  Input_PadAttached(void);

#define INPUT_RESET_HOLD_FRAMES 120
int  Input_EmergencyResetFired(void);

void Input_StartRecording(const char *path);
void Input_StopRecording(void);
void Input_StartPlayback(const char *path);
void Input_StopPlayback(void);
int  Input_IsRecording(void);
int  Input_IsPlaying(void);
void Input_SetGameplayInputBlocked(int blocked);
