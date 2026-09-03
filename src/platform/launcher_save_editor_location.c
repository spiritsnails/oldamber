#include "launcher_save_editor_location.h"

#include "data_dir.h"
#include "../data/map_data.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static se_location_t locations[SE_LOCATION_MAX];
static int location_count;

static int compare_ci(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a++);
        int cb = tolower((unsigned char)*b++);
        if (ca != cb) return ca - cb;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static int has_block_suffix(const char *name) {
    size_t n = strlen(name);
    return n > 6 && strcmp(name + n - 6, ".block") == 0;
}

static void parse_location_file(se_location_t *location, const char *path) {
    FILE *f = fopen(path, "r");
    char line[1024];
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        char map_name[64];
        int a, b, c;
        if (!location->has_spawn &&
            sscanf(line, "warpspot %63s %d %d %d", map_name, &a, &b, &c) == 4 &&
            a == 0 && compare_ci(map_name, location->name) == 0) {
            if (b < 0) b = 0;
            if (c < 0) c = 0;
            if (b > 255) b = 255;
            if (c > 255) c = 255;
            location->spawn_x = (uint8_t)b;
            location->spawn_y = (uint8_t)c;
            location->has_spawn = 1;
        } else if (!location->has_bounds &&
                   sscanf(line, "mapsize %63s %d %d", map_name, &a, &b) == 3 &&
                   compare_ci(map_name, location->name) == 0 && a > 0 && b > 0) {
            int max_x = a * 2 - 1;
            int max_y = b * 2 - 1;
            location->max_x = (uint8_t)(max_x > 255 ? 255 : max_x);
            location->max_y = (uint8_t)(max_y > 255 ? 255 : max_y);
            location->has_bounds = 1;
        }
    }
    fclose(f);
}

static int load_directory(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    struct dirent *entry;
    if (!dir) return 0;
    location_count = 0;
    while ((entry = readdir(dir)) != NULL && location_count < SE_LOCATION_MAX) {
        size_t n;
        char path[1400];
        se_location_t *location;
        if (entry->d_name[0] == '.' || !has_block_suffix(entry->d_name)) continue;
        n = strlen(entry->d_name) - 6;
        if (n == 0 || n >= sizeof(locations[0].name)) continue;
        location = &locations[location_count];
        memset(location, 0, sizeof(*location));
        memcpy(location->name, entry->d_name, n);
        location->name[n] = '\0';
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        parse_location_file(location, path);
        location_count++;
    }
    closedir(dir);
    for (int i = 1; i < location_count; i++) {
        se_location_t value = locations[i];
        int j = i - 1;
        while (j >= 0 && compare_ci(locations[j].name, value.name) > 0) {
            locations[j + 1] = locations[j];
            j--;
        }
        locations[j + 1] = value;
    }
    return location_count > 0;
}

int SE_LocationLoad(const char *version_label) {
    char data_dir[1200];
    char candidate[1400];
    const char *version = version_label && compare_ci(version_label, "BLUE") == 0
                        ? "blue" : "red";
    location_count = 0;
    if (DataDir_Get(data_dir, sizeof(data_dir))) {
        snprintf(candidate, sizeof(candidate),
                 "%smod_runtime/generatedmaps/%s/blocks", data_dir, version);
        if (load_directory(candidate)) return location_count;
    }
    snprintf(candidate, sizeof(candidate),
             "mod_runtime/generatedmaps/%s/blocks", version);
    if (load_directory(candidate)) return location_count;
    snprintf(candidate, sizeof(candidate),
             "../mod_runtime/generatedmaps/%s/blocks", version);
    if (load_directory(candidate)) return location_count;
    return 0;
}

int SE_LocationCount(void) {
    return location_count;
}

const char *SE_LocationLabel(void *ctx, int index) {
    (void)ctx;
    if (index < 0 || index >= location_count) return "";
    return locations[index].name;
}

int SE_LocationFind(const char *name) {
    if (!name || !name[0]) return -1;
    for (int i = 0; i < location_count; i++)
        if (compare_ci(locations[i].name, name) == 0) return i;
    return -1;
}

const char *SE_LocationCurrentName(const save_editor_data_t *data) {
    int slot;
    if (!data || data->cur_map < PKS_VIRTUAL_MAP_FIRST) return NULL;
    slot = data->cur_map - PKS_VIRTUAL_MAP_FIRST;
    return data->vmap_bindings[slot][0] ? data->vmap_bindings[slot] : NULL;
}

int SE_LocationCurrentBounds(const save_editor_data_t *data,
                             uint32_t *max_x, uint32_t *max_y) {
    int index = SE_LocationFind(SE_LocationCurrentName(data));
    if (index < 0 || !locations[index].has_bounds) return 0;
    if (max_x) *max_x = locations[index].max_x;
    if (max_y) *max_y = locations[index].max_y;
    return 1;
}

int SE_LocationApply(save_editor_data_t *data, int index) {
    int current_slot = -1;
    int last_slot = -1;
    int target_slot = -1;
    uint8_t old_map;
    const se_location_t *location;
    if (!data || index < 0 || index >= location_count) return 0;
    location = &locations[index];
    if (data->cur_map >= PKS_VIRTUAL_MAP_FIRST &&
        data->cur_map < PKS_VIRTUAL_MAP_LAST)
        current_slot = data->cur_map - PKS_VIRTUAL_MAP_FIRST;
    if (data->last_map >= PKS_VIRTUAL_MAP_FIRST &&
        data->last_map < PKS_VIRTUAL_MAP_LAST)
        last_slot = data->last_map - PKS_VIRTUAL_MAP_FIRST;
    for (int i = 0; i < PKS_VIRTUAL_MAP_COUNT - 1; i++) {
        if (data->vmap_bindings[i][0] &&
            compare_ci(data->vmap_bindings[i], location->name) == 0) {
            target_slot = i;
            break;
        }
    }
    if (target_slot == current_slot) return 0;
    if (target_slot < 0) {
        for (int i = 0; i < PKS_VIRTUAL_MAP_COUNT - 1; i++) {
            if (i != current_slot && !data->vmap_bindings[i][0]) {
                target_slot = i;
                break;
            }
        }
    }
    if (target_slot < 0) {
        for (int i = 0; i < PKS_VIRTUAL_MAP_COUNT - 1; i++) {
            if (i != current_slot && i != last_slot) {
                target_slot = i;
                break;
            }
        }
    }
    if (target_slot < 0) return 0;
    old_map = data->cur_map;
    snprintf(data->vmap_bindings[target_slot],
             sizeof(data->vmap_bindings[target_slot]), "%s", location->name);
    data->vmap_bindings[PKS_VIRTUAL_MAP_COUNT - 1][0] = '\0';
    data->last_map = old_map;
    data->cur_map = (uint8_t)(PKS_VIRTUAL_MAP_FIRST + target_slot);
    if (location->has_spawn) {
        data->x_coord = location->spawn_x;
        data->y_coord = location->spawn_y;
    } else if (location->has_bounds) {
        data->x_coord = location->max_x / 2;
        data->y_coord = location->max_y / 2;
    } else {
        data->x_coord = 0;
        data->y_coord = 0;
    }
    data->location_changed = 1;
    return 1;
}
