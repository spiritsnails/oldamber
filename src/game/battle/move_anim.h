#pragma once

#include <stdint.h>

typedef struct {
    uint8_t cmd0;
    uint8_t cmd1;
    uint8_t cmd2;
} move_anim_cmd_t;

typedef struct {
    uint8_t frameblock_id;
    uint8_t basecoord_id;
    uint8_t mode;
} subanim_entry_t;

typedef struct {
    uint8_t type;
    uint8_t count;
    const subanim_entry_t *entries;
} subanim_def_t;

typedef struct {
    void (*clear_mon_pic)(void);
    void (*clear_screen)(void);
} move_anim_trade_hooks_t;

void MoveAnim_SetTradeHooks(const move_anim_trade_hooks_t *hooks);

typedef struct {
    uint8_t animation_id;
    uint8_t trace_fired;
    uint8_t trace_anim_id;
    uint8_t animation_type;
    uint8_t anim_sound_id;
    uint8_t which_tileset;
    uint8_t subanim_frame_delay;
    uint8_t subanim_counter;
    uint8_t subanim_postdraw_pending;
    uint8_t subanim_transform;
    uint16_t subanim_entry_index;
    uint8_t fb_mode;
    uint8_t base_x;
    uint8_t base_y;
    uint16_t fb_dest_oam_index;
    uint16_t script_index;
    uint8_t wait_frames;
    uint16_t timing_frac_1e4;
    uint8_t runtime_state;
    uint8_t active_subanim;
    uint8_t active_special_effect;
    uint8_t active_se_id;
    uint8_t se_phase;
    uint8_t se_index;
    uint8_t se_counter0;

    uint8_t aid_phase;
    uint8_t aid_index;

    uint8_t skip_sound_waits;

    uint8_t entry_sound_waited;
    uint8_t pending_oam_clean;
    uint8_t script_done;

    uint8_t subanim_tiles_pending;
    uint8_t pending_sound_id;
    uint8_t pending_subanim_id;

    uint8_t apply_phase;

    uint8_t ball_item;
    uint8_t ball_anim_data;
    uint8_t num_shakes;
} move_anim_ctx_t;

void MoveAnim_Run(move_anim_ctx_t *ctx);
void MoveAnim_Begin(move_anim_ctx_t *ctx);
int MoveAnim_Tick(move_anim_ctx_t *ctx);
int MoveAnim_IsDone(const move_anim_ctx_t *ctx);

void MoveAnim_SetOnSgb(int on);
