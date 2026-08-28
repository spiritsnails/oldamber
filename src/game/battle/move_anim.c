
#include <string.h>
#include <stdio.h>
#include "move_anim.h"
#include "../constants.h"
#include "battle_ui.h"
#include "assetpack_bind.h"
#include "../gen2_species.h"
#include "crystal_mon_pics.h"
#include "../../data/ghost_front_sprite.h"
#include "../../platform/hardware.h"
#include "../../platform/display.h"
#include "../../platform/audio.h"
#include "../../data/move_anim_scripts.h"
#include "../../data/move_anim_subanims.h"
#include "../../data/move_anim_frameblocks.h"
#include "../../data/move_anim_basecoords.h"
#include "../../data/move_anim_tiles.h"
#include "../../data/move_sfx_data.h"
#include "../../data/base_stats.h"
#include "../../data/pokemon_sprites.h"
#include "../sprite_mod.h"
#include "../../data/font_data.h"
#include "../overworld.h"

#define MOVE_AMNESIA_ID           0x85u
#define MOVE_ANIM_NO_MOVE_MINUS_ONE 0xFFu
#define MOVE_ANIM_SLP_ANIM_ID       189u
#define MOVE_ANIM_CONF_ANIM_ID      191u

#define MOVE_ANIM_COPY_TEMP_PIC_FRAMES 7u

#define MOVE_ANIM_TILE_BASE_ID      0x31u
#define MOVE_ANIM_HVFLIP_BASE_Y     136u
#define MOVE_ANIM_HVFLIP_BASE_X     168u

#define MOVE_ANIM_SUBANIMTYPE_NORMAL    0u
#define MOVE_ANIM_SUBANIMTYPE_HVFLIP    1u
#define MOVE_ANIM_SUBANIMTYPE_HFLIP     2u
#define MOVE_ANIM_SUBANIMTYPE_COORDFLIP 3u
#define MOVE_ANIM_SUBANIMTYPE_REVERSE   4u
#define MOVE_ANIM_SUBANIMTYPE_ENEMY     5u

#define MOVE_ANIM_FRAMEBLOCKMODE_02 2u
#define MOVE_ANIM_FRAMEBLOCKMODE_03 3u
#define MOVE_ANIM_FRAMEBLOCKMODE_04 4u
#define MOVE_ANIM_ENEMY_SPR_TILE_BASE 0u
#define MOVE_ANIM_PLAYER_BG_TILE_BASE 53u
#define MOVE_ANIM_PLAYER_BG_COL       1u
#define MOVE_ANIM_PLAYER_BG_ROW       5u

#define TRADE_BALL_DROP_ANIM_ID   170u
#define TRADE_BALL_SHAKE_ANIM_ID  171u
#define TRADE_BALL_TILT_ANIM_ID   172u
#define TRADE_BALL_POOF_ANIM_ID   173u

#define BALL_TOSS_ANIM_ID         193u
#define BALL_SHAKE_ANIM_ID        194u
#define BALL_POOF_ANIM_ID         195u
#define GREAT_TOSS_ANIM_ID        197u
#define ULTRA_TOSS_ANIM_ID        198u

#define MOVE_ANIM_ULTRA_BALL      2u

#define MOVE_ANIM_BALL_GHOST      0x10u

#define TRADE_BALL_OAM_COUNT 4u

#define MOVE_MEGA_PUNCH_ID        0x05u
#define MOVE_GUILLOTINE_ID        0x0Cu
#define MOVE_MEGA_KICK_ID         0x19u
#define MOVE_HEADBUTT_ID          0x1Du
#define MOVE_DISABLE_ID           0x32u
#define MOVE_HYPER_BEAM_ID        0x3Fu
#define MOVE_REFLECT_ID           0x73u
#define MOVE_SELFDESTRUCT_ID      0x78u
#define MOVE_SPORE_ID             0x93u
#define MOVE_EXPLOSION_ID         0x99u
#define MOVE_ROCK_SLIDE_ID        0x9Du

#define MOVE_ANIM_RT_UNINITIALIZED 0u
#define MOVE_ANIM_RT_RUNNING       1u
#define MOVE_ANIM_RT_DONE          2u
static int MoveAnim_PlayAnimationStep(move_anim_ctx_t *ctx);
static int MoveAnim_PlaySubanimationStep(move_anim_ctx_t *ctx);
static void MoveAnim_LoadSubanimation(move_anim_ctx_t *ctx, uint8_t subanim_id);
static void MoveAnim_DrawFrameBlock(move_anim_ctx_t *ctx, uint8_t frameblock_id);
static void MoveAnim_LoadTileset(move_anim_ctx_t *ctx, uint8_t tileset_id);

