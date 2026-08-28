
#include "breakpoint.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#endif

#define BP_DIR    "bugs/breakpoints"
#define BP_MAGIC  0x314B5042u
#define BP_VER    1u

typedef struct bp_header_t {
    uint32_t magic;
    uint32_t version;
    uint32_t state_size;
    uint32_t count;
} bp_header_t;

static int bp_mkdir(const char *path) {
    struct stat st;
    if (!path || !*path) return -1;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode) ? 0 : -1;
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0777);
#endif
}

static int bp_mkdir_tree(const char *path) {
    char tmp[256];
    size_t len;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char c = tmp[i];
            tmp[i] = '\0';
            if (*tmp) bp_mkdir(tmp);
            tmp[i] = c;
        }
    }
    return bp_mkdir(tmp);
}

static void bp_sanitize(const char *in, char *out, size_t out_sz) {
    size_t n = 0;
    if (!in || !out || out_sz == 0) { if (out && out_sz) out[0] = '\0'; return; }
    for (size_t i = 0; in[i] && n + 1 < out_sz; i++) {
        char c = in[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-')
            out[n++] = c;
        else if (c == ' ')
            out[n++] = '_';
    }
    out[n] = '\0';
    if (n == 0) snprintf(out, out_sz, "map");
}

static int bp_next_index(void) {
    DIR *d = opendir(BP_DIR);
    struct dirent *ent;
    int next = 1;
    if (!d) return 1;
    while ((ent = readdir(d)) != NULL) {
        int idx = -1;
        if (sscanf(ent->d_name, "bp_%d_", &idx) == 1 && idx >= next)
            next = idx + 1;
    }
    closedir(d);
    return next;
}

int Breakpoint_Commit(const char *map_label,
                      const uint8_t *rw_data, const int *rw_seq, int rw_len,
                      size_t state_size, uint8_t cur_map, int x, int y,
                      char *out_name, size_t out_name_sz) {
    char label[40], name[80], dir[160], bin[200], meta[200];
    FILE *f;
    bp_header_t hdr;
    int i, kept = 0, idx;

    if (!rw_data || !rw_seq || rw_len < 1 || state_size == 0) return -1;

    bp_mkdir_tree(BP_DIR);
    bp_sanitize(map_label ? map_label : "map", label, sizeof(label));
    idx = bp_next_index();
    snprintf(name, sizeof(name), "bp_%03d_%s", idx, label);
    snprintf(dir,  sizeof(dir),  "%s/%s", BP_DIR, name);
    if (bp_mkdir(dir) != 0) return -1;
    snprintf(bin,  sizeof(bin),  "%s/bundle.bin", dir);
    snprintf(meta, sizeof(meta), "%s/meta.json",  dir);

    for (i = 0; i < rw_len; i += BREAKPOINT_DOWNSAMPLE) kept++;
    if ((rw_len - 1) % BREAKPOINT_DOWNSAMPLE != 0) kept++;

    f = fopen(bin, "wb");
    if (!f) return -1;
    hdr.magic = BP_MAGIC; hdr.version = BP_VER;
    hdr.state_size = (uint32_t)state_size; hdr.count = (uint32_t)kept;
    if (fwrite(&hdr, sizeof(hdr), 1, f) != 1) { fclose(f); return -1; }
    for (i = 0; i < rw_len; i += BREAKPOINT_DOWNSAMPLE) {
        const uint8_t *src = rw_data + (size_t)rw_seq[i] * state_size;
        if (fwrite(src, 1, state_size, f) != state_size) { fclose(f); return -1; }
    }
    if ((rw_len - 1) % BREAKPOINT_DOWNSAMPLE != 0) {
        const uint8_t *src = rw_data + (size_t)rw_seq[rw_len - 1] * state_size;
        if (fwrite(src, 1, state_size, f) != state_size) { fclose(f); return -1; }
    }
    fclose(f);

    f = fopen(meta, "w");
    if (f) {
        fprintf(f,
            "{\n"
            "  \"name\": \"%s\",\n"
            "  \"created\": %ld,\n"
            "  \"map_id\": %d,\n"
            "  \"map_label\": \"%s\",\n"
            "  \"x\": %d,\n"
            "  \"y\": %d,\n"
            "  \"frames\": %d,\n"
            "  \"states\": %d,\n"
            "  \"hz\": %d,\n"
            "  \"lead_seconds\": %.1f\n"
            "}\n",
            name, (long)time(NULL), (int)cur_map, label, x, y,
            rw_len, kept, 60 / BREAKPOINT_DOWNSAMPLE,
            (double)rw_len / 60.0);
        fclose(f);
    }

    if (out_name && out_name_sz) snprintf(out_name, out_name_sz, "%s", name);
    printf("[breakpoint] committed '%s' (%d frames -> %d states, ~%.0fs window)\n",
           name, rw_len, kept, (double)rw_len / 60.0);
    return 0;
}

int Breakpoint_LoadBundle(const char *name, uint8_t *rw_data, int *rw_seq,
                          int *rw_len, size_t state_size, int max_slots) {
    char bin[220];
    FILE *f;
    bp_header_t hdr;
    int i, n;

    if (!name || !*name || !rw_data || !rw_seq || !rw_len) return -1;

    if (strstr(name, "..") || strchr(name, '/') || strchr(name, '\\')) return -1;

    snprintf(bin, sizeof(bin), "%s/%s/bundle.bin", BP_DIR, name);
    f = fopen(bin, "rb");
    if (!f) { printf("[breakpoint] restore: no bundle '%s'\n", name); return -1; }
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) { fclose(f); return -1; }
    if (hdr.magic != BP_MAGIC || hdr.version != BP_VER) {
        printf("[breakpoint] restore: '%s' bad header\n", name);
        fclose(f); return -1;
    }
    if (hdr.state_size != state_size) {
        printf("[breakpoint] restore: '%s' state_size %u != build %u "
               "(save format changed since commit) -- cannot load\n",
               name, hdr.state_size, (unsigned)state_size);
        fclose(f); return -1;
    }
    n = (int)hdr.count;
    if (n < 1) { fclose(f); return -1; }
    if (n > max_slots) n = max_slots;

    for (i = 0; i < n; i++) {
        uint8_t *dst = rw_data + (size_t)i * state_size;
        if (fread(dst, 1, state_size, f) != state_size) { fclose(f); return -1; }
        rw_seq[i] = i;
    }
    fclose(f);
    *rw_len = n;
    printf("[breakpoint] restored '%s' into ring (%d states)\n", name, n);
    return n;
}
