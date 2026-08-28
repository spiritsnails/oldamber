
#include "battle_ui.h"
#include "battle_init.h"
#include "battle_loop.h"
#include "battle_ai.h"
#include "battle_core.h"
#include "battle_exp.h"
#include "battle_switch.h"
#include "battle_trainer.h"
#include "battle_catch.h"
#include "battle_items.h"
#include "move_anim.h"
#include "../speed_settings.h"
#include "../party_menu.h"
#include "../rom_text.h"
#include "../bag_menu.h"
#include "../inventory.h"
#include "../../platform/hardware.h"
#include "../../platform/display.h"
#include "../../platform/audio.h"
#include "../music.h"
#include "../../data/font_data.h"
#include "../../data/moves_data.h"
#include "../../data/base_stats.h"
#include "../../data/event_constants.h"
#include "../../data/pokemon_sprites.h"
#include "../../data/ghost_front_sprite.h"
#include "../gbc_color.h"
#include "../../data/trainer_sprites.h"
#include "../constants.h"
#include "../text.h"
#include "../pokedex.h"
#include "../pokemon.h"
#include "../sprite_mod.h"
#include "../overworld.h"
#include "../trainer_sight.h"
#include "../player.h"
#include "../naming_screen.h"
#include "../yesno.h"
#include "../session_log.h"
#include "../debug_cli.h"
#include "../amberscript_core.h"
#include "../amberscript_battle_debug.h"
#include "../johto_music.h"
#include "../../data/map_data.h"
#include "../amberscript_mapbank.h"
#include "../inventory.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../gen2_species.h"
#include "crystal_mon_pics.h"
#include "crystal_trainer_pics.h"
#include "johto_trainers.h"
#include "../map_music.h"
#include "../../platform/debug_log.h"

typedef enum {
    BUI_INACTIVE      = 0,
    BUI_SLIDE_IN,
    BUI_APPEARED,
    BUI_SEND_OUT,
    BUI_ENEMY_SLIDE_OUT,
    BUI_TRAINER_SLIDE_OUT,
    BUI_ENEMY_SEND_OUT,
    BUI_POKEMON_APPEAR,
    BUI_INTRO_SFX,
    BUI_INTRO,
    BUI_DRAW_HUD,
    BUI_MENU,
    BUI_MOVE_SELECT,
    BUI_MIMIC_PROMPT,
    BUI_MIMIC_SELECT,
    BUI_MOVE_ANIM,
    BUI_HP_ANIM,
    BUI_EXEC_MOVE_B,
    BUI_EXEC_SECOND,
    BUI_TURN_END,
    BUI_TURN_FINISH,
    BUI_RESIDUAL_EVENT,
    BUI_RESIDUAL_ANIM,
    BUI_RESIDUAL_HP_ANIM,
    BUI_RESIDUAL_FAINT_DELAY,
    BUI_RESIDUAL_RESOLVE,
    BUI_EXP_DRAIN,
    BUI_TRAINER_VICTORY_SLIDE,
    BUI_TRAINER_VICTORY_PAUSE,
    BUI_TRAINER_VICTORY_TEXT,
    BUI_RIVAL1_LOSS_TEXT,
    BUI_GYM_BADGE_JINGLE,
    BUI_WAIT_CRY,
    BUI_GHOST_REVEAL,
    BUI_WAIT_SOUND,
    BUI_FADE_WHITE,
    BUI_LEVELUP_STATS,
    BUI_LEARN_FORGET_YESNO,
    BUI_LEARN_PICK_MOVE,
    BUI_LEARN_STOP_YESNO,
    BUI_LEARN_SWAP_TEXT,
    BUI_LEARN_POOF_TEXT,
    BUI_LEARN_FORGOT_TEXT,
    BUI_LEARN_LEARNED_TEXT,
    BUI_LEARN_CANT_FORGET,
    BUI_LEARN_RESULT_TEXT,
    BUI_ENEMY_FAINT_ANIM,
    BUI_PLAYER_FAINT_ANIM,
    BUI_PLAYER_FAINTED,
    BUI_USE_NEXT_MON,
    BUI_SHIFT_PROMPT,
    BUI_SHIFT_PARTY,
    BUI_PARTY_SELECT,
    BUI_SWITCH_SELECT,
    BUI_RETREAT_ANIM,
    BUI_SWITCH_ENEMY_TURN,
    BUI_BAG_BATTLE,
    BUI_ITEM_TARGET,
    BUI_ITEM_FAIL_TEXT,
    BUI_ITEM_HEAL_ANIM,
    BUI_ITEM_RESULT_TEXT,
    BUI_ITEM_SECOND_TEXT,
    BUI_ITEM_FLUTE_MUSIC,
    BUI_AI_ACTION,
    BUI_AI_HP_SCROLL,
    BUI_AI_SWITCH_SEND,
    BUI_SAFARI_ITEM_ANIM,
    BUI_SAFARI_RESOLVE,
    BUI_BALL_THROW,
    BUI_BALL_POOF,
    BUI_BALL_SHAKE,
    BUI_CAUGHT,
    BUI_CAUGHT_DEX_WAIT,
    BUI_CAUGHT_BOX_TEXT,
    BUI_CAUGHT_NICK_PROMPT,
    BUI_CAUGHT_NICK_QUERY,
    BUI_CAUGHT_NICKNAME,
    BUI_CAUGHT_NICK_WAIT,
    BUI_EVOLUTION,
    BUI_BLACKOUT_FADE,
    BUI_END,
} bui_state_t;

static bui_state_t bui_state        = BUI_INACTIVE;
static int         bui_cursor       = 0;
static int         s_debug_autowin_pending = 0;
static int         s_is_champion_room_battle = 0;

static void bui_debug_force_enemy_party_defeated(void) {
    wEnemyMon.hp = 0;
    for (int i = 0; i < PARTY_LENGTH; i++) {
        wEnemyMons[i].base.hp = 0;
    }
}

static int         s_slide_cx       = 0;

static int  s_player_first;
static char s_name_a[20];
static char s_pfx_a[8];
static char s_name_b[20];
static char s_pfx_b[8];

static char s_msg_buf[384];

static int s_enemy_hp_bar_draw = -1;

static int s_player_hp_bar_draw = -1;

static void bui_show_text(const char *s) {
    Text_SetEndsWithPrompt();
    Text_ShowASCII(s);
}

static void bui_show_text_done(const char *s) {
    wDoNotWaitForButtonPress = 1;
    Text_KeepTilesOnClose();
    bui_show_text(s);
}

static const char *bui_party_slot_name(int slot) {
    static char s_name[NAME_LENGTH + 1];
    if (slot >= 0 && slot < PARTY_LENGTH && slot < (int)wPartyCount) {
        const uint8_t *nick = wPartyMonNicks[slot];
        if (nick[0] != 0x00 && nick[0] != 0x50) {
            int out = 0;
            for (int i = 0; i < NAME_LENGTH - 1 && out < NAME_LENGTH; i++) {
                uint8_t c = nick[i];
                if (c == 0x50) break;
                if      (c >= 0x80 && c <= 0x99) s_name[out++] = (char)('A' + (c - 0x80));
                else if (c >= 0xA0 && c <= 0xB9) s_name[out++] = (char)('a' + (c - 0xA0));
                else if (c >= 0xF6)              s_name[out++] = (char)('0' + (c - 0xF6));
                else if (c == 0x7F)              s_name[out++] = ' ';
                else if (c == 0xE8)              s_name[out++] = '.';
                else if (c == 0xE7)              s_name[out++] = '!';
                else if (c == 0xE6)              s_name[out++] = '?';
                else if (c == 0xE3)              s_name[out++] = '-';
                else if (c == 0xE0)              s_name[out++] = '\'';
            }
            s_name[out] = '\0';
            if (out > 0) return s_name;
        }
        return Pokemon_GetName(Species_Dex(wPartyMons[slot].base.species));
    }
    return Pokemon_GetName(Species_Dex(wBattleMon.species));
}

static const char *bui_player_mon_name(void) {
    return bui_party_slot_name((int)wPlayerMonNumber);
}

static uint8_t s_enemy_display_species;

static int bui_enemy_drawn_as_ghost(void);

static const char *bui_enemy_mon_name(void) {

    if (bui_enemy_drawn_as_ghost()) return "GHOST";
    if (!(wEnemyBattleStatus3 & (1u << BSTAT3_TRANSFORMED)))
        s_enemy_display_species = wEnemyMon.species;
    return Pokemon_GetName(Species_Dex(s_enemy_display_species));
}

static uint8_t s_pending_item = 0;

static char s_item_second_text[64];
static int  s_item_second_anim;
static int  s_flute_music_started;

extern void (*gMoveAnimTraceHook)(uint8_t anim_id, uint8_t whose_turn);

void (*gEvoTraceHook)(int phase, uint8_t species, uint8_t dex) = NULL;

static int s_saved_battle_menu_item = 0;

int BattleUI_GetSavedMenuItem(void) { return s_saved_battle_menu_item; }

static bui_state_t   bui_exp_dest    = BUI_END;
static char          s_exp_suffix[64];
static char          s_exp_suffix2[64];

static int s_faint_step  = 0;
static int s_faint_timer = 0;

static const uint8_t kBuiBlackoutFadeOut[4][3] = {
    { 0xE4, 0xD0, 0xE0 },
    { 0xF9, 0xE4, 0xE4 },
    { 0xFE, 0xFE, 0xF8 },
    { 0xFF, 0xFF, 0xFF },
};
static int s_bo_fade_step  = -1;
static int s_bo_fade_timer = 0;

static levelup_stats_t s_pending_lvl_stats;

static uint8_t s_learn_slot = 0xFF;
static uint8_t s_learn_move = 0;
static uint8_t s_learn_old_move = 0;
static int     s_learn_cursor = 0;
static int     s_learn_phase  = 0;

static int         s_victory_timer      = 0;

static int         s_rival1_loss        = 0;
static char        s_trainer_money_text[80];

static const char *s_badge_recv_text    = NULL;
static const char *s_badge_info_text    = NULL;
static bui_state_t s_wait_cry_next_state = BUI_END;
static char        s_wait_cry_text[64];

static int         s_wait_cry_text_keep = 0;

static int         s_wait_cry_delay = 0;

static int         s_intro_sfx_timer = 0;
static int         s_intro_sfx_wait  = 0;
static int         s_ghost_intro_phase   = 0;
static int         s_ghost_intro_timer   = 0;

#define MAP_POKEMON_TOWER_1F 0x8E
#define MAP_POKEMON_TOWER_7F 0x94
#define ITEM_SILPH_SCOPE     0x48

static void bui_ghost_set_sprite_pal(int dmg_pal) {
    if (dmg_pal >= 0) Display_SetOBP1((uint8_t)dmg_pal);
    Display_SetOBJColorPermute(GBC_OBJ_PAL_ENEMY_MON, dmg_pal);
}

#define GHOST_OBP1_NORMAL    0xE4

#define GHOST_OBP1_FLASH     (GHOST_OBP1_NORMAL ^ 0x80)

static int bui_map_is_pokemon_tower(void) {
    if (wCurMap >= MAP_POKEMON_TOWER_1F && wCurMap <= MAP_POKEMON_TOWER_7F) return 1;
    if (wCurMap < PKS_VIRTUAL_MAP_FIRST || wCurMap > PKS_VIRTUAL_MAP_LAST) return 0;
    {
        const char *n = AmberScript_MapBank_NameForRealId(wCurMap);
        return n && strncmp(n, "PokemonTower", 12) == 0;
    }
}

#define MAP_POKEMON_TOWER_3F 0x90
static int bui_map_is_tower_wild_floor(void) {
    if (wCurMap >= MAP_POKEMON_TOWER_3F && wCurMap <= MAP_POKEMON_TOWER_7F) return 1;
    if (wCurMap < PKS_VIRTUAL_MAP_FIRST || wCurMap > PKS_VIRTUAL_MAP_LAST) return 0;
    {
        const char *n = AmberScript_MapBank_NameForRealId(wCurMap);
        if (!n || strncmp(n, "PokemonTower", 12) != 0) return 0;
        return n[12] >= '3' && n[12] <= '7' && n[13] == 'F' && n[14] == '\0';
    }
}

static int bui_should_run_marowak_ghost_intro(void);

static int bui_enemy_drawn_as_ghost(void) {
    if (Battle_IsGhostBattle()) return 1;
    if (bui_should_run_marowak_ghost_intro())
        return s_ghost_intro_phase < 3;
    return 0;
}

int BattleUI_EnemyDrawnAsGhost(void) { return bui_enemy_drawn_as_ghost(); }

static int bui_should_run_marowak_ghost_intro(void) {
    if (wIsInBattle != 1) return 0;
    if (!bui_map_is_pokemon_tower()) return 0;
    if (wEnemyMon.species != SPECIES_MAROWAK) return 0;

    return Inventory_GetQty(ITEM_SILPH_SCOPE) > 0;
}

void BattleUI_SetBadgeRecvText(const char *text) { s_badge_recv_text = text; }
void BattleUI_SetBadgeInfoText(const char *text) { s_badge_info_text = text; }

static int s_grow_stage;
static int s_grow_frame;

static int s_grow_after_switch;

static int s_mid_battle_send;

static int     s_retreat_stage;
static int     s_retreat_frame;
static uint8_t s_retreat_species;
static uint8_t s_switch_slot;
static int     s_struggle_pending;
static int     s_turn_already_used_pending;

static int     s_shift_phase;
static int     s_shift_pending;
static uint8_t s_shift_slot;

static uint8_t s_evo_slot;
static uint8_t s_evo_old_species;
static uint8_t s_evo_new_species;
static int     s_evo_phase;
static int     s_evo_timer;
static int     s_evo_blink;
static int     s_evo_wave;
static int     s_evo_wave_units;
static int     s_evo_cancelled;
static int     s_evo_screen_white;

#define THROW_BALL_OAM_BASE  0

#define BALL_SHAKE_OAM_Y     56
#define BALL_SHAKE_OAM_X     120

#define THROW_FPW            3

#define SHAKE_DELAY          40
#define SHAKE_CYCLE          56

#define BALL_TILE_NEUT_TOP   114
#define BALL_TILE_NEUT_BOT   115
#define BALL_TILE_TILT_TL    116
#define BALL_TILE_TILT_TR    117
#define BALL_TILE_TILT_BL    118
#define BALL_TILE_TILT_BR    119

#define POOF_TILE_BASE    53
#define POOF_OAM_BASE     4
#define POOF_OAM_COUNT    16

#define POOF_T20  (POOF_TILE_BASE + 0)
#define POOF_T21  (POOF_TILE_BASE + 1)
#define POOF_T23  (POOF_TILE_BASE + 2)
#define POOF_T24  (POOF_TILE_BASE + 3)
#define POOF_T25  (POOF_TILE_BASE + 4)
#define POOF_T30  (POOF_TILE_BASE + 5)
#define POOF_T31  (POOF_TILE_BASE + 6)
#define POOF_T32  (POOF_TILE_BASE + 7)
#define POOF_T33  (POOF_TILE_BASE + 8)
#define POOF_T34  (POOF_TILE_BASE + 9)

static uint8_t        s_ball_item     = 0;
static catch_result_t s_catch_result  = CATCH_RESULT_0_SHAKES;

static int s_oldman_auto_delay = -1;

static int s_oldman_menu_phase = 0;
static int            s_throw_frame   = 0;
static int            s_shake_total   = 0;
static int            s_shake_frame   = 0;
static int            s_poof_frame    = 0;
static int            s_poof_phase    = 0;

static int            s_poof_oam_live = 0;
static int            s_safari_resolve_phase  = 0;
static uint8_t        s_safari_item_anim_id = 0;
static int            s_safari_item_anim_started = 0;
static uint8_t        s_caught_species = 0;
static uint8_t        s_caught_dex = 0;
static int            s_caught_new_entry = 0;
static int            s_caught_sent_to_box = 0;
static int            s_caught_dex_started = 0;
static int            s_caught_party_slot = -1;
static int            s_caught_box_slot = -1;
static int            s_caught_box_index = -1;

static int s_anim_type;
static int s_anim_frame;
static int s_anim_total;
static int s_anim_first;
static int s_move_anim_active;
static int s_move_anim_hit_sfx_started;
static int s_move_anim_should_hit_sfx;
static uint8_t s_move_anim_owner_turn;
static uint8_t s_move_anim_queue_ids[3];
static uint8_t s_move_anim_queue_forced_turn[3];
static uint8_t s_move_anim_queue_count;
static uint8_t s_move_anim_queue_index;
static uint8_t s_status_hud_redraw_mask;

static int s_player_charge_hidden;
static int s_enemy_charge_hidden;
static int s_player_charge_resolving_anim;
static int s_enemy_charge_resolving_anim;
static move_anim_ctx_t s_move_anim_ctx;

static int bui_move_anim_tick(void) {
    int steps = SpeedSettings_MoveAnim();
    if (steps == SPEED_UNCAPPED) {

        s_move_anim_ctx.skip_sound_waits = 1;
        for (int guard = 0; guard < 4096; guard++) {
            if (MoveAnim_Tick(&s_move_anim_ctx)) return 1;

            if (!s_move_anim_ctx.entry_sound_waited) return 0;
        }
        return 1;
    }

    s_move_anim_ctx.skip_sound_waits = (steps > SPEED_NORMAL);
    for (int i = 0; i < steps; i++) {
        if (MoveAnim_Tick(&s_move_anim_ctx)) return 1;
        if (!s_move_anim_ctx.entry_sound_waited) return 0;
    }
    return 0;
}
typedef struct {
    uint8_t is_pre;
    battle_status_msg_t msg;
} bui_status_evt_t;
#define BUI_STATUS_EVT_MAX 8
static bui_status_evt_t s_evt_status_q[2][BUI_STATUS_EVT_MAX];
static uint8_t s_evt_status_q_count[2];
static battle_move_result_t s_evt_move_result;
#define BUI_HIT_EVT_Q_MAX 8
static uint8_t s_evt_hit_sfx_q[BUI_HIT_EVT_Q_MAX];
static uint8_t s_evt_hit_crit_q[BUI_HIT_EVT_Q_MAX];
static uint8_t s_evt_hit_sfx_q_count;
static uint8_t s_evt_hit_sfx_q_index;
static uint8_t s_evt_hp_target_q[BUI_HIT_EVT_Q_MAX];

static uint16_t s_evt_hp_target_hp_q[BUI_HIT_EVT_Q_MAX];
static uint16_t s_evt_hp_target_hp_pending = 0xFFFFu;
static uint8_t s_evt_hp_target_q_count;
static uint8_t s_evt_hp_target_q_index;

#define BUI_HIT_TEXT_MAX 6
static char s_hit_text[BUI_HIT_TEXT_MAX][128];
static uint8_t s_hit_text_count;
static uint8_t s_hit_index;
static uint8_t s_hit_text_shown;
static uint8_t s_hit_anim_whose;
static uint8_t s_suppress_move_text;

static char s_pending_status_text[192];
static uint8_t s_pending_status_text_active;
#define BUI_STAT_TEXT_EVT_MAX 4
typedef struct {
    uint8_t stat_idx;
    uint8_t side;
    uint8_t flags;
} bui_stat_text_evt_t;
static bui_stat_text_evt_t s_evt_stat_text_q[BUI_STAT_TEXT_EVT_MAX];
static uint8_t s_evt_stat_text_q_count;

#define BUI_EFFECT_MSG_EVT_MAX 4
typedef struct { uint8_t msg, side, extra; } bui_effect_msg_evt_t;
static bui_effect_msg_evt_t s_evt_effect_msg_q[BUI_EFFECT_MSG_EVT_MAX];
static uint8_t s_evt_effect_msg_q_count;

static int      s_hp_anim_who;
static int      s_hp_old_px;
static int      s_hp_new_px;
static int      s_hp_cur_px;
static int      s_hp_half_frame;
static int      s_hp_cur_hp;
static int      s_hp_new_hp;
static int      s_hp_stage2_hp;
static int      s_hp_bar_pending;
static int      s_hp_delay;

static int      s_hp_anim_active;

static int      s_hp_hold_active;

static int      s_hp_anim_deferred;

#define BUI_HIT_HP_MAX 8
static uint16_t s_hit_hp[BUI_HIT_HP_MAX];
static uint8_t  s_hit_hp_count;
static uint8_t  s_hit_text_count;
static uint8_t bui_hit_replay_count(void) {
    return (s_hit_hp_count > s_hit_text_count) ? s_hit_hp_count : s_hit_text_count;
}
static int      s_hit_scroll_pending;

static uint8_t bui_hit_replay_count(void);
static int      s_hp_anim_multihit;
static uint16_t s_hp_pre_hp;
static uint16_t s_hp_pre_max;
static uint16_t s_hp_pre_player_hp;
static uint16_t s_hp_pre_player_max;
static uint16_t s_hp_pre_enemy_hp;
static uint16_t s_hp_pre_enemy_max;
static int      s_hp_stage2_pending;
static int      s_hp_stage2_px;
static int      s_multihit_replay_armed;
static int      s_multihit_replay_whose;
static int      s_residual_phase;
static int      s_residual_alive;
#define BUI_RESIDUAL_EVT_MAX 12

#define BUI_RESIDUAL_FAINT_DELAY_FRAMES 21
static battle_event_t s_residual_evt_q[BUI_RESIDUAL_EVT_MAX];
static uint8_t  s_residual_evt_q_count;
static uint8_t  s_residual_evt_q_index;
static int      s_residual_delay_frames;

static char s_post_move_text[384];

static char s_apply_text[192];
static uint8_t s_apply_text_pending;

static uint8_t s_post_hp_anim_id;
static uint8_t s_post_hp_anim_turn;
static uint8_t s_post_hp_anim_pending;
static uint8_t s_post_hp_anim_running;

static uint8_t s_post_hp_anim_msgs_before;
static char    s_pre_shake_text[64];
static uint8_t s_pre_shake_text_pending;

static char s_drain_text[128];

typedef struct {
    uint8_t target_status;
    uint8_t target_bstat1;
    uint8_t attacker_bstat1;
    uint8_t attacker_bstat2;
    uint8_t target_stat_mods[6];
    uint8_t attacker_stat_mods[6];
} pre_move_snap_t;
static pre_move_snap_t s_pre;

static int bui_char_to_tile(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return Font_CharToTile(0x80 + (c - 'A'));
    if (c >= 'a' && c <= 'z') return Font_CharToTile(0xA0 + (c - 'a'));
    if (c == ' ')              return BLANK_TILE_SLOT;
    if (c >= '0' && c <= '9') return Font_CharToTile(0xF6 + (c - '0'));
    if (c == '!')              return Font_CharToTile(0xE7);
    if (c == '?')              return Font_CharToTile(0xE6);
    if (c == '.')              return Font_CharToTile(0xE8);
    if (c == ',')              return Font_CharToTile(0xF4);
    if (c == '-')              return Font_CharToTile(0xE3);
    if (c == '\'')             return Font_CharToTile(0xE0);
    if (c == ':')              return Font_CharToTile(0x9C);
    if (c == '/')              return Font_CharToTile(0xF3);
    if (c == '>')              return Font_CharToTile(0xED);
    if (c == '#')              return Font_CharToTile(0xE0);
    return BLANK_TILE_SLOT;
}

static void bui_place_player_sprite(void);
static void bui_hide_player_sprite(void);
static void bui_hide_player_slide_oam(void);
static void bui_ball_load_poof_tiles(void);
static void bui_note_target_hud_redraw_if_status_changed(int whose);

static void bui_decode_poke_str(const uint8_t *src, char *dst, int max) {
    int out = 0;
    for (int i = 0; out < max - 1 && src[i] != 0x50; i++) {
        uint8_t c = src[i];
        if      (c >= 0x80 && c <= 0x99) dst[out++] = (char)('A' + (c - 0x80));
        else if (c >= 0xA0 && c <= 0xB9) dst[out++] = (char)('a' + (c - 0xA0));
        else if (c == 0x7F)               dst[out++] = ' ';
        else if (c >= 0xF6)               dst[out++] = (char)('0' + (c - 0xF6));
        else if (c == 0xE8)               dst[out++] = '.';
        else if (c == 0xE3)               dst[out++] = '-';
        else                              dst[out++] = '?';
    }
    dst[out] = '\0';
}

static int bui_is_ball(uint8_t id) {
    return id == ITEM_MASTER_BALL || id == ITEM_ULTRA_BALL ||
           id == ITEM_GREAT_BALL  || id == ITEM_POKE_BALL  ||
           id == ITEM_SAFARI_BALL;
}

static int item_needs_target(uint8_t id) {
    switch (id) {
    case ITEM_ANTIDOTE:    case ITEM_BURN_HEAL:   case ITEM_ICE_HEAL:
    case ITEM_AWAKENING:   case ITEM_PARLYZ_HEAL: case ITEM_FULL_HEAL:
    case ITEM_FULL_RESTORE:case ITEM_MAX_POTION:  case ITEM_HYPER_POTION:
    case ITEM_SUPER_POTION:case ITEM_POTION:      case ITEM_FRESH_WATER:
    case ITEM_SODA_POP:    case ITEM_LEMONADE:
    case ITEM_REVIVE:      case ITEM_MAX_REVIVE:
        return 1;
    default:
        return 0;
    }
}

static void bui_clear_rows(int r0, int r1);
static void bui_draw_enemy_hud(void);
static void bui_draw_player_hud(void);
static void bui_load_sprites(void);
static void bui_hide_pokeballs(void);
static void bui_set_enemy_oam_visible(int visible);
static void bui_draw_box(void);
extern uint8_t BattleRandom(void);

static int bui_pokedex_owned_num(uint8_t dex_num) {
    int bit;
    if (dex_num < 1 || dex_num > 151) return 0;
    bit = dex_num - 1;
    return (wPokedexOwned[bit >> 3] >> (bit & 7)) & 1;
}

static int bui_current_box_is_full(void) {
    uint8_t box = (uint8_t)(wCurrentBoxNum % NUM_BOXES);
    return wBoxCount[box] >= BOX_CAPACITY;
}

static void bui_retreat_text(const char *nick, char *out, size_t outsz) {
    uint16_t dropped = (uint16_t)(wLastSwitchInEnemyMonHP - wEnemyMon.hp);
    uint8_t  divisor = (uint8_t)((wEnemyMon.max_hp >> 2) & 0xFFu);
    uint8_t  pct     = divisor ? (uint8_t)(((uint32_t)dropped * 25u) / divisor) : 0u;

    const char *praise;
    if      (pct == 0) praise = " enough!";
    else if (pct < 30) praise = " ";
    else if (pct < 70) praise = " OK!";
    else               praise = " good!";

    snprintf(out, outsz, "%s%s\nCome back!", nick, praise);
}

static void bui_log_colour_state(const char *when) {
    printf("[green] %s posattr=%d colormode=%d bgp=%02x\n",
           when, Display_GetPositionAttrMode(), Display_GetColorMode(),
           Display_GetBGP());
    for (int p = 0; p < 5; p++) {
        printf("[green]   bg[%d]", p);
        for (int i = 0; i < 4; i++) {
            uint16_t c = Display_GetBGColorEntry(p, i);
            printf(" (%02d,%02d,%02d)", c & 31, (c >> 5) & 31, (c >> 10) & 31);
        }
        printf("\n");
    }
    for (int row = 0; row < SCREEN_HEIGHT; row += 2) {
        printf("[green]   attr r%-2d", row);
        for (int col = 0; col < SCREEN_WIDTH; col += 2)
            printf(" %d", Display_GetPositionAttr(col, row));
        printf("\n");
    }
    fflush(stdout);
}

static void bui_restore_battle_palette(void) {
    bui_log_colour_state("restore-enter");
    Display_SetPalette(0xE4, 0xE4, 0xE4);
}

static void bui_caught_after_naming(void) {
    if (s_caught_sent_to_box) {
        const char *ename = Pokemon_GetName(s_caught_dex);
        snprintf(s_msg_buf, sizeof(s_msg_buf),
                 CheckEvent(EVENT_MET_BILL)
                    ? "%s was\ntransferred to\nBILL's PC!"
                    : "%s was\ntransferred to\nsomeone's PC!",
                 ename ? ename : "");
        bui_show_text(s_msg_buf);
        bui_state = BUI_CAUGHT_BOX_TEXT;
        return;
    }
    bui_state = BUI_FADE_WHITE;
}

static void bui_restore_catch_screen_after_dex(void) {
    bui_clear_rows(0, SCREEN_HEIGHT - 1);
    bui_draw_enemy_hud();
    bui_draw_player_hud();
    bui_load_sprites();
    bui_hide_pokeballs();
    bui_set_enemy_oam_visible(0);
    bui_draw_box();
    Display_SetPalette(0xE4, 0xE4, 0xE4);
}

static void bui_ball_load_tiles(void) {
    Display_LoadSpriteTile(BALL_TILE_NEUT_TOP, kBallTileNeutTop);
    Display_LoadSpriteTile(BALL_TILE_NEUT_BOT, kBallTileNeutBot);
    Display_LoadSpriteTile(BALL_TILE_TILT_TL,  kBallTileTiltTL);
    Display_LoadSpriteTile(BALL_TILE_TILT_TR,  kBallTileTiltTR);
    Display_LoadSpriteTile(BALL_TILE_TILT_BL,  kBallTileTiltBL);
    Display_LoadSpriteTile(BALL_TILE_TILT_BR,  kBallTileTiltBR);
    bui_ball_load_poof_tiles();
}

static void bui_ball_set_oam(uint8_t baseY, uint8_t baseX, int fb) {
    int b = THROW_BALL_OAM_BASE;
    switch (fb) {
    case 0:
        wShadowOAM[b+0].y = baseY;          wShadowOAM[b+0].x = baseX;
        wShadowOAM[b+0].tile = BALL_TILE_NEUT_TOP; wShadowOAM[b+0].flags = 0;
        wShadowOAM[b+1].y = baseY;          wShadowOAM[b+1].x = (uint8_t)(baseX + 8);
        wShadowOAM[b+1].tile = BALL_TILE_NEUT_TOP; wShadowOAM[b+1].flags = OAM_FLAG_FLIP_X;
        wShadowOAM[b+2].y = (uint8_t)(baseY + 8); wShadowOAM[b+2].x = baseX;
        wShadowOAM[b+2].tile = BALL_TILE_NEUT_BOT; wShadowOAM[b+2].flags = 0;
        wShadowOAM[b+3].y = (uint8_t)(baseY + 8); wShadowOAM[b+3].x = (uint8_t)(baseX + 8);
        wShadowOAM[b+3].tile = BALL_TILE_NEUT_BOT; wShadowOAM[b+3].flags = OAM_FLAG_FLIP_X;
        break;
    case 1:
        wShadowOAM[b+0].y = baseY;          wShadowOAM[b+0].x = baseX;
        wShadowOAM[b+0].tile = BALL_TILE_TILT_TL;  wShadowOAM[b+0].flags = 0;
        wShadowOAM[b+1].y = baseY;          wShadowOAM[b+1].x = (uint8_t)(baseX + 8);
        wShadowOAM[b+1].tile = BALL_TILE_TILT_TR;  wShadowOAM[b+1].flags = 0;
        wShadowOAM[b+2].y = (uint8_t)(baseY + 8); wShadowOAM[b+2].x = baseX;
        wShadowOAM[b+2].tile = BALL_TILE_TILT_BL;  wShadowOAM[b+2].flags = 0;
        wShadowOAM[b+3].y = (uint8_t)(baseY + 8); wShadowOAM[b+3].x = (uint8_t)(baseX + 8);
        wShadowOAM[b+3].tile = BALL_TILE_TILT_BR;  wShadowOAM[b+3].flags = 0;
        break;
    case 2:
        wShadowOAM[b+0].y = baseY;          wShadowOAM[b+0].x = baseX;
        wShadowOAM[b+0].tile = BALL_TILE_TILT_TR;  wShadowOAM[b+0].flags = OAM_FLAG_FLIP_X;
        wShadowOAM[b+1].y = baseY;          wShadowOAM[b+1].x = (uint8_t)(baseX + 8);
        wShadowOAM[b+1].tile = BALL_TILE_TILT_TL;  wShadowOAM[b+1].flags = OAM_FLAG_FLIP_X;
        wShadowOAM[b+2].y = (uint8_t)(baseY + 8); wShadowOAM[b+2].x = baseX;
        wShadowOAM[b+2].tile = BALL_TILE_TILT_BR;  wShadowOAM[b+2].flags = OAM_FLAG_FLIP_X;
        wShadowOAM[b+3].y = (uint8_t)(baseY + 8); wShadowOAM[b+3].x = (uint8_t)(baseX + 8);
        wShadowOAM[b+3].tile = BALL_TILE_TILT_BL;  wShadowOAM[b+3].flags = OAM_FLAG_FLIP_X;
        break;
    }
}

static void bui_ball_hide(void) {
    for (int i = 0; i < 4; i++)
        wShadowOAM[THROW_BALL_OAM_BASE + i].y = 0;
}

static void bui_begin_ball_throw(uint8_t item) {
    s_ball_item    = item;
    s_throw_frame  = 0;
    s_catch_result = Battle_CatchAttempt(s_ball_item);

    if (wBattleType == 1) s_catch_result = CATCH_RESULT_SUCCESS;

    wPokeBallAnimData = (uint8_t)s_catch_result;

    char pname[NAME_LENGTH + 1];
    char iname[20];
    if (wBattleType == 1) {
        snprintf(pname, sizeof(pname), "OLD MAN");
    } else {
        bui_decode_poke_str(wPlayerName, pname, sizeof(pname));
    }
    Inventory_DecodeASCII(item, iname, sizeof(iname));
    snprintf(s_msg_buf, sizeof(s_msg_buf), "%s used\n%s!", pname, iname);
    Text_KeepTilesOnClose();
    bui_show_text(s_msg_buf);

    bui_ball_load_tiles();
    bui_ball_hide();
    bui_state = BUI_BALL_THROW;
}

static void bui_ball_fail_text_and_advance(void) {
    const char *fail;
    switch (s_catch_result) {
    case CATCH_RESULT_CANNOT_CATCH:
        fail = RomText("ItemUseBallText00");
        break;
    case CATCH_RESULT_0_SHAKES:
        fail = RomText("ItemUseBallText01");
        break;
    case CATCH_RESULT_1_SHAKE:
        fail = RomText("ItemUseBallText02");
        break;
    case CATCH_RESULT_2_SHAKES:
        fail = RomText("_ItemUseBallText03");
        break;
    default:
        fail = RomText("_ItemUseBallText04");
        break;
    }
    bui_show_text(fail);
    if (wBattleType == 2) {
        s_safari_resolve_phase = 0;
        bui_state = BUI_SAFARI_RESOLVE;
    } else {

        wActionResultOrTookBattleTurn = 1;
        s_turn_already_used_pending = 1;
        bui_state = BUI_MOVE_SELECT;
    }
}

static void bui_begin_safari_item(int is_bait) {

    s_safari_item_anim_id = is_bait ? 202u  : 201u ;
    s_safari_item_anim_started = 0;
    s_safari_resolve_phase = 0;
    uint8_t add;
    do {
        add = (uint8_t)(BattleRandom() & 7u);
    } while (add >= 5u);
    add++;

    if (is_bait) {
        wEnemyMonActualCatchRate >>= 1;
        wSafariEscapeFactor = 0;
        if ((uint16_t)wSafariBaitFactor + add > 255u)
            wSafariBaitFactor = 255;
        else
            wSafariBaitFactor = (uint8_t)(wSafariBaitFactor + add);
        {
            char pname[NAME_LENGTH + 1];
            bui_decode_poke_str(wPlayerName, pname, sizeof(pname));
            snprintf(s_msg_buf, sizeof(s_msg_buf), "%s threw\nsome BAIT.", pname);
            Text_KeepTilesOnClose();
            bui_show_text(s_msg_buf);
        }
    } else {
        uint16_t rate = (uint16_t)wEnemyMonActualCatchRate * 2u;
        wEnemyMonActualCatchRate = (rate > 255u) ? 255u : (uint8_t)rate;
        wSafariBaitFactor = 0;
        if ((uint16_t)wSafariEscapeFactor + add > 255u)
            wSafariEscapeFactor = 255;
        else
            wSafariEscapeFactor = (uint8_t)(wSafariEscapeFactor + add);
        {
            char pname[NAME_LENGTH + 1];
            bui_decode_poke_str(wPlayerName, pname, sizeof(pname));
            snprintf(s_msg_buf, sizeof(s_msg_buf), "%s threw a\nROCK.", pname);
            Text_KeepTilesOnClose();
            bui_show_text(s_msg_buf);
        }
    }
    bui_state = BUI_SAFARI_ITEM_ANIM;
}

static int bui_safari_should_flee(void) {

    uint8_t enemy_spd_lo = (uint8_t)(wEnemyMon.spd & 0xFFu);
    uint16_t b = (uint16_t)(enemy_spd_lo * 2u);

    if (enemy_spd_lo >= 128u)
        return 1;

    if (wSafariBaitFactor)
        b >>= 2;
    if (wSafariEscapeFactor) {
        b <<= 1;
        if (b > 255u) b = 255u;
    }

    return BattleRandom() < (uint8_t)b;
}

