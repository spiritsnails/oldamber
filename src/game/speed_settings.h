#pragma once

#define SPEED_UNCAPPED 0
#define SPEED_NORMAL  1
#define SPEED_MAX     4

int  SpeedSettings_MoveAnim(void);
void SpeedSettings_SetMoveAnim(int steps_per_frame);

int  SpeedSettings_MiscAnim(void);
void SpeedSettings_SetMiscAnim(int steps_per_frame);

int  SpeedSettings_Transition(void);
void SpeedSettings_SetTransition(int steps_per_frame);

int  SpeedSettings_Battle(void);
void SpeedSettings_SetBattle(int steps_per_frame);
int  SpeedSettings_BattleIsCustom(void);

int  SpeedSettings_HpBar(void);
void SpeedSettings_SetHpBar(int steps_per_frame);

int  SpeedSettings_Text(void);
void SpeedSettings_SetText(int steps_per_frame);

int  SpeedSettings_RomTextScroll(void);
void SpeedSettings_SetRomTextScroll(int on);

int  SpeedSettings_Overworld(void);
void SpeedSettings_SetOverworld(int multiple);

int  SpeedSettings_OverworldFramePct(void);
