#include "platform/launcher_save_editor_location.h"

#include <stdio.h>
#include <string.h>

#ifndef LOCATION_TEST_DATA_DIR
#error LOCATION_TEST_DATA_DIR is required
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int DataDir_Get(char *out, size_t n) {
    int written = snprintf(out, n, "%s/", LOCATION_TEST_DATA_DIR);
    return written > 0 && (size_t)written < n;
}

int main(void) {
    save_editor_data_t data;
    uint32_t max_x = 0, max_y = 0;
    int alpha, beta, gamma;

    CHECK(SE_LocationLoad("RED") == 3);
    alpha = SE_LocationFind("AlphaTown");
    beta = SE_LocationFind("betacave");
    gamma = SE_LocationFind("GammaRoute");
    CHECK(alpha == 0 && beta == 1 && gamma == 2);

    memset(&data, 0, sizeof(data));
    data.cur_map = 248;
    data.last_map = 249;
    snprintf(data.vmap_bindings[0], sizeof(data.vmap_bindings[0]), "AlphaTown");
    snprintf(data.vmap_bindings[1], sizeof(data.vmap_bindings[1]), "BetaCave");
    CHECK(strcmp(SE_LocationCurrentName(&data), "AlphaTown") == 0);
    CHECK(SE_LocationCurrentBounds(&data, &max_x, &max_y));
    CHECK(max_x == 19 && max_y == 17);

    CHECK(SE_LocationApply(&data, gamma));
    CHECK(data.cur_map == 250);
    CHECK(data.last_map == 248);
    CHECK(strcmp(data.vmap_bindings[2], "GammaRoute") == 0);
    CHECK(data.x_coord == 12 && data.y_coord == 3);
    CHECK(data.location_changed);
    CHECK(!SE_LocationApply(&data, gamma));

    CHECK(SE_LocationApply(&data, beta));
    CHECK(data.cur_map == 249);
    CHECK(data.last_map == 250);
    CHECK(data.x_coord == 3 && data.y_coord == 3);

    memset(&data, 0, sizeof(data));
    data.cur_map = 248;
    data.last_map = 249;
    for (int i = 0; i < 7; i++)
        snprintf(data.vmap_bindings[i], sizeof(data.vmap_bindings[i]),
                 "Occupied%d", i);
    CHECK(SE_LocationApply(&data, alpha));
    CHECK(data.cur_map == 250);
    CHECK(strcmp(data.vmap_bindings[0], "Occupied0") == 0);
    CHECK(strcmp(data.vmap_bindings[1], "Occupied1") == 0);
    CHECK(strcmp(data.vmap_bindings[2], "AlphaTown") == 0);
    CHECK(data.vmap_bindings[7][0] == '\0');

    puts("save editor location tests passed");
    return 0;
}
