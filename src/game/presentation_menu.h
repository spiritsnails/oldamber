#pragma once

void PresentationMenu_Open(void);

void PresentationMenu_Close(void);

void PresentationMenu_Toggle(void);

int  PresentationMenu_IsOpen(void);

void PresentationMenu_Tick(void);

void PresentationMenu_PreloadRenderer(void);

void PresentationMenu_LoadSettings(void);

int         PresentationMenu_PageCount(void);
const char *PresentationMenu_PageName(int page);

int         PresentationMenu_PageRowCount(int page);
const char *PresentationMenu_RowLabel(int page, int i);

int         PresentationMenu_RowId(int page, int i);

const char *PresentationMenu_RowHeader(int pg, int i);

int         PresentationMenu_RowAvailable(int row_id);

int         PresentationMenu_ChoiceCount(int row_id);
const char *PresentationMenu_ChoiceLabel(int row_id, int index);
int         PresentationMenu_CurrentIndex(int row_id);

void        PresentationMenu_RefreshNow(void);

int         PresentationMenu_FastBoot(void);

void        PresentationMenu_SetIndex(int row_id, int index);

int         PresentationMenu_TakeWidescreenNotice(void);
void        PresentationMenu_DismissWidescreenNotice(int never_again);
