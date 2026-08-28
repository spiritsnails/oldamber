#pragma once

#include <stdint.h>

void DebugCLI_Tick(void);
void DebugCLI_PostRender(void);

void DebugCLI_ConsoleOpen(void);
void DebugCLI_ConsoleClose(void);
int  DebugCLI_ConsoleIsOpen(void);
void DebugCLI_ConsoleAddChar(char c);
void DebugCLI_ConsoleBackspace(void);
void DebugCLI_ConsoleExecute(void);
void DebugCLI_ConsoleRender(void);
void DebugCLI_ConsoleSetOverlayEnabled(int enabled);
void DebugCLI_ConsoleSetAlwaysOpen(int enabled);
const char *DebugCLI_ConsoleGetBuffer(void);

int DebugCLI_GetHistoryCount(void);
const char *DebugCLI_GetHistoryLine(int newest_index);
int DebugCLI_GetHistoryColor(int newest_index);
int DebugCLI_IsReplayPlaying(void);
void DebugCLI_HistoryPushExternal(const char *line);
int DebugCLI_TriggerNpcWalkoff(void);
int DebugCLI_IsNpcWalkoffActive(void);
int DebugCLI_OnNpcInteracted(int npc_idx);

void DebugCLI_ClearSceneNpcBindingsForMap(uint8_t map_id);
int DebugCLI_IsAutoWinEnabled(void);

void DebugCLI_PollExternal(void);

void DebugCLI_PumpButtons(void);

int  DebugCLI_SceneIsActive(void);

void DebugCLI_WriteMarchAnimFrame(const char *header_line);

#define CLI_HIST_COLOR_DEFAULT 0
#define CLI_HIST_COLOR_OK      1
#define CLI_HIST_COLOR_ERROR   2
#define CLI_HIST_COLOR_LOG     3
