#include "fly_anim.h"

#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "../platform/display.h"
#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "warp.h"
#include "constants.h"
#include "../data/sprite_data.h"
#include <stddef.h>

typedef enum {
    FLY_IDLE = 0,
    FLY_DEPART_FLAP,
    FLY_DEPART_FLY1,
    FLY_DEPART_WAIT,
    FLY_DEPART_FLY2,
    FLY_FADE_OUT,
    FLY_FADE_IN,
    FLY_ARRIVE_FLY,
} fly_phase_t;

typedef struct {
    fly_phase_t phase;
    uint8_t dest_map;
    int dest_x;
    int dest_y;
    int frames_left;
    int step;
    int blocks_left;
    int block_frames_left;
    uint8_t bird_image_index;
    int bird_frame;
    uint8_t bird_flags;
    int bird_x;
    int bird_y;
} fly_anim_state_t;

typedef struct {
    uint8_t y;
    uint8_t x;
} fly_coord_t;

static fly_anim_state_t s_fly = {0};

#define FLY_TILE_BASE   64
#define FLY_TILE_BYTES  16
#define FLY_STEP_FRAMES 3
#define FLY_WAIT_FRAMES  40

#define FLY_BIRD_FRAME_STAND 2
#define FLY_BIRD_FRAME_WALK  5

static const uint8_t kFlyFadeOut[3][3] = {
    { 0x90, 0x80, 0x90 },
    { 0x40, 0x40, 0x40 },
    { 0x00, 0x00, 0x00 },
};

static const uint8_t kFlyFadeIn[4][3] = {
    { 0x00, 0x00, 0x00 },
    { 0x0A, 0x01, 0x08 },
    { 0x28, 0x04, 0x20 },
    { 0xE4, 0xD0, 0xE0 },
};

static const fly_coord_t kFlyDepartPath1[] = {
    { 0x3C, 0x48 },
    { 0x3C, 0x50 },
    { 0x3B, 0x58 },
    { 0x3A, 0x60 },
    { 0x39, 0x68 },
    { 0x37, 0x70 },
    { 0x37, 0x78 },
    { 0x33, 0x80 },
    { 0x30, 0x88 },
    { 0x2D, 0x90 },
    { 0x2A, 0x98 },
    { 0x27, 0xA0 },
};

static const fly_coord_t kFlyDepartPath2[] = {
    { 0x1A, 0x90 },
    { 0x19, 0x80 },
    { 0x17, 0x70 },
    { 0x15, 0x60 },
    { 0x12, 0x50 },
    { 0x0F, 0x40 },
    { 0x0C, 0x30 },
    { 0x09, 0x20 },
    { 0x05, 0x10 },
    { 0x00, 0x00 },
    { 0xF0, 0x00 },
};

static const fly_coord_t kFlyArrivePath[] = {
    { 0x05, 0x98 },
    { 0x0F, 0x90 },
    { 0x18, 0x88 },
    { 0x20, 0x80 },
    { 0x27, 0x78 },
    { 0x2D, 0x70 },
    { 0x32, 0x68 },
    { 0x36, 0x60 },
    { 0x39, 0x58 },
    { 0x3B, 0x50 },
    { 0x3C, 0x48 },
    { 0x3C, 0x40 },
};

static void load_bird_frame(int frame) {
    int tile_base = FLY_TILE_BASE;
    int tile = frame * 4;
    for (int i = 0; i < 4; i++) {
        Display_LoadSpriteTile((uint8_t)(tile_base + i),
                               &gSpriteGfx[9][(tile + i) * FLY_TILE_BYTES]);
    }
}

static void fly_apply_bird_image(uint8_t image_index) {
    s_fly.bird_image_index = image_index;
    s_fly.bird_frame = (image_index & 1u) ? FLY_BIRD_FRAME_WALK : FLY_BIRD_FRAME_STAND;
    s_fly.bird_flags = (image_index & 0x04u) ? OAM_FLAG_FLIP_X : 0;
}

