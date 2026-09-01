#pragma once
#include <stddef.h>

int DataDir_Get(char *out, size_t n);

int DataDir_IsSeparate(void);

int DataDir_SeedFromInstall(void);

int UserDataDir_Get(char *out, size_t n);
int UserDataPath(const char *relative, char *out, size_t n);
void UserData_UseCurrentDirectory(void);

int UserData_MigrateFromInstall(void);
