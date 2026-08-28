#pragma once
#include <stdio.h>

#ifndef AMBER_DEBUG_PRINTS
#define AMBER_DEBUG_PRINTS 1
#endif

#if AMBER_DEBUG_PRINTS
#define DBG_PRINTF(...)      printf(__VA_ARGS__)
#define DBG_FPRINTF(f, ...)  fprintf((f), __VA_ARGS__)
#else
#define DBG_PRINTF(...)      do { if (0) printf(__VA_ARGS__); } while (0)
#define DBG_FPRINTF(f, ...)  do { if (0) fprintf((f), __VA_ARGS__); } while (0)
#endif