static void bui_ball_load_poof_tiles(void) {
    Display_LoadSpriteTile(POOF_T20, kPoofTile20);
    Display_LoadSpriteTile(POOF_T21, kPoofTile21);
    Display_LoadSpriteTile(POOF_T23, kPoofTile23);
    Display_LoadSpriteTile(POOF_T24, kPoofTile24);
    Display_LoadSpriteTile(POOF_T25, kPoofTile25);
    Display_LoadSpriteTile(POOF_T30, kPoofTile30);
    Display_LoadSpriteTile(POOF_T31, kPoofTile31);
    Display_LoadSpriteTile(POOF_T32, kPoofTile32);
    Display_LoadSpriteTile(POOF_T33, kPoofTile33);
    Display_LoadSpriteTile(POOF_T34, kPoofTile34);
}

typedef struct { int8_t col; int8_t row; uint8_t tile; uint8_t flags; } poof_spr_t;

static const poof_spr_t kFB06[12] = {
    {1,0,POOF_T23,0},                        {0,1,POOF_T32,0},  {1,1,POOF_T33,0},
    {2,0,POOF_T23,OAM_FLAG_FLIP_X},          {2,1,POOF_T33,OAM_FLAG_FLIP_X},
    {3,1,POOF_T32,OAM_FLAG_FLIP_X},
    {0,2,POOF_T32,OAM_FLAG_FLIP_Y},          {1,2,POOF_T33,OAM_FLAG_FLIP_Y},
    {1,3,POOF_T23,OAM_FLAG_FLIP_Y},
    {2,2,POOF_T33,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
    {3,2,POOF_T32,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
    {2,3,POOF_T23,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
};

static const poof_spr_t kFB07[16] = {
    {0,0,POOF_T20,0}, {1,0,POOF_T21,0}, {0,1,POOF_T30,0}, {1,1,POOF_T31,0},
    {2,0,POOF_T21,OAM_FLAG_FLIP_X}, {3,0,POOF_T20,OAM_FLAG_FLIP_X},
    {2,1,POOF_T31,OAM_FLAG_FLIP_X}, {3,1,POOF_T30,OAM_FLAG_FLIP_X},
    {0,2,POOF_T30,OAM_FLAG_FLIP_Y}, {1,2,POOF_T31,OAM_FLAG_FLIP_Y},
    {0,3,POOF_T20,OAM_FLAG_FLIP_Y}, {1,3,POOF_T21,OAM_FLAG_FLIP_Y},
    {2,2,POOF_T31,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
    {3,2,POOF_T30,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
    {2,3,POOF_T21,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
    {3,3,POOF_T20,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
};

static const poof_spr_t kFB08[16] = {
    {0,0,POOF_T20,0}, {1,0,POOF_T21,0}, {0,1,POOF_T30,0}, {1,1,POOF_T31,0},
    {3,0,POOF_T21,OAM_FLAG_FLIP_X}, {4,0,POOF_T20,OAM_FLAG_FLIP_X},
    {3,1,POOF_T31,OAM_FLAG_FLIP_X}, {4,1,POOF_T30,OAM_FLAG_FLIP_X},
    {0,3,POOF_T30,OAM_FLAG_FLIP_Y}, {1,3,POOF_T31,OAM_FLAG_FLIP_Y},
    {0,4,POOF_T20,OAM_FLAG_FLIP_Y}, {1,4,POOF_T21,OAM_FLAG_FLIP_Y},
    {3,3,POOF_T31,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
    {4,3,POOF_T30,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
    {3,4,POOF_T21,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
    {4,4,POOF_T20,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
};

static const poof_spr_t kFB09[12] = {
    {0,0,POOF_T24,0}, {1,0,POOF_T25,0}, {0,1,POOF_T34,0},
    {3,0,POOF_T25,OAM_FLAG_FLIP_X}, {4,0,POOF_T24,OAM_FLAG_FLIP_X},
    {4,1,POOF_T34,OAM_FLAG_FLIP_X},
    {0,3,POOF_T34,OAM_FLAG_FLIP_Y}, {0,4,POOF_T24,OAM_FLAG_FLIP_Y},
    {1,4,POOF_T25,OAM_FLAG_FLIP_Y},
    {4,3,POOF_T34,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
    {3,4,POOF_T25,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
    {4,4,POOF_T24,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
};

static const poof_spr_t kFB0A[12] = {
    {0,0,POOF_T24,0}, {1,0,POOF_T25,0}, {0,1,POOF_T34,0},
    {4,0,POOF_T25,OAM_FLAG_FLIP_X}, {5,0,POOF_T24,OAM_FLAG_FLIP_X},
    {5,1,POOF_T34,OAM_FLAG_FLIP_X},
    {0,4,POOF_T34,OAM_FLAG_FLIP_Y}, {0,5,POOF_T24,OAM_FLAG_FLIP_Y},
    {1,5,POOF_T25,OAM_FLAG_FLIP_Y},
    {5,4,POOF_T34,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
    {4,5,POOF_T25,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
    {5,5,POOF_T24,OAM_FLAG_FLIP_X|OAM_FLAG_FLIP_Y},
};

static void bui_draw_poof_frame_tf(int entry, int hflip) {
    static const uint8_t kBaseY[6] = {48, 48, 44, 44, 40, 40};
    static const uint8_t kBaseX[6] = {112,112,108,108,104,104};
    static const poof_spr_t * const kFB[6]  = {kFB06,kFB07,kFB08,kFB09,kFB0A,kFB0A};
    static const int                kSz[6]  = {12,16,16,12,12,12};

    int baseY = kBaseY[entry], baseX = kBaseX[entry];
    const poof_spr_t *fb = kFB[entry];
    int n = kSz[entry];
    for (int i = 0; i < n; i++) {
        int idx = POOF_OAM_BASE + i;
        int y = baseY + fb[i].row * 8;
        int x = baseX + fb[i].col * 8;
        uint8_t flags = fb[i].flags;
        if (hflip) {
            y += 40;
            x  = 168 - x;
            flags ^= OAM_FLAG_FLIP_X;
        }
        wShadowOAM[idx].y     = (uint8_t)y;
        wShadowOAM[idx].x     = (uint8_t)x;
        wShadowOAM[idx].tile  = fb[i].tile;
        wShadowOAM[idx].flags = flags;
    }
    s_poof_oam_live = 1;
}

static void bui_draw_poof_frame(int entry)        { bui_draw_poof_frame_tf(entry, 0); }
static void bui_draw_player_poof_frame(int entry) { bui_draw_poof_frame_tf(entry, 1); }

static void bui_set_tile(int col, int row, uint8_t tile) {
    if (col >= 0 && col < SCREEN_WIDTH && row >= 0 && row < SCREEN_HEIGHT) {
        gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
        wTileMap[row * SCREEN_WIDTH + col] = tile;
    }
}

static void bui_put_str(int col, int row, const char *s) {
    while (*s && col < SCREEN_WIDTH) {
        bui_set_tile(col++, row, (uint8_t)bui_char_to_tile((unsigned char)*s));
        s++;
    }
}

static uint8_t bui_reverse_bits(uint8_t v) {
    v = (uint8_t)(((v & 0xF0u) >> 4) | ((v & 0x0Fu) << 4));
    v = (uint8_t)(((v & 0xCCu) >> 2) | ((v & 0x33u) << 2));
    v = (uint8_t)(((v & 0xAAu) >> 1) | ((v & 0x55u) << 1));
    return v;
}

static void bui_clear_rows(int r0, int r1) {
    for (int r = r0; r <= r1; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            bui_set_tile(c, r, BLANK_TILE_SLOT);
}

static void bui_fill_rows_tile(int r0, int r1, uint8_t tile) {
    for (int r = r0; r <= r1; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            bui_set_tile(c, r, tile);
}

static void bui_fill_rows_tile_both(int r0, int r1, uint8_t tile) {
    bui_fill_rows_tile(r0, r1, tile);
    if (r0 < 0) r0 = 0;
    if (r1 >= SCREEN_HEIGHT) r1 = SCREEN_HEIGHT - 1;
    for (int r = r0; r <= r1; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            wTileMap[r * SCREEN_WIDTH + c] = tile;
}

static void bui_clear_rect(int c0, int r0, int c1, int r1) {
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++)
            bui_set_tile(c, r, BLANK_TILE_SLOT);
}

static void bui_put_num3(int col, int row, uint16_t val) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%3u", (unsigned)val);
    bui_put_str(col, row, buf);
}

static void bui_put_num2(int col, int row, uint16_t val) {
    char buf[3];
    snprintf(buf, sizeof(buf), "%2u", (unsigned)val);
    bui_put_str(col, row, buf);
}

static void bui_draw_levelup_stats(const levelup_stats_t *s) {

    const int c0 = 9, c1 = 19, r0 = 2, r1 = 11;
    bui_set_tile(c0, r0, (uint8_t)Font_CharToTile(0x79));
    for (int c = c0+1; c < c1; c++) bui_set_tile(c, r0, (uint8_t)Font_CharToTile(0x7A));
    bui_set_tile(c1, r0, (uint8_t)Font_CharToTile(0x7B));
    for (int r = r0+1; r < r1; r++) {
        bui_set_tile(c0, r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = c0+1; c < c1; c++) bui_set_tile(c, r, BLANK_TILE_SLOT);
        bui_set_tile(c1, r, (uint8_t)Font_CharToTile(0x7C));
    }
    bui_set_tile(c0, r1, (uint8_t)Font_CharToTile(0x7D));
    for (int c = c0+1; c < c1; c++) bui_set_tile(c, r1, (uint8_t)Font_CharToTile(0x7A));
    bui_set_tile(c1, r1, (uint8_t)Font_CharToTile(0x7E));

    bui_put_str(11, 3, "ATTACK");
    bui_put_str(11, 5, "DEFENSE");
    bui_put_str(11, 7, "SPEED");
    bui_put_str(11, 9, "SPECIAL");

    bui_put_num3(15, 4,  s->atk);
    bui_put_num3(15, 6,  s->def);
    bui_put_num3(15, 8,  s->spd);
    bui_put_num3(15, 10, s->spc);
}

#define ENEMY_SPR_TILE_BASE   0
#define PLAYER_SPR_BG_BASE    53
#define ENEMY_SPR_OAM_BASE    53

#define ENEMY_SPR_PX_X        96

#define VICTORY_TRAINER_PX_X  112
#define ENEMY_SPR_PX_Y        0

#define PLAYER_SPR_COL        1
#define PLAYER_SPR_ROW        5

#define PLAYER_SLIDE_OAM_BASE 4
#define PLAYER_SLIDE_TILE_BASE 53

#define POKEBALL_OAM_BASE         102
#define ENEMY_POKEBALL_OAM_BASE   108

#define MOVE_ANIM_STATUS_AFFECTED_ID 167u
#define MOVE_ANIM_POUND_ID           1u
#define POKEBALL_TILE_BASE    120

#define POKEBALL_OAM_Y        96

#define POKEBALL_OAM_X_START  96
#define POKEBALL_OAM_X_STEP   8

static uint8_t sEnemySpriteSavedY[49];
static uint8_t sEnemySpriteSavedX[49];
static uint8_t sEnemySpriteVisible = 1u;

uint16_t BattleUI_GetAnimOAMStart(void) {
    return 0u;
}

uint16_t BattleUI_GetAnimOAMEnd(void) {

    return 52u;
}

uint16_t BattleUI_GetEnemyOAMStart(void) {
    return ENEMY_SPR_OAM_BASE;
}

uint16_t BattleUI_GetEnemyOAMEnd(void) {
    return (uint16_t)(ENEMY_SPR_OAM_BASE + 48u);
}

void BattleUI_EnemySpriteCaptureState(void) {
    uint16_t i;
    uint16_t start = BattleUI_GetEnemyOAMStart();
    uint16_t end = BattleUI_GetEnemyOAMEnd();
    for (i = start; i <= end && i < MAX_SPRITES; i++) {
        uint16_t idx = i - start;
        if (idx >= 49u) break;
        sEnemySpriteSavedY[idx] = wShadowOAM[i].y;
        sEnemySpriteSavedX[idx] = wShadowOAM[i].x;
    }
    sEnemySpriteVisible = 1u;
}

void BattleUI_EnemySpriteSetVisible(uint8_t visible) {
    uint16_t i;
    uint16_t start = BattleUI_GetEnemyOAMStart();
    uint16_t end = BattleUI_GetEnemyOAMEnd();
    uint8_t old_visible = sEnemySpriteVisible;
    uint8_t new_visible = visible ? 1u : 0u;
    sEnemySpriteVisible = new_visible;
    for (i = start; i <= end && i < MAX_SPRITES; i++) {
        uint16_t idx = i - start;
        if (idx >= 49u) break;
        if (new_visible) {
            wShadowOAM[i].y = sEnemySpriteSavedY[idx];
            wShadowOAM[i].x = sEnemySpriteSavedX[idx];
        } else {

            if (old_visible) {
                sEnemySpriteSavedY[idx] = wShadowOAM[i].y;
                sEnemySpriteSavedX[idx] = wShadowOAM[i].x;
            }
            wShadowOAM[i].y = 0u;
        }
    }
}

uint8_t BattleUI_IsEnemySpriteVisible(void) {
    return sEnemySpriteVisible;
}

void BattleUI_EnemySpriteOffsetY(int8_t delta) {
    uint16_t i;
    uint16_t start = BattleUI_GetEnemyOAMStart();
    uint16_t end = BattleUI_GetEnemyOAMEnd();
    for (i = start; i <= end && i < MAX_SPRITES; i++) {
        uint16_t idx = i - start;
        int16_t y;
        if (idx >= 49u) break;
        if (wShadowOAM[i].y != 0u) {
            y = (int16_t)wShadowOAM[i].y + (int16_t)delta;
            if (y < 0) y = 0;
            if (y > 255) y = 255;
            wShadowOAM[i].y = (uint8_t)y;
            sEnemySpriteSavedY[idx] = wShadowOAM[i].y;
        } else {
            y = (int16_t)sEnemySpriteSavedY[idx] + (int16_t)delta;
            if (y < 0) y = 0;
            if (y > 255) y = 255;
            sEnemySpriteSavedY[idx] = (uint8_t)y;
        }
    }
}

const uint8_t *BattleUI_PlayerBackTile(int i) {
    return (wBattleType == 1) ? gOldManBackSprite[i] : gRedBackSprite[i];
}
#define player_back_tile BattleUI_PlayerBackTile

static uint8_t pokeball_tile_for(const party_mon_t *m) {
    if (m->base.hp == 0)     return (uint8_t)(POKEBALL_TILE_BASE + 2);
    if (m->base.status != 0) return (uint8_t)(POKEBALL_TILE_BASE + 1);
    return (uint8_t)(POKEBALL_TILE_BASE + 0);
}

static void bui_draw_pokeballs(void) {
    for (int i = 0; i < 4; i++)
        Display_LoadSpriteTile((uint8_t)(POKEBALL_TILE_BASE + i), kPokeballTiles[i]);

    bui_set_tile(18, 10, (uint8_t)Font_CharToTile(0x73));
    bui_set_tile(18, 11, (uint8_t)Font_CharToTile(0x77));
    for (int c = 10; c <= 17; c++)
        bui_set_tile(c, 11, (uint8_t)Font_CharToTile(0x76));
    bui_set_tile(9,  11, (uint8_t)Font_CharToTile(0x6F));

    for (int i = 0; i < PARTY_LENGTH; i++) {
        uint8_t tile = (i >= wPartyCount) ? (uint8_t)(POKEBALL_TILE_BASE + 3) : pokeball_tile_for(&wPartyMons[i]);
        wShadowOAM[POKEBALL_OAM_BASE + i].y     = POKEBALL_OAM_Y;
        wShadowOAM[POKEBALL_OAM_BASE + i].x     = (uint8_t)(POKEBALL_OAM_X_START + i * POKEBALL_OAM_X_STEP);
        wShadowOAM[POKEBALL_OAM_BASE + i].tile  = tile;
        wShadowOAM[POKEBALL_OAM_BASE + i].flags = 0;
    }

    if (wIsInBattle == 2) {
        bui_set_tile(1,  2, (uint8_t)Font_CharToTile(0x73));
        bui_set_tile(1,  3, (uint8_t)Font_CharToTile(0x74));
        for (int c = 2; c <= 9; c++)
            bui_set_tile(c, 3, (uint8_t)Font_CharToTile(0x76));
        bui_set_tile(10, 3, (uint8_t)Font_CharToTile(0x78));

        for (int i = 0; i < PARTY_LENGTH; i++) {
            uint8_t tile = (i >= wEnemyPartyCount) ? (uint8_t)(POKEBALL_TILE_BASE + 3) : pokeball_tile_for(&wEnemyMons[i]);

            wShadowOAM[ENEMY_POKEBALL_OAM_BASE + i].y     = 32;
            wShadowOAM[ENEMY_POKEBALL_OAM_BASE + i].x     = (uint8_t)(72 - i * 8);
            wShadowOAM[ENEMY_POKEBALL_OAM_BASE + i].tile  = tile;
            wShadowOAM[ENEMY_POKEBALL_OAM_BASE + i].flags = 0;
        }
    }
}

static void bui_hide_pokeballs(void) {
    for (int i = 0; i < PARTY_LENGTH; i++) {
        wShadowOAM[POKEBALL_OAM_BASE + i].y = 0;
        wShadowOAM[ENEMY_POKEBALL_OAM_BASE + i].y = 0;
    }
}

static void bui_place_enemy_sprite_full_oam(void) {
    for (int ty = 0; ty < 7; ty++) {
        for (int tx = 0; tx < 7; tx++) {
            int idx = ENEMY_SPR_OAM_BASE + ty * 7 + tx;
            wShadowOAM[idx].y     = (uint8_t)(ENEMY_SPR_PX_Y + ty * 8 + OAM_Y_OFS);
            wShadowOAM[idx].x     = (uint8_t)(ENEMY_SPR_PX_X + tx * 8 + OAM_X_OFS);
            wShadowOAM[idx].tile  = (uint8_t)(ENEMY_SPR_TILE_BASE + ty * 7 + tx);
            wShadowOAM[idx].flags = 0;
        }
    }
}

static int s_enemy_pic_kind = BUI_ENEMY_PIC_NONE;

int BattleUI_EnemyPicKind(void) { return s_enemy_pic_kind; }

static int s_enemy_on_field = 0;

int BattleUI_EnemyMonOnField(void) { return s_enemy_on_field; }

static int bui_johto_trainer_class(void) {
    int idx = (int)gEngagedJohtoParty - 1;
    if (idx < 0 || idx >= JOHTO_TRAINER_COUNT) return 0;
    int jc = gJohtoTrainers[idx].class_id;
    if (jc <= 0 || jc >= CRYSTAL_TRAINER_CLASS_COUNT) return 0;
    return gCrystalTrainerPicByClassValid[jc] ? jc : 0;
}

static int bui_has_gen2_pic(uint8_t species) {
    int g = Species_Dex(species);
    return g >= 152 && g < CRYSTAL_MON_COUNT;
}

static int bui_has_front_sprite(uint8_t species, uint8_t dex) {
    if (SpriteMod_GetFrontTile(species, 0) != NULL) return 1;
    if (dex > 0 && dex <= 151) return 1;
    return bui_has_gen2_pic(species);
}

static int bui_has_back_sprite(uint8_t species, uint8_t dex) {
    if (SpriteMod_GetBackTile(species, 0) != NULL) return 1;
    if (dex > 0 && dex <= 151) return 1;
    return bui_has_gen2_pic(species);
}

static uint8_t s_enemy_pic_species;

uint8_t BattleUI_EnemyPicSpecies(void) { return s_enemy_pic_species; }

static void bui_load_enemy_front_tiles(uint8_t species, uint8_t dex) {
    s_enemy_pic_species = species;

    if (bui_enemy_drawn_as_ghost()) {
        for (int i = 0; i < POKEMON_FRONT_CANVAS_TILES; i++)
            Display_LoadSpriteTile((uint8_t)(ENEMY_SPR_TILE_BASE + i),
                                   kGhostFrontSprite[i]);
        s_enemy_pic_kind = BUI_ENEMY_PIC_GHOST;
        return;
    }

    {
        int gdex = Species_Dex(species);
        if (gdex >= 152 && gdex < CRYSTAL_MON_COUNT) {
            const crystal_pic_t *p = &gCrystalMonPic[gdex];
            for (int i = 0; i < POKEMON_FRONT_CANVAS_TILES; i++) {
                const uint8_t *tile = SpriteMod_GetFrontTile(species, i);
                Display_LoadSpriteTile((uint8_t)(ENEMY_SPR_TILE_BASE + i),
                                       tile ? tile
                                            : gCrystalPicTiles[p->tile_base + i]);
            }
            s_enemy_pic_kind = BUI_ENEMY_PIC_MON;
            return;
        }
    }
    for (int i = 0; i < POKEMON_FRONT_CANVAS_TILES; i++) {
        const uint8_t *tile = SpriteMod_GetFrontTile(species, i);
        Display_LoadSpriteTile((uint8_t)(ENEMY_SPR_TILE_BASE + i),
                               tile ? tile : gPokemonFrontSprite[dex][i]);
    }
    s_enemy_pic_kind = BUI_ENEMY_PIC_MON;
}

static void bui_load_player_back_tiles(uint8_t species, uint8_t dex) {

    int gdex = Species_Dex(species);
    if (gdex >= 152 && gdex < CRYSTAL_MON_COUNT) {
        for (int i = 0; i < POKEMON_BACK_TILES; i++) {
            const uint8_t *tile = SpriteMod_GetBackTile(species, i);
            Display_LoadTile((uint8_t)(PLAYER_SPR_BG_BASE + i),
                             tile ? tile : gCrystalMonBackPic[gdex][i]);
        }
        return;
    }
    for (int i = 0; i < POKEMON_BACK_TILES; i++) {
        const uint8_t *tile = SpriteMod_GetBackTile(species, i);
        Display_LoadTile((uint8_t)(PLAYER_SPR_BG_BASE + i),
                         tile ? tile : gPokemonBackSprite[dex][i]);
    }
}

static void bui_load_sprites(void) {
    uint8_t e_dex = gSpeciesToDex[wEnemyMon.species];
    uint8_t p_dex = gSpeciesToDex[wBattleMon.species];

    if (!(wEnemyBattleStatus1 & (1u << BSTAT1_INVULNERABLE)))  s_enemy_charge_hidden = 0;
    if (!(wPlayerBattleStatus1 & (1u << BSTAT1_INVULNERABLE))) s_player_charge_hidden = 0;

    uint8_t enemy_hidden = (uint8_t)((wEnemyBattleStatus1 & (1u << BSTAT1_INVULNERABLE)) ||
                                     s_enemy_charge_hidden);
    uint8_t player_hidden = (uint8_t)((wPlayerBattleStatus1 & (1u << BSTAT1_INVULNERABLE)) ||
                                      s_player_charge_hidden);

    if ((wPlayerBattleStatus1 & (1u << BSTAT1_CHARGING_UP)) ||
        (wEnemyBattleStatus1 & (1u << BSTAT1_CHARGING_UP)) ||
        s_player_charge_hidden || s_enemy_charge_hidden ||
        s_player_charge_resolving_anim || s_enemy_charge_resolving_anim) {
        printf("[DIGDBG] load_sprites p_b1=0x%02X e_b1=0x%02X p_hidden=%u e_hidden=%u p_latch=%d e_latch=%d p_resolve=%d e_resolve=%d\n",
               wPlayerBattleStatus1, wEnemyBattleStatus1,
               player_hidden, enemy_hidden,
               s_player_charge_hidden, s_enemy_charge_hidden,
               s_player_charge_resolving_anim, s_enemy_charge_resolving_anim);
    }

    if (bui_has_front_sprite(wEnemyMon.species, e_dex)) {
        bui_load_enemy_front_tiles(wEnemyMon.species, e_dex);
        bui_place_enemy_sprite_full_oam();
        BattleUI_EnemySpriteCaptureState();
        if (enemy_hidden)
            BattleUI_EnemySpriteSetVisible(0u);
    }

    if (wBattleType == 2 || wBattleType == 1) {
        for (int i = 0; i < 49; i++)
            Display_LoadTile((uint8_t)(PLAYER_SPR_BG_BASE + i), player_back_tile(i));
        bui_hide_player_slide_oam();
        bui_place_player_sprite();
        return;
    }

    if (bui_has_back_sprite(wBattleMon.species, p_dex)) {
        bui_load_player_back_tiles(wBattleMon.species, p_dex);

        bui_hide_player_slide_oam();
        if (player_hidden)
            bui_hide_player_sprite();
        else
            bui_place_player_sprite();
    }
}

static void bui_place_player_sprite(void) {
    for (int ty = 0; ty < 7; ty++) {
        for (int tx = 0; tx < 7; tx++) {
            int col = PLAYER_SPR_COL + tx;
            int row = PLAYER_SPR_ROW + ty;
            uint8_t tile = (uint8_t)(PLAYER_SPR_BG_BASE + ty * 7 + tx);
            uint16_t sidx = (uint16_t)(row + 2) * SCROLL_MAP_W + (uint16_t)(col + 2) + Map_UiColOfs();
            uint16_t tidx = (uint16_t)row * SCREEN_WIDTH + (uint16_t)col;
            if (sidx < (SCROLL_MAP_W * SCROLL_MAP_H))
                gScrollTileMap[sidx] = tile;
            if (tidx < SCREEN_AREA)
                wTileMap[tidx] = tile;
        }
    }
}

static const int kDownscale3[3] = {0, 3, 6};
static const int kDownscale5[5] = {0, 1, 3, 5, 6};

static void bui_place_player_grow_stage(int stage) {
    uint8_t p_dex = gSpeciesToDex[wBattleMon.species];
    bui_clear_rect(1, 5, 7, 11);
    if (!bui_has_back_sprite(wBattleMon.species, p_dex)) return;

    if (stage == 1) {
        for (int dty = 0; dty < 3; dty++)
            for (int dtx = 0; dtx < 3; dtx++)
                bui_set_tile(3 + dtx, 9 + dty,
                    (uint8_t)(PLAYER_SPR_BG_BASE + kDownscale3[dty] * 7 + kDownscale3[dtx]));
        return;
    }

    if (stage == 2) {
        for (int dty = 0; dty < 5; dty++)
            for (int dtx = 0; dtx < 5; dtx++)
                bui_set_tile(2 + dtx, 7 + dty,
                    (uint8_t)(PLAYER_SPR_BG_BASE + kDownscale5[dty] * 7 + kDownscale5[dtx]));
        return;
    }

    bui_place_player_sprite();
}

static void bui_hide_player_sprite(void) {
    for (int ty = 0; ty < 7; ty++) {
        for (int tx = 0; tx < 7; tx++) {
            int col = PLAYER_SPR_COL + tx;
            int row = PLAYER_SPR_ROW + ty;
            uint16_t sidx = (uint16_t)(row + 2) * SCROLL_MAP_W + (uint16_t)(col + 2) + Map_UiColOfs();
            uint16_t tidx = (uint16_t)row * SCREEN_WIDTH + (uint16_t)col;
            if (sidx < (SCROLL_MAP_W * SCROLL_MAP_H))
                gScrollTileMap[sidx] = BLANK_TILE_SLOT;
            if (tidx < SCREEN_AREA)
                wTileMap[tidx] = BLANK_TILE_SLOT;
        }
    }
}

static void bui_hide_player_slide_oam(void) {
    s_poof_oam_live = 0;
    for (int i = 0; i < 49; i++) {
        wShadowOAM[PLAYER_SLIDE_OAM_BASE + i].y = 0;
    }
}

int BattleUI_HudOverlayActive(void) {
    switch (bui_state) {
    case BUI_MOVE_SELECT:
    case BUI_MIMIC_SELECT:
    case BUI_BAG_BATTLE:
    case BUI_ITEM_TARGET:
    case BUI_ITEM_HEAL_ANIM:
    case BUI_PARTY_SELECT:
    case BUI_SWITCH_SELECT:
    case BUI_SHIFT_PARTY:
    case BUI_LEVELUP_STATS:
    case BUI_LEARN_FORGET_YESNO:
    case BUI_LEARN_PICK_MOVE:
    case BUI_LEARN_STOP_YESNO:
    case BUI_CAUGHT_NICKNAME:
    case BUI_CAUGHT_NICK_WAIT:
    case BUI_EVOLUTION:
        return 1;
    default:
        return 0;
    }
}

int BattleUI_PoofRomTile(uint8_t sprite_tile) {
    static const uint8_t kRom[10] = {0x20, 0x21, 0x23, 0x24, 0x25,
                                     0x30, 0x31, 0x32, 0x33, 0x34};
    if (!s_poof_oam_live) return -1;
    if (sprite_tile < POOF_TILE_BASE) return -1;
    if (sprite_tile >= POOF_TILE_BASE + (int)(sizeof kRom)) return -1;
    return kRom[sprite_tile - POOF_TILE_BASE];
}

static int bui_hpbar_pixels(int hp, int max_hp) {
    if (max_hp <= 0) return 0;
    long prod = (long)hp * 48;
    int divisor = max_hp;
    if (max_hp > 255) { prod >>= 2; divisor >>= 2; }
    int px = (divisor > 0) ? (int)(prod / divisor) : 0;
    if (px < 0) px = 0;
    if (px > 48) px = 48;
    if (hp > 0 && px == 0) px = 1;
    return px;
}

static void bui_draw_hp_bar(int col, int row, int hp, int max_hp) {
    int pixels = bui_hpbar_pixels(hp, max_hp);
    for (int i = 0; i < 6; i++) {
        int seg = pixels - i * 8;
        uint8_t tile;
        if (seg <= 0)       tile = 0x63;
        else if (seg >= 8)  tile = 0x6B;
        else                tile = (uint8_t)(0x63 + seg);
        bui_set_tile(col + i, row, (uint8_t)Font_CharToTile(tile));
    }
}

static int calc_hp_pixels(int hp, int max_hp) {
    return bui_hpbar_pixels(hp, max_hp);
}

static void bui_draw_hp_bar_px(int col, int row, int pixels) {

    if (col == 4 && row == 2) {
        static int last = -999;
        if (pixels != last) {
            last = pixels;
            printf("[DRAINDBG] tile enemy px=%d hold=%d live=%u anim=%d who=%d cur=%d\n",
                   pixels, s_enemy_hp_bar_draw, (unsigned)wEnemyMon.hp,
                   s_hp_anim_active, s_hp_anim_who, s_hp_cur_hp);
            fflush(stdout);
        }
    }
    for (int i = 0; i < 6; i++) {
        int seg = pixels - i * 8;
        uint8_t tile;
        if (seg <= 0)       tile = 0x63;
        else if (seg >= 8)  tile = 0x6B;
        else                tile = (uint8_t)(0x63 + seg);
        bui_set_tile(col + i, row, (uint8_t)Font_CharToTile(tile));
    }
}

static void bui_put_level(int col, int row, int level) {
    if (level >= 100) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", level);
        bui_put_str(col, row, buf);
    } else {
        bui_set_tile(col, row, (uint8_t)Font_CharToTile(0x6E));
        char buf[3];
        snprintf(buf, sizeof(buf), "%-2d", level);
        bui_put_str(col + 1, row, buf);
    }
}

static int bui_put_status_condition(int col, int row, uint8_t status) {
    const char *label = NULL;
    if (status & STATUS_PSN)
        label = "PSN";
    else if (status & STATUS_BRN)
        label = "BRN";
    else if (status & STATUS_FRZ)
        label = "FRZ";
    else if (status & STATUS_PAR)
        label = "PAR";
    else if (status & STATUS_SLP_MASK)
        label = "SLP";

    if (!label)
        return 0;
    bui_put_str(col, row, label);
    return 1;
}

static const char *bui_status_name(uint8_t status) {
    if (status & STATUS_PSN) return "PSN";
    if (status & STATUS_BRN) return "BRN";
    if (status & STATUS_FRZ) return "FRZ";
    if (status & STATUS_PAR) return "PAR";
    if (status & STATUS_SLP_MASK) return "SLP";
    return "OK";
}

static void bui_log_hud_status(const char *tag, const char *who, uint8_t status, uint8_t level,
                               int printed_status) {
    printf("[HUDDBG] %s %s status=0x%02X(%s) level=%u printed=%s\n",
           tag, who, status, bui_status_name(status), (unsigned)level,
           printed_status ? "status" : "level");
}

static void bui_draw_box(void) {
    const int r0 = 12, r1 = 17;
    bui_set_tile(0,              r0, (uint8_t)Font_CharToTile(0x79));
    for (int c = 1; c < SCREEN_WIDTH - 1; c++)
        bui_set_tile(c,          r0, (uint8_t)Font_CharToTile(0x7A));
    bui_set_tile(SCREEN_WIDTH-1, r0, (uint8_t)Font_CharToTile(0x7B));
    for (int r = r0+1; r < r1; r++) {
        bui_set_tile(0,              r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = 1; c < SCREEN_WIDTH-1; c++)
            bui_set_tile(c,          r, BLANK_TILE_SLOT);
        bui_set_tile(SCREEN_WIDTH-1, r, (uint8_t)Font_CharToTile(0x7C));
    }
    bui_set_tile(0,              r1, (uint8_t)Font_CharToTile(0x7D));
    for (int c = 1; c < SCREEN_WIDTH - 1; c++)
        bui_set_tile(c,          r1, (uint8_t)Font_CharToTile(0x7A));
    bui_set_tile(SCREEN_WIDTH-1, r1, (uint8_t)Font_CharToTile(0x7E));
}

static uint16_t bui_hud_hp(int side, uint16_t live) {
    if (s_hp_hold_active && s_hp_anim_who == side) return (uint16_t)s_hp_cur_hp;
    if (side == 0 && s_enemy_hp_bar_draw  >= 0) return (uint16_t)s_enemy_hp_bar_draw;
    if (side == 1 && s_player_hp_bar_draw >= 0) return (uint16_t)s_player_hp_bar_draw;
    return live;
}

int BattleUI_CenterMonNameOffset(int len) {
    int off = 2;
    if (len >= 3) off--;
    if (len >= 5) off--;
    return off;
}

static int bui_name_len(const char *s) {
    int n = 0;
    while (n < 10 && s && s[n]) n++;
    return n;
}

static int s_player_name_centered = 1;

void BattleUI_SetPlayerNameCentered(int on) { s_player_name_centered = on ? 1 : 0; }

int BattleUI_PlayerMonIsOut(void) {
    return !(wBattleType == 1 || wBattleType == 2);
}

static void bui_draw_enemy_hud(void) {
    bui_clear_rect(0, 0, 11, 3);

    bui_set_tile(1,  2, (uint8_t)Font_CharToTile(0x73));
    bui_set_tile(1,  3, (uint8_t)Font_CharToTile(0x74));
    for (int c = 2; c <= 9; c++)
        bui_set_tile(c, 3, (uint8_t)Font_CharToTile(0x76));
    bui_set_tile(10, 3, (uint8_t)Font_CharToTile(0x78));

    const char *name = bui_enemy_mon_name();
    bui_put_str(1 + BattleUI_CenterMonNameOffset(bui_name_len(name)), 0, name);

    int printed_status = bui_put_status_condition(5, 1, wEnemyMon.status);
    if (!printed_status)
        bui_put_level(4, 1, wEnemyMon.level);
    bui_log_hud_status("draw_enemy", "enemy", wEnemyMon.status, wEnemyMon.level, printed_status);

    bui_set_tile(2,  2, (uint8_t)Font_CharToTile(0x71));
    bui_set_tile(3,  2, (uint8_t)Font_CharToTile(0x62));
    bui_draw_hp_bar(4, 2, bui_hud_hp(0, wEnemyMon.hp), wEnemyMon.max_hp);
    bui_set_tile(10, 2, (uint8_t)Font_CharToTile(0x6C));
}

static void bui_draw_player_hud(void) {
    bui_clear_rect(9, 7, 19, 11);

    bui_set_tile(18, 10, (uint8_t)Font_CharToTile(0x73));
    bui_set_tile(18, 11, (uint8_t)Font_CharToTile(0x77));
    for (int c = 10; c <= 17; c++)
        bui_set_tile(c, 11, (uint8_t)Font_CharToTile(0x76));
    bui_set_tile(9,  11, (uint8_t)Font_CharToTile(0x6F));

    const char *name = bui_player_mon_name();
    int noff = s_player_name_centered
                   ? BattleUI_CenterMonNameOffset(bui_name_len(name)) : 0;
    bui_put_str(10 + noff, 7, name);

    int printed_status = bui_put_status_condition(15, 8, wBattleMon.status);
    if (!printed_status)
        bui_put_level(14, 8, wBattleMon.level);
    bui_log_hud_status("draw_player", "player", wBattleMon.status, wBattleMon.level, printed_status);

    bui_set_tile(10, 9, (uint8_t)Font_CharToTile(0x71));
    bui_set_tile(11, 9, (uint8_t)Font_CharToTile(0x62));
    bui_draw_hp_bar(12, 9, bui_hud_hp(1, wBattleMon.hp), wBattleMon.max_hp);
    bui_set_tile(18, 9, (uint8_t)Font_CharToTile(0x6D));

    char hbuf[4];
    snprintf(hbuf, sizeof(hbuf), "%3d", bui_hud_hp(1, wBattleMon.hp));
    bui_put_str(11, 10, hbuf);
    bui_set_tile(14, 10, (uint8_t)bui_char_to_tile('/'));
    snprintf(hbuf, sizeof(hbuf), "%3d", wBattleMon.max_hp);
    bui_put_str(15, 10, hbuf);

    if (wBattleMon.hp == 0)
        Audio_SetLowHealthAlarm(0);
    else
        Audio_SetLowHealthAlarm(bui_hpbar_pixels(wBattleMon.hp,
                                                 wBattleMon.max_hp) < 10);
}

static void bui_draw_battle_menu_box(void) {
    const int c0 = 8, c1 = 19, r0 = 12, r1 = 17;
    bui_set_tile(c0,             r0, (uint8_t)Font_CharToTile(0x79));
    for (int c = c0+1; c < c1; c++)
        bui_set_tile(c,          r0, (uint8_t)Font_CharToTile(0x7A));
    bui_set_tile(c1,             r0, (uint8_t)Font_CharToTile(0x7B));
    for (int r = r0+1; r < r1; r++) {
        bui_set_tile(c0,         r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = c0+1; c < c1; c++)
            bui_set_tile(c,      r, BLANK_TILE_SLOT);
        bui_set_tile(c1,         r, (uint8_t)Font_CharToTile(0x7C));
    }
    bui_set_tile(c0,             r1, (uint8_t)Font_CharToTile(0x7D));
    for (int c = c0+1; c < c1; c++)
        bui_set_tile(c,          r1, (uint8_t)Font_CharToTile(0x7A));
    bui_set_tile(c1,             r1, (uint8_t)Font_CharToTile(0x7E));
}

static void bui_draw_safari_menu(int cursor) {
    bui_draw_box();

    bui_put_str(2, 14, "BALL");
    bui_set_tile(6, 14, (uint8_t)Font_CharToTile(0xF1));
    bui_put_num2(7, 14, wNumSafariBalls);
    bui_put_str(13, 14, "BAIT");
    bui_put_str(2, 16, "THROW ROCK");
    bui_put_str(13, 16, "RUN");

    switch (cursor) {
    case 0: bui_set_tile(1, 14, (uint8_t)Font_CharToTile(0xED)); break;
    case 1: bui_set_tile(12, 14, (uint8_t)Font_CharToTile(0xED)); break;
    case 2: bui_set_tile(1, 16, (uint8_t)Font_CharToTile(0xED)); break;
    case 3: bui_set_tile(12, 16, (uint8_t)Font_CharToTile(0xED)); break;
    }
}

static void bui_draw_main_menu(int cursor) {
    if (wBattleType == 2) {
        bui_draw_safari_menu(cursor);
        return;
    }
    bui_draw_box();
    bui_draw_battle_menu_box();

    bui_put_str(9,  14, cursor == 0 ? ">FIGHT" : " FIGHT");
    bui_set_tile(15, 14, cursor == 1 ? (uint8_t)bui_char_to_tile('>') : BLANK_TILE_SLOT);
    bui_set_tile(16, 14, (uint8_t)Font_CharToTile(0xE1));
    bui_set_tile(17, 14, (uint8_t)Font_CharToTile(0xE2));

    bui_put_str(9,  16, cursor == 2 ? ">ITEM" : " ITEM");
    bui_put_str(15, 16, cursor == 3 ? ">RUN" : " RUN");
}

static void bui_draw_move_box(void) {
    const int c0 = 4, c1 = 19, r0 = 12, r1 = 17;

    bui_set_tile(c0,             r0, (uint8_t)Font_CharToTile(0x7A));
    for (int c = c0+1; c < c1; c++)
        bui_set_tile(c,          r0, (uint8_t)Font_CharToTile(0x7A));
    bui_set_tile(10,             r0, (uint8_t)Font_CharToTile(0x7E));
    bui_set_tile(c1,             r0, (uint8_t)Font_CharToTile(0x7B));

    for (int r = r0+1; r < r1; r++) {
        bui_set_tile(c0,         r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = c0+1; c < c1; c++)
            bui_set_tile(c,      r, BLANK_TILE_SLOT);
        bui_set_tile(c1,         r, (uint8_t)Font_CharToTile(0x7C));
    }

    bui_set_tile(c0,             r1, (uint8_t)Font_CharToTile(0x7D));
    for (int c = c0+1; c < c1; c++)
        bui_set_tile(c,          r1, (uint8_t)Font_CharToTile(0x7A));
    bui_set_tile(c1,             r1, (uint8_t)Font_CharToTile(0x7E));
}

static const char *bui_type_name(uint8_t type) {
    switch (type) {
        case TYPE_NORMAL:    return "NORMAL";
        case TYPE_FIGHTING:  return "FIGHTNG";
        case TYPE_FLYING:    return "FLYING";
        case TYPE_POISON:    return "POISON";
        case TYPE_GROUND:    return "GROUND";
        case TYPE_ROCK:      return "ROCK";
        case TYPE_BIRD:      return "BIRD";
        case TYPE_BUG:       return "BUG";
        case TYPE_GHOST:     return "GHOST";
        case TYPE_FIRE:      return "FIRE";
        case TYPE_WATER:     return "WATER";
        case TYPE_GRASS:     return "GRASS";
        case TYPE_ELECTRIC:  return "ELECTRC";
        case TYPE_PSYCHIC:   return "PSYCHIC";
        case TYPE_ICE:       return "ICE";
        case TYPE_DRAGON:    return "DRAGON";
        default:             return "?????";
    }
}

static void bui_draw_pp_info_box(int cursor) {
    const int c0 = 0, c1 = 10, r0 = 8, r1 = 12;

    bui_set_tile(c0,             r0, (uint8_t)Font_CharToTile(0x79));
    for (int c = c0+1; c < c1; c++)
        bui_set_tile(c,          r0, (uint8_t)Font_CharToTile(0x7A));
    bui_set_tile(c1,             r0, (uint8_t)Font_CharToTile(0x7B));

    for (int r = r0+1; r < r1; r++) {
        bui_set_tile(c0,         r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = c0+1; c < c1; c++)
            bui_set_tile(c,      r, BLANK_TILE_SLOT);
        bui_set_tile(c1,         r, (uint8_t)Font_CharToTile(0x7C));
    }

    bui_set_tile(c0,             r1, (uint8_t)Font_CharToTile(0x7D));
    for (int c = c0+1; c < c1; c++)
        bui_set_tile(c,          r1, (uint8_t)Font_CharToTile(0x7A));
    bui_set_tile(c1,             r1, (uint8_t)Font_CharToTile(0x7E));

    int move_id = wBattleMon.moves[cursor];

    if (move_id) {

        hWhoseTurn = 0;
        wPlayerSelectedMove  = (uint8_t)move_id;
        wPlayerMoveListIndex = (uint8_t)cursor;
        Battle_GetCurrentMove();
    }

    if (!move_id) {

        bui_put_str(1, 9, "TYPE");
        bui_set_tile(5, 9, (uint8_t)bui_char_to_tile('/'));

        bui_put_str(5, 11, "--/--");
        return;
    }

    bui_put_str(1, 9, "TYPE");
    bui_set_tile(5, 9, (uint8_t)bui_char_to_tile('/'));

    bui_put_str(2, 10, bui_type_name(gMoves[move_id].type));

    uint8_t cur_pp = wBattleMon.pp[cursor] & 0x3F;
    uint8_t max_pp = gMoves[move_id].pp;
    char pp[6];
    snprintf(pp, sizeof(pp), "%2d", cur_pp);
    bui_put_str(5, 11, pp);
    bui_set_tile(7, 11, (uint8_t)bui_char_to_tile('/'));
    snprintf(pp, sizeof(pp), "%2d", max_pp);
    bui_put_str(8, 11, pp);
}

static void bui_clear_move_menu_overlay(void) {
    bui_clear_rect(0, 8, 19, 17);
    bui_place_player_sprite();
    bui_draw_player_hud();
    bui_draw_box();
}

static void bui_draw_mimic_menu(int cursor) {
    bui_draw_move_box();
    for (int i = 0; i < 4; i++) {
        char buf[14];
        char prefix = (cursor == i) ? '>' : ' ';
        if (wEnemyMon.moves[i]) {
            const char *mn = gMoveNames[wEnemyMon.moves[i]];
            snprintf(buf, sizeof(buf), "%c%-12s", prefix, mn ? mn : "-");
        } else {
            snprintf(buf, sizeof(buf), "%c-", prefix);
        }
        buf[sizeof(buf) - 1] = 0;
        bui_put_str(5, 13 + i, buf);
    }
}

static void bui_draw_move_menu(int cursor) {
    bui_draw_move_box();
    bui_draw_pp_info_box(cursor);
    for (int i = 0; i < 4; i++) {
        char buf[14];
        char prefix = (cursor == i) ? '>' : ' ';

        if (wBattleMon.moves[i]) {
            const char *mn = gMoveNames[wBattleMon.moves[i]];
            snprintf(buf, sizeof(buf), "%c%-12s", prefix, mn ? mn : "-");
        } else {

            snprintf(buf, sizeof(buf), "%c-", prefix);
        }
        buf[13] = '\0';
        bui_put_str(5, 13 + i, buf);
    }
}

static const char *bui_party_mon_name(uint8_t slot) {
    static char s_name[NAME_LENGTH + 1];
    if (slot < PARTY_LENGTH && slot < wPartyCount) {
        const uint8_t *nick = wPartyMonNicks[slot];
        if (nick[0] != 0x00 && nick[0] != 0x50) {
            int out = 0;
            for (int i = 0; i < NAME_LENGTH - 1 && out < NAME_LENGTH; i++) {
                uint8_t c = nick[i];
                if (c == 0x50) break;
                if      (c >= 0x80 && c <= 0x99) s_name[out++] = (char)('A' + (c - 0x80));
                else if (c >= 0xA0 && c <= 0xB9) s_name[out++] = (char)('a' + (c - 0xA0));
                else if (c >= 0xF6)              s_name[out++] = (char)('0' + (c - 0xF6));
                else if (c == 0x7F)              s_name[out++] = ' ';
                else if (c == 0xE8)              s_name[out++] = '.';
                else if (c == 0xE7)              s_name[out++] = '!';
                else if (c == 0xE6)              s_name[out++] = '?';
                else if (c == 0xE3)              s_name[out++] = '-';
                else if (c == 0xE0)              s_name[out++] = '\'';
            }
            s_name[out] = '\0';
            if (out > 0) return s_name;
        }
        return Pokemon_GetName(Species_Dex(wPartyMons[slot].base.species));
    }
    return "";
}

static int is_hm_move(uint8_t move_id) {
    static const uint8_t kHmMoves[] = {
        MOVE_CUT, MOVE_FLY, MOVE_SURF, MOVE_STRENGTH, MOVE_FLASH
    };
    for (int i = 0; i < (int)(sizeof(kHmMoves) / sizeof(kHmMoves[0])); i++) {
        if (kHmMoves[i] == move_id) return 1;
    }
    return 0;
}

static const char *bui_move_name(uint8_t move_id) {
    return (move_id && move_id < NUM_MOVE_DEFS && gMoveNames[move_id])
               ? gMoveNames[move_id] : "a move";
}

static uint8_t s_learn_screen_bak[SCREEN_HEIGHT * SCREEN_WIDTH];
static int     s_learn_screen_saved = 0;

static void bui_learn_save_screen(void) {
    for (int i = 0; i < SCREEN_HEIGHT * SCREEN_WIDTH; i++)
        s_learn_screen_bak[i] = wTileMap[i];
    s_learn_screen_saved = 1;
}

static void bui_learn_restore_rect(int c0, int r0, int c1, int r1) {
    if (!s_learn_screen_saved) return;
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++)
            bui_set_tile(c, r, s_learn_screen_bak[r * SCREEN_WIDTH + c]);
}

static void bui_draw_learn_yesno(int cursor) {
    bui_set_tile(14, 7,  (uint8_t)Font_CharToTile(0x79));
    bui_set_tile(19, 7,  (uint8_t)Font_CharToTile(0x7B));
    bui_set_tile(14, 11, (uint8_t)Font_CharToTile(0x7D));
    bui_set_tile(19, 11, (uint8_t)Font_CharToTile(0x7E));
    for (int c = 15; c < 19; c++) {
        bui_set_tile(c, 7,  (uint8_t)Font_CharToTile(0x7A));
        bui_set_tile(c, 11, (uint8_t)Font_CharToTile(0x7A));
    }
    for (int r = 8; r <= 10; r++) {
        bui_set_tile(14, r, (uint8_t)Font_CharToTile(0x7C));
        bui_set_tile(19, r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = 15; c < 19; c++) bui_set_tile(c, r, BLANK_TILE_SLOT);
    }
    bui_put_str(16, 8,  "YES");
    bui_put_str(16, 10, "NO");
    bui_set_tile(15, 8,  (uint8_t)Font_CharToTile(cursor == 0 ? 0xED : 0x7F));
    bui_set_tile(15, 10, (uint8_t)Font_CharToTile(cursor == 1 ? 0xED : 0x7F));
}

static void bui_draw_learn_move_list(uint8_t slot, int cursor) {
    bui_set_tile(4,  7,  (uint8_t)Font_CharToTile(0x79));
    bui_set_tile(19, 7,  (uint8_t)Font_CharToTile(0x7B));
    bui_set_tile(4,  12, (uint8_t)Font_CharToTile(0x7D));
    bui_set_tile(19, 12, (uint8_t)Font_CharToTile(0x7E));
    for (int c = 5; c < 19; c++) {
        bui_set_tile(c, 7,  (uint8_t)Font_CharToTile(0x7A));
        bui_set_tile(c, 12, (uint8_t)Font_CharToTile(0x7A));
    }
    for (int r = 8; r <= 11; r++) {
        bui_set_tile(4,  r, (uint8_t)Font_CharToTile(0x7C));
        bui_set_tile(19, r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = 5; c < 19; c++) bui_set_tile(c, r, BLANK_TILE_SLOT);
    }
    for (int i = 0; i < NUM_MOVES; i++) {
        uint8_t move = wPartyMons[slot].base.moves[i];
        bui_put_str(6, 8 + i, move ? bui_move_name(move) : "-");
        bui_set_tile(5, 8 + i, (uint8_t)Font_CharToTile(cursor == i ? 0xED : 0x7F));
    }
}

static void bui_open_party_select(void) {

    bui_set_enemy_oam_visible(0);
    bui_hide_player_slide_oam();
    PartyMenu_Open(1 );
    bui_state = BUI_PARTY_SELECT;
}

static void bui_restart_move_anim_replay(int whose);

static int bui_advance_hit(void) {
    if (bui_hit_replay_count() == 0u || s_hit_index >= bui_hit_replay_count())
        return 0;

    if (!s_hit_text_shown && s_hit_text[s_hit_index][0]) {
        snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", s_hit_text[s_hit_index]);
        s_hit_text_shown = 1u;
        bui_show_text(s_msg_buf);
        return 1;
    }
    s_hit_text_shown = 0u;
    s_hit_index++;
    if (s_hit_index < bui_hit_replay_count()) {
        s_anim_frame = 0;
        s_move_anim_hit_sfx_started = 0;
        s_hit_scroll_pending = 1;
        bui_restart_move_anim_replay((int)s_hit_anim_whose);
        return 1;
    }
    return 0;
}

static int bui_status_msg_to_page(battle_status_msg_t msg, uint8_t side,
                                  const char *name, char *out, size_t outsz) {
    if (!out || outsz == 0 || !name) return 0;
    out[0] = '\0';
    switch (msg) {
    case BSTAT_MSG_FAST_ASLEEP:
        snprintf(out, outsz, "%s\nis fast asleep!", name);
        return 1;
    case BSTAT_MSG_WOKE_UP:
        snprintf(out, outsz, "%s\nwoke up!", name);
        return 1;
    case BSTAT_MSG_FROZEN:
        snprintf(out, outsz, "%s\nis frozen solid!", name);
        return 1;
    case BSTAT_MSG_CANT_MOVE:
        snprintf(out, outsz, "%s\ncan't move!", name);
        return 1;
    case BSTAT_MSG_FLINCHED:
        snprintf(out, outsz, "%s\nflinched!", name);
        return 1;
    case BSTAT_MSG_MUST_RECHARGE:
        snprintf(out, outsz, "%s\nmust recharge!", name);
        return 1;
    case BSTAT_MSG_HURT_ITSELF:
        snprintf(out, outsz, "It hurt itself\nin its confusion!");
        return 1;

    case BSTAT_MSG_TOO_SCARED:
        snprintf(out, outsz, "%s is too\nscared to move!", name);
        return 1;
    case BSTAT_MSG_GET_OUT:
        snprintf(out, outsz, "%s", RomText("_GetOutText"));
        return 1;
    case BSTAT_MSG_FULLY_PARALYZED:
        snprintf(out, outsz, "%s's\nfully paralyzed!", name);
        return 1;
    case BSTAT_MSG_MOVE_DISABLED: {

        uint8_t dis = (side == 0u) ? wPlayerDisabledMoveNumber
                                   : wEnemyDisabledMoveNumber;
        const char *dn = (dis && dis < 166 && gMoveNames[dis]) ? gMoveNames[dis] : "move";
        snprintf(out, outsz, "%s's\n%s is\ndisabled!", name, dn);
        return 1;
    }
    case BSTAT_MSG_IS_CONFUSED:
        snprintf(out, outsz, "%s\nis confused!", name);
        return 1;
    case BSTAT_MSG_DISABLED_NO_MORE:
        snprintf(out, outsz, "%s's\ndisabled no more!", name);
        return 1;
    case BSTAT_MSG_CONFUSED_NO_MORE:
        snprintf(out, outsz, "%s's\nconfused no more!", name);
        return 1;
    default:
        return 0;
    }
}

static int bui_collect_status_pages(uint8_t side, uint8_t is_pre,
                                    const char *name,
                                    char *out, size_t outsz) {
    size_t pos = 0;
    int pages = 0;
    char page[64];
    uint8_t i;

    if (!out || outsz == 0 || side > 1u) return 0;
    out[0] = '\0';

    for (i = 0; i < s_evt_status_q_count[side]; i++) {
        bui_status_evt_t *ev = &s_evt_status_q[side][i];
        if (ev->is_pre != (is_pre ? 1u : 0u)) continue;
        if (!bui_status_msg_to_page(ev->msg, side, name, page, sizeof(page))) continue;
        if (pages > 0 && pos + 1 < outsz) out[pos++] = '\f';
        if (pos < outsz) {
            size_t n = snprintf(out + pos, outsz - pos, "%s", page);
            if (n >= outsz - pos) {
                out[outsz - 1] = '\0';
                return pages + 1;
            }
            pos += n;
        }
        pages++;
    }

    if (pages == 0) {
        battle_status_msg_t msg = BSTAT_MSG_NONE;
        if (is_pre) {
            msg = (side == 0u) ? Battle_GetPlayerPreStatusMsg() : Battle_GetEnemyPreStatusMsg();
        } else {
            msg = (side == 0u) ? Battle_GetPlayerStatusMsg() : Battle_GetEnemyStatusMsg();
        }
        if (msg != BSTAT_MSG_NONE && bui_status_msg_to_page(msg, side, name, page, sizeof(page))) {
            snprintf(out, outsz, "%s", page);
            pages = 1;
        }
    }

    return pages;
}

static void bui_append_text_page(char *dst, size_t dstsz, int *pos, const char *fmt, ...) {
    if (!dst || !pos || dstsz == 0 || *pos < 0 || (size_t)*pos >= dstsz) return;
    if (*pos > 0)
        *pos += snprintf(dst + *pos, dstsz - (size_t)*pos, "\f");
    if ((size_t)*pos >= dstsz) return;

    va_list ap;
    va_start(ap, fmt);
    *pos += vsnprintf(dst + *pos, dstsz - (size_t)*pos, fmt, ap);
    va_end(ap);
}

static void bui_show_after_move(int whose, const char *pfx, const char *name,
                                 uint8_t selected, uint8_t move_id,
                                 uint8_t crit, uint8_t missed, uint8_t eff) {
    char pre_buf[192];
    char block_buf[192];

    char full_name[40];
    snprintf(full_name, sizeof(full_name), "%s%s", pfx, name);
    int pre_pages = bui_collect_status_pages((uint8_t)whose, 1u, full_name, pre_buf, sizeof(pre_buf));
    int block_pages = bui_collect_status_pages((uint8_t)whose, 0u, full_name, block_buf, sizeof(block_buf));
    battle_move_result_t fail_result = s_evt_move_result;
    if (fail_result == BATTLE_MOVE_RESULT_NONE) {
        if (missed) {
            if (eff == 0u && move_id < NUM_MOVE_DEFS && gMoves[move_id].power > 0u)
                fail_result = BATTLE_MOVE_RESULT_NO_EFFECT;
            else
                fail_result = BATTLE_MOVE_RESULT_MISS;
        }
    }
    s_evt_move_result = BATTLE_MOVE_RESULT_NONE;
    s_post_move_text[0] = '\0';
    s_apply_text[0] = '\0';
    s_apply_text_pending = 0u;

    s_drain_text[0] = '\0';
    s_hit_text_count = 0u;
    s_hit_index = 0u;
    s_hit_text_shown = 0u;
    s_pending_status_text_active = 0u;
    s_pending_status_text[0] = '\0';

    if (selected == CANNOT_MOVE) {
        return;
    }

    if (block_pages > 0) {
        uint8_t status_anim_id =
            (whose == 0) ? Battle_GetPlayerStatusAnimId() : Battle_GetEnemyStatusAnimId();
        battle_status_msg_t bmsg =
            (whose == 0) ? Battle_GetPlayerStatusMsg() : Battle_GetEnemyStatusMsg();

        if (status_anim_id != 0u && whose == 0 && bmsg == BSTAT_MSG_FAST_ASLEEP) {

            snprintf(s_pending_status_text, sizeof(s_pending_status_text), "%s", block_buf);
            s_pending_status_text_active = 1u;
            if (pre_pages > 0) {
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", pre_buf);
                bui_show_text(s_msg_buf);
            }
            return;
        }
        if (status_anim_id != 0u && bmsg == BSTAT_MSG_HURT_ITSELF) {
            snprintf(s_pending_status_text, sizeof(s_pending_status_text), "%s", block_buf);
            s_pending_status_text_active = 1u;
            if (pre_pages > 0) {

                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", pre_buf);
                bui_show_text_done(s_msg_buf);
            }

            return;
        }
        if (status_anim_id != 0u && pre_pages > 0) {

            snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", pre_buf);
            bui_show_text_done(s_msg_buf);
            snprintf(s_pending_status_text, sizeof(s_pending_status_text), "%s", block_buf);
            s_pending_status_text_active = 1u;
            return;
        }
        {

            if (pre_pages > 0)
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s\f%s", pre_buf, block_buf);
            else
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", block_buf);
            bui_show_text(s_msg_buf);
            return;
        }
    }

    if (move_id == 0) {

        if (pre_pages > 0) {
            snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", pre_buf);
            bui_show_text(s_msg_buf);
        }
        return;
    }

    const char *mn = (selected == 0 && move_id == 90 ) ? ""
                   : (move_id < 166 && gMoveNames[move_id])
                     ? gMoveNames[move_id] : "?????";

    char move_buf[256];
    uint8_t replaced = (whose == 0) ? Battle_GetPlayerReplacedMove()
                                    : Battle_GetEnemyReplacedMove();
    int pos;

    battle_announce_t ann = (whose == 0) ? Battle_GetPlayerAnnounce()
                                         : Battle_GetEnemyAnnounce();
    if (ann != BATTLE_ANNOUNCE_USED_MOVE) {
        const char *line = (ann == BATTLE_ANNOUNCE_THRASHING)        ? "%s%s's\nthrashing about!"
                         : (ann == BATTLE_ANNOUNCE_ATTACK_CONTINUES) ? "%s%s's\nattack continues!"
                                                                     : "%s%s\nunleashed energy!";
        pos = snprintf(move_buf, sizeof(move_buf), line, pfx, name);
    } else if (replaced == MOVE_MIRROR_MOVE && replaced < 166 && gMoveNames[replaced]) {

        pos = snprintf(move_buf, sizeof(move_buf), "%s%s\nused %s!\f%s%s\nused %s!",
                       pfx, name, gMoveNames[replaced], pfx, name, mn);
    } else if (replaced && replaced < 166 && gMoveNames[replaced]) {

        pos = snprintf(move_buf, sizeof(move_buf), "%s%s\nused %s!",
                       pfx, name, gMoveNames[replaced]);
        snprintf(s_pending_status_text, sizeof(s_pending_status_text),
                 "%s%s\nused %s!", pfx, name, mn);
        s_pending_status_text_active = 1u;
    } else {
        pos = snprintf(move_buf, sizeof(move_buf), "%s%s\nused %s!", pfx, name, mn);
    }
    int post_pos = 0;
    if (move_id < NUM_MOVE_DEFS) {
        uint8_t move_eff = gMoves[move_id].effect;
        uint8_t attacker_pre_b1 = s_pre.attacker_bstat1;
        uint8_t attacker_cur_b1 = (whose == 0) ? wPlayerBattleStatus1 : wEnemyBattleStatus1;
        uint8_t charge_started =
            ((attacker_pre_b1 & (1u << BSTAT1_CHARGING_UP)) == 0u) &&
            ((attacker_cur_b1 & (1u << BSTAT1_CHARGING_UP)) != 0u);
        if ((move_eff == EFFECT_CHARGE || move_eff == EFFECT_FLY) &&
            charge_started) {

            const char *act;
            switch (move_id) {
                case MOVE_RAZOR_WIND: act = "made a whirlwind!"; break;
                case MOVE_SOLARBEAM:  act = "took in sunlight!"; break;
                case MOVE_SKULL_BASH: act = "lowered its head!"; break;
                case MOVE_SKY_ATTACK: act = "is glowing!";       break;
                case MOVE_FLY:        act = "flew up high!";     break;
                default:              act = "dug a hole!";       break;
            }
            snprintf(s_pending_status_text, sizeof(s_pending_status_text),
                     "%s%s\n%s", pfx, name, act);
            s_pending_status_text_active = 1u;

            if (pre_pages > 0) {
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", pre_buf);
                bui_show_text(s_msg_buf);
            } else if (replaced && replaced < 166 && gMoveNames[replaced]) {
                snprintf(s_msg_buf, sizeof(s_msg_buf),
                         "%s%s\nused %s!", pfx, name, gMoveNames[replaced]);
                bui_show_text(s_msg_buf);
            }
            return;
        }
    }
    if (fail_result == BATTLE_MOVE_RESULT_MISS) {
        bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                             "%s%s's\nattack missed!", pfx, name);
    } else if (fail_result == BATTLE_MOVE_RESULT_NO_EFFECT) {
        const char *tpfx = (whose == 0)
                         ? ("Enemy ")
                         : "";
        const char *tname = (whose == 0)
                          ? bui_enemy_mon_name()
                          : bui_player_mon_name();
        bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                             "It doesn't affect\n%s%s!", tpfx, tname);
    } else if (fail_result == BATTLE_MOVE_RESULT_UNAFFECTED) {
        const char *tpfx = (whose == 0)
                         ? ("Enemy ")
                         : "";
        const char *tname = (whose == 0)
                          ? bui_enemy_mon_name()
                          : bui_player_mon_name();

        bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                             "%s%s's\nunaffected!", tpfx, tname);
    } else if (fail_result == BATTLE_MOVE_RESULT_EVADED) {
        const char *tpfx  = (whose == 0) ? "Enemy " : "";
        const char *tname = (whose == 0)
                          ? bui_enemy_mon_name()
                          : bui_player_mon_name();
        bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                             "%s%s\nevaded attack!", tpfx, tname);
    } else if (fail_result == BATTLE_MOVE_RESULT_BUT_IT_FAILED) {
        bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                             RomText("_ButItFailedText"));
    } else if (fail_result == BATTLE_MOVE_RESULT_NO_EFFECT_PLAIN) {
        bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                             RomText("_NoEffectText"));
    } else if (fail_result == BATTLE_MOVE_RESULT_DIDNT_AFFECT ||
               fail_result == BATTLE_MOVE_RESULT_ALREADY_ASLEEP) {

        const char *tpfx = (whose == 0)
                         ? ("Enemy ")
                         : "";
        const char *tname = (whose == 0)
                          ? bui_enemy_mon_name()
                          : bui_player_mon_name();
        if (fail_result == BATTLE_MOVE_RESULT_ALREADY_ASLEEP)
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s's\nalready asleep!", tpfx, tname);
        else
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "It didn't affect\n%s%s!", tpfx, tname);
    } else {

        int apply_pos = 0;
        for (uint8_t ei = 0; ei < s_evt_effect_msg_q_count; ei++) {
            bui_effect_msg_evt_t *sev = &s_evt_effect_msg_q[ei];
            if (sev->msg != BATTLE_EFFECT_MSG_SUBSTITUTE_TOOK_DAMAGE &&
                sev->msg != BATTLE_EFFECT_MSG_SUBSTITUTE_BROKE) continue;
            const char *spfx  = (sev->side == 1u) ? "Enemy " : "";
            const char *sname = (sev->side == 1u)
                              ? bui_enemy_mon_name()
                              : bui_player_mon_name();
            if (sev->msg == BATTLE_EFFECT_MSG_SUBSTITUTE_TOOK_DAMAGE)
                bui_append_text_page(s_apply_text, sizeof(s_apply_text), &apply_pos,
                                     "The SUBSTITUTE\ntook damage for\n%s%s!", spfx, sname);
            else

                bui_append_text_page(s_apply_text, sizeof(s_apply_text), &apply_pos,
                                     "%s%s's\nSUBSTITUTE broke!", spfx, sname);
            sev->msg = BATTLE_EFFECT_MSG_NONE;
        }

        if (s_evt_hit_sfx_q_count >= 2u) {
            for (uint8_t h = 0; h < s_evt_hit_sfx_q_count && h < BUI_HIT_TEXT_MAX; h++) {
                uint8_t hcrit = s_evt_hit_crit_q[h];
                uint8_t heff  = (uint8_t)(s_evt_hit_sfx_q[h] & 0x7Fu);
                int hp2 = 0;
                s_hit_text[h][0] = '\0';
                if (hcrit == 1)
                    bui_append_text_page(s_hit_text[h], sizeof(s_hit_text[h]), &hp2,
                                         RomText("_CriticalHitText"));
                else if (hcrit == 2)
                    bui_append_text_page(s_hit_text[h], sizeof(s_hit_text[h]), &hp2,
                                         RomText("_OHKOText"));
                if (heff >= 20)
                    bui_append_text_page(s_hit_text[h], sizeof(s_hit_text[h]), &hp2,
                                         RomText("_SuperEffectiveText"));
                else if (heff > 0 && heff <= 5)
                    bui_append_text_page(s_hit_text[h], sizeof(s_hit_text[h]), &hp2,
                                         RomText("_NotVeryEffectiveText"));
                s_hit_text_count = (uint8_t)(h + 1u);
                s_hit_anim_whose = (uint8_t)whose;
            }
        } else {
            if (crit == 1)
                bui_append_text_page(s_apply_text, sizeof(s_apply_text), &apply_pos,
                                     RomText("_CriticalHitText"));
            else if (crit == 2)
                bui_append_text_page(s_apply_text, sizeof(s_apply_text), &apply_pos,
                                     RomText("_OHKOText"));
            if (eff >= 20)
                bui_append_text_page(s_apply_text, sizeof(s_apply_text), &apply_pos,
                                     RomText("_SuperEffectiveText"));
            else if (eff > 0 && eff <= 5)
                bui_append_text_page(s_apply_text, sizeof(s_apply_text), &apply_pos,
                                     RomText("_NotVeryEffectiveText"));
        }
        s_apply_text_pending = (uint8_t)(s_apply_text[0] != '\0');
    }

    for (uint8_t ei = 0; ei < s_evt_effect_msg_q_count; ei++) {
        const bui_effect_msg_evt_t *ev = &s_evt_effect_msg_q[ei];
        const char *epfx = (ev->side == 1u) ? "Enemy " : "";
        const char *ename = (ev->side == 1u)
                          ? bui_enemy_mon_name()
                          : bui_player_mon_name();
        const char *mname = (ev->extra && ev->extra < 166 && gMoveNames[ev->extra])
                          ? gMoveNames[ev->extra] : "?????";
        switch ((battle_effect_msg_t)ev->msg) {
        case BATTLE_EFFECT_MSG_CONVERTED_TYPE:
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "Converted type to\n%s%s's!", epfx, ename); break;
        case BATTLE_EFFECT_MSG_HAZE:
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 RomText("_StatusChangesEliminatedText")); break;
        case BATTLE_EFFECT_MSG_COINS_SCATTERED:
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 RomText("_CoinsScatteredText")); break;
        case BATTLE_EFFECT_MSG_GAINED_ARMOR:
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s\ngained armor!", epfx, ename); break;
        case BATTLE_EFFECT_MSG_LIGHT_SCREEN:
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s's\nprotected against\nspecial attacks!", epfx, ename); break;
        case BATTLE_EFFECT_MSG_SUBSTITUTE_MADE:
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 RomText("_SubstituteText")); break;
        case BATTLE_EFFECT_MSG_SUBSTITUTE_TOO_WEAK:
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 RomText("_TooWeakSubstituteText")); break;
        case BATTLE_EFFECT_MSG_HIT_WITH_RECOIL:
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s's\nhit with recoil!", epfx, ename); break;
        case BATTLE_EFFECT_MSG_RAN_FROM_BATTLE:
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s\nran from battle!", epfx, ename); break;
        case BATTLE_EFFECT_MSG_RAGE_BUILDING:
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s's\nRAGE is building!", epfx, ename); break;
        case BATTLE_EFFECT_MSG_MIRROR_MOVE_FAILED:
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 RomText("_MirrorMoveFailedText")); break;
        case BATTLE_EFFECT_MSG_CRASHED:
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s\nkept going and\ncrashed!", epfx, ename); break;
        case BATTLE_EFFECT_MSG_NOTHING_HAPPENED:

            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 RomText("_NothingHappenedText")); break;
        case BATTLE_EFFECT_MSG_TRANSFORMED: {

            const char *tsp = (ev->side == 0u)
                            ? Pokemon_GetName(Species_Dex(wEnemyMon.species))
                            : Pokemon_GetName(Species_Dex(wBattleMon.species));
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s\ntransformed into\n%s!", pfx, name, tsp);
            break;
        }
        case BATTLE_EFFECT_MSG_MOVE_DISABLED:
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s's\n%s was\ndisabled!", epfx, ename, mname); break;
        case BATTLE_EFFECT_MSG_HIT_N_TIMES: {

            char *hb  = s_post_move_text;
            size_t hbsz = sizeof(s_post_move_text);
            int prepos = 0;
            int *hpp = &post_pos;
            if (s_post_hp_anim_pending && ei < s_post_hp_anim_msgs_before) {
                hb = s_pre_shake_text; hbsz = sizeof(s_pre_shake_text); hpp = &prepos;
                s_pre_shake_text_pending = 1u;
            }
            if (ev->side == 0u)
                bui_append_text_page(hb, hbsz, hpp,
                                     "Hit the enemy\n%u times!", (unsigned)ev->extra);
            else
                bui_append_text_page(hb, hbsz, hpp,
                                     "Hit %u times!", (unsigned)ev->extra);
            break;
        }
        case BATTLE_EFFECT_MSG_SUBSTITUTE_TOOK_DAMAGE:
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "The SUBSTITUTE\ntook damage for\n%s%s!", epfx, ename); break;
        case BATTLE_EFFECT_MSG_LEARNED_MOVE:
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s\nlearned\n%s!", epfx, ename, mname); break;
        case BATTLE_EFFECT_MSG_FIRE_DEFROSTED:

            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "Fire defrosted\n%s%s!", epfx, ename); break;
        case BATTLE_EFFECT_MSG_HAS_SUBSTITUTE:

            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s\nhas a SUBSTITUTE!", epfx, ename); break;
        case BATTLE_EFFECT_MSG_REGAINED_HEALTH:

            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s\nregained health!", epfx, ename); break;
        case BATTLE_EFFECT_MSG_STARTED_SLEEPING:

            bui_append_text_page(move_buf, sizeof(move_buf), &pos,
                                 "%s%s\nstarted sleeping!", epfx, ename); break;
        case BATTLE_EFFECT_MSG_FELL_ASLEEP_HEALTHY:

            bui_append_text_page(move_buf, sizeof(move_buf), &pos,
                                 "%s%s\nfell asleep and\nbecame healthy!", epfx, ename); break;
        default: break;
        }
    }
    s_evt_effect_msg_q_count = 0u;

    if (fail_result == BATTLE_MOVE_RESULT_NONE && move_id > 0 && move_id < 166 &&
        gMoves[move_id].effect == EFFECT_LEECH_SEED) {
        const char *tname, *tpfx;
        if (whose == 0) {
            tname = bui_enemy_mon_name();
            tpfx  = "Enemy ";
        } else {
            tname = bui_player_mon_name();
            tpfx  = "";
        }
        bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                             "%s%s\nwas seeded!", tpfx, tname);
    }

    s_drain_text[0] = '\0';
    if (fail_result == BATTLE_MOVE_RESULT_NONE && move_id > 0 && move_id < 166) {
        uint8_t eff_id = gMoves[move_id].effect;
        if (eff_id == EFFECT_DRAIN_HP || eff_id == EFFECT_DREAM_EATER) {
            const char *tname, *tpfx;
            if (whose == 0) {
                tname = bui_enemy_mon_name();
                tpfx  = "Enemy ";
            } else {
                tname = bui_player_mon_name();
                tpfx  = "";
            }
            if (eff_id == EFFECT_DREAM_EATER)
                snprintf(s_drain_text, sizeof(s_drain_text),
                         "%s%s's\ndream was eaten!", tpfx, tname);
            else

                snprintf(s_drain_text, sizeof(s_drain_text),
                         "Sucked health from\n%s%s!", tpfx, tname);
        }
    }

    if (fail_result == BATTLE_MOVE_RESULT_NONE) {
        static const char *kStatNames[6] = {
            "ATTACK", "DEFENSE", "SPEED", "SPECIAL", "ACCURACY", "EVADE"
        };

        const char *tpfx2, *tname2;
        uint8_t cur_tgt_status, cur_tgt_bstat1;
        if (whose == 0) {
            tpfx2  = "Enemy ";
            tname2 = bui_enemy_mon_name();
            cur_tgt_status = wEnemyMon.status;
            cur_tgt_bstat1 = wEnemyBattleStatus1;
        } else {
            tpfx2  = "";
            tname2 = bui_player_mon_name();
            cur_tgt_status = wBattleMon.status;
            cur_tgt_bstat1 = wPlayerBattleStatus1;
        }

        uint8_t sdiff = cur_tgt_status ^ s_pre.target_status;
        if ((sdiff & STATUS_SLP_MASK) && IS_ASLEEP(cur_tgt_status))
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s\nfell asleep!", tpfx2, tname2);
        else if ((sdiff & STATUS_PSN) && IS_POISONED(cur_tgt_status)) {

            uint8_t tgt_bs3 = (whose == 0) ? wEnemyBattleStatus3
                                           : wPlayerBattleStatus3;
            if (tgt_bs3 & (1u << BSTAT3_BADLY_POISONED))
                bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                     "%s%s's\nbadly poisoned!", tpfx2, tname2);
            else
                bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                     "%s%s\nwas poisoned!", tpfx2, tname2);
        }
        else if ((sdiff & STATUS_BRN) && IS_BURNED(cur_tgt_status))
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s\nwas burned!", tpfx2, tname2);
        else if ((sdiff & STATUS_FRZ) && IS_FROZEN(cur_tgt_status))
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s\nwas frozen solid!", tpfx2, tname2);
        else if ((sdiff & STATUS_PAR) && IS_PARALYZED(cur_tgt_status))

            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s's\nparalyzed! It may\nnot attack!", tpfx2, tname2);

        {
            uint8_t cur_att_b2 = (whose == 0) ? wPlayerBattleStatus2 : wEnemyBattleStatus2;
            uint8_t adiff = (uint8_t)(cur_att_b2 ^ s_pre.attacker_bstat2);
            const char *upfx = (whose == 0) ? "" : "Enemy ";
            if ((adiff & (1u << BSTAT2_PROTECTED_BY_MIST)) &&
                (cur_att_b2 & (1u << BSTAT2_PROTECTED_BY_MIST)))
                bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                     "%s%s's\nshrouded in mist!", upfx, name);
            if ((adiff & (1u << BSTAT2_GETTING_PUMPED)) &&
                (cur_att_b2 & (1u << BSTAT2_GETTING_PUMPED)))
                bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                     "%s%s's\ngetting pumped!", upfx, name);
        }

        uint8_t bdiff = cur_tgt_bstat1 ^ s_pre.target_bstat1;
        if ((bdiff & (1u << BSTAT1_CONFUSED)) && (cur_tgt_bstat1 & (1u << BSTAT1_CONFUSED)))
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 "%s%s\nbecame confused!", tpfx2, tname2);

        uint8_t move_eff = (move_id < NUM_MOVE_DEFS)
                          ? gMoves[move_id].effect
                          : ((whose == 0) ? wPlayerMoveEffect : wEnemyMoveEffect);
        uint8_t stat_idx = 0xFFu;

        for (uint8_t ei = 0; ei < s_evt_stat_text_q_count; ei++) {
            const bui_stat_text_evt_t *ev = &s_evt_stat_text_q[ei];
            const char *spfx = (ev->side == 1u) ? ("Enemy ") : "";
            const char *sname = (ev->side == 1u)
                              ? bui_enemy_mon_name()
                              : bui_player_mon_name();
            uint8_t down = (uint8_t)(ev->flags & 1u);
            uint8_t greatly = (uint8_t)(ev->flags & 2u);
            bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                 down
                                     ? (greatly ? "%s%s's\n%s\ngreatly fell!"
                                                : "%s%s's\n%s fell!")
                                     : (greatly ? "%s%s's\n%s\ngreatly rose!"
                                                : "%s%s's\n%s rose!"),
                                 spfx, sname, kStatNames[ev->stat_idx]);
        }

        if (s_evt_stat_text_q_count == 0u) {
            if ((move_eff >= EFFECT_ATTACK_DOWN1 && move_eff <= EFFECT_EVASION_DOWN1) ||
                (move_eff >= EFFECT_ATTACK_DOWN2 && move_eff <= EFFECT_EVASION_DOWN2)) {
                stat_idx = (move_eff <= EFFECT_EVASION_DOWN1)
                         ? (uint8_t)(move_eff - EFFECT_ATTACK_DOWN1)
                         : (uint8_t)(move_eff - EFFECT_ATTACK_DOWN2);
            } else if ((move_eff >= EFFECT_ATTACK_UP1 && move_eff <= EFFECT_EVASION_UP1) ||
                       (move_eff >= EFFECT_ATTACK_UP2 && move_eff <= EFFECT_EVASION_UP2)) {
                stat_idx = (move_eff <= EFFECT_EVASION_UP1)
                         ? (uint8_t)(move_eff - EFFECT_ATTACK_UP1)
                         : (uint8_t)(move_eff - EFFECT_ATTACK_UP2);
            }
            if (stat_idx < 6u)
                bui_append_text_page(s_post_move_text, sizeof(s_post_move_text), &post_pos,
                                     RomText("_NothingHappenedText"));
        }
        s_evt_stat_text_q_count = 0u;
    }
    (void)pos;

    if (s_suppress_move_text) {

        s_suppress_move_text = 0u;
        return;
    }
    if (pre_pages > 0) {

        uint8_t tail_status_anim =
            (whose == 0) ? Battle_GetPlayerStatusAnimId() : Battle_GetEnemyStatusAnimId();
        if (tail_status_anim != 0u && !s_pending_status_text_active) {
            snprintf(s_pending_status_text, sizeof(s_pending_status_text), "%s", move_buf);
            s_pending_status_text_active = 1u;
            snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", pre_buf);
            bui_show_text(s_msg_buf);
            return;
        }
        snprintf(s_msg_buf, sizeof(s_msg_buf), "%s\f%s", pre_buf, move_buf);
    } else {
        snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", move_buf);
    }
    bui_show_text(s_msg_buf);
}

