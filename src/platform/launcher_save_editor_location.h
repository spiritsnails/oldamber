#pragma once

#include "save.h"

#define SE_LOCATION_MAX 512

typedef struct {
    char name[32];
    uint8_t spawn_x;
    uint8_t spawn_y;
    uint8_t max_x;
    uint8_t max_y;
    int has_spawn;
    int has_bounds;
} se_location_t;

int SE_LocationLoad(const char *version_label);
int SE_LocationCount(void);
const char *SE_LocationLabel(void *ctx, int index);
int SE_LocationFind(const char *name);
const char *SE_LocationCurrentName(const save_editor_data_t *data);
int SE_LocationCurrentBounds(const save_editor_data_t *data,
                             uint32_t *max_x, uint32_t *max_y);
int SE_LocationApply(save_editor_data_t *data, int index);
