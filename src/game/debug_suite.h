#pragma once

#include <stdint.h>
#include <stddef.h>

void DebugSuite_Init(const uint8_t *rw_data, const int *rw_seq,
                     const int *rw_len, size_t state_size, int slots);
void DebugSuite_SetLabMode(int on);
int  DebugSuite_IsLab(void);
int  DebugSuite_IsTurbo(void);
int  DebugSuite_SpeedPct(void);
void DebugSuite_SetSpeedPct(int pct);

int  DebugSuite_TakeRewindDelta(void);
void DebugSuite_SetRewindPos(int pos, int len);

int  DebugSuite_TakeBreakpointRestore(char *buf, size_t n);
int  DebugSuite_TakeBreakpointCommit(void);
int  DebugSuite_QuitRequested(void);

int  DebugSuite_FrameGate(void);
void DebugSuite_PausedTick(void);
void DebugSuite_TopOfFrame(void);
void DebugSuite_InjectInput(void);
void DebugSuite_InjectLiveHold(void);
void DebugSuite_EndFrame(void);

void DebugSuite_RecordFrame(uint8_t joy_input, int slot);
int  DebugSuite_ReplayActive(void);

int  DebugSuite_CaptureReport(const char *message);

int  DebugSuite_TryCommand(const char *cmd);
