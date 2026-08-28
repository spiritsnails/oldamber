#pragma once

#include <stddef.h>
#include <stdint.h>

#define TEXT_BOX_ROW  12

#define TEXT_ASCII_CONT  0x0B

typedef char text_ascii_cont_is_vtab[(TEXT_ASCII_CONT == '\v') ? 1 : -1];

void Text_ShowBox(const uint8_t *str);
void Text_ShowASCII(const char *str);

void Text_InstantNext(void);
int  Text_IsOpen(void);
void Text_Update(void);
void Text_Close(void);

void Text_KeepTilesOnClose(void);

void Text_CancelKeepTilesOnClose(void);

void Text_OverwriteTopLine(const char *s);

int Text_SnapshotBox(char *out, size_t outsz);

void Text_BlitBoxToBGAndHideWindow(void);

void Text_SuppressCursorNext(void);

void Text_DrawEmptyBox(void);

int  Text_GetCurrentStr(char *buf, int size);

extern int wDoNotWaitForButtonPress;

extern int gTextLetterDelay;

void Text_SetPendingSFX(void (*fn)(void));

void Text_SetPendingSFXOnPrint(void (*fn)(void));

void Text_SetCloseAfterPrintSFX(void);

void Text_HoldAfterPrompt(void);

int  Text_IsHeldAfterPrompt(void);

void Text_SetItemName(uint8_t item_id);

void Text_SetMonName(uint8_t species);

void Text_SetBoxNumber(int box);

void Text_SetEndsWithPrompt(void);

void Text_SetNameBufferItem(uint8_t item_id);
void Text_SetNameBufferMon(uint8_t species);
void Text_SetNameBufferString(const char *s);
