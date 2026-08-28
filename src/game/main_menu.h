#pragma once

#define MAIN_MENU_PENDING   0
#define MAIN_MENU_CONTINUE  1
#define MAIN_MENU_NEW_GAME  2
#define MAIN_MENU_BACK      3

#define MAIN_MENU_OPTION    4

void MainMenu_Open(int has_save);
int  MainMenu_IsOpen(void);
int  MainMenu_GetResult(void);
void MainMenu_Tick(void);