static void bui_snapshot_pre(int whose) {
    if (whose == 0) {
        s_pre.target_status = wEnemyMon.status;
        s_pre.target_bstat1 = wEnemyBattleStatus1;
        s_pre.attacker_bstat1 = wPlayerBattleStatus1;
        s_pre.attacker_bstat2 = wPlayerBattleStatus2;
        memcpy(s_pre.target_stat_mods,   wEnemyMonStatMods,  6);
        memcpy(s_pre.attacker_stat_mods, wPlayerMonStatMods, 6);
    } else {
        s_pre.target_status = wBattleMon.status;
        s_pre.target_bstat1 = wPlayerBattleStatus1;
        s_pre.attacker_bstat1 = wEnemyBattleStatus1;
        s_pre.attacker_bstat2 = wEnemyBattleStatus2;
        memcpy(s_pre.target_stat_mods,   wPlayerMonStatMods, 6);
        memcpy(s_pre.attacker_stat_mods, wEnemyMonStatMods,  6);
    }
}

static void bui_snapshot_hp_pre(void) {
    s_hp_pre_player_hp = wBattleMon.hp;
    s_hp_pre_player_max = wBattleMon.max_hp;
    s_hp_pre_enemy_hp = wEnemyMon.hp;
    s_hp_pre_enemy_max = wEnemyMon.max_hp;
}

