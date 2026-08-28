#include "test_runner.h"
#include "../src/game/battle/move_anim.h"
#include "../src/platform/hardware.h"
#include <string.h>

extern uint8_t gTestDisplayLastBGP;
extern uint8_t gTestDisplayBGPHistory[256];
extern int gTestDisplayBGPHistoryCount;

static void reset_move_anim_state(void) {
    extern void WRAMClear(void);
    WRAMClear();
    gTestDisplayLastBGP = 0;
    gTestDisplayBGPHistoryCount = 0;
    memset(gTestDisplayBGPHistory, 0, sizeof(gTestDisplayBGPHistory));
    hWhoseTurn = 0;
    hFrameCounter = 0;
}

static int history_contains(uint8_t v) {
    int i;
    for (i = 0; i < gTestDisplayBGPHistoryCount; i++) {
        if (gTestDisplayBGPHistory[i] == v) return 1;
    }
    return 0;
}

TEST(MoveAnimPhaseF, HighFreqSE_LeerAnim_DarkFlashReset) {
    move_anim_ctx_t ctx;
    reset_move_anim_state();
    memset(&ctx, 0, sizeof(ctx));

    ctx.animation_id = 43;
    MoveAnim_Run(&ctx);

    EXPECT_TRUE(history_contains(0x6F));
    EXPECT_TRUE(history_contains(0x1B));
    EXPECT_TRUE(history_contains(0x00));
    EXPECT_EQ((int)gTestDisplayLastBGP, 0xE4);
    EXPECT_EQ((int)hFrameCounter, 8);
}

TEST(MoveAnimPhaseF, HighFreqSE_MistAnim_LightThenReset) {
    move_anim_ctx_t ctx;
    reset_move_anim_state();
    memset(&ctx, 0, sizeof(ctx));

    ctx.animation_id = 54;
    MoveAnim_Run(&ctx);

    EXPECT_TRUE(history_contains(0x90));
    EXPECT_EQ((int)gTestDisplayLastBGP, 0xE4);
}

TEST(MoveAnimPhaseF, HighFreqSE_TailWhipAnim_Delay10Timing) {
    move_anim_ctx_t ctx;
    reset_move_anim_state();
    memset(&ctx, 0, sizeof(ctx));

    ctx.animation_id = 39;
    MoveAnim_Run(&ctx);

    EXPECT_EQ((int)hFrameCounter, 30);
}

TEST(MoveAnimPhaseF, HighFreqSubanims_RunThroughExpectedScripts) {
    static const struct {
        uint8_t animation_id;
        uint8_t expected_tileset;
        uint8_t expected_delay;
    } kCases[] = {

        { 5, 1, 0x06 },

        { 3, 0, 0x05 },

        { 4, 0, 0x04 },

        { 20, 0, 0x04 },

        { 23, 1, 0x08 },
    };
    int i;

    for (i = 0; i < (int)(sizeof(kCases) / sizeof(kCases[0])); i++) {
        move_anim_ctx_t ctx;
        reset_move_anim_state();
        memset(&ctx, 0, sizeof(ctx));

        ctx.animation_id = kCases[i].animation_id;
        MoveAnim_Run(&ctx);

        EXPECT_EQ((int)ctx.animation_id, (int)kCases[i].animation_id);
        EXPECT_EQ((int)ctx.which_tileset, (int)kCases[i].expected_tileset);
        EXPECT_EQ((int)ctx.subanim_frame_delay, (int)kCases[i].expected_delay);
        EXPECT_EQ((int)ctx.subanim_counter, 0);
    }
}

TEST(MoveAnimPhaseG, AnimationsDisabled_SkipsScriptButKeepsDelayContract) {
    move_anim_ctx_t ctx;
    reset_move_anim_state();
    memset(&ctx, 0, sizeof(ctx));

    wOptions = 0x80u;

    ctx.animation_id = 43;
    MoveAnim_Run(&ctx);

    EXPECT_EQ((int)hFrameCounter, 30);
    EXPECT_FALSE(history_contains(0x6F));
    EXPECT_FALSE(history_contains(0x1B));
    EXPECT_EQ((int)gTestDisplayLastBGP, 0xE4);
}

TEST(MoveAnimPhaseG, RuntimeCleanup_MatchesAsmEndState) {
    move_anim_ctx_t ctx;
    reset_move_anim_state();
    memset(&ctx, 0, sizeof(ctx));

    ctx.animation_id = 5;
    MoveAnim_Run(&ctx);

    EXPECT_EQ((int)ctx.subanim_entry_index, 0);
    EXPECT_EQ((int)ctx.subanim_transform, 0);
    EXPECT_EQ((int)ctx.anim_sound_id, 0xFF);
}

TEST(MoveAnimPhaseG, SwiftAnim_WritesProjectileOAMDuringTicks) {
    move_anim_ctx_t ctx;
    int saw_visible = 0;
    int saw_star_tile = 0;
    int guard = 0;
    reset_move_anim_state();
    memset(&ctx, 0, sizeof(ctx));

    ctx.animation_id = 129;
    MoveAnim_Begin(&ctx);

    while (!MoveAnim_IsDone(&ctx) && guard < 256) {
        if (MoveAnim_Tick(&ctx)) break;

        for (int i = 0; i < MAX_SPRITES; i++) {
            if (wShadowOAM[i].y != 0u && wShadowOAM[i].y < 160u) {
                saw_visible = 1;
            }

            if (wShadowOAM[i].tile == 0x34u || wShadowOAM[i].tile == 0x44u) {
                saw_star_tile = 1;
            }
        }
        guard++;
    }

    EXPECT_TRUE(saw_visible);
    EXPECT_TRUE(saw_star_tile);
}
