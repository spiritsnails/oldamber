#pragma once
#include <stddef.h>

int DataDir_Get(char *out, size_t n);

int DataDir_IsSeparate(void);

int DataDir_SeedFromInstall(void);
