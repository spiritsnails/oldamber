#pragma once
#include <stddef.h>

int Scenario_LoadAndStart(const char *path, char *error, size_t error_size);
int Scenario_CaptureCurrent(const char *path, char *error, size_t error_size);