static void MoveAnim_DoSpecialEffectByAnimationId(move_anim_ctx_t *ctx);
static int MoveAnim_DoSpecialEffectByAnimationIdStep(move_anim_ctx_t *ctx);
static int MoveAnim_SE_TradeShakePokeballStep(move_anim_ctx_t *ctx);
static int MoveAnim_SE_TradeJumpPokeballStep(move_anim_ctx_t *ctx);
static int  MoveAnim_PlayApplyingAttackStep(move_anim_ctx_t *ctx);
static void MoveAnim_PlayApplyingAttackSound(void);
static int  MoveAnim_SE_TempPicShowStep(move_anim_ctx_t *ctx);
static int  MoveAnim_SE_ChangeMonPicStep(move_anim_ctx_t *ctx);
static int  MoveAnim_ShakeHorizStep(move_anim_ctx_t *ctx, uint8_t amplitude);
static int  MoveAnim_ShakeVertStep(move_anim_ctx_t *ctx, uint8_t amplitude);
static int  MoveAnim_ShakeSlowStep(move_anim_ctx_t *ctx, uint8_t b, uint8_t rep);
static void MoveAnim_SetAnimationPalette(move_anim_ctx_t *ctx);
static void MoveAnim_ShareMoveAnimations(move_anim_ctx_t *ctx);
static void MoveAnim_RunSpecialEffect(move_anim_ctx_t *ctx, uint8_t se_id);
static int MoveAnim_RunSpecialEffectStep(move_anim_ctx_t *ctx);
static void MoveAnim_PlayCommandSound(move_anim_ctx_t *ctx, uint8_t sound_id_minus_one);
static void MoveAnim_AnimationDarkScreenPalette(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationResetScreenPalette(move_anim_ctx_t *ctx);
static int  MoveAnim_SE_ShakeScreenStep(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationWaterDropletsEverywhere(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationDarkenMonPalette(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationFlashScreenLong(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationSlideMonUp(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationSlideMonDown(move_anim_ctx_t *ctx);
static int  MoveAnim_SE_SlideMonOffStep(move_anim_ctx_t *ctx);
static void MoveAnim_SlideMonOffOneStep(void);
static int  MoveAnim_SE_BlinkMonStep(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationMoveMonHorizontally(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationResetMonPosition(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationLightScreenPalette(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationHideMonPic(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationSquishMonPic(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationShootBallsUpward(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationShootManyBallsUpward(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationBoundUpAndDown(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationSlideMonDownAndHide(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationLeavesFalling(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationPetalsFalling(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationShakeEnemyHUD(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationSpiralBallsInward(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationDelay10(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationHideEnemyMonPic(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationShowMonPic(move_anim_ctx_t *ctx);
static void MoveAnim_ShowEnemyPicAtHome(void);
static const move_anim_trade_hooks_t *sMoveAnimTradeHooks;
static void MoveAnim_AnimationShowEnemyMonPic(move_anim_ctx_t *ctx);
static void MoveAnim_ShakeBackAndForthDrawAt(uint8_t right);
static void MoveAnim_ShakeBackAndForthClearAt(uint8_t right);
static int  MoveAnim_FlashScreenPhase(move_anim_ctx_t *ctx, uint8_t *phase);
static int  MoveAnim_AnimIdWantsFlash(const move_anim_ctx_t *ctx);
static int  MoveAnim_AnimIdWantsShake(const move_anim_ctx_t *ctx);
static int  MoveAnim_RockSlideShakePhase(move_anim_ctx_t *ctx);
static int  MoveAnim_SE_ShakeBackAndForthStep(move_anim_ctx_t *ctx);
static void MoveAnim_AnimationWavyScreen(move_anim_ctx_t *ctx);
static uint8_t MoveAnim_GetSubanimationTransform1(uint8_t subanim_type);
static uint8_t MoveAnim_GetSubanimationTransform2(void);
static uint8_t MoveAnim_HVFlipFlags(uint8_t flags);
static uint8_t MoveAnim_HFlipFlags(uint8_t flags);
static uint8_t MoveAnim_GetFrameBlockYOffset(const move_anim_frameblock_sprite_t *sprite);
static uint8_t MoveAnim_GetFrameBlockXOffset(const move_anim_frameblock_sprite_t *sprite);
static void MoveAnim_DelayFramesAsm(uint8_t frames);
static uint8_t MoveAnim_ConvertAsmFramesToEngineTicks(move_anim_ctx_t *ctx, uint8_t asm_frames);
static void MoveAnim_AnimationCleanOAM(void);
static void MoveAnim_ClearAnimationOAM(void);
static void MoveAnim_SetBGP(uint8_t bgp);
static void MoveAnim_EnemyPicFollowBGP(uint8_t on);
static void MoveAnim_Finalize(move_anim_ctx_t *ctx);
static void MoveAnim_ResetVisualState(void);
static void MoveAnim_ResetEnemyOAMPoseCanonical(void);
static void MoveAnim_SetEnemyVisible(uint8_t visible);
static void MoveAnim_OffsetEnemyY(int8_t delta);
static void MoveAnim_UpdatePerFrameEffects(move_anim_ctx_t *ctx);
static void MoveAnim_LoadEnemyFrontSprite(uint8_t species);
static void MoveAnim_LoadPlayerBackSprite(uint8_t species);
static void MoveAnim_HidePlayerBackSprite(void);
static void MoveAnim_ClearPlayerBackSpriteAt(uint8_t col, uint8_t row);
static void MoveAnim_PlacePlayerBackSpriteAt(uint8_t col, uint8_t row);
static int MoveAnim_SE_FlashScreenLongStep(move_anim_ctx_t *ctx);
static int MoveAnim_SE_SpiralBallsInwardStep(move_anim_ctx_t *ctx);
static int MoveAnim_SE_WaterDropletsEverywhereStep(move_anim_ctx_t *ctx);
static int MoveAnim_SE_SlideMonUpStep(move_anim_ctx_t *ctx);
static int MoveAnim_SE_SlideMonDownStep(move_anim_ctx_t *ctx);
static int MoveAnim_SE_ShootBallsUpwardStep(move_anim_ctx_t *ctx);
static int MoveAnim_SE_ShootManyBallsUpwardStep(move_anim_ctx_t *ctx);
static int MoveAnim_SE_ShakeEnemyHUDStep(move_anim_ctx_t *ctx);
static int MoveAnim_SE_LeavesFallingStep(move_anim_ctx_t *ctx);
static int MoveAnim_SE_PetalsFallingStep(move_anim_ctx_t *ctx);
static int MoveAnim_SE_SquishMonPicStep(move_anim_ctx_t *ctx);
static int MoveAnim_SE_WavyScreenStep(move_anim_ctx_t *ctx);
static int MoveAnim_SE_SlideMonDownAndHideStep(move_anim_ctx_t *ctx);
static uint8_t MoveAnim_IsCryMove(uint8_t animation_id);

static const subanim_def_t *sMoveAnimLoadedSubanimation;

static uint8_t sMoveAnimCurrentOBP0 = 0xE4u;
static uint8_t sMoveAnimCurrentBGP = 0xE4u;
static uint8_t sMoveAnimFlashSavedBGP = 0xE4u;

static uint8_t sMoveAnimEnemyPicOnOBP1 = 0u;
static uint8_t sMoveAnimLoadedTileset = 0xFFu;
static move_anim_ctx_t *sMoveAnimExecCtx = 0;
static int8_t sMoveAnimShakeX = 0;
static int8_t sMoveAnimShakeY = 0;
static uint8_t sMoveAnimShakeToggle = 0;
static uint8_t sMoveAnimWavyActive = 0;
static uint8_t sMoveAnimWavyPhase = 0;
static const uint8_t sMoveAnimFlashScreenLongPals[12] = {

    0xF9u, 0xFEu, 0xFFu, 0xFEu, 0xF9u, 0xE4u,
    0x90u, 0x40u, 0x00u, 0x40u, 0x90u, 0xE4u
};

static const uint8_t sMoveAnimUpwardBallsXPlayerTurn[] = {0x10u, 0x40u, 0x28u, 0x18u, 0x38u, 0x30u, 0xFFu};
static const uint8_t sMoveAnimUpwardBallsXEnemyTurn[] = {0x60u, 0x90u, 0x78u, 0x68u, 0x88u, 0x80u, 0xFFu};

static uint8_t sMoveAnimWaterBaseX = 0u;
static uint8_t sMoveAnimShootBaseY = 0u;
static uint8_t sMoveAnimShootBaseX = 0u;
static uint8_t sMoveAnimShootBallCount = 0u;
static uint8_t sMoveAnimShootDelay = 0u;
static uint8_t sMoveAnimFallingCount = 0u;
static uint8_t sMoveAnimFallingTile = 0u;
static uint8_t sMoveAnimFallingMove[20];

static unsigned sMoveAnimTickFrames = 0u;

void MoveAnim_Run(move_anim_ctx_t *ctx) {
    uint16_t guard = 0;
    MoveAnim_Begin(ctx);
    while (!MoveAnim_IsDone(ctx) && guard < 8192u) {
        if (MoveAnim_Tick(ctx)) {
            break;
        }
        guard++;
    }
    if (ctx && !MoveAnim_IsDone(ctx)) {
        MoveAnim_Finalize(ctx);
    }
}

void (*gMoveAnimTraceHook)(uint8_t anim_id, uint8_t whose_turn) = 0;

void MoveAnim_Begin(move_anim_ctx_t *ctx) {
    if (!ctx) return;
    printf("[ANIMDBG] begin id=%u turn=%u script=%s\n",
           (unsigned)ctx->animation_id, (unsigned)hWhoseTurn,
           (ctx->animation_id != 0u &&
            ctx->animation_id <= MOVE_ANIM_NUM_ATTACK_ANIMS &&
            gMoveAnimAttackAnimationPointers[(uint16_t)ctx->animation_id - 1u])
               ? "present" : "MISSING");
    fflush(stdout);
    sMoveAnimTickFrames = 0u;
    ctx->trace_fired = 0;

    ctx->entry_sound_waited = 0;
    ctx->trace_anim_id = ctx->animation_id;

    ctx->script_index = 0;
    ctx->wait_frames = 0;
    ctx->timing_frac_1e4 = 0;
    ctx->active_subanim = 0;
    ctx->runtime_state = MOVE_ANIM_RT_RUNNING;
    ctx->subanim_entry_index = 0;
    ctx->subanim_transform = 0;
    ctx->subanim_postdraw_pending = 0;
    ctx->anim_sound_id = MOVE_ANIM_NO_MOVE_MINUS_ONE;
    ctx->active_special_effect = 0;
    ctx->active_se_id = 0;
    ctx->se_phase = 0;
    ctx->se_index = 0;
    ctx->se_counter0 = 0;
    ctx->aid_phase = 0;
    ctx->aid_index = 0;
    ctx->pending_oam_clean = 0;
    ctx->script_done = 0;
    ctx->subanim_tiles_pending = 0;
    ctx->apply_phase = 0;
    ctx->pending_sound_id = MOVE_ANIM_NO_MOVE_MINUS_ONE;
    ctx->pending_subanim_id = 0;
    sMoveAnimLoadedSubanimation = 0;
    MoveAnim_ResetVisualState();

    MoveAnim_SetAnimationPalette(ctx);
    if (ctx->animation_id == 0) {
        MoveAnim_Finalize(ctx);
        return;
    }

    if (wOptions & (1u << 7)) {
        ctx->script_done = 1u;
        ctx->wait_frames = 30u;
        return;
    }
    MoveAnim_ShareMoveAnimations(ctx);
}

int MoveAnim_Tick(move_anim_ctx_t *ctx) {
    uint16_t guard = 0u;

    if (!ctx) return 1;
    if (ctx->runtime_state == MOVE_ANIM_RT_DONE) return 1;
    if (ctx->runtime_state != MOVE_ANIM_RT_RUNNING) return 1;

    if (!ctx->trace_fired) {
        ctx->trace_fired = 1;
        if (gMoveAnimTraceHook)
            gMoveAnimTraceHook(ctx->trace_anim_id, (uint8_t)hWhoseTurn);
    }

    if (++sMoveAnimTickFrames % 60u == 0u) {
        printf("[STALLDBG] id=%u f=%u entry_waited=%u movesfx=%d sfx=%d cry=%d "
               "wait=%u sidx=%u done=%u se=0x%02X sephase=%u apply=%u oam=%u\n",
               (unsigned)ctx->animation_id, sMoveAnimTickFrames,
               (unsigned)ctx->entry_sound_waited,
               Audio_IsMoveSFXPlaying(), Audio_IsSFXPlaying(), Audio_IsCryPlaying(),
               (unsigned)ctx->wait_frames, (unsigned)ctx->script_index,
               (unsigned)ctx->script_done, (unsigned)ctx->active_se_id,
               (unsigned)ctx->se_phase, (unsigned)ctx->apply_phase,
               (unsigned)ctx->pending_oam_clean);
        fflush(stdout);
    }

    if (!ctx->entry_sound_waited) {
        if (Audio_IsMoveSFXPlaying() || Audio_IsSFXPlaying() ||
            Audio_IsCryPlaying()) {
            return 0;
        }
        ctx->entry_sound_waited = 1u;
    }

    while (ctx->wait_frames == 0u && guard++ < 1024u) {
        if (ctx->pending_oam_clean == 1u) {

            ctx->pending_oam_clean = 2u;
            ctx->wait_frames = MoveAnim_ConvertAsmFramesToEngineTicks(ctx, 1u);
            if (ctx->wait_frames == 0u) {
                ctx->wait_frames = 1u;
            }
            break;
        }
        if (ctx->pending_oam_clean == 2u) {

            MoveAnim_ClearAnimationOAM();
            ctx->pending_oam_clean = 0u;
            continue;
        }
        if (ctx->script_done) {
            int applied;

            sMoveAnimExecCtx = ctx;

            applied = MoveAnim_PlayApplyingAttackStep(ctx);
            if (!applied) {
                sMoveAnimExecCtx = 0;
                break;
            }

            if (!ctx->skip_sound_waits &&
                (Audio_IsMoveSFXPlaying() || Audio_IsSFXPlaying() ||
                 Audio_IsCryPlaying())) {
                MoveAnim_DelayFramesAsm(1u);
                sMoveAnimExecCtx = 0;
                break;
            }
            sMoveAnimExecCtx = 0;
            MoveAnim_Finalize(ctx);
            return 1;
        }

        sMoveAnimExecCtx = ctx;

        if (MoveAnim_PlayAnimationStep(ctx)) {
            ctx->script_done = 1u;
        }
        sMoveAnimExecCtx = 0;
    }

    if (guard >= 1024u) {
        printf("[STALLDBG] id=%u VM SPUN OUT (1024 steps, no frame owed) "
               "sidx=%u done=%u se=0x%02X sephase=%u apply=%u\n",
               (unsigned)ctx->animation_id, (unsigned)ctx->script_index,
               (unsigned)ctx->script_done, (unsigned)ctx->active_se_id,
               (unsigned)ctx->se_phase, (unsigned)ctx->apply_phase);
        fflush(stdout);
    }

    if (ctx->wait_frames != 0u) {
        MoveAnim_UpdatePerFrameEffects(ctx);
        ctx->wait_frames = (uint8_t)(ctx->wait_frames - 1u);
    }
    return 0;
}

int MoveAnim_IsDone(const move_anim_ctx_t *ctx) {
    if (!ctx) return 1;
    return ctx->runtime_state == MOVE_ANIM_RT_DONE;
}

static void MoveAnim_Finalize(move_anim_ctx_t *ctx) {
    uint8_t keep_charge_hide = 0u;

    uint8_t player_should_hide =
        (uint8_t)((wPlayerBattleStatus1 & (1u << BSTAT1_INVULNERABLE)) != 0u);
    if (!ctx) return;

    if (ctx->pending_oam_clean) {
        MoveAnim_ClearAnimationOAM();
        ctx->pending_oam_clean = 0u;
    }

    if (sMoveAnimEnemyPicOnOBP1) {
        MoveAnim_EnemyPicFollowBGP(0u);
        Display_SetOBP1(0xE4u);
    }

    ctx->subanim_entry_index = 0;
    ctx->subanim_transform = 0;
    ctx->subanim_postdraw_pending = 0;
    ctx->anim_sound_id = MOVE_ANIM_NO_MOVE_MINUS_ONE;
    ctx->active_subanim = 0;
    ctx->active_special_effect = 0;
    ctx->active_se_id = 0;
    ctx->se_phase = 0;
    ctx->se_index = 0;
    ctx->se_counter0 = 0;
    ctx->aid_phase = 0;
    ctx->aid_index = 0;
    ctx->pending_oam_clean = 0;
    ctx->script_done = 0;
    ctx->wait_frames = 0;
    ctx->timing_frac_1e4 = 0;
    ctx->runtime_state = MOVE_ANIM_RT_DONE;
    sMoveAnimLoadedSubanimation = 0;

    if ((ctx->animation_id == 192u || ctx->animation_id == MOVE_TELEPORT)) {
        if ((hWhoseTurn == 0u && (wPlayerBattleStatus1 & (1u << BSTAT1_CHARGING_UP))) ||
            (hWhoseTurn != 0u && (wEnemyBattleStatus1 & (1u << BSTAT1_CHARGING_UP)))) {
            keep_charge_hide = 1u;
        }
    }
    if (keep_charge_hide) {
        printf("[DIGDBG] finalize keep_hide anim=%u turn=%u p_b1=0x%02X e_b1=0x%02X\n",
               ctx->animation_id, hWhoseTurn, wPlayerBattleStatus1, wEnemyBattleStatus1);
        sMoveAnimShakeX = 0;
        sMoveAnimShakeY = 0;
        sMoveAnimShakeToggle = 0;
        sMoveAnimWavyActive = 0u;
        sMoveAnimWavyPhase = 0u;
        Display_SetWavyPhase(0, 0);
        Display_SetShakeOffset(0, 0);
        return;
    }

    MoveAnim_ResetVisualState();

    if (wBattleType == 2u) {
        return;
    }

    if (sMoveAnimTradeHooks) {

    } else if (player_should_hide) {
        MoveAnim_HidePlayerBackSprite();
    } else {
        MoveAnim_LoadPlayerBackSprite(wBattleMon.species);
    }
}

static int MoveAnim_PlayAnimationStep(move_anim_ctx_t *ctx) {
    const uint8_t *script;

    if (!ctx) return 1;
    if (ctx->animation_id == 0) return 1;
    if (ctx->animation_id > MOVE_ANIM_NUM_ATTACK_ANIMS) return 1;

    script = gMoveAnimAttackAnimationPointers[(uint16_t)ctx->animation_id - 1u];
    if (!script) {
        printf("[ANIMDBG] step id=%u -> NULL script, nothing to play\n",
               (unsigned)ctx->animation_id);
        fflush(stdout);
        return 1;
    }

    if (ctx->active_subanim) {
        if (!MoveAnim_PlaySubanimationStep(ctx)) {
            return 0;
        }
        ctx->active_subanim = 0;

        if (ctx->wait_frames != 0u || ctx->pending_oam_clean != 0u) {
            return 0;
        }
    }
    if (ctx->active_special_effect) {
        if (!MoveAnim_RunSpecialEffectStep(ctx)) {
            return 0;
        }
        ctx->active_special_effect = 0;
        if (ctx->wait_frames != 0u || ctx->pending_oam_clean != 0u) {
            return 0;
        }
    }

    if (ctx->subanim_tiles_pending) {
        ctx->subanim_tiles_pending = 0u;
        MoveAnim_PlayCommandSound(ctx, ctx->pending_sound_id);
        MoveAnim_LoadSubanimation(ctx, ctx->pending_subanim_id);
        ctx->active_subanim = 1;
        if (!MoveAnim_PlaySubanimationStep(ctx)) {
            return 0;
        }
        ctx->active_subanim = 0;
        if (ctx->wait_frames != 0u || ctx->pending_oam_clean != 0u) {
            return 0;
        }
    }

    while (script[ctx->script_index] != 0xFFu) {
        uint8_t cmd0 = script[ctx->script_index++];

        if (cmd0 < MOVE_ANIM_FIRST_SE_ID) {

            ctx->pending_sound_id = script[ctx->script_index++];
            ctx->pending_subanim_id = script[ctx->script_index++];

            ctx->subanim_frame_delay = (uint8_t)(cmd0 & 0x3Fu);
            ctx->which_tileset = (uint8_t)(cmd0 >> 6);

            MoveAnim_LoadTileset(ctx, ctx->which_tileset);
            ctx->subanim_tiles_pending = 1u;
            return 0;
        }

        MoveAnim_PlayCommandSound(ctx, script[ctx->script_index++]);
        ctx->active_special_effect = 1u;
        ctx->active_se_id = cmd0;
        ctx->se_phase = 0u;
        ctx->se_index = 0u;
        ctx->se_counter0 = 0u;
        if (!MoveAnim_RunSpecialEffectStep(ctx)) {
            return 0;
        }
        ctx->active_special_effect = 0u;
        if (ctx->wait_frames != 0u || ctx->pending_oam_clean != 0u) {
            return 0;
        }
    }

    return 1;
}

static int MoveAnim_RunSpecialEffectStep(move_anim_ctx_t *ctx) {
    if (!ctx) return 1;

    switch (ctx->active_se_id) {
        case 0xFA:
            return MoveAnim_SE_WaterDropletsEverywhereStep(ctx);
        case 0xF8:
            return MoveAnim_SE_FlashScreenLongStep(ctx);
        case 0xF7:
            return MoveAnim_SE_SlideMonUpStep(ctx);
        case 0xF6:
            return MoveAnim_SE_SlideMonDownStep(ctx);
        case 0xEE:
            return MoveAnim_SE_SquishMonPicStep(ctx);
        case 0xED:
            return MoveAnim_SE_ShootBallsUpwardStep(ctx);
        case 0xEC:
            return MoveAnim_SE_ShootManyBallsUpwardStep(ctx);
        case 0xE4:
        case 0xE3:
            return MoveAnim_SE_ShakeEnemyHUDStep(ctx);
        case 0xE7:
            return MoveAnim_SE_LeavesFallingStep(ctx);
        case 0xE6:
            return MoveAnim_SE_PetalsFallingStep(ctx);
        case 0xE9:
            return MoveAnim_SE_SlideMonDownAndHideStep(ctx);
        case 0xE2:
            return MoveAnim_SE_SpiralBallsInwardStep(ctx);
        case 0xFE:
            return MoveAnim_FlashScreenPhase(ctx, &ctx->se_phase);
        case 0xFB:
            return MoveAnim_SE_ShakeScreenStep(ctx);
        case 0xF4:
        case 0xE5:
        case 0xDB:
            return MoveAnim_SE_SlideMonOffStep(ctx);
        case 0xF3:
        case 0xDE:
            return MoveAnim_SE_BlinkMonStep(ctx);
        case 0xD9:
        case 0xEA:
            return MoveAnim_SE_TempPicShowStep(ctx);
        case 0xE8:
        case 0xF5:
        case 0xE0:
            return MoveAnim_SE_ChangeMonPicStep(ctx);
        case 0xDA:
            return MoveAnim_SE_ShakeBackAndForthStep(ctx);
        case 0xD8:
            return MoveAnim_SE_WavyScreenStep(ctx);
        default:
            MoveAnim_RunSpecialEffect(ctx, ctx->active_se_id);
            return 1;
    }
}

static int MoveAnim_PlaySubanimationStep(move_anim_ctx_t *ctx) {
    const subanim_entry_t *entry;
    const move_anim_basecoord_t *basecoord;

    if (!ctx) return 1;
    if (!sMoveAnimLoadedSubanimation) return 1;

    if (ctx->anim_sound_id != MOVE_ANIM_NO_MOVE_MINUS_ONE) {

    }

    if (ctx->subanim_counter != 0u) {
        if (ctx->subanim_entry_index >= sMoveAnimLoadedSubanimation->count) return 1;

        entry = &sMoveAnimLoadedSubanimation->entries[ctx->subanim_entry_index];
        if (!ctx->subanim_postdraw_pending) {
            if (entry->basecoord_id >= MOVE_ANIM_NUM_BASECOORDS) return 1;

            basecoord = &gMoveAnimFrameBlockBaseCoords[entry->basecoord_id];
            ctx->base_y = basecoord->y;
            ctx->base_x = basecoord->x;
            ctx->fb_mode = entry->mode;

            MoveAnim_DrawFrameBlock(ctx, entry->frameblock_id);
            ctx->subanim_postdraw_pending = 1u;
            if (ctx->wait_frames != 0u || ctx->pending_oam_clean != 0u) {
                return 0;
            }
        }

        if (!MoveAnim_DoSpecialEffectByAnimationIdStep(ctx)) {
            return 0;
        }
        ctx->subanim_postdraw_pending = 0u;

        ctx->subanim_counter = (uint8_t)(ctx->subanim_counter - 1u);
        if (ctx->subanim_counter == 0u) return 1;

        if (ctx->subanim_transform == MOVE_ANIM_SUBANIMTYPE_REVERSE) {
            ctx->subanim_entry_index = (uint16_t)(ctx->subanim_entry_index - 1u);
        } else {
            ctx->subanim_entry_index = (uint16_t)(ctx->subanim_entry_index + 1u);
        }

        return 0;
    }

    return 1;
}

static void MoveAnim_LoadSubanimation(move_anim_ctx_t *ctx, uint8_t subanim_id) {
    const subanim_def_t *def;
    uint8_t transform;

    if (!ctx) return;
    if (subanim_id >= MOVE_ANIM_NUM_SUBANIMS) {
        sMoveAnimLoadedSubanimation = 0;
        ctx->subanim_counter = 0;
        return;
    }

    def = gMoveAnimSubanimationPointers[subanim_id];
    if (!def) {
        sMoveAnimLoadedSubanimation = 0;
        ctx->subanim_counter = 0;
        return;
    }

    sMoveAnimLoadedSubanimation = def;
    ctx->subanim_counter = def->count;
    ctx->subanim_postdraw_pending = 0u;

    ctx->fb_dest_oam_index = 0;

    if (def->type == MOVE_ANIM_SUBANIMTYPE_ENEMY) {
        transform = MoveAnim_GetSubanimationTransform2();
    } else {
        transform = MoveAnim_GetSubanimationTransform1(def->type);
    }
    ctx->subanim_transform = transform;

    if (transform == MOVE_ANIM_SUBANIMTYPE_REVERSE && ctx->subanim_counter != 0u) {
        ctx->subanim_entry_index = (uint16_t)(ctx->subanim_counter - 1u);
    } else {
        ctx->subanim_entry_index = 0;
    }
}

static void MoveAnim_DrawFrameBlock(move_anim_ctx_t *ctx, uint8_t frameblock_id) {
    const move_anim_frameblock_def_t *frameblock;
    uint16_t dest_oam_index;
    uint8_t i;

    if (!ctx) return;
    if (frameblock_id >= MOVE_ANIM_NUM_FRAMEBLOCKS) return;

    frameblock = gMoveAnimFrameBlockPointers[frameblock_id];
    if (!frameblock) return;

    dest_oam_index = ctx->fb_dest_oam_index;
    for (i = 0; i < frameblock->count; i++) {
        const move_anim_frameblock_sprite_t *sprite = &frameblock->sprites[i];
        uint8_t y_offset = MoveAnim_GetFrameBlockYOffset(sprite);
        uint8_t x_offset = MoveAnim_GetFrameBlockXOffset(sprite);
        uint8_t y_coord;
        uint8_t x_coord;
        uint8_t flags;
        uint8_t tile = (uint8_t)(sprite->tile_id + MOVE_ANIM_TILE_BASE_ID);

        switch (ctx->subanim_transform) {
            case MOVE_ANIM_SUBANIMTYPE_HVFLIP: {
                uint8_t y_sum = (uint8_t)(ctx->base_y + y_offset);
                uint8_t x_sum = (uint8_t)(ctx->base_x + x_offset);
                y_coord = (uint8_t)(MOVE_ANIM_HVFLIP_BASE_Y - y_sum);
                x_coord = (uint8_t)(MOVE_ANIM_HVFLIP_BASE_X - x_sum);
                flags = MoveAnim_HVFlipFlags(sprite->flags);
                break;
            }
            case MOVE_ANIM_SUBANIMTYPE_HFLIP: {
                uint8_t x_sum = (uint8_t)(ctx->base_x + x_offset);
                y_coord = (uint8_t)(ctx->base_y + y_offset + 40u);
                x_coord = (uint8_t)(MOVE_ANIM_HVFLIP_BASE_X - x_sum);
                flags = MoveAnim_HFlipFlags(sprite->flags);
                break;
            }
            case MOVE_ANIM_SUBANIMTYPE_COORDFLIP:
                y_coord = (uint8_t)((uint8_t)(MOVE_ANIM_HVFLIP_BASE_Y - ctx->base_y) + y_offset);
                x_coord = (uint8_t)((uint8_t)(MOVE_ANIM_HVFLIP_BASE_X - ctx->base_x) + x_offset);
                flags = sprite->flags;
                break;
            default:
                y_coord = (uint8_t)(ctx->base_y + y_offset);
                x_coord = (uint8_t)(ctx->base_x + x_offset);
                flags = sprite->flags;
                break;
        }

        if (dest_oam_index < MAX_SPRITES) {
            wShadowOAM[dest_oam_index].y = y_coord;
            wShadowOAM[dest_oam_index].x = x_coord;
            wShadowOAM[dest_oam_index].tile = tile;
            wShadowOAM[dest_oam_index].flags = flags;
        }
        dest_oam_index++;
    }

    if (ctx->fb_mode == MOVE_ANIM_FRAMEBLOCKMODE_02) {
        ctx->fb_dest_oam_index = dest_oam_index;
        return;
    }

    MoveAnim_DelayFramesAsm(ctx->subanim_frame_delay);
    if (ctx->fb_mode == MOVE_ANIM_FRAMEBLOCKMODE_03) {
        ctx->fb_dest_oam_index = dest_oam_index;
        return;
    }
    if (ctx->fb_mode == MOVE_ANIM_FRAMEBLOCKMODE_04) return;

    if (ctx->animation_id != MOVE_GROWL) {

        ctx->pending_oam_clean = 1u;
    }
    ctx->fb_dest_oam_index = 0;
}

static void MoveAnim_LoadTileset(move_anim_ctx_t *ctx, uint8_t tileset_id) {
    const uint8_t (*tiles)[16] = 0;
    uint8_t tile_count = 0;
    uint8_t i;

    if (!ctx) return;
    if (tileset_id > 2u) tileset_id = 0u;

    switch (tileset_id) {
        case 0:
            tiles = gMoveAnimTileset0;
            tile_count = MOVE_ANIM_TILESET0_TILES;
            break;
        case 1:
            tiles = gMoveAnimTileset1;
            tile_count = MOVE_ANIM_TILESET1_TILES;
            break;
        default:

            tiles = gMoveAnimTileset0;
            tile_count = MOVE_ANIM_TILESET2_TILES;
            break;
    }

    MoveAnim_DelayFramesAsm((uint8_t)(tile_count / 8u + 1u));

    for (i = 0; i < tile_count; i++) {
        Display_LoadSpriteTile((uint8_t)(MOVE_ANIM_TILE_BASE_ID + i), tiles[i]);
    }

    sMoveAnimLoadedTileset = tileset_id;
}

static const int8_t sTradeBallMoveDistances1[] = { -12, -12, -8 };

static const int8_t sTradeBallMoveDistances2[] = { 11, 12, -12, -7, 7, 12, -8, 8 };

static const move_anim_trade_hooks_t *sMoveAnimTradeHooks = 0;

void MoveAnim_SetTradeHooks(const move_anim_trade_hooks_t *hooks) {
    sMoveAnimTradeHooks = hooks;
}

static void MoveAnim_TradeOffsetBallY(int8_t delta) {
    for (uint8_t i = 0u; i < TRADE_BALL_OAM_COUNT && i < MAX_SPRITES; i++) {
        wShadowOAM[i].y = (uint8_t)(wShadowOAM[i].y + (uint8_t)delta);
    }
}

static int MoveAnim_SE_TradeShakePokeballStep(move_anim_ctx_t *ctx) {
    if (ctx->subanim_counter != 1u) return 1;

    if (ctx->aid_index < (uint8_t)(sizeof sTradeBallMoveDistances1)) {
        MoveAnim_TradeOffsetBallY(sTradeBallMoveDistances1[ctx->aid_index]);
        ctx->aid_index++;
        MoveAnim_DelayFramesAsm(3u);
        return 0;
    }

    MoveAnim_AnimationCleanOAM();
    Audio_PlaySFX_TradeMachine();
    ctx->aid_index = 0u;
    return 1;
}

static int MoveAnim_SE_TradeJumpPokeballStep(move_anim_ctx_t *ctx) {
    const uint8_t n = (uint8_t)(sizeof sTradeBallMoveDistances2);

    if (ctx->aid_phase == 1u) {
        hSCX = (uint8_t)(hSCX - 8u);
        ctx->aid_phase = 0u;
    }

    if (ctx->aid_index >= n) {

        if (sMoveAnimTradeHooks && sMoveAnimTradeHooks->clear_screen) {
            sMoveAnimTradeHooks->clear_screen();
        }
        ctx->aid_index = 0u;
        return 1;
    }

    MoveAnim_TradeOffsetBallY(sTradeBallMoveDistances2[ctx->aid_index]);
    ctx->aid_index++;

    if (ctx->aid_index >= n || sTradeBallMoveDistances2[ctx->aid_index] == 12) {
        Audio_PlaySFX_Swap();
    }

    ctx->aid_phase = 1u;
    MoveAnim_DelayFramesAsm(5u);
    return 0;
}

static int MoveAnim_FlashScreenPhase(move_anim_ctx_t *ctx, uint8_t *phase) {
    (void)ctx;
    switch (*phase) {
        case 0u:
            sMoveAnimFlashSavedBGP = sMoveAnimCurrentBGP;
            MoveAnim_SetBGP(0x1Bu);
            MoveAnim_DelayFramesAsm(2u);
            *phase = 1u;
            return 0;
        case 1u:
            MoveAnim_SetBGP(0x00u);
            MoveAnim_DelayFramesAsm(2u);
            *phase = 2u;
            return 0;
        default:
            MoveAnim_SetBGP(sMoveAnimFlashSavedBGP);
            *phase = 0u;
            return 1;
    }
}

static int MoveAnim_AnimIdWantsFlash(const move_anim_ctx_t *ctx) {
    uint8_t c = ctx->subanim_counter;
    switch (ctx->animation_id) {
        case MOVE_MEGA_PUNCH_ID:
        case MOVE_GUILLOTINE_ID:
        case MOVE_MEGA_KICK_ID:
        case MOVE_HEADBUTT_ID:
        case MOVE_DISABLE_ID:
        case MOVE_BUBBLEBEAM:
        case MOVE_REFLECT_ID:
        case MOVE_SPORE_ID:
            return 1;
        case MOVE_HYPER_BEAM_ID:
            return (c & 3u) == 0u;
        case MOVE_THUNDERBOLT:
            return (c & 7u) == 0u;
        case MOVE_BLIZZARD:
            return c == 13u || c == 9u || c == 5u || c == 1u;
        case MOVE_SELFDESTRUCT_ID:
        case MOVE_EXPLOSION_ID:
            return (c != 1u) && ((c & 3u) == 0u);
        case MOVE_ROCK_SLIDE_ID:
            return c == 1u;
        default:
            return 0;
    }
}

static int MoveAnim_AnimIdWantsShake(const move_anim_ctx_t *ctx) {
    return ctx->animation_id == MOVE_ROCK_SLIDE_ID &&
           ctx->subanim_counter >= 8u && ctx->subanim_counter < 12u;
}

static int MoveAnim_RockSlideShakePhase(move_anim_ctx_t *ctx) {

    switch (ctx->aid_phase) {
        case 0u:
            sMoveAnimShakeX = 1;
            Display_SetShakeOffset(sMoveAnimShakeX, sMoveAnimShakeY);
            MoveAnim_DelayFramesAsm(4u);
            ctx->aid_phase = 1u;
            return 0;
        case 1u:
            MoveAnim_DelayFramesAsm(1u);
            ctx->aid_phase = 2u;
            return 0;
        case 2u:
            sMoveAnimShakeX = 0;
            Display_SetShakeOffset(sMoveAnimShakeX, sMoveAnimShakeY);
            MoveAnim_DelayFramesAsm(4u);
            ctx->aid_phase = 3u;
            return 0;
        case 3u:
            sMoveAnimShakeY = 1;
            Display_SetShakeOffset(sMoveAnimShakeX, sMoveAnimShakeY);
            MoveAnim_DelayFramesAsm(3u);
            ctx->aid_phase = 4u;
            return 0;
        case 4u:
            sMoveAnimShakeY = 0;
            Display_SetShakeOffset(sMoveAnimShakeX, sMoveAnimShakeY);
            MoveAnim_DelayFramesAsm(3u);
            ctx->aid_phase = 5u;
            return 0;
        default:
            ctx->aid_phase = 0u;
            return 1;
    }
}

static int MoveAnim_DoSpecialEffectByAnimationIdStep(move_anim_ctx_t *ctx) {
    if (!ctx) return 1;

    switch (ctx->animation_id) {
        case TRADE_BALL_DROP_ANIM_ID:

            if (ctx->subanim_counter == 6u &&
                sMoveAnimTradeHooks && sMoveAnimTradeHooks->clear_mon_pic) {
                sMoveAnimTradeHooks->clear_mon_pic();
            }
            return 1;

        case TRADE_BALL_SHAKE_ANIM_ID:
            return MoveAnim_SE_TradeShakePokeballStep(ctx);

        case TRADE_BALL_TILT_ANIM_ID:
            return MoveAnim_SE_TradeJumpPokeballStep(ctx);

        default:
            break;
    }

    if (MoveAnim_AnimIdWantsShake(ctx)) {
        return MoveAnim_RockSlideShakePhase(ctx);
    }

    if (ctx->aid_phase != 0u || MoveAnim_AnimIdWantsFlash(ctx)) {
        return MoveAnim_FlashScreenPhase(ctx, &ctx->aid_phase);
    }

    MoveAnim_DoSpecialEffectByAnimationId(ctx);
    return 1;
}

static void MoveAnim_SlideMonOffOneStep(void);

static void MoveAnim_ShiftEnemyPicRight(void) {
    uint8_t saved_turn = hWhoseTurn;
    hWhoseTurn = 1u;
    MoveAnim_SlideMonOffOneStep();
    hWhoseTurn = saved_turn;
}

static void MoveAnim_SE_BallToss(move_anim_ctx_t *ctx) {

    if (ctx->ball_item <= MOVE_ANIM_ULTRA_BALL) {
        sMoveAnimCurrentOBP0 ^= 0x3Cu;
        Display_SetOBP0(sMoveAnimCurrentOBP0);
    }

    if (ctx->subanim_counter == 11u) {
        Audio_PlaySfxModified(SFX_BALL_TOSS, 0, 0u);
    }

    if (wIsInBattle == 2u) {
        if (ctx->subanim_counter == 3u) {
            ctx->subanim_counter = (uint8_t)(ctx->subanim_counter - 1u);
        }
        return;
    }

    if (ctx->ball_anim_data != MOVE_ANIM_BALL_GHOST) return;
    if (ctx->subanim_counter >= 1u && ctx->subanim_counter <= 3u) {
        MoveAnim_ShiftEnemyPicRight();
    }
}

static void MoveAnim_SE_BallShake(move_anim_ctx_t *ctx) {
    if (ctx->subanim_counter == 4u) {
        Audio_PlaySfxModified(SFX_TINK_2, 0, 0u);
        MoveAnim_DelayFramesAsm(40u);
    }

    if (ctx->subanim_counter != 1u) return;

    ctx->num_shakes = (uint8_t)(ctx->num_shakes - 1u);
    if (ctx->num_shakes == 0u) return;

    ctx->subanim_entry_index = (uint16_t)(ctx->subanim_entry_index - 4u);
    ctx->subanim_counter = 5u;
}

static void MoveAnim_DoSpecialEffectByAnimationId(move_anim_ctx_t *ctx) {
    if (!ctx) return;

    switch (ctx->animation_id) {
        case MOVE_SELFDESTRUCT_ID:
        case MOVE_EXPLOSION_ID:
            if (ctx->subanim_counter == 1u) {
                MoveAnim_AnimationHideMonPic(ctx);
            }
            return;

        case MOVE_ROCK_SLIDE_ID:
            if (ctx->subanim_counter >= 12u) {
                return;
            }

            return;

        case MOVE_GROWL:
            if (MAX_SPRITES >= 8u) {
                memcpy(&wShadowOAM[4], &wShadowOAM[0], sizeof(wShadowOAM[0]) * 4u);
            }
            if (ctx->subanim_counter == 1u) {
                MoveAnim_AnimationCleanOAM();
            }
            return;

        case MOVE_TAIL_WHIP:

            ctx->subanim_counter = 1u;
            MoveAnim_DelayFramesAsm(20u);
            return;

        case BALL_TOSS_ANIM_ID:
        case GREAT_TOSS_ANIM_ID:
        case ULTRA_TOSS_ANIM_ID:
            MoveAnim_SE_BallToss(ctx);
            return;

        case BALL_SHAKE_ANIM_ID:
            MoveAnim_SE_BallShake(ctx);
            return;

        case BALL_POOF_ANIM_ID:

            if (ctx->subanim_counter == 5u) {
                Audio_PlaySfxModified(SFX_BALL_POOF, 0, 0u);
            }
            return;

        default:
            return;
    }
}

static void MoveAnim_PlayApplyingAttackSound(void) {
    uint8_t dmg = (uint8_t)(wDamageMultipliers & 0x7Fu);
    if (dmg == 0u) return;
    if (dmg == 10u) {
        Audio_PlaySfxModified(SFX_DAMAGE, (int8_t)0x20, 0x30u);
    } else if (dmg > 10u) {
        Audio_PlaySfxModified(SFX_SUPER_EFFECTIVE, (int8_t)0xE0, 0xFFu);
    } else {
        Audio_PlaySfxModified(SFX_NOT_VERY_EFFECTIVE, (int8_t)0x50, 0x01u);
    }
}

static int MoveAnim_PlayApplyingAttackStep(move_anim_ctx_t *ctx) {
    if (!ctx) return 1;
    if (ctx->animation_type == 0u || ctx->animation_type > 6u) return 1;

    if (ctx->apply_phase == 0u) {
        if (ctx->animation_type != 3u && ctx->animation_type != 6u) {

            if (!ctx->skip_sound_waits &&
                (Audio_IsMoveSFXPlaying() || Audio_IsSFXPlaying() ||
                 Audio_IsCryPlaying())) {
                MoveAnim_DelayFramesAsm(1u);
                return 0;
            }
            MoveAnim_PlayApplyingAttackSound();
        }
        ctx->se_phase = 0u;
        ctx->apply_phase = 1u;
    }

    switch (ctx->animation_type) {
        case 1u: return MoveAnim_ShakeVertStep(ctx, 8u);
        case 2u: return MoveAnim_ShakeHorizStep(ctx, 8u);
        case 3u: return MoveAnim_ShakeSlowStep(ctx, 6u, 2u);
        case 4u:
            ctx->active_se_id = 0xDEu;
            return MoveAnim_SE_BlinkMonStep(ctx);
        case 5u: return MoveAnim_ShakeHorizStep(ctx, 2u);
        case 6u: return MoveAnim_ShakeSlowStep(ctx, 3u, 2u);
        default: return 1;
    }
}

static void MoveAnim_SetAnimationPalette(move_anim_ctx_t *ctx) {
    if (!ctx) return;

    MoveAnim_SetBGP(0xE4u);
}

static void MoveAnim_ShareMoveAnimations(move_anim_ctx_t *ctx) {
    if (!ctx) return;

    if (hWhoseTurn == 0) return;

    if (ctx->animation_id == MOVE_AMNESIA_ID) {
        ctx->animation_id = MOVE_ANIM_CONF_ANIM_ID;
        return;
    }
    if (ctx->animation_id == MOVE_REST) {
        ctx->animation_id = MOVE_ANIM_SLP_ANIM_ID;
    }
}

static void MoveAnim_RunSpecialEffect(move_anim_ctx_t *ctx, uint8_t se_id) {
    if (!ctx) return;

    switch (se_id) {

        case 0xFD: MoveAnim_AnimationDarkScreenPalette(ctx); break;
        case 0xFC: MoveAnim_AnimationResetScreenPalette(ctx); break;

        case 0xFA: MoveAnim_AnimationWaterDropletsEverywhere(ctx); break;
        case 0xF9: MoveAnim_AnimationDarkenMonPalette(ctx); break;
        case 0xF8: MoveAnim_AnimationFlashScreenLong(ctx); break;
        case 0xF7: MoveAnim_AnimationSlideMonUp(ctx); break;
        case 0xF6: MoveAnim_AnimationSlideMonDown(ctx); break;
        case 0xF2: MoveAnim_AnimationMoveMonHorizontally(ctx); break;
        case 0xF1: MoveAnim_AnimationResetMonPosition(ctx); break;
        case 0xF0: MoveAnim_AnimationLightScreenPalette(ctx); break;
        case 0xEF: MoveAnim_AnimationHideMonPic(ctx); break;
        case 0xEE: MoveAnim_AnimationSquishMonPic(ctx); break;
        case 0xED: MoveAnim_AnimationShootBallsUpward(ctx); break;
        case 0xEC: MoveAnim_AnimationShootManyBallsUpward(ctx); break;
        case 0xEB: MoveAnim_AnimationBoundUpAndDown(ctx); break;
        case 0xE9: MoveAnim_AnimationSlideMonDownAndHide(ctx); break;
        case 0xE7: MoveAnim_AnimationLeavesFalling(ctx); break;
        case 0xE6: MoveAnim_AnimationPetalsFalling(ctx); break;
        case 0xE4: MoveAnim_AnimationShakeEnemyHUD(ctx); break;
        case 0xE3: MoveAnim_AnimationShakeEnemyHUD(ctx); break;
        case 0xE2: MoveAnim_AnimationSpiralBallsInward(ctx); break;
        case 0xE1: MoveAnim_AnimationDelay10(ctx); break;
        case 0xDF: MoveAnim_AnimationHideEnemyMonPic(ctx); break;
        case 0xDD: MoveAnim_AnimationShowMonPic(ctx); break;

        case 0xDC: MoveAnim_AnimationShowEnemyMonPic(ctx); break;

        case 0xD8: MoveAnim_AnimationWavyScreen(ctx); break;
        default:
            break;
    }
}

#define MOVE_ANIM_SE_NOOP(fn_name) \
    static void fn_name(move_anim_ctx_t *ctx) { (void)ctx; }

MOVE_ANIM_SE_NOOP(MoveAnim_AnimationWaterDropletsEverywhere)
MOVE_ANIM_SE_NOOP(MoveAnim_AnimationFlashScreenLong)
MOVE_ANIM_SE_NOOP(MoveAnim_AnimationShootBallsUpward)
MOVE_ANIM_SE_NOOP(MoveAnim_AnimationShootManyBallsUpward)
MOVE_ANIM_SE_NOOP(MoveAnim_AnimationShakeEnemyHUD)
MOVE_ANIM_SE_NOOP(MoveAnim_AnimationSpiralBallsInward)

static int MoveAnim_ShakeHorizStep(move_anim_ctx_t *ctx, uint8_t amplitude) {
    uint8_t mutate;

    if (ctx->se_phase == 0u) {
        ctx->se_counter0 = amplitude;
        ctx->se_index = 0u;
        ctx->se_phase = 1u;
    }

    if (ctx->se_phase == 1u || ctx->se_phase == 3u) {
        mutate = (uint8_t)(ctx->se_index ^ ctx->se_counter0);
        ctx->se_index = mutate;

        sMoveAnimShakeX = (int8_t)((mutate & 0x80u) ? 0 : mutate);
        Display_SetShakeOffset(sMoveAnimShakeX, sMoveAnimShakeY);
        MoveAnim_DelayFramesAsm(4u);

        if (ctx->se_phase == 1u) {
            ctx->se_phase = 2u;
            return 0;
        }
        ctx->se_counter0 = (uint8_t)(ctx->se_counter0 - 1u);
        if (ctx->se_counter0 == 0u) {
            ctx->se_phase = 4u;
        } else {
            ctx->se_index = ctx->se_counter0;
            ctx->se_phase = 1u;
        }
        return 0;
    }

    if (ctx->se_phase == 2u) {
        MoveAnim_DelayFramesAsm(1u);
        ctx->se_phase = 3u;
        return 0;
    }

    sMoveAnimShakeX = 0;
    Display_SetShakeOffset(sMoveAnimShakeX, sMoveAnimShakeY);
    ctx->se_phase = 0u;
    return 1;
}

static int MoveAnim_ShakeVertStep(move_anim_ctx_t *ctx, uint8_t amplitude) {
    uint8_t mutate;

    if (ctx->se_phase == 0u) {
        ctx->se_counter0 = amplitude;
        ctx->se_index = 0u;
        ctx->se_phase = 1u;
    }

    if (ctx->se_phase == 1u || ctx->se_phase == 2u) {
        mutate = (uint8_t)(ctx->se_index ^ ctx->se_counter0);
        ctx->se_index = mutate;
        sMoveAnimShakeY = (int8_t)mutate;
        Display_SetShakeOffset(sMoveAnimShakeX, sMoveAnimShakeY);
        MoveAnim_DelayFramesAsm(3u);

        if (ctx->se_phase == 1u) {
            ctx->se_phase = 2u;
            return 0;
        }
        ctx->se_counter0 = (uint8_t)(ctx->se_counter0 - 1u);
        if (ctx->se_counter0 == 0u) {
            ctx->se_phase = 3u;
        } else {
            ctx->se_index = ctx->se_counter0;
            ctx->se_phase = 1u;
        }
        return 0;
    }

    sMoveAnimShakeY = 0;
    Display_SetShakeOffset(sMoveAnimShakeX, sMoveAnimShakeY);
    ctx->se_phase = 0u;
    return 1;
}

static int MoveAnim_ShakeSlowStep(move_anim_ctx_t *ctx, uint8_t b, uint8_t rep) {
    if (ctx->se_phase == 0u) {
        ctx->se_counter0 = rep;
        ctx->se_index = 0u;
        sMoveAnimShakeX = 0;
        ctx->se_phase = 1u;
    }

    if (ctx->se_phase == 1u || ctx->se_phase == 2u) {
        sMoveAnimShakeX = (int8_t)(sMoveAnimShakeX +
                                   ((ctx->se_phase == 1u) ? 1 : -1));
        Display_SetShakeOffset(sMoveAnimShakeX, sMoveAnimShakeY);
        MoveAnim_DelayFramesAsm(2u);

        ctx->se_index = (uint8_t)(ctx->se_index + 1u);
        if (ctx->se_index >= b) {
            ctx->se_index = 0u;
            if (ctx->se_phase == 1u) {
                ctx->se_phase = 2u;
            } else {
                ctx->se_counter0 = (uint8_t)(ctx->se_counter0 - 1u);
                ctx->se_phase = (uint8_t)(ctx->se_counter0 == 0u ? 3u : 1u);
            }
        }
        return 0;
    }

    sMoveAnimShakeX = 0;
    Display_SetShakeOffset(sMoveAnimShakeX, sMoveAnimShakeY);
    ctx->se_phase = 0u;
    return 1;
}

static int MoveAnim_SE_ChangeMonPicStep(move_anim_ctx_t *ctx) {
    uint8_t saved_turn;
    uint8_t species;

    if (!ctx) return 1;
    saved_turn = hWhoseTurn;
    if (ctx->active_se_id == 0xE0u) {
        hWhoseTurn = (uint8_t)(hWhoseTurn ^ 1u);
    }

    if (hWhoseTurn == 0u) {
        species = (ctx->active_se_id == 0xE8u) ? wEnemyMon.species
                                               : wBattleMon.species;
    } else {
        species = (ctx->active_se_id == 0xE8u) ? wBattleMon.species
                                               : wEnemyMon.species;
    }

    if (hWhoseTurn != 0u) {
        MoveAnim_LoadEnemyFrontSprite(species);
        hWhoseTurn = saved_turn;
        ctx->se_phase = 0u;
        return 1;
    }

    if (ctx->se_phase == 0u) {
        MoveAnim_ClearPlayerBackSpriteAt(1u, 5u);
        MoveAnim_LoadPlayerBackSprite(species);
        MoveAnim_ClearPlayerBackSpriteAt(1u, 5u);
        MoveAnim_DelayFramesAsm(7u);
        ctx->se_phase = 1u;
        hWhoseTurn = saved_turn;
        return 0;
    }

    MoveAnim_PlacePlayerBackSpriteAt(1u, 5u);
    ctx->se_phase = 0u;
    hWhoseTurn = saved_turn;
    return 1;
}

static int MoveAnim_SE_TempPicShowStep(move_anim_ctx_t *ctx) {
    if (!ctx) return 1;
    if (ctx->se_phase == 0u) {
        MoveAnim_DelayFramesAsm(MOVE_ANIM_COPY_TEMP_PIC_FRAMES);
        if (ctx->active_se_id == 0xEAu) {
            MoveAnim_DelayFramesAsm(3u);
        }
        ctx->se_phase = 1u;
        return 0;
    }
    MoveAnim_AnimationShowMonPic(ctx);
    ctx->se_phase = 0u;
    return 1;
}

static int MoveAnim_SE_ShakeScreenStep(move_anim_ctx_t *ctx) {

    if (!ctx) return 1;
    return MoveAnim_ShakeHorizStep(ctx, 8u);
}

static void MoveAnim_AnimationMoveMonHorizontally(move_anim_ctx_t *ctx) {
    (void)ctx;
    MoveAnim_AnimationHideMonPic(ctx);
    if (hWhoseTurn == 0u) {

        MoveAnim_PlacePlayerBackSpriteAt(2u, 5u);
    } else {

        MoveAnim_SetEnemyVisible(1u);
        MoveAnim_OffsetEnemyY(0);
        {
            uint16_t i;
            uint16_t start = BattleUI_GetEnemyOAMStart();
            uint16_t end = BattleUI_GetEnemyOAMEnd();
            for (i = start; i <= end && i < MAX_SPRITES; i++) {
                if (wShadowOAM[i].y != 0u) {
                    int16_t x = (int16_t)wShadowOAM[i].x - 8;
                    if (x < 0) x = 0;
                    wShadowOAM[i].x = (uint8_t)x;
                }
            }
            BattleUI_EnemySpriteCaptureState();
        }
    }
    MoveAnim_DelayFramesAsm(3u);
}

static void MoveAnim_AnimationResetMonPosition(move_anim_ctx_t *ctx) {
    (void)ctx;
    sMoveAnimShakeX = 0;
    sMoveAnimShakeY = 0;
    Display_SetShakeOffset(0, 0);
    if (hWhoseTurn == 0u) {

        MoveAnim_ClearPlayerBackSpriteAt(2u, 5u);
        MoveAnim_HidePlayerBackSprite();
        MoveAnim_LoadPlayerBackSprite(wBattleMon.species);
    } else {

        MoveAnim_ShowEnemyPicAtHome();
    }

    MoveAnim_DelayFramesAsm(3u);
}

static void MoveAnim_AnimationSlideMonUp(move_anim_ctx_t *ctx) {
    (void)ctx;
    if (hWhoseTurn != 0u) {
        MoveAnim_OffsetEnemyY(-8);
    } else {
        sMoveAnimShakeY = -8;
        Display_SetShakeOffset(sMoveAnimShakeX, sMoveAnimShakeY);
    }
    MoveAnim_DelayFramesAsm(4);
}

static void MoveAnim_DrawPlayerBackRowsShiftedDown(uint8_t shift_rows) {
    uint8_t ty, tx;
    MoveAnim_HidePlayerBackSprite();
    if (shift_rows >= 7u) return;

    for (ty = 0u; ty < (uint8_t)(7u - shift_rows); ty++) {
        uint8_t dst_ty = (uint8_t)(ty + shift_rows);
        for (tx = 0u; tx < 7u; tx++) {
            uint16_t row = (uint16_t)(MOVE_ANIM_PLAYER_BG_ROW + dst_ty);
            uint16_t col = (uint16_t)(MOVE_ANIM_PLAYER_BG_COL + tx);
            uint16_t idx = (uint16_t)(row * SCREEN_WIDTH + col);
            uint16_t sidx = (uint16_t)(row + 2u) * SCROLL_MAP_W + (uint16_t)(col + 2u) + Map_UiColOfs();
            uint8_t tile = (uint8_t)(MOVE_ANIM_PLAYER_BG_TILE_BASE + ty * 7u + tx);
            if (idx < SCREEN_AREA) {
                wTileMap[idx] = tile;
            }
            if (sidx < (SCROLL_MAP_W * SCROLL_MAP_H)) {
                gScrollTileMap[sidx] = tile;
            }
        }
    }
}

static void MoveAnim_DrawEnemyFrontRowsShiftedDown(uint8_t shift_rows) {
    uint8_t ty, tx;
    uint16_t start = BattleUI_GetEnemyOAMStart();
    const uint8_t base_x = (uint8_t)(96u + OAM_X_OFS);
    const uint8_t base_y = (uint8_t)(0u + OAM_Y_OFS);

    for (ty = 0u; ty < 7u; ty++) {
        for (tx = 0u; tx < 7u; tx++) {
            uint16_t idx = (uint16_t)(start + (uint16_t)ty * 7u + (uint16_t)tx);
            if (idx >= MAX_SPRITES) {
                return;
            }
            if ((uint8_t)(ty + shift_rows) < 7u) {
                uint8_t dst_ty = (uint8_t)(ty + shift_rows);
                wShadowOAM[idx].y = (uint8_t)(base_y + dst_ty * 8u);
                wShadowOAM[idx].x = (uint8_t)(base_x + tx * 8u);
                wShadowOAM[idx].tile = (uint8_t)(MOVE_ANIM_ENEMY_SPR_TILE_BASE + ty * 7u + tx);
                wShadowOAM[idx].flags = 0u;
            } else {
                wShadowOAM[idx].y = (uint8_t)(SCREEN_HEIGHT_PX + OAM_Y_OFS);
            }
        }
    }
}

static void MoveAnim_AnimationSlideMonDown(move_anim_ctx_t *ctx) {
    uint8_t step;
    (void)ctx;
    for (step = 0u; step < 7u; step++) {
        if (hWhoseTurn == 0u) {
            MoveAnim_DrawPlayerBackRowsShiftedDown(step);
        } else {
            MoveAnim_DrawEnemyFrontRowsShiftedDown(step);
        }
        MoveAnim_DelayFramesAsm(3u);
    }
    MoveAnim_AnimationHideMonPic(ctx);
}

static void MoveAnim_SlideMonOffOneStep(void) {
    if (hWhoseTurn == 0u) {
        uint8_t row, col;
        for (row = 0u; row < 7u; row++) {
            for (col = 0u; col < 8u; col++) {
                uint16_t r = (uint16_t)(MOVE_ANIM_PLAYER_BG_ROW + row);
                uint16_t c = (uint16_t)col;
                uint16_t idx = (uint16_t)(r * SCREEN_WIDTH + c);
                uint16_t sidx = (uint16_t)(r + 2u) * SCROLL_MAP_W + (uint16_t)(c + 2u) + Map_UiColOfs();
                uint8_t out = BLANK_TILE_SLOT;
                if (idx < SCREEN_AREA) {
                    uint8_t t = wTileMap[idx];
                    if (t >= MOVE_ANIM_PLAYER_BG_TILE_BASE &&
                        t < (uint8_t)(MOVE_ANIM_PLAYER_BG_TILE_BASE + 49u)) {
                        uint8_t i = (uint8_t)(t - MOVE_ANIM_PLAYER_BG_TILE_BASE);
                        uint8_t prow = (uint8_t)(i / 7u);
                        uint8_t pcol = (uint8_t)((i % 7u) + 1u);
                        if ((uint16_t)(pcol * 7u + prow) < 48u) {
                            out = (uint8_t)(MOVE_ANIM_PLAYER_BG_TILE_BASE +
                                            prow * 7u + pcol);
                        }
                    }
                    wTileMap[idx] = out;
                }
                if (sidx < (SCROLL_MAP_W * SCROLL_MAP_H)) gScrollTileMap[sidx] = out;
            }
        }
    } else {

        uint16_t i;
        uint16_t start = BattleUI_GetEnemyOAMStart();
        uint16_t end = BattleUI_GetEnemyOAMEnd();
        for (i = start; i <= end && i < MAX_SPRITES; i++) {
            if (wShadowOAM[i].y != 0u) {
                int16_t x = (int16_t)wShadowOAM[i].x + 8;
                if (x >= 168) {
                    wShadowOAM[i].y = (uint8_t)(SCREEN_HEIGHT_PX + OAM_Y_OFS);
                } else {
                    wShadowOAM[i].x = (uint8_t)x;
                }
            }
        }
    }
}

static int MoveAnim_SE_SlideMonOffStep(move_anim_ctx_t *ctx) {
    uint8_t steps, delay, half, flip, saved_turn;

    if (!ctx) return 1;
    half = (uint8_t)(ctx->active_se_id == 0xE5u);
    flip = (uint8_t)(ctx->active_se_id == 0xDBu);
    steps = half ? 4u : 8u;
    delay = half ? 4u : 3u;

    saved_turn = hWhoseTurn;
    if (flip) hWhoseTurn = (uint8_t)(hWhoseTurn ^ 1u);

    if (ctx->se_phase == 0u) {
        ctx->se_counter0 = steps;
        ctx->se_phase = 1u;
        if (hWhoseTurn != 0u) {

            MoveAnim_SetEnemyVisible(1u);
        }
    }

    if (ctx->se_phase == 1u) {
        MoveAnim_SlideMonOffOneStep();
        MoveAnim_DelayFramesAsm(delay);
        ctx->se_counter0 = (uint8_t)(ctx->se_counter0 - 1u);
        if (ctx->se_counter0 == 0u) {
            ctx->se_phase = 2u;
        }
        hWhoseTurn = saved_turn;
        return 0;
    }

    if (hWhoseTurn != 0u) {
        BattleUI_EnemySpriteCaptureState();
        if (!half) {
            MoveAnim_SetEnemyVisible(0u);
        }
    }
    if (half) {
        MoveAnim_DelayFramesAsm(3u);
    }
    hWhoseTurn = saved_turn;
    ctx->se_phase = 0u;
    return 1;
}

static void MoveAnim_AnimationSlideMonDownAndHide(move_anim_ctx_t *ctx) {
    MoveAnim_AnimationSlideMonDown(ctx);
    if (hWhoseTurn == 0u) {
        MoveAnim_HidePlayerBackSprite();
    } else {
        MoveAnim_SetEnemyVisible(0u);
    }
}

static void MoveAnim_AnimationHideMonPic(move_anim_ctx_t *ctx) {
    (void)ctx;
    if (hWhoseTurn == 0u) {
        MoveAnim_HidePlayerBackSprite();
    } else {
        MoveAnim_SetEnemyVisible(0u);
    }
}

static void MoveAnim_ShowEnemyPicAtHome(void) {
    MoveAnim_ResetEnemyOAMPoseCanonical();
    BattleUI_EnemySpriteCaptureState();
    MoveAnim_SetEnemyVisible(1u);
}

static void MoveAnim_AnimationShowMonPic(move_anim_ctx_t *ctx) {
    (void)ctx;
    if (hWhoseTurn == 0u) {
        MoveAnim_LoadPlayerBackSprite(wBattleMon.species);
    } else {
        MoveAnim_ShowEnemyPicAtHome();
    }
    MoveAnim_DelayFramesAsm(3u);
}

static void MoveAnim_AnimationHideEnemyMonPic(move_anim_ctx_t *ctx) {
    (void)ctx;
    MoveAnim_SetEnemyVisible(0u);
    MoveAnim_DelayFramesAsm(3u);
}

static void MoveAnim_AnimationShowEnemyMonPic(move_anim_ctx_t *ctx) {
    (void)ctx;
    MoveAnim_ShowEnemyPicAtHome();
    MoveAnim_DelayFramesAsm(3u);
}

static int MoveAnim_SE_BlinkMonStep(move_anim_ctx_t *ctx) {

    uint8_t saved_turn = hWhoseTurn;
    if (!ctx) return 1;
    if (ctx->active_se_id == 0xDEu) {
        hWhoseTurn = (uint8_t)(hWhoseTurn ^ 1u);
    }

    if (ctx->se_phase == 0u) {
        ctx->se_counter0 = 6u;
        ctx->se_phase = 1u;
    }

    if (ctx->se_phase == 1u) {
        MoveAnim_AnimationHideMonPic(ctx);
        MoveAnim_DelayFramesAsm(5u);
        ctx->se_phase = 2u;
        hWhoseTurn = saved_turn;
        return 0;
    }

    if (ctx->se_phase == 2u) {
        MoveAnim_AnimationShowMonPic(ctx);
        MoveAnim_DelayFramesAsm(5u);
        ctx->se_counter0 = (uint8_t)(ctx->se_counter0 - 1u);
        ctx->se_phase = (uint8_t)(ctx->se_counter0 == 0u ? 3u : 1u);
        hWhoseTurn = saved_turn;
        return 0;
    }

    ctx->se_phase = 0u;
    hWhoseTurn = saved_turn;
    return 1;
}

static void MoveAnim_ShakeBackAndForthDrawAt(uint8_t right) {
    if (hWhoseTurn == 0u) {
        MoveAnim_PlacePlayerBackSpriteAt(right ? 2u : 0u, 5u);
    } else {
        uint16_t i;
        uint16_t start = BattleUI_GetEnemyOAMStart();
        uint16_t end = BattleUI_GetEnemyOAMEnd();
        int8_t dx = right ? 8 : -8;
        MoveAnim_SetEnemyVisible(1u);
        MoveAnim_ResetEnemyOAMPoseCanonical();
        for (i = start; i <= end && i < MAX_SPRITES; i++) {
            if (wShadowOAM[i].y != 0u) {
                wShadowOAM[i].x = (uint8_t)(wShadowOAM[i].x + dx);
            }
        }
        BattleUI_EnemySpriteCaptureState();
    }
}

static void MoveAnim_ShakeBackAndForthClearAt(uint8_t right) {
    if (hWhoseTurn == 0u) {
        uint8_t row, col;
        uint8_t col0 = right ? 2u : 0u;
        for (row = 0u; row < 7u; row++) {
            for (col = 0u; col < 9u; col++) {
                uint16_t r = (uint16_t)(MOVE_ANIM_PLAYER_BG_ROW + row);
                uint16_t c = (uint16_t)(col0 + col);
                uint16_t idx = (uint16_t)(r * SCREEN_WIDTH + c);
                uint16_t sidx = (uint16_t)(r + 2u) * SCROLL_MAP_W + (uint16_t)(c + 2u) + Map_UiColOfs();
                if (c >= SCREEN_WIDTH) continue;
                if (idx < SCREEN_AREA) wTileMap[idx] = BLANK_TILE_SLOT;
                if (sidx < (SCROLL_MAP_W * SCROLL_MAP_H)) {
                    gScrollTileMap[sidx] = BLANK_TILE_SLOT;
                }
            }
        }
    } else {
        MoveAnim_SetEnemyVisible(0u);
    }
}

static int MoveAnim_SE_ShakeBackAndForthStep(move_anim_ctx_t *ctx) {

    if (!ctx) return 1;

    switch (ctx->se_phase) {
        case 0u:
            ctx->se_counter0 = 16u;
            ctx->se_index = 0u;
            ctx->se_phase = 1u;
            return 0;

        case 1u:
            MoveAnim_ShakeBackAndForthDrawAt(ctx->se_index);
            MoveAnim_DelayFramesAsm(3u);
            ctx->se_phase = 2u;
            return 0;

        case 2u:
            MoveAnim_ShakeBackAndForthClearAt(ctx->se_index);
            if (ctx->se_index == 0u) {
                ctx->se_index = 1u;
                ctx->se_phase = 1u;
                return 0;
            }
            ctx->se_index = 0u;
            ctx->se_counter0 = (uint8_t)(ctx->se_counter0 - 1u);
            if (ctx->se_counter0 == 0u) {
                ctx->se_phase = 0u;
                return 1;
            }
            ctx->se_phase = 1u;
            return 0;

        default:
            ctx->se_phase = 0u;
            return 1;
    }
}

static uint8_t sMoveAnimOnSgb = 0;

void MoveAnim_SetOnSgb(int on) { sMoveAnimOnSgb = on ? 1u : 0u; }

static void MoveAnim_SetAnimationBGPalette(uint8_t non_sgb, uint8_t sgb) {
    MoveAnim_SetBGP(sMoveAnimOnSgb ? sgb : non_sgb);
}

static void MoveAnim_AnimationDarkScreenPalette(move_anim_ctx_t *ctx) {
    (void)ctx;
    MoveAnim_SetAnimationBGPalette(0x6Fu, 0x6Fu);
}

static void MoveAnim_AnimationDarkenMonPalette(move_anim_ctx_t *ctx) {
    (void)ctx;

    MoveAnim_SetAnimationBGPalette(0xF9u, 0xF4u);
}

static void MoveAnim_AnimationResetScreenPalette(move_anim_ctx_t *ctx) {
    (void)ctx;
    MoveAnim_SetAnimationBGPalette(0xE4u, 0xE4u);
}

static void MoveAnim_AnimationLightScreenPalette(move_anim_ctx_t *ctx) {
    (void)ctx;
    MoveAnim_SetAnimationBGPalette(0x90u, 0x90u);
}

static void MoveAnim_AnimationDelay10(move_anim_ctx_t *ctx) {
    if (!ctx) return;

    MoveAnim_DelayFramesAsm(10);
}

static void MoveAnim_AnimationSquishMonPic(move_anim_ctx_t *ctx) {
    (void)ctx;
}

static void MoveAnim_AnimationBoundUpAndDown(move_anim_ctx_t *ctx) {
    uint8_t i;
    if (!ctx) return;

    for (i = 0u; i < 5u; i++) {
        MoveAnim_AnimationSlideMonDown(ctx);
    }
    MoveAnim_AnimationShowMonPic(ctx);
}

static void MoveAnim_AnimationLeavesFalling(move_anim_ctx_t *ctx) {
    (void)ctx;
}

static void MoveAnim_AnimationPetalsFalling(move_anim_ctx_t *ctx) {
    (void)ctx;
}

static void MoveAnim_AnimationWavyScreen(move_anim_ctx_t *ctx) {
    (void)ctx;

}

#undef MOVE_ANIM_SE_NOOP

static void MoveAnim_PlayCommandSound(move_anim_ctx_t *ctx, uint8_t sound_id_minus_one) {
    const move_sfx_data_t *move_sfx;
    uint8_t species;

    if (!ctx) return;

    ctx->anim_sound_id = sound_id_minus_one;

    if (sound_id_minus_one == MOVE_ANIM_NO_MOVE_MINUS_ONE) return;
    if (sound_id_minus_one >= MOVE_SFX_DATA_COUNT) return;

    move_sfx = &gMoveSfxData[sound_id_minus_one];
    if (Audio_IsMoveSfxDebug()) {
        printf("[SFXDBG] anim=0x%02X sound=0x%02X idx=%u sfx=%u pitch=0x%02X tempo=0x%02X turn=%u\n",
               (unsigned)ctx->animation_id,
               (unsigned)sound_id_minus_one,
               (unsigned)sound_id_minus_one,
               (unsigned)move_sfx->sfx_index,
               (unsigned)move_sfx->pitch_mod,
               (unsigned)move_sfx->tempo_mod,
               (unsigned)hWhoseTurn);
    }

    if (MoveAnim_IsCryMove(ctx->animation_id)) {
        if (hWhoseTurn == 0u) {
            species = wBattleMon.species;
        } else {

            species = wEnemyMon.species;
        }
        printf("[CRYDBG] anim=0x%02X turn=%u sound=0x%02X species=%u pitch=0x%02X tempo=0x%02X active=%d\n",
               (unsigned)ctx->animation_id,
               (unsigned)hWhoseTurn,
               (unsigned)sound_id_minus_one,
               (unsigned)species,
               (unsigned)move_sfx->pitch_mod,
               (unsigned)move_sfx->tempo_mod,
               Audio_IsCryPlaying());
        Audio_PlayCryModified(species, (int8_t)move_sfx->pitch_mod, move_sfx->tempo_mod);
        printf("[CRYDBG] post anim=0x%02X active=%d\n",
               (unsigned)ctx->animation_id,
               Audio_IsCryPlaying());
        return;
    }

    if (Audio_PlaySfxModified(move_sfx->sfx_index, (int8_t)move_sfx->pitch_mod,
                              move_sfx->tempo_mod)) return;

    if (move_sfx->sfx_index == SFX_DAMAGE) {
        Audio_PlaySFX_BattleHit(10u);
    } else if (move_sfx->sfx_index == SFX_NOT_VERY_EFFECTIVE) {
        Audio_PlaySFX_BattleHit(5u);
    } else if (move_sfx->sfx_index == SFX_SUPER_EFFECTIVE) {
        Audio_PlaySFX_BattleHit(20u);
    } else if (move_sfx->sfx_index == SFX_FAINT_FALL) {
        Audio_PlaySFX_FaintFallOnly();
    }
}

static uint8_t MoveAnim_IsCryMove(uint8_t animation_id) {
    if (animation_id == MOVE_GROWL) return 1u;
    if (animation_id == MOVE_ROAR) return 1u;
    return 0u;
}

static uint8_t MoveAnim_GetSubanimationTransform1(uint8_t subanim_type) {
    if (hWhoseTurn != 0u) return subanim_type;
    return MOVE_ANIM_SUBANIMTYPE_NORMAL;
}

static uint8_t MoveAnim_GetSubanimationTransform2(void) {
    if (hWhoseTurn == 0u) return MOVE_ANIM_SUBANIMTYPE_HFLIP;
    return MOVE_ANIM_SUBANIMTYPE_NORMAL;
}

static uint8_t MoveAnim_HVFlipFlags(uint8_t flags) {
    if (flags == 0u) return (uint8_t)(OAM_FLAG_FLIP_Y | OAM_FLAG_FLIP_X);
    if (flags == OAM_FLAG_FLIP_X) return OAM_FLAG_FLIP_Y;
    if (flags == OAM_FLAG_FLIP_Y) return OAM_FLAG_FLIP_X;
    return 0;
}

static uint8_t MoveAnim_HFlipFlags(uint8_t flags) {
    if ((flags & OAM_FLAG_FLIP_X) != 0u) {
        return (uint8_t)(flags & (uint8_t)~OAM_FLAG_FLIP_X);
    }
    return (uint8_t)(flags | OAM_FLAG_FLIP_X);
}

static uint8_t MoveAnim_GetFrameBlockYOffset(const move_anim_frameblock_sprite_t *sprite) {
    if (!sprite) return 0;

    return (uint8_t)(sprite->x_offset * 8u + sprite->x_offset_flip);
}

static uint8_t MoveAnim_GetFrameBlockXOffset(const move_anim_frameblock_sprite_t *sprite) {
    if (!sprite) return 0;

    return (uint8_t)(sprite->y_offset * 8u + sprite->y_offset_flip);
}

static void MoveAnim_DelayFramesAsm(uint8_t frames) {
    hFrameCounter = (uint8_t)(hFrameCounter + frames);
    if (sMoveAnimExecCtx && frames != 0u) {
        uint8_t ticks = MoveAnim_ConvertAsmFramesToEngineTicks(sMoveAnimExecCtx, frames);

        sMoveAnimExecCtx->wait_frames = (uint8_t)(sMoveAnimExecCtx->wait_frames + ticks);
    }

}

static uint8_t MoveAnim_ConvertAsmFramesToEngineTicks(move_anim_ctx_t *ctx, uint8_t asm_frames) {
    (void)ctx;

    return asm_frames;
}

static void MoveAnim_AnimationCleanOAM(void) {

    MoveAnim_DelayFramesAsm(1);
    if (sMoveAnimExecCtx) {
        sMoveAnimExecCtx->pending_oam_clean = 2u;
        return;
    }
    MoveAnim_ClearAnimationOAM();
}

static void MoveAnim_ClearAnimationOAM(void) {
    uint16_t i;
    uint16_t oam_start = BattleUI_GetAnimOAMStart();
    uint16_t oam_end = BattleUI_GetAnimOAMEnd();
    if (oam_end >= MAX_SPRITES) {
        oam_end = (uint16_t)(MAX_SPRITES - 1u);
    }
    for (i = oam_start; i <= oam_end; i++) {
        wShadowOAM[i].y = 0u;
    }
}

static void MoveAnim_ResetVisualState(void) {

    uint8_t enemy_should_hide =
        (uint8_t)((wEnemyBattleStatus1 & (1u << BSTAT1_INVULNERABLE)) != 0u);
    sMoveAnimShakeX = 0;
    sMoveAnimShakeY = 0;
    sMoveAnimShakeToggle = 0;
    sMoveAnimWavyActive = 0u;
    sMoveAnimWavyPhase = 0u;
    Display_SetWavyPhase(0, 0);

    if (!sMoveAnimTradeHooks) {
        MoveAnim_LoadEnemyFrontSprite(wEnemyMon.species);
    }

    MoveAnim_ResetEnemyOAMPoseCanonical();
    BattleUI_EnemySpriteCaptureState();
    if (sMoveAnimTradeHooks) {

        BattleUI_EnemySpriteSetVisible(0u);
    } else {
        BattleUI_EnemySpriteSetVisible(1u);
        if (enemy_should_hide) {
            printf("[DIGDBG] reset_visual keep_enemy_hidden e_b1=0x%02X\n", wEnemyBattleStatus1);
            BattleUI_EnemySpriteSetVisible(0u);
        }
    }
    Display_SetShakeOffset(0, 0);
}

static void MoveAnim_ResetEnemyOAMPoseCanonical(void) {
    uint16_t start = BattleUI_GetEnemyOAMStart();

    const uint8_t base_x = (uint8_t)(96u + OAM_X_OFS);
    const uint8_t base_y = (uint8_t)(0u + OAM_Y_OFS);
    for (uint8_t ty = 0u; ty < 7u; ty++) {
        for (uint8_t tx = 0u; tx < 7u; tx++) {
            uint16_t idx = (uint16_t)(start + (uint16_t)ty * 7u + (uint16_t)tx);
            if (idx >= MAX_SPRITES) return;
            wShadowOAM[idx].y = (uint8_t)(base_y + ty * 8u);
            wShadowOAM[idx].x = (uint8_t)(base_x + tx * 8u);
            wShadowOAM[idx].tile = (uint8_t)(MOVE_ANIM_ENEMY_SPR_TILE_BASE + ty * 7u + tx);
            wShadowOAM[idx].flags = 0u;
        }
    }
}

static void MoveAnim_EnemyPicFollowBGP(uint8_t on) {
    uint16_t i, first = BattleUI_GetEnemyOAMStart(), last = BattleUI_GetEnemyOAMEnd();
    for (i = first; i <= last; i++) {
        if (on) wShadowOAM[i].flags |= OAM_FLAG_PALETTE;
        else    wShadowOAM[i].flags &= (uint8_t)~OAM_FLAG_PALETTE;
    }
    if (on) Display_SetOBP1(sMoveAnimCurrentBGP);
    sMoveAnimEnemyPicOnOBP1 = on ? 1u : 0u;
}

static void MoveAnim_UpdatePerFrameEffects(move_anim_ctx_t *ctx) {
    (void)ctx;
    MoveAnim_EnemyPicFollowBGP(1u);
    if (sMoveAnimWavyActive) {
        Display_SetWavyPhase(1, sMoveAnimWavyPhase);
        sMoveAnimWavyPhase = (uint8_t)((sMoveAnimWavyPhase + 1u) & 31u);
    }
}

static int MoveAnim_SE_WavyScreenStep(move_anim_ctx_t *ctx) {
    if (!ctx) return 1;

    switch (ctx->se_phase) {
        case 0:
            sMoveAnimWavyActive = 1u;
            sMoveAnimWavyPhase = 0u;
            Display_SetWavyPhase(1, 0);
            ctx->se_counter0 = 128u;
            ctx->se_phase = 1u;
            MoveAnim_DelayFramesAsm(3u);
            MoveAnim_DelayFramesAsm(3u);
            return 0;

        case 1:
            if (ctx->se_counter0 == 0u) {
                ctx->se_phase = 2u;
                return 0;
            }
            ctx->se_counter0 = (uint8_t)(ctx->se_counter0 - 1u);
            MoveAnim_DelayFramesAsm(1u);
            return 0;

        case 2:
            sMoveAnimWavyActive = 0u;
            Display_SetWavyPhase(0, 0);
            ctx->se_phase = 3u;
            MoveAnim_DelayFramesAsm(3u);
            MoveAnim_DelayFramesAsm(3u);
            MoveAnim_DelayFramesAsm(3u);
            return 0;

        default:
            ctx->se_phase = 0u;
            return 1;
    }
}

static int MoveAnim_SE_SlideMonUpStep(move_anim_ctx_t *ctx) {
    if (!ctx) return 1;
    switch (ctx->se_phase) {
        case 0u:

            ctx->se_counter0 = 7u;
            ctx->se_phase = 1u;
            return 0;
        case 1u:

            ctx->se_counter0 = (uint8_t)(ctx->se_counter0 - 1u);
            if (hWhoseTurn == 0u) {
                MoveAnim_DrawPlayerBackRowsShiftedDown(ctx->se_counter0);
            } else {
                MoveAnim_DrawEnemyFrontRowsShiftedDown(ctx->se_counter0);
            }
            MoveAnim_DelayFramesAsm(2u);
            if (ctx->se_counter0 == 0u) {
                ctx->se_phase = 0u;
                return 1;
            }
            return 0;
        default:
            ctx->se_phase = 0u;
            return 1;
    }
}

static int MoveAnim_SE_SlideMonDownStep(move_anim_ctx_t *ctx) {
    if (!ctx) return 1;
    switch (ctx->se_phase) {
        case 0u:
            ctx->se_counter0 = 0u;
            ctx->se_phase = 1u;
            return 0;
        case 1u:
            if (hWhoseTurn == 0u) {
                MoveAnim_DrawPlayerBackRowsShiftedDown(ctx->se_counter0);
            } else {
                MoveAnim_DrawEnemyFrontRowsShiftedDown(ctx->se_counter0);
            }
            MoveAnim_DelayFramesAsm(3u);
            ctx->se_counter0 = (uint8_t)(ctx->se_counter0 + 1u);
            if (ctx->se_counter0 >= 7u) {
                MoveAnim_AnimationHideMonPic(ctx);
                ctx->se_phase = 0u;
                return 1;
            }
            return 0;
        default:
            ctx->se_phase = 0u;
            return 1;
    }
}

static void MoveAnim_SetEnemyVisible(uint8_t visible) {
    BattleUI_EnemySpriteSetVisible(visible);
}

static void MoveAnim_OffsetEnemyY(int8_t delta) {
    BattleUI_EnemySpriteOffsetY(delta);
}

static void MoveAnim_LoadEnemyFrontSprite(uint8_t species) {
    uint8_t dex = gSpeciesToDex[species];
    uint8_t i;

    if (BattleUI_EnemyDrawnAsGhost()) {
        for (i = 0; i < POKEMON_FRONT_CANVAS_TILES; i++)
            Display_LoadSpriteTile((uint8_t)(MOVE_ANIM_ENEMY_SPR_TILE_BASE + i),
                                   kGhostFrontSprite[i]);
        return;
    }

    {
        int gdex = Species_Dex(species);
        if (gdex >= 152 && gdex < CRYSTAL_MON_COUNT) {
            const crystal_pic_t *p = &gCrystalMonPic[gdex];
            for (i = 0; i < POKEMON_FRONT_CANVAS_TILES; i++) {
                const uint8_t *tile = SpriteMod_GetFrontTile(species, i);
                Display_LoadSpriteTile((uint8_t)(MOVE_ANIM_ENEMY_SPR_TILE_BASE + i),
                                       tile ? tile
                                            : gCrystalPicTiles[p->tile_base + i]);
            }
            return;
        }
    }
    if ((dex == 0u || dex > 151u) && SpriteMod_GetFrontTile(species, 0) == NULL) return;
    for (i = 0; i < POKEMON_FRONT_CANVAS_TILES; i++) {
        const uint8_t *tile = SpriteMod_GetFrontTile(species, i);
        Display_LoadSpriteTile((uint8_t)(MOVE_ANIM_ENEMY_SPR_TILE_BASE + i),
                               tile ? tile : gPokemonFrontSprite[dex][i]);
    }
}

static void MoveAnim_LoadPlayerBackSprite(uint8_t species) {
    uint8_t dex = gSpeciesToDex[species];
    uint8_t ty;
    {
        int gdex = Species_Dex(species);
        if (gdex >= 152 && gdex < CRYSTAL_MON_COUNT) {
            for (ty = 0; ty < POKEMON_BACK_TILES; ty++) {
                const uint8_t *tile = SpriteMod_GetBackTile(species, ty);
                Display_LoadTile((uint8_t)(MOVE_ANIM_PLAYER_BG_TILE_BASE + ty),
                                 tile ? tile : gCrystalMonBackPic[gdex][ty]);
            }
            return;
        }
    }
    if ((dex == 0u || dex > 151u) && SpriteMod_GetBackTile(species, 0) == NULL) return;

    for (ty = 0; ty < POKEMON_BACK_TILES; ty++) {
        const uint8_t *tile = SpriteMod_GetBackTile(species, ty);
        Display_LoadTile((uint8_t)(MOVE_ANIM_PLAYER_BG_TILE_BASE + ty),
                         tile ? tile : gPokemonBackSprite[dex][ty]);
    }

    for (ty = 0; ty < 7u; ty++) {
        uint8_t tx;
        for (tx = 0; tx < 7u; tx++) {
            uint16_t row = (uint16_t)(MOVE_ANIM_PLAYER_BG_ROW + ty);
            uint16_t col = (uint16_t)(MOVE_ANIM_PLAYER_BG_COL + tx);
            uint16_t idx = (uint16_t)(row * SCREEN_WIDTH + col);
            uint16_t sidx = (uint16_t)(row + 2u) * SCROLL_MAP_W + (uint16_t)(col + 2u) + Map_UiColOfs();
            uint8_t tile = (uint8_t)(MOVE_ANIM_PLAYER_BG_TILE_BASE + ty * 7u + tx);
            if (idx < SCREEN_AREA) {
                wTileMap[idx] = tile;
            }
            if (sidx < (SCROLL_MAP_W * SCROLL_MAP_H)) {
                gScrollTileMap[sidx] = tile;
            }
        }
    }
}

static void MoveAnim_HidePlayerBackSprite(void) {
    MoveAnim_ClearPlayerBackSpriteAt(MOVE_ANIM_PLAYER_BG_COL, MOVE_ANIM_PLAYER_BG_ROW);
}

static void MoveAnim_ClearPlayerBackSpriteAt(uint8_t col, uint8_t row) {
    uint8_t ty;
    for (ty = 0; ty < 7u; ty++) {
        uint8_t tx;
        for (tx = 0; tx < 7u; tx++) {
            uint16_t r = (uint16_t)(row + ty);
            uint16_t c = (uint16_t)(col + tx);
            uint16_t idx = (uint16_t)(r * SCREEN_WIDTH + c);
            uint16_t sidx = (uint16_t)(r + 2u) * SCROLL_MAP_W + (uint16_t)(c + 2u) + Map_UiColOfs();
            if (idx < SCREEN_AREA) wTileMap[idx] = BLANK_TILE_SLOT;
            if (sidx < (SCROLL_MAP_W * SCROLL_MAP_H)) gScrollTileMap[sidx] = BLANK_TILE_SLOT;
        }
    }
}

static void MoveAnim_PlacePlayerBackSpriteAt(uint8_t col, uint8_t row) {
    uint8_t ty;
    for (ty = 0; ty < 7u; ty++) {
        uint8_t tx;
        for (tx = 0; tx < 7u; tx++) {
            uint16_t r = (uint16_t)(row + ty);
            uint16_t c = (uint16_t)(col + tx);
            uint16_t idx = (uint16_t)(r * SCREEN_WIDTH + c);
            uint16_t sidx = (uint16_t)(r + 2u) * SCROLL_MAP_W + (uint16_t)(c + 2u) + Map_UiColOfs();
            uint8_t tile = (uint8_t)(MOVE_ANIM_PLAYER_BG_TILE_BASE + ty * 7u + tx);
            if (idx < SCREEN_AREA) wTileMap[idx] = tile;
            if (sidx < (SCROLL_MAP_W * SCROLL_MAP_H)) gScrollTileMap[sidx] = tile;
        }
    }
}

static int MoveAnim_SE_FlashScreenLongStep(move_anim_ctx_t *ctx) {
    uint8_t pal;
    if (!ctx) return 1;

    if (ctx->se_phase == 0u) {
        ctx->se_counter0 = 3u;
        ctx->se_index = 0u;
        ctx->se_phase = 1u;
    }

    pal = sMoveAnimFlashScreenLongPals[ctx->se_index];
    MoveAnim_SetBGP(pal);
    if (ctx->se_counter0 == 3u) {
        MoveAnim_DelayFramesAsm(2u);
    } else {
        MoveAnim_DelayFramesAsm(1u);
    }

    ctx->se_index = (uint8_t)(ctx->se_index + 1u);
    if (ctx->se_index >= 12u) {
        ctx->se_index = 0u;
        if (ctx->se_counter0 > 0u) {
            ctx->se_counter0 = (uint8_t)(ctx->se_counter0 - 1u);
        }
        if (ctx->se_counter0 == 0u) {
            ctx->se_phase = 0u;
            return 1;
        }
    }
    return 0;
}

static int MoveAnim_SE_SpiralBallsInwardStep(move_anim_ctx_t *ctx) {
    int16_t base_y;
    int16_t base_x;
    uint8_t i;
    if (!ctx) return 1;

    if (ctx->se_phase == 0u) {

        MoveAnim_LoadTileset(ctx, 0u);
        ctx->se_index = 0u;
        ctx->se_phase = 1u;
        return 0;
    }

    base_y = (hWhoseTurn == 0u) ? 0 : -40;
    base_x = (hWhoseTurn == 0u) ? 0 : 80;

    if (gMoveAnimSpiralCoords[(uint16_t)ctx->se_index * 2u] == 0xFFu) {
        ctx->se_phase = 2u;
    }

    if (ctx->se_phase == 1u) {
        for (i = 0u; i < 3u; i++) {
            uint16_t pos = (uint16_t)(ctx->se_index + i) * 2u;
            uint8_t yoff = gMoveAnimSpiralCoords[pos];
            uint8_t xoff;
            int16_t y;
            int16_t x;

            if (yoff == 0xFFu) {
                ctx->se_phase = 2u;
                break;
            }
            xoff = gMoveAnimSpiralCoords[pos + 1u];
            y = (int16_t)((int16_t)(int8_t)yoff + base_y);
            x = (int16_t)((int16_t)(int8_t)xoff + base_x);
            if (i < MAX_SPRITES) {
                if (y < 0) y = 0;
                if (y > 255) y = 255;
                if (x < 0) x = 0;
                if (x > 255) x = 255;
                wShadowOAM[i].y = (uint8_t)y;
                wShadowOAM[i].x = (uint8_t)x;
                wShadowOAM[i].tile = 0x7Au;
                wShadowOAM[i].flags = 0u;
            }
        }
        if (ctx->se_phase == 1u) {
            MoveAnim_DelayFramesAsm(5u);
            ctx->se_index = (uint8_t)(ctx->se_index + 1u);
            return 0;
        }
    }

    if (ctx->se_phase == 2u) {

        MoveAnim_AnimationCleanOAM();
        ctx->se_phase = 3u;
        return 0;
    }

    if (!MoveAnim_FlashScreenPhase(ctx, &ctx->se_counter0)) {
        return 0;
    }
    ctx->se_phase = 0u;
    return 1;
}

static int MoveAnim_SE_WaterDropletsEverywhereStep(move_anim_ctx_t *ctx) {
    uint8_t base_y;
    uint16_t oam;
    if (!ctx) return 1;

    if (ctx->se_phase == 0u) {

        MoveAnim_LoadTileset(ctx, 0u);
        ctx->se_counter0 = 32u;
        ctx->se_index = 0u;
        ctx->se_phase = 1u;
        sMoveAnimWaterBaseX = (uint8_t)-16;
        return 0;
    }

    if (ctx->se_phase == 2u) {

        MoveAnim_DelayFramesAsm(1u);
        ctx->se_phase = 1u;

        if (ctx->se_index == 0u) {
            ctx->se_index = 1u;
            return 0;
        }
        ctx->se_index = 0u;
        if (ctx->se_counter0 > 0u) {
            ctx->se_counter0 = (uint8_t)(ctx->se_counter0 - 1u);
        }
        if (ctx->se_counter0 == 0u) {
            ctx->se_phase = 0u;
            return 1;
        }
        return 0;
    }

    base_y = (ctx->se_index == 0u) ? 16u : 24u;
    oam = 0u;
    while (1) {
        uint8_t x;
        sMoveAnimWaterBaseX = (uint8_t)(sMoveAnimWaterBaseX + 27u);
        x = sMoveAnimWaterBaseX;

        if (oam < MAX_SPRITES) {
            wShadowOAM[oam].y = base_y;
            wShadowOAM[oam].x = x;
            wShadowOAM[oam].tile = 0x71u;
            wShadowOAM[oam].flags = 0u;
        }
        oam++;

        if (x < 144u) {
            continue;
        }
        sMoveAnimWaterBaseX = (uint8_t)(sMoveAnimWaterBaseX - 168u);
        base_y = (uint8_t)(base_y + 16u);
        if (base_y < 112u) {
            continue;
        }
        break;
    }

    MoveAnim_AnimationCleanOAM();
    ctx->se_phase = 2u;
    return 0;
}

static void MoveAnim_SE_FallingObjectsInit(uint8_t count, uint8_t tile) {
    uint8_t i;
    sMoveAnimFallingCount = count;
    sMoveAnimFallingTile = tile;
    if (sMoveAnimFallingCount > 20u) {
        sMoveAnimFallingCount = 20u;
    }

    for (i = 0u; i < sMoveAnimFallingCount && i < MAX_SPRITES; i++) {

        wShadowOAM[i].y = (uint8_t)((i + 1u) * 8u);
        wShadowOAM[i].x = gMoveAnimFallingInitialX[i];
        wShadowOAM[i].tile = sMoveAnimFallingTile;
        wShadowOAM[i].flags = 0u;
        sMoveAnimFallingMove[i] = gMoveAnimFallingInitialMove[i];
    }

    if (sMoveAnimFallingCount > 0u) {
        wShadowOAM[0].y = 0u;
    }
}

static uint8_t MoveAnim_SE_FallingObjectsUpdateMovementByte(uint8_t movement) {
    uint8_t a = (uint8_t)(movement + 1u);
    if ((a & 0x7Fu) == 9u) {
        a = (uint8_t)(((a & 0x80u) ^ 0x80u));
    }
    return a;
}

static void MoveAnim_SE_FallingObjectsUpdateOAM(uint8_t idx, uint8_t movement) {
    uint8_t y;
    uint8_t delta;
    if (idx >= sMoveAnimFallingCount || idx >= MAX_SPRITES) return;

    y = (uint8_t)(wShadowOAM[idx].y + 2u);
    if (y >= 112u) {
        y = (uint8_t)(SCREEN_HEIGHT_PX + OAM_Y_OFS);
    }
    wShadowOAM[idx].y = y;

    {

        delta = gMoveAnimFallingDeltaX[movement & 0x7Fu];
    }
    if ((movement & 0x80u) != 0u) {
        wShadowOAM[idx].x = (uint8_t)(wShadowOAM[idx].x - delta);
        wShadowOAM[idx].flags = OAM_FLAG_FLIP_X;
    } else {
        wShadowOAM[idx].x = (uint8_t)(wShadowOAM[idx].x + delta);
        wShadowOAM[idx].flags = 0u;
    }
}

static int MoveAnim_SE_LeavesFallingStep(move_anim_ctx_t *ctx) {
    uint8_t i;
    if (!ctx) return 1;

    if (ctx->se_phase == 0u) {

        MoveAnim_LoadTileset(ctx, 1u);
        ctx->se_phase = 9u;
        return 0;
    }

    if (ctx->se_phase == 9u) {
        MoveAnim_SE_FallingObjectsInit(3u, 0x37u);
        ctx->se_phase = 1u;
    }

    for (i = 0u; i < sMoveAnimFallingCount; i++) {
        uint8_t movement = sMoveAnimFallingMove[i];
        movement = MoveAnim_SE_FallingObjectsUpdateMovementByte(movement);
        MoveAnim_SE_FallingObjectsUpdateOAM(i, movement);
        sMoveAnimFallingMove[i] = movement;
    }

    MoveAnim_DelayFramesAsm(3u);
    if (sMoveAnimFallingCount > 0u && wShadowOAM[0].y == 104u) {
        ctx->se_phase = 0u;
        return 1;
    }
    return 0;
}

static int MoveAnim_SE_PetalsFallingStep(move_anim_ctx_t *ctx) {
    uint8_t i;
    if (!ctx) return 1;

    if (ctx->se_phase == 0u) {

        MoveAnim_LoadTileset(ctx, 1u);
        ctx->se_phase = 9u;
        return 0;
    }

    if (ctx->se_phase == 2u) {

        MoveAnim_ClearAnimationOAM();
        ctx->se_phase = 0u;
        return 1;
    }

    if (ctx->se_phase == 9u) {
        MoveAnim_SE_FallingObjectsInit(20u, 0x71u);
        ctx->se_phase = 1u;
    }

    for (i = 0u; i < sMoveAnimFallingCount; i++) {
        uint8_t movement = sMoveAnimFallingMove[i];
        movement = MoveAnim_SE_FallingObjectsUpdateMovementByte(movement);
        MoveAnim_SE_FallingObjectsUpdateOAM(i, movement);
        sMoveAnimFallingMove[i] = movement;
    }

    MoveAnim_DelayFramesAsm(3u);
    if (sMoveAnimFallingCount > 0u && wShadowOAM[0].y == 104u) {

        ctx->se_phase = 2u;
        return 0;
    }
    return 0;
}

static void MoveAnim_SE_SquishCopyRowLeft(uint8_t row, uint8_t start_col) {
    uint8_t k;
    if (row >= SCREEN_HEIGHT) return;
    for (k = 0u; k < 3u; k++) {
        uint8_t src_col = (uint8_t)(start_col + k);
        uint8_t dst_col;
        if (src_col >= SCREEN_WIDTH) continue;
        if (src_col == 0u) continue;
        dst_col = (uint8_t)(src_col - 1u);
        wTileMap[(uint16_t)row * SCREEN_WIDTH + dst_col] =
            wTileMap[(uint16_t)row * SCREEN_WIDTH + src_col];
    }
    if ((uint8_t)(start_col + 2u) < SCREEN_WIDTH) {
        wTileMap[(uint16_t)row * SCREEN_WIDTH + (uint8_t)(start_col + 2u)] = (uint8_t)' ';
    }
}

static void MoveAnim_SE_SquishCopyRowRight(uint8_t row, uint8_t start_col) {
    int8_t k;
    if (row >= SCREEN_HEIGHT) return;
    for (k = 0; k < 3; k++) {
        int16_t src_col = (int16_t)start_col - k;
        int16_t dst_col = src_col + 1;
        if (src_col < 0 || src_col >= SCREEN_WIDTH) continue;
        if (dst_col < 0 || dst_col >= SCREEN_WIDTH) continue;
        wTileMap[(uint16_t)row * SCREEN_WIDTH + (uint16_t)dst_col] =
            wTileMap[(uint16_t)row * SCREEN_WIDTH + (uint16_t)src_col];
    }
    if (start_col >= 2u) {
        wTileMap[(uint16_t)row * SCREEN_WIDTH + (uint8_t)(start_col - 2u)] = (uint8_t)' ';
    }
}

static void MoveAnim_SE_SquishPass(uint8_t direction_right) {
    uint8_t row;
    uint8_t start_row = (hWhoseTurn == 0u) ? 5u : 0u;
    uint8_t left_start_col = (hWhoseTurn == 0u) ? 5u : 16u;
    uint8_t right_start_col = (hWhoseTurn == 0u) ? 3u : 14u;

    for (row = 0u; row < 7u; row++) {
        uint8_t y = (uint8_t)(start_row + row);
        if (y >= SCREEN_HEIGHT) break;
        if (!direction_right) {
            MoveAnim_SE_SquishCopyRowLeft(y, left_start_col);
        } else {
            MoveAnim_SE_SquishCopyRowRight(y, right_start_col);
        }
    }
}

static int MoveAnim_SE_SquishMonPicStep(move_anim_ctx_t *ctx) {
    if (!ctx) return 1;

    if (ctx->se_phase == 0u) {
        ctx->se_counter0 = 4u;
        ctx->se_index = 0u;
        ctx->se_phase = 1u;
    }

    if (ctx->se_phase == 1u) {
        MoveAnim_SE_SquishPass((uint8_t)(ctx->se_index != 0u));
        MoveAnim_DelayFramesAsm(3u);

        if (ctx->se_index == 0u) {
            ctx->se_index = 1u;
            return 0;
        }

        ctx->se_index = 0u;
        if (ctx->se_counter0 > 0u) {
            ctx->se_counter0 = (uint8_t)(ctx->se_counter0 - 1u);
        }
        if (ctx->se_counter0 == 0u) {
            ctx->se_phase = 2u;
        }
        return 0;
    }

    MoveAnim_AnimationHideMonPic(ctx);

    MoveAnim_DelayFramesAsm(1u);
    ctx->se_phase = 0u;
    return 1;
}

static void MoveAnim_SE_InitShootingBalls(void) {
    uint8_t i;
    uint8_t y = sMoveAnimShootBaseY;

    for (i = 0u; i < sMoveAnimShootBallCount && i < MAX_SPRITES; i++) {
        y = (uint8_t)(y + 8u);
        wShadowOAM[i].y = y;
        wShadowOAM[i].x = sMoveAnimShootBaseX;
        wShadowOAM[i].tile = 0x7Au;
        wShadowOAM[i].flags = 0u;
    }
}

static void MoveAnim_SE_UpdateShootingBalls(move_anim_ctx_t *ctx) {
    uint8_t i;
    uint8_t top_y;

    if (!ctx) return;
    top_y = (uint8_t)(sMoveAnimShootBaseY + 8u);

    for (i = 0u; i < sMoveAnimShootBallCount && i < MAX_SPRITES; i++) {
        uint8_t y = wShadowOAM[i].y;
        if (y == top_y) {
            wShadowOAM[i].y = 0u;
            if (ctx->se_counter0 != 0u) {
                ctx->se_counter0 = (uint8_t)(ctx->se_counter0 - 1u);
            }
        } else {
            wShadowOAM[i].y = (uint8_t)(y - 4u);
        }
    }
}

static int MoveAnim_SE_ShootBallsUpwardStep(move_anim_ctx_t *ctx) {
    if (!ctx) return 1;

    if (ctx->se_phase == 0u) {

        MoveAnim_LoadTileset(ctx, 0u);
        ctx->se_phase = 9u;
        return 0;
    }

    if (ctx->se_phase == 9u) {
        if (hWhoseTurn == 0u) {
            sMoveAnimShootBaseY = 6u * 8u;
            sMoveAnimShootBaseX = 5u * 8u;
        } else {
            sMoveAnimShootBaseY = 0u;
            sMoveAnimShootBaseX = 16u * 8u;
        }
        sMoveAnimShootBallCount = 5u;
        sMoveAnimShootDelay = 1u;
        ctx->se_counter0 = sMoveAnimShootBallCount;
        MoveAnim_SE_InitShootingBalls();
        MoveAnim_DelayFramesAsm(1u);
        ctx->se_phase = 1u;
        return 0;
    }

    if (ctx->se_counter0 == 0u) {
        MoveAnim_AnimationCleanOAM();
        ctx->se_phase = 0u;
        return 1;
    }

    MoveAnim_SE_UpdateShootingBalls(ctx);
    MoveAnim_DelayFramesAsm(sMoveAnimShootDelay);
    return 0;
}

static int MoveAnim_SE_ShootManyBallsUpwardStep(move_anim_ctx_t *ctx) {
    if (!ctx) return 1;

    if (ctx->se_phase == 0u) {
        ctx->se_index = 0u;
        if (hWhoseTurn == 0u) {
            sMoveAnimShootBaseY = 0x50u;
        } else {
            sMoveAnimShootBaseY = 0x28u;
        }
        ctx->se_phase = 1u;
    }

    if (ctx->se_phase == 1u) {

        uint8_t x = (hWhoseTurn == 0u)
            ? sMoveAnimUpwardBallsXPlayerTurn[ctx->se_index]
            : sMoveAnimUpwardBallsXEnemyTurn[ctx->se_index];
        if (x == 0xFFu) {
            MoveAnim_AnimationCleanOAM();
            ctx->se_phase = 0u;
            return 1;
        }

        MoveAnim_LoadTileset(ctx, 0u);
        sMoveAnimShootBaseX = x;
        ctx->se_phase = 9u;
        return 0;
    }

    if (ctx->se_phase == 9u) {
        sMoveAnimShootBallCount = 4u;
        sMoveAnimShootDelay = 1u;
        ctx->se_counter0 = sMoveAnimShootBallCount;
        MoveAnim_SE_InitShootingBalls();
        MoveAnim_DelayFramesAsm(1u);
        ctx->se_phase = 2u;
        return 0;
    }

    MoveAnim_SE_UpdateShootingBalls(ctx);
    MoveAnim_DelayFramesAsm(sMoveAnimShootDelay);

    if (ctx->se_counter0 == 0u) {

        ctx->se_index = (uint8_t)(ctx->se_index + 1u);
        ctx->se_phase = 1u;
    }
    return 0;
}

static int MoveAnim_SE_ShakeEnemyHUDStep(move_anim_ctx_t *ctx) {
    if (!ctx) return 1;

    switch (ctx->se_phase) {
        case 0u:
            sMoveAnimShakeX = 0;
            Display_SetShakeOffset(sMoveAnimShakeX, sMoveAnimShakeY);
            ctx->se_counter0 = 8u;
            ctx->se_phase = 1u;
            MoveAnim_DelayFramesAsm(3u);
            return 0;
        case 1u:
            sMoveAnimShakeX = 2;
            Display_SetShakeOffset(sMoveAnimShakeX, sMoveAnimShakeY);
            ctx->se_phase = 2u;
            MoveAnim_DelayFramesAsm(2u);
            return 0;
        case 2u:
            sMoveAnimShakeX = -2;
            Display_SetShakeOffset(sMoveAnimShakeX, sMoveAnimShakeY);
            if (ctx->se_counter0 > 0u) {
                ctx->se_counter0 = (uint8_t)(ctx->se_counter0 - 1u);
            }
            if (ctx->se_counter0 == 0u) {
                ctx->se_phase = 3u;
            } else {
                ctx->se_phase = 1u;
            }
            MoveAnim_DelayFramesAsm(2u);
            return 0;
        case 3u:
            sMoveAnimShakeX = 0;
            Display_SetShakeOffset(sMoveAnimShakeX, sMoveAnimShakeY);

            MoveAnim_AnimationShowMonPic(ctx);
            MoveAnim_DelayFramesAsm(3u);
            ctx->se_phase = 0u;
            return 1;
        default:
            ctx->se_phase = 0u;
            return 1;
    }
}

static int MoveAnim_SE_SlideMonDownAndHideStep(move_anim_ctx_t *ctx) {
    uint8_t row, col;
    if (!ctx) return 1;

    switch (ctx->se_phase) {
        case 0u:
            ctx->se_counter0 = 0u;
            ctx->se_phase = 1u;
            return 0;
        case 1u:
            if (hWhoseTurn == 0u) {

                uint8_t top = (ctx->se_counter0 == 0u) ? MOVE_ANIM_PLAYER_BG_ROW : (uint8_t)(MOVE_ANIM_PLAYER_BG_ROW + 1u);
                uint8_t bottom = (uint8_t)(MOVE_ANIM_PLAYER_BG_ROW + 6u);
                if (top > bottom) top = bottom;
                for (row = bottom; row > top; row--) {
                    for (col = 0u; col < 7u; col++) {
                        uint16_t dst = (uint16_t)row * SCREEN_WIDTH + (uint16_t)(MOVE_ANIM_PLAYER_BG_COL + col);
                        uint16_t src = (uint16_t)(row - 1u) * SCREEN_WIDTH + (uint16_t)(MOVE_ANIM_PLAYER_BG_COL + col);
                        uint16_t sdst = (uint16_t)(row + 2u) * SCROLL_MAP_W + (uint16_t)(MOVE_ANIM_PLAYER_BG_COL + col + 2u) + Map_UiColOfs();
                        uint16_t ssrc = (uint16_t)(row + 1u) * SCROLL_MAP_W + (uint16_t)(MOVE_ANIM_PLAYER_BG_COL + col + 2u) + Map_UiColOfs();
                        wTileMap[dst] = wTileMap[src];
                        gScrollTileMap[sdst] = gScrollTileMap[ssrc];
                    }
                }
                for (col = 0u; col < 7u; col++) {
                    uint16_t idx = (uint16_t)top * SCREEN_WIDTH + (uint16_t)(MOVE_ANIM_PLAYER_BG_COL + col);
                    uint16_t sidx = (uint16_t)(top + 2u) * SCROLL_MAP_W + (uint16_t)(MOVE_ANIM_PLAYER_BG_COL + col + 2u) + Map_UiColOfs();
                    wTileMap[idx] = BLANK_TILE_SLOT;
                    gScrollTileMap[sidx] = BLANK_TILE_SLOT;
                }
            } else {

                MoveAnim_OffsetEnemyY((ctx->se_counter0 == 0u) ? 8 : 16);
            }
            MoveAnim_DelayFramesAsm(8u);
            ctx->se_counter0 = (uint8_t)(ctx->se_counter0 + 1u);
            if (ctx->se_counter0 >= 2u) {
                ctx->se_phase = 2u;
            }
            return 0;
        case 2u:
            if (hWhoseTurn == 0u) {
                for (row = 0u; row < 7u; row++) {
                    uint8_t y = (uint8_t)(MOVE_ANIM_PLAYER_BG_ROW + row);
                    for (col = 0u; col < 7u; col++) {
                        uint16_t idx = (uint16_t)y * SCREEN_WIDTH + (uint16_t)(MOVE_ANIM_PLAYER_BG_COL + col);
                        uint16_t sidx = (uint16_t)(y + 2u) * SCROLL_MAP_W + (uint16_t)(MOVE_ANIM_PLAYER_BG_COL + col + 2u) + Map_UiColOfs();
                        wTileMap[idx] = BLANK_TILE_SLOT;
                        gScrollTileMap[sidx] = BLANK_TILE_SLOT;
                    }
                }
            } else {
                MoveAnim_AnimationHideMonPic(ctx);
            }

            MoveAnim_DelayFramesAsm(MOVE_ANIM_COPY_TEMP_PIC_FRAMES);
            ctx->se_phase = 0u;
            return 1;
        default:
            ctx->se_phase = 0u;
            return 1;
    }
}

static void MoveAnim_SetBGP(uint8_t bgp) {
    sMoveAnimCurrentBGP = bgp;
    Display_SetBGP(bgp);

    if (sMoveAnimEnemyPicOnOBP1) Display_SetOBP1(bgp);
}