static void bui_evt_reset_hit_queues(void) {
    s_evt_move_result = BATTLE_MOVE_RESULT_NONE;
    s_evt_effect_msg_q_count = 0u;
    memset(s_evt_effect_msg_q, 0, sizeof(s_evt_effect_msg_q));
    s_evt_hit_sfx_q_count = 0u;
    s_evt_hit_sfx_q_index = 0u;
    s_evt_hp_target_q_count = 0u;
    s_evt_hp_target_q_index = 0u;
    s_evt_stat_text_q_count = 0u;
    memset(s_evt_hit_sfx_q, 0, sizeof(s_evt_hit_sfx_q));
    memset(s_evt_hit_crit_q, 0, sizeof(s_evt_hit_crit_q));
    memset(s_evt_hp_target_q, 0, sizeof(s_evt_hp_target_q));
    memset(s_evt_stat_text_q, 0, sizeof(s_evt_stat_text_q));
}

static void bui_evt_reset_status_queue(void) {
    s_evt_status_q_count[0] = 0u;
    s_evt_status_q_count[1] = 0u;
    memset(s_evt_status_q, 0, sizeof(s_evt_status_q));
}

static void bui_evt_push_status_msg(uint8_t side, uint8_t is_pre, battle_status_msg_t msg) {
    if (side > 1u || msg == BSTAT_MSG_NONE) return;
    if (s_evt_status_q_count[side] >= BUI_STATUS_EVT_MAX) return;
    s_evt_status_q[side][s_evt_status_q_count[side]].is_pre = is_pre ? 1u : 0u;
    s_evt_status_q[side][s_evt_status_q_count[side]].msg = msg;
    s_evt_status_q_count[side]++;
}

static void bui_evt_push_hit_sfx(uint8_t mult, uint8_t crit) {
    if (mult == 0u) return;
    if (s_evt_hit_sfx_q_count >= BUI_HIT_EVT_Q_MAX) return;
    s_evt_hit_crit_q[s_evt_hit_sfx_q_count] = crit;
    s_evt_hit_sfx_q[s_evt_hit_sfx_q_count++] = (uint8_t)(mult & 0x7Fu);
}

static uint8_t bui_evt_pop_hit_sfx_or(uint8_t fallback) {
    if (s_evt_hit_sfx_q_index < s_evt_hit_sfx_q_count)
        return s_evt_hit_sfx_q[s_evt_hit_sfx_q_index++];
    return fallback;
}

static void bui_evt_push_hp_target(uint8_t side, uint16_t hp_after) {
    if (side > 1u) return;
    if (s_evt_hp_target_q_count >= BUI_HIT_EVT_Q_MAX) return;
    s_evt_hp_target_hp_q[s_evt_hp_target_q_count] = hp_after;
    s_evt_hp_target_q[s_evt_hp_target_q_count++] = side;
}

static void bui_evt_push_stat_text(uint8_t stat_idx, uint8_t side, uint8_t flags) {
    if (stat_idx >= 6u || side > 1u) return;
    if (s_evt_stat_text_q_count >= BUI_STAT_TEXT_EVT_MAX) return;
    s_evt_stat_text_q[s_evt_stat_text_q_count].stat_idx = stat_idx;
    s_evt_stat_text_q[s_evt_stat_text_q_count].side = side;
    s_evt_stat_text_q[s_evt_stat_text_q_count].flags = flags;
    s_evt_stat_text_q_count++;
}

static uint8_t bui_evt_pop_hp_target_or(uint8_t fallback) {
    if (s_evt_hp_target_q_index < s_evt_hp_target_q_count) {
        s_evt_hp_target_hp_pending = s_evt_hp_target_hp_q[s_evt_hp_target_q_index];
        return s_evt_hp_target_q[s_evt_hp_target_q_index++];
    }
    s_evt_hp_target_hp_pending = 0xFFFFu;
    return fallback;
}

static int bui_evt_has_hp_target(void) {
    return s_evt_hp_target_q_index < s_evt_hp_target_q_count;
}

static void bui_evt_drop_hp_targets(uint8_t n) {
    while (n-- > 0u && bui_evt_has_hp_target())
        s_evt_hp_target_q_index++;
}

static void bui_start_move_anim_from_queue(void) {
    printf("[DRAINDBG] ANIM start idx=%u/%u id=%u\n",
           (unsigned)s_move_anim_queue_index, (unsigned)s_move_anim_queue_count,
           (unsigned)s_move_anim_queue_ids[s_move_anim_queue_index]);
    fflush(stdout);
    uint8_t forced_turn = s_move_anim_queue_forced_turn[s_move_anim_queue_index];
    memset(&s_move_anim_ctx, 0, sizeof(s_move_anim_ctx));
    s_move_anim_ctx.animation_id = s_move_anim_queue_ids[s_move_anim_queue_index];
    hWhoseTurn = (forced_turn == 0xFFu) ? s_move_anim_owner_turn : forced_turn;
    MoveAnim_Begin(&s_move_anim_ctx);
    s_move_anim_active = !MoveAnim_IsDone(&s_move_anim_ctx);
}

static uint8_t s_residual_anim_saved_turn;

static void bui_start_residual_anim(uint8_t anim_id, uint8_t forced_turn) {
    memset(&s_move_anim_ctx, 0, sizeof(s_move_anim_ctx));
    s_move_anim_ctx.animation_id = anim_id;
    s_residual_anim_saved_turn = hWhoseTurn;
    hWhoseTurn = forced_turn;
    MoveAnim_Begin(&s_move_anim_ctx);
    s_move_anim_active = !MoveAnim_IsDone(&s_move_anim_ctx);
    if (!s_move_anim_active)
        hWhoseTurn = s_residual_anim_saved_turn;
}

static void bui_restart_move_anim_replay(int whose) {
    uint8_t move_id = (whose == 0) ? wPlayerMoveNum : wEnemyMoveNum;
    s_move_anim_active = 0;
    s_move_anim_owner_turn = (uint8_t)whose;
    s_move_anim_queue_count = 0u;
    s_move_anim_queue_index = 0u;
    memset(&s_move_anim_ctx, 0, sizeof(s_move_anim_ctx));
    s_move_anim_ctx.animation_id = move_id;
    hWhoseTurn = s_move_anim_owner_turn;
    MoveAnim_Begin(&s_move_anim_ctx);
    s_move_anim_active = !MoveAnim_IsDone(&s_move_anim_ctx);
}

static int bui_move_anim_queue_contains(uint8_t anim_id) {
    for (uint8_t qi = 0u; qi < s_move_anim_queue_count; qi++)
        if (s_move_anim_queue_ids[qi] == anim_id) return 1;
    return 0;
}

static void bui_run_move_anim_runtime(int whose) {

    uint8_t move_id = Battle_SideExecutedMove(whose)
                    ? ((whose == 0) ? wPlayerMoveNum : wEnemyMoveNum)
                    : 0u;
    battle_status_msg_t smsg =
        (whose == 0) ? Battle_GetPlayerStatusMsg() : Battle_GetEnemyStatusMsg();
    uint8_t status_anim_id =
        (whose == 0) ? Battle_GetPlayerStatusAnimId() : Battle_GetEnemyStatusAnimId();
    uint8_t confusion_selfhit_pending =
        (whose == 0) ? Battle_GetPlayerConfusionSelfHitAnimPending()
                     : Battle_GetEnemyConfusionSelfHitAnimPending();
    uint8_t status_affected_pending =
        (whose == 0) ? Battle_GetPlayerStatusAffectedAnimPending()
                     : Battle_GetEnemyStatusAffectedAnimPending();
    uint8_t move_effect = (move_id < NUM_MOVE_DEFS) ? gMoves[move_id].effect : 0;
    uint8_t status1 = (whose == 0) ? wPlayerBattleStatus1 : wEnemyBattleStatus1;
    uint8_t status1_pre = s_pre.attacker_bstat1;
    uint8_t is_charge_move = (uint8_t)(move_effect == EFFECT_CHARGE || move_effect == EFFECT_FLY);
    uint8_t charge_started =
        ((status1_pre & (1u << BSTAT1_CHARGING_UP)) == 0u) &&
        ((status1 & (1u << BSTAT1_CHARGING_UP)) != 0u);
    uint8_t charge_resolving =
        ((status1_pre & (1u << BSTAT1_CHARGING_UP)) != 0u) &&
        ((status1 & (1u << BSTAT1_CHARGING_UP)) == 0u);
    battle_event_t bev;

    s_move_anim_active = 0;
    s_move_anim_owner_turn = (uint8_t)whose;
    s_move_anim_queue_count = 0u;
    s_move_anim_queue_index = 0u;
    s_post_hp_anim_pending = 0u;
    s_post_hp_anim_running = 0u;
    s_post_hp_anim_msgs_before = 0u;
    s_pre_shake_text[0] = '\0';
    s_pre_shake_text_pending = 0u;
    bui_evt_reset_status_queue();
    bui_evt_reset_hit_queues();
    memset(&s_move_anim_ctx, 0, sizeof(s_move_anim_ctx));

    s_enemy_hp_bar_draw = -1;

    uint8_t hp_target_seen = 0u;
    while (BattleEvent_Pop(&bev)) {
        if (bev.type == BATTLE_EVENT_PLAY_ANIM) {
            if (hp_target_seen) {

                s_post_hp_anim_id = bev.arg0;
                s_post_hp_anim_turn = (bev.arg1 <= 1u) ? bev.arg1 : (uint8_t)hWhoseTurn;
                s_post_hp_anim_pending = 1u;

                s_post_hp_anim_msgs_before = s_evt_effect_msg_q_count;
                continue;
            }
            if (s_move_anim_queue_count >= sizeof(s_move_anim_queue_ids)) break;
            s_move_anim_queue_ids[s_move_anim_queue_count] = bev.arg0;
            s_move_anim_queue_forced_turn[s_move_anim_queue_count] = bev.arg1;
            s_move_anim_queue_count++;
        } else if (bev.type == BATTLE_EVENT_STATUS_MSG && bev.arg2 < 2u) {
            bui_evt_push_status_msg(bev.arg2, bev.arg1, (battle_status_msg_t)bev.arg0);
        } else if (bev.type == BATTLE_EVENT_HIT_SFX) {
            bui_evt_push_hit_sfx((uint8_t)(bev.arg0 & 0x7Fu), bev.arg1);
        } else if (bev.type == BATTLE_EVENT_HP_TARGET && bev.arg0 < 2u) {
            hp_target_seen = 1u;
            bui_evt_push_hp_target(bev.arg0,
                                   (bev.arg2 & 0x80u)
                                       ? (uint16_t)(bev.arg1 |
                                           ((uint16_t)(bev.arg2 & 0x7Fu) << 8))
                                       : 0xFFFFu);
        } else if (bev.type == BATTLE_EVENT_MOVE_RESULT) {
            if (bev.arg0 <= BATTLE_MOVE_RESULT_MAX)
                s_evt_move_result = (battle_move_result_t)bev.arg0;
        } else if (bev.type == BATTLE_EVENT_STAT_MOD_TEXT) {
            bui_evt_push_stat_text(bev.arg0, bev.arg1, bev.arg2);
        } else if (bev.type == BATTLE_EVENT_EFFECT_MSG) {
            if (s_evt_effect_msg_q_count < BUI_EFFECT_MSG_EVT_MAX) {
                bui_effect_msg_evt_t *ev = &s_evt_effect_msg_q[s_evt_effect_msg_q_count++];
                ev->msg = bev.arg0; ev->side = bev.arg1; ev->extra = bev.arg2;
            }
        }
    }

    for (uint8_t i = s_evt_hp_target_q_index; i < s_evt_hp_target_q_count; i++) {
        uint8_t  side  = s_evt_hp_target_q[i];
        uint16_t after = s_evt_hp_target_hp_q[i];
        uint16_t pre   = (side == 0u) ? s_hp_pre_enemy_hp : s_hp_pre_player_hp;
        if (side > 1u)        continue;
        if (after == 0xFFFFu) continue;
        if (after <= pre)     continue;
        if (side == 0u) s_enemy_hp_bar_draw  = (int)pre;
        else            s_player_hp_bar_draw = (int)pre;
        printf("[DRAINDBG] ARM side=%u pre=%u after=%u\n",
               (unsigned)side, (unsigned)pre, (unsigned)after);
        fflush(stdout);
    }

    if ((BATTLE_RESULT_SUPPRESSES_ANIM(s_evt_move_result) || wMoveMissed) &&
        move_effect != EFFECT_EXPLODE) {
        uint8_t w = 0u;
        for (uint8_t qi = 0u; qi < s_move_anim_queue_count; qi++) {
            if (s_move_anim_queue_ids[qi] == move_id) continue;
            s_move_anim_queue_ids[w] = s_move_anim_queue_ids[qi];
            s_move_anim_queue_forced_turn[w] = s_move_anim_queue_forced_turn[qi];
            w++;
        }
        s_move_anim_queue_count = w;
    }

    uint8_t acted_through_status =
        (uint8_t)(status_anim_id != 0u &&
                  smsg == BSTAT_MSG_NONE && !confusion_selfhit_pending);
    if (s_move_anim_queue_count > 0u && !acted_through_status) {
        bui_start_move_anim_from_queue();
        return;
    }

    {
        uint8_t selected_mv = (whose == 0) ? wPlayerSelectedMove : wEnemySelectedMove;
        if (selected_mv == CANNOT_MOVE) return;
    }

    if (move_id == 0 && status_anim_id == 0u &&
        !status_affected_pending && !confusion_selfhit_pending) return;
    if (smsg != BSTAT_MSG_NONE && status_anim_id == 0u &&
        !status_affected_pending && !confusion_selfhit_pending) return;

    uint8_t charge_vanishes = (uint8_t)(move_effect == EFFECT_FLY ||
                                        move_id == MOVE_DIG);
    if (is_charge_move && charge_started) {
        if (whose == 0) {
            s_player_charge_hidden = charge_vanishes;
            s_player_charge_resolving_anim = 0;
        } else {
            s_enemy_charge_hidden = charge_vanishes;
            s_enemy_charge_resolving_anim = 0;
        }
    } else if (is_charge_move && charge_resolving && charge_vanishes) {

        if (whose == 0) {
            s_player_charge_resolving_anim = 1;
            s_player_charge_hidden = 0;
            bui_hide_player_sprite();
        } else {
            s_enemy_charge_resolving_anim = 1;
            s_enemy_charge_hidden = 0;
            bui_set_enemy_oam_visible(0);
        }
    }

    if (is_charge_move) {
        printf("[DIGDBG] run_anim whose=%d move=0x%02X eff=0x%02X pre_b1=0x%02X cur_b1=0x%02X started=%u resolving=%u p_latch=%d e_latch=%d p_resolve=%d e_resolve=%d\n",
               whose, move_id, move_effect, status1_pre, status1,
               charge_started, charge_resolving,
               s_player_charge_hidden, s_enemy_charge_hidden,
               s_player_charge_resolving_anim, s_enemy_charge_resolving_anim);
    }

    if (status_anim_id != 0u && !bui_move_anim_queue_contains(status_anim_id)) {
        s_move_anim_queue_ids[s_move_anim_queue_count] = status_anim_id;
        s_move_anim_queue_forced_turn[s_move_anim_queue_count] = 0xFFu;
        s_move_anim_queue_count++;
    }
    if (confusion_selfhit_pending &&
        !bui_move_anim_queue_contains(MOVE_ANIM_POUND_ID)) {
        s_move_anim_queue_ids[s_move_anim_queue_count] = MOVE_ANIM_POUND_ID;
        s_move_anim_queue_forced_turn[s_move_anim_queue_count] = (whose == 0) ? 1u : 0u;
        s_move_anim_queue_count++;
    }
    if (status_affected_pending) {
        s_move_anim_queue_ids[s_move_anim_queue_count] = MOVE_ANIM_STATUS_AFFECTED_ID;
        s_move_anim_queue_forced_turn[s_move_anim_queue_count] = 0xFFu;
        s_move_anim_queue_count++;
    }
    if (s_move_anim_queue_count > 0u && !acted_through_status) {
        bui_start_move_anim_from_queue();
        return;
    }

    uint8_t effect_msg_failed = 0u;
    for (uint8_t ei = 0; ei < s_evt_effect_msg_q_count; ei++) {
        uint8_t m = s_evt_effect_msg_q[ei].msg;
        if (m == BATTLE_EFFECT_MSG_MIRROR_MOVE_FAILED ||
            m == BATTLE_EFFECT_MSG_SUBSTITUTE_TOO_WEAK ||
            m == BATTLE_EFFECT_MSG_HAS_SUBSTITUTE)
            effect_msg_failed = 1u;
    }
    if ((BATTLE_RESULT_SUPPRESSES_ANIM(s_evt_move_result) || effect_msg_failed) &&
        move_effect != EFFECT_EXPLODE) {
        if (!is_charge_move) {

            if (s_move_anim_queue_count > 0u)
                bui_start_move_anim_from_queue();
            return;
        }
        if (s_move_anim_queue_count > 0u &&
            s_move_anim_queue_count < sizeof(s_move_anim_queue_ids)) {
            s_move_anim_queue_ids[s_move_anim_queue_count] = MOVE_ANIM_STATUS_AFFECTED_ID;
            s_move_anim_queue_forced_turn[s_move_anim_queue_count] = 0xFFu;
            s_move_anim_queue_count++;
            bui_start_move_anim_from_queue();
            return;
        }
        s_move_anim_ctx.animation_id = MOVE_ANIM_STATUS_AFFECTED_ID;
        hWhoseTurn = s_move_anim_owner_turn;
        MoveAnim_Begin(&s_move_anim_ctx);
        s_move_anim_active = !MoveAnim_IsDone(&s_move_anim_ctx);
        return;
    }

    if ((move_effect >= EFFECT_ATTACK_UP1 && move_effect <= EFFECT_EVASION_UP1) ||
        (move_effect >= EFFECT_ATTACK_UP2 && move_effect <= EFFECT_EVASION_UP2)) {
        uint8_t stat_idx = (move_effect <= EFFECT_EVASION_UP1)
                         ? (uint8_t)(move_effect - EFFECT_ATTACK_UP1)
                         : (uint8_t)(move_effect - EFFECT_ATTACK_UP2);
        const uint8_t *cur_atk_mods = (whose == 0) ? wPlayerMonStatMods : wEnemyMonStatMods;
        if (stat_idx < 6u && cur_atk_mods[stat_idx] <= s_pre.attacker_stat_mods[stat_idx]) {
            if (s_move_anim_queue_count > 0u)
                bui_start_move_anim_from_queue();
            return;
        }
    } else if ((move_effect >= EFFECT_ATTACK_DOWN1 && move_effect <= EFFECT_EVASION_DOWN1) ||
               (move_effect >= EFFECT_ATTACK_DOWN2 && move_effect <= EFFECT_EVASION_DOWN2)) {
        uint8_t stat_idx = (move_effect <= EFFECT_EVASION_DOWN1)
                         ? (uint8_t)(move_effect - EFFECT_ATTACK_DOWN1)
                         : (uint8_t)(move_effect - EFFECT_ATTACK_DOWN2);
        const uint8_t *cur_tgt_mods = (whose == 0) ? wEnemyMonStatMods : wPlayerMonStatMods;
        if (stat_idx < 6u && cur_tgt_mods[stat_idx] >= s_pre.target_stat_mods[stat_idx]) {
            if (s_move_anim_queue_count > 0u)
                bui_start_move_anim_from_queue();
            return;
        }
    }

    uint8_t final_anim_id;
    if (is_charge_move && charge_started) {
        if (move_id == MOVE_DIG)
            final_anim_id = 192u;
        else if (move_effect == EFFECT_FLY)
            final_anim_id = MOVE_TELEPORT;
        else
            final_anim_id = (whose == 0) ? 174u : 175u;
    } else {
        final_anim_id = move_id;
    }
    if (s_move_anim_queue_count > 0u) {

        if (!bui_move_anim_queue_contains(final_anim_id) &&
            s_move_anim_queue_count < sizeof(s_move_anim_queue_ids)) {
            s_move_anim_queue_ids[s_move_anim_queue_count] = final_anim_id;
            s_move_anim_queue_forced_turn[s_move_anim_queue_count] = 0xFFu;
            s_move_anim_queue_count++;
        }
        bui_start_move_anim_from_queue();
        return;
    }
    s_move_anim_ctx.animation_id = final_anim_id;
    hWhoseTurn = s_move_anim_owner_turn;
    MoveAnim_Begin(&s_move_anim_ctx);
    s_move_anim_active = !MoveAnim_IsDone(&s_move_anim_ctx);
}

static void bui_set_enemy_oam_visible(int visible) {

    BattleUI_EnemySpriteSetVisible((uint8_t)(visible ? 1u : 0u));
}

static void bui_enemy_faint_oam(int step) {
    for (int ty = 0; ty < 7; ty++) {
        for (int tx = 0; tx < 7; tx++) {
            int idx = ENEMY_SPR_OAM_BASE + ty * 7 + tx;
            if (ty <= 6 - step) {
                wShadowOAM[idx].y = (uint8_t)(ENEMY_SPR_PX_Y + (ty + step) * 8 + OAM_Y_OFS);
            } else {
                wShadowOAM[idx].y = 0;
            }
        }
    }
}

static void bui_player_faint_bg(int step) {
    bui_clear_rect(PLAYER_SPR_COL, PLAYER_SPR_ROW,
                   PLAYER_SPR_COL + 6, PLAYER_SPR_ROW + 6);
    for (int ty = 0; ty <= 6 - step; ty++)
        for (int tx = 0; tx < 7; tx++)
            bui_set_tile(PLAYER_SPR_COL + tx, PLAYER_SPR_ROW + ty + step,
                         (uint8_t)(PLAYER_SPR_BG_BASE + ty * 7 + tx));
}

static void bui_setup_anim(int whose) {
    uint8_t move_id = (whose == 0) ? wPlayerMoveNum : wEnemyMoveNum;

    uint8_t selected = (whose == 0) ? wPlayerSelectedMove : wEnemySelectedMove;
    if (selected == CANNOT_MOVE) { s_anim_type = 0; s_anim_total = 0; return; }
    battle_status_msg_t smsg =
        (whose == 0) ? Battle_GetPlayerStatusMsg() : Battle_GetEnemyStatusMsg();
    s_anim_frame    = 0;
    s_move_anim_should_hit_sfx = 0;

    if (smsg != BSTAT_MSG_NONE) {

        s_anim_type  = 0;
        s_anim_total = 0;
        return;
    }

    if (wMoveMissed || move_id == 0) {

        s_anim_type  = 0;
        s_anim_total = 0;
        return;
    }

    uint8_t effect = (move_id < 166) ? gMoves[move_id].effect : 0;
    uint8_t power  = (move_id < 166) ? gMoves[move_id].power  : 0;
    uint8_t status1 = (whose == 0) ? wPlayerBattleStatus1 : wEnemyBattleStatus1;
    uint8_t status1_pre = s_pre.attacker_bstat1;
    uint8_t charge_started =
        ((status1_pre & (1u << BSTAT1_CHARGING_UP)) == 0u) &&
        ((status1 & (1u << BSTAT1_CHARGING_UP)) != 0u);
    if ((effect == EFFECT_CHARGE || effect == EFFECT_FLY) &&
        charge_started) {

        s_anim_type  = 0;
        s_anim_total = 0;
        return;
    }
    if (power == 0) {

        s_anim_type  = 0;
        s_anim_total = 0;
        return;
    }
    s_move_anim_should_hit_sfx = 1;

    if (whose == 0) {

        if (effect == 0) {
            s_anim_type  = 4;
            s_anim_total = 60;
        } else {
            s_anim_type  = 5;
            s_anim_total = 18;
        }
    } else {

        if (effect == 0) {
            s_anim_type  = 1;
            s_anim_total = 48;
        } else {
            s_anim_type  = 2;
            s_anim_total = 72;
        }
    }
}

static void bui_setup_hp_anim(int whose) {
    int target = (int)bui_evt_pop_hp_target_or((uint8_t)whose);
    s_hp_anim_who  = target;
    if (target == 0) {

        s_enemy_hp_bar_draw = -1;
        s_hp_pre_hp = s_hp_pre_enemy_hp;
        s_hp_pre_max = s_hp_pre_enemy_max;
        s_hp_new_px = calc_hp_pixels(wEnemyMon.hp, wEnemyMon.max_hp);
    } else {

        s_player_hp_bar_draw = -1;
        s_hp_pre_hp = s_hp_pre_player_hp;
        s_hp_pre_max = s_hp_pre_player_max;
        s_hp_new_px = calc_hp_pixels(wBattleMon.hp, wBattleMon.max_hp);
    }
    s_hp_new_hp    = (target == 0) ? (int)wEnemyMon.hp : (int)wBattleMon.hp;
    if (s_evt_hp_target_hp_pending != 0xFFFFu) {

        uint16_t snap = s_evt_hp_target_hp_pending;
        s_hp_new_hp = (int)snap;
        s_hp_new_px = calc_hp_pixels((int)snap, (int)(target == 0 ? wEnemyMon.max_hp
                                                                 : wBattleMon.max_hp));
        if (target == 0) s_hp_pre_enemy_hp  = snap;
        else             s_hp_pre_player_hp = snap;
    }
    s_hp_cur_hp    = (int)s_hp_pre_hp;
    s_hp_old_px    = calc_hp_pixels(s_hp_pre_hp, s_hp_pre_max);
    s_hp_cur_px    = s_hp_old_px;
    printf("[MH] scroll side=%d %d->%d  px %d->%d\n", s_hp_anim_who,
           s_hp_cur_hp, s_hp_new_hp, s_hp_old_px,
           calc_hp_pixels(s_hp_new_hp, (int)s_hp_pre_max));
    fflush(stdout);
    s_hp_half_frame = 0;
    s_hp_bar_pending = 0;
    s_hp_delay      = 0;

    s_hp_anim_active = 1;
    s_hp_anim_deferred = (wMoveMissed && s_hp_cur_hp != s_hp_new_hp);
    s_hp_stage2_pending = 0;
    s_hp_stage2_px = 0;
    s_hp_stage2_hp = 0;
    s_multihit_replay_armed = 0;
    s_hit_hp_count = 0u;
    s_hit_scroll_pending = 0;
    s_hp_anim_multihit = 0;
    s_hp_hold_active = 0;
}

static void bui_setup_hit_hp_scroll(uint8_t k) {

    uint16_t from = (k == 0u) ? s_hp_pre_hp : s_hit_hp[k - 1u];
    s_hp_cur_hp = (int)from;
    s_hp_new_hp = (int)s_hit_hp[k];
    printf("[MH] hit %u/%u side=%d %u->%u  px %d->%d\n", (unsigned)(k + 1u),
           (unsigned)s_hit_hp_count, s_hp_anim_who, (unsigned)from,
           (unsigned)s_hit_hp[k], calc_hp_pixels((int)from, (int)s_hp_pre_max),
           calc_hp_pixels((int)s_hit_hp[k], (int)s_hp_pre_max));
    fflush(stdout);
    s_hp_cur_px = calc_hp_pixels(s_hp_cur_hp, (int)s_hp_pre_max);
    s_hp_bar_pending = 0;
    s_hp_delay = 0;
    s_hp_anim_deferred = 0;
    s_hp_anim_active = 1;
}

int BattleUI_HpBarAnim(int side, int *px_out, int *hp_out, int *max_out) {

    if (s_hp_anim_active && s_hp_anim_who == side) {
        if (px_out)  *px_out  = s_hp_cur_px;
        if (hp_out)  *hp_out  = s_hp_cur_hp;
        if (max_out) *max_out = (int)s_hp_pre_max;
        return 1;
    }

    if (s_hp_hold_active && s_hp_anim_who == side) {
        if (px_out)  *px_out  = s_hp_cur_px;
        if (hp_out)  *hp_out  = s_hp_cur_hp;
        if (max_out) *max_out = (int)s_hp_pre_max;
        return 1;
    }

    if (side == 0 && s_enemy_hp_bar_draw >= 0) {
        if (px_out)  *px_out  = calc_hp_pixels(s_enemy_hp_bar_draw,
                                               (int)wEnemyMon.max_hp);
        if (hp_out)  *hp_out  = s_enemy_hp_bar_draw;
        if (max_out) *max_out = (int)wEnemyMon.max_hp;
        return 1;
    }

    if (side == 1 && s_player_hp_bar_draw >= 0) {
        if (px_out)  *px_out  = calc_hp_pixels(s_player_hp_bar_draw,
                                               (int)wBattleMon.max_hp);
        if (hp_out)  *hp_out  = s_player_hp_bar_draw;
        if (max_out) *max_out = (int)wBattleMon.max_hp;
        return 1;
    }
    return 0;
}

