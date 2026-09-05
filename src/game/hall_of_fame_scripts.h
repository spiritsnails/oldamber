#pragma once

void HallOfFameScripts_OnMapLoad(void);
void HallOfFameScripts_Tick(void);
int  HallOfFameScripts_IsActive(void);
int  HallOfFameScripts_ShouldUpdateOverworld(void);

void HallOfFameScripts_OakInteract(void);

void HallOfFame_RecordParty(void);
void HallOfFameViewer_Open(void);
void HallOfFameViewer_Tick(void);
int  HallOfFameViewer_IsOpen(void);
