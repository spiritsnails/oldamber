#pragma once
#include <stdint.h>

int  NtscFilter_SetEnabled(int on);
int  NtscFilter_IsEnabled(void);

#define NTSC_DECODE_SCALE 2

const uint32_t *NtscFilter_Apply(const uint32_t *src, int w, int h,
                                 int *out_w, int *out_h);
