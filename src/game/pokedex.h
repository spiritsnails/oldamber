#pragma once

void Pokedex_Open(void);
int  Pokedex_IsOpen(void);
void Pokedex_Tick(void);

void Pokedex_SetSeen(int species);
void Pokedex_SetOwned(int species);

void Pokedex_SetSeenByDexNum(int dex_num);

void Pokedex_ShowData(int dex_num);
void Pokedex_ShowDataTick(void);
int  Pokedex_IsShowingData(void);