static void bui_put_hp_number(int hp, int max_hp) {
    char b[4];
    snprintf(b, sizeof(b), "%3d", hp);
    bui_put_str(11, 10, b);
    bui_set_tile(14, 10, (uint8_t)bui_char_to_tile('/'));
    snprintf(b, sizeof(b), "%3d", max_hp);
    bui_put_str(15, 10, b);
}

static void bui_draw_hp_anim_bar(void) {
    if (s_hp_anim_who == 0) bui_draw_hp_bar_px(4,  2, s_hp_cur_px);
    else                    bui_draw_hp_bar_px(12, 9, s_hp_cur_px);
}

static int bui_hp_anim_step(void) {
    if (s_hp_delay > 0) { s_hp_delay--; return 0; }

    if (s_hp_bar_pending) {
        s_hp_bar_pending = 0;
        bui_draw_hp_anim_bar();
        s_hp_delay = 1;
        return 0;
    }

    if (s_hp_cur_hp == s_hp_new_hp) {

        int final_px = calc_hp_pixels(s_hp_cur_hp, (int)s_hp_pre_max);
        if (final_px != s_hp_cur_px) {
            s_hp_cur_px = final_px;
            bui_draw_hp_anim_bar();
        }
        s_hp_anim_active = 0;
        return 1;
    }

    int old_hp = s_hp_cur_hp;
    s_hp_cur_hp += (s_hp_cur_hp > s_hp_new_hp) ? -1 : 1;
    if (s_hp_anim_who == 1)
        bui_put_hp_number(s_hp_cur_hp, (int)s_hp_pre_max);

    int old_px = calc_hp_pixels(old_hp, (int)s_hp_pre_max);
    if (old_px != calc_hp_pixels(s_hp_cur_hp, (int)s_hp_pre_max)) {
        s_hp_cur_px = old_px;
        s_hp_bar_pending = 1;
    }
    return 0;
}

static int bui_hp_anim_step_scaled(void) {
    int steps = SpeedSettings_HpBar();
    if (steps == SPEED_UNCAPPED) {
        for (int guard = 0; guard < 8192; guard++)
            if (bui_hp_anim_step()) return 1;
        return 1;
    }
    for (int i = 0; i < steps; i++)
        if (bui_hp_anim_step()) return 1;
    return 0;
}

static void bui_configure_multihit_visuals(int attacker_whose) {
    uint8_t hit_count = 1;
    uint16_t first_hp = 0;
    uint16_t final_hp = (s_hp_anim_who == 0) ? wEnemyMon.hp : wBattleMon.hp;

    if (attacker_whose == 0) {
        hit_count = Battle_GetLastPlayerHitCount();
        first_hp = Battle_GetLastPlayerFirstTargetHP();
    } else {
        hit_count = Battle_GetLastEnemyHitCount();
        first_hp = Battle_GetLastEnemyFirstTargetHP();
    }

    {
        const uint16_t *log = NULL;
        uint8_t n = Battle_GetHitHpLog(attacker_whose, &log);
        if (n >= 2u && log) {
            s_hit_hp_count = (n > BUI_HIT_HP_MAX) ? BUI_HIT_HP_MAX : n;
            for (uint8_t k = 0; k < s_hit_hp_count; k++) s_hit_hp[k] = log[k];
            printf("[MH] plan side=%d hits=%u pre=%u/%u px=%d |", s_hp_anim_who,
                   (unsigned)s_hit_hp_count, (unsigned)s_hp_pre_hp,
                   (unsigned)s_hp_pre_max, s_hp_old_px);
            for (uint8_t k = 0; k < s_hit_hp_count; k++)
                printf(" %u(px%d)", (unsigned)s_hit_hp[k],
                       calc_hp_pixels((int)s_hit_hp[k], (int)s_hp_pre_max));
            printf("\n"); fflush(stdout);
            s_hit_scroll_pending = 1;
            s_hp_anim_multihit = 0;
            s_hp_hold_active = 1;
            bui_evt_drop_hp_targets((uint8_t)(s_hit_hp_count - 1u));
            s_hp_new_hp = (int)s_hp_pre_hp;
            s_hp_new_px = s_hp_old_px;
            s_hp_stage2_pending = 0;
            s_multihit_replay_armed = 0;
            s_multihit_replay_whose = attacker_whose;
            return;
        }
    }

    if (hit_count < 2 || first_hp == 0 || first_hp >= s_hp_pre_hp ||
        first_hp <= final_hp) {
        printf("[MH] NO PLAN side=%d englog=%u hits=%u first=%u pre=%u final=%u"
               " (bar will chain the queued targets in one drain)\n",
               s_hp_anim_who, (unsigned)Battle_GetHitHpLog(attacker_whose, NULL),
               (unsigned)hit_count, (unsigned)first_hp, (unsigned)s_hp_pre_hp,
               (unsigned)final_hp);
        fflush(stdout);
        return;
    }

    uint16_t dmg = (uint16_t)(s_hp_pre_hp - first_hp);
    s_hit_hp_count = (hit_count > BUI_HIT_HP_MAX) ? BUI_HIT_HP_MAX : hit_count;
    for (uint8_t k = 0; k < s_hit_hp_count; k++) {
        if ((uint8_t)(k + 1u) == s_hit_hp_count) { s_hit_hp[k] = final_hp; continue; }
        uint16_t d = (uint16_t)(dmg * (uint16_t)(k + 1u));
        s_hit_hp[k] = (s_hp_pre_hp > d) ? (uint16_t)(s_hp_pre_hp - d) : 0u;
    }
    printf("[MH] plan(RECONSTRUCTED) side=%d hits=%u dmg=%u pre=%u\n",
           s_hp_anim_who, (unsigned)s_hit_hp_count, (unsigned)dmg,
           (unsigned)s_hp_pre_hp); fflush(stdout);
    s_hit_scroll_pending = 1;
    s_hp_anim_multihit = 0;
    s_hp_hold_active = 1;

    bui_evt_drop_hp_targets((uint8_t)(s_hit_hp_count - 1u));

    s_hp_new_hp = (int)s_hp_pre_hp;
    s_hp_new_px = s_hp_old_px;
    s_hp_stage2_pending = 0;
    s_multihit_replay_armed = 0;
    s_multihit_replay_whose = attacker_whose;
}

static int s_hud_redraw_pending;

static void bui_finish_move_hud(void) {
    if (s_post_move_text[0]) {
        s_hud_redraw_pending = 1;
        return;
    }
    bui_draw_enemy_hud();
    bui_draw_player_hud();
}

static void bui_flush_pending_hud_redraw(void) {
    if (!s_hud_redraw_pending) return;
    s_hud_redraw_pending = 0;
    bui_draw_enemy_hud();
    bui_draw_player_hud();
}

static void bui_finish_first_move(void) {
    bui_finish_move_hud();
    bui_state = BUI_EXEC_MOVE_B;
}

static void bui_finish_second_move(void) {
    bui_finish_move_hud();
    bui_state = BUI_TURN_END;
}

static uint32_t s_turn_count;
static uint32_t s_turn_limit;

void (*gBattleTurnFenceHook)(void) = NULL;

int BattleUI_GetTurnCount(void) { return (int)s_turn_count; }
void BattleUI_SetTurnLimit(int n) { s_turn_limit = (uint32_t)(n < 0 ? 0 : n); }

static int bui_read_bar_px(int col, int row) {
    int px = 0;
    for (int i = 0; i < 6; i++) {
        uint8_t t = wTileMap[row * SCREEN_WIDTH + (col + i)];
        int seg = -1;
        for (int k = 0; k <= 8; k++)
            if (t == (uint8_t)Font_CharToTile((uint8_t)(0x63 + k))) { seg = k; break; }
        if (seg < 0) return -1;
        px += seg;
    }
    return px;
}

static char bui_read_char(int col, int row) {
    uint8_t t = wTileMap[row * SCREEN_WIDTH + col];
    static const char cand[] = "0123456789/ ";
    for (const char *c = cand; *c; c++)
        if (t == (uint8_t)bui_char_to_tile((unsigned char)*c)) return *c;
    return '?';
}

int BattleUI_SnapshotHUD(char *out, size_t outsz) {
    if (!out || outsz == 0) return 0;
    out[0] = '\0';

    int pbar = bui_read_bar_px(12, 9);
    int ebar = bui_read_bar_px(4,  2);
    if (pbar < 0 || ebar < 0) return 0;

    char num[8];
    for (int i = 0; i < 7; i++) num[i] = bui_read_char(11 + i, 10);
    num[7] = 0;

    int cur = 0, max = 0;
    if (sscanf(num, "%d/%d", &cur, &max) != 2 || max != (int)wBattleMon.max_hp)
        return 0;

    snprintf(out, outsz, "%s pbar=%d ebar=%d", num, pbar, ebar);
    return 1;
}

static uint8_t s_mimic_resume_slot;
static uint8_t s_mimic_choice_made;

static int bui_mimic_intercept(uint8_t resume_slot) {
    if (s_mimic_choice_made) return 0;
    if (wActionResultOrTookBattleTurn != 0) return 0;
    uint8_t mv = wPlayerSelectedMove;
    if (mv == 0 || mv >= NUM_MOVE_DEFS) return 0;
    if (gMoves[mv].effect != EFFECT_MIMIC) return 0;
    if (wBattleMon.status & (STATUS_FRZ | STATUS_SLP_MASK)) return 0;

    if (wPlayerBattleStatus1 & (1u << BSTAT1_FLINCHED)) return 0;
    if (wEnemyBattleStatus1 & (1u << BSTAT1_USING_TRAPPING)) return 0;
    if (wPlayerBattleStatus2 & (1u << BSTAT2_NEEDS_TO_RECHARGE)) return 0;
    if (wPlayerDisabledMoveNumber &&
        wPlayerDisabledMoveNumber == wPlayerSelectedMove) return 0;

    snprintf(s_msg_buf, sizeof(s_msg_buf), "%s\nused %s!",
             bui_player_mon_name(),
             gMoveNames[mv] ? gMoveNames[mv] : "?????");
    Text_KeepTilesOnClose();
    bui_show_text(s_msg_buf);
    s_mimic_resume_slot = resume_slot;
    bui_state = BUI_MIMIC_PROMPT;
    return 1;
}

static bui_state_t s_ai_rejoin_state = BUI_TURN_END;
static int  s_ai_action_shown = 0;
static char s_ai_withdraw_name[16];
static uint16_t s_ai_pre_hp = 0;
static bui_state_t s_ai_switch_rejoin = BUI_TURN_END;
static int  s_ai_switch_active = 0;

static int  s_slide_is_ai_switch = 0;
static const char *bui_trainer_name(void);

static const char *bui_ai_item_name(uint8_t id) {
    switch (id) {
        case 0x10: return "FULL RESTORE"; case 0x12: return "HYPER POTION";
        case 0x13: return "SUPER POTION"; case 0x14: return "POTION";
        case 0x34: return "FULL HEAL";    case 0x37: return "GUARD SPEC.";
        case 0x41: return "X ATTACK";     case 0x42: return "X DEFEND";
        case 0x43: return "X SPEED";      case 0x44: return "X SPECIAL";
        default:   return "an item";
    }
}

static void bui_begin_ai_action(bui_state_t rejoin) {
    if (gLastAIAction.kind == AI_ACT_SWITCH)

        snprintf(s_msg_buf, sizeof(s_msg_buf), "%s with-\ndrew %s!",
                 bui_trainer_name(), s_ai_withdraw_name);
    else

        snprintf(s_msg_buf, sizeof(s_msg_buf), "%s\nused %s%con %s!",
                 bui_trainer_name(), bui_ai_item_name(gLastAIAction.item_id),
                 TEXT_ASCII_CONT, bui_enemy_mon_name());

    s_enemy_hp_bar_draw = (gLastAIAction.kind == AI_ACT_ITEM_HEAL)
                              ? (int)gLastAIAction.hp_before : -1;
    s_ai_rejoin_state = rejoin;
    s_ai_action_shown = 0;
    s_ai_pre_hp = s_hp_pre_enemy_hp;
    bui_state = BUI_AI_ACTION;
}

static void bui_exec_first_move(void) {

    if (s_player_first && bui_mimic_intercept(1u)) return;
    s_turn_count++;
    if (s_turn_limit && s_turn_count > s_turn_limit) return;
    const char *wild_pfx = "Enemy ";

    if (s_player_first) {
        snprintf(s_name_a, sizeof(s_name_a), "%s",
                 bui_player_mon_name());
        s_pfx_a[0] = '\0';
        snprintf(s_name_b, sizeof(s_name_b), "%s",
                 bui_enemy_mon_name());
        snprintf(s_pfx_b, sizeof(s_pfx_b), "%s", wild_pfx);

        hWhoseTurn = 0;

        int player_half_silent = (wActionResultOrTookBattleTurn != 0);
        bui_snapshot_pre(0);
        bui_snapshot_hp_pre();
        Battle_ExecutePlayerMove();
        bui_note_target_hud_redraw_if_status_changed(0);
        bui_run_move_anim_runtime(0);
        bui_setup_anim(0);
        bui_setup_hp_anim(0);
        bui_configure_multihit_visuals(0);
        if (player_half_silent) {

            s_anim_type = 0; s_anim_total = 0;
            s_anim_first = 1;
            bui_state = BUI_MOVE_ANIM;
            s_move_anim_hit_sfx_started = 0;
            return;
        }
    } else {
        snprintf(s_name_a, sizeof(s_name_a), "%s",
                 bui_enemy_mon_name());
        snprintf(s_pfx_a, sizeof(s_pfx_a), "%s", wild_pfx);
        snprintf(s_name_b, sizeof(s_name_b), "%s",
                 bui_player_mon_name());
        s_pfx_b[0] = '\0';

        hWhoseTurn = 1;
        bui_snapshot_pre(1);
        bui_snapshot_hp_pre();

        snprintf(s_ai_withdraw_name, sizeof(s_ai_withdraw_name), "%s", bui_enemy_mon_name());
        if (AI_TrainerAI()) { bui_begin_ai_action(BUI_EXEC_MOVE_B); return; }
        Battle_ExecuteEnemyMove();
        bui_note_target_hud_redraw_if_status_changed(1);
        bui_run_move_anim_runtime(1);
        bui_setup_anim(1);
        bui_setup_hp_anim(1);
        bui_configure_multihit_visuals(1);
    }
    s_anim_first = 1;

    Text_KeepTilesOnClose();
    if (s_player_first)
        bui_show_after_move(0, s_pfx_a, s_name_a, wPlayerSelectedMove, wPlayerMoveNum,
                            Battle_GetLastCrit(), wMoveMissed, wDamageMultipliers & 0x7F);
    else
        bui_show_after_move(1, s_pfx_a, s_name_a, wEnemySelectedMove, wEnemyMoveNum,
                            Battle_GetLastCrit(), wMoveMissed, wDamageMultipliers & 0x7F);
    bui_state    = BUI_MOVE_ANIM;
    s_move_anim_hit_sfx_started = 0;
}

static void bui_begin_trainer_victory_seq(void) {

    char player_ascii[NAME_LENGTH] = "RED";
    for (int i = 0; i < NAME_LENGTH - 1; i++) {
        uint8_t c = wPlayerName[i];
        if (c == 0x50) break;
        if (c >= 0x80 && c <= 0x99) player_ascii[i] = (char)('A' + c - 0x80);
        else if (c >= 0xA0 && c <= 0xB9) player_ascii[i] = (char)('a' + c - 0xA0);
        else { player_ascii[i] = '?'; }
        player_ascii[i + 1] = '\0';
    }

    uint32_t money = (uint32_t)(
        ((wPlayerMoney[0] >> 4) & 0xF) * 100000u +
        (wPlayerMoney[0] & 0xF)        * 10000u  +
        ((wPlayerMoney[1] >> 4) & 0xF) * 1000u   +
        (wPlayerMoney[1] & 0xF)        * 100u     +
        ((wPlayerMoney[2] >> 4) & 0xF) * 10u      +
        (wPlayerMoney[2] & 0xF));
    money += wAmountMoneyWon;
    if (money > 999999u) money = 999999u;
    wPlayerMoney[0] = (uint8_t)(((money / 100000u) << 4) | ((money / 10000u) % 10u));
    wPlayerMoney[1] = (uint8_t)((((money / 1000u) % 10u) << 4) | ((money / 100u) % 10u));
    wPlayerMoney[2] = (uint8_t)((((money / 10u) % 10u) << 4) | (money % 10u));

    const char *tname = bui_trainer_name();
    bui_exp_dest = BUI_TRAINER_VICTORY_SLIDE;
    snprintf(s_exp_suffix, sizeof(s_exp_suffix),
             "%s defeated\n%s!", player_ascii, tname);
    s_exp_suffix2[0] = '\0';

    snprintf(s_trainer_money_text, sizeof(s_trainer_money_text),
             "%s got \xA5%lu\nfor winning!", player_ascii, (unsigned long)wAmountMoneyWon);
    s_slide_cx      = 48;
    s_victory_timer = 0;
}

static void bui_handle_enemy_fainted(void) {

    const char *faint_pfx  = "Enemy ";
    const char *faint_name = bui_enemy_mon_name();

    Audio_PlaySFX_Faint();

    Audio_DisableLowHealthAlarm();
    Battle_HandleEnemyMonFainted();

    s_enemy_on_field = 0;
    bui_clear_rect(0, 0, 11, 3);
    bui_draw_player_hud();

    if (wBattleResult == BATTLE_OUTCOME_WILD_VICTORY) {

        if (AmberScript_IsEnabled() && wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST)
            JohtoMusic_Stop();
        Music_Play(MUSIC_DEFEATED_WILD_MON);
    }

    snprintf(s_msg_buf, sizeof(s_msg_buf), "%s%s\nfainted!", faint_pfx, faint_name);
    bui_show_text(s_msg_buf);

    if (wBattleResult == BATTLE_OUTCOME_WILD_VICTORY) {
        bui_exp_dest    = BUI_FADE_WHITE;
        s_exp_suffix[0] = '\0';
        s_exp_suffix2[0] = '\0';
    } else if (wBattleResult == BATTLE_OUTCOME_TRAINER_VICTORY) {
        bui_begin_trainer_victory_seq();
    } else {

        s_grow_stage = 0; s_grow_frame = 0; s_mid_battle_send = 1;
        s_shift_pending = 0;

        if (!(wOptions & (1u << 6)) && wPartyCount > 1) {
            s_shift_phase   = 0;
            bui_exp_dest    = BUI_SHIFT_PROMPT;
            s_exp_suffix[0] = '\0';
        } else {

            const char *new_name = bui_enemy_mon_name();
            bui_exp_dest = BUI_ENEMY_SEND_OUT;
            snprintf(s_exp_suffix, sizeof(s_exp_suffix), "Foe sent out\n%s!", new_name);
        }
    }
    bui_state = BUI_EXP_DRAIN;
}

static void bui_start_enemy_faint_anim(void) {
    s_faint_step  = 0;
    s_faint_timer = 0;
    bui_state = BUI_ENEMY_FAINT_ANIM;
}

static char s_player_faint_name[16];

static void bui_begin_player_faint_slide(const char *name) {
    snprintf(s_player_faint_name, sizeof(s_player_faint_name), "%s", name ? name : "");

    Audio_SetLowHealthAlarm(0);
    bui_clear_rect(9, 7, 19, 11);
    s_faint_step  = 0;
    s_faint_timer = 0;
    bui_state = BUI_PLAYER_FAINT_ANIM;
}

static void bui_handle_player_fainted(const char *name) {
    Battle_HandlePlayerMonFainted();
    bui_begin_player_faint_slide(name);
}

static void bui_get_side_name_prefix(uint8_t side, const char **pfx, const char **name) {
    if (side == 0u) {
        *pfx = "";
        *name = bui_player_mon_name();
    } else {
        *pfx = "Enemy ";
        *name = bui_enemy_mon_name();
    }
}

static int bui_format_residual_event_text(char *line, size_t linesz, const battle_event_t *bev) {
    const char *pfx = "";
    const char *name = "";

    if (!line || linesz == 0 || !bev) return 0;
    if (bev->type != BATTLE_EVENT_RESIDUAL_MSG) return 0;
    if (bev->arg1 > 1u) return 0;

    bui_get_side_name_prefix(bev->arg1, &pfx, &name);
    line[0] = '\0';

    switch ((battle_residual_msg_t)bev->arg0) {
    case BATTLE_RESIDUAL_MSG_BURN:

        snprintf(line, linesz, "%s%s's\nhurt by the burn!", pfx, name);
        break;
    case BATTLE_RESIDUAL_MSG_POISON:
        snprintf(line, linesz, "%s%s's\nhurt by poison!", pfx, name);
        break;
    case BATTLE_RESIDUAL_MSG_LEECH_SEED:
        snprintf(line, linesz, "LEECH SEED saps\n%s%s!", pfx, name);
        break;
    default:
        return 0;
    }
    return 1;
}

static int bui_apply_residual_and_collect_events(void) {
    battle_event_t bev;
    int alive;
    bui_snapshot_hp_pre();
    s_residual_evt_q_count = 0u;
    s_residual_evt_q_index = 0u;
    BattleEvent_ResetTurnQueue();
    alive = Battle_HandlePoisonBurnLeechSeed();
    while (BattleEvent_Pop(&bev)) {
        if (s_residual_evt_q_count >= BUI_RESIDUAL_EVT_MAX) break;
        s_residual_evt_q[s_residual_evt_q_count++] = bev;
    }

    for (uint8_t i = 0; i < s_residual_evt_q_count; i++) {
        const battle_event_t *e = &s_residual_evt_q[i];
        if (e->type != BATTLE_EVENT_HP_TARGET || e->arg0 > 1u) continue;
        if (e->arg0 == 0u) s_enemy_hp_bar_draw  = (int)s_hp_pre_enemy_hp;
        else               s_player_hp_bar_draw = (int)s_hp_pre_player_hp;
    }
    return alive;
}

static void bui_begin_residual(int phase) {
    s_residual_phase = phase;
    s_residual_alive = bui_apply_residual_and_collect_events();
    bui_state = BUI_RESIDUAL_EVENT;
}

static void bui_note_target_hud_redraw_if_status_changed(int whose) {
    if (whose == 0) {
        if (wEnemyMon.status != s_pre.target_status) {
            printf("[HUDDBG] queue enemy redraw: pre=0x%02X(%s) cur=0x%02X(%s)\n",
                   s_pre.target_status, bui_status_name(s_pre.target_status),
                   wEnemyMon.status, bui_status_name(wEnemyMon.status));
            s_status_hud_redraw_mask |= 1u;
        } else {
            printf("[HUDDBG] skip enemy redraw: pre=0x%02X(%s) cur=0x%02X(%s)\n",
                   s_pre.target_status, bui_status_name(s_pre.target_status),
                   wEnemyMon.status, bui_status_name(wEnemyMon.status));
        }
    } else {
        if (wBattleMon.status != s_pre.target_status) {
            printf("[HUDDBG] queue player redraw: pre=0x%02X(%s) cur=0x%02X(%s)\n",
                   s_pre.target_status, bui_status_name(s_pre.target_status),
                   wBattleMon.status, bui_status_name(wBattleMon.status));
            s_status_hud_redraw_mask |= 2u;
        } else {
            printf("[HUDDBG] skip player redraw: pre=0x%02X(%s) cur=0x%02X(%s)\n",
                   s_pre.target_status, bui_status_name(s_pre.target_status),
                   wBattleMon.status, bui_status_name(wBattleMon.status));
        }
    }
}

static void bui_flush_status_hud_redraws(void) {
    if (!s_status_hud_redraw_mask) {
        printf("[HUDDBG] flush status redraws: none pending\n");
        return;
    }
    printf("[HUDDBG] flush status redraws: mask=0x%02X enemy=0x%02X(%s) player=0x%02X(%s)\n",
           s_status_hud_redraw_mask,
           wEnemyMon.status, bui_status_name(wEnemyMon.status),
           wBattleMon.status, bui_status_name(wBattleMon.status));
    if (s_status_hud_redraw_mask & 1u)
        bui_draw_enemy_hud();
    if (s_status_hud_redraw_mask & 2u)
        bui_draw_player_hud();
    s_status_hud_redraw_mask = 0u;
}

static void bui_exec_second_move(void) {
    if (!s_player_first && bui_mimic_intercept(2u)) return;
    if (s_player_first) {
        hWhoseTurn = 1;
        bui_snapshot_pre(1);
        bui_snapshot_hp_pre();

        snprintf(s_ai_withdraw_name, sizeof(s_ai_withdraw_name), "%s", bui_enemy_mon_name());
        if (AI_TrainerAI()) { bui_begin_ai_action(BUI_TURN_END); return; }
        Battle_ExecuteEnemyMove();
        bui_note_target_hud_redraw_if_status_changed(1);
        bui_run_move_anim_runtime(1);
        bui_setup_anim(1);
        bui_setup_hp_anim(1);
        bui_configure_multihit_visuals(1);
    } else {
        hWhoseTurn = 0;

        int player_half_silent = (wActionResultOrTookBattleTurn != 0);
        bui_snapshot_pre(0);
        bui_snapshot_hp_pre();
        Battle_ExecutePlayerMove();
        bui_note_target_hud_redraw_if_status_changed(0);
        bui_run_move_anim_runtime(0);
        bui_setup_anim(0);
        bui_setup_hp_anim(0);
        bui_configure_multihit_visuals(0);
        if (player_half_silent) {
            s_anim_type = 0; s_anim_total = 0;
            s_anim_first = 0;
            bui_state = BUI_MOVE_ANIM;
            s_move_anim_hit_sfx_started = 0;
            return;
        }
    }
    s_anim_first = 0;

    Text_KeepTilesOnClose();
    if (s_player_first)
        bui_show_after_move(1, s_pfx_b, s_name_b, wEnemySelectedMove, wEnemyMoveNum,
                            Battle_GetLastCrit(), wMoveMissed, wDamageMultipliers & 0x7F);
    else
        bui_show_after_move(0, s_pfx_b, s_name_b, wPlayerSelectedMove, wPlayerMoveNum,
                            Battle_GetLastCrit(), wMoveMissed, wDamageMultipliers & 0x7F);
    bui_state    = BUI_MOVE_ANIM;
    s_move_anim_hit_sfx_started = 0;
}

void BattleUI_Restore(void) {
    s_turn_count = 0;

    s_enemy_display_species = wEnemyMon.species;

    bui_state = BUI_DRAW_HUD;
    s_move_anim_active = 0;
    s_move_anim_owner_turn = 0u;
    s_move_anim_queue_count = 0u;
    s_move_anim_queue_index = 0u;
    s_status_hud_redraw_mask = 0u;
    memset(s_move_anim_queue_ids, 0, sizeof(s_move_anim_queue_ids));
    memset(s_move_anim_queue_forced_turn, 0xFF, sizeof(s_move_anim_queue_forced_turn));
    s_move_anim_hit_sfx_started = 0;
    s_hp_stage2_pending = 0;
    s_hp_stage2_px = 0;
    s_multihit_replay_armed = 0;
    s_multihit_replay_whose = 0;
    s_residual_phase = 0;
    s_residual_alive = 1;
    s_residual_evt_q_count = 0u;
    s_residual_evt_q_index = 0u;
    s_residual_delay_frames = 0;
    s_player_charge_hidden = 0;
    s_enemy_charge_hidden = 0;
    s_player_charge_resolving_anim = 0;
    s_enemy_charge_resolving_anim = 0;
    s_post_move_text[0] = '\0';
    s_drain_text[0] = '\0';
    memset(&s_move_anim_ctx, 0, sizeof(s_move_anim_ctx));
    bui_evt_reset_status_queue();
    bui_evt_reset_hit_queues();
}

void BattleUI_Enter(void) {

    bui_state       = BUI_SLIDE_IN;
    s_rival1_loss   = 0;

    memset(gScrollTileMap, BLANK_TILE_SLOT,
           (size_t)SCROLL_MAP_W * (size_t)SCROLL_MAP_H);

    Audio_ResetLowHealthAlarm();
    s_slide_cx      = 144;

    s_grow_after_switch = 0;

    s_wait_cry_delay = 0;
    s_wait_cry_text_keep = 0;
    s_intro_sfx_timer = 0;

    s_ghost_intro_phase = 0;
    s_ghost_intro_timer = 0;

    s_enemy_pic_species = 0;

    bui_ghost_set_sprite_pal(-1);
    bui_cursor      = 0;
    s_post_move_text[0] = '\0';
    s_drain_text[0] = '\0';
    s_anim_type     = 0;
    s_anim_frame    = 0;
    s_anim_total    = 0;
    s_move_anim_active = 0;
    s_move_anim_owner_turn = 0u;
    s_move_anim_queue_count = 0u;
    s_move_anim_queue_index = 0u;
    s_status_hud_redraw_mask = 0u;
    memset(s_move_anim_queue_ids, 0, sizeof(s_move_anim_queue_ids));
    memset(s_move_anim_queue_forced_turn, 0xFF, sizeof(s_move_anim_queue_forced_turn));
    s_move_anim_hit_sfx_started = 0;
    s_hp_stage2_pending = 0;
    s_hp_stage2_px = 0;
    s_multihit_replay_armed = 0;
    s_multihit_replay_whose = 0;
    s_residual_phase = 0;
    s_residual_alive = 1;
    s_residual_evt_q_count = 0u;
    s_residual_evt_q_index = 0u;
    s_residual_delay_frames = 0;
    s_player_charge_hidden = 0;
    s_enemy_charge_hidden = 0;
    s_player_charge_resolving_anim = 0;
    s_enemy_charge_resolving_anim = 0;
    s_safari_item_anim_id = 0;
    s_safari_item_anim_started = 0;
    memset(&s_move_anim_ctx, 0, sizeof(s_move_anim_ctx));
    bui_evt_reset_status_queue();
    bui_evt_reset_hit_queues();
    s_caught_species = 0;
    s_caught_dex = 0;
    s_caught_new_entry = 0;
    s_caught_sent_to_box = 0;
    s_caught_dex_started = 0;
    s_caught_party_slot = -1;
    s_caught_box_slot = -1;
    s_caught_box_index = -1;
    s_learn_slot = 0xFF;
    s_learn_move = 0;
    s_learn_cursor = 0;
    s_debug_autowin_pending = 0;
    s_is_champion_room_battle = (wIsInBattle == 2) && ((uint8_t)wCurMap == 0x78);

    s_enemy_on_field = (wIsInBattle == 1);
    Display_SetShakeOffset(0, 0);
    gScrollPxX = 0;
    gScrollPxY = 0;
    bui_clear_rows(0, SCREEN_HEIGHT - 1);

    Text_DrawEmptyBox();
    memset(wShadowOAM, 0, sizeof(wShadowOAM));
    Font_Load();
    Font_LoadHudTiles();

    Display_SetPalette(0xE4, 0xE4, 0xE4);

    uint8_t e_dex = gSpeciesToDex[wEnemyMon.species];
    uint8_t p_dex = gSpeciesToDex[wBattleMon.species];

    if (wIsInBattle == 2) {

        int jc = bui_johto_trainer_class();
        if (jc > 0) {
            for (int i = 0; i < TRAINER_CANVAS_TILES; i++)
                Display_LoadSpriteTile((uint8_t)(ENEMY_SPR_TILE_BASE + i),
                                       gCrystalTrainerPicByClass[jc][i]);
            s_enemy_pic_kind = BUI_ENEMY_PIC_TRAINER;
        } else {
            int tc = (int)gEngagedTrainerClass - 1;
            if (tc >= 0 && tc < NUM_TRAINERS) {
                for (int i = 0; i < TRAINER_CANVAS_TILES; i++)
                    Display_LoadSpriteTile((uint8_t)(ENEMY_SPR_TILE_BASE + i),
                                           gTrainerFrontSprite[tc][i]);
                s_enemy_pic_kind = BUI_ENEMY_PIC_TRAINER;
            }
        }
    } else if (bui_enemy_drawn_as_ghost()) {

        for (int i = 0; i < POKEMON_FRONT_CANVAS_TILES; i++) {
            Display_LoadSpriteTile((uint8_t)(ENEMY_SPR_TILE_BASE + i),
                                   kGhostFrontSprite[i]);
        }
        s_enemy_pic_kind = BUI_ENEMY_PIC_GHOST;
    } else if (bui_has_front_sprite(wEnemyMon.species, e_dex)) {
        bui_load_enemy_front_tiles(wEnemyMon.species, e_dex);
    }

    for (int i = 0; i < 49; i++)
        Display_LoadSpriteTile((uint8_t)(PLAYER_SLIDE_TILE_BASE + i), player_back_tile(i));

    if (bui_has_front_sprite(wEnemyMon.species, e_dex)) {
        for (int ty = 0; ty < 7; ty++) {
            for (int tx = 0; tx < 7; tx++) {
                int idx = ENEMY_SPR_OAM_BASE + ty * 7 + tx;
                int raw_x = ENEMY_SPR_PX_X + tx * 8 + OAM_X_OFS - 144;
                wShadowOAM[idx].y     = (uint8_t)(ENEMY_SPR_PX_Y + ty * 8 + OAM_Y_OFS);
                wShadowOAM[idx].x     = (uint8_t)(raw_x < 0 ? 0 : raw_x);
                wShadowOAM[idx].tile  = (uint8_t)(ENEMY_SPR_TILE_BASE + ty * 7 + tx);
                wShadowOAM[idx].flags = 0;
            }
        }
    }

    if (bui_has_back_sprite(wBattleMon.species, p_dex)) {
        for (int ty = 0; ty < 7; ty++) {
            for (int tx = 0; tx < 7; tx++) {
                int idx = PLAYER_SLIDE_OAM_BASE + ty * 7 + tx;
                wShadowOAM[idx].y     = (uint8_t)(PLAYER_SPR_ROW * 8 + ty * 8 + 16);
                wShadowOAM[idx].x     = (uint8_t)((PLAYER_SPR_COL * 8 + 144 + tx * 8 + 8) & 0xFF);
                wShadowOAM[idx].tile  = (uint8_t)(PLAYER_SLIDE_TILE_BASE + ty * 7 + tx);
                wShadowOAM[idx].flags = 0;
            }
        }
    }
}

int BattleUI_IsActive(void) {
    return bui_state != BUI_INACTIVE;
}

int BattleUI_IsEvolutionScreen(void) {
    return bui_state == BUI_EVOLUTION;
}

int BattleUI_GetState(void) {
    return (int)bui_state;
}

int BattleUI_IsAtActionMenu(void) {
    return bui_state == BUI_MENU;
}

int BattleUI_BeginPendingEvolution(void) {
    uint8_t slot, new_species;
    if (!Battle_CheckNextEvolution(&slot, &new_species)) return 0;

    s_evo_slot         = slot;
    s_evo_old_species  = wPartyMons[slot].base.species;
    s_evo_new_species  = new_species;
    s_evo_phase        = 0;
    s_evo_cancelled    = 0;
    s_evo_timer        = 0;
    s_evo_screen_white = 0;
    bui_state = BUI_EVOLUTION;
    return 1;
}

static void bui_decode_player_name(char *out) {
    int j = 0;
    for (int i = 0; i < NAME_LENGTH - 1; i++) {
        uint8_t c = wPlayerName[i];
        if (c == 0x50 || c == 0x00) break;
        if (c >= 0x80 && c <= 0x99)      out[j++] = (char)('A' + c - 0x80);
        else if (c >= 0xA0 && c <= 0xB9) out[j++] = (char)('a' + c - 0xA0);
        else                              out[j++] = '?';
    }
    if (j == 0) { out[0] = 'R'; out[1] = 'E'; out[2] = 'D'; j = 3; }
    out[j] = '\0';
}

static const char *bui_trainer_name(void) {
    if (gEngagedTrainerClass == 0x19 || gEngagedTrainerClass == 0x2A ||
        gEngagedTrainerClass == 0x2B)
        return "{RIVAL}";
    int tc = (int)gEngagedTrainerClass - 1;
    return (tc >= 0 && tc < NUM_TRAINERS) ? gTrainerClassNames[tc] : "TRAINER";
}

