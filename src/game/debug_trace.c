
#include "debug_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef _WIN32
#include <direct.h>
#define trace_mkdir(p) _mkdir(p)
#else
#include <sys/stat.h>
#define trace_mkdir(p) mkdir((p), 0755)
#endif

#define TRACE_LINE_MAX 192

typedef struct trace_ring_t {
    const char *name;
    int      capacity;
    char    *lines;
    int      head;
    int      count;
    FILE    *stream;
} trace_ring_t;

static trace_ring_t s_rings[TRACE_CHANNEL_COUNT] = {
    { "actor", 20000, NULL, 0, 0, NULL },
    { "ui",     4096, NULL, 0, 0, NULL },
    { "npc",    2048, NULL, 0, 0, NULL },
    { "warp",   2048, NULL, 0, 0, NULL },
    { "flag",   2048, NULL, 0, 0, NULL },
    { "scene",  8192, NULL, 0, 0, NULL },
};

static uint64_t s_trace_frame = 0;

const char *Trace_ChannelName(int channel) {
    if (channel < 0 || channel >= TRACE_CHANNEL_COUNT) return "?";
    return s_rings[channel].name;
}

void Trace_SetFrame(uint64_t frame) { s_trace_frame = frame; }

void Trace_Emit(int channel, const char *fmt, ...) {
    trace_ring_t *r;
    char *slot;
    int off;
    va_list ap;

    if (channel < 0 || channel >= TRACE_CHANNEL_COUNT) return;
    r = &s_rings[channel];
    if (!r->lines) {
        r->lines = (char *)calloc((size_t)r->capacity, TRACE_LINE_MAX);
        if (!r->lines) return;
    }
    slot = r->lines + (size_t)r->head * TRACE_LINE_MAX;
    off = snprintf(slot, TRACE_LINE_MAX, "{\"f\":%llu,",
                   (unsigned long long)s_trace_frame);
    va_start(ap, fmt);
    off += vsnprintf(slot + off, (size_t)(TRACE_LINE_MAX - off - 2), fmt, ap);
    va_end(ap);
    if (off > TRACE_LINE_MAX - 3) off = TRACE_LINE_MAX - 3;
    slot[off] = '}';
    slot[off + 1] = '\0';

    r->head = (r->head + 1) % r->capacity;
    if (r->count < r->capacity) r->count++;

    if (r->stream) {
        fputs(slot, r->stream);
        fputc('\n', r->stream);
    }
}

void Trace_DumpAll(const char *bundle_dir) {
    char dir[400], path[512];
    snprintf(dir, sizeof(dir), "%s/traces", bundle_dir);
    trace_mkdir(dir);
    for (int c = 0; c < TRACE_CHANNEL_COUNT; c++) {
        trace_ring_t *r = &s_rings[c];
        FILE *fp;
        if (!r->lines || r->count == 0) continue;
        snprintf(path, sizeof(path), "%s/%s.jsonl", dir, r->name);
        fp = fopen(path, "wb");
        if (!fp) continue;
        for (int i = 0; i < r->count; i++) {
            int idx = (r->head - r->count + i + r->capacity * 2) % r->capacity;
            const char *line = r->lines + (size_t)idx * TRACE_LINE_MAX;
            if (*line) {
                fputs(line, fp);
                fputc('\n', fp);
            }
        }
        fclose(fp);
    }
}

static int trace_stream_set(int channel, int on) {
    trace_ring_t *r = &s_rings[channel];
    if (on) {
        char path[128];
        if (r->stream) fclose(r->stream);
        snprintf(path, sizeof(path), "bugs/trace_%s.jsonl", r->name);
        r->stream = fopen(path, "w");
        return r->stream != NULL;
    }
    if (r->stream) {
        fclose(r->stream);
        r->stream = NULL;
    }
    return 1;
}

int Trace_SetStreamByName(const char *name, int on) {
    int any = 0;
    if (!name) return 0;
    if (strcmp(name, "all") == 0) {
        for (int c = 0; c < TRACE_CHANNEL_COUNT; c++)
            any |= trace_stream_set(c, on);
        return any;
    }
    for (int c = 0; c < TRACE_CHANNEL_COUNT; c++) {
        if (strcmp(name, s_rings[c].name) == 0)
            return trace_stream_set(c, on);
    }
    return 0;
}

void Trace_FlushStreams(void) {
    for (int c = 0; c < TRACE_CHANNEL_COUNT; c++)
        if (s_rings[c].stream) fflush(s_rings[c].stream);
}

void Trace_RestartStreams(void) {
    for (int c = 0; c < TRACE_CHANNEL_COUNT; c++)
        if (s_rings[c].stream) trace_stream_set(c, 1);
}
