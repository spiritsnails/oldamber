#pragma once

#define TITLE_SCREEN_PENDING     0
#define TITLE_SCREEN_MAIN_MENU   1
#define TITLE_SCREEN_CLEAR_SAVE  2

void TitleScreen_Open(void);

void TitleScreen_OpenAtTitle(void);
int  TitleScreen_IsOpen(void);
int  TitleScreen_GetResult(void);
void TitleScreen_Tick(void);
