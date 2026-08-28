
#include "game_version.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifndef AMBER_GAME_VERSION
#define AMBER_GAME_VERSION "red"
#endif

typedef struct {
    const char *id;
    const char *label;
    const char *title;
    const char *save;
} game_version_t;

static const game_version_t kVersions[] = {

    { "red",  "RED",  "RED",  "pokered.sav"  },
    { "blue", "BLUE", "BLUE", "pokeblue.sav" },
};
#define KVERSIONS_N ((int)(sizeof(kVersions) / sizeof(kVersions[0])))

static const game_version_t *find(const char *id) {
    if (!id) return NULL;
    for (int i = 0; i < KVERSIONS_N; i++)
        if (strcmp(kVersions[i].id, id) == 0) return &kVersions[i];
    return NULL;
}

static char s_current[16] = AMBER_GAME_VERSION;

const char *GameVersion_Current(void) { return s_current; }

int GameVersion_SupportedCount(void) { return KVERSIONS_N; }

void GameVersion_Set(const char *id) {
    if (!find(id)) {

        fprintf(stderr, "[version] unknown game version '%s' -- staying on %s\n",
                id ? id : "(null)", s_current);
        return;
    }
    snprintf(s_current, sizeof(s_current), "%s", id);
    printf("[version] running as %s\n", s_current);
}

static int installed(const char *id) {
    char path[256];
    struct stat st;
    snprintf(path, sizeof(path), "packages/%s/packages.txt", id);
    if (stat(path, &st) == 0) return 1;
    snprintf(path, sizeof(path), "../packages/%s/packages.txt", id);
    return stat(path, &st) == 0;
}

int GameVersion_ScanInstalled(const char *out[], int max) {
    int n = 0;
    for (int i = 0; i < KVERSIONS_N && n < max; i++)
        if (installed(kVersions[i].id)) out[n++] = kVersions[i].id;
    return n;
}

const char *GameVersion_Label(const char *id) {
    const game_version_t *v = find(id);
    return v ? v->label : (id ? id : "");
}

const char *GameVersion_LabelAt(int index) {
    if (index < 0 || index >= KVERSIONS_N) return NULL;
    return kVersions[index].label;
}

const char *GameVersion_FromRomHeader(const char *rom_path) {
    if (!rom_path || !rom_path[0]) return NULL;

    FILE *f = fopen(rom_path, "rb");
    if (!f) return NULL;
    char title[17];
    memset(title, 0, sizeof(title));
    if (fseek(f, 0x134, SEEK_SET) == 0)
        (void)fread(title, 1, 16, f);
    fclose(f);

    for (int i = 0; i < 16; i++) {
        unsigned char c = (unsigned char)title[i];
        if (c < 0x20 || c > 0x7E) { title[i] = '\0'; break; }
        if (c >= 'a' && c <= 'z') title[i] = (char)(c - 'a' + 'A');
    }
    title[16] = '\0';

    for (int i = 0; i < KVERSIONS_N; i++)
        if (strstr(title, kVersions[i].title)) return kVersions[i].id;
    return NULL;
}

const char *GameVersion_SavePath(const char *id) {
    const game_version_t *v = find(id ? id : s_current);
    return v ? v->save : "pokered.sav";
}
