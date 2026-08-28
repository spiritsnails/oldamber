#pragma once

#include <stddef.h>

void DebugFields_Init(void);

int  DebugFields_Set(const char *name, long value);

void DebugFields_Dump(void);