static void fly_start_block(void) {
    fly_apply_bird_image((uint8_t)(s_fly.bird_image_index ^ 1u));
    s_fly.block_frames_left = FLY_STEP_FRAMES;
}

static void draw_bird(void) {
    int sx = s_fly.bird_x;
    int sy = s_fly.bird_y;
    load_bird_frame(s_fly.bird_frame);
    uint8_t tl = FLY_TILE_BASE + 0;
    uint8_t tr = FLY_TILE_BASE + 1;
    uint8_t bl = FLY_TILE_BASE + 2;
    uint8_t br = FLY_TILE_BASE + 3;

    if (s_fly.bird_flags & OAM_FLAG_FLIP_X) {
        tl = FLY_TILE_BASE + 1;
        tr = FLY_TILE_BASE + 0;
        bl = FLY_TILE_BASE + 3;
        br = FLY_TILE_BASE + 2;
    }

    wShadowOAM[0].y = (uint8_t)(sy + OAM_Y_OFS);
    wShadowOAM[0].x = (uint8_t)(sx + OAM_X_OFS);
    wShadowOAM[0].tile = tl;
    wShadowOAM[0].flags = s_fly.bird_flags;

    wShadowOAM[1].y = (uint8_t)(sy + OAM_Y_OFS);
    wShadowOAM[1].x = (uint8_t)(sx + 8 + OAM_X_OFS);
    wShadowOAM[1].tile = tr;
    wShadowOAM[1].flags = s_fly.bird_flags;

    wShadowOAM[2].y = (uint8_t)(sy + 8 + OAM_Y_OFS);
    wShadowOAM[2].x = (uint8_t)(sx + OAM_X_OFS);
    wShadowOAM[2].tile = bl;
    wShadowOAM[2].flags = s_fly.bird_flags;

    wShadowOAM[3].y = (uint8_t)(sy + 8 + OAM_Y_OFS);
    wShadowOAM[3].x = (uint8_t)(sx + 8 + OAM_X_OFS);
    wShadowOAM[3].tile = br;
    wShadowOAM[3].flags = s_fly.bird_flags;
}

static void fly_set_fade_out_step(int step) {
    Display_SetPalette(kFlyFadeOut[step][0], kFlyFadeOut[step][1], kFlyFadeOut[step][2]);
}

static void fly_set_fade_in_step(int step) {
    Display_SetPalette(kFlyFadeIn[step][0], kFlyFadeIn[step][1], kFlyFadeIn[step][2]);
}

static void fly_begin_depart_path1(void) {
    Audio_PlaySFX_Fly();
    s_fly.phase = FLY_DEPART_FLY1;
    s_fly.step = 0;
    s_fly.blocks_left = (int)(sizeof(kFlyDepartPath1) / sizeof(kFlyDepartPath1[0]));
    s_fly.block_frames_left = 0;
    fly_apply_bird_image(0x0Cu);
}

static void fly_begin_depart_path2(void) {
    s_fly.phase = FLY_DEPART_FLY2;
    s_fly.step = 0;
    s_fly.blocks_left = (int)(sizeof(kFlyDepartPath2) / sizeof(kFlyDepartPath2[0]));
    s_fly.block_frames_left = 0;
    fly_apply_bird_image(0x08u);
}

static void fly_begin_arrival(void) {
    Audio_PlaySFX_Fly();
    s_fly.phase = FLY_ARRIVE_FLY;
    s_fly.step = 0;
    s_fly.blocks_left = (int)(sizeof(kFlyArrivePath) / sizeof(kFlyArrivePath[0]));
    s_fly.block_frames_left = 0;
    fly_apply_bird_image(0x08u);
}

