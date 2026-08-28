
#include <stdint.h>
#include <stddef.h>
#include "../data/event_data.h"

int AmberScript_IsEnabled(void) {
    return 0;
}

const map_events_t *AmberScript_GetMapEventsFor(uint8_t map_id) {
    return &gMapEvents[map_id];
}

const map_events_t *AmberScript_GetMapEventsForFreshLoad(uint8_t map_id) {
    return &gMapEvents[map_id];
}

int AmberScript_GetSurfableOverrideAt(int tx, int ty, uint8_t *surfable) {
    (void)tx;
    (void)ty;
    (void)surfable;
    return 0;
}

int AmberScript_GetGrassOverrideAt(int tx, int ty, uint8_t *grass) {
    (void)tx;
    (void)ty;
    (void)grass;
    return 0;
}

int AmberScript_GetLedgeOverrideAt(int tx, int ty, int *ledge_dirs) {
    (void)tx;
    (void)ty;
    (void)ledge_dirs;
    return 0;
}

int AmberScript_MapBank_EnsureResidentByName(const char *name) {
    (void)name;
    return -1;
}

const char *AmberScript_MapBank_NameForRealId(int real_id) {
    (void)real_id;
    return NULL;
}

int AmberScript_MapBank_GetGbcTilesetForRealId(int real_id) {
    (void)real_id;
    return -1;
}

int AmberScript_IsPairBlockedAt(int from_tx, int from_ty, int to_tx, int to_ty) {
    (void)from_tx;
    (void)from_ty;
    (void)to_tx;
    (void)to_ty;
    return 0;
}

uint16_t AmberScript_GetItemFlagBitAt(uint8_t real_id, int item_index) {
    (void)real_id;
    (void)item_index;
    return 0;
}

void AmberScript_MarkAllTrainersDefeated(const char *map_name) {
    (void)map_name;
}

void AmberScript_SetSubtilePixels(const char *name, const uint8_t pixels[16]) {
    (void)name;
    (void)pixels;
}

int AmberScript_TilePlaceCustom(const char *name, int x, int y) {
    (void)name;
    (void)x;
    (void)y;
    return 0;
}

int AmberScript_GetCutSpanBlockAt(int tx, int ty) {
    (void)tx;
    (void)ty;
    return -1;
}

void AmberScript_DebugDumpTilePropAt(int x, int y, char *out, size_t out_sz) {
    (void)x;
    (void)y;
    if (out && out_sz) out[0] = '\0';
}

int AmberScript_MapFindLiveNpcByDeclaredTile(int real_id, int key_x, int key_y) {
    (void)real_id;
    (void)key_x;
    (void)key_y;
    return -1;
}

int AmberScript_MapBank_GetCrystalEnvForRealId(int real_id, int *out_group) {
    (void)real_id;
    (void)out_group;
    return 0;
}

int AmberScript_MapBank_IsDarkForRealId(int real_id) {
    (void)real_id;
    return 0;
}

void AmberScript_SceneTriggerSuppressAt(int map_id, int x, int y) {
    (void)map_id;
    (void)x;
    (void)y;
}

int AmberScript_IsAutoWinEnabled(void) {
    return 0;
}

int AmberScript_MapBank_GetMusicForRealId(int real_id, char *out_track, size_t out_cap) {
    (void)real_id;
    (void)out_track;
    (void)out_cap;
    return 0;
}

int AmberScript_SubtileBlitPixels(const char *name, const uint8_t *pixels) {
    (void)name;
    (void)pixels;
    return 0;
}

int AmberScript_SubtileReadPixels(const char *name, uint8_t *out) {
    (void)name;
    (void)out;
    return 0;
}

int AmberScript_GetGrassRustleOverrideAt(int tx, int ty, uint8_t *rustle) {
    (void)tx;
    (void)ty;
    (void)rustle;
    return 0;
}
