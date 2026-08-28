#pragma once

void Menu_SetGen2Account(int on);
int  Menu_Gen2Account(void);

void Menu_Open(void);
void Menu_Tick(void);

void Menu_OpenOptionsStandalone(void);
int  Menu_TickOptionsStandalone(void);
int  Menu_IsOpen(void);

int  Menu_SaveRowIndex(void);
int  Menu_CursorRow(void);
int  Menu_SaveFlowState(void);
int  Menu_SaveAwaitingYesNo(void);

void Menu_ResumeFromBag(void);

void Menu_DrawBackdropForBag(void);
