#pragma once

int  FieldMove_HasFlash(int slot);
int  FieldMove_HasDig(int slot);
int  FieldMove_HasTeleport(int slot);

int  FieldMove_TryDig(int slot);

int  FieldMove_TryTeleport(int slot);

int  FieldMove_TryFlash(int slot);

int  FieldMove_TryFly(int slot);

int  FieldMove_UseSurfFromMenu(int slot);

int  FieldMove_UseCutFromMenu(void);

int  FieldMove_TryStrength(int slot);
int  FieldMove_IsStrengthActive(void);
void FieldMove_ClearStrength(void);
void FieldMove_OnMapLoad(void);

int  FieldMove_IsActive(void);

void FieldMove_Tick(void);

void FieldMove_PostRender(void);