static void bui_draw_shift_yesno(void) {
    bui_set_tile(0, 7,  (uint8_t)Font_CharToTile(0x79));
    bui_set_tile(5, 7,  (uint8_t)Font_CharToTile(0x7B));
    bui_set_tile(0, 11, (uint8_t)Font_CharToTile(0x7D));
    bui_set_tile(5, 11, (uint8_t)Font_CharToTile(0x7E));
    for (int c = 1; c < 5; c++) {
        bui_set_tile(c, 7,  (uint8_t)Font_CharToTile(0x7A));
        bui_set_tile(c, 11, (uint8_t)Font_CharToTile(0x7A));
    }
    for (int r = 8; r <= 10; r++) {
        bui_set_tile(0, r, (uint8_t)Font_CharToTile(0x7C));
        bui_set_tile(5, r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = 1; c < 5; c++) bui_set_tile(c, r, BLANK_TILE_SLOT);
    }
    bui_put_str(2, 8,  "YES");
    bui_put_str(2, 10, "NO");
    bui_set_tile(1, 8,  (uint8_t)Font_CharToTile(bui_cursor == 0 ? 0xED : 0x7F));
    bui_set_tile(1, 10, (uint8_t)Font_CharToTile(bui_cursor == 1 ? 0xED : 0x7F));
}

static void bui_shift_to_send_out(void) {
    bui_clear_rows(0, SCREEN_HEIGHT - 1);
    bui_draw_player_hud();
    bui_load_sprites();
    bui_set_enemy_oam_visible(0);

    bui_draw_box();
    const char *emon = bui_enemy_mon_name();
    snprintf(s_msg_buf, sizeof(s_msg_buf), "%s sent\nout %s!", bui_trainer_name(), emon);

    bui_show_text_done(s_msg_buf);
    s_grow_stage = 0; s_grow_frame = 0;
    bui_state = BUI_ENEMY_SEND_OUT;
}

static void bui_tick_once(void);

static int bui_state_is_misc_anim(int st) {
    switch (st) {
    case BUI_SLIDE_IN:
    case BUI_SEND_OUT:
    case BUI_ENEMY_SLIDE_OUT:
    case BUI_TRAINER_SLIDE_OUT:
    case BUI_ENEMY_SEND_OUT:
    case BUI_POKEMON_APPEAR:
    case BUI_RETREAT_ANIM:
    case BUI_ENEMY_FAINT_ANIM:
    case BUI_PLAYER_FAINT_ANIM:
    case BUI_TRAINER_VICTORY_SLIDE:
    case BUI_TRAINER_VICTORY_PAUSE:
        return 1;
    default:
        return 0;
    }
}

void BattleUI_Tick(void) {
    bui_tick_once();

    int steps = SpeedSettings_MiscAnim();
    if (steps == SPEED_NORMAL) return;
    int extra = (steps == SPEED_UNCAPPED) ? 512 : steps - 1;
    while (extra-- > 0 && bui_state_is_misc_anim(bui_state))
        bui_tick_once();
}

