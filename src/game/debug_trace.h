#pragma once

#include <stdint.h>

enum {
    TRACE_ACTOR = 0,
    TRACE_UI,
    TRACE_NPC,
    TRACE_WARP,
    TRACE_FLAG,
    TRACE_SCENE,
    TRACE_CHANNEL_COUNT
};

void Trace_Emit(int channel, const char *fmt, ...);
void Trace_SetFrame(uint64_t frame);
void Trace_DumpAll(const char *bundle_dir);
int  Trace_SetStreamByName(const char *name, int on);
void Trace_FlushStreams(void);
void Trace_RestartStreams(void);
const char *Trace_ChannelName(int channel);
