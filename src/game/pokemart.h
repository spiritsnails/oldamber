#pragma once
#include <stdint.h>

void Pokemart_SetInventory(const uint8_t *inv);

void Pokemart_Start(void);
int  Pokemart_IsActive(void);
void Pokemart_Tick(void);

void ViridianMart_Start(void);
void PewterMart_Start(void);
void CeruleanMart_Start(void);
void VermilionMart_Start(void);
void LavenderMart_Start(void);
void Celadon2F1Mart_Start(void);
void Celadon2F2Mart_Start(void);
void Celadon4FMart_Start(void);
void Celadon5F1Mart_Start(void);
void Celadon5F2Mart_Start(void);
void FuchsiaMart_Start(void);
void CinnabarMart_Start(void);
void SaffronMart_Start(void);
void IndigoMart_Start(void);