static void bui_tick_once(void) {
    switch (bui_state) {

    case BUI_INACTIVE:
        return;

    case BUI_SLIDE_IN: {
        int cx = s_slide_cx;
        uint8_t e_dex2 = gSpeciesToDex[wEnemyMon.species];

        if (cx == 144)
            bui_draw_box();

        if (wIsInBattle == 2 || bui_has_front_sprite(wEnemyMon.species, e_dex2)) {
            for (int ty = 0; ty < 7; ty++)
                for (int tx = 0; tx < 7; tx++) {
                    int idx = ENEMY_SPR_OAM_BASE + ty * 7 + tx;
                    int raw_x = ENEMY_SPR_PX_X + tx * 8 + OAM_X_OFS - cx;
                    wShadowOAM[idx].x = (uint8_t)(raw_x < 0 ? 0 : raw_x);
                }
        }

        for (int ty = 0; ty < 7; ty++)
            for (int tx = 0; tx < 7; tx++) {
                int idx = PLAYER_SLIDE_OAM_BASE + ty * 7 + tx;
                wShadowOAM[idx].x = (uint8_t)((PLAYER_SPR_COL * 8 + cx + tx * 8 + OAM_X_OFS) & 0xFF);
            }

        if (cx > 0) {
            s_slide_cx -= 2;
        } else {

            if (wIsInBattle == 2) {

                Audio_PlaySFX_SilphScope();
                s_intro_sfx_timer = 0;
                bui_state = BUI_INTRO_SFX;
                break;
            } else {
                bui_draw_pokeballs();
                if (bui_should_run_marowak_ghost_intro()) {

                    bui_show_text("GHOST\nappeared!");
                    s_ghost_intro_phase = 0;
                    s_ghost_intro_timer = 0;
                    bui_state = BUI_GHOST_REVEAL;
                } else if (Battle_IsGhostBattle() && bui_map_is_tower_wild_floor()) {

                    snprintf(s_wait_cry_text, sizeof(s_wait_cry_text),
                             "GHOST\nappeared!\fDarn! The GHOST\ncan't be ID'd!");

                    s_wait_cry_next_state = (wBattleType == 2 || wBattleType == 1) ? BUI_DRAW_HUD : BUI_SEND_OUT;
                    bui_state = BUI_WAIT_CRY;
                } else {
                    Audio_PlayCry(wEnemyMon.species);
                    const char *e_name2 = bui_enemy_mon_name();
                    snprintf(s_wait_cry_text, sizeof(s_wait_cry_text), "Wild %s\nappeared!", e_name2);

                    s_wait_cry_next_state = (wBattleType == 2 || wBattleType == 1) ? BUI_DRAW_HUD : BUI_SEND_OUT;
                    bui_state = BUI_WAIT_CRY;
                }
            }
        }
        break;
    }

    case BUI_GHOST_REVEAL:
        if (s_ghost_intro_phase == 0) {
            bui_show_text("SILPH SCOPE\nunveiled the\nGHOST's identity!");
            s_ghost_intro_phase = 1;
            s_ghost_intro_timer = 0;
            for (int ty = 0; ty < 7; ty++) {
                for (int tx = 0; tx < 7; tx++) {
                    int idx = ENEMY_SPR_OAM_BASE + ty * 7 + tx;
                    wShadowOAM[idx].flags |= OAM_FLAG_PALETTE;
                }
            }
            bui_ghost_set_sprite_pal(GHOST_OBP1_NORMAL);
            break;
        }
        if (s_ghost_intro_phase == 1) {
            s_ghost_intro_timer++;
            if ((s_ghost_intro_timer % 10) == 0) {
                int flash_idx = s_ghost_intro_timer / 10;

                bui_ghost_set_sprite_pal((flash_idx & 1) ? GHOST_OBP1_FLASH : GHOST_OBP1_NORMAL);
                if (flash_idx >= 8) {
                    s_ghost_intro_phase = 2;
                    s_ghost_intro_timer = 0;
                    bui_ghost_set_sprite_pal(GHOST_OBP1_NORMAL);
                }
            }
            break;
        }
        if (s_ghost_intro_phase == 2) {

            static const uint8_t kFadeOutObp1[] = { 0xE4, 0x90, 0x40, 0x00 };
            s_ghost_intro_timer++;
            if ((s_ghost_intro_timer % 10) == 0) {
                int i = s_ghost_intro_timer / 10;
                if (i < (int)(sizeof(kFadeOutObp1) / sizeof(kFadeOutObp1[0]))) {
                    bui_ghost_set_sprite_pal(kFadeOutObp1[i]);
                } else {

                    uint8_t marowak_dex = gSpeciesToDex[SPECIES_MAROWAK];
                    if (marowak_dex > 0 && marowak_dex <= 151) {
                        for (int t = 0; t < POKEMON_FRONT_CANVAS_TILES; t++) {
                            Display_LoadSpriteTile((uint8_t)(ENEMY_SPR_TILE_BASE + t),
                                                   gPokemonFrontSprite[marowak_dex][t]);
                        }
                    }
                    s_ghost_intro_phase = 3;
                    s_ghost_intro_timer = 0;
                }
            }
            break;
        }
        if (s_ghost_intro_phase == 3) {

            static const uint8_t kFadeInObp1[] = { 0x40, 0x90, 0xE4 };
            s_ghost_intro_timer++;
            if ((s_ghost_intro_timer % 10) == 0) {
                int i = s_ghost_intro_timer / 10;
                if (i <= (int)(sizeof(kFadeInObp1) / sizeof(kFadeInObp1[0]))) {
                    bui_ghost_set_sprite_pal(kFadeInObp1[i - 1]);
                }
                if (i >= (int)(sizeof(kFadeInObp1) / sizeof(kFadeInObp1[0]))) {
                    Display_SetOBP1(GHOST_OBP1_NORMAL);

                    bui_ghost_set_sprite_pal(-1);
                    for (int ty = 0; ty < 7; ty++) {
                        for (int tx = 0; tx < 7; tx++) {
                            int idx = ENEMY_SPR_OAM_BASE + ty * 7 + tx;
                            wShadowOAM[idx].flags &= (uint8_t)~OAM_FLAG_PALETTE;
                        }
                    }

                    bui_place_enemy_sprite_full_oam();
                    BattleUI_EnemySpriteCaptureState();
                    Audio_PlayCry(wEnemyMon.species);
                    {
                        const char *e_name2 = bui_enemy_mon_name();
                        snprintf(s_wait_cry_text, sizeof(s_wait_cry_text), "Wild %s\nappeared!", e_name2);
                    }
                    s_wait_cry_next_state = BUI_SEND_OUT;
                    bui_state = BUI_WAIT_CRY;
                }
            }
        }
        break;

    case BUI_INTRO_SFX: {

        if (Audio_IsSFXPlaying_SilphScope()) { s_intro_sfx_wait++; break; }
        if (s_intro_sfx_timer == 0)
            DBG_PRINTF("[INTRODBG] silph chirp held %d frames (rom budget 18)\n",
                   s_intro_sfx_wait);
        if (++s_intro_sfx_timer < 20) break;
        DBG_PRINTF("[INTRODBG] balls drawn at %d frames after sfx start (rom 38)\n",
               s_intro_sfx_wait + s_intro_sfx_timer);
        fflush(stdout);
        bui_draw_pokeballs();
        snprintf(s_msg_buf, sizeof(s_msg_buf), "%s\nwants to fight!", bui_trainer_name());
        bui_show_text(s_msg_buf);
        bui_state = BUI_SEND_OUT;
        break;
    }

    case BUI_APPEARED:

        bui_state = BUI_SEND_OUT;
        break;

    case BUI_SEND_OUT: {

        bui_hide_pokeballs();
        bui_clear_rect(9, 7, 19, 11);
        if (wIsInBattle == 2)
            bui_clear_rect(0, 0, 11, 3);
        bui_draw_box();
        s_slide_cx = 0;

        bui_state = (wIsInBattle == 2) ? BUI_ENEMY_SLIDE_OUT : BUI_TRAINER_SLIDE_OUT;
        break;
    }

    case BUI_ENEMY_SLIDE_OUT: {
        s_slide_cx += 4;
        for (int ty = 0; ty < 7; ty++)
            for (int tx = 0; tx < 7; tx++) {
                int idx = ENEMY_SPR_OAM_BASE + ty * 7 + tx;
                int nx = ENEMY_SPR_PX_X + tx * 8 + OAM_X_OFS + s_slide_cx;
                wShadowOAM[idx].x = (uint8_t)(nx > 255 ? 0 : nx);
            }
        if (s_slide_cx >= 64) {

            if (s_slide_is_ai_switch) {
                printf("[SWDBG] slide done, showing sent-out text\n");
                fflush(stdout);
                s_slide_is_ai_switch = 0;
                bui_set_enemy_oam_visible(0);
                s_enemy_on_field = 0;

                bui_clear_rect(0, 0, 11, 3);

                bui_draw_box();
                snprintf(s_msg_buf, sizeof(s_msg_buf), "Foe sent out\n%s!",
                         bui_enemy_mon_name());

                bui_show_text_done(s_msg_buf);
                s_grow_stage = 0;
                s_grow_frame = 0;
                s_mid_battle_send  = 1;
                s_shift_pending    = 0;
                s_ai_switch_active = 1;
                s_ai_switch_rejoin = s_ai_rejoin_state;
                bui_state = BUI_ENEMY_SEND_OUT;
                break;
            }

            bui_set_enemy_oam_visible(0);
            const char *e_name3 = bui_enemy_mon_name();
            snprintf(s_msg_buf, sizeof(s_msg_buf), "Foe sent out\n%s!", e_name3);

            bui_show_text_done(s_msg_buf);
            s_grow_stage = 0;
            s_grow_frame = 0;
            s_mid_battle_send = 0;
            bui_state = BUI_ENEMY_SEND_OUT;
        }
        break;
    }

    case BUI_TRAINER_SLIDE_OUT: {
        s_slide_cx += 4;
        for (int ty = 0; ty < 7; ty++)
            for (int tx = 0; tx < 7; tx++) {
                int idx = PLAYER_SLIDE_OAM_BASE + ty * 7 + tx;
                int nx = PLAYER_SPR_COL * 8 + tx * 8 + OAM_X_OFS - s_slide_cx;
                wShadowOAM[idx].x = (uint8_t)(nx < 0 ? 0 : nx);
            }

        if (s_slide_cx >= 72) {
            for (int i = 0; i < 49; i++)
                wShadowOAM[PLAYER_SLIDE_OAM_BASE + i].y = 0;
            s_grow_stage = 0;
            s_grow_frame = 0;
            if (wIsInBattle != 2) {

                uint8_t p_dex2 = gSpeciesToDex[wBattleMon.species];
                if (bui_has_back_sprite(wBattleMon.species, p_dex2))
                    bui_load_player_back_tiles(wBattleMon.species, p_dex2);
                const char *p_name2 = bui_player_mon_name();
                snprintf(s_msg_buf, sizeof(s_msg_buf), "Go! %s!", p_name2);
                bui_show_text(s_msg_buf);
            }

            bui_state = BUI_POKEMON_APPEAR;
        }
        break;
    }

    case BUI_ENEMY_SEND_OUT: {
        uint8_t e_dex4 = gSpeciesToDex[wEnemyMon.species];

        if (s_grow_stage == 0 && s_grow_frame == 0) {

            if (bui_has_front_sprite(wEnemyMon.species, e_dex4))
                bui_load_enemy_front_tiles(wEnemyMon.species, e_dex4);

            bui_place_enemy_sprite_full_oam();
            BattleUI_EnemySpriteCaptureState();
            bui_set_enemy_oam_visible(0);

        }
        s_grow_frame++;

        if (s_grow_stage == 0 && s_grow_frame >= 3) {

            if (bui_has_front_sprite(wEnemyMon.species, e_dex4)) {
                for (int dty = 0; dty < 3; dty++)
                    for (int dtx = 0; dtx < 3; dtx++) {
                        int oidx = ENEMY_SPR_OAM_BASE + (2 + dty) * 7 + (2 + dtx);
                        wShadowOAM[oidx].y    = (uint8_t)(ENEMY_SPR_PX_Y + (4+dty)*8 + OAM_Y_OFS);
                        wShadowOAM[oidx].x    = (uint8_t)(ENEMY_SPR_PX_X + (2+dtx)*8 + OAM_X_OFS);
                        wShadowOAM[oidx].tile = (uint8_t)(ENEMY_SPR_TILE_BASE
                                                          + kDownscale3[dty]*7 + kDownscale3[dtx]);
                    }
            }
            s_grow_stage = 1;  s_grow_frame = 0;
        } else if (s_grow_stage == 1 && s_grow_frame >= 4) {

            bui_set_enemy_oam_visible(0);
            if (bui_has_front_sprite(wEnemyMon.species, e_dex4)) {
                for (int dty = 0; dty < 5; dty++)
                    for (int dtx = 0; dtx < 5; dtx++) {
                        int oidx = ENEMY_SPR_OAM_BASE + (1+dty)*7 + (1+dtx);
                        wShadowOAM[oidx].y    = (uint8_t)(ENEMY_SPR_PX_Y + (2+dty)*8 + OAM_Y_OFS);
                        wShadowOAM[oidx].x    = (uint8_t)(ENEMY_SPR_PX_X + (1+dtx)*8 + OAM_X_OFS);
                        wShadowOAM[oidx].tile = (uint8_t)(ENEMY_SPR_TILE_BASE
                                                          + kDownscale5[dty]*7 + kDownscale5[dtx]);
                    }
            }
            s_grow_stage = 2;  s_grow_frame = 0;
        } else if (s_grow_stage == 2 && s_grow_frame >= 5) {

            bui_place_enemy_sprite_full_oam();
            BattleUI_EnemySpriteCaptureState();
            bui_set_enemy_oam_visible(1);
            Audio_PlayCry(wEnemyMon.species);

            s_enemy_on_field = 1;
            bui_draw_enemy_hud();
            s_grow_stage = 0;  s_grow_frame = 0;
            if (s_mid_battle_send) {
                s_mid_battle_send = 0;
                if (s_shift_pending) {

                    s_shift_pending     = 0;
                    s_retreat_species   = wBattleMon.species;
                    s_switch_slot       = s_shift_slot;
                    s_retreat_stage     = 0;  s_retreat_frame = 0;
                    s_grow_after_switch = 0;
                    const char *old_name = Pokemon_GetName(Species_Dex(s_retreat_species));
                    bui_retreat_text(old_name, s_wait_cry_text, sizeof(s_wait_cry_text));
                    s_wait_cry_text_keep  = 1;
                    s_wait_cry_next_state = BUI_RETREAT_ANIM;
                } else if (s_ai_switch_active) {

                    s_ai_switch_active = 0;
                    s_wait_cry_next_state = s_ai_switch_rejoin;
                } else {

                    s_wait_cry_next_state = BUI_DRAW_HUD;
                }
            } else {

                uint8_t p_dex2 = gSpeciesToDex[wBattleMon.species];
                if (bui_has_back_sprite(wBattleMon.species, p_dex2))
                    bui_load_player_back_tiles(wBattleMon.species, p_dex2);
                const char *p_name2 = bui_player_mon_name();
                snprintf(s_wait_cry_text, sizeof(s_wait_cry_text), "Go! %s!", p_name2);
                s_slide_cx = 0;

                s_wait_cry_text_keep = 1;

                s_wait_cry_delay = 40;
                s_wait_cry_next_state = BUI_TRAINER_SLIDE_OUT;
            }
            bui_state = BUI_WAIT_CRY;
        }
        break;
    }

    case BUI_POKEMON_APPEAR: {

        if (s_grow_stage == 0 && s_grow_frame == 0) {

            s_saved_battle_menu_item = 0;
            bui_draw_enemy_hud();
            bui_draw_player_hud();

            bui_ball_load_poof_tiles();
            for (int i = 0; i < POOF_OAM_COUNT; i++)
                wShadowOAM[POOF_OAM_BASE + i].y = 0;
            bui_draw_player_poof_frame(0);
        }
        s_grow_frame++;

        if (s_grow_stage == 0 && s_grow_frame == 2 && gMoveAnimTraceHook)
            gMoveAnimTraceHook(195 , 1 );
        if (s_grow_stage == 0) {

            int entry    = (s_grow_frame - 1) / 4;
            int subframe = (s_grow_frame - 1) % 4;
            if (s_grow_frame == 5)
                Audio_PlaySFX_BallPoof();
            if (entry >= 6) {

                for (int i = 0; i < POOF_OAM_COUNT; i++)
                    wShadowOAM[POOF_OAM_BASE + i].y = 0;
                bui_clear_rect(1, 5, 7, 11);
                s_grow_stage = 1;  s_grow_frame = 0;
            } else if (subframe == 0) {
                for (int i = 0; i < POOF_OAM_COUNT; i++)
                    wShadowOAM[POOF_OAM_BASE + i].y = 0;
                bui_draw_player_poof_frame(entry);
            }
        } else if (s_grow_stage == 1 && s_grow_frame >= 3) {

            bui_place_player_grow_stage(1);
            s_grow_stage = 2;  s_grow_frame = 0;
        } else if (s_grow_stage == 2 && s_grow_frame >= 4) {

            bui_place_player_grow_stage(2);
            s_grow_stage = 3;  s_grow_frame = 0;
        } else if (s_grow_stage == 3 && s_grow_frame >= 5) {

            Audio_PlayCry(wBattleMon.species);
            bui_place_player_sprite();

            bui_draw_box();
            s_grow_stage = 4;  s_grow_frame = 0;
            if (s_grow_after_switch) {

                s_grow_after_switch = 0;
                s_turn_already_used_pending = 1;
                s_wait_cry_next_state = BUI_MOVE_SELECT;
            } else {

                wActionResultOrTookBattleTurn = 0;
                s_wait_cry_next_state = BUI_DRAW_HUD;
            }
            bui_state = BUI_WAIT_CRY;
        }
        break;
    }

    case BUI_INTRO: {
        bui_draw_enemy_hud();
        bui_draw_player_hud();
        bui_load_sprites();
        const char *e_name = bui_enemy_mon_name();
        const char *p_name = bui_player_mon_name();
        if (wIsInBattle == 2)
            snprintf(s_msg_buf, sizeof(s_msg_buf),
                     "Trainer sent out\n%s!\fGo! %s!", e_name, p_name);
        else
            snprintf(s_msg_buf, sizeof(s_msg_buf),
                     "Wild %s\nappeared!\fGo! %s!", e_name, p_name);
        bui_show_text(s_msg_buf);
        bui_state = BUI_DRAW_HUD;
        break;
    }

    case BUI_DRAW_HUD:

        if (gBattleTurnFenceHook) gBattleTurnFenceHook();
        if (wBattleType == 2 || wBattleType == 1) {

            bui_hide_pokeballs();
            bui_hide_player_slide_oam();
            bui_load_sprites();
            bui_clear_rect(9, 7, 19, 11);
            bui_draw_enemy_hud();
            bui_draw_box();
            bui_cursor = s_saved_battle_menu_item;
            bui_draw_main_menu(bui_cursor);
            hWY = SCREEN_HEIGHT_PX;
            bui_state = BUI_MENU;
            break;
        }
        bui_clear_rows(0, SCREEN_HEIGHT - 1);
        bui_draw_enemy_hud();
        bui_draw_player_hud();
        bui_load_sprites();

        if ((wPlayerBattleStatus2 & ((1u << BSTAT2_NEEDS_TO_RECHARGE) |
                                     (1u << BSTAT2_USING_RAGE))) ||
            (wPlayerBattleStatus1 & ((1u << BSTAT1_THRASHING_ABOUT) |
                                     (1u << BSTAT1_CHARGING_UP) |
                                     (1u << BSTAT1_STORING_ENERGY) |
                                     (1u << BSTAT1_USING_TRAPPING))) ||
            (wEnemyBattleStatus1 & (1u << BSTAT1_USING_TRAPPING))) {
            battle_result_t prep;

            if (s_turn_limit && s_turn_count >= s_turn_limit) {
                bui_state = BUI_MENU;
                break;
            }

            prep = Battle_TurnPrepare();

            if (prep == BATTLE_RESULT_PLAYER_FAINTED) { bui_begin_player_faint_slide(bui_player_mon_name()); break; }
            if (prep == BATTLE_RESULT_ENEMY_FAINTED)  { Battle_HandleEnemyMonFainted(); bui_state = BUI_END; break; }
            s_player_first = Battle_TurnPlayerFirst();
            bui_exec_first_move();
            break;
        }
        bui_cursor = s_saved_battle_menu_item;
        bui_draw_main_menu(bui_cursor);
        hWY = SCREEN_HEIGHT_PX;
        bui_state = BUI_MENU;
        break;

    case BUI_MENU:
        if (wBattleType == 1) {

            if (s_oldman_auto_delay < 0) {
                s_oldman_menu_phase = 0;
                bui_draw_main_menu(0);
                s_oldman_auto_delay = 80;
                return;
            }
            if (--s_oldman_auto_delay > 0) return;
            switch (s_oldman_menu_phase) {
            case 0:
                s_oldman_menu_phase = 1;
                bui_draw_main_menu(2);
                s_oldman_auto_delay = 50;
                return;
            default:
                s_oldman_auto_delay = -1;
                s_oldman_menu_phase = 0;
                bui_set_enemy_oam_visible(0);
                bui_draw_box();
                BagMenu_OpenBattleOldMan();
                bui_state = BUI_BAG_BATTLE;
                break;
            }
            break;
        }
        if (hJoyPressed & PAD_LEFT)  { bui_cursor &= ~1; bui_draw_main_menu(bui_cursor); }
        if (hJoyPressed & PAD_RIGHT) { bui_cursor |=  1; bui_draw_main_menu(bui_cursor); }
        if (hJoyPressed & PAD_UP)    { bui_cursor &= ~2; bui_draw_main_menu(bui_cursor); }
        if (hJoyPressed & PAD_DOWN)  { bui_cursor |=  2; bui_draw_main_menu(bui_cursor); }
        if (hJoyPressed & PAD_A) {
            Audio_PlaySFX_PressAB();

            s_saved_battle_menu_item = bui_cursor;
            switch (bui_cursor) {
            case 0:
                if (wBattleType == 2) {
                    if (wNumSafariBalls == 0) {
                        bui_show_text("No Safari Balls\nleft!");
                        bui_state = BUI_END;
                        break;
                    }
                    wNumSafariBalls--;
                    bui_begin_ball_throw(ITEM_SAFARI_BALL);
                } else if (wBattleMon.status & (STATUS_FRZ | STATUS_SLP_MASK)) {

                    battle_result_t prep = Battle_TurnPrepare();
                    if (prep == BATTLE_RESULT_PLAYER_FAINTED) {
                        bui_begin_player_faint_slide(bui_player_mon_name());
                        break;
                    }
                    if (prep == BATTLE_RESULT_ENEMY_FAINTED) {
                        Battle_HandleEnemyMonFainted();
                        bui_state = BUI_END;
                        break;
                    }
                    s_player_first = Battle_TurnPlayerFirst();
                    bui_exec_first_move();
                    break;
                } else {

                    uint8_t dis_slot = (uint8_t)(wPlayerDisabledMove >> 4);
                    int any_pp = 0;
                    for (int i = 0; i < 4 && !any_pp; i++) {
                        if (dis_slot && i == (int)(dis_slot - 1)) continue;
                        if (wBattleMon.moves[i] && (wBattleMon.pp[i] & 0x3F) > 0)
                            any_pp = 1;
                    }
                    if (!any_pp) {
                        wPlayerSelectedMove  = MOVE_STRUGGLE;
                        wPlayerMoveListIndex = 0;
                        s_struggle_pending   = 1;

                        RomTextSplice(s_msg_buf, sizeof(s_msg_buf),
                                      "_NoMovesLeftText",
                                      "{ram:D009}", bui_player_mon_name());
                        bui_show_text(s_msg_buf);

                        bui_state = BUI_MOVE_SELECT;
                    } else {

                        bui_cursor = (wPlayerMoveListIndex < NUM_MOVES &&
                                      wBattleMon.moves[wPlayerMoveListIndex])
                                   ? (int)wPlayerMoveListIndex : 0;

                        bui_draw_move_menu(bui_cursor);
                        bui_state = BUI_MOVE_SELECT;
                    }
                }
                break;
            case 1:
                if (wBattleType == 2) {
                    bui_begin_safari_item(1);
                    break;
                }
                bui_set_enemy_oam_visible(0);
                PartyMenu_Open(2 );
                bui_state = BUI_SWITCH_SELECT;
                break;
            case 2:
                if (wBattleType == 2) {
                    bui_begin_safari_item(0);
                    break;
                }
                bui_set_enemy_oam_visible(0);

                bui_draw_box();
                BagMenu_OpenBattle();
                bui_state = BUI_BAG_BATTLE;
                break;
            case 3:
                wActionResultOrTookBattleTurn = 0;
                if (Battle_TryRunningFromBattle()) {

                    Audio_PlaySFX_Run();
                    bui_show_text(RomText("GotAwayText"));
                    bui_state = BUI_END;
                } else if (wActionResultOrTookBattleTurn) {

                    bui_show_text(RomText("CantEscapeText"));
                    s_turn_already_used_pending = 1;
                    bui_state = BUI_MOVE_SELECT;
                } else {

                    bui_show_text(RomText("NoRunningText"));
                    bui_state = BUI_DRAW_HUD;
                }
                break;
            }
        }
        break;

    case BUI_MOVE_SELECT:

        if (s_struggle_pending || s_turn_already_used_pending) {
            s_struggle_pending = 0;
            s_turn_already_used_pending = 0;
            battle_result_t prep = Battle_TurnPrepare();

            if (prep == BATTLE_RESULT_PLAYER_FAINTED) { bui_begin_player_faint_slide(bui_player_mon_name()); break; }
            if (prep == BATTLE_RESULT_ENEMY_FAINTED)  { Battle_HandleEnemyMonFainted(); bui_state = BUI_END; break; }
            s_player_first = Battle_TurnPlayerFirst();
            bui_exec_first_move();
            break;
        }

        {
            int n = 0;
            for (int i = 0; i < NUM_MOVES; i++)
                if (wBattleMon.moves[i]) n++;
            if (n < 1) n = 1;
            if (hJoyPressed & PAD_UP)   { bui_cursor = (bui_cursor + n - 1) % n;
                                          bui_draw_move_menu(bui_cursor); }
            if (hJoyPressed & PAD_DOWN) { bui_cursor = (bui_cursor + 1) % n;
                                          bui_draw_move_menu(bui_cursor); }
        }

        if (hJoyPressed & PAD_B) {
            Audio_PlaySFX_PressAB();
            bui_cursor = s_saved_battle_menu_item;
            bui_clear_move_menu_overlay();
            bui_draw_main_menu(bui_cursor);
            bui_state = BUI_MENU;
        }

        if (hJoyPressed & PAD_A) {
            Audio_PlaySFX_PressAB();
            uint8_t move = wBattleMon.moves[bui_cursor];
            if (!move) break;

            if ((wBattleMon.pp[bui_cursor] & 0x3F) == 0) {
                bui_show_text(RomText("_MoveNoPPText"));
                break;
            }

            {
                uint8_t dis_slot = (uint8_t)((wPlayerDisabledMove >> 4) & 0x0F);
                if (dis_slot != 0 && (uint8_t)(dis_slot - 1u) == (uint8_t)bui_cursor) {
                    bui_show_text(RomText("_MoveDisabledText"));
                    break;
                }
            }

            bui_clear_move_menu_overlay();

            wPlayerSelectedMove  = move;
            wPlayerMoveListIndex = (uint8_t)bui_cursor;
            s_mimic_choice_made  = 0u;
            if (AmberScript_IsEnabled() ? AmberScript_IsAutoWinEnabled() : DebugCLI_IsAutoWinEnabled())
                s_debug_autowin_pending = 1;

            battle_result_t prep = Battle_TurnPrepare();
            if (prep == BATTLE_RESULT_PLAYER_FAINTED) {

                bui_begin_player_faint_slide(bui_player_mon_name());
                break;
            }
            if (prep == BATTLE_RESULT_ENEMY_FAINTED) {

                Battle_HandleEnemyMonFainted();
                bui_state = BUI_END;
                break;
            }
            s_player_first = Battle_TurnPlayerFirst();
            bui_exec_first_move();
        }
        break;

    case BUI_MIMIC_PROMPT:
        bui_cursor = 0;
        bui_draw_mimic_menu(0);
        Text_OverwriteTopLine("WHICH TECHNIQUE?");
        s_suppress_move_text = 1u;
        bui_state = BUI_MIMIC_SELECT;
        break;

    case BUI_MIMIC_SELECT: {
        if (hJoyPressed & PAD_UP)   { if (bui_cursor > 0) bui_cursor--; bui_draw_mimic_menu(bui_cursor); }
        if (hJoyPressed & PAD_DOWN) { if (bui_cursor < 3) bui_cursor++; bui_draw_mimic_menu(bui_cursor); }
        if (!(hJoyPressed & PAD_A)) break;

        Audio_PlaySFX_PressAB();
        if (!wEnemyMon.moves[bui_cursor]) break;

        wPlayerMimicChoice = (uint8_t)bui_cursor;
        bui_clear_move_menu_overlay();
        Text_Close();

        s_mimic_choice_made = 1u;
        if (s_mimic_resume_slot == 2u) {
            s_mimic_resume_slot = 0u;
            bui_exec_second_move();
        } else {
            s_mimic_resume_slot = 0u;
            bui_exec_first_move();
        }
        break;
    }

    case BUI_EXEC_MOVE_B: {
        if (s_post_move_text[0]) {
            snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", s_post_move_text);
            s_post_move_text[0] = '\0';
            bui_show_text(s_msg_buf);
            return;
        }
        bui_flush_pending_hud_redraw();

        if (s_drain_text[0]) {
            snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", s_drain_text);
            s_drain_text[0] = '\0';
            bui_show_text(s_msg_buf);
            return;
        }

        if (wEscapedFromBattle) {
            wEscapedFromBattle = 0;
            bui_state = BUI_END;
            return;
        }

        if (s_player_first) {
            if (s_debug_autowin_pending) {
                bui_debug_force_enemy_party_defeated();
                s_debug_autowin_pending = 0;
            }

            if (wEnemyMon.hp == 0) {
                bui_start_enemy_faint_anim();
                return;
            }
        } else {

            if (wBattleMon.hp == 0) {

                bui_handle_player_fainted(s_name_b);
                return;
            }
        }

        bui_begin_residual(0);
        break;
    }

    case BUI_MOVE_ANIM: {

        if (!s_move_anim_active && !s_pending_status_text_active &&
            s_move_anim_queue_count > 0u &&
            (uint8_t)(s_move_anim_queue_index + 1u) < s_move_anim_queue_count) {
            s_move_anim_queue_index++;
            bui_start_move_anim_from_queue();
            break;
        }

        if (s_move_anim_active) {
            if (bui_move_anim_tick()) {
                uint8_t keep_enemy_hidden = 0u;
                s_move_anim_active = 0;
                printf("[DRAINDBG] ANIM done idx=%u/%u\n",
                       (unsigned)s_move_anim_queue_index,
                       (unsigned)s_move_anim_queue_count);
                fflush(stdout);
                if (s_move_anim_queue_count > 0u &&
                    (uint8_t)(s_move_anim_queue_index + 1u) < s_move_anim_queue_count) {

                    if (s_pending_status_text_active) {
                        s_pending_status_text_active = 0u;
                        bui_show_text(s_pending_status_text);
                        break;
                    }
                    s_move_anim_queue_index++;
                    bui_start_move_anim_from_queue();
                    break;
                }
                hWhoseTurn = s_move_anim_owner_turn;
                if (hWhoseTurn == 0u && s_player_charge_resolving_anim) {
                    s_player_charge_hidden = 0;
                    s_player_charge_resolving_anim = 0;
                } else if (hWhoseTurn != 0u && s_enemy_charge_resolving_anim) {
                    s_enemy_charge_hidden = 0;
                    s_enemy_charge_resolving_anim = 0;
                }

                if (hWhoseTurn != 0u) {
                    uint8_t move_id = wEnemyMoveNum;
                    uint8_t move_eff = (move_id < NUM_MOVE_DEFS) ? gMoves[move_id].effect : 0u;
                    if ((move_eff == EFFECT_CHARGE || move_eff == EFFECT_FLY) &&
                        (wEnemyBattleStatus1 & (1u << BSTAT1_CHARGING_UP))) {
                        keep_enemy_hidden = 1u;
                    }
                }
                if (!keep_enemy_hidden)
                    bui_set_enemy_oam_visible(1);
            }
            break;
        }

        if (s_anim_total == 0) {

            Display_SetShakeOffset(0, 0);
            if (s_move_anim_should_hit_sfx && !s_move_anim_hit_sfx_started && !wMoveMissed) {
                uint8_t dmg_mult = bui_evt_pop_hit_sfx_or((uint8_t)(wDamageMultipliers & 0x7Fu));
                Audio_PlaySFX_BattleHit(dmg_mult);
                s_move_anim_hit_sfx_started = 1;
            }
            if (Audio_IsSFXPlaying()) {
                break;
            }
            if (s_pending_status_text_active) {
                s_pending_status_text_active = 0u;
                bui_show_text(s_pending_status_text);
                break;
            }
            if (bui_advance_hit()) break;
            bui_state = BUI_HP_ANIM;
            break;
        }

        int ox = 0, oy = 0;
        switch (s_anim_type) {
        case 1: {

            int step  = s_anim_frame / 6;
            int phase = s_anim_frame % 6;
            if (step == 0)
                oy = (phase < 3) ? 8 : 0;
            else
                oy = (phase < 3) ? 0 : (8 - step);
            break;
        }
        case 2:

            ox = ((s_anim_frame % 9) < 5) ? 8 : 0;
            break;
        case 4: {

            int hidden = (s_anim_frame % 10) < 5;
            bui_set_enemy_oam_visible(!hidden);
            break;
        }
        case 5:

            ox = ((s_anim_frame % 9) < 5) ? 2 : 0;
            break;
        }

        Display_SetShakeOffset(ox, oy);
        s_anim_frame++;

        if (s_anim_frame >= s_anim_total) {
            Display_SetShakeOffset(0, 0);
            bui_set_enemy_oam_visible(1);
            if (s_move_anim_should_hit_sfx && !s_move_anim_hit_sfx_started && !wMoveMissed) {
                uint8_t dmg_mult = bui_evt_pop_hit_sfx_or((uint8_t)(wDamageMultipliers & 0x7Fu));
                Audio_PlaySFX_BattleHit(dmg_mult);
                s_move_anim_hit_sfx_started = 1;
            }
            if (Audio_IsSFXPlaying()) {
                break;
            }

            if (s_hit_scroll_pending && s_hit_index < s_hit_hp_count) {
                s_hit_scroll_pending = 0;
                bui_setup_hit_hp_scroll(s_hit_index);
                s_hp_anim_multihit = 1;
                bui_state = BUI_HP_ANIM;
                break;
            }
            if (bui_advance_hit()) break;
            bui_state = BUI_HP_ANIM;
        }
        break;
    }

    case BUI_HP_ANIM: {

        if (s_post_hp_anim_running) {
            if (s_move_anim_active && bui_move_anim_tick()) {
                s_move_anim_active = 0;
                bui_set_enemy_oam_visible(1);
            }
            if (!s_move_anim_active) {
                hWhoseTurn = s_residual_anim_saved_turn;
                if (Audio_IsSFXPlaying()) break;
                s_post_hp_anim_running = 0;
                if (s_anim_first) bui_finish_first_move();
                else              bui_finish_second_move();
            }
            break;
        }

        if (s_hp_anim_deferred) {
            s_hp_anim_deferred = 0;
            if (s_post_move_text[0]) {
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", s_post_move_text);
                s_post_move_text[0] = '\0';
                bui_show_text(s_msg_buf);
                return;
            }
        }
        if (bui_hp_anim_step_scaled()) {

            if (s_hp_anim_multihit) {
                s_hp_anim_multihit = 0;

                if ((uint8_t)(s_hit_index + 1u) >= s_hit_hp_count)
                    s_hp_hold_active = 0;

                if (s_hp_anim_who == 1) {
                    bui_put_hp_number(s_hp_cur_hp, (int)s_hp_pre_max);
                    bui_draw_hp_bar_px(12, 9,
                        calc_hp_pixels(s_hp_cur_hp, (int)s_hp_pre_max));
                } else {
                    bui_draw_hp_bar_px(4, 2,
                        calc_hp_pixels(s_hp_cur_hp, (int)s_hp_pre_max));
                }
                bui_state = BUI_MOVE_ANIM;
                break;
            }
            if (s_hp_stage2_pending) {
                s_hp_stage2_pending = 0;
                s_hp_new_hp = s_hp_stage2_hp;
                s_hp_new_px = s_hp_stage2_px;
                s_hp_half_frame = 0;
                s_hp_bar_pending = 0;
                s_hp_delay = 0;

                if (s_multihit_replay_armed && s_hit_text_count < 2u) {
                    s_multihit_replay_armed = 0;
                    s_anim_frame = 0;
                    bui_restart_move_anim_replay(s_multihit_replay_whose);
                    s_move_anim_hit_sfx_started = 0;
                    bui_state = BUI_MOVE_ANIM;
                    break;
                }
                s_multihit_replay_armed = 0;
                break;
            }

            if (s_apply_text_pending) {
                s_apply_text_pending = 0u;
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", s_apply_text);
                bui_show_text(s_msg_buf);
                break;
            }

            if (bui_evt_has_hp_target()) {
                bui_setup_hp_anim(s_hp_anim_who);
                break;
            }

            if (s_post_hp_anim_pending) {

                if (s_pre_shake_text_pending) {
                    s_pre_shake_text_pending = 0u;
                    snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", s_pre_shake_text);
                    bui_show_text(s_msg_buf);
                    break;
                }
                s_post_hp_anim_pending = 0u;
                bui_start_residual_anim(s_post_hp_anim_id, s_post_hp_anim_turn);
                if (s_move_anim_active) {
                    s_post_hp_anim_running = 1u;
                    break;
                }
            }

            if (s_anim_first) bui_finish_first_move();
            else              bui_finish_second_move();
        }
        break;
    }

    case BUI_RESIDUAL_EVENT: {
        while (s_residual_evt_q_index < s_residual_evt_q_count) {
            battle_event_t *bev = &s_residual_evt_q[s_residual_evt_q_index++];
            if (bev->type == BATTLE_EVENT_RESIDUAL_MSG) {
                if (bui_format_residual_event_text(s_msg_buf, sizeof(s_msg_buf), bev))
                    bui_show_text(s_msg_buf);
                break;
            }
            if (bev->type == BATTLE_EVENT_PLAY_ANIM) {
                uint8_t turn = (bev->arg1 <= 1u) ? bev->arg1 : (uint8_t)hWhoseTurn;
                bui_start_residual_anim(bev->arg0, turn);
                if (s_move_anim_active) {
                    bui_state = BUI_RESIDUAL_ANIM;
                    break;
                }
                continue;
            }
            if (bev->type == BATTLE_EVENT_HP_TARGET && bev->arg0 <= 1u) {
                bui_setup_hp_anim((int)bev->arg0);
                if (bev->arg2 & 0x80u) {

                    uint16_t snap = (uint16_t)bev->arg1
                                  | ((uint16_t)(bev->arg2 & 0x7Fu) << 8);
                    s_hp_new_hp = (int)snap;
                    s_hp_new_px = calc_hp_pixels((int)snap, (int)s_hp_pre_max);
                    if (bev->arg0 == 1u) s_hp_pre_player_hp = snap;
                    else                 s_hp_pre_enemy_hp  = snap;
                }
                bui_state = BUI_RESIDUAL_HP_ANIM;
                break;
            }
        }
        if (bui_state != BUI_RESIDUAL_EVENT) break;

        if (s_residual_evt_q_index < s_residual_evt_q_count) break;
        if (!s_residual_alive) {

            bui_draw_enemy_hud();
            bui_draw_player_hud();
            s_residual_delay_frames = BUI_RESIDUAL_FAINT_DELAY_FRAMES;
            bui_state = BUI_RESIDUAL_FAINT_DELAY;
            break;
        }
        if (s_residual_evt_q_count > 0u) {

            s_residual_delay_frames = BUI_RESIDUAL_FAINT_DELAY_FRAMES;
            bui_state = BUI_RESIDUAL_FAINT_DELAY;
            break;
        }
        bui_state = BUI_RESIDUAL_RESOLVE;
        break;
    }

    case BUI_RESIDUAL_ANIM:
        if (s_move_anim_active && bui_move_anim_tick()) {
            s_move_anim_active = 0;
            bui_set_enemy_oam_visible(1);
        }
        if (!s_move_anim_active) {
            hWhoseTurn = s_residual_anim_saved_turn;

            if (Audio_IsSFXPlaying()) break;
            bui_state = BUI_RESIDUAL_EVENT;
        }
        break;

    case BUI_RESIDUAL_HP_ANIM: {
        if (bui_hp_anim_step_scaled())
            bui_state = BUI_RESIDUAL_EVENT;
        break;
    }

    case BUI_RESIDUAL_FAINT_DELAY:
        if (s_residual_delay_frames > 0) {
            s_residual_delay_frames--;
            break;
        }
        bui_state = BUI_RESIDUAL_RESOLVE;
        break;

    case BUI_RESIDUAL_RESOLVE:
        bui_draw_enemy_hud();
        bui_draw_player_hud();
        if (!s_residual_alive) {
            if (s_residual_phase == 0) {

                if (s_player_first) {
                    bui_handle_player_fainted(s_name_a);
                } else {

                    bui_start_enemy_faint_anim();
                }
            } else {

                if (s_player_first) {

                    bui_start_enemy_faint_anim();
                } else {
                    bui_handle_player_fainted(s_name_b);
                }
            }
            break;
        }

        if (s_residual_phase == 0) {
            bui_exec_second_move();
        } else {
            Battle_CheckNumAttacksLeft();
            bui_draw_enemy_hud();
            bui_draw_player_hud();
            bui_state = BUI_DRAW_HUD;
        }
        break;

    case BUI_EXEC_SECOND:
        bui_exec_second_move();
        break;

    case BUI_TURN_END: {
        if (s_post_move_text[0]) {
            snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", s_post_move_text);
            s_post_move_text[0] = '\0';
            bui_show_text(s_msg_buf);
            return;
        }
        bui_flush_pending_hud_redraw();

        if (s_drain_text[0]) {
            snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", s_drain_text);
            s_drain_text[0] = '\0';
            bui_show_text(s_msg_buf);
            return;
        }

        bui_flush_status_hud_redraws();

        if (wEscapedFromBattle) {
            wEscapedFromBattle = 0;
            bui_show_text(RomText("_GotAwayText"));
            bui_state = BUI_END;
            return;
        }

        if (s_player_first) {
            if (wBattleMon.hp == 0) {

                bui_draw_enemy_hud();
                bui_handle_player_fainted(s_name_a);
                return;
            }
        } else {
            if (s_debug_autowin_pending) {
                bui_debug_force_enemy_party_defeated();
                s_debug_autowin_pending = 0;
            }
            if (wEnemyMon.hp == 0) {

                bui_start_enemy_faint_anim();
                return;
            }
        }

        bui_begin_residual(1);
        break;
    }

    case BUI_TURN_FINISH:
        Battle_CheckNumAttacksLeft();
        bui_flush_status_hud_redraws();
        bui_draw_enemy_hud();
        bui_draw_player_hud();
        bui_state = BUI_DRAW_HUD;
        break;

    case BUI_EXP_DRAIN: {

        if (s_pending_lvl_stats.valid) {

            bui_clear_move_menu_overlay();
            bui_draw_levelup_stats(&s_pending_lvl_stats);
            s_pending_lvl_stats.valid = 0;
            bui_state = BUI_LEVELUP_STATS;
            break;
        }
        battleexp_event_t ev;
        if (BattleExp_TakeNextEvent(&ev)) {
            if (ev.type == BEXP_EVENT_TEXT) {
                bui_draw_player_hud();

                if (ev.stats.valid) Audio_PlaySFX_LevelUp();
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", ev.text);
                bui_show_text(s_msg_buf);

                s_pending_lvl_stats = ev.stats;

            } else if (ev.type == BEXP_EVENT_LEARN_MOVE) {
                s_learn_slot   = ev.slot;
                s_learn_move   = ev.move_id;
                s_learn_cursor = 0;
                s_learn_phase  = 0;
                bui_learn_save_screen();
                bui_state = BUI_LEARN_FORGET_YESNO;
            }
        } else if (s_exp_suffix[0] != '\0') {

            if (bui_exp_dest == BUI_TRAINER_VICTORY_SLIDE) {
                extern uint8_t wGymLeaderNo;

                if (AmberScript_IsEnabled() && wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST)
                    JohtoMusic_Stop();
                Music_Play((wGymLeaderNo || s_is_champion_room_battle)
                               ? MUSIC_DEFEATED_GYM_LEADER
                               : MUSIC_DEFEATED_TRAINER);
            }
            snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", s_exp_suffix);
            s_exp_suffix[0] = '\0';

            if (bui_exp_dest == BUI_ENEMY_SEND_OUT) {
                wDoNotWaitForButtonPress = 1;
                Text_KeepTilesOnClose();
            }
            bui_show_text(s_msg_buf);

            if (bui_exp_dest == BUI_TRAINER_VICTORY_SLIDE)
                Text_KeepTilesOnClose();

            if (s_exp_suffix2[0] == '\0')
                bui_state = bui_exp_dest;
        } else if (s_exp_suffix2[0] != '\0') {
            snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", s_exp_suffix2);
            s_exp_suffix2[0] = '\0';
            bui_show_text(s_msg_buf);
            bui_state = bui_exp_dest;
        } else {
            bui_state = bui_exp_dest;
        }
        break;
    }

    case BUI_LEVELUP_STATS:
        if (hJoyPressed & PAD_A) {

            bui_clear_rect(9, 2, 19, 11);
            bui_state = BUI_EXP_DRAIN;
        }
        break;

    case BUI_LEARN_FORGET_YESNO: {
        if (s_learn_phase == 0) {
            const char *mon = bui_party_mon_name(s_learn_slot);
            const char *mv  = bui_move_name(s_learn_move);
            snprintf(s_msg_buf, sizeof(s_msg_buf),
                     "%s is\ntrying to learn\n%s!\f"
                     "But, %s\ncan't learn more\nthan 4 moves!\f"
                     "Delete an older\nmove to make room\nfor %s?",
                     mon, mv, mon, mv);
            bui_show_text_done(s_msg_buf);
            s_learn_phase = 1;
            break;
        }
        if (s_learn_phase == 1) {
            if (Text_IsOpen()) break;
            Text_BlitBoxToBGAndHideWindow();
            s_learn_cursor = 0;
            bui_draw_learn_yesno(s_learn_cursor);
            s_learn_phase = 2;
            break;
        }
        if (hJoyPressed & (PAD_UP | PAD_DOWN)) s_learn_cursor ^= 1;
        bui_draw_learn_yesno(s_learn_cursor);
        if (hJoyPressed & (PAD_A | PAD_B)) {
            Audio_PlaySFX_PressAB();
            int chose_no = (hJoyPressed & PAD_B) || (s_learn_cursor == 1);
            bui_learn_restore_rect(14, 7, 19, 11);
            s_learn_phase = 0;

            bui_state = chose_no ? BUI_LEARN_STOP_YESNO : BUI_LEARN_PICK_MOVE;
        }
        break;
    }

    case BUI_LEARN_PICK_MOVE: {
        if (s_learn_slot >= wPartyCount) {
            bui_state = BUI_EXP_DRAIN;
            break;
        }
        if (s_learn_phase == 0) {
            bui_show_text_done(RomText("_WhichMoveToForgetText"));
            s_learn_phase = 1;
            break;
        }
        if (s_learn_phase == 1) {
            if (Text_IsOpen()) break;
            Text_BlitBoxToBGAndHideWindow();
            bui_draw_learn_move_list(s_learn_slot, s_learn_cursor);
            s_learn_phase = 2;
            break;
        }
        if (hJoyPressed & PAD_UP) {
            if (s_learn_cursor > 0) s_learn_cursor--;
        } else if (hJoyPressed & PAD_DOWN) {
            if (s_learn_cursor < NUM_MOVES - 1) s_learn_cursor++;
        }
        bui_draw_learn_move_list(s_learn_slot, s_learn_cursor);

        if (hJoyPressed & (PAD_A | PAD_B)) {
            Audio_PlaySFX_PressAB();

            bui_learn_restore_rect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
            s_learn_phase = 0;

            if (hJoyPressed & PAD_B) {
                bui_state = BUI_LEARN_STOP_YESNO;
                break;
            }
            uint8_t old_move = wPartyMons[s_learn_slot].base.moves[s_learn_cursor];
            if (is_hm_move(old_move)) {
                bui_show_text(RomText("_HMCantDeleteText"));
                bui_state = BUI_LEARN_CANT_FORGET;
                break;
            }
            s_learn_old_move = old_move;
            wPartyMons[s_learn_slot].base.moves[s_learn_cursor] = s_learn_move;
            wPartyMons[s_learn_slot].base.pp[s_learn_cursor] =
                (s_learn_move < NUM_MOVE_DEFS) ? gMoves[s_learn_move].pp : 0;
            if (s_learn_slot == wPlayerMonNumber) {
                wBattleMon.moves[s_learn_cursor] = s_learn_move;
                wBattleMon.pp[s_learn_cursor] = wPartyMons[s_learn_slot].base.pp[s_learn_cursor];
            }

            Text_SetPendingSFX(Audio_PlaySFX_Swap);
            bui_show_text(RomText("_OneTwoAndText"));
            bui_state = BUI_LEARN_SWAP_TEXT;
        }
        break;
    }

    case BUI_LEARN_STOP_YESNO: {
        if (s_learn_phase == 0) {
            snprintf(s_msg_buf, sizeof(s_msg_buf), "Abandon learning\n%s?",
                     bui_move_name(s_learn_move));
            bui_show_text_done(s_msg_buf);
            s_learn_phase = 1;
            break;
        }
        if (s_learn_phase == 1) {
            if (Text_IsOpen()) break;
            Text_BlitBoxToBGAndHideWindow();
            s_learn_cursor = 0;
            bui_draw_learn_yesno(s_learn_cursor);
            s_learn_phase = 2;
            break;
        }
        if (hJoyPressed & (PAD_UP | PAD_DOWN)) s_learn_cursor ^= 1;
        bui_draw_learn_yesno(s_learn_cursor);
        if (hJoyPressed & (PAD_A | PAD_B)) {
            Audio_PlaySFX_PressAB();
            int chose_no = (hJoyPressed & PAD_B) || (s_learn_cursor == 1);
            bui_learn_restore_rect(14, 7, 19, 11);
            s_learn_phase = 0;
            if (chose_no) {
                bui_state = BUI_LEARN_FORGET_YESNO;
            } else {
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s\ndid not learn\n%s!",
                         bui_party_mon_name(s_learn_slot), bui_move_name(s_learn_move));
                bui_show_text(s_msg_buf);
                bui_state = BUI_LEARN_RESULT_TEXT;
            }
        }
        break;
    }

    case BUI_LEARN_SWAP_TEXT:
        if (Text_IsOpen()) break;
        bui_show_text(RomText("_PoofText"));
        bui_state = BUI_LEARN_POOF_TEXT;
        break;

    case BUI_LEARN_POOF_TEXT:
        if (Text_IsOpen()) break;
        {
            const char *mon = bui_party_mon_name(s_learn_slot);
            const char *oldm = (s_learn_old_move < NUM_MOVE_DEFS && gMoveNames[s_learn_old_move])
                               ? gMoveNames[s_learn_old_move] : "a move";
            snprintf(s_msg_buf, sizeof(s_msg_buf), "%s forgot\n%s!\fAnd...", mon, oldm);
            bui_show_text(s_msg_buf);
        }
        bui_state = BUI_LEARN_FORGOT_TEXT;
        break;

    case BUI_LEARN_FORGOT_TEXT:
        if (Text_IsOpen()) break;
        {
            const char *mon = bui_party_mon_name(s_learn_slot);
            const char *newm = (s_learn_move < NUM_MOVE_DEFS && gMoveNames[s_learn_move])
                               ? gMoveNames[s_learn_move] : "a move";
            Audio_PlaySFX_LevelUp();
            snprintf(s_msg_buf, sizeof(s_msg_buf), "%s learned\n%s!", mon, newm);
            bui_show_text(s_msg_buf);
        }
        bui_state = BUI_LEARN_LEARNED_TEXT;
        break;

    case BUI_LEARN_LEARNED_TEXT:
        if (Text_IsOpen()) break;
        bui_state = BUI_LEARN_RESULT_TEXT;
        break;

    case BUI_LEARN_CANT_FORGET:
        if (Text_IsOpen()) break;
        s_learn_phase = 0;
        bui_state = BUI_LEARN_PICK_MOVE;
        break;

    case BUI_LEARN_RESULT_TEXT:
        if (Text_IsOpen()) break;
        s_learn_slot = 0xFF;
        s_learn_move = 0;
        s_learn_old_move = 0;
        s_learn_phase = 0;
        s_learn_screen_saved = 0;
        bui_state = BUI_EXP_DRAIN;
        break;

    case BUI_ENEMY_FAINT_ANIM: {
        if (--s_faint_timer <= 0) {
            s_faint_step++;
            if (s_faint_step > 7) {

                s_faint_step = 0;
                bui_handle_enemy_fainted();
            } else {
                bui_enemy_faint_oam(s_faint_step);
                s_faint_timer = 2;
            }
        }
        break;
    }

    case BUI_PLAYER_FAINT_ANIM: {
        if (--s_faint_timer <= 0) {
            s_faint_step++;
            if (s_faint_step > 7) {
                s_faint_step = 0;
                Audio_PlaySFX_Faint();
                Audio_PlayCry(wBattleMon.species);
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s\nfainted!",
                         s_player_faint_name[0] ? s_player_faint_name : s_name_b);
                bui_show_text(s_msg_buf);
                bui_state = BUI_PLAYER_FAINTED;
            } else {
                bui_player_faint_bg(s_faint_step);
                s_faint_timer = 2;
            }
        }
        break;
    }

    case BUI_PLAYER_FAINTED: {

        if (wEnemyMon.hp == 0) {

            Battle_AwardExpForFaintedEnemy();
            if (wIsInBattle != 2) {
                bui_exp_dest    = BUI_FADE_WHITE;
                s_exp_suffix[0] = '\0';
                s_exp_suffix2[0] = '\0';
            } else if (!Battle_AnyEnemyPokemonAliveCheck()) {

                Battle_TrainerBattleVictory();
                bui_begin_trainer_victory_seq();
            } else {
                Battle_ReplaceFaintedEnemyMon();
                const char *nn = bui_enemy_mon_name();
                bui_exp_dest = BUI_PLAYER_FAINTED;
                snprintf(s_exp_suffix, sizeof(s_exp_suffix), "Foe sent out\n%s!", nn);
                s_exp_suffix2[0] = '\0';
            }
            bui_state = BUI_EXP_DRAIN;
            return;
        }

        if (!Battle_AnyPartyAlive()) {

            if ((int)gEngagedTrainerClass == OPP_RIVAL1 - OPP_ID_OFFSET) {
                s_rival1_loss   = 1;
                s_victory_timer = 0;
                s_slide_cx      = 48;
                bui_state       = BUI_TRAINER_VICTORY_SLIDE;
                return;
            }

            Battle_HandlePlayerBlackOut();
            bui_show_text(RomText("_PlayerBlackedOutText2"));

            s_bo_fade_step = -1;
            bui_state = BUI_BLACKOUT_FADE;
            return;
        }

        if (wIsInBattle != 2) {
            bui_cursor = 0;
            bui_show_text(RomText("_UseNextMonText"));
            bui_state = BUI_USE_NEXT_MON;
        } else {
            bui_open_party_select();
        }
        break;
    }

    case BUI_USE_NEXT_MON: {

        bui_draw_box();
        bui_put_str(1, 13, bui_cursor == 0 ? ">YES" : " YES");
        bui_put_str(1, 14, bui_cursor == 1 ? ">NO " : " NO ");

        if (hJoyPressed & (PAD_UP | PAD_DOWN))
            bui_cursor ^= 1;

        if ((hJoyPressed & PAD_A) || (hJoyPressed & PAD_B)) {
            int chose_no = (bui_cursor == 1) || (hJoyPressed & PAD_B);
            if (!chose_no) {

                bui_open_party_select();
            } else {

                int ran = Battle_TryRunningFromBattle();
                wActionResultOrTookBattleTurn = 0;
                if (ran) {
                    Audio_PlaySFX_Run();
                    bui_show_text(RomText("_GotAwayText"));
                    bui_state = BUI_END;
                } else {
                    bui_open_party_select();
                }
            }
        }
        break;
    }

    case BUI_SHIFT_PROMPT: {
        if (s_shift_phase == 0) {

            char pname[NAME_LENGTH];
            bui_decode_player_name(pname);
            const char *emon = bui_enemy_mon_name();
            snprintf(s_msg_buf, sizeof(s_msg_buf),
                     "%s is\nabout to use\n%s!\fWill %s\nchange #MON?",
                     bui_trainer_name(), emon, pname);

            bui_show_text_done(s_msg_buf);
            s_shift_phase = 1;
            break;
        }
        if (s_shift_phase == 1) {
            Text_BlitBoxToBGAndHideWindow();
            bui_cursor = 0;
            bui_draw_shift_yesno();
            s_shift_phase = 2;
            break;
        }
        if (hJoyPressed & (PAD_UP | PAD_DOWN)) bui_cursor ^= 1;
        bui_draw_shift_yesno();
        if (hJoyPressed & PAD_A) {
            Audio_PlaySFX_PressAB();
            if (bui_cursor == 0) {
                bui_set_enemy_oam_visible(0);

                PartyMenu_Open(PARTY_MENU_BATTLE_SHIFT);
                bui_state = BUI_SHIFT_PARTY;
            } else {
                bui_shift_to_send_out();
            }
        } else if (hJoyPressed & PAD_B) {
            Audio_PlaySFX_PressAB();
            bui_shift_to_send_out();
        }
        break;
    }
    case BUI_SHIFT_PARTY: {
        if (!PartyMenu_IsOpen()) { PartyMenu_Open(PARTY_MENU_BATTLE_SHIFT); break; }
        PartyMenu_Tick();
        if (PartyMenu_IsOpen()) break;
        bui_restore_battle_palette();
        int sslot = PartyMenu_GetSelected();
        if (sslot < 0) { bui_shift_to_send_out(); break; }

        s_shift_pending = 1;
        s_shift_slot    = (uint8_t)sslot;
        bui_shift_to_send_out();
        break;
    }

    case BUI_PARTY_SELECT: {
        if (!PartyMenu_IsOpen()) {

            PartyMenu_Open(1 );
            break;
        }
        PartyMenu_Tick();
        if (PartyMenu_IsOpen()) break;
        bui_restore_battle_palette();

        int slot = PartyMenu_GetSelected();
        Battle_ChooseNextMon((uint8_t)slot);

        bui_clear_rows(0, SCREEN_HEIGHT - 1);
        bui_draw_enemy_hud();
        bui_draw_player_hud();
        bui_load_sprites();
        bui_clear_rect(1, 5, 7, 11);

        const char *new_name = bui_player_mon_name();
        const char *go_prefix;
        if (wEnemyMon.max_hp == 0 || wEnemyMon.hp == 0) {
            go_prefix = "Go";
        } else {
            int pct = (int)wEnemyMon.hp * 100 / (int)wEnemyMon.max_hp;
            if (pct >= 70)      go_prefix = "Go";
            else if (pct >= 40) go_prefix = "Do it";
            else                go_prefix = "Get'm";
        }
        snprintf(s_msg_buf, sizeof(s_msg_buf), "%s! %s!", go_prefix, new_name);
        bui_show_text(s_msg_buf);

        s_grow_after_switch = 0;
        s_grow_stage        = 0;
        s_grow_frame        = 0;
        bui_state = BUI_POKEMON_APPEAR;
        break;
    }

    case BUI_SWITCH_SELECT: {
        if (!PartyMenu_IsOpen()) {

            PartyMenu_Open(2);
            break;
        }
        PartyMenu_Tick();
        if (PartyMenu_IsOpen()) break;
        bui_restore_battle_palette();

        int slot = PartyMenu_GetSelected();
        if (slot < 0) {

            bui_state = BUI_DRAW_HUD;
            break;
        }

        s_retreat_species = wBattleMon.species;
        s_switch_slot     = (uint8_t)slot;

        s_grow_after_switch = 1;

        bui_clear_rows(0, SCREEN_HEIGHT - 1);
        bui_draw_enemy_hud();
        bui_draw_player_hud();
        bui_load_sprites();

        const char *old_name = Pokemon_GetName(Species_Dex(s_retreat_species));
        bui_retreat_text(old_name, s_msg_buf, sizeof(s_msg_buf));
        bui_show_text_done(s_msg_buf);

        s_retreat_stage = 0;
        s_retreat_frame = 0;
        bui_state = BUI_RETREAT_ANIM;
        break;
    }

    case BUI_RETREAT_ANIM: {

        if (Text_IsOpen()) break;
        s_retreat_frame++;
        uint8_t rdex = gSpeciesToDex[s_retreat_species];

        if (s_retreat_stage == 0 && s_retreat_frame >= 50) {

            bui_clear_rect(1, 5, 7, 11);
            if (rdex > 0 && rdex <= 151)
                for (int dty = 0; dty < 5; dty++)
                    for (int dtx = 0; dtx < 5; dtx++)
                        bui_set_tile(3 + dtx, 7 + dty,
                            (uint8_t)(PLAYER_SPR_BG_BASE + kDownscale5[dty]*7 + kDownscale5[dtx]));
            s_retreat_stage = 1;  s_retreat_frame = 0;
        } else if (s_retreat_stage == 1 && s_retreat_frame >= 4) {

            bui_clear_rect(1, 5, 7, 11);
            if (rdex > 0 && rdex <= 151)
                for (int dty = 0; dty < 3; dty++)
                    for (int dtx = 0; dtx < 3; dtx++)
                        bui_set_tile(4 + dtx, 9 + dty,
                            (uint8_t)(PLAYER_SPR_BG_BASE + kDownscale3[dty]*7 + kDownscale3[dtx]));
            s_retreat_stage = 2;  s_retreat_frame = 0;
        } else if (s_retreat_stage == 2 && s_retreat_frame >= 3) {

            Audio_PlaySFX_BallPoof();
            bui_clear_rect(1, 5, 7, 11);
            s_retreat_stage = 0;  s_retreat_frame = 0;

            Battle_SwitchPlayerMon(s_switch_slot);

            uint8_t p_dex_new = gSpeciesToDex[wBattleMon.species];
            if (bui_has_back_sprite(wBattleMon.species, p_dex_new))
                bui_load_player_back_tiles(wBattleMon.species, p_dex_new);

            const char *new_name = bui_player_mon_name();
            const char *go_prefix;
            if (wEnemyMon.max_hp == 0 || wEnemyMon.hp == 0) {
                go_prefix = "Go";
            } else {
                int pct = (int)wEnemyMon.hp * 100 / (int)wEnemyMon.max_hp;
                if (pct >= 70)      go_prefix = "Go";
                else if (pct >= 40) go_prefix = "Do it";
                else                go_prefix = "Get'm";
            }
            snprintf(s_msg_buf, sizeof(s_msg_buf), "%s! %s!", go_prefix, new_name);

            bui_show_text_done(s_msg_buf);

            s_grow_stage       = 0;
            s_grow_frame       = 0;
            bui_state          = BUI_POKEMON_APPEAR;
        }
        break;
    }

    case BUI_BAG_BATTLE: {
        BagMenu_Tick();
        if (BagMenu_IsOpen()) break;

        bui_clear_rows(0, SCREEN_HEIGHT - 1);
        bui_draw_enemy_hud();

        if (wBattleType != 1) bui_draw_player_hud();
        bui_load_sprites();
        bui_hide_pokeballs();

        uint8_t item = BagMenu_GetSelected();

        if (item == 0) {

            bui_set_enemy_oam_visible(1);
            bui_cursor = s_saved_battle_menu_item;
            bui_draw_main_menu(bui_cursor);
            bui_state = BUI_MENU;
            break;
        }

        if (bui_is_ball(item) &&
            wIsInBattle == 1 &&
            wBattleType != 1 &&
            wPartyCount >= PARTY_LENGTH &&
            bui_current_box_is_full()) {

            bui_show_text("The #MON BOX\nis full! Can't\nuse that item!");
            bui_set_enemy_oam_visible(1);
            bui_cursor = s_saved_battle_menu_item;
            bui_draw_main_menu(bui_cursor);
            bui_state = BUI_MENU;
            break;
        }

        if (bui_is_ball(item)) {

            if (wIsInBattle == 2) {

                Inventory_Remove(item, 1);

                s_ball_item   = item;
                s_throw_frame = 0;
                s_catch_result = CATCH_RESULT_CANNOT_CATCH;
                wPokeBallAnimData = (uint8_t)s_catch_result;
                bui_ball_load_tiles();
                bui_ball_hide();
                bui_state = BUI_BALL_THROW;
                break;
            }

            Inventory_Remove(item, 1);
            bui_begin_ball_throw(item);
        } else if (item_needs_target(item)) {

            s_pending_item = item;
            bui_state = BUI_ITEM_TARGET;
        } else {

            char pname[NAME_LENGTH + 1];
            char iname[20];
            bui_decode_poke_str(wPlayerName, pname, sizeof(pname));
            Inventory_DecodeASCII(item, iname, sizeof(iname));

            item_use_result_t use_result = Battle_UseItem(item, (uint8_t)wPlayerMonNumber);

            if (item == ITEM_POKE_FLUTE) {

                if (use_result == ITEM_USE_OK) {

                    snprintf(s_msg_buf, sizeof(s_msg_buf),
                        "%s played the\nPOKe FLUTE.", pname);
                    snprintf(s_item_second_text, sizeof(s_item_second_text),
                        "All sleeping\nMonster woke up.");
                    bui_show_text(s_msg_buf);
                    s_flute_music_started = 0;
                    bui_set_enemy_oam_visible(1);
                    bui_state = BUI_ITEM_FLUTE_MUSIC;
                    break;
                }
                snprintf(s_msg_buf, sizeof(s_msg_buf),
                    "Played the POKe\nFLUTE.\fNow, that's a\ncatchy tune!");
                bui_show_text(s_msg_buf);
            } else if (use_result == ITEM_USE_CANNOT_USE) {

                wActionResultOrTookBattleTurn = 0;
                snprintf(s_msg_buf, sizeof(s_msg_buf), "Can't use that\nin battle!");
                bui_show_text(s_msg_buf);
                bui_state = BUI_ITEM_FAIL_TEXT;
                break;
            } else {
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s used\n%s!", pname, iname);
                bui_show_text(s_msg_buf);
                if (use_result == ITEM_USE_OK && !Inventory_IsKeyItem(item))
                    Inventory_Remove(item, 1);
                if (item >= ITEM_X_ATTACK && item <= ITEM_X_SPECIAL) {

                    static const char *const kXStat[4] =
                        { "ATTACK", "DEFENSE", "SPEED", "SPECIAL" };
                    if (use_result == ITEM_USE_OK) {
                        snprintf(s_item_second_text, sizeof(s_item_second_text),
                                 "%s's\n%s rose!", bui_player_mon_name(),
                                 kXStat[item - ITEM_X_ATTACK]);
                        s_item_second_anim = 1;
                    } else {
                        snprintf(s_item_second_text, sizeof(s_item_second_text),
                                 "%s", RomText("_NothingHappenedText"));
                        s_item_second_anim = 0;
                    }
                    wActionResultOrTookBattleTurn = 1;
                    bui_set_enemy_oam_visible(1);
                    bui_state = BUI_ITEM_SECOND_TEXT;
                    break;
                }
            }

            wActionResultOrTookBattleTurn = 1;
            bui_set_enemy_oam_visible(1);
            s_turn_already_used_pending = 1;
            bui_state = BUI_MOVE_SELECT;
        }
        break;
    }

    case BUI_ITEM_TARGET: {
        if (!PartyMenu_IsOpen()) {

            bui_set_enemy_oam_visible(0);
            bui_hide_player_slide_oam();

            PartyMenu_Open(PARTY_MENU_ITEM_USE);
            break;
        }
        PartyMenu_Tick();
        if (PartyMenu_IsOpen()) break;

        int slot = PartyMenu_GetSelected();

        if (slot < 0) {

            bui_restore_battle_palette();
            wActionResultOrTookBattleTurn = 0;

            bui_clear_rows(0, SCREEN_HEIGHT - 1);
            bui_draw_enemy_hud();
            bui_draw_player_hud();
            bui_load_sprites();
            bui_hide_pokeballs();
            bui_set_enemy_oam_visible(0);
            bui_draw_box();
            BagMenu_OpenBattle();
            bui_state = BUI_BAG_BATTLE;
            break;
        }

        item_use_result_t use_result = Battle_UseItem(s_pending_item, (uint8_t)slot);

        if (use_result != ITEM_USE_OK) {

            bui_restore_battle_palette();
            wActionResultOrTookBattleTurn = 0;
            bui_show_text(RomText("ItemUseNoEffectText"));
            bui_state = BUI_ITEM_FAIL_TEXT;
            break;
        }

        if (!Inventory_IsKeyItem(s_pending_item))
            Inventory_Remove(s_pending_item, 1);

        uint8_t msg = Battle_GetMedicineMsg();
        const char *nick = bui_party_slot_name(slot);

        uint16_t old_hp = Battle_GetMedicineOldHP();
        uint16_t new_hp = Battle_GetMedicineNewHP();
        uint16_t healed = (new_hp > old_hp) ? (uint16_t)(new_hp - old_hp) : 0;
        if ((msg == MEDICINE_MSG_POTION || msg == MEDICINE_MSG_REVIVE) && healed > 0) {
            char line1[24], line2[24];
            snprintf(line1, sizeof(line1), "%s", nick);
            if (msg == MEDICINE_MSG_REVIVE)
                snprintf(line2, sizeof(line2), "is revitalized!");
            else
                snprintf(line2, sizeof(line2), "recovered by %u!", (unsigned)healed);
            Audio_PlaySFX_HealHP();
            PartyMenu_SetHealResultMessage(line1, line2);
            PartyMenu_AnimateItemHeal(slot, old_hp, new_hp, healed);
            bui_state = BUI_ITEM_HEAL_ANIM;
            break;
        }

        bui_restore_battle_palette();
        if (msg == MEDICINE_MSG_POTION || msg == MEDICINE_MSG_REVIVE) {

            Audio_PlaySFX_HealHP();
            if (msg == MEDICINE_MSG_REVIVE)
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s\nis revitalized!", nick);
            else
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s\nrecovered by %u!",
                         nick, (unsigned)healed);
        } else {

            Audio_PlaySFX_HealAilment();
            switch (msg) {
            case MEDICINE_MSG_ANTIDOTE:
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s was\ncured of poison!", nick);
                break;
            case MEDICINE_MSG_BURN_HEAL:
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s's\nburn was healed!", nick);
                break;
            case MEDICINE_MSG_ICE_HEAL:
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s was\ndefrosted!", nick);
                break;
            case MEDICINE_MSG_AWAKENING:
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s\nwoke up!", nick);
                break;
            case MEDICINE_MSG_PARLYZ_HEAL:
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s's\nrid of paralysis!", nick);
                break;
            default:
                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s's\nhealth returned!", nick);
                break;
            }
        }
        bui_show_text(s_msg_buf);
        bui_state = BUI_ITEM_RESULT_TEXT;
        break;
    }

    case BUI_ITEM_HEAL_ANIM:

        if (PartyMenu_IsOpen()) {
            PartyMenu_Tick();
            if (PartyMenu_IsOpen()) break;
        }
        bui_restore_battle_palette();
        bui_clear_rows(0, SCREEN_HEIGHT - 1);
        bui_draw_enemy_hud();
        bui_draw_player_hud();
        bui_load_sprites();
        bui_hide_pokeballs();
        bui_draw_box();
        bui_set_enemy_oam_visible(1);
        wActionResultOrTookBattleTurn = 1;
        s_turn_already_used_pending = 1;
        bui_state = BUI_MOVE_SELECT;
        break;

    case BUI_ITEM_RESULT_TEXT:

        if (Text_IsOpen()) break;
        bui_clear_rows(0, SCREEN_HEIGHT - 1);
        bui_draw_enemy_hud();
        bui_draw_player_hud();
        bui_load_sprites();
        bui_hide_pokeballs();
        bui_draw_box();
        bui_set_enemy_oam_visible(1);
        wActionResultOrTookBattleTurn = 1;
        s_turn_already_used_pending = 1;
        bui_state = BUI_MOVE_SELECT;
        break;

    case BUI_ITEM_SECOND_TEXT:

        if (Text_IsOpen()) break;
        if (s_item_second_anim) {
            s_item_second_anim = 0;
            if (gMoveAnimTraceHook)
                gMoveAnimTraceHook(174 , 0);
        }
        bui_show_text(s_item_second_text);
        wActionResultOrTookBattleTurn = 1;
        s_turn_already_used_pending = 1;
        bui_state = BUI_MOVE_SELECT;
        break;

    case BUI_ITEM_FLUTE_MUSIC:

        if (Text_IsOpen()) break;
        if (!s_flute_music_started) {
            s_flute_music_started = 1;

            if (Audio_IsLowHealthAlarmOn()) {
                s_flute_music_started = 2;
            } else if (Audio_IsSFXPlaying() || Audio_IsCryPlaying()) {

                s_flute_music_started = 0;
                break;
            } else {
                Audio_PlayPokeFluteInBattle();
            }
            break;
        }

        if (s_flute_music_started == 1 && Audio_IsPokeFluteInBattlePlaying())
            break;
        bui_show_text(s_item_second_text);
        wActionResultOrTookBattleTurn = 1;
        s_turn_already_used_pending = 1;
        bui_state = BUI_MOVE_SELECT;
        break;

    case BUI_AI_ACTION:
        if (!s_ai_action_shown) {
            s_ai_action_shown = 1;

            wDoNotWaitForButtonPress = 0;
            bui_show_text(s_msg_buf);
            switch (gLastAIAction.kind) {
                case AI_ACT_ITEM_HEAL:  Audio_PlaySFX_HealHP();      break;
                case AI_ACT_FULL_HEAL:  Audio_PlaySFX_HealAilment(); break;
                case AI_ACT_X_STAT:
                case AI_ACT_GUARD_SPEC: Audio_PlaySFX_HealAilment(); break;
                default: break;
            }
            break;
        }
        if (Text_IsOpen()) break;
        if (gLastAIAction.kind == AI_ACT_ITEM_HEAL &&
            gLastAIAction.hp_after != gLastAIAction.hp_before) {

            s_ai_pre_hp = gLastAIAction.hp_before;

            s_hp_anim_who    = 0;
            s_hp_pre_hp      = s_ai_pre_hp;
            s_hp_pre_max     = wEnemyMon.max_hp;
            s_hp_cur_hp      = s_ai_pre_hp;
            s_hp_new_hp      = (int)wEnemyMon.hp;
            s_hp_old_px      = calc_hp_pixels(s_ai_pre_hp, (int)wEnemyMon.max_hp);
            s_hp_cur_px      = s_hp_old_px;
            s_hp_new_px      = calc_hp_pixels(wEnemyMon.hp, wEnemyMon.max_hp);
            s_hp_half_frame  = 0;
            s_hp_bar_pending = 0;
            s_hp_delay       = 0;
            s_hp_anim_deferred  = 0;
            s_hp_stage2_pending = 0;

            s_enemy_hp_bar_draw = -1;
            bui_state = BUI_AI_HP_SCROLL;
            break;
        }

        s_enemy_hp_bar_draw = -1;
        printf("[SWDBG] ai_action_done kind=%d rejoin=%d\n",
               (int)gLastAIAction.kind, (int)s_ai_rejoin_state);
        fflush(stdout);
        if (gLastAIAction.kind == AI_ACT_SWITCH) {

            s_slide_cx = 0;
            s_slide_is_ai_switch = 1;
            printf("[SWDBG] -> ENEMY_SLIDE_OUT (enemy oam y0=%u x0=%u)\n",
                   (unsigned)wShadowOAM[ENEMY_SPR_OAM_BASE].y,
                   (unsigned)wShadowOAM[ENEMY_SPR_OAM_BASE].x);
            fflush(stdout);
            bui_state = BUI_ENEMY_SLIDE_OUT;
            break;
        }

        bui_draw_enemy_hud();
        bui_state = s_ai_rejoin_state;
        break;

    case BUI_AI_HP_SCROLL:
        if (bui_hp_anim_step_scaled()) {
            s_enemy_hp_bar_draw = -1;
            bui_draw_enemy_hud();
            bui_state = s_ai_rejoin_state;
        }
        break;

    case BUI_AI_SWITCH_SEND:
        if (Text_IsOpen()) break;

        s_mid_battle_send  = 1;
        s_shift_pending    = 0;
        s_grow_stage       = 0;
        s_grow_frame       = 0;
        s_ai_switch_active = 1;
        s_ai_switch_rejoin = s_ai_rejoin_state;
        bui_state = BUI_ENEMY_SEND_OUT;
        break;

    case BUI_ITEM_FAIL_TEXT:

        if (Text_IsOpen()) break;
        bui_clear_rows(0, SCREEN_HEIGHT - 1);
        bui_draw_enemy_hud();
        bui_draw_player_hud();
        bui_load_sprites();
        bui_hide_pokeballs();
        bui_set_enemy_oam_visible(0);
        bui_draw_box();
        BagMenu_OpenBattle();
        bui_state = BUI_BAG_BATTLE;
        break;

    case BUI_SAFARI_ITEM_ANIM: {
        if (!s_safari_item_anim_started) {
            if (Text_IsOpen()) break;

            bui_draw_box();

            bui_load_sprites();
            memset(&s_move_anim_ctx, 0, sizeof(s_move_anim_ctx));
            s_move_anim_ctx.animation_id = s_safari_item_anim_id;
            hWhoseTurn = 0u;
            MoveAnim_Begin(&s_move_anim_ctx);
            s_move_anim_active = !MoveAnim_IsDone(&s_move_anim_ctx);
            s_safari_item_anim_started = 1;
            break;
        }
        if (s_move_anim_active) {
            if (bui_move_anim_tick()) s_move_anim_active = 0;
            bui_draw_box();
            break;
        }

        bui_load_sprites();
        bui_draw_box();
        s_safari_item_anim_started = 0;
        bui_state = BUI_SAFARI_RESOLVE;
        break;
    }

    case BUI_SAFARI_RESOLVE: {
        const char *e_name = bui_enemy_mon_name();
        if (s_safari_resolve_phase == 0) {
            if (wSafariBaitFactor > 0) {
                wSafariBaitFactor--;
                RomTextSplice(s_msg_buf, sizeof(s_msg_buf), "SafariZoneEatingText",
                              "{ram:CFDA}", e_name);
                bui_show_text(s_msg_buf);
                s_safari_resolve_phase = 1;
                break;
            }
            if (wSafariEscapeFactor > 0) {
                wSafariEscapeFactor--;
                if (wSafariEscapeFactor == 0)
                    wEnemyMonActualCatchRate = wEnemyMon.catch_rate;
                RomTextSplice(s_msg_buf, sizeof(s_msg_buf), "SafariZoneAngryText",
                              "{ram:CFDA}", e_name);
                bui_show_text(s_msg_buf);
                s_safari_resolve_phase = 1;
                break;
            }
            s_safari_resolve_phase = 1;
        }

        if (bui_safari_should_flee()) {
            Audio_PlaySFX_Run();

            RomTextSplice(s_msg_buf, sizeof(s_msg_buf), "WildRanText",
                          "{ram:CFDA}", bui_enemy_mon_name());
            bui_show_text(s_msg_buf);
            wEscapedFromBattle = 1;
            bui_state = BUI_END;
            break;
        }

        s_safari_resolve_phase = 0;
        bui_cursor = s_saved_battle_menu_item;
        bui_hide_player_slide_oam();
        bui_load_sprites();
        bui_draw_main_menu(bui_cursor);
        bui_state = BUI_MENU;
        break;
    }

    case BUI_BALL_THROW: {

        static const uint8_t kArcY[11] = {88,76,64,56,48,40,32,30,32,41,50};
        static const uint8_t kArcX[11] = {40,48,56,64,72,80,88,96,104,112,120};

        if (s_throw_frame == 0) {
            Audio_PlaySFX_BallToss();

            if (gMoveAnimTraceHook)
                gMoveAnimTraceHook(193 , 0);
        }

        int waypoint = s_throw_frame / THROW_FPW;

        if (waypoint < 11) {
            bui_ball_set_oam(kArcY[waypoint], kArcX[waypoint], 0);
            s_throw_frame++;
            break;
        }

        if (wIsInBattle == 2) {
            bui_ball_hide();
            bui_set_enemy_oam_visible(1);
            bui_show_text("The trainer\nblocked the BALL!\n\nDon't be a thief!");
            bui_state = BUI_SWITCH_ENEMY_TURN;
            break;
        }

        if (s_catch_result == CATCH_RESULT_CANNOT_CATCH) {
            bui_ball_hide();
            bui_set_enemy_oam_visible(1);
            bui_ball_fail_text_and_advance();
            break;
        }

        switch (s_catch_result) {
        case CATCH_RESULT_SUCCESS:  s_shake_total = 3; break;
        case CATCH_RESULT_3_SHAKES: s_shake_total = 3; break;
        case CATCH_RESULT_2_SHAKES: s_shake_total = 2; break;
        case CATCH_RESULT_1_SHAKE:  s_shake_total = 1; break;
        default:                    s_shake_total = 0; break;
        }
        s_shake_frame = 0;
        s_poof_frame  = 0;
        s_poof_phase  = 0;

        for (int i = 0; i < POOF_OAM_COUNT; i++)
            wShadowOAM[POOF_OAM_BASE + i].y = 0;

        bui_state = BUI_BALL_POOF;
        break;
    }

    case BUI_BALL_POOF: {

        if (s_poof_frame == 0) {

            if (s_poof_phase == 0 && s_catch_result != CATCH_RESULT_0_SHAKES)
                bui_set_enemy_oam_visible(0);
            bui_ball_hide();
        }

        if (s_poof_frame == 4)
            Audio_PlaySFX_BallPoof();

        int entry    = s_poof_frame / 4;
        int subframe = s_poof_frame % 4;

        if (subframe == 0) {

            for (int i = 0; i < POOF_OAM_COUNT; i++)
                wShadowOAM[POOF_OAM_BASE + i].y = 0;
            bui_draw_poof_frame(entry);
        }

        s_poof_frame++;
        if (s_poof_frame >= 24) {

            for (int i = 0; i < POOF_OAM_COUNT; i++)
                wShadowOAM[POOF_OAM_BASE + i].y = 0;

            if (s_poof_phase == 1) {
                bui_set_enemy_oam_visible(1);
                bui_ball_hide();
                bui_ball_fail_text_and_advance();
                break;
            }

            if (s_catch_result == CATCH_RESULT_0_SHAKES) {
                bui_set_enemy_oam_visible(1);
                bui_ball_hide();
                bui_ball_fail_text_and_advance();
                break;
            }

            bui_ball_set_oam(BALL_SHAKE_OAM_Y, BALL_SHAKE_OAM_X, 0);
            bui_state = BUI_BALL_SHAKE;
        }
        break;
    }

    case BUI_BALL_SHAKE: {
        if (s_shake_total == 0 || s_shake_frame >= s_shake_total * SHAKE_CYCLE) {
            if (s_catch_result == CATCH_RESULT_SUCCESS) {

                const char *ename = bui_enemy_mon_name();

                bui_ball_set_oam(BALL_SHAKE_OAM_Y, BALL_SHAKE_OAM_X, 2);
                Audio_PlaySFX_CaughtMon();

                snprintf(s_msg_buf, sizeof(s_msg_buf), "All right!\n%s was%ccaught!",
                         ename, TEXT_ASCII_CONT);

                if (wBattleType == 1) wDoNotWaitForButtonPress = 1;
                bui_show_text(s_msg_buf);
                bui_state = BUI_CAUGHT;
            } else {

                if (s_shake_total > 0) {
                    s_poof_phase = 1;
                    s_poof_frame = 0;
                    bui_state = BUI_BALL_POOF;
                } else {
                    bui_ball_hide();
                    bui_set_enemy_oam_visible(1);
                    bui_ball_fail_text_and_advance();
                }
            }
            break;
        }

        int phase = s_shake_frame % SHAKE_CYCLE;

        if (phase == 0)
            Audio_PlaySFX_Tink();

        if (phase < SHAKE_DELAY) {

            bui_ball_set_oam(BALL_SHAKE_OAM_Y, BALL_SHAKE_OAM_X, 0);
        } else {

            int vp = phase - SHAKE_DELAY;
            int fb = (vp < 4) ? 0 : (vp < 8) ? 1 : (vp < 12) ? 0 : 2;
            bui_ball_set_oam(BALL_SHAKE_OAM_Y, BALL_SHAKE_OAM_X, fb);
        }

        s_shake_frame++;
        break;
    }

    case BUI_CAUGHT: {
        const char *ename;

        if (wBattleType == 1) {

            if (Text_IsOpen()) break;
            wBattleResult = BATTLE_OUTCOME_CAUGHT;
            bui_state = BUI_END;
            break;
        }

        wCapturedMonSpecies = wEnemyMon.species;
        s_caught_species = wEnemyMon.species;
        s_caught_dex = gSpeciesToDex[s_caught_species];
        s_caught_new_entry = !bui_pokedex_owned_num(s_caught_dex);
        s_caught_sent_to_box = 0;
        s_caught_dex_started = 0;
        s_caught_party_slot = -1;
        s_caught_box_slot = -1;
        s_caught_box_index = -1;
        ename = Pokemon_GetName(s_caught_dex);

        Pokedex_SetOwned(s_caught_species);

        if (wPartyCount < PARTY_LENGTH) {

            uint8_t party_slot = wPartyCount;
            party_mon_t *p = &wPartyMons[party_slot];
            memset(p, 0, sizeof(*p));
            p->base.species    = s_caught_species;
            p->base.hp         = wEnemyMon.hp;
            p->base.box_level  = wEnemyMon.level;
            p->base.status     = wEnemyMon.status;
            p->base.type1      = wEnemyMon.type1;
            p->base.type2      = wEnemyMon.type2;
            p->base.catch_rate = wEnemyMon.catch_rate;
            memcpy(p->base.moves, wEnemyMon.moves, 4);
            memcpy(p->base.pp,    wEnemyMon.pp,    4);
            p->base.dvs        = wEnemyMon.dvs;
            p->base.ot_id      = wPlayerID;

            uint8_t caught_dex = gSpeciesToDex[s_caught_species];
            uint8_t growth = (caught_dex > 0 && caught_dex <= NUM_POKEMON)
                           ? gBaseStats[caught_dex].growth_rate
                           : GROWTH_MEDIUM_FAST;
            uint32_t xp = CalcExpForLevel(growth, wEnemyMon.level);
            p->base.exp[0] = (uint8_t)((xp >> 16) & 0xFF);
            p->base.exp[1] = (uint8_t)((xp >>  8) & 0xFF);
            p->base.exp[2] = (uint8_t)( xp         & 0xFF);
            p->level   = wEnemyMon.level;
            p->max_hp  = wEnemyMon.max_hp;
            p->atk     = wEnemyMon.atk;
            p->def     = wEnemyMon.def;
            p->spd     = wEnemyMon.spd;
            p->spc     = wEnemyMon.spc;
            memcpy(wPartyMonOT[party_slot], wPlayerName, NAME_LENGTH);
            Pokemon_EncodeNameString(ename ? ename : "", wPartyMonNicks[party_slot]);

            wPartySpecies[party_slot]     = s_caught_species;
            wPartySpecies[party_slot + 1] = 0xFF;
            wPartyCount++;
            s_caught_party_slot = party_slot;
        } else {
            s_caught_sent_to_box = 1;
            if (!Pokemon_SendBattleMonToBox(&wEnemyMon)) {

                bui_state = BUI_END;
                break;
            }
            s_caught_box_index = (int)(wCurrentBoxNum % NUM_BOXES);
            s_caught_box_slot = (int)wBoxCount[s_caught_box_index] - 1;
        }

        wBattleResult = BATTLE_OUTCOME_CAUGHT;
        SessionLog_CapturedMon(s_caught_species, wEnemyMon.level,
                               s_caught_sent_to_box, s_caught_new_entry);
        if (s_caught_new_entry) {
            snprintf(s_msg_buf, sizeof(s_msg_buf),
                     "New #DEX data\nwill be added\nfor %s!", ename);
            bui_show_text(s_msg_buf);
            bui_state = BUI_CAUGHT_DEX_WAIT;
        } else {

            bui_state = BUI_CAUGHT_NICK_PROMPT;
        }
        break;
    }

    case BUI_CAUGHT_DEX_WAIT:
        if (!s_caught_dex_started) {
            Pokedex_ShowData(s_caught_dex);
            s_caught_dex_started = 1;
            break;
        }
        if (Pokedex_IsShowingData()) {
            Pokedex_ShowDataTick();
            break;
        }

        if (Text_IsOpen())
            Text_Close();
        hWY = 144;
        bui_restore_catch_screen_after_dex();
        bui_state = BUI_CAUGHT_NICK_PROMPT;
        break;

    case BUI_CAUGHT_BOX_TEXT:

        if (Text_IsOpen()) break;
        bui_state = BUI_FADE_WHITE;
        break;

    case BUI_CAUGHT_NICK_PROMPT: {
        const char *ename = Pokemon_GetName(s_caught_dex);
        snprintf(s_msg_buf, sizeof(s_msg_buf),
                 "Do you want to\ngive a nickname\nto %s?", ename ? ename : "");
        YesNo_Show(s_msg_buf);
        bui_state = BUI_CAUGHT_NICK_QUERY;
        break;
    }

    case BUI_CAUGHT_NICK_QUERY:
        YesNo_Tick();
        if (YesNo_IsOpen()) break;
        if (YesNo_GetResult()) bui_state = BUI_CAUGHT_NICKNAME;
        else                   bui_caught_after_naming();
        break;

    case BUI_CAUGHT_NICKNAME:
        if (NamingScreen_IsOpen()) {
            bui_state = BUI_CAUGHT_NICK_WAIT;
            break;
        }
        if (s_caught_party_slot >= 0 && s_caught_party_slot < PARTY_LENGTH) {
            NamingScreen_Open(NAME_MON_SCREEN, s_caught_species, wPartyMonNicks[s_caught_party_slot]);
            bui_state = BUI_CAUGHT_NICK_WAIT;
            break;
        }
        if (s_caught_sent_to_box &&
            s_caught_box_index >= 0 && s_caught_box_index < NUM_BOXES &&
            s_caught_box_slot >= 0 && s_caught_box_slot < BOX_CAPACITY) {
            NamingScreen_Open(NAME_MON_SCREEN, s_caught_species, wBoxMonNicks[s_caught_box_index][s_caught_box_slot]);
            bui_state = BUI_CAUGHT_NICK_WAIT;
            break;
        }
        bui_caught_after_naming();
        break;

    case BUI_CAUGHT_NICK_WAIT:
        if (NamingScreen_IsOpen()) break;
        bui_caught_after_naming();
        break;

    case BUI_SWITCH_ENEMY_TURN: {
        const char *wild_pfx = "Enemy ";

        snprintf(s_name_a, sizeof(s_name_a), "%s",
                 bui_player_mon_name());
        s_pfx_a[0] = '\0';
        snprintf(s_name_b, sizeof(s_name_b), "%s",
                 bui_enemy_mon_name());
        snprintf(s_pfx_b, sizeof(s_pfx_b), "%s", wild_pfx);
        s_player_first = 1;

        hWhoseTurn = 1;
        bui_snapshot_pre(1);
        bui_snapshot_hp_pre();

        snprintf(s_ai_withdraw_name, sizeof(s_ai_withdraw_name), "%s", bui_enemy_mon_name());
        if (AI_TrainerAI()) { bui_begin_ai_action(BUI_TURN_END); break; }
        Battle_ExecuteEnemyMove();
        bui_note_target_hud_redraw_if_status_changed(1);
        bui_run_move_anim_runtime(1);
        bui_setup_anim(1);
        bui_setup_hp_anim(1);
        s_anim_first = 0;

        Text_KeepTilesOnClose();
        bui_show_after_move(1, s_pfx_b, s_name_b, wEnemySelectedMove, wEnemyMoveNum,
                            Battle_GetLastCrit(), wMoveMissed, wDamageMultipliers & 0x7F);
        bui_state = BUI_MOVE_ANIM;
        break;
    }

    case BUI_TRAINER_VICTORY_SLIDE: {
        if (s_victory_timer == 0) {

            if (s_rival1_loss) {
                bui_clear_rows(0, 8);
                bui_clear_rect(0, 8, 0, 8);
            } else {
                bui_clear_rows(0, 4);
            }
            if (s_rival1_loss) {

                bui_clear_rect(1, 5, 7, 11);
                bui_clear_rect(9, 7, 19, 11);
            } else {
                bui_place_player_sprite();
            }

            int tc = (int)gEngagedTrainerClass - 1;
            if (tc >= 0 && tc < NUM_TRAINERS) {
                for (int i = 0; i < TRAINER_CANVAS_TILES; i++)
                    Display_LoadSpriteTile((uint8_t)(ENEMY_SPR_TILE_BASE + i),
                                           gTrainerFrontSprite[tc][i]);

                s_enemy_pic_kind = BUI_ENEMY_PIC_TRAINER;
            }
            for (int ty = 0; ty < 7; ty++)
                for (int tx = 0; tx < 7; tx++) {
                    int idx = ENEMY_SPR_OAM_BASE + ty * 7 + tx;
                    wShadowOAM[idx].y     = (uint8_t)(ENEMY_SPR_PX_Y + ty * 8 + OAM_Y_OFS);
                    wShadowOAM[idx].x     = (uint8_t)(VICTORY_TRAINER_PX_X + tx * 8 + OAM_X_OFS + s_slide_cx);
                    wShadowOAM[idx].tile  = (uint8_t)(ENEMY_SPR_TILE_BASE + ty * 7 + tx);
                    wShadowOAM[idx].flags = 0;
                }
            s_victory_timer = 1;
            break;
        }
        if (s_slide_cx > 0) {
            s_slide_cx -= 2;
            if (s_slide_cx < 0) s_slide_cx = 0;
        }
        for (int ty = 0; ty < 7; ty++)
            for (int tx = 0; tx < 7; tx++) {
                int idx = ENEMY_SPR_OAM_BASE + ty * 7 + tx;
                wShadowOAM[idx].x = (uint8_t)(VICTORY_TRAINER_PX_X + tx * 8 + OAM_X_OFS + s_slide_cx);
            }
        if (s_slide_cx == 0) {
            s_victory_timer = 40;
            bui_state = BUI_TRAINER_VICTORY_PAUSE;
        }
        break;
    }

    case BUI_TRAINER_VICTORY_PAUSE: {
        if (--s_victory_timer <= 0) {
            if (s_rival1_loss) {

                bui_show_text(RomText("_Rival1WinText"));
                bui_state = BUI_RIVAL1_LOSS_TEXT;
                break;
            }
            if (gTrainerAfterText && gTrainerAfterText[0]) {
                bui_show_text(gTrainerAfterText);
                bui_state = BUI_TRAINER_VICTORY_TEXT;
            } else {

                if (s_trainer_money_text[0])
                    bui_show_text(s_trainer_money_text);
                s_victory_timer = 0;
                bui_state = BUI_FADE_WHITE;
            }
        }
        break;
    }

    case BUI_RIVAL1_LOSS_TEXT:
        if (Text_IsOpen()) break;
        s_rival1_loss = 0;
        if (gBattleNoBlackoutOnLoss) {
            Battle_HandlePlayerLossNoBlackOut();
            bui_state = BUI_END;
            break;
        }

        Battle_HandlePlayerBlackOut();
        bui_show_text(RomText("_PlayerBlackedOutText2"));
        s_bo_fade_step = -1;
        bui_state = BUI_BLACKOUT_FADE;
        break;

    case BUI_TRAINER_VICTORY_TEXT:

        if (Text_IsOpen()) break;

        bui_draw_box();
        if (s_badge_recv_text) {
            Audio_PlaySFX_GetKeyItem();
            bui_show_text(s_badge_recv_text);
            s_badge_recv_text = NULL;
            bui_state = BUI_GYM_BADGE_JINGLE;
        } else {
            bui_state = BUI_WAIT_SOUND;
        }
        break;

    case BUI_GYM_BADGE_JINGLE:

        if (Text_IsOpen()) break;
        if (s_badge_info_text) {
            bui_show_text(s_badge_info_text);
            s_badge_info_text = NULL;
        }
        bui_state = BUI_WAIT_SOUND;
        break;

    case BUI_WAIT_CRY:
        if (!Audio_IsCryPlaying()) {

            if (s_wait_cry_delay > 0) { s_wait_cry_delay--; break; }
            if (s_wait_cry_text[0]) {

                snprintf(s_msg_buf, sizeof(s_msg_buf), "%s", s_wait_cry_text);
                if (s_wait_cry_text_keep) {

                    wDoNotWaitForButtonPress = 1;
                    Text_KeepTilesOnClose();
                    s_wait_cry_text_keep = 0;
                }
                bui_show_text(s_msg_buf);
                s_wait_cry_text[0] = '\0';
            }
            bui_state = s_wait_cry_next_state;
        }
        break;

    case BUI_WAIT_SOUND:

        if (Text_IsOpen()) break;
        if (!Audio_IsSFXPlaying()) {
            if (s_trainer_money_text[0])
                bui_show_text(s_trainer_money_text);
            s_victory_timer = 0;

            {
                uint8_t evo_slot, evo_new;
                if (Battle_CheckNextEvolution(&evo_slot, &evo_new))
                    bui_state = BUI_END;
                else
                    bui_state = BUI_FADE_WHITE;
            }
        }
        break;

    case BUI_FADE_WHITE: {

        if (s_victory_timer == 0)
            s_victory_timer = 10;
        if (--s_victory_timer <= 0) {

            Display_SetPalette(0x00, 0x00, 0x00);
            bui_clear_rows(0, SCREEN_HEIGHT - 1);
            s_evo_screen_white = 1;
            bui_state = BUI_END;
        }
        break;
    }

    case BUI_EVOLUTION: {

#define EVO_SPR_COL 7
#define EVO_SPR_ROW 2
#define EVO_PIC_TILE_BASE PLAYER_SPR_BG_BASE
#define EVO_LOAD_SPRITE(species) do { \
    uint8_t _dex = gSpeciesToDex[(species)]; \
    if (gEvoTraceHook) gEvoTraceHook(s_evo_phase, (species), \
                                     (_dex > 0 && _dex <= 151) ? _dex : 0xFF); \
    if (_dex > 0 && _dex <= 151) { \
        for (int _ty = 0; _ty < 7; _ty++) \
            for (int _tx = 0; _tx < 7; _tx++) { \
                int _src = _ty * 7 + (6 - _tx); \
                uint8_t _tile[16]; \
                for (int _row = 0; _row < 8; _row++) { \
                    _tile[_row * 2 + 0] = bui_reverse_bits(gPokemonFrontSprite[_dex][_src][_row * 2 + 0]); \
                    _tile[_row * 2 + 1] = bui_reverse_bits(gPokemonFrontSprite[_dex][_src][_row * 2 + 1]); \
                } \
                uint8_t _tid = (uint8_t)(EVO_PIC_TILE_BASE + _ty * 7 + _tx); \
                Display_LoadTile(_tid, _tile); \
                bui_set_tile(EVO_SPR_COL + _tx, EVO_SPR_ROW + _ty, _tid); \
            } \
    } \
} while(0)

        if (gEvoTraceHook) gEvoTraceHook(-1 - s_evo_phase, s_evo_old_species, s_evo_new_species);

        switch (s_evo_phase) {

        case 0: {

            wDoNotWaitForButtonPress = 1;
            Text_InstantNext();

            bui_clear_rows(12, SCREEN_HEIGHT - 1);

            RomTextSplice(s_msg_buf, sizeof(s_msg_buf), "IsEvolvingText",
                          "{name}",
                          Pokemon_GetName(Species_Dex(s_evo_old_species)));
            bui_show_text(s_msg_buf);

            s_evo_timer = 50;
            s_evo_phase = 1;
            break;
        }

        case 1:
            if (--s_evo_timer > 0) break;

            bui_clear_rows(0, 11);

            {
                static const uint8_t kWhiteTile[16] = {0};
                Display_LoadTile(0xFF, kWhiteTile);
                bui_fill_rows_tile_both(0, 11, 0xFF);
            }
            for (int i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
            Display_SetPalette(0xE4, 0xD0, 0xE0);

            Music_Stop();
            Audio_PlaySFX_Tink();

            s_evo_timer = 60;
            s_evo_phase = 2;
            break;

        case 2:
            if (--s_evo_timer > 0) break;
            EVO_LOAD_SPRITE(s_evo_old_species);

            GbcColor_SetPalPokemonWholeScreen(gSpeciesToDex[s_evo_old_species]);
            Audio_PlayCry(s_evo_old_species);
            s_evo_timer = 120;
            s_evo_phase = 3;
            break;

        case 3:

            if (Audio_IsSFXPlaying() && --s_evo_timer > 0) break;
            Music_Play(MUSIC_SAFARI_ZONE);
            s_evo_timer = 80;
            s_evo_phase = 4;
            break;

        case 4:

            if (--s_evo_timer > 0) break;

            s_evo_blink = 0;
            s_evo_wave  = 1;
            s_evo_wave_units = 0;
            s_evo_timer = 16;

            GbcColor_SetPalPokemonWholeScreen(0);
            s_evo_phase = 5;
            break;

        case 5: {

            if (s_evo_wave_units == 0) {

                if (hJoyPressed & PAD_B) {
                    s_evo_cancelled = 1;
                    s_evo_phase     = 6;
                    break;
                }
                if (--s_evo_timer > 0) break;
                s_evo_wave_units = s_evo_wave * 2;
                s_evo_timer = 3;
                s_evo_blink = 0;
                break;
            }

            if (--s_evo_timer > 0) break;
            s_evo_timer = 3;
            s_evo_blink ^= 1;
            EVO_LOAD_SPRITE(s_evo_blink ? s_evo_new_species : s_evo_old_species);
            if (--s_evo_wave_units <= 0) {
                s_evo_wave++;
                if (s_evo_wave > 8) {
                    s_evo_phase = 6;
                    break;
                }
                s_evo_timer = 18 - (2 * s_evo_wave);
                if (s_evo_timer < 1) s_evo_timer = 1;
                s_evo_wave_units = 0;
            }
            break;
        }

        case 6: {

            uint8_t final_species = s_evo_cancelled ? s_evo_old_species : s_evo_new_species;
            EVO_LOAD_SPRITE(final_species);

            GbcColor_SetPalPokemonWholeScreen(gSpeciesToDex[final_species]);
            Music_Stop();
            Audio_PlayCry(final_species);

            Display_SetPalette(0xE4, 0xD0, 0xE0);
            if (!s_evo_cancelled) {
                snprintf(s_msg_buf, sizeof(s_msg_buf),
                         "%s evolved\ninto %s!",
                         Pokemon_GetName(Species_Dex(s_evo_old_species)),
                         Pokemon_GetName(Species_Dex(s_evo_new_species)));

                wDoNotWaitForButtonPress = 1;
            } else {
                snprintf(s_msg_buf, sizeof(s_msg_buf),
                         "%s\nstopped evolving.",
                         Pokemon_GetName(Species_Dex(s_evo_old_species)));
            }
            bui_show_text(s_msg_buf);
            s_evo_phase = 7;
            break;
        }

        case 7: {

            if (!s_evo_cancelled) {
                Battle_ApplyEvolution(s_evo_slot, s_evo_new_species);
                s_evo_phase = 8;
            } else {
                Battle_CancelEvolution(s_evo_slot);
                s_evo_phase = 10;
            }
            break;
        }

        case 8:

            if (Audio_StillSounding()) break;
            Audio_PlaySFX_GetItem2();
            s_evo_timer = 40;
            s_evo_phase = 9;
            break;

        case 9:

            if (Audio_StillSounding()) break;
            if (--s_evo_timer > 0) break;
            s_evo_phase = 10;
            break;

        case 10: {

            Text_Close();

            uint8_t next_slot, next_species;
            if (Battle_CheckNextEvolution(&next_slot, &next_species)) {
                s_evo_slot         = next_slot;
                s_evo_old_species  = wPartyMons[next_slot].base.species;
                s_evo_new_species  = next_species;
                s_evo_phase        = 0;
                s_evo_cancelled    = 0;
                s_evo_timer        = 0;
                s_evo_screen_white = 0;
            } else {

                MapMusic_FadeToForMap(wCurMap);
                bui_state = BUI_INACTIVE;
            }
            break;
        }

        }
#undef EVO_LOAD_SPRITE
        break;
    }

    case BUI_BLACKOUT_FADE: {
        if (Text_IsOpen()) return;
        if (s_bo_fade_step < 0) {
            s_bo_fade_step  = 0;
            s_bo_fade_timer = 8;
            Display_SetPalette(kBuiBlackoutFadeOut[0][0], kBuiBlackoutFadeOut[0][1], kBuiBlackoutFadeOut[0][2]);
            return;
        }
        if (--s_bo_fade_timer > 0) return;
        s_bo_fade_step++;
        if (s_bo_fade_step > 3) {
            bui_state = BUI_END;
            return;
        }
        Display_SetPalette(kBuiBlackoutFadeOut[s_bo_fade_step][0],
                            kBuiBlackoutFadeOut[s_bo_fade_step][1],
                            kBuiBlackoutFadeOut[s_bo_fade_step][2]);
        s_bo_fade_timer = 8;
        return;
    }

    case BUI_END: {
        Display_SetShakeOffset(0, 0);
        YesNo_Reset();

        Audio_ResetLowHealthAlarm();

        wPartyMons[wPlayerMonNumber].base.hp     = wBattleMon.hp;
        wPartyMons[wPlayerMonNumber].base.status = wBattleMon.status;
        wIsInBattle = 0;

        int bui_end_was_win = (wBattleResult == BATTLE_OUTCOME_WILD_VICTORY ||
                                wBattleResult == BATTLE_OUTCOME_TRAINER_VICTORY);

        wBattleResult        = 0;
        wEscapedFromBattle   = 0;
        wPlayerSelectedMove  = 0;
        wEnemySelectedMove   = 0;

        wPlayerBattleStatus1 = wPlayerBattleStatus2 = wPlayerBattleStatus3 = 0;
        wEnemyBattleStatus1  = wEnemyBattleStatus2  = wEnemyBattleStatus3  = 0;
        wPlayerConfusedCounter = wEnemyConfusedCounter = 0;
        wPlayerToxicCounter    = wEnemyToxicCounter    = 0;
        wPlayerDisabledMove    = wEnemyDisabledMove    = 0;
        wPlayerDisabledMoveNumber = wEnemyDisabledMoveNumber = 0;
        wPlayerNumAttacksLeft  = wEnemyNumAttacksLeft  = 0;
        wPlayerNumHits         = wEnemyNumHits         = 0;
        wPlayerBideAccumulatedDamage = wEnemyBideAccumulatedDamage = 0;
        wPlayerSubstituteHP    = wEnemySubstituteHP    = 0;
        wPlayerMonMinimized    = wEnemyMonMinimized    = 0;

        {
            uint8_t evo_slot, evo_new;
            if (bui_end_was_win && Battle_CheckNextEvolution(&evo_slot, &evo_new)) {

                s_evo_slot         = evo_slot;
                s_evo_old_species  = wPartyMons[evo_slot].base.species;
                s_evo_new_species  = evo_new;
                s_evo_phase        = 0;
                s_evo_cancelled    = 0;
                s_evo_timer        = 0;
                s_evo_screen_white = 0;
                bui_state = BUI_EVOLUTION;
            } else {

                if (wBattleResult != BATTLE_OUTCOME_CAUGHT)
                    bui_set_enemy_oam_visible(1);
                bui_state = BUI_INACTIVE;
            }
        }
        break;
    }
    }
}
