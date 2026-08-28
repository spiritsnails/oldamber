#pragma once

#if defined(_MSC_VER)
#define WEAK
#define PACKED
#else
#define WEAK __attribute__((weak))
#define PACKED __attribute__((packed))
#endif
