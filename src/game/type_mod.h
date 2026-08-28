#pragma once

#include <stdint.h>

int TypeMod_ResolveTypeToken(const char *tok, uint8_t *out_type);
void TypeMod_SetTypeAlias(const char *name, uint8_t type_id);
const char *TypeMod_GetTypeName(uint8_t type_id);

void TypeMod_SetEffect(uint8_t atk, uint8_t def, uint8_t eff);
int TypeMod_GetEffectOverride(uint8_t atk, uint8_t def, uint8_t *out_eff);

void TypeMod_SetSpeciesTypes(uint8_t species, uint8_t type1, uint8_t type2);
void TypeMod_GetSpeciesTypes(uint8_t species, uint8_t *out_type1, uint8_t *out_type2);

void TypeMod_BankSetEnabled(int enabled);
int TypeMod_BankIsEnabled(void);
int TypeMod_BankSave(void);
int TypeMod_BankLoad(void);
int TypeMod_BankClear(void);