void FlyAnim_Start(uint8_t dest_map, int dest_x, int dest_y) {
    Player_SyncOAM();
    s_fly.phase = FLY_DEPART_FLAP;
    s_fly.dest_map = dest_map;
    s_fly.dest_x = dest_x;
    s_fly.dest_y = dest_y;
    s_fly.step = 0;
    s_fly.blocks_left = 8;
    s_fly.block_frames_left = 0;
    fly_apply_bird_image(0x0Cu);
    s_fly.bird_x = (int)wShadowOAM[0].x - OAM_X_OFS;
    s_fly.bird_y = (int)wShadowOAM[0].y - OAM_Y_OFS;
}

int FlyAnim_IsActive(void) {
    return s_fly.phase != FLY_IDLE;
}

static int fly_tick_motion_phase(const fly_coord_t *coords, int coord_count) {
    if (s_fly.block_frames_left == 0) {
        fly_start_block();
    }

    draw_bird();

    if (--s_fly.block_frames_left > 0) {
        return 1;
    }

    if (coords != NULL && s_fly.step < coord_count) {
        s_fly.bird_y = coords[s_fly.step].y;
        s_fly.bird_x = coords[s_fly.step].x;
        s_fly.step++;
    }

    if (--s_fly.blocks_left > 0) {
        return 1;
    }

    return 0;
}

void FlyAnim_Tick(void) {
    if (s_fly.phase == FLY_IDLE) return;

    if (s_fly.phase == FLY_DEPART_FLAP) {
        if (fly_tick_motion_phase(NULL, 0)) return;
        fly_begin_depart_path1();
        return;
    }

    if (s_fly.phase == FLY_DEPART_FLY1) {
        if (fly_tick_motion_phase(kFlyDepartPath1, (int)(sizeof(kFlyDepartPath1) / sizeof(kFlyDepartPath1[0])))) return;
        draw_bird();
        s_fly.phase = FLY_DEPART_WAIT;
        s_fly.frames_left = FLY_WAIT_FRAMES;
        return;
    }

    if (s_fly.phase == FLY_DEPART_WAIT) {
        if (--s_fly.frames_left > 0) return;
        fly_begin_depart_path2();
        return;
    }

    if (s_fly.phase == FLY_DEPART_FLY2) {
        if (fly_tick_motion_phase(kFlyDepartPath2, (int)(sizeof(kFlyDepartPath2) / sizeof(kFlyDepartPath2[0])))) return;
        draw_bird();
        s_fly.phase = FLY_FADE_OUT;
        s_fly.step = 0;
        s_fly.frames_left = 8;
        fly_set_fade_out_step(0);
        return;
    }

    if (s_fly.phase == FLY_FADE_OUT) {
        if (--s_fly.frames_left > 0) return;
        if (++s_fly.step < 3) {
            s_fly.frames_left = 8;
            fly_set_fade_out_step(s_fly.step);
            return;
        }
        Warp_ForceTeleport(s_fly.dest_map, s_fly.dest_x, s_fly.dest_y);
        Map_BuildScrollView();
        NPC_BuildView(gScrollPxX, gScrollPxY);
        s_fly.phase = FLY_FADE_IN;
        s_fly.step = 0;
        s_fly.frames_left = 8;
        fly_set_fade_in_step(0);
        return;
    }

    if (s_fly.phase == FLY_FADE_IN) {
        if (--s_fly.frames_left > 0) return;
        if (++s_fly.step < 4) {
            s_fly.frames_left = 8;
            fly_set_fade_in_step(s_fly.step);
            return;
        }
        fly_begin_arrival();
        return;
    }

    if (fly_tick_motion_phase(kFlyArrivePath, (int)(sizeof(kFlyArrivePath) / sizeof(kFlyArrivePath[0])))) return;
    Player_SyncOAM();
    Map_BuildScrollView();
    NPC_BuildView(gScrollPxX, gScrollPxY);
    if (s_fly.phase == FLY_ARRIVE_FLY) {
        s_fly.phase = FLY_IDLE;
    }
}
