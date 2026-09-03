#pragma once

union SDL_Event;

void SuspendMenu_Open(void);
void SuspendMenu_Close(void);
void SuspendMenu_Toggle(void);
int  SuspendMenu_IsOpen(void);
void SuspendMenu_SetDebugToolingEnabled(int enabled);
int  SuspendMenu_DebugToolingEnabled(void);

void SuspendMenu_HandleEvent(const union SDL_Event *ev);

void SuspendMenu_Tick(void);

int  SuspendMenu_IsCapturing(void);

int  SuspendMenu_ExitToLauncherRequested(void);
