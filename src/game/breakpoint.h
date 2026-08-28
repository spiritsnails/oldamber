#pragma once

#include <stdint.h>
#include <stddef.h>

#define BREAKPOINT_DOWNSAMPLE 6

int Breakpoint_Commit(const char *map_label,
                      const uint8_t *rw_data, const int *rw_seq, int rw_len,
                      size_t state_size, uint8_t cur_map, int x, int y,
                      char *out_name, size_t out_name_sz);

int Breakpoint_LoadBundle(const char *name, uint8_t *rw_data, int *rw_seq,
                          int *rw_len, size_t state_size, int max_slots);
