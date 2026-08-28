#pragma once

#include <stdint.h>

#define ANIM_SFX_MAX 256

typedef struct {
    const char *symbol;
    int         is_cry;
    uint8_t     species;
    int8_t      pitch;
    uint8_t     tempo;
} anim_sfx_event_t;

extern anim_sfx_event_t gAnimSfxEvents[ANIM_SFX_MAX];
extern int gAnimSfxCount;

extern int gAnimSfxSimulate;
extern int gAnimSfxRemaining;
void AnimSfx_AdvanceFrame(void);
