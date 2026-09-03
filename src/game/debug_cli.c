
#include "debug_cli.h"
#include "debug_suite.h"
#include "overworld.h"
#include "npc.h"
#include "gbc_color.h"
#include "gen2_resources.h"
#include "crystal_fade.h"
#include "gen1color/gen1color_battle.h"
#include "presentation_menu.h"
#include "menu.h"
#include "../platform/display.h"
#include "amberscript_saveops.h"
extern int Game_GetScene(void);

extern void Game_StartWildBattleScripted(uint8_t species, uint8_t level);
extern int Game_WarpToRealMap(uint8_t real_id, int x, int y);
#include "player.h"
#include "warp.h"
#include "text.h"
#include "yesno.h"
#include "trainer_sight.h"
#include "pokecenter.h"
#include "pokemon.h"
#include "battle/battle_ui.h"
#include "trade.h"
#include "battle/battle_init.h"
#include "battle/battle_exp.h"
#include "battle/battle.h"
#include "battle/battle_loop.h"
#include "music.h"
#include "johto_music.h"
#include "amberscript_mapbank.h"
#include "amberscript_scene.h"
#include "field_moves.h"
#include "poison.h"
#include "rockethideout_b4f_scripts.h"
#include "pallet_scripts.h"
#include "oakslab_scripts.h"
#include "viridian_mart_scripts.h"
#include "route24_scripts.h"
#include "bills_house_scripts.h"
#include "route2gate_scripts.h"
#include "vermilion_gym_scripts.h"
#include "cinnabar_gym_scripts.h"
#include "rockethideout_scripts.h"
#include "game_corner_scripts.h"
#include "pokemontower6f_scripts.h"
#include "pokemontower7f_scripts.h"
#include "mrfujis_house_scripts.h"
#include "saffron_city_scripts.h"
#include "celadon_city_scripts.h"
#include "seafoam_scripts.h"
#include "safari_zone_scripts.h"
#include "gate_scripts.h"
#include "gym_scripts.h"
#include "pokeflute.h"
#include "../data/base_stats.h"
#include "../data/map_data.h"
#include "../data/event_data.h"
#include "../data/event_flag_names.h"
#include "../data/event_flag_ids.h"
#include "../data/moves_data.h"
#include "../data/trainer_sprites.h"
#include "../data/event_constants.h"
#include "../data/item_names_gen.h"
#include "inventory.h"
#include "badge.h"
#include "py_ai_bridge.h"
#include "type_mod.h"
#include "species_mod.h"
#include "sprite_mod.h"
#include "amberscript_core.h"
#include "amberscript_tilemod.h"
#include "../platform/hardware.h"
#include "../platform/save.h"
#include "../game/constants.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include "../data/font_data.h"

#include "gen2_species.h"
#include "../platform/audio.h"

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

#define CMD_FILE   "bugs/cli_cmd.txt"
#define STATE_FILE "bugs/cli_state.txt"
#define REPLAY_DIR "bugs/replays"
#define DSL_STARTUP_CFG_PATH "mod_runtime/config/startup_dsl.cfg"
#define DSL_STARTUP_CFG_PATH_LEGACY "bugs/dsl_startup.cfg"
#define DSL_STARTUP_COMMANDS_DEFAULT "mod_runtime/scenes/startup_dsl_commands.scene"

#define BTN_A      0x01
#define BTN_B      0x02
#define BTN_SELECT 0x04
#define BTN_START  0x08
#define BTN_RIGHT  0x10
#define BTN_LEFT   0x20
#define BTN_UP     0x40
#define BTN_DOWN   0x80

uint8_t gCliButtons = 0;
int     gCliFrames  = 0;

#define SEQ_MAX 512
static uint8_t s_seq[SEQ_MAX];
static int     s_seq_len = 0;
static int     s_seq_pos = 0;

static void seq_push(uint8_t btn, int press_frames, int gap_frames) {
    for (int i = 0; i < press_frames && s_seq_len < SEQ_MAX; i++)
        s_seq[s_seq_len++] = btn;
    for (int i = 0; i < gap_frames && s_seq_len < SEQ_MAX; i++)
        s_seq[s_seq_len++] = 0;
}

static void seq_clear(void) { s_seq_len = 0; s_seq_pos = 0; }

static int s_poll_timer     = 0;
static int s_wait_remaining = 0;
static int s_pending_write  = 0;
static int s_script_trace_enabled = 0;
static int s_script_trace_to_file = 0;
static uint8_t s_trace_prev_map = 0xFF;
static uint8_t s_trace_prev_trainer_engaging = 0xFF;
static uint8_t s_trace_prev_text_open = 0xFF;
static uint8_t s_trace_prev_gate_active = 0xFF;
static uint8_t s_trace_prev_route22_active = 0xFF;
static uint8_t s_trace_prev_route24_active = 0xFF;
static uint8_t s_trace_prev_ssanne_active = 0xFF;
static uint8_t s_trace_prev_viridian_mart_active = 0xFF;
static uint8_t s_trace_prev_gym_active = 0xFF;
static uint8_t s_trace_prev_rockethideout_b4f_active = 0xFF;
static int s_temp_npc_walkoff_active = 0;
static int s_temp_npc_walkoff_idx = -1;
static int s_temp_npc_walkoff_phase = 0;
static int s_temp_npc_walkoff_pretext_frames = 0;
static int s_py_law_enabled = 0;
static int s_py_law_npc_idx = -1;
static int s_py_law_frame_accum = 0;
static uint32_t s_py_law_elapsed_sec = 0;
static char s_py_law_script[160] = {0};

#define SCENE_CMD_MAX 128
#define SCENE_ACTOR_MAX 16
#define SCENE_DEF_MAX 16
#define SCENE_DEF_LINE_MAX 32
typedef enum scene_op_t {
    SCOP_NOP = 0,
    SCOP_SPAWN,
    SCOP_DESPAWN,
    SCOP_FACE,
    SCOP_MOVE,
    SCOP_SAY,
    SCOP_ASK,
    SCOP_BATTLESTART,
    SCOP_BATTLEEND,
    SCOP_MUSIC,
    SCOP_WAIT,
    SCOP_WAIT_TEXT,
    SCOP_LOCK_INPUT,
    SCOP_TILE_COPY,
    SCOP_TILE_SAVE,
    SCOP_TILE_PLACE_CUSTOM,
    SCOP_BLOCK_SAVE,
    SCOP_BLOCK_PLACE_CUSTOM,
    SCOP_PY_AI,
    SCOP_PY_INJECT,
    SCOP_PY_LAW,
    SCOP_PY_LAW_SPAWN,
    SCOP_TYPEMOD,
    SCOP_SPRITE_FRONT_LOAD,
    SCOP_SPRITE_BACK_LOAD,
    SCOP_END
} scene_op_t;
typedef struct scene_cmd_t {
    scene_op_t op;
    char actor[24];
    int a, b, c, d;
    char text[160];
    uint8_t team_count;
    uint8_t team_species[6];
    uint8_t team_level[6];
    uint8_t team_moves[6][4];
} scene_cmd_t;
typedef struct scene_actor_t {
    int used;
    char name[24];
    int npc_idx;
    int spawned_by_scene;
    uint8_t sprite_id;
    int last_x;
    int last_y;
} scene_actor_t;
static int s_scene_active = 0;
static scene_cmd_t s_scene_cmds[SCENE_CMD_MAX];
static int s_scene_cmd_count = 0;
static int s_scene_pc = 0;
static int s_scene_wait = 0;
static int s_scene_wait_yesno = 0;
static int s_scene_wait_say = 0;
static int s_scene_say_opened = 0;
static int s_scene_wait_battle = 0;
static int s_scene_wait_battleend_text = 0;
static int s_scene_battle_started = 0;
static int s_scene_battlestart_pending = 0;
static int s_scene_battlestart_saw_text = 0;
static int s_scene_battlestart_delay = 0;
static int s_scene_battlestart_tc = 0;
static int s_scene_battlestart_tn = 0;
static int s_scene_last_yesno = -1;
static uint8_t s_scene_yesno_prev_joyignore = 0;
static int s_scene_yesno_restore_joyignore = 0;
static int s_scene_yesno_prev_scripted_movement = 0;
static int s_scene_yesno_restore_scripted_movement = 0;
static int s_scene_move_steps_left = 0;
static int s_scene_move_dir = 0;
static int s_scene_move_actor = -1;
static scene_actor_t s_scene_actors[SCENE_ACTOR_MAX];
typedef struct scene_def_t {
    int used;
    char name[32];
    char lines[SCENE_DEF_LINE_MAX][192];
    int line_count;
} scene_def_t;
static scene_def_t s_scene_defs[SCENE_DEF_MAX];

#define SCENE_TRIGGER_MAX 16
typedef struct scene_trigger_t {
    int used;
    char scene[64];
    uint8_t map_id;
    int x;
    int y;
    int armed;
    uint8_t cond_kind;
    uint16_t cond_event;
} scene_trigger_t;
static scene_trigger_t s_scene_triggers[SCENE_TRIGGER_MAX];
#define SCENE_TILE_PROP_MAX 256
typedef struct scene_tile_prop_t {
    int used;
    int banked;
    uint8_t map_id;
    int x;
    int y;
    uint8_t block_id;
    uint8_t tiles[4];
    uint8_t warp_mode;
    uint8_t dest_map;
    uint8_t dest_warp_idx;
} scene_tile_prop_t;
static scene_tile_prop_t s_scene_tile_props[SCENE_TILE_PROP_MAX];

#define SCENE_SAVED_TILE_MAX 32
typedef struct scene_saved_tile_t {
    int used;
    char name[32];
    uint8_t block_id;
    uint8_t tiles[4];
    uint8_t warp_mode;
    uint8_t dest_map;
    uint8_t dest_warp_idx;
} scene_saved_tile_t;
static scene_saved_tile_t s_scene_saved_tiles[SCENE_SAVED_TILE_MAX];

#define SCENE_SAVED_BLOCK_MAX 16
#define SCENE_SAVED_BLOCK_CELL_MAX 128
typedef struct scene_saved_block_cell_t {
    int dx;
    int dy;
    uint8_t block_id;
    uint8_t tiles[4];
    uint8_t warp_mode;
    uint8_t dest_map;
    uint8_t dest_warp_idx;
} scene_saved_block_cell_t;
typedef struct scene_saved_block_t {
    int used;
    char name[32];
    int cell_count;
    scene_saved_block_cell_t cells[SCENE_SAVED_BLOCK_CELL_MAX];
} scene_saved_block_t;
static scene_saved_block_t s_scene_saved_blocks[SCENE_SAVED_BLOCK_MAX];
typedef struct scene_npc_binding_t {
    int used;
    int npc_idx;
    uint8_t map_id;
    uint8_t sprite_id;
    int tile_x;
    int tile_y;
    char name[24];
    char scene[64];
} scene_npc_binding_t;
static scene_npc_binding_t s_scene_npc_bindings[SCENE_ACTOR_MAX];
static int s_dsl_bank_enabled = 0;
static int s_dsl_bank_init_done = 0;
static uint8_t s_dsl_bank_last_map = 0xFF;
static int s_dsl_startup_checked = 0;

#define SCRIPT_TRACE_LOG_PATH "bugs/script_trace.log"
#define SCRIPT_TRACE_LOG_PATH_OLD "bugs/script_trace.log.1"
#define SCRIPT_TRACE_MAX_BYTES (256 * 1024)
extern int Game_IsWarpFadeActive(void);

#define REPLAY_MAGIC   0x31504C52u
#define REPLAY_VERSION 1u

typedef struct replay_header_t {
    uint32_t magic;
    uint32_t version;
    uint32_t state_size;
    uint32_t input_frames;
} replay_header_t;

static int      s_replay_recording = 0;
static int      s_replay_playing = 0;
static uint32_t s_replay_play_pos = 0;
static uint32_t s_replay_play_len = 0;
static uint8_t *s_replay_play_buf = NULL;
static FILE    *s_replay_rec_fp = NULL;
static char     s_replay_name[96] = {0};
static char     s_replay_tmp_state[192] = {0};
static char     s_replay_tmp_input[192] = {0};

#define CLI_HIST_MAX   64
#define CLI_HIST_WIDTH 72
static char s_hist[CLI_HIST_MAX][CLI_HIST_WIDTH + 1];
static uint8_t s_hist_color[CLI_HIST_MAX];
static int  s_hist_head = 0;
static int  s_hist_count = 0;
static int  s_last_cmd_hist_slot = -1;
static int  s_last_cmd_valid = 1;

static int cli_hist_push_color(const char *line, uint8_t color) {
    int slot;
    size_t i = 0;
    if (!line || !*line) return -1;
    slot = s_hist_head;
    while (line[i] && i < CLI_HIST_WIDTH) {
        char c = line[i];
        s_hist[s_hist_head][i++] = (c >= 32 && c <= 126) ? c : ' ';
    }
    s_hist[s_hist_head][i] = '\0';
    s_hist_color[s_hist_head] = color;
    s_hist_head = (s_hist_head + 1) % CLI_HIST_MAX;
    if (s_hist_count < CLI_HIST_MAX) s_hist_count++;
    return slot;
}

static int cli_hist_push(const char *line) {
    return cli_hist_push_color(line, CLI_HIST_COLOR_DEFAULT);
}

static int cli_name_eq_loose(const char *a, const char *b) {
    for (;;) {
        while (*a == '_' || *a == '-' || *a == ' ') a++;
        while (*b == '_' || *b == '-' || *b == ' ') b++;
        if (!*a || !*b) break;
        char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

int DebugCLI_GetHistoryCount(void) { return s_hist_count; }

const char *DebugCLI_GetHistoryLine(int newest_index) {
    int idx;
    if (newest_index < 0 || newest_index >= s_hist_count) return NULL;
    idx = s_hist_head - 1 - newest_index;
    while (idx < 0) idx += CLI_HIST_MAX;
    return s_hist[idx];
}

int DebugCLI_GetHistoryColor(int newest_index) {
    int idx;
    if (newest_index < 0 || newest_index >= s_hist_count) return CLI_HIST_COLOR_DEFAULT;
    idx = s_hist_head - 1 - newest_index;
    while (idx < 0) idx += CLI_HIST_MAX;
    return s_hist_color[idx];
}

int DebugCLI_IsReplayPlaying(void) { return s_replay_playing; }

void DebugCLI_HistoryPushExternal(const char *line) {
    if (!line || !*line) return;
    cli_hist_push_color(line, CLI_HIST_COLOR_LOG);
}

static int s_animlab_enabled  = 0;
static int s_animlab_move_id  = 1;
static int s_animlab_loops    = 0;
static int s_animlab_level    = 50;
static int s_autowin_enabled  = 0;
int DebugCLI_IsAutoWinEnabled(void) { return s_autowin_enabled; }
void DebugCLI_SetAutoWinEnabled(int enabled) { s_autowin_enabled = enabled ? 1 : 0; }
int DebugCLI_IsNoClipEnabled(void) { return gNoClip ? 1 : 0; }
void DebugCLI_SetNoClipEnabled(int enabled) { gNoClip = enabled ? 1 : 0; }

int DebugCLI_TeleportToRealMap(uint8_t map_id, int x, int y) {
    int ok = Game_WarpToRealMap(map_id, x, y);
    if (!ok) return 0;
    gMapPalOffset = (map_id == 0x52 || map_id == 0xE8 ||
                     AmberScript_MapBank_IsDarkForRealId(map_id)) ? 6 : 0;
    Display_LoadMapPalette();
    return 1;
}

#define CON_TOP_ROW      16
#define CON_IN_ROW       17
#define CON_BUFMAX       64
#define CON_BLINK_PERIOD 60

static char    s_con_buf[CON_BUFMAX + 1] = {0};
static int     s_con_len   = 0;
static int     s_con_open  = 0;
static int     s_con_blink = 0;
static uint8_t s_con_saved[2 * SCREEN_WIDTH];
static int     s_con_overlay_enabled = 1;
static int     s_con_always_open = 0;

static int get_scene(void) {
    extern int Game_GetScene(void);
    return Game_GetScene();
}

static const char *scene_name(int sc) {
    switch (sc) {
        case 0: return "OVERWORLD";
        case 1: return "BATTLE_TRANS";
        case 2: return "BATTLE";
        default: return "MENU";
    }
}

static const char *facing_name(uint8_t dir) {
    switch (dir) {
        case 0:  return "DOWN";
        case 4:  return "UP";
        case 8:  return "LEFT";
        case 12: return "RIGHT";
        default: return "?";
    }
}

static const char *status_str(uint8_t st) {
    if (st & 0x07) return "SLP";
    if (st & 0x40) return "PSN";
    if (st & 0x10) return "BRN";
    if (st & 0x20) return "FRZ";
    if (st & 0x08) return "PAR";
    return "OK";
}

static const char *bui_state_name(int s) {

    switch (s) {
        case  0: return "INACTIVE";
        case  1: return "SLIDE_IN";
        case  2: return "APPEARED";
        case  3: return "SEND_OUT";
        case  4: return "ENEMY_SLIDE_OUT";
        case  5: return "TRAINER_SLIDE_OUT";
        case  6: return "ENEMY_SEND_OUT";
        case  7: return "POKEMON_APPEAR";
        case  8: return "INTRO";
        case  9: return "DRAW_HUD";
        case 10: return "MENU";
        case 11: return "MOVE_SELECT";
        case 12: return "MOVE_ANIM";
        case 13: return "HP_ANIM";
        case 14: return "EXEC_MOVE_B";
        case 15: return "EXEC_SECOND";
        case 16: return "TURN_END";
        case 17: return "TURN_FINISH";
        case 18: return "EXP_DRAIN";
        case 19: return "LEVELUP_STATS";
        case 20: return "ENEMY_FAINT_ANIM";
        case 21: return "PLAYER_FAINTED";
        case 22: return "USE_NEXT_MON";
        case 23: return "PARTY_SELECT";
        case 24: return "SWITCH_SELECT";
        case 25: return "RETREAT_ANIM";
        case 26: return "SWITCH_ENEMY_TURN";
        case 27: return "BAG_BATTLE";
        case 28: return "BALL_THROW";
        case 29: return "BALL_POOF";
        case 30: return "BALL_SHAKE";
        case 31: return "CAUGHT";
        case 32: return "END";
        default: return "?";
    }
}

static int debugcli_start_npc_walkoff(int print_log) {
    int spawn_x = (int)wXCoord + 1;
    int spawn_y = (int)wYCoord;
    int npc_face = 0;
    int idx;
    if (s_temp_npc_walkoff_active && s_temp_npc_walkoff_idx >= 0) {
        NPC_DebugDespawn(s_temp_npc_walkoff_idx);
        s_temp_npc_walkoff_active = 0;
        s_temp_npc_walkoff_idx = -1;
        s_temp_npc_walkoff_phase = 0;
        s_temp_npc_walkoff_pretext_frames = 0;
    }
    if (wPlayerDirection == 0) npc_face = 0;
    else if (wPlayerDirection == 4) npc_face = 1;
    else if (wPlayerDirection == 8) npc_face = 2;
    else if (wPlayerDirection == 12) npc_face = 3;
    idx = NPC_DebugSpawn(0x01, spawn_x, spawn_y, npc_face, 0 );
    if (idx < 0) {
        if (print_log) printf("[cli] npc_walkoff: failed to spawn NPC (no free slot?)\n");
        return 0;
    }
    s_temp_npc_walkoff_active = 1;
    s_temp_npc_walkoff_idx = idx;
    s_temp_npc_walkoff_phase = 1;
    s_temp_npc_walkoff_pretext_frames = 24;
    wJoyIgnore = PAD_CTRL_PAD;
    if (print_log) {
        printf("[cli] npc_walkoff: spawned npc idx=%d at (%d,%d), dialogue->walkoff sequence started\n",
               idx, spawn_x, spawn_y);
    }
    return 1;
}

int DebugCLI_TriggerNpcWalkoff(void) {
    return debugcli_start_npc_walkoff(0);
}

int DebugCLI_IsNpcWalkoffActive(void) {
    return s_temp_npc_walkoff_active;
}

static const char *hittrace_reason_name(uint8_t r) {
    switch (r) {
        case BHTR_HIT: return "hit";
        case BHTR_MISS_DREAM_EATER: return "miss:dream_eater_target_awake";
        case BHTR_HIT_SWIFT: return "hit:swift_always";
        case BHTR_MISS_INVULNERABLE: return "miss:target_invulnerable";
        case BHTR_MISS_MIST: return "miss:mist_block";
        case BHTR_HIT_XACCURACY: return "hit:x_accuracy_bypass";
        case BHTR_MISS_ACCURACY_ROLL: return "miss:accuracy_roll";
        default: return "unknown";
    }
}

static int first_alive_party_slot(void) {
    for (int i = 0; i < wPartyCount && i < PARTY_LENGTH; i++) {
        if (wPartyMons[i].base.hp > 0) return i;
    }
    return -1;
}

static void animlab_set_player_move(uint8_t move_id) {
    if (move_id == 0 || move_id >= NUM_MOVE_DEFS) move_id = 1;

    int slot = (int)wPlayerMonNumber;
    if (slot < 0 || slot >= PARTY_LENGTH || slot >= wPartyCount) slot = 0;

    wBattleMon.moves[0] = move_id;
    wBattleMon.moves[1] = 0;
    wBattleMon.moves[2] = 0;
    wBattleMon.moves[3] = 0;
    wBattleMon.pp[0]    = gMoves[move_id].pp;
    wBattleMon.pp[1]    = 0;
    wBattleMon.pp[2]    = 0;
    wBattleMon.pp[3]    = 0;

    wPartyMons[slot].base.moves[0] = move_id;
    wPartyMons[slot].base.moves[1] = 0;
    wPartyMons[slot].base.moves[2] = 0;
    wPartyMons[slot].base.moves[3] = 0;
    wPartyMons[slot].base.pp[0]    = gMoves[move_id].pp;
    wPartyMons[slot].base.pp[1]    = 0;
    wPartyMons[slot].base.pp[2]    = 0;
    wPartyMons[slot].base.pp[3]    = 0;
}

static void animlab_set_enemy_harmless(void) {

    wEnemyMon.moves[0] = MOVE_GROWL;
    wEnemyMon.moves[1] = 0;
    wEnemyMon.moves[2] = 0;
    wEnemyMon.moves[3] = 0;
    wEnemyMon.pp[0]    = gMoves[MOVE_GROWL].pp;
    wEnemyMon.pp[1]    = 0;
    wEnemyMon.pp[2]    = 0;
    wEnemyMon.pp[3]    = 0;
}

static void animlab_start_battle(int level) {
    int alive = first_alive_party_slot();
    if (alive < 0) {
        Pokemon_InitMon(&wPartyMons[0], STARTER1, (uint8_t)level);
        wPartyCount = 1;
        alive = 0;
    }

    wPartyMons[alive].base.hp     = wPartyMons[alive].max_hp;
    wPartyMons[alive].base.status = 0;

    wCurPartySpecies = SPECIES_RHYDON;
    wCurEnemyLevel   = (uint8_t)level;

    Music_Play(MUSIC_WILD_BATTLE);
    Battle_Start();

    animlab_set_enemy_harmless();
    animlab_set_player_move((uint8_t)s_animlab_move_id);

    BattleUI_Enter();
    extern void Game_SetScene(int);
    Game_SetScene(2);

    s_animlab_enabled = 1;
    s_animlab_level   = level;
    printf("[cli] animlab: started (level %d), auto-playing move animations\n", level);
}

static int count_bits8(uint8_t value) {
#ifdef _MSC_VER
    unsigned int bits = value;
    bits = bits - ((bits >> 1) & 0x55u);
    bits = (bits & 0x33u) + ((bits >> 2) & 0x33u);
    return (int)((bits + (bits >> 4)) & 0x0Fu);
#else
    return __builtin_popcount((unsigned int)value);
#endif
}

static int cli_resolve_move_id(const char *move_str) {
    if (!move_str || !*move_str) return 0;

    char *end;
    long parsed = strtol(move_str, &end, 0);
    if (end != move_str && *end == '\0') return (int)parsed;

    char needle[32] = {0};
    int ni = 0;
    for (int i = 0; move_str[i] && ni < 31; i++) {
        char c = (char)tolower((unsigned char)move_str[i]);
        if (c != ' ' && c != '_') needle[ni++] = c;
    }
    for (int id = 1; id <= 0xA5; id++) {
        const char *nm = gMoveNames[id];
        char norm[32] = {0};
        int nnorm = 0;
        for (int i = 0; nm[i] && nnorm < 31; i++) {
            char c = (char)tolower((unsigned char)nm[i]);
            if (c != ' ' && c != '_') norm[nnorm++] = c;
        }
        if (strcmp(needle, norm) == 0) return id;
    }
    return 0;
}

static int cli_resolve_species_id(const char *species_str) {
    uint8_t sid = 0;
    if (!species_str || !*species_str) return 0;
    if (SpeciesMod_ResolveSpeciesToken(species_str, &sid)) return (int)sid;
    {
        char *end;
        long parsed = strtol(species_str, &end, 0);
        if (end != species_str && *end == '\0') {
            if (parsed >= 1 && parsed <= 151) return gDexToSpecies[parsed];
            return (int)parsed;
        }
    }

    {
        char needle[40] = {0};
        int ni = 0;
        for (int i = 0; species_str[i] && ni < 39; i++) {
            char c = (char)tolower((unsigned char)species_str[i]);
            if (c != ' ' && c != '_') needle[ni++] = c;
        }
        for (int dex = 1; dex <= 151; dex++) {
            const char *nm = Pokemon_GetName((uint8_t)dex);
            char norm[40] = {0};
            int nn = 0;
            for (int i = 0; nm[i] && nn < 39; i++) {
                char c = (char)tolower((unsigned char)nm[i]);
                if (c != ' ' && c != '_') norm[nn++] = c;
            }
            if (strcmp(needle, norm) == 0) return gDexToSpecies[dex];
        }
    }
    return 0;
}

static int scene_parse_team_slot(const char *slot_str,
                                 uint8_t *out_species,
                                 uint8_t *out_level,
                                 uint8_t out_moves[4]) {
    char buf[160];
    char parts[6][32];
    int part_count = 0;
    int start = 0;
    int len;
    if (!slot_str || !out_species || !out_level || !out_moves) return 0;

    {
        char lower[24] = {0};
        int li = 0;
        while (*slot_str == ' ' || *slot_str == '\t') slot_str++;
        for (int i = 0; slot_str[i] && li < (int)sizeof(lower) - 1; i++)
            lower[li++] = (char)tolower((unsigned char)slot_str[i]);
        lower[li] = '\0';
        if (*slot_str == '\0' ||
            strcmp(slot_str, "-") == 0 ||
            strcmp(slot_str, "0") == 0 ||
            strcmp(lower, "empty") == 0 ||
            strcmp(lower, "none") == 0) {
            *out_species = 0;
            *out_level = 0;
            memset(out_moves, 0, 4);
            return 1;
        }
    }

    snprintf(buf, sizeof(buf), "%s", slot_str);
    len = (int)strlen(buf);
    for (int i = 0; i <= len && part_count < 6; i++) {
        if (buf[i] == ',' || buf[i] == '\0') {
            int n = i - start;
            while (n > 0 && (buf[start] == ' ' || buf[start] == '\t')) { start++; n--; }
            while (n > 0 && (buf[start + n - 1] == ' ' || buf[start + n - 1] == '\t')) n--;
            if (n <= 0) return 0;
            if (n >= (int)sizeof(parts[0])) n = (int)sizeof(parts[0]) - 1;
            memcpy(parts[part_count], buf + start, (size_t)n);
            parts[part_count][n] = '\0';
            part_count++;
            start = i + 1;
        }
    }
    if (part_count != 6) return 0;

    {
        int species = cli_resolve_species_id(parts[0]);
        int level = (int)strtol(parts[1], NULL, 0);
        if (species <= 0 || species > 255) return 0;
        if (level < 1 || level > 100) return 0;
        *out_species = (uint8_t)species;
        *out_level = (uint8_t)level;
        for (int i = 0; i < 4; i++) {
            int move = cli_resolve_move_id(parts[2 + i]);
            if (move < 0 || move > 255) return 0;
            out_moves[i] = (uint8_t)move;
        }
    }
    return 1;
}

static int cli_parse_arg(const char *src, int arg_index, char *out, size_t out_sz) {
    int idx = 0;
    const char *p = src;
    if (!src || !out || out_sz == 0) return 0;
    out[0] = '\0';
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (idx == arg_index) {
            size_t n = 0;
            char quote = 0;
            if (*p == '"' || *p == '\'') {
                quote = *p++;
                while (*p && *p != quote && n + 1 < out_sz) out[n++] = *p++;
                if (*p == quote) p++;
            } else {
                while (*p && *p != ' ' && *p != '\t' && n + 1 < out_sz) out[n++] = *p++;
            }
            out[n] = '\0';
            return n > 0;
        }
        if (*p == '"' || *p == '\'') {
            char q = *p++;
            while (*p && *p != q) p++;
            if (*p == q) p++;
        } else {
            while (*p && *p != ' ' && *p != '\t') p++;
        }
        idx++;
    }
    return 0;
}

static void cli_sanitize_key(const char *in, char *out, size_t out_sz) {
    size_t n = 0;
    if (!in || !out || out_sz == 0) return;
    for (size_t i = 0; in[i] && n + 1 < out_sz; i++) {
        char c = in[i];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '-') {
            out[n++] = c;
        } else if (c == ' ') {
            out[n++] = '_';
        }
    }
    out[n] = '\0';
}

static void cli_build_state_path(const char *key, char *path_out, size_t path_sz) {
    char clean[80] = {0};
    int all_digits = 1;
    if (!key || !*key) {
        snprintf(path_out, path_sz, "bugs/qs_slot_1.state");
        return;
    }
    for (int i = 0; key[i]; i++) {
        if (key[i] < '0' || key[i] > '9') { all_digits = 0; break; }
    }
    cli_sanitize_key(key, clean, sizeof(clean));
    if (clean[0] == '\0') strcpy(clean, "slot_1");
    if (all_digits) snprintf(path_out, path_sz, "bugs/qs_slot_%s.state", clean);
    else snprintf(path_out, path_sz, "bugs/qs_%s.state", clean);
}

static int cli_file_exists(const char *path) {
    struct stat st;
    return (path && stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

static int cli_make_dir_if_needed(const char *path) {
    struct stat st;
    if (!path || !*path) return -1;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode) ? 0 : -1;
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0777);
#endif
}

static int cli_copy_file(const char *src, const char *dst) {
    FILE *in = NULL, *out = NULL;
    char buf[4096];
    size_t n;
    in = fopen(src, "rb");
    if (!in) return -1;
    out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { fclose(in); fclose(out); return -1; }
    }
    fclose(in);
    fclose(out);
    return 0;
}

static int cli_backup_state_if_exists(const char *state_path) {
    char backup_dir[192] = {0};
    char backup_file[256] = {0};
    char base[96] = {0};
    char ext[24] = {0};
    int next_idx = 1;
    const char *name = NULL;
    const char *dot = NULL;
    DIR *d = NULL;
    struct dirent *ent;

    if (!cli_file_exists(state_path)) return 0;

    name = strrchr(state_path, '/');
    name = name ? name + 1 : state_path;
    dot = strrchr(name, '.');
    if (dot) {
        size_t blen = (size_t)(dot - name);
        if (blen >= sizeof(base)) blen = sizeof(base) - 1;
        memcpy(base, name, blen);
        base[blen] = '\0';
        snprintf(ext, sizeof(ext), "%s", dot);
    } else {
        snprintf(base, sizeof(base), "%s", name);
        snprintf(ext, sizeof(ext), ".state");
    }

    snprintf(backup_dir, sizeof(backup_dir), "bugs/%s_backup", base);
    if (cli_make_dir_if_needed(backup_dir) != 0) return -1;

    d = opendir(backup_dir);
    if (d) {
        while ((ent = readdir(d)) != NULL) {
            int idx = -1;
            char pat[128] = {0};
            snprintf(pat, sizeof(pat), "%s_%%d%s", base, ext);
            if (sscanf(ent->d_name, pat, &idx) == 1 && idx >= next_idx) {
                next_idx = idx + 1;
            }
        }
        closedir(d);
    }

    snprintf(backup_file, sizeof(backup_file), "%s/%s_%03d%s", backup_dir, base, next_idx, ext);
    return cli_copy_file(state_path, backup_file);
}

static void cli_reload_after_state_load(void) {
    AmberScript_ReloadAfterStateLoad();
}

static void cli_force_interrupt_runtime(void) {
    extern void Game_SetScene(int);
    if (Text_IsOpen())
        Text_Close();
    wJoyIgnore = 0;
    hJoyHeld = 0;
    gScriptedMovement = 0;
    wIsInBattle = 0;
    wBattleResult = BATTLE_OUTCOME_NONE;
    Game_SetScene(0);
}

static int cli_make_dir_tree(const char *path) {
    char tmp[256];
    size_t len;
    if (!path || !*path) return -1;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char save = tmp[i];
            tmp[i] = '\0';
            if (*tmp) cli_make_dir_if_needed(tmp);
            tmp[i] = save;
        }
    }
    return cli_make_dir_if_needed(tmp);
}

static void cli_build_replay_path(const char *name, char *out_path, size_t out_sz) {
    char clean[96] = {0};
    cli_sanitize_key(name, clean, sizeof(clean));
    if (clean[0] == '\0') snprintf(clean, sizeof(clean), "replay");
    snprintf(out_path, out_sz, "%s/%s.rpl", REPLAY_DIR, clean);
}

static void replay_reset_playback(void) {
    if (s_replay_play_buf) {
        free(s_replay_play_buf);
        s_replay_play_buf = NULL;
    }
    s_replay_playing = 0;
    s_replay_play_pos = 0;
    s_replay_play_len = 0;
}

static void replay_reset_recording(void) {
    if (s_replay_rec_fp) {
        fclose(s_replay_rec_fp);
        s_replay_rec_fp = NULL;
    }
    s_replay_recording = 0;
    s_replay_name[0] = '\0';
    s_replay_tmp_state[0] = '\0';
    s_replay_tmp_input[0] = '\0';
}

static void cli_script_trace_reset_latches(void) {
    s_trace_prev_map = 0xFF;
    s_trace_prev_trainer_engaging = 0xFF;
    s_trace_prev_text_open = 0xFF;
    s_trace_prev_gate_active = 0xFF;
    s_trace_prev_route22_active = 0xFF;
    s_trace_prev_route24_active = 0xFF;
    s_trace_prev_ssanne_active = 0xFF;
    s_trace_prev_viridian_mart_active = 0xFF;
    s_trace_prev_gym_active = 0xFF;
    s_trace_prev_rockethideout_b4f_active = 0xFF;
}

static void cli_script_trace_log_line(const char *line) {
    FILE *fp;
    struct stat st;
    if (!line || !*line) return;
    if (!s_script_trace_to_file) return;

    if (stat(SCRIPT_TRACE_LOG_PATH, &st) == 0 && st.st_size >= SCRIPT_TRACE_MAX_BYTES) {
        remove(SCRIPT_TRACE_LOG_PATH_OLD);
        rename(SCRIPT_TRACE_LOG_PATH, SCRIPT_TRACE_LOG_PATH_OLD);
    }

    fp = fopen(SCRIPT_TRACE_LOG_PATH, "a");
    if (!fp) return;
    fprintf(fp, "%s\n", line);
    fclose(fp);
}

static void cli_script_trace_emitf(const char *fmt, ...) {
    va_list ap;
    char buf[192];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("%s\n", buf);
    cli_script_trace_log_line(buf);
}

static int replay_start_record(const char *name) {
    char clean[96] = {0};
    if (!name || !*name) return -1;
    cli_sanitize_key(name, clean, sizeof(clean));
    if (clean[0] == '\0') return -1;

    cli_make_dir_tree(REPLAY_DIR);
    replay_reset_playback();
    replay_reset_recording();

    snprintf(s_replay_name, sizeof(s_replay_name), "%s", clean);
    snprintf(s_replay_tmp_state, sizeof(s_replay_tmp_state), "%s/%s.tmp.state", REPLAY_DIR, clean);
    snprintf(s_replay_tmp_input, sizeof(s_replay_tmp_input), "%s/%s.tmp.inputs", REPLAY_DIR, clean);

    if (Save_StateWrite(s_replay_tmp_state) != 0) return -1;
    s_replay_rec_fp = fopen(s_replay_tmp_input, "wb");
    if (!s_replay_rec_fp) {
        remove(s_replay_tmp_state);
        return -1;
    }
    s_replay_recording = 1;
    return 0;
}

static int replay_stop_record(char *out_final_path, size_t out_sz) {
    FILE *state = NULL, *in = NULL, *out = NULL;
    long state_sz_l = 0, input_sz_l = 0;
    size_t state_sz, input_sz;
    replay_header_t hdr;
    char final_path[192] = {0};
    char copy_buf[4096];
    size_t n;

    if (!s_replay_recording || !s_replay_rec_fp) return -1;
    fclose(s_replay_rec_fp);
    s_replay_rec_fp = NULL;
    s_replay_recording = 0;

    cli_build_replay_path(s_replay_name, final_path, sizeof(final_path));
    if (cli_backup_state_if_exists(final_path) != 0) {

    }

    state = fopen(s_replay_tmp_state, "rb");
    in = fopen(s_replay_tmp_input, "rb");
    out = fopen(final_path, "wb");
    if (!state || !in || !out) goto fail;

    if (fseek(state, 0, SEEK_END) != 0) goto fail;
    state_sz_l = ftell(state);
    if (state_sz_l < 0) goto fail;
    rewind(state);

    if (fseek(in, 0, SEEK_END) != 0) goto fail;
    input_sz_l = ftell(in);
    if (input_sz_l < 0) goto fail;
    rewind(in);

    state_sz = (size_t)state_sz_l;
    input_sz = (size_t)input_sz_l;

    hdr.magic = REPLAY_MAGIC;
    hdr.version = REPLAY_VERSION;
    hdr.state_size = (uint32_t)state_sz;
    hdr.input_frames = (uint32_t)input_sz;
    if (fwrite(&hdr, 1, sizeof(hdr), out) != sizeof(hdr)) goto fail;

    while ((n = fread(copy_buf, 1, sizeof(copy_buf), state)) > 0) {
        if (fwrite(copy_buf, 1, n, out) != n) goto fail;
    }
    while ((n = fread(copy_buf, 1, sizeof(copy_buf), in)) > 0) {
        if (fwrite(copy_buf, 1, n, out) != n) goto fail;
    }

    fclose(state); fclose(in); fclose(out);
    remove(s_replay_tmp_state);
    remove(s_replay_tmp_input);
    if (out_final_path && out_sz > 0) snprintf(out_final_path, out_sz, "%s", final_path);
    s_replay_name[0] = '\0';
    return 0;

fail:
    if (state) fclose(state);
    if (in) fclose(in);
    if (out) fclose(out);
    return -1;
}

static int replay_start_play(const char *name) {
    FILE *fp = NULL;
    replay_header_t hdr;
    char path[192] = {0};
    char tmp_state[192] = {0};
    uint8_t *state_buf = NULL;

    if (!name || !*name) return -1;
    cli_build_replay_path(name, path, sizeof(path));

    fp = fopen(path, "rb");
    if (!fp) return -1;
    if (fread(&hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) { fclose(fp); return -1; }
    if (hdr.magic != REPLAY_MAGIC || hdr.version != REPLAY_VERSION) { fclose(fp); return -1; }
    if (hdr.state_size == 0) { fclose(fp); return -1; }

    state_buf = (uint8_t *)malloc(hdr.state_size);
    if (!state_buf) { fclose(fp); return -1; }
    if (fread(state_buf, 1, hdr.state_size, fp) != hdr.state_size) {
        free(state_buf); fclose(fp); return -1;
    }

    replay_reset_playback();
    s_replay_play_buf = NULL;
    if (hdr.input_frames > 0) {
        s_replay_play_buf = (uint8_t *)malloc(hdr.input_frames);
        if (!s_replay_play_buf) { free(state_buf); fclose(fp); return -1; }
        if (fread(s_replay_play_buf, 1, hdr.input_frames, fp) != hdr.input_frames) {
            free(state_buf); replay_reset_playback(); fclose(fp); return -1;
        }
    }
    fclose(fp);

    snprintf(tmp_state, sizeof(tmp_state), "%s/.replay_load_%s.state", REPLAY_DIR, name);
    {
        FILE *sf = fopen(tmp_state, "wb");
        if (!sf) { free(state_buf); replay_reset_playback(); return -1; }
        if (fwrite(state_buf, 1, hdr.state_size, sf) != hdr.state_size) {
            fclose(sf); free(state_buf); replay_reset_playback(); return -1;
        }
        fclose(sf);
    }
    free(state_buf);

    if (Save_StateLoad(tmp_state) != 0) {
        remove(tmp_state);
        replay_reset_playback();
        return -1;
    }
    remove(tmp_state);
    cli_reload_after_state_load();

    s_replay_playing = 1;
    s_replay_play_pos = 0;
    s_replay_play_len = hdr.input_frames;
    return 0;
}

typedef struct cli_eventdiff_snapshot_t {
    int valid;
    uint8_t badges;
    uint8_t map;
    uint8_t x;
    uint8_t y;
    uint8_t party_count;
    uint8_t key_flags[15];
} cli_eventdiff_snapshot_t;

static cli_eventdiff_snapshot_t s_eventdiff = {0};

static const uint16_t s_eventdiff_keys[15] = {
    EVENT_GOT_POKEDEX,
    EVENT_BEAT_BROCK,
    EVENT_BEAT_MISTY,
    EVENT_BEAT_LT_SURGE,
    EVENT_BEAT_ERIKA,
    EVENT_BEAT_KOGA,
    EVENT_BEAT_BLAINE,
    EVENT_BEAT_SABRINA,
    EVENT_GOT_HM01,
    EVENT_GOT_TM34,
    EVENT_GOT_TM11,
    EVENT_GOT_TM24,
    EVENT_GOT_TM21,
    EVENT_GOT_TM06,
    EVENT_GOT_TM38
};

static void cli_gym_clear(const char *name) {
    if (strcmp(name, "brock") == 0) {
        ClearEvent(EVENT_BEAT_BROCK);
        ClearEvent(EVENT_GOT_TM34);
        ClearEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
        wObtainedBadges &= (uint8_t)~(1u << BADGE_BOULDER);
    } else if (strcmp(name, "misty") == 0) {
        ClearEvent(EVENT_BEAT_MISTY);
        ClearEvent(EVENT_GOT_TM11);
        ClearEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        ClearEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        wObtainedBadges &= (uint8_t)~(1u << BADGE_CASCADE);
    } else if (strcmp(name, "surge") == 0) {
        ClearEvent(EVENT_BEAT_LT_SURGE);
        ClearEvent(EVENT_GOT_TM24);
        ClearEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        ClearEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        ClearEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        wObtainedBadges &= (uint8_t)~(1u << BADGE_THUNDER);
    } else if (strcmp(name, "erika") == 0) {
        ClearEvent(EVENT_BEAT_ERIKA);
        ClearEvent(EVENT_GOT_TM21);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        wObtainedBadges &= (uint8_t)~(1u << BADGE_RAINBOW);
    } else if (strcmp(name, "koga") == 0) {
        ClearEvent(EVENT_BEAT_KOGA);
        ClearEvent(EVENT_GOT_TM06);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_0);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_1);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_2);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_3);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_4);
        ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_5);
        wObtainedBadges &= (uint8_t)~(1u << BADGE_SOUL);
    } else if (strcmp(name, "blaine") == 0) {
        ClearEvent(EVENT_BEAT_BLAINE);
        ClearEvent(EVENT_GOT_TM38);
        ClearEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_0);
        ClearEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_1);
        ClearEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_2);
        ClearEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_3);
        ClearEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_4);
        ClearEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_5);
        ClearEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_6);
        wObtainedBadges &= (uint8_t)~(1u << BADGE_VOLCANO);
    }
}

static int cli_is_numeric_token(const char *s) {
    if (!s || !*s) return 0;
    if (s[0] == '+' || s[0] == '-') s++;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        if (!*s) return 0;
        while (*s) {
            if (!((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F')))
                return 0;
            s++;
        }
        return 1;
    }
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        s++;
    }
    return 1;
}

static void cli_norm(char *dst, size_t dst_sz, const char *src) {
    size_t n = 0;
    if (!dst || dst_sz == 0) return;
    dst[0] = '\0';
    if (!src) return;
    for (size_t i = 0; src[i] && n + 1 < dst_sz; i++) {
        char c = (char)tolower((unsigned char)src[i]);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) dst[n++] = c;
    }
    dst[n] = '\0';
}

static char *cli_trim(char *s) {
    char *e;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) e--;
    *e = '\0';
    return s;
}

static void scene_normalize_ascii(char *s) {
    unsigned char *buf = (unsigned char*)s;
    int ri = 0;
    int wi = 0;
    if (!s) return;
    while (buf[ri]) {
        if (buf[ri] < 0x80) {
            buf[wi++] = buf[ri++];
            continue;
        }
        if (buf[ri] == 0xE2 && buf[ri + 1] == 0x80 && buf[ri + 2]) {
            unsigned char t = buf[ri + 2];
            if (t == 0x98 || t == 0x99) buf[wi++] = '\'';
            else if (t == 0x9C || t == 0x9D) buf[wi++] = '"';
            else if (t == 0x93 || t == 0x94) buf[wi++] = '-';
            ri += 3;
            continue;
        }
        ri++;
    }
    buf[wi] = '\0';
}

static int scene_parse_dir(const char *tok) {
    if (!tok) return -1;
    if (strcmp(tok, "down") == 0) return 0;
    if (strcmp(tok, "up") == 0) return 1;
    if (strcmp(tok, "left") == 0) return 2;
    if (strcmp(tok, "right") == 0) return 3;
    return -1;
}

static int cli_resolve_trainer_class_id(const char *tok) {
    char want[40];
    if (!tok || !*tok) return 0;
    if (cli_is_numeric_token(tok)) {
        int v = (int)strtol(tok, NULL, 0);
        if (v >= 1 && v <= NUM_TRAINERS) return v;
        return 0;
    }
    cli_norm(want, sizeof(want), tok);
    for (int i = 0; i < NUM_TRAINERS; i++) {
        char norm[40];
        cli_norm(norm, sizeof(norm), gTrainerClassNames[i]);
        if (strcmp(want, norm) == 0) return i + 1;
    }
    return 0;
}

static uint8_t scene_trainer_class_to_overworld_sprite(int trainer_class) {
    switch (trainer_class) {
        case 34: return 0x0C;
        case 35: return 0x06;
        case 36: return 0x07;
        case 37: return 0x0D;
        case 38: return 0x18;
        case 39: return 0x0B;
        case 40: return 0x06;
        case 44: return 0x3B;
        case 45: return 0x3A;
        case 46: return 0x39;
        case 47: return 0x1E;
        default: return 0x04;
    }
}

static int scene_parse_sprite(const char *tok) {
    static const struct { const char *name; int id; } k[] = {
        { "YOUNGSTER", 0x04 }, { "GAMBLER", 0x0B }, { "SUPER_NERD", 0x0C },
        { "GIRL", 0x0D }, { "COOLTRAINER_M", 0x07 }, { "COOLTRAINER_F", 0x06 },
        { "ROCKET", 0x18 }, { "GUARD", 0x31 }, { NULL, 0 }
    };
    if (!tok || !*tok) return -1;
    for (int i = 0; k[i].name; i++) if (strcmp(tok, k[i].name) == 0) return k[i].id;
    {
        int tc = cli_resolve_trainer_class_id(tok);
        if (tc > 0) return scene_trainer_class_to_overworld_sprite(tc);
    }
    if (cli_is_numeric_token(tok)) return (int)strtol(tok, NULL, 0);
    return -1;
}

static int scene_parse_music_track(const char *tok) {
    char norm[48];
    if (!tok || !*tok) return -1;
    cli_norm(norm, sizeof(norm), tok);
    if (strcmp(norm, "wildbattle") == 0 || strcmp(norm, "wild") == 0)
        return MUSIC_WILD_BATTLE;
    if (strcmp(norm, "trainerbattle") == 0 || strcmp(norm, "trainer") == 0)
        return MUSIC_TRAINER_BATTLE;
    if (strcmp(norm, "gymleader") == 0 || strcmp(norm, "gymleaderbattle") == 0 ||
        strcmp(norm, "elitefour") == 0 || strcmp(norm, "elite4") == 0 ||
        strcmp(norm, "elite_4") == 0 || strcmp(norm, "gymelitefour") == 0)
        return MUSIC_GYM_LEADER_BATTLE;
    if (strcmp(norm, "champion") == 0 || strcmp(norm, "championbattle") == 0)
        return MUSIC_FINAL_BATTLE;
    return -1;
}

static int scene_defs_find(const char *name) {
    for (int i = 0; i < SCENE_DEF_MAX; i++) {
        if (s_scene_defs[i].used && strcmp(s_scene_defs[i].name, name) == 0) return i;
    }
    return -1;
}

static int scene_defs_add(const char *name) {
    int i = scene_defs_find(name);
    if (i >= 0) return i;
    for (i = 0; i < SCENE_DEF_MAX; i++) {
        if (!s_scene_defs[i].used) {
            s_scene_defs[i].used = 1;
            s_scene_defs[i].line_count = 0;
            snprintf(s_scene_defs[i].name, sizeof(s_scene_defs[i].name), "%s", name);
            return i;
        }
    }
    return -1;
}

static int scene_is_line_command_start(const char *s) {
    if (!s || !*s) return 0;
    if (strncmp(s, "spawn ", 6) == 0) return 1;
    if (strncmp(s, "despawn ", 8) == 0) return 1;
    if (strncmp(s, "face ", 5) == 0) return 1;
    if (strncmp(s, "move ", 5) == 0) return 1;
    if (strncmp(s, "say ", 4) == 0) return 1;
    if (strncmp(s, "ask ", 4) == 0) return 1;
    if (strncmp(s, "battlestart", 11) == 0) return 1;
    if (strncmp(s, "battlend", 8) == 0) return 1;
    if (strncmp(s, "music ", 6) == 0) return 1;
    if (strncmp(s, "wait ", 5) == 0) return 1;
    if (strcmp(s, "wait_text") == 0) return 1;
    if (strcmp(s, "lock_input on") == 0) return 1;
    if (strcmp(s, "lock_input off") == 0) return 1;
    if (strncmp(s, "tile_copy ", 10) == 0) return 1;
    if (strncmp(s, "copy_tile ", 10) == 0) return 1;
    if (strncmp(s, "tile_save ", 10) == 0) return 1;
    if (strncmp(s, "tile_place_custom ", 18) == 0) return 1;
    if (strncmp(s, "tile_place ", 11) == 0) return 1;
    if (strncmp(s, "block_save ", 11) == 0) return 1;
    if (strncmp(s, "block_place_custom ", 19) == 0) return 1;
    if (strncmp(s, "block_place ", 12) == 0) return 1;
    if (strncmp(s, "py_ai ", 6) == 0) return 1;
    if (strncmp(s, "py_inject ", 10) == 0) return 1;
    if (strncmp(s, "py_law ", 7) == 0) return 1;
    if (strncmp(s, "py_law_spawn ", 13) == 0) return 1;
    if (strncmp(s, "type_define ", 12) == 0) return 1;
    if (strncmp(s, "type_chart_set ", 15) == 0) return 1;
    if (strncmp(s, "species_set_types ", 18) == 0) return 1;
    if (strncmp(s, "species_define ", 15) == 0) return 1;
    if (strncmp(s, "species_name_set ", 17) == 0) return 1;
    if (strncmp(s, "species_stats_set ", 18) == 0) return 1;
    if (strncmp(s, "species_moves_set ", 18) == 0) return 1;
    if (strncmp(s, "species_learn_add ", 18) == 0) return 1;
    if (strncmp(s, "species_bank ", 13) == 0) return 1;
    if (strncmp(s, "type_bank ", 10) == 0) return 1;
    if (strncmp(s, "sprite_front_load ", 18) == 0) return 1;
    if (strncmp(s, "sprite_back_load ", 17) == 0) return 1;
    if (strcmp(s, "end") == 0) return 1;
    if (strncmp(s, "use ", 4) == 0) return 1;
    if (strncmp(s, "include ", 8) == 0) return 1;
    if (strncmp(s, "def ", 4) == 0) return 1;
    if (strcmp(s, "enddef") == 0) return 1;
    return 0;
}

static void scene_unescape_text(char *s);
static void scene_format_dialog_text(char *s);

static int scene_parse_runtime_line(const char *line, scene_cmd_t *out_cmd) {
    scene_cmd_t cmd;
    const char *s = line;
    if (!line || !out_cmd) return 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0' || *s == '#') return 0;
    memset(&cmd, 0, sizeof(cmd));

    if (strncmp(s, "spawn ", 6) == 0) {
        char id[24], sprite[32], x[32], y[32];
        if (sscanf(s + 6, "%23s %31s %31s %31s", id, sprite, x, y) != 4) return 0;
        cmd.op = SCOP_SPAWN;
        snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
        cmd.a = scene_parse_sprite(sprite);
        cmd.b = cli_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
        cmd.c = cli_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
        if (!cli_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) cmd.b = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
        if (!cli_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) cmd.c = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
    } else if (strncmp(s, "despawn ", 8) == 0) {
        cmd.op = SCOP_DESPAWN;
        if (sscanf(s + 8, "%23s", cmd.actor) != 1) return 0;
    } else if (strncmp(s, "face ", 5) == 0) {
        char id[24], dir[24];
        if (sscanf(s + 5, "%23s %23s", id, dir) != 2) return 0;
        cmd.op = SCOP_FACE;
        snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
        if (strcmp(dir, "player") == 0) cmd.a = -2; else cmd.a = scene_parse_dir(dir);
    } else if (strncmp(s, "move ", 5) == 0) {
        char id[24], dir[24], steps[24];
        if (sscanf(s + 5, "%23s %23s %23s", id, dir, steps) != 3) return 0;
        cmd.op = SCOP_MOVE;
        snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
        cmd.a = scene_parse_dir(dir);
        cmd.b = (int)strtol(steps, NULL, 0);
    } else if (strncmp(s, "wait ", 5) == 0) {
        cmd.op = SCOP_WAIT;
        cmd.a = (int)strtol(s + 5, NULL, 0);
    } else if (strncmp(s, "say ", 4) == 0) {
        cmd.op = SCOP_SAY;
        snprintf(cmd.text, sizeof(cmd.text), "%s", s + 4);
        {
            size_t n = strlen(cmd.text);
            if (n >= 2 && cmd.text[0] == '"' && cmd.text[n - 1] == '"') {
                memmove(cmd.text, cmd.text + 1, n - 2);
                cmd.text[n - 2] = '\0';
            }
        }
        scene_unescape_text(cmd.text);
        scene_format_dialog_text(cmd.text);
    } else if (strcmp(s, "wait_text") == 0) {
        cmd.op = SCOP_WAIT_TEXT;
    } else if (strcmp(s, "lock_input on") == 0) {
        cmd.op = SCOP_LOCK_INPUT; cmd.a = 1;
    } else if (strcmp(s, "lock_input off") == 0) {
        cmd.op = SCOP_LOCK_INPUT; cmd.a = 0;
    } else if (strcmp(s, "end") == 0) {
        cmd.op = SCOP_END;
    } else if (strncmp(s, "py_law ", 7) == 0) {
        char mode[16] = {0};
        char script[120] = {0};
        int idx = -1;
        if (sscanf(s + 7, "%15s %d %119s", mode, &idx, script) >= 1) {
            cmd.op = SCOP_PY_LAW;
            cmd.a = (strcmp(mode, "on") == 0) ? 1 : 0;
            cmd.b = idx;
            if (script[0]) snprintf(cmd.text, sizeof(cmd.text), "%s", script);
        } else return 0;
    } else if (strncmp(s, "py_law_spawn ", 13) == 0) {
        char sprite[24] = {0}, x[32] = {0}, y[32] = {0}, script[120] = {0};
        if (sscanf(s + 13, "%23s %31s %31s %119s", sprite, x, y, script) != 4) return 0;
        cmd.op = SCOP_PY_LAW_SPAWN;
        cmd.a = scene_parse_sprite(sprite);
        cmd.b = cli_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
        cmd.c = cli_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
        if (!cli_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) cmd.b = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
        if (!cli_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) cmd.c = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
        snprintf(cmd.text, sizeof(cmd.text), "%s", script);
    } else if (strncmp(s, "type_define ", 12) == 0 ||
               strncmp(s, "type_chart_set ", 15) == 0 ||
               strncmp(s, "species_set_types ", 18) == 0 ||
               strncmp(s, "species_define ", 15) == 0 ||
               strncmp(s, "species_name_set ", 17) == 0 ||
               strncmp(s, "species_stats_set ", 18) == 0 ||
               strncmp(s, "species_moves_set ", 18) == 0 ||
               strncmp(s, "species_learn_add ", 18) == 0 ||
               strncmp(s, "species_bank ", 13) == 0 ||
               strncmp(s, "type_bank ", 10) == 0) {
        cmd.op = SCOP_TYPEMOD;
        snprintf(cmd.text, sizeof(cmd.text), "%s", s);
    } else if (strncmp(s, "sprite_front_load ", 18) == 0) {
        char sp[32], path[140];
        int sid;
        if (sscanf(s + 18, "%31s %139s", sp, path) != 2) return 0;
        sid = cli_resolve_species_id(sp);
        if (sid <= 0) return 0;
        cmd.op = SCOP_SPRITE_FRONT_LOAD;
        cmd.a = sid;
        snprintf(cmd.text, sizeof(cmd.text), "%s", path);
    } else if (strncmp(s, "sprite_back_load ", 17) == 0) {
        char sp[32], path[140];
        int sid;
        if (sscanf(s + 17, "%31s %139s", sp, path) != 2) return 0;
        sid = cli_resolve_species_id(sp);
        if (sid <= 0) return 0;
        cmd.op = SCOP_SPRITE_BACK_LOAD;
        cmd.a = sid;
        snprintf(cmd.text, sizeof(cmd.text), "%s", path);
    } else {
        return 0;
    }

    *out_cmd = cmd;
    return 1;
}

static void scene_inject_cmd_after_pc(const scene_cmd_t *cmd) {
    int insert_at;
    int tail_count;
    if (!cmd) return;
    if (s_scene_cmd_count >= SCENE_CMD_MAX) return;
    insert_at = s_scene_pc + 1;
    if (insert_at < 0) insert_at = 0;
    if (insert_at > s_scene_cmd_count) insert_at = s_scene_cmd_count;
    tail_count = s_scene_cmd_count - insert_at;
    if (tail_count > 0) {
        memmove(&s_scene_cmds[insert_at + 1], &s_scene_cmds[insert_at], (size_t)tail_count * sizeof(scene_cmd_t));
    }
    s_scene_cmds[insert_at] = *cmd;
    s_scene_cmd_count++;
}

static int scene_exec_py_inject(const char *spec) {
    static const char *kPathFmt[] = {
        "%s",
        "../%s",
        "mod_runtime/python/%s",
        "../mod_runtime/python/%s",
        "mods/python_ai/%s",
        "../mods/python_ai/%s",
    };
    char cmdline[640];
    char script_path[320] = {0};
    char runner[320] = {0};
    char out_line[256];
    FILE *fp;
    int injected = 0;
    int launch_ok = 0;
    scene_cmd_t parsed[SCENE_CMD_MAX];
    int parsed_count = 0;
    if (!spec || !*spec) return 0;
#ifdef _WIN32

#ifdef _WIN32
    _putenv_s("COMSPEC", "C:\\Windows\\System32\\cmd.exe");
#endif
#endif
    for (int pi = 0; pi < (int)(sizeof(kPathFmt) / sizeof(kPathFmt[0])); pi++) {
        FILE *probe;
        snprintf(script_path, sizeof(script_path), kPathFmt[pi], spec);
        probe = fopen(script_path, "r");
        if (probe) { fclose(probe); break; }
        script_path[0] = '\0';
    }
    if (!script_path[0]) {
        printf("[scene] py_inject: script not found '%s'\n", spec);
        return 0;
    }
    for (int i = 0; runner[i]; i++) {
        if (runner[i] == '\r' || runner[i] == '\n') runner[i] = '\0';
    }

    {
        FILE *rprobe = fopen("C:/Progra~1/Python311/python.exe", "r");
        if (rprobe) {
            fclose(rprobe);
            snprintf(runner, sizeof(runner), "%s", "C:/Progra~1/Python311/python.exe");
        }
    }
    if (!runner[0]) {
        FILE *wf = POPEN("where python 2>nul", "r");
        if (wf) {
            if (fgets(out_line, sizeof(out_line), wf)) {
                char *s = cli_trim(out_line);
                if (*s) snprintf(runner, sizeof(runner), "%s", s);
            }
            PCLOSE(wf);
        }
    }
    if (!runner[0]) {
        printf("[scene] py_inject: no python runner found\n");
        return 0;
    }
    {
        char cleaned[320];
        int j = 0;
        for (int i = 0; runner[i] && j + 1 < (int)sizeof(cleaned); i++) {
            char c = runner[i];
            if (c == '"' || c == '\r' || c == '\n') continue;
            if (c == '/') c = '\\';
            cleaned[j++] = c;
        }
        cleaned[j] = '\0';
        snprintf(runner, sizeof(runner), "%s", cleaned);
    }
    {
        char cleaned_script[320];
        int j = 0;
        for (int i = 0; script_path[i] && j + 1 < (int)sizeof(cleaned_script); i++) {
            char c = script_path[i];
            if (c == '/') c = '\\';
            cleaned_script[j++] = c;
        }
        cleaned_script[j] = '\0';
        snprintf(script_path, sizeof(script_path), "%s", cleaned_script);
    }

    {
        int run_injected = 0;
        int rc;
        snprintf(cmdline, sizeof(cmdline),
                 "%s %s %u %d %d %u 2>&1",
                 runner,
                 script_path,
                 (unsigned)wCurMap,
                 (int)wXCoord,
                 (int)wYCoord,
                 (unsigned)wPlayerDirection);
        printf("[scene] py_inject run: %s\n", cmdline);
        fp = POPEN(cmdline, "r");
        if (!fp) return 0;
        launch_ok = 1;
        while (fgets(out_line, sizeof(out_line), fp)) {
            scene_cmd_t icmd;
            char *s = cli_trim(out_line);
            scene_normalize_ascii(s);
            if (scene_parse_runtime_line(s, &icmd)) {
                if (parsed_count < SCENE_CMD_MAX) {
                    parsed[parsed_count++] = icmd;
                    run_injected++;
                }
            } else if (*s) {
                printf("[scene] py_inject out: %s\n", s);
            }
        }
        rc = PCLOSE(fp);
        fp = NULL;
        if (run_injected > 0) {
            for (int i = parsed_count - 1; i >= 0; i--) {
                if (s_scene_cmd_count >= SCENE_CMD_MAX) break;
                scene_inject_cmd_after_pc(&parsed[i]);
                injected++;
            }
            return injected;
        }
        (void)rc;
    }
    if (launch_ok) {
        printf("[scene] py_inject: python exited with non-zero status for '%s'\n", spec);
    }
    return injected;
}

static void py_law_tick_once(void) {
    char cmdline[640];
    char script_path[320] = {0};
    char out_line[256];
    char runner[320] = {0};
    int npc_x = -1, npc_y = -1;
    uint32_t day_sec;
    uint32_t minute;
    uint32_t band;
    FILE *fp;
    if (!s_py_law_enabled || !s_py_law_script[0]) return;

#ifdef _WIN32
    _putenv_s("COMSPEC", "C:\\Windows\\System32\\cmd.exe");
#endif
    if (s_py_law_npc_idx < 0 || s_py_law_npc_idx >= NPC_GetCount()) return;
    NPC_GetTilePos(s_py_law_npc_idx, &npc_x, &npc_y);
    if (npc_x < 0 || npc_y < 0) return;
    snprintf(script_path, sizeof(script_path), "%s", s_py_law_script);
    {
        FILE *probe = fopen(script_path, "r");
        if (!probe) {
            snprintf(script_path, sizeof(script_path), "../%s", s_py_law_script);
            probe = fopen(script_path, "r");
        }
        if (!probe) return;
        fclose(probe);
    }
    {
        FILE *probe = fopen("C:/Progra~1/Python311/python.exe", "r");
        if (probe) {
            fclose(probe);
            snprintf(runner, sizeof(runner), "C:/Progra~1/Python311/python.exe");
        }
    }
    if (!runner[0]) return;
    day_sec = s_py_law_elapsed_sec % 300u;
    minute = day_sec / 60u;
    band = minute % 2u;
    snprintf(cmdline, sizeof(cmdline),
             "%s %s %u %d %d %u %d %d %d %u %u %u 2>&1",
             runner, script_path,
             (unsigned)wCurMap, (int)wXCoord, (int)wYCoord, (unsigned)wPlayerDirection,
             s_py_law_npc_idx, npc_x, npc_y,
             (unsigned)s_py_law_elapsed_sec, (unsigned)day_sec, (unsigned)band);
    fp = POPEN(cmdline, "r");
    if (!fp) return;
    while (fgets(out_line, sizeof(out_line), fp)) {
        char *s = cli_trim(out_line);
        int dir;
        scene_normalize_ascii(s);
        if (sscanf(s, "npc_step %d", &dir) == 1) {
            if (dir >= 0 && dir <= 3 && !NPC_IsWalking(s_py_law_npc_idx)) {
                NPC_DoScriptedStep(s_py_law_npc_idx, dir);
            }
        } else if (sscanf(s, "npc_face %d", &dir) == 1) {
            if (dir >= 0 && dir <= 3) NPC_SetFacing(s_py_law_npc_idx, dir);
        } else if (*s) {
            printf("[scene] py_law out: %s\n", s);
        }
    }
    PCLOSE(fp);
}

static void scene_exec_typemod_line(const char *line) {
    char a[32], b[32], c[32];
    char nbuf[32];
    uint8_t ta, td, t1, t2;
    if (!line || !*line) return;
    if (sscanf(line, "type_define %31s %31s", a, b) == 2) {
        if (TypeMod_ResolveTypeToken(b, &t1)) TypeMod_SetTypeAlias(a, t1);
        return;
    }
    if (sscanf(line, "type_chart_set %31s %31s %31s", a, b, c) == 3) {
        if (!TypeMod_ResolveTypeToken(a, &ta) || !TypeMod_ResolveTypeToken(b, &td)) return;
        if (strcmp(c, "IMMUNE") == 0 || strcmp(c, "0") == 0) TypeMod_SetEffect(ta, td, 0);
        else if (strcmp(c, "NOT_VERY") == 0 || strcmp(c, "HALF") == 0 || strcmp(c, "5") == 0) TypeMod_SetEffect(ta, td, 5);
        else if (strcmp(c, "SUPER") == 0 || strcmp(c, "20") == 0 || strcmp(c, "2") == 0) TypeMod_SetEffect(ta, td, 20);
        else TypeMod_SetEffect(ta, td, 10);
        return;
    }
    if (sscanf(line, "species_set_types %31s %31s %31s", a, b, c) == 3) {
        unsigned species = 0;
        char *end = NULL;
        long sv = strtol(a, &end, 0);
        if (end != a && *end == '\0' && sv >= 0 && sv <= 255) species = (unsigned)sv;
        else {
            int sid = cli_resolve_species_id(a);
            if (sid > 0) species = (unsigned)sid;
        }
        if (species == 0) return;
        if (!TypeMod_ResolveTypeToken(b, &t1) || !TypeMod_ResolveTypeToken(c, &t2)) return;
        TypeMod_SetSpeciesTypes((uint8_t)species, t1, t2);
        return;
    }
    if (sscanf(line, "type_bank %31s", a) == 1) {
        if (strcmp(a, "on") == 0) TypeMod_BankSetEnabled(1);
        else if (strcmp(a, "off") == 0) TypeMod_BankSetEnabled(0);
        else if (strcmp(a, "save") == 0) TypeMod_BankSave();
        else if (strcmp(a, "load") == 0) TypeMod_BankLoad();
        else if (strcmp(a, "clear") == 0) TypeMod_BankClear();
        else if (strcmp(a, "status") == 0)
            printf("[scene] type_bank: %s\n", TypeMod_BankIsEnabled() ? "on" : "off");
        return;
    }
    if (sscanf(line, "species_define %31s %31s", a, b) == 2) {
        uint8_t sid = 0;
        if (!SpeciesMod_ResolveSpeciesToken(b, &sid)) return;
        SpeciesMod_DefineAlias(a, sid);
        return;
    }
    if (sscanf(line, "species_name_set %31s %31s", a, b) == 2) {
        int sid = cli_resolve_species_id(a);
        if (sid <= 0 || sid > 255) return;
        snprintf(nbuf, sizeof(nbuf), "%s", b);
        for (int i = 0; nbuf[i]; i++) if (nbuf[i] == '_') nbuf[i] = ' ';
        SpeciesMod_SetName((uint8_t)sid, nbuf);
        return;
    }
    {
        unsigned hp = 0, atk = 0, def = 0, spd = 0, spc = 0, cr = 0, be = 0, gr = 0;
        if (sscanf(line, "species_stats_set %31s %u %u %u %u %u %31s %31s %u %u %u",
                   a, &hp, &atk, &def, &spd, &spc, b, c, &cr, &be, &gr) == 11) {
            int sid = cli_resolve_species_id(a);
            base_stats_t bs = {0};
            if (sid <= 0 || sid > 255) return;
            if (!TypeMod_ResolveTypeToken(b, &t1) || !TypeMod_ResolveTypeToken(c, &t2)) return;
            if (!SpeciesMod_GetBaseStats((uint8_t)sid, &bs)) memset(&bs, 0, sizeof(bs));
            bs.hp = (uint8_t)hp; bs.atk = (uint8_t)atk; bs.def = (uint8_t)def; bs.spd = (uint8_t)spd; bs.spc = (uint8_t)spc;
            bs.type1 = t1; bs.type2 = t2;
            bs.catch_rate = (uint8_t)cr;
            bs.base_exp = (uint8_t)be;
            bs.growth_rate = (uint8_t)gr;
            SpeciesMod_SetBaseStats((uint8_t)sid, &bs);

            TypeMod_SetSpeciesTypes((uint8_t)sid, t1, t2);
            return;
        }
    }
    if (strncmp(line, "species_moves_set ", 18) == 0) {
        char m1s[32] = {0}, m2s[32] = {0}, m3s[32] = {0}, m4s[32] = {0};
        int sid, m1, m2, m3, m4;
        if (sscanf(line, "species_moves_set %31s %31s %31s %31s %31s", a, m1s, m2s, m3s, m4s) != 5) return;
        sid = cli_resolve_species_id(a);
        if (sid <= 0 || sid > 255) return;
        m1 = cli_resolve_move_id(m1s); m2 = cli_resolve_move_id(m2s); m3 = cli_resolve_move_id(m3s); m4 = cli_resolve_move_id(m4s);
        if (m1 < 0 || m1 > 255 || m2 < 0 || m2 > 255 || m3 < 0 || m3 > 255 || m4 < 0 || m4 > 255) return;
        SpeciesMod_SetStartMoves((uint8_t)sid, (uint8_t)m1, (uint8_t)m2, (uint8_t)m3, (uint8_t)m4);
        return;
    }
    {
        unsigned lv = 0;
        char ms[32] = {0};
        if (sscanf(line, "species_learn_add %31s %u %31s", a, &lv, ms) == 3) {
            int sid = cli_resolve_species_id(a);
            int mid = cli_resolve_move_id(ms);
            if (sid <= 0 || sid > 255 || lv > 100 || mid < 0 || mid > 255) return;
            SpeciesMod_AddLevelMove((uint8_t)sid, (uint8_t)lv, (uint8_t)mid);
            return;
        }
    }
    if (sscanf(line, "species_bank %31s", a) == 1) {
        if (strcmp(a, "on") == 0) SpeciesMod_BankSetEnabled(1);
        else if (strcmp(a, "off") == 0) SpeciesMod_BankSetEnabled(0);
        else if (strcmp(a, "save") == 0) SpeciesMod_BankSave();
        else if (strcmp(a, "load") == 0) SpeciesMod_BankLoad();
        else if (strcmp(a, "clear") == 0) SpeciesMod_BankClear();
        else if (strcmp(a, "status") == 0)
            printf("[scene] species_bank: %s\n", SpeciesMod_BankIsEnabled() ? "on" : "off");
    }
}

static void scene_apply_args(const char *src, char *dst, int dst_sz, char args[16][96]) {
    int di = 0;
    for (int si = 0; src[si] && di + 1 < dst_sz; si++) {

        if (src[si] == '$' && src[si + 1] == '1' && src[si + 2] >= '0' && src[si + 2] <= '6') {
            int ai = 9 + (src[si + 2] - '0');
            const char *a = args[ai];
            for (int k = 0; a[k] && di + 1 < dst_sz; k++) dst[di++] = a[k];
            si += 2;
        } else if (src[si] == '$' && src[si + 1] >= '1' && src[si + 1] <= '9') {
            int ai = src[si + 1] - '1';
            const char *a = args[ai];
            for (int k = 0; a[k] && di + 1 < dst_sz; k++) dst[di++] = a[k];
            si++;
        } else {
            dst[di++] = src[si];
        }
    }
    dst[di] = '\0';
}

static int scene_split_args_quoted(const char *s, char out[][96], int max_out) {
    int count = 0;
    int i = 0;
    while (s && s[i] && count < max_out) {
        int oi = 0;
        while (s[i] == ' ' || s[i] == '\t') i++;
        if (!s[i]) break;
        if (s[i] == '"') {
            i++;
            while (s[i] && s[i] != '"' && oi + 1 < 96) out[count][oi++] = s[i++];
            if (s[i] == '"') i++;
        } else {
            while (s[i] && s[i] != ' ' && s[i] != '\t' && oi + 1 < 96) out[count][oi++] = s[i++];
        }
        out[count][oi] = '\0';
        count++;
    }
    return count;
}

static void scene_unescape_text(char *s) {
    char out[160];
    int oi = 0;
    for (int i = 0; s[i] && oi + 1 < (int)sizeof(out); i++) {
        if (s[i] == '\\' && s[i + 1]) {
            char n = s[i + 1];
            if (n == 'n') { out[oi++] = '\n'; i++; continue; }
            if (n == 'f') { out[oi++] = '\f'; i++; continue; }

            if (n == 'c') { out[oi++] = TEXT_ASCII_CONT; i++; continue; }
            if (n == 't') { out[oi++] = '\t'; i++; continue; }
            if (n == '\\') { out[oi++] = '\\'; i++; continue; }
            if (n == '"') { out[oi++] = '"'; i++; continue; }
        }
        out[oi++] = s[i];
    }
    out[oi] = '\0';
    snprintf(s, 160, "%s", out);
}

static void scene_format_dialog_text(char *s) {
    enum { MAX_COLS = 18, MAX_LINES = 2 };
    char out[160];
    int oi = 0;
    int i = 0;
    int col = 0;
    int line = 0;
    int ended = 0;

    while (s[i] && oi + 1 < (int)sizeof(out)) {
        char c = s[i];

        if (c == '@') {
            out[oi++] = '@';
            ended = 1;
            break;
        }
        if (c == '\f') {
            out[oi++] = '\f';
            col = 0;
            line = 0;
            i++;
            continue;
        }
        if (c == '\n' || c == TEXT_ASCII_CONT) {

            out[oi++] = c;
            col = 0;
            line++;
            if (line >= MAX_LINES) line = MAX_LINES - 1;
            i++;
            continue;
        }

        if (c == ' ' || c == '\t' || c == '\r') {
            i++;
            continue;
        }

        {
            int ws = i;
            int we = i;
            int wlen;
            while (s[we] && s[we] != '@' && s[we] != '\n' && s[we] != '\f' &&
                   s[we] != TEXT_ASCII_CONT &&
                   s[we] != ' ' && s[we] != '\t' && s[we] != '\r') {
                we++;
            }
            wlen = we - ws;
            if (wlen <= 0) {
                i = we;
                continue;
            }

            if (wlen > MAX_COLS) {
                int p = ws;
                if (col > 0) {
                    if (line == 0) { out[oi++] = '\n'; line = 1; }
                    else { out[oi++] = '\f'; line = 0; }
                    col = 0;
                }
                while (p < we && oi + 1 < (int)sizeof(out)) {
                    int chunk = we - p;
                    if (chunk > MAX_COLS) chunk = MAX_COLS;
                    for (int k = 0; k < chunk && oi + 1 < (int)sizeof(out); k++)
                        out[oi++] = s[p + k];
                    p += chunk;
                    col = chunk;
                    if (p < we && oi + 1 < (int)sizeof(out)) {
                        if (line == 0) { out[oi++] = '\n'; line = 1; }
                        else { out[oi++] = '\f'; line = 0; }
                        col = 0;
                    }
                }
                i = we;
                continue;
            }

            if (col == 0) {
                for (int k = 0; k < wlen && oi + 1 < (int)sizeof(out); k++)
                    out[oi++] = s[ws + k];
                col = wlen;
            } else if (col + 1 + wlen <= MAX_COLS) {
                out[oi++] = ' ';
                for (int k = 0; k < wlen && oi + 1 < (int)sizeof(out); k++)
                    out[oi++] = s[ws + k];
                col += 1 + wlen;
            } else {
                if (line == 0) { out[oi++] = '\n'; line = 1; }
                else { out[oi++] = '\f'; line = 0; }
                col = 0;
                for (int k = 0; k < wlen && oi + 1 < (int)sizeof(out); k++)
                    out[oi++] = s[ws + k];
                col = wlen;
            }

            i = we;
        }
    }

    if (!ended && oi + 1 < (int)sizeof(out))
        out[oi++] = '@';
    out[oi] = '\0';
    snprintf(s, 160, "%s", out);
}

static void scene_reset_runtime(void) {
    s_scene_active = 0;
    s_scene_cmd_count = 0;
    s_scene_pc = 0;
    s_scene_wait = 0;
    s_scene_wait_yesno = 0;
    s_scene_wait_say = 0;
    s_scene_say_opened = 0;
    s_scene_wait_battle = 0;
    s_scene_wait_battleend_text = 0;
    s_scene_battle_started = 0;
    s_scene_battlestart_pending = 0;
    s_scene_battlestart_saw_text = 0;
    s_scene_battlestart_delay = 0;
    s_scene_battlestart_tc = 0;
    s_scene_battlestart_tn = 0;
    s_scene_last_yesno = -1;
    s_scene_yesno_prev_joyignore = 0;
    s_scene_yesno_restore_joyignore = 0;
    s_scene_yesno_prev_scripted_movement = 0;
    s_scene_yesno_restore_scripted_movement = 0;
    s_scene_move_steps_left = 0;
    s_scene_move_dir = 0;
    s_scene_move_actor = -1;
    memset(s_scene_actors, 0, sizeof(s_scene_actors));
}

static int scene_find_actor(const char *name) {
    for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
        if (s_scene_actors[i].used && strcmp(s_scene_actors[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int scene_add_actor(const char *name, int npc_idx) {
    int i = scene_find_actor(name);
    if (i >= 0) { s_scene_actors[i].npc_idx = npc_idx; return i; }
    for (i = 0; i < SCENE_ACTOR_MAX; i++) {
        if (!s_scene_actors[i].used) {
            s_scene_actors[i].used = 1;
            snprintf(s_scene_actors[i].name, sizeof(s_scene_actors[i].name), "%s", name);
            s_scene_actors[i].npc_idx = npc_idx;
            return i;
        }
    }
    return -1;
}

static int scene_npc_binding_find_by_idx(int npc_idx) {

    int dx = 0, dy = 0, have_pos = 0;
    if (npc_idx >= 0 && npc_idx < NPC_GetCount()) {
        int decl = NPC_GetDeclIdx(npc_idx);
        if (decl >= 0 && AmberScript_GetNpcDeclaredPos((uint8_t)wCurMap, decl, &dx, &dy))
            have_pos = 1;
        else { NPC_GetTilePos(npc_idx, &dx, &dy); have_pos = 1; }
    }
    if (have_pos) {
        for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
            if (!s_scene_npc_bindings[i].used) continue;
            if (s_scene_npc_bindings[i].map_id != wCurMap) continue;
            if (s_scene_npc_bindings[i].tile_x != dx || s_scene_npc_bindings[i].tile_y != dy) continue;
            s_scene_npc_bindings[i].npc_idx = npc_idx;
            return i;
        }
    }

    for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
        if (!s_scene_npc_bindings[i].used) continue;
        if (s_scene_npc_bindings[i].map_id != wCurMap) continue;
        if (s_scene_npc_bindings[i].npc_idx == npc_idx) return i;
    }
    return -1;
}

static int scene_npc_binding_alloc(void) {
    for (int i = 0; i < SCENE_ACTOR_MAX; i++)
        if (!s_scene_npc_bindings[i].used) return i;
    return -1;
}

void DebugCLI_ClearSceneNpcBindingsForMap(uint8_t map_id) {
    for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
        if (s_scene_npc_bindings[i].used && s_scene_npc_bindings[i].map_id == map_id)
            memset(&s_scene_npc_bindings[i], 0, sizeof(s_scene_npc_bindings[i]));
    }
    for (int i = 0; i < SCENE_TILE_PROP_MAX; i++) {
        if (s_scene_tile_props[i].used && s_scene_tile_props[i].map_id == map_id)
            memset(&s_scene_tile_props[i], 0, sizeof(s_scene_tile_props[i]));
    }
}

static int scene_tile_prop_find_slot(int x, int y);
static int scene_tile_prop_find_slot_for_map(uint8_t map_id, int x, int y);
static int scene_tile_prop_alloc_slot(void);
static int scene_saved_tile_alloc(const char *name);
static int scene_saved_block_alloc(const char *name);
static int scene_parse_coord_expr(const char *tok, int is_x, int *out);
static void scene_normalize_coord_args(const char *src, char *dst, size_t dst_sz);
static int scene_parse_block_save_args(const char *args, char *name, size_t name_sz,
                                       int *sx, int *sy, int *ex, int *ey);
static int scene_parse_named_coord_args(const char *args, char *name, size_t name_sz,
                                        int *x, int *y);
static void dsl_bank_save(void);

static const char *kDslBankDataPaths[] = {
    "debug/dsl_bank_scene_npc.dat",
    "../debug/dsl_bank_scene_npc.dat",
    "build/debug/dsl_bank_scene_npc.dat"
};
static const char *kDslBankCfgPaths[] = {
    "debug/dsl_bank_scene_npc.cfg",
    "../debug/dsl_bank_scene_npc.cfg",
    "build/debug/dsl_bank_scene_npc.cfg"
};

static void dsl_bank_save(void) {
    for (int pi = 0; pi < (int)(sizeof(kDslBankDataPaths) / sizeof(kDslBankDataPaths[0])); pi++) {
        FILE *fp = fopen(kDslBankDataPaths[pi], "w");
        if (!fp) continue;
        fprintf(fp, "DSLBANK2\n");
        for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
            scene_npc_binding_t *b = &s_scene_npc_bindings[i];
            if (!b->used) continue;
            fprintf(fp, "NPC|%u|%u|%d|%d|%s|%s\n",
                    (unsigned)b->map_id, (unsigned)b->sprite_id, b->tile_x, b->tile_y,
                    b->name, b->scene);
        }
        for (int i = 0; i < SCENE_SAVED_TILE_MAX; i++) {
            scene_saved_tile_t *t = &s_scene_saved_tiles[i];
            if (!t->used) continue;
            fprintf(fp, "TILEDEF|%s|%u|%u|%u|%u|%u|%u|%u|%u\n",
                    t->name, (unsigned)t->block_id, (unsigned)t->warp_mode,
                    (unsigned)t->dest_map, (unsigned)t->dest_warp_idx,
                    (unsigned)t->tiles[0], (unsigned)t->tiles[1],
                    (unsigned)t->tiles[2], (unsigned)t->tiles[3]);
        }
        for (int i = 0; i < SCENE_SAVED_BLOCK_MAX; i++) {
            scene_saved_block_t *b = &s_scene_saved_blocks[i];
            if (!b->used) continue;
            fprintf(fp, "BLOCKDEF|%s|%d\n", b->name, b->cell_count);
            for (int c = 0; c < b->cell_count; c++) {
                scene_saved_block_cell_t *cell = &b->cells[c];
                fprintf(fp, "BLOCKCELL|%s|%d|%d|%u|%u|%u|%u|%u|%u|%u|%u\n",
                        b->name, cell->dx, cell->dy, (unsigned)cell->block_id,
                        (unsigned)cell->warp_mode, (unsigned)cell->dest_map,
                        (unsigned)cell->dest_warp_idx, (unsigned)cell->tiles[0],
                        (unsigned)cell->tiles[1], (unsigned)cell->tiles[2],
                        (unsigned)cell->tiles[3]);
            }
        }
        for (int i = 0; i < SCENE_TILE_PROP_MAX; i++) {
            scene_tile_prop_t *t = &s_scene_tile_props[i];
            if (!t->used || !t->banked) continue;
            fprintf(fp, "TILE|%u|%d|%d|%u|%u|%u|%u|%u|%u|%u|%u\n",
                    (unsigned)t->map_id, t->x, t->y, (unsigned)t->block_id,
                    (unsigned)t->warp_mode, (unsigned)t->dest_map,
                    (unsigned)t->dest_warp_idx, (unsigned)t->tiles[0],
                    (unsigned)t->tiles[1], (unsigned)t->tiles[2],
                    (unsigned)t->tiles[3]);
        }
        fclose(fp);
    }
}

static void dsl_bank_write_cfg(void) {
    for (int pi = 0; pi < (int)(sizeof(kDslBankCfgPaths) / sizeof(kDslBankCfgPaths[0])); pi++) {
        FILE *fp = fopen(kDslBankCfgPaths[pi], "w");
        if (!fp) continue;
        fprintf(fp, "%d\n", s_dsl_bank_enabled ? 1 : 0);
        fclose(fp);
    }
}

static void dsl_bank_load(void) {
    FILE *fp = NULL;
    char line[256];
    int header_version = 1;
    for (int pi = 0; pi < (int)(sizeof(kDslBankDataPaths) / sizeof(kDslBankDataPaths[0])); pi++) {
        fp = fopen(kDslBankDataPaths[pi], "r");
        if (fp) break;
    }
    if (!fp) return;
    memset(s_scene_npc_bindings, 0, sizeof(s_scene_npc_bindings));
    memset(s_scene_saved_tiles, 0, sizeof(s_scene_saved_tiles));
    memset(s_scene_saved_blocks, 0, sizeof(s_scene_saved_blocks));
    for (int i = 0; i < SCENE_TILE_PROP_MAX; i++) {
        if (s_scene_tile_props[i].used && s_scene_tile_props[i].banked)
            memset(&s_scene_tile_props[i], 0, sizeof(s_scene_tile_props[i]));
    }
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return; }
    if (strncmp(line, "DSLBANK2", 8) == 0) header_version = 2;
    else if (strncmp(line, "DSLBANK1", 8) != 0) {
        fseek(fp, 0, SEEK_SET);
        header_version = 1;
    }
    while (fgets(line, sizeof(line), fp)) {
        unsigned map_id = 0, sprite_id = 0, block_id = 0, warp_mode = 0, dest_map = 0, dest_warp_idx = 0;
        unsigned t0 = 0, t1 = 0, t2 = 0, t3 = 0;
        int tx = 0, ty = 0;
        char name[24] = {0}, scene[64] = {0};
        if (strncmp(line, "NPC|", 4) == 0 || header_version == 1) {
            const char *p = (strncmp(line, "NPC|", 4) == 0) ? line + 4 : line;
            int slot = scene_npc_binding_alloc();
            if (slot < 0) continue;
            if (sscanf(p, "%u|%u|%d|%d|%23[^|]|%63[^\n]", &map_id, &sprite_id, &tx, &ty, name, scene) != 6)
                continue;
            s_scene_npc_bindings[slot].used = 1;
            s_scene_npc_bindings[slot].npc_idx = -1;
            s_scene_npc_bindings[slot].map_id = (uint8_t)map_id;
            s_scene_npc_bindings[slot].sprite_id = (uint8_t)sprite_id;
            s_scene_npc_bindings[slot].tile_x = tx;
            s_scene_npc_bindings[slot].tile_y = ty;
            snprintf(s_scene_npc_bindings[slot].name, sizeof(s_scene_npc_bindings[slot].name), "%s", name);
            snprintf(s_scene_npc_bindings[slot].scene, sizeof(s_scene_npc_bindings[slot].scene), "%s", scene);
        } else if (strncmp(line, "TILEDEF|", 8) == 0) {
            char tname[32] = {0};
            int slot;
            if (sscanf(line + 8, "%31[^|]|%u|%u|%u|%u|%u|%u|%u|%u",
                       tname, &block_id, &warp_mode, &dest_map, &dest_warp_idx,
                       &t0, &t1, &t2, &t3) != 9) continue;
            slot = scene_saved_tile_alloc(tname);
            if (slot < 0) continue;
            s_scene_saved_tiles[slot].used = 1;
            snprintf(s_scene_saved_tiles[slot].name, sizeof(s_scene_saved_tiles[slot].name), "%s", tname);
            s_scene_saved_tiles[slot].block_id = (uint8_t)block_id;
            s_scene_saved_tiles[slot].warp_mode = (uint8_t)warp_mode;
            s_scene_saved_tiles[slot].dest_map = (uint8_t)dest_map;
            s_scene_saved_tiles[slot].dest_warp_idx = (uint8_t)dest_warp_idx;
            s_scene_saved_tiles[slot].tiles[0] = (uint8_t)t0;
            s_scene_saved_tiles[slot].tiles[1] = (uint8_t)t1;
            s_scene_saved_tiles[slot].tiles[2] = (uint8_t)t2;
            s_scene_saved_tiles[slot].tiles[3] = (uint8_t)t3;
        } else if (strncmp(line, "BLOCKDEF|", 9) == 0) {
            char bname[32] = {0};
            int count = 0, slot;
            if (sscanf(line + 9, "%31[^|]|%d", bname, &count) != 2) continue;
            slot = scene_saved_block_alloc(bname);
            if (slot < 0) continue;
            s_scene_saved_blocks[slot].used = 1;
            snprintf(s_scene_saved_blocks[slot].name, sizeof(s_scene_saved_blocks[slot].name), "%s", bname);
            s_scene_saved_blocks[slot].cell_count = 0;
            (void)count;
        } else if (strncmp(line, "BLOCKCELL|", 10) == 0) {
            char bname[32] = {0};
            int dx = 0, dy = 0, slot;
            if (sscanf(line + 10, "%31[^|]|%d|%d|%u|%u|%u|%u|%u|%u|%u|%u",
                       bname, &dx, &dy, &block_id, &warp_mode, &dest_map,
                       &dest_warp_idx, &t0, &t1, &t2, &t3) != 11) continue;
            slot = scene_saved_block_alloc(bname);
            if (slot < 0 || s_scene_saved_blocks[slot].cell_count >= SCENE_SAVED_BLOCK_CELL_MAX) continue;
            scene_saved_block_t *b = &s_scene_saved_blocks[slot];
            scene_saved_block_cell_t *cell = &b->cells[b->cell_count++];
            b->used = 1;
            snprintf(b->name, sizeof(b->name), "%s", bname);
            cell->dx = dx;
            cell->dy = dy;
            cell->block_id = (uint8_t)block_id;
            cell->warp_mode = (uint8_t)warp_mode;
            cell->dest_map = (uint8_t)dest_map;
            cell->dest_warp_idx = (uint8_t)dest_warp_idx;
            cell->tiles[0] = (uint8_t)t0;
            cell->tiles[1] = (uint8_t)t1;
            cell->tiles[2] = (uint8_t)t2;
            cell->tiles[3] = (uint8_t)t3;
        } else if (strncmp(line, "TILE|", 5) == 0) {
            int slot;
            if (sscanf(line + 5, "%u|%d|%d|%u|%u|%u|%u|%u|%u|%u|%u",
                       &map_id, &tx, &ty, &block_id, &warp_mode, &dest_map,
                       &dest_warp_idx, &t0, &t1, &t2, &t3) != 11) continue;
            slot = scene_tile_prop_find_slot_for_map((uint8_t)map_id, tx, ty);
            if (slot < 0) slot = scene_tile_prop_alloc_slot();
            if (slot < 0) continue;
            s_scene_tile_props[slot].used = 1;
            s_scene_tile_props[slot].banked = 1;
            s_scene_tile_props[slot].map_id = (uint8_t)map_id;
            s_scene_tile_props[slot].x = tx;
            s_scene_tile_props[slot].y = ty;
            s_scene_tile_props[slot].block_id = (uint8_t)block_id;
            s_scene_tile_props[slot].warp_mode = (uint8_t)warp_mode;
            s_scene_tile_props[slot].dest_map = (uint8_t)dest_map;
            s_scene_tile_props[slot].dest_warp_idx = (uint8_t)dest_warp_idx;
            s_scene_tile_props[slot].tiles[0] = (uint8_t)t0;
            s_scene_tile_props[slot].tiles[1] = (uint8_t)t1;
            s_scene_tile_props[slot].tiles[2] = (uint8_t)t2;
            s_scene_tile_props[slot].tiles[3] = (uint8_t)t3;
        }
    }
    fclose(fp);
}

static void dsl_bank_ensure_current_map_spawns(void) {
    for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
        scene_npc_binding_t *b = &s_scene_npc_bindings[i];
        int idx;
        if (!b->used) continue;
        if (b->map_id != wCurMap) continue;
        idx = NPC_FindAtTile(b->tile_x, b->tile_y);
        if (idx < 0) idx = NPC_DebugSpawn(b->sprite_id, b->tile_x, b->tile_y, 0, 0);
        b->npc_idx = idx;
    }
}

static void dsl_bank_ensure_current_map_tiles(void) {
    int any = 0;
    for (int i = 0; i < SCENE_TILE_PROP_MAX; i++) {
        if (s_scene_tile_props[i].used && s_scene_tile_props[i].banked &&
            s_scene_tile_props[i].map_id == wCurMap) {
            any = 1;
            break;
        }
    }
    if (any) Map_BuildScrollView();
}

static void dsl_bank_mark_runtime_tiles_banked(void) {
    for (int i = 0; i < SCENE_TILE_PROP_MAX; i++) {
        if (s_scene_tile_props[i].used)
            s_scene_tile_props[i].banked = 1;
    }
}

static void dsl_bank_init_if_needed(void) {
    FILE *fp;
    if (s_dsl_bank_init_done) return;
    s_dsl_bank_init_done = 1;
    fp = NULL;
    for (int pi = 0; pi < (int)(sizeof(kDslBankCfgPaths) / sizeof(kDslBankCfgPaths[0])); pi++) {
        fp = fopen(kDslBankCfgPaths[pi], "r");
        if (fp) break;
    }
    if (!fp) return;
    {
        int enabled = 0;
        if (fscanf(fp, "%d", &enabled) == 1) s_dsl_bank_enabled = enabled ? 1 : 0;
    }
    fclose(fp);
    if (s_dsl_bank_enabled) {
        dsl_bank_load();
        dsl_bank_ensure_current_map_spawns();
        dsl_bank_ensure_current_map_tiles();
        s_dsl_bank_last_map = wCurMap;
    }
}

static int dsl_parse_bool(const char *s, int default_value) {
    char n[16];
    int j = 0;
    if (!s || !*s) return default_value;
    for (int i = 0; s[i] && j + 1 < (int)sizeof(n); i++) {
        char c = (char)tolower((unsigned char)s[i]);
        if (c == ' ' || c == '\t') continue;
        n[j++] = c;
    }
    n[j] = '\0';
    if (strcmp(n, "1") == 0 || strcmp(n, "true") == 0 || strcmp(n, "yes") == 0 || strcmp(n, "on") == 0) return 1;
    if (strcmp(n, "0") == 0 || strcmp(n, "false") == 0 || strcmp(n, "no") == 0 || strcmp(n, "off") == 0) return 0;
    return default_value;
}

static void dsl_startup_apply_line(const char *line) {
    scene_cmd_t cmd;
    const char *s = line;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0' || *s == '#') return;
    if (!scene_parse_runtime_line(s, &cmd)) return;
    switch (cmd.op) {
        case SCOP_PY_INJECT:
            (void)scene_exec_py_inject(cmd.text);
            break;
        case SCOP_TYPEMOD:
            scene_exec_typemod_line(cmd.text);
            break;
        case SCOP_SPRITE_FRONT_LOAD:
            (void)SpriteMod_LoadFrontFromFile((uint8_t)cmd.a, cmd.text);
            break;
        case SCOP_SPRITE_BACK_LOAD:
            (void)SpriteMod_LoadBackFromFile((uint8_t)cmd.a, cmd.text);
            break;
        default:
            break;
    }
}

static void dsl_startup_run_if_enabled(void) {
    FILE *fp;
    char line[256];
    int enabled = 0;
    char commands_path[220];
    if (s_dsl_startup_checked) return;
    s_dsl_startup_checked = 1;
    snprintf(commands_path, sizeof(commands_path), "%s", DSL_STARTUP_COMMANDS_DEFAULT);

    fp = fopen(DSL_STARTUP_CFG_PATH, "r");
    if (!fp) fp = fopen(DSL_STARTUP_CFG_PATH_LEGACY, "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            char *s = cli_trim(line);
            char key[64] = {0};
            char val[200] = {0};
            if (*s == '\0' || *s == '#') continue;
            scene_normalize_ascii(s);
            if (sscanf(s, "%63[^=]=%199[^\n]", key, val) == 2) {
                char *k = cli_trim(key);
                char *v = cli_trim(val);
                if (strcmp(k, "enabled") == 0 || strcmp(k, "inject_on_startup") == 0)
                    enabled = dsl_parse_bool(v, enabled);
                else if (strcmp(k, "commands_file") == 0 || strcmp(k, "dsl_commands_file") == 0)
                    snprintf(commands_path, sizeof(commands_path), "%s", v);
            }
        }
        fclose(fp);
    }
    if (!enabled) return;

    {
        const char *kPathFmt[] = {
            "%s",
            "../%s",
            "mod_runtime/scenes/%s",
            "../mod_runtime/scenes/%s",
        };
        char resolved[320] = {0};
        for (int i = 0; i < (int)(sizeof(kPathFmt) / sizeof(kPathFmt[0])); i++) {
            FILE *probe;
            snprintf(resolved, sizeof(resolved), kPathFmt[i], commands_path);
            probe = fopen(resolved, "r");
            if (probe) { fclose(probe); break; }
            resolved[0] = '\0';
        }
        if (!resolved[0]) {
            printf("[cli] dsl_startup: commands file not found '%s'\n", commands_path);
            return;
        }
        fp = fopen(resolved, "r");
        if (!fp) return;
        while (fgets(line, sizeof(line), fp)) {
            char *s = cli_trim(line);
            scene_normalize_ascii(s);
            dsl_startup_apply_line(s);
        }
        fclose(fp);
        printf("[cli] dsl_startup: applied commands from %s\n", commands_path);
    }
}

static void scene_track_actor_positions(void) {
    int tx, ty;
    for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
        if (!s_scene_actors[i].used) continue;
        if (s_scene_actors[i].npc_idx < 0 || s_scene_actors[i].npc_idx >= NPC_GetCount()) continue;
        NPC_GetTilePos(s_scene_actors[i].npc_idx, &tx, &ty);
        s_scene_actors[i].last_x = tx;
        s_scene_actors[i].last_y = ty;
    }
}

static void scene_restore_spawned_actors_after_battle(void) {
    int restored_any = 0;
    for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
        int idx;
        if (!s_scene_actors[i].used) continue;
        if (!s_scene_actors[i].spawned_by_scene) continue;
        idx = NPC_DebugSpawn(s_scene_actors[i].sprite_id,
                             s_scene_actors[i].last_x,
                             s_scene_actors[i].last_y,
                             0, 0);
        s_scene_actors[i].npc_idx = idx;
        if (idx >= 0) restored_any = 1;
    }
    if (restored_any) {

        NPC_BuildView(gScrollPxX, gScrollPxY);
    }
}

static void scene_start_trainer_battle(int trainer_class, int trainer_no, const char *defeat_text) {
    extern void Game_StartTrainerBattleScripted(uint8_t trainer_class, uint8_t trainer_no);

    wJoyIgnore = 0;
    gScriptedMovement = 0;
    gEngagedTrainerClass = (uint8_t)trainer_class;
    gEngagedTrainerNo = (uint8_t)trainer_no;

    if (defeat_text && *defeat_text)
        Trainer_SetDefeatText(trainer_class, defeat_text);
    else
        gTrainerAfterText = NULL;
    Game_StartTrainerBattleScripted((uint8_t)trainer_class, (uint8_t)trainer_no);
}

static void scene_start_custom_trainer_battle(const scene_cmd_t *cmd) {
    extern void Game_StartCustomTrainerBattleScripted(uint8_t trainer_class,
                                                      uint8_t music_id,
                                                      const uint8_t species[6],
                                                      const uint8_t level[6],
                                                      const uint8_t moves[6][4],
                                                      uint8_t count);
    if (!cmd) return;
    wJoyIgnore = 0;
    gScriptedMovement = 0;
    gTrainerAfterText = (cmd->text[0] != '\0') ? cmd->text : NULL;
    Game_StartCustomTrainerBattleScripted((uint8_t)cmd->a, (uint8_t)cmd->b, cmd->team_species, cmd->team_level, cmd->team_moves, cmd->team_count);
}

static int scene_pick_random_map_trainer(int *out_class, int *out_no) {
    const map_events_t *ev;
    int idx;
    if (!out_class || !out_no) return 0;
    if (wCurMap >= NUM_MAPS) return 0;
    ev = &gMapEvents[wCurMap];
    if (!ev->trainers || ev->num_trainers == 0) return 0;
    idx = rand() % ev->num_trainers;
    *out_class = ev->trainers[idx].trainer_class;
    *out_no = ev->trainers[idx].trainer_no;
    return 1;
}

static int scene_load_file(const char *name) {
    char path[192];
    FILE *fp = NULL;
    char line[1024];
    int lineno = 0;
    static const char *kScenePaths[] = {
        "mod_runtime/scenes/%s.scene",
        "../mod_runtime/scenes/%s.scene",
        "build/mod_runtime/scenes/%s.scene",
        "debug/scenes/%s.scene",
        "../debug/scenes/%s.scene",
        "build/debug/scenes/%s.scene",
        "bugs/scenes/%s.scene",
        "../bugs/scenes/%s.scene",
        "build/bugs/scenes/%s.scene"
    };
    s_scene_cmd_count = 0;
    for (int i = 0; i < (int)(sizeof(kScenePaths)/sizeof(kScenePaths[0])); i++) {
        snprintf(path, sizeof(path), kScenePaths[i], name);
        fp = fopen(path, "r");
        if (fp) break;
    }
    if (!fp) return -1;
    memset(s_scene_defs, 0, sizeof(s_scene_defs));
    while (fgets(line, sizeof(line), fp)) {
        char *s = cli_trim(line);
        scene_normalize_ascii(s);
        scene_cmd_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        lineno++;
        if (*s == '\0' || *s == '#') continue;
        if (s_scene_cmd_count >= SCENE_CMD_MAX) break;

        if (strncmp(s, "include ", 8) == 0) {
            char inc[64] = {0};
            FILE *ifp = NULL;
            char ipath[192];
            if (sscanf(s + 8, "%63s", inc) == 1) {
                static const char *kScenePaths[] = {
                    "mod_runtime/scenes/%s.scene",
                    "../mod_runtime/scenes/%s.scene",
                    "build/mod_runtime/scenes/%s.scene",
                    "debug/scenes/%s.scene",
                    "../debug/scenes/%s.scene",
                    "build/debug/scenes/%s.scene",
                    "bugs/scenes/%s.scene",
                    "../bugs/scenes/%s.scene",
                    "build/bugs/scenes/%s.scene"
                };
                for (int pi = 0; pi < (int)(sizeof(kScenePaths)/sizeof(kScenePaths[0])); pi++) {
                    snprintf(ipath, sizeof(ipath), kScenePaths[pi], inc);
                    ifp = fopen(ipath, "r");
                    if (ifp) break;
                }
                if (ifp) {
                    while (fgets(line, sizeof(line), ifp)) {
                        char *ds = cli_trim(line);
                        scene_normalize_ascii(ds);
                        if (strncmp(ds, "def ", 4) != 0) continue;
                        {
                            char defname[32] = {0};
                            int didx;
                            if (sscanf(ds + 4, "%31s", defname) != 1) continue;
                            didx = scene_defs_add(defname);
                            if (didx < 0) continue;
                            while (fgets(line, sizeof(line), ifp)) {
                                char *dline = cli_trim(line);
                                scene_normalize_ascii(dline);
                                if (strcmp(dline, "enddef") == 0) break;
                                if (*dline == '\0' || *dline == '#') continue;
                                if (s_scene_defs[didx].line_count < SCENE_DEF_LINE_MAX) {
                                    snprintf(s_scene_defs[didx].lines[s_scene_defs[didx].line_count],
                                             sizeof(s_scene_defs[didx].lines[0]), "%s", dline);
                                    s_scene_defs[didx].line_count++;
                                }
                            }
                        }
                    }
                    fclose(ifp);
                } else {
                    printf("[scene] include not found: %s\n", inc);
                }
            }
            continue;
        }

        if (strncmp(s, "def ", 4) == 0) {
            char defname[32] = {0};
            int didx;
            if (sscanf(s + 4, "%31s", defname) != 1) continue;
            didx = scene_defs_add(defname);
            if (didx < 0) continue;
            while (fgets(line, sizeof(line), fp)) {
                char *ds = cli_trim(line);
                scene_normalize_ascii(ds);
                lineno++;
                if (strcmp(ds, "enddef") == 0) break;
                if (*ds == '\0' || *ds == '#') continue;
                if (s_scene_defs[didx].line_count < SCENE_DEF_LINE_MAX) {
                    snprintf(s_scene_defs[didx].lines[s_scene_defs[didx].line_count],
                             sizeof(s_scene_defs[didx].lines[0]), "%s", ds);
                    s_scene_defs[didx].line_count++;
                }
            }
            continue;
        }

        if (strncmp(s, "use ", 4) == 0) {
            char defname[32] = {0};
            char args[16][96];
            char expanded[1024];
            char toks[17][96];
            char usebuf[1024];
            char use_music[96] = {0};
            int didx;
            memset(args, 0, sizeof(args));
            memset(toks, 0, sizeof(toks));
            {
                int nt;
                snprintf(usebuf, sizeof(usebuf), "%s", s + 4);

                while (1) {
                    long pos = ftell(fp);
                    char cont_line[1024];
                    char *ct;
                    if (!fgets(cont_line, sizeof(cont_line), fp)) break;
                    ct = cli_trim(cont_line);
                    scene_normalize_ascii(ct);
                    if (*ct == '\0' || *ct == '#') continue;
                    if (strncmp(ct, "music ", 6) == 0) {
                        const char *mv = ct + 6;
                        while (*mv == ' ' || *mv == '\t') mv++;
                        if (*mv) {
                            snprintf(use_music, sizeof(use_music), "%s", mv);
                        }
                        continue;
                    }
                    if (scene_is_line_command_start(ct)) {
                        fseek(fp, pos, SEEK_SET);
                        break;
                    }
                    if ((int)strlen(usebuf) + 1 + (int)strlen(ct) < (int)sizeof(usebuf)) {
                        strcat(usebuf, " ");
                        strcat(usebuf, ct);
                    }
                }
                nt = scene_split_args_quoted(usebuf, toks, 17);
                if (nt < 1) continue;
                snprintf(defname, sizeof(defname), "%s", toks[0]);
                for (int ti = 1; ti < nt && ti <= 16; ti++)
                    snprintf(args[ti - 1], sizeof(args[ti - 1]), "%s", toks[ti]);
                if (use_music[0] != '\0') {

                    for (int ai = 15; ai >= 2; ai--)
                        snprintf(args[ai], sizeof(args[ai]), "%s", args[ai - 1]);
                    snprintf(args[1], sizeof(args[1]), "%s", use_music);
                }
                if (strcmp(defname, "battle_intro_custom") == 0) {
                    printf("[scene] use battle_intro_custom args: $1='%s' $2='%s' $3='%s' $10='%s' $11='%s'\n",
                           args[0], args[1], args[2], args[9], args[10]);
                }
            }
            didx = scene_defs_find(defname);
            if (didx < 0) {
                printf("[scene] unknown def '%s' at line %d\n", defname, lineno);
                continue;
            }
            for (int li = 0; li < s_scene_defs[didx].line_count; li++) {
                s = s_scene_defs[didx].lines[li];
                scene_apply_args(s, expanded, sizeof(expanded), args);
                s = expanded;

                memset(&cmd, 0, sizeof(cmd));
                if (strncmp(s, "spawn ", 6) == 0) {
                    char id[24], sprite[32], x[32], y[32];
                    if (sscanf(s + 6, "%23s %31s %31s %31s", id, sprite, x, y) != 4) continue;
                    cmd.op = SCOP_SPAWN;
                    snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
                    cmd.a = scene_parse_sprite(sprite);
                    cmd.b = cli_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
                    cmd.c = cli_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
                    if (!cli_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) cmd.b = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
                    if (!cli_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) cmd.c = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
                } else if (strncmp(s, "despawn ", 8) == 0) {
                    cmd.op = SCOP_DESPAWN;
                    sscanf(s + 8, "%23s", cmd.actor);
                } else if (strncmp(s, "face ", 5) == 0) {
                    char id[24], dir[24];
                    if (sscanf(s + 5, "%23s %23s", id, dir) != 2) continue;
                    cmd.op = SCOP_FACE;
                    snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
                    if (strcmp(dir, "player") == 0) cmd.a = -2; else cmd.a = scene_parse_dir(dir);
                } else if (strncmp(s, "move ", 5) == 0) {
                    char id[24], dir[24], steps[24];
                    if (sscanf(s + 5, "%23s %23s %23s", id, dir, steps) != 3) continue;
                    cmd.op = SCOP_MOVE;
                    snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
                    cmd.a = scene_parse_dir(dir);
                    cmd.b = (int)strtol(steps, NULL, 0);
                } else if (strncmp(s, "say ", 4) == 0) {
                    cmd.op = SCOP_SAY;
                    snprintf(cmd.text, sizeof(cmd.text), "%s", s + 4);
                    {
                        size_t n = strlen(cmd.text);
                        if (n >= 2 && cmd.text[0] == '"' && cmd.text[n - 1] == '"') {
                            memmove(cmd.text, cmd.text + 1, n - 2);
                            cmd.text[n - 2] = '\0';
                        }
                    }
                    scene_unescape_text(cmd.text);
                    scene_format_dialog_text(cmd.text);
                } else if (strncmp(s, "ask ", 4) == 0) {
                    cmd.op = SCOP_ASK;
                    snprintf(cmd.text, sizeof(cmd.text), "%s", s + 4);
                    {
                        size_t n = strlen(cmd.text);
                        if (n >= 2 && cmd.text[0] == '"' && cmd.text[n - 1] == '"') {
                            memmove(cmd.text, cmd.text + 1, n - 2);
                            cmd.text[n - 2] = '\0';
                        }
                    }
                    scene_unescape_text(cmd.text);
                    scene_format_dialog_text(cmd.text);
                } else if (strncmp(s, "battlestart", 11) == 0) {
                    char btoks[10][96];
                    int nt = scene_split_args_quoted(s + 11, btoks, 10);
                    memset(&cmd, 0, sizeof(cmd));
                    cmd.op = SCOP_BATTLESTART;
                    cmd.a = 34; cmd.b = 1;
                    if (nt >= 1 && strcmp(btoks[0], "custom") == 0) {
                        int slot_base = 1;
                        int ok = 1;
                        cmd.c = 1;
                        cmd.b = MUSIC_TRAINER_BATTLE;
                        if (nt > 1) {
                            int mid = scene_parse_music_track(btoks[1]);
                            if (mid > 0) {
                                cmd.b = mid;
                                slot_base = 2;
                            }
                        }
                        if (nt > slot_base) {
                            int tc = cli_resolve_trainer_class_id(btoks[slot_base]);
                            if (tc > 0) {
                                cmd.a = tc;
                                slot_base++;
                            }
                        }
                        for (int si = 0; si < 6; si++) {
                            int tix = slot_base + si;
                            const char *slot = (tix < nt) ? btoks[tix] : "empty";
                            if (!scene_parse_team_slot(slot,
                                                       &cmd.team_species[si],
                                                       &cmd.team_level[si],
                                                       cmd.team_moves[si])) {
                                ok = 0;
                                break;
                            }
                            if (cmd.team_species[si] != 0) cmd.team_count = (uint8_t)(si + 1);
                        }
                        if (nt > slot_base + 6) {
                            snprintf(cmd.text, sizeof(cmd.text), "%s", btoks[slot_base + 6]);
                            scene_unescape_text(cmd.text);
                            scene_format_dialog_text(cmd.text);
                        }
                        if (!ok) {
                            printf("[scene] bad custom battlestart spec near '%s' (nt=%d slot_base=%d)\n",
                                   s, nt, slot_base);
                            continue;
                        }
                        printf("[scene] battlestart custom parsed: music=%d trainer=%d team_count=%u\n",
                               cmd.b, cmd.a, (unsigned)cmd.team_count);
                    } else if (nt >= 1) {
                        if (strcmp(btoks[0], "random") == 0) { cmd.a = -1; cmd.b = 0; }
                        else if (cli_is_numeric_token(btoks[0])) {
                            cmd.a = (int)strtol(btoks[0], NULL, 0);
                            if (nt >= 2 && cli_is_numeric_token(btoks[1]))
                                cmd.b = (int)strtol(btoks[1], NULL, 0);
                        }
                        if (nt >= 3) {
                            snprintf(cmd.text, sizeof(cmd.text), "%s", btoks[2]);
                            scene_unescape_text(cmd.text);
                            scene_format_dialog_text(cmd.text);
                        }
                    }
                } else if (strncmp(s, "battlend", 8) == 0) {
                    cmd.op = SCOP_BATTLEEND;
                    if (sscanf(s + 8, " %159[^\n]", cmd.text) != 1) snprintf(cmd.text, sizeof(cmd.text), "Battle complete.@");
                    {
                        size_t n = strlen(cmd.text);
                        if (n >= 2 && cmd.text[0] == '"' && cmd.text[n - 1] == '"') {
                            memmove(cmd.text, cmd.text + 1, n - 2);
                            cmd.text[n - 2] = '\0';
                        }
                    }
                    scene_unescape_text(cmd.text);
                    scene_format_dialog_text(cmd.text);
                } else if (strncmp(s, "music ", 6) == 0) {
                    int mid = scene_parse_music_track(s + 6);
                    if (mid < 0) continue;
                    cmd.op = SCOP_MUSIC;
                    cmd.a = mid;
                } else if (strncmp(s, "wait ", 5) == 0) { cmd.op = SCOP_WAIT; cmd.a = (int)strtol(s + 5, NULL, 0); }
                else if (strcmp(s, "wait_text") == 0) { cmd.op = SCOP_WAIT_TEXT; }
                else if (strcmp(s, "lock_input on") == 0) { cmd.op = SCOP_LOCK_INPUT; cmd.a = 1; }
                else if (strcmp(s, "lock_input off") == 0) { cmd.op = SCOP_LOCK_INPUT; cmd.a = 0; }
                else if (strncmp(s, "tile_copy ", 10) == 0 || strncmp(s, "copy_tile ", 10) == 0) {
                    const char *args4 = s + 10;
                    char norm[192], x1[32], y1[32], x2[32], y2[32];
                    scene_normalize_coord_args(args4, norm, sizeof(norm));
                    if (sscanf(norm, "%31s %31s %31s %31s", x1, y1, x2, y2) != 4) continue;
                    if (!scene_parse_coord_expr(x1, 1, &cmd.a) || !scene_parse_coord_expr(y1, 0, &cmd.b) ||
                        !scene_parse_coord_expr(x2, 1, &cmd.c) || !scene_parse_coord_expr(y2, 0, &cmd.d)) continue;
                    cmd.op = SCOP_TILE_COPY;
                }
                else if (strncmp(s, "tile_save ", 10) == 0) {
                    cmd.op = SCOP_TILE_SAVE;
                    sscanf(s + 10, "%31s", cmd.text);
                }
                else if (strncmp(s, "tile_place_custom ", 18) == 0) {
                    if (!scene_parse_named_coord_args(s + 18, cmd.text, sizeof(cmd.text), &cmd.a, &cmd.b)) continue;
                    cmd.op = SCOP_TILE_PLACE_CUSTOM;
                }
                else if (strncmp(s, "block_save ", 11) == 0) {
                    if (!scene_parse_block_save_args(s + 11, cmd.text, sizeof(cmd.text), &cmd.a, &cmd.b, &cmd.c, &cmd.d)) continue;
                    cmd.op = SCOP_BLOCK_SAVE;
                }
                else if (strncmp(s, "block_place_custom ", 19) == 0) {
                    if (!scene_parse_named_coord_args(s + 19, cmd.text, sizeof(cmd.text), &cmd.a, &cmd.b)) continue;
                    cmd.op = SCOP_BLOCK_PLACE_CUSTOM;
                }
                else if (strncmp(s, "py_ai ", 6) == 0) {
                    char mode[16] = {0};
                    char script[160] = {0};
                    if (sscanf(s + 6, "%15s %159[^\n]", mode, script) < 1) continue;
                    cmd.op = SCOP_PY_AI;
                    cmd.a = (strcmp(mode, "on") == 0) ? 1 : 0;
                    if (script[0]) snprintf(cmd.text, sizeof(cmd.text), "%s", script);
                }
                else if (strncmp(s, "py_inject ", 10) == 0) {
                    cmd.op = SCOP_PY_INJECT;
                    snprintf(cmd.text, sizeof(cmd.text), "%s", s + 10);
                }
                else if (strncmp(s, "py_law ", 7) == 0) {
                    char mode[16] = {0};
                    int idx = -1;
                    char script[120] = {0};
                    if (sscanf(s + 7, "%15s %d %119s", mode, &idx, script) < 1) continue;
                    cmd.op = SCOP_PY_LAW;
                    cmd.a = (strcmp(mode, "on") == 0) ? 1 : 0;
                    cmd.b = idx;
                    if (script[0]) snprintf(cmd.text, sizeof(cmd.text), "%s", script);
                } else if (strncmp(s, "py_law_spawn ", 13) == 0) {
                    char sprite[24] = {0}, x[32] = {0}, y[32] = {0}, script[120] = {0};
                    if (sscanf(s + 13, "%23s %31s %31s %119s", sprite, x, y, script) != 4) continue;
                    cmd.op = SCOP_PY_LAW_SPAWN;
                    cmd.a = scene_parse_sprite(sprite);
                    cmd.b = cli_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
                    cmd.c = cli_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
                    if (!cli_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) cmd.b = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
                    if (!cli_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) cmd.c = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
                    snprintf(cmd.text, sizeof(cmd.text), "%s", script);
                }
                else if (strncmp(s, "type_define ", 12) == 0 ||
                         strncmp(s, "type_chart_set ", 15) == 0 ||
                         strncmp(s, "species_set_types ", 18) == 0 ||
                         strncmp(s, "species_define ", 15) == 0 ||
                         strncmp(s, "species_name_set ", 17) == 0 ||
                         strncmp(s, "species_stats_set ", 18) == 0 ||
                         strncmp(s, "species_moves_set ", 18) == 0 ||
                         strncmp(s, "species_learn_add ", 18) == 0 ||
                         strncmp(s, "species_bank ", 13) == 0 ||
                         strncmp(s, "type_bank ", 10) == 0) {
                    cmd.op = SCOP_TYPEMOD;
                    snprintf(cmd.text, sizeof(cmd.text), "%s", s);
                } else if (strncmp(s, "sprite_front_load ", 18) == 0) {
                    char sp[32], path[140];
                    int sid;
                    if (sscanf(s + 18, "%31s %139s", sp, path) != 2) continue;
                    sid = cli_resolve_species_id(sp);
                    if (sid <= 0) continue;
                    cmd.op = SCOP_SPRITE_FRONT_LOAD;
                    cmd.a = sid;
                    snprintf(cmd.text, sizeof(cmd.text), "%s", path);
                } else if (strncmp(s, "sprite_back_load ", 17) == 0) {
                    char sp[32], path[140];
                    int sid;
                    if (sscanf(s + 17, "%31s %139s", sp, path) != 2) continue;
                    sid = cli_resolve_species_id(sp);
                    if (sid <= 0) continue;
                    cmd.op = SCOP_SPRITE_BACK_LOAD;
                    cmd.a = sid;
                    snprintf(cmd.text, sizeof(cmd.text), "%s", path);
                }
                else if (strcmp(s, "end") == 0) { cmd.op = SCOP_END; }
                else continue;

                if (s_scene_cmd_count < SCENE_CMD_MAX) s_scene_cmds[s_scene_cmd_count++] = cmd;
            }
            continue;
        }

        if (strncmp(s, "spawn ", 6) == 0) {
            char id[24], sprite[32], x[32], y[32];
            if (sscanf(s + 6, "%23s %31s %31s %31s", id, sprite, x, y) != 4) continue;
            cmd.op = SCOP_SPAWN;
            snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
            cmd.a = scene_parse_sprite(sprite);
            cmd.b = cli_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
            cmd.c = cli_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
            if (!cli_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) cmd.b = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
            if (!cli_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) cmd.c = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
        } else if (strncmp(s, "despawn ", 8) == 0) {
            cmd.op = SCOP_DESPAWN;
            sscanf(s + 8, "%23s", cmd.actor);
        } else if (strncmp(s, "face ", 5) == 0) {
            char id[24], dir[24];
            if (sscanf(s + 5, "%23s %23s", id, dir) != 2) continue;
            cmd.op = SCOP_FACE;
            snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
            if (strcmp(dir, "player") == 0) cmd.a = -2; else cmd.a = scene_parse_dir(dir);
        } else if (strncmp(s, "move ", 5) == 0) {
            char id[24], dir[24], steps[24];
            if (sscanf(s + 5, "%23s %23s %23s", id, dir, steps) != 3) continue;
            cmd.op = SCOP_MOVE;
            snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
            cmd.a = scene_parse_dir(dir);
            cmd.b = (int)strtol(steps, NULL, 0);
        } else if (strncmp(s, "say ", 4) == 0) {
            cmd.op = SCOP_SAY;
            snprintf(cmd.text, sizeof(cmd.text), "%s", s + 4);
            {
                size_t n = strlen(cmd.text);
                if (n >= 2 && cmd.text[0] == '"' && cmd.text[n - 1] == '"') {
                    memmove(cmd.text, cmd.text + 1, n - 2);
                    cmd.text[n - 2] = '\0';
                }
            }
            scene_unescape_text(cmd.text);
            scene_format_dialog_text(cmd.text);
        } else if (strncmp(s, "ask ", 4) == 0) {
            cmd.op = SCOP_ASK;
            snprintf(cmd.text, sizeof(cmd.text), "%s", s + 4);
            {
                size_t n = strlen(cmd.text);
                if (n >= 2 && cmd.text[0] == '"' && cmd.text[n - 1] == '"') {
                    memmove(cmd.text, cmd.text + 1, n - 2);
                    cmd.text[n - 2] = '\0';
                }
            }
            scene_unescape_text(cmd.text);
            scene_format_dialog_text(cmd.text);
        } else if (strncmp(s, "battlestart", 11) == 0 || strncmp(s, "battlend", 8) == 0) {
            printf("[scene] %s:%d raw battle ops are disallowed at top-level; use a reusable def (e.g. include defs_battle + use battle_intro ...)\n",
                   path, lineno);
            fclose(fp);
            return -2;
        } else if (strncmp(s, "music ", 6) == 0) {
            int mid = scene_parse_music_track(s + 6);
            if (mid < 0) continue;
            cmd.op = SCOP_MUSIC;
            cmd.a = mid;
        } else if (strncmp(s, "wait ", 5) == 0) {
            cmd.op = SCOP_WAIT;
            cmd.a = (int)strtol(s + 5, NULL, 0);
        } else if (strcmp(s, "wait_text") == 0) {
            cmd.op = SCOP_WAIT_TEXT;
        } else if (strcmp(s, "lock_input on") == 0) {
            cmd.op = SCOP_LOCK_INPUT; cmd.a = 1;
        } else if (strcmp(s, "lock_input off") == 0) {
            cmd.op = SCOP_LOCK_INPUT; cmd.a = 0;
        } else if (strncmp(s, "tile_copy ", 10) == 0 || strncmp(s, "copy_tile ", 10) == 0) {
            const char *args4 = s + 10;
            char norm[192], x1[32], y1[32], x2[32], y2[32];
            scene_normalize_coord_args(args4, norm, sizeof(norm));
            if (sscanf(norm, "%31s %31s %31s %31s", x1, y1, x2, y2) != 4) continue;
            if (!scene_parse_coord_expr(x1, 1, &cmd.a) || !scene_parse_coord_expr(y1, 0, &cmd.b) ||
                !scene_parse_coord_expr(x2, 1, &cmd.c) || !scene_parse_coord_expr(y2, 0, &cmd.d)) continue;
            cmd.op = SCOP_TILE_COPY;
        } else if (strncmp(s, "tile_save ", 10) == 0) {
            cmd.op = SCOP_TILE_SAVE;
            sscanf(s + 10, "%31s", cmd.text);
        } else if (strncmp(s, "tile_place_custom ", 18) == 0) {
            if (!scene_parse_named_coord_args(s + 18, cmd.text, sizeof(cmd.text), &cmd.a, &cmd.b)) continue;
            cmd.op = SCOP_TILE_PLACE_CUSTOM;
        } else if (strncmp(s, "block_save ", 11) == 0) {
            if (!scene_parse_block_save_args(s + 11, cmd.text, sizeof(cmd.text), &cmd.a, &cmd.b, &cmd.c, &cmd.d)) continue;
            cmd.op = SCOP_BLOCK_SAVE;
        } else if (strncmp(s, "block_place_custom ", 19) == 0) {
            if (!scene_parse_named_coord_args(s + 19, cmd.text, sizeof(cmd.text), &cmd.a, &cmd.b)) continue;
            cmd.op = SCOP_BLOCK_PLACE_CUSTOM;
        } else if (strncmp(s, "py_ai ", 6) == 0) {
            char mode[16] = {0};
            char script[160] = {0};
            if (sscanf(s + 6, "%15s %159[^\n]", mode, script) < 1) continue;
            cmd.op = SCOP_PY_AI;
            cmd.a = (strcmp(mode, "on") == 0) ? 1 : 0;
            if (script[0]) snprintf(cmd.text, sizeof(cmd.text), "%s", script);
        } else if (strncmp(s, "py_inject ", 10) == 0) {
            cmd.op = SCOP_PY_INJECT;
            snprintf(cmd.text, sizeof(cmd.text), "%s", s + 10);
        } else if (strncmp(s, "py_law ", 7) == 0) {
            char mode[16] = {0};
            int idx = -1;
            char script[120] = {0};
            if (sscanf(s + 7, "%15s %d %119s", mode, &idx, script) < 1) continue;
            cmd.op = SCOP_PY_LAW;
            cmd.a = (strcmp(mode, "on") == 0) ? 1 : 0;
            cmd.b = idx;
            if (script[0]) snprintf(cmd.text, sizeof(cmd.text), "%s", script);
        } else if (strncmp(s, "py_law_spawn ", 13) == 0) {
            char sprite[24] = {0}, x[32] = {0}, y[32] = {0}, script[120] = {0};
            if (sscanf(s + 13, "%23s %31s %31s %119s", sprite, x, y, script) != 4) continue;
            cmd.op = SCOP_PY_LAW_SPAWN;
            cmd.a = scene_parse_sprite(sprite);
            cmd.b = cli_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
            cmd.c = cli_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
            if (!cli_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) cmd.b = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
            if (!cli_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) cmd.c = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
            snprintf(cmd.text, sizeof(cmd.text), "%s", script);
        } else if (strncmp(s, "type_define ", 12) == 0 ||
                   strncmp(s, "type_chart_set ", 15) == 0 ||
                   strncmp(s, "species_set_types ", 18) == 0 ||
                   strncmp(s, "species_define ", 15) == 0 ||
                   strncmp(s, "species_name_set ", 17) == 0 ||
                   strncmp(s, "species_stats_set ", 18) == 0 ||
                   strncmp(s, "species_moves_set ", 18) == 0 ||
                   strncmp(s, "species_learn_add ", 18) == 0 ||
                   strncmp(s, "species_bank ", 13) == 0 ||
                   strncmp(s, "type_bank ", 10) == 0) {
            cmd.op = SCOP_TYPEMOD;
            snprintf(cmd.text, sizeof(cmd.text), "%s", s);
        } else if (strncmp(s, "sprite_front_load ", 18) == 0) {
            char sp[32], path[140];
            int sid;
            if (sscanf(s + 18, "%31s %139s", sp, path) != 2) continue;
            sid = cli_resolve_species_id(sp);
            if (sid <= 0) continue;
            cmd.op = SCOP_SPRITE_FRONT_LOAD;
            cmd.a = sid;
            snprintf(cmd.text, sizeof(cmd.text), "%s", path);
        } else if (strncmp(s, "sprite_back_load ", 17) == 0) {
            char sp[32], path[140];
            int sid;
            if (sscanf(s + 17, "%31s %139s", sp, path) != 2) continue;
            sid = cli_resolve_species_id(sp);
            if (sid <= 0) continue;
            cmd.op = SCOP_SPRITE_BACK_LOAD;
            cmd.a = sid;
            snprintf(cmd.text, sizeof(cmd.text), "%s", path);
        } else if (strcmp(s, "end") == 0) {
            cmd.op = SCOP_END;
        } else {
            continue;
        }
        s_scene_cmds[s_scene_cmd_count++] = cmd;
    }
    fclose(fp);
    return s_scene_cmd_count;
}

static int scene_trigger_find_slot(const char *scene, int map_id, int x, int y) {
    for (int i = 0; i < SCENE_TRIGGER_MAX; i++) {
        if (!s_scene_triggers[i].used) continue;
        if (strcmp(s_scene_triggers[i].scene, scene) == 0 &&
            (int)s_scene_triggers[i].map_id == map_id &&
            s_scene_triggers[i].x == x &&
            s_scene_triggers[i].y == y) return i;
    }
    return -1;
}

static int scene_trigger_alloc_slot(void) {
    for (int i = 0; i < SCENE_TRIGGER_MAX; i++) if (!s_scene_triggers[i].used) return i;
    return -1;
}

static int scene_tile_prop_find_slot_for_map(uint8_t map_id, int x, int y) {
    for (int i = 0; i < SCENE_TILE_PROP_MAX; i++) {
        if (!s_scene_tile_props[i].used) continue;
        if (s_scene_tile_props[i].map_id == map_id &&
            s_scene_tile_props[i].x == x &&
            s_scene_tile_props[i].y == y) return i;
    }
    return -1;
}

static int scene_tile_prop_find_slot(int x, int y) {
    return scene_tile_prop_find_slot_for_map(wCurMap, x, y);
}

static int scene_tile_prop_alloc_slot(void) {
    for (int i = 0; i < SCENE_TILE_PROP_MAX; i++) {
        if (!s_scene_tile_props[i].used) return i;
    }
    return -1;
}

static int scene_saved_tile_find(const char *name) {
    for (int i = 0; i < SCENE_SAVED_TILE_MAX; i++) {
        if (s_scene_saved_tiles[i].used && strcmp(s_scene_saved_tiles[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int scene_saved_tile_alloc(const char *name) {
    int slot = scene_saved_tile_find(name);
    if (slot >= 0) return slot;
    for (int i = 0; i < SCENE_SAVED_TILE_MAX; i++) {
        if (!s_scene_saved_tiles[i].used) return i;
    }
    return -1;
}

static int scene_saved_block_find(const char *name) {
    for (int i = 0; i < SCENE_SAVED_BLOCK_MAX; i++) {
        if (s_scene_saved_blocks[i].used && strcmp(s_scene_saved_blocks[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int scene_saved_block_alloc(const char *name) {
    int slot = scene_saved_block_find(name);
    if (slot >= 0) return slot;
    for (int i = 0; i < SCENE_SAVED_BLOCK_MAX; i++) {
        if (!s_scene_saved_blocks[i].used) return i;
    }
    return -1;
}

static int scene_game_coord_in_bounds(int x, int y) {
    return x >= 0 && y >= 0 &&
           x < (int)wCurMapWidth * 2 &&
           y < (int)wCurMapHeight * 2;
}

static int scene_get_canonical_warp_at(int x, int y, uint8_t *dest_map, uint8_t *dest_warp_idx) {
    if (wCurMap >= NUM_MAPS) return 0;
    const map_events_t *ev = &gMapEvents[wCurMap];
    if (!ev->warps) return 0;
    for (int i = 0; i < ev->num_warps; i++) {
        if (ev->warps[i].x == x && ev->warps[i].y == y) {
            if (dest_map) *dest_map = ev->warps[i].dest_map;
            if (dest_warp_idx) *dest_warp_idx = ev->warps[i].dest_warp_idx;
            return 1;
        }
    }
    return 0;
}

static int scene_get_effective_warp_at(int x, int y, uint8_t *has_warp, uint8_t *dest_map, uint8_t *dest_warp_idx) {
    int slot = scene_tile_prop_find_slot(x, y);
    if (slot >= 0) {
        if (has_warp) *has_warp = s_scene_tile_props[slot].warp_mode ? 1 : 0;
        if (dest_map) *dest_map = s_scene_tile_props[slot].dest_map;
        if (dest_warp_idx) *dest_warp_idx = s_scene_tile_props[slot].dest_warp_idx;
        return 1;
    }
    if (scene_get_canonical_warp_at(x, y, dest_map, dest_warp_idx)) {
        if (has_warp) *has_warp = 1;
        return 1;
    }
    if (has_warp) *has_warp = 0;
    if (dest_map) *dest_map = 0;
    if (dest_warp_idx) *dest_warp_idx = 0;
    return 1;
}

static int scene_tile_capture_at(int x, int y, scene_saved_tile_t *out) {
    uint8_t has_warp = 0, dest_map = 0, dest_warp_idx = 0;
    if (!out || !scene_game_coord_in_bounds(x, y)) return 0;
    memset(out, 0, sizeof(*out));
    out->block_id = Map_GetBlockAt(x, y);
    out->tiles[0] = Map_GetTile(x * 2,     y * 2);
    out->tiles[1] = Map_GetTile(x * 2 + 1, y * 2);
    out->tiles[2] = Map_GetTile(x * 2,     y * 2 + 1);
    out->tiles[3] = Map_GetTile(x * 2 + 1, y * 2 + 1);
    scene_get_effective_warp_at(x, y, &has_warp, &dest_map, &dest_warp_idx);
    out->warp_mode = has_warp ? 1 : 0;
    out->dest_map = dest_map;
    out->dest_warp_idx = dest_warp_idx;
    return 1;
}

static int scene_tile_place_data(const scene_saved_tile_t *data, int x, int y) {
    int slot;
    if (!data || !scene_game_coord_in_bounds(x, y)) return 0;
    slot = scene_tile_prop_find_slot(x, y);
    if (slot < 0) slot = scene_tile_prop_alloc_slot();
    if (slot < 0) return 0;
    s_scene_tile_props[slot].used = 1;
    s_scene_tile_props[slot].banked = s_dsl_bank_enabled ? 1 : 0;
    s_scene_tile_props[slot].map_id = wCurMap;
    s_scene_tile_props[slot].x = x;
    s_scene_tile_props[slot].y = y;
    s_scene_tile_props[slot].block_id = data->block_id;
    s_scene_tile_props[slot].warp_mode = data->warp_mode ? 1 : 0;
    s_scene_tile_props[slot].dest_map = data->dest_map;
    s_scene_tile_props[slot].dest_warp_idx = data->dest_warp_idx;
    memcpy(s_scene_tile_props[slot].tiles, data->tiles, sizeof(data->tiles));
    return 1;
}

static int scene_tile_copy(int sx, int sy, int dx, int dy) {
    scene_saved_tile_t tile;
    if (!scene_tile_capture_at(sx, sy, &tile)) return 0;
    if (!scene_tile_place_data(&tile, dx, dy)) return 0;
    Map_BuildScrollView();
    if (s_dsl_bank_enabled) dsl_bank_save();
    return 1;
}

static int scene_tile_save_right_of_player(const char *name) {
    int slot;
    if (!name || !*name) return 0;
    slot = scene_saved_tile_alloc(name);
    if (slot < 0) return 0;
    if (!scene_tile_capture_at((int)wXCoord + 1, (int)wYCoord, &s_scene_saved_tiles[slot])) return 0;
    s_scene_saved_tiles[slot].used = 1;
    snprintf(s_scene_saved_tiles[slot].name, sizeof(s_scene_saved_tiles[slot].name), "%s", name);
    if (s_dsl_bank_enabled) dsl_bank_save();
    return 1;
}

static int scene_tile_place_custom(const char *name, int x, int y) {
    int slot = scene_saved_tile_find(name);
    if (slot < 0) return 0;
    if (!scene_tile_place_data(&s_scene_saved_tiles[slot], x, y)) return 0;
    Map_BuildScrollView();
    if (s_dsl_bank_enabled) dsl_bank_save();
    return 1;
}

static int scene_block_save(const char *name, int sx, int sy, int ex, int ey) {
    int min_x = sx < ex ? sx : ex;
    int max_x = sx > ex ? sx : ex;
    int min_y = sy < ey ? sy : ey;
    int max_y = sy > ey ? sy : ey;
    int slot, count = 0;
    scene_saved_block_t tmp;
    if (!name || !*name) return 0;
    if ((max_x - min_x + 1) * (max_y - min_y + 1) > SCENE_SAVED_BLOCK_CELL_MAX) return 0;
    slot = scene_saved_block_alloc(name);
    if (slot < 0) return 0;
    memset(&tmp, 0, sizeof(tmp));
    tmp.used = 1;
    snprintf(tmp.name, sizeof(tmp.name), "%s", name);
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            scene_saved_tile_t tile;
            scene_saved_block_cell_t *cell;
            if (!scene_tile_capture_at(x, y, &tile)) return 0;
            cell = &tmp.cells[count++];
            cell->dx = x - sx;
            cell->dy = y - sy;
            cell->block_id = tile.block_id;
            cell->warp_mode = tile.warp_mode;
            cell->dest_map = tile.dest_map;
            cell->dest_warp_idx = tile.dest_warp_idx;
            memcpy(cell->tiles, tile.tiles, sizeof(cell->tiles));
        }
    }
    tmp.cell_count = count;
    s_scene_saved_blocks[slot] = tmp;
    if (s_dsl_bank_enabled) dsl_bank_save();
    return 1;
}

static int scene_block_place_custom(const char *name, int x, int y) {
    int slot = scene_saved_block_find(name);
    int needed = 0;
    if (slot < 0) return 0;
    if (s_scene_saved_blocks[slot].cell_count <= 0) return 0;
    for (int i = 0; i < s_scene_saved_blocks[slot].cell_count; i++) {
        int tx = x + s_scene_saved_blocks[slot].cells[i].dx;
        int ty = y + s_scene_saved_blocks[slot].cells[i].dy;
        if (!scene_game_coord_in_bounds(tx, ty)) return 0;
        if (scene_tile_prop_find_slot(tx, ty) < 0) needed++;
    }
    for (int i = 0; i < SCENE_TILE_PROP_MAX && needed > 0; i++) {
        if (!s_scene_tile_props[i].used) needed--;
    }
    if (needed > 0) return 0;
    for (int i = 0; i < s_scene_saved_blocks[slot].cell_count; i++) {
        scene_saved_tile_t tile;
        scene_saved_block_cell_t *cell = &s_scene_saved_blocks[slot].cells[i];
        memset(&tile, 0, sizeof(tile));
        tile.block_id = cell->block_id;
        tile.warp_mode = cell->warp_mode;
        tile.dest_map = cell->dest_map;
        tile.dest_warp_idx = cell->dest_warp_idx;
        memcpy(tile.tiles, cell->tiles, sizeof(tile.tiles));
        if (!scene_tile_place_data(&tile, x + cell->dx, y + cell->dy)) return 0;
    }
    Map_BuildScrollView();
    if (s_dsl_bank_enabled) dsl_bank_save();
    return 1;
}

int DebugCLI_GetTileOverrideAt(int tx, int ty, uint8_t *tile_id) {
    int gx = tx >> 1;
    int gy = ty >> 1;
    int slot;
    if (!tile_id) return 0;
    slot = scene_tile_prop_find_slot(gx, gy);
    if (slot < 0) return 0;
    if ((ty & 1) && (tx & 1)) *tile_id = s_scene_tile_props[slot].tiles[3];
    else if ((ty & 1) && !(tx & 1)) *tile_id = s_scene_tile_props[slot].tiles[2];
    else if (!(ty & 1) && (tx & 1)) *tile_id = s_scene_tile_props[slot].tiles[1];
    else *tile_id = s_scene_tile_props[slot].tiles[0];
    return 1;
}

int DebugCLI_GetWarpOverrideAt(int x, int y, uint8_t *has_warp, uint8_t *dest_map, uint8_t *dest_warp_idx) {
    int slot = scene_tile_prop_find_slot(x, y);
    if (slot < 0) return 0;
    if (has_warp) *has_warp = s_scene_tile_props[slot].warp_mode ? 1 : 0;
    if (dest_map) *dest_map = s_scene_tile_props[slot].dest_map;
    if (dest_warp_idx) *dest_warp_idx = s_scene_tile_props[slot].dest_warp_idx;
    return 1;
}

void DebugCLI_ClearTileOverrides(void) {
    for (int i = 0; i < SCENE_TILE_PROP_MAX; i++) {
        if (s_scene_tile_props[i].used && !s_scene_tile_props[i].banked)
            memset(&s_scene_tile_props[i], 0, sizeof(s_scene_tile_props[i]));
    }
}

static int scene_parse_coord_expr(const char *tok, int is_x, int *out) {
    const char *base = is_x ? "player.x" : "player.y";
    const char *base_typo = is_x ? "play.x" : "play.y";
    const char *base_short = "player";
    if (!tok || !out) return 0;
    if (cli_is_numeric_token(tok)) {
        *out = (int)strtol(tok, NULL, 0);
        return 1;
    }
    if (strncmp(tok, base, strlen(base)) == 0) {
        int v = is_x ? (int)wXCoord : (int)wYCoord;
        const char *p = tok + strlen(base);
        if (*p == '\0') {
            *out = v;
            return 1;
        }
        if ((*p == '+' || *p == '-') && cli_is_numeric_token(p + 1)) {
            int off = (int)strtol(p + 1, NULL, 0);
            if (*p == '-') off = -off;
            *out = v + off;
            return 1;
        }
    }
    if (strncmp(tok, base_typo, strlen(base_typo)) == 0) {
        int v = is_x ? (int)wXCoord : (int)wYCoord;
        const char *p = tok + strlen(base_typo);
        if (*p == '\0') {
            *out = v;
            return 1;
        }
        if ((*p == '+' || *p == '-') && cli_is_numeric_token(p + 1)) {
            int off = (int)strtol(p + 1, NULL, 0);
            if (*p == '-') off = -off;
            *out = v + off;
            return 1;
        }
    }
    if (strncmp(tok, base_short, strlen(base_short)) == 0) {
        int v = is_x ? (int)wXCoord : (int)wYCoord;
        const char *p = tok + strlen(base_short);
        if (*p == '\0') {
            *out = v;
            return 1;
        }
        if ((*p == '+' || *p == '-') && cli_is_numeric_token(p + 1)) {
            int off = (int)strtol(p + 1, NULL, 0);
            if (*p == '-') off = -off;
            *out = v + off;
            return 1;
        }
    }
    return 0;
}

static void scene_normalize_coord_args(const char *src, char *dst, size_t dst_sz) {
    size_t di = 0;
    char prev = '\0';
    if (!dst || dst_sz == 0) return;
    if (!src) src = "";
    for (size_t si = 0; src[si] && di + 1 < dst_sz; si++) {
        char c = src[si];
        if (c == ',') c = ' ';
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            size_t ni = si + 1;
            while (src[ni] == ' ' || src[ni] == '\t') ni++;
            if (src[ni] == '+' || src[ni] == '-' || src[ni] == ',') continue;
            if (prev == '+' || prev == '-') continue;
            if (di > 0 && dst[di - 1] != ' ') {
                dst[di++] = ' ';
                prev = ' ';
            }
            continue;
        }
        if ((c == '+' || c == '-') && di > 0 && dst[di - 1] == ' ')
            di--;
        dst[di++] = c;
        prev = c;
    }
    while (di > 0 && dst[di - 1] == ' ') di--;
    dst[di] = '\0';
}

static int scene_parse_block_save_args(const char *args, char *name, size_t name_sz,
                                       int *sx, int *sy, int *ex, int *ey) {
    char norm[256];
    char t0[32], t1[32], t2[32], t3[32], t4[32], t5[32], t6[32];
    scene_normalize_coord_args(args, norm, sizeof(norm));
    if (sscanf(norm, "%31s %31s %31s %31s %31s %31s %31s", t0, t1, t2, t3, t4, t5, t6) != 7)
        return 0;
    if (strcmp(t1, "start") != 0 || strcmp(t4, "end") != 0) return 0;
    if (!scene_parse_coord_expr(t2, 1, sx) || !scene_parse_coord_expr(t3, 0, sy)) return 0;
    if (!scene_parse_coord_expr(t5, 1, ex) || !scene_parse_coord_expr(t6, 0, ey)) return 0;
    snprintf(name, name_sz, "%s", t0);
    return 1;
}

static int scene_parse_named_coord_args(const char *args, char *name, size_t name_sz,
                                        int *x, int *y) {
    char norm[192];
    char t0[32], t1[32], t2[32];
    scene_normalize_coord_args(args, norm, sizeof(norm));
    if (sscanf(norm, "%31s %31s %31s", t0, t1, t2) != 3) return 0;
    if (!scene_parse_coord_expr(t1, 1, x) || !scene_parse_coord_expr(t2, 0, y)) return 0;
    snprintf(name, name_sz, "%s", t0);
    return 1;
}

static int cli_resolve_event_token(const char *tok, uint16_t *out_event) {
    char want[96];
    if (!tok || !*tok || !out_event) return 0;
    if (cli_is_numeric_token(tok)) {
        long v = strtol(tok, NULL, 0);
        if (v < 0 || v > 65535) return 0;
        *out_event = (uint16_t)v;
        return 1;
    }

    cli_norm(want, sizeof(want), tok);
    for (int i = 0; i <= 4095; i++) {
        const char *name = EventFlagName((uint16_t)i);
        char norm[96], short_norm[96];
        if (!name || strcmp(name, "UNKNOWN_EVENT") == 0) continue;
        cli_norm(norm, sizeof(norm), name);
        if (strcmp(norm, want) == 0) {
            *out_event = (uint16_t)i;
            return 1;
        }
        if (strncmp(norm, "event", 5) == 0) {
            snprintf(short_norm, sizeof(short_norm), "%s", norm + 5);
            if (strcmp(short_norm, want) == 0) {
                *out_event = (uint16_t)i;
                return 1;
            }
        }
    }
    return 0;
}

static int cli_resolve_map_token(const char *tok, int *out_map_id) {
    char want[64];
    if (!tok || !*tok || !out_map_id) return 0;
    if (strcmp(tok, "here") == 0 || strcmp(tok, "current") == 0) {
        *out_map_id = (int)wCurMap;
        return 1;
    }
    if (cli_is_numeric_token(tok)) {
        *out_map_id = (int)strtol(tok, NULL, 0);
        return (*out_map_id >= 0 && *out_map_id < NUM_MAPS);
    }

    cli_norm(want, sizeof(want), tok);
    for (int i = 0; i < NUM_MAPS; i++) {
        char have[64];
        cli_norm(have, sizeof(have), gMapTable[i].name);
        if (strcmp(want, have) == 0) {
            *out_map_id = i;
            return 1;
        }
    }
    return 0;
}

static void write_battle_state(FILE *fp) {
    int bui = BattleUI_GetState();
    const char *btype = (wIsInBattle == 2) ? "TRAINER" : "WILD";

    fprintf(fp, "=== BATTLE (%s) ===\n", btype);
    fprintf(fp, "UI State: %s\n\n", bui_state_name(bui));

    {
        battle_hittrace_t ht = Battle_GetLastHitTrace();
        fprintf(fp, "HITTRACE: %s\n", Battle_HitTraceIsEnabled() ? "ON" : "OFF");
        if (ht.seq > 0) {
            const char *mname = (ht.move_num < NUM_MOVE_DEFS && gMoveNames[ht.move_num])
                ? gMoveNames[ht.move_num] : "???";
            fprintf(fp,
                "  seq=%lu turn=%s move=%u(%s) effect=0x%02X base_acc=%u scaled_acc=%u roll=%u missed=%u reason=%s\n\n",
                (unsigned long)ht.seq,
                ht.player_turn ? "enemy" : "player",
                ht.move_num, mname, ht.move_effect,
                ht.base_acc, ht.scaled_acc, ht.roll, ht.missed,
                hittrace_reason_name(ht.reason));
        } else {
            fprintf(fp, "  (no MoveHitTest samples yet)\n\n");
        }
    }

    if (s_animlab_enabled) {
        uint8_t next = (uint8_t)((s_animlab_move_id > 0 && s_animlab_move_id < NUM_MOVE_DEFS)
            ? s_animlab_move_id : 1);
        const char *next_name = gMoveNames[next] ? gMoveNames[next] : "???";
        fprintf(fp, "ANIMLAB: ON  next=%d (%s)  loops=%d\n\n",
                (int)next, next_name, s_animlab_loops);
    }

    fprintf(fp, "ENEMY:  %s Lv%d  HP: %d/%d  [%s]\n",
            Pokemon_GetName(Species_Dex(wEnemyMon.species)),
            wEnemyMon.level,
            wEnemyMon.hp, wEnemyMon.max_hp,
            status_str(wEnemyMon.status));

    fprintf(fp, "PLAYER: %s Lv%d  HP: %d/%d  [%s]\n\n",
            Pokemon_GetName(Species_Dex(wBattleMon.species)),
            wBattleMon.level,
            wBattleMon.hp, wBattleMon.max_hp,
            status_str(wBattleMon.status));

    fprintf(fp, "Moves:\n");
    for (int i = 0; i < 4; i++) {
        uint8_t mid = wBattleMon.moves[i];
        if (!mid) { fprintf(fp, "  [%d] ---\n", i + 1); continue; }
        uint8_t pp = wBattleMon.pp[i] & 0x3F;
        const char *mname = (mid < NUM_MOVE_DEFS && gMoveNames[mid]) ? gMoveNames[mid] : "???";
        fprintf(fp, "  [%d] %-12s  %d pp\n", i + 1, mname, pp);
    }

    fprintf(fp, "\n");
    if (bui == 10 ) {
        fprintf(fp, ">> Waiting for action: fight <1-4> | run | pkmn | bag\n");
    } else if (bui == 11 ) {
        fprintf(fp, ">> Waiting for move: fight <1-4> | b (back)\n");
    } else if (bui == 22 ) {
        fprintf(fp, ">> \"Use next Monster?\"  a (yes) | b (no)\n");
    } else if (bui == 23 || bui == 24 ) {
        fprintf(fp, ">> Choose next Monster from party menu\n");
    } else {
        fprintf(fp, ">> Animation in progress — wait or press a/b to advance text\n");
    }
}

static char tile_char(int mx, int my, int px, int py, int nc,
                      const map_events_t *ev) {

    if (mx == px * 2 && my == py * 2 + 1) return '@';
    for (int i = 0; i < nc; i++) {
        int ntx, nty;
        NPC_GetTilePos(i, &ntx, &nty);
        if (ntx * 2 == mx && nty * 2 + 1 == my) {

            char label;
            if (AmberScript_GetMarchActorLabelForNpcIdx(i, &label)) return label;
            return 'N';
        }
    }
    if (ev) {
        for (int i = 0; i < ev->num_signs; i++)
            if ((int)ev->signs[i].x * 2 == mx && (int)ev->signs[i].y * 2 + 1 == my) return 'S';
        for (int i = 0; i < ev->num_items; i++)
            if ((int)ev->items[i].x * 2 == mx && (int)ev->items[i].y * 2 + 1 == my) return 'I';
        for (int i = 0; i < ev->num_hidden_events; i++)
            if ((int)ev->hidden_events[i].x == mx && (int)ev->hidden_events[i].y == my) return 'H';
    }
    uint8_t tid = Map_GetTile(mx, my);
    if (Warp_IsDoorTile(tid))                    return '+';

    if (AmberScript_IsEnabled() && wCurMap >= PKS_VIRTUAL_MAP_FIRST && wCurMap <= PKS_VIRTUAL_MAP_LAST) {
        uint8_t is_grass = 0;
        if (AmberScript_GetGrassOverrideAt(mx, my, &is_grass) && is_grass) return '"';
    } else if (tid == wGrassTile && wGrassTile != 0xFF) {
        return '"';
    }
    int ld = Player_GetLedgeDir(tid);
    if (ld ==  0) return 'v';
    if (ld ==  4) return '^';
    if (ld ==  8) return '<';
    if (ld == 12) return '>';

    if (!Map_IsTilePassableAt(mx / 2, (my - 1) / 2)) return '#';
    return '.';
}

static const char *debug_cli_map_name(int map_id) {
    if (map_id < NUM_MAPS) return gMapTable[map_id].name;
    if (AmberScript_IsEnabled()) {
        const char *n = AmberScript_MapBank_NameForRealId(map_id);
        if (n && n[0]) return n;
    }
    return "???";
}

static void debug_cli_render_grid(FILE *fp) {
    int nc = NPC_GetCount();
    int px = (int)wXCoord, py = (int)wYCoord;
    const map_events_t *ev = (wCurMap < NUM_MAPS) ? &gMapEvents[wCurMap] : NULL;

    fprintf(fp, "+");
    for (int x = 0; x < SCREEN_WIDTH; x++) fprintf(fp, "-");
    fprintf(fp, "+\n");
    for (int ty = 0; ty < SCREEN_HEIGHT; ty++) {
        fprintf(fp, "|");
        for (int tx = 0; tx < SCREEN_WIDTH; tx++)
            fprintf(fp, "%c", tile_char(gCamX + tx, gCamY + ty, px, py, nc, ev));
        fprintf(fp, "|\n");
    }
    fprintf(fp, "+");
    for (int x = 0; x < SCREEN_WIDTH; x++) fprintf(fp, "-");
    fprintf(fp, "+\n");
}

static void write_overworld_state(FILE *fp) {
    fprintf(fp, "=== OVERWORLD ===\n");
    fprintf(fp, "Map: %d (%s)  Player: (%d, %d)  Facing: %s\n\n",
            wCurMap, debug_cli_map_name(wCurMap), wXCoord, wYCoord, facing_name(wPlayerDirection));

    static const char *legend[] = {
        "@  = Player",
        "#  = Wall/Solid",
        ".  = Open",
        "\"  = Grass",
        "+  = Warp/Door",
        "N  = NPC (or a march actor's own letter, e.g. G = gymguide)",
        "^v<> = Ledge",
        "S  = Sign",
        "I  = Item",
        "H  = Hidden event",
    };
    static const int LEGEND_COUNT = (int)(sizeof(legend) / sizeof(legend[0]));

    int nc = NPC_GetCount();
    int px = (int)wXCoord, py = (int)wYCoord;
    const map_events_t *ev = (wCurMap < NUM_MAPS) ? &gMapEvents[wCurMap] : NULL;

    fprintf(fp, "+");
    for (int x = 0; x < SCREEN_WIDTH; x++) fprintf(fp, "-");
    fprintf(fp, "+  Legend:\n");
    for (int ty = 0; ty < SCREEN_HEIGHT; ty++) {
        fprintf(fp, "|");
        for (int tx = 0; tx < SCREEN_WIDTH; tx++)
            fprintf(fp, "%c", tile_char(gCamX + tx, gCamY + ty, px, py, nc, ev));
        fprintf(fp, "|");
        if (ty < LEGEND_COUNT) fprintf(fp, "  %s", legend[ty]);
        fprintf(fp, "\n");
    }
    fprintf(fp, "+");
    for (int x = 0; x < SCREEN_WIDTH; x++) fprintf(fp, "-");
    fprintf(fp, "+\n");
}

static FILE *s_march_anim_file = NULL;
void DebugCLI_WriteMarchAnimFrame(const char *header_line) {
    if (!s_march_anim_file) s_march_anim_file = fopen("march_anim_strip.log", "w");
    if (!s_march_anim_file) return;
    fprintf(s_march_anim_file, "Map: %d (%s)  %s\n",
            wCurMap, debug_cli_map_name(wCurMap), header_line);
    debug_cli_render_grid(s_march_anim_file);
    fprintf(s_march_anim_file, "\n");
    fflush(s_march_anim_file);
}

static void write_state(void) {
    FILE *fp = fopen(STATE_FILE, "w");
    if (!fp) return;

    if (Text_IsOpen()) {
        char tbuf[256];
        fprintf(fp, "=== TEXT ===\n");
        if (Text_GetCurrentStr(tbuf, sizeof(tbuf)))
            fprintf(fp, "%s\n", tbuf);
        else
            fprintf(fp, "<dialog open>\n");
        fprintf(fp, "\n>> press a to continue | b to dismiss\n");
        fclose(fp);
        return;
    }

    if (Pokecenter_IsWaitingYesNo()) {
        fprintf(fp, "=== POKECENTER ===\n");
        fprintf(fp, "Nurse Joy: Shall we heal your Monster?\n");
        fprintf(fp, "\n>> a (yes, heal) | b (no, cancel)\n");
        fclose(fp);
        return;
    }

    int sc = get_scene();

    if (sc == 2  || sc == 1 ) {
        write_battle_state(fp);
    } else {
        write_overworld_state(fp);
    }

    fprintf(fp, "\nParty (%d):\n", wPartyCount);
    for (int i = 0; i < wPartyCount && i < 6; i++) {
        const party_mon_t *m = &wPartyMons[i];
        fprintf(fp, "  [%d] %s Lv%d  HP:%d/%d  [%s]\n",
                i + 1,
                Pokemon_GetName(Species_Dex(m->base.species)),
                m->level, (int)m->base.hp, (int)m->max_hp,
                status_str(m->base.status));
    }

    unsigned money = ((unsigned)wPlayerMoney[0] >> 4) * 100000
                   + ((unsigned)wPlayerMoney[0] & 0xF) * 10000
                   + ((unsigned)wPlayerMoney[1] >> 4) * 1000
                   + ((unsigned)wPlayerMoney[1] & 0xF) * 100
                   + ((unsigned)wPlayerMoney[2] >> 4) * 10
                   + ((unsigned)wPlayerMoney[2] & 0xF);
    fprintf(fp, "\nMoney: $%u  Badges: %d  Frame: %d\n",
            money, count_bits8(wObtainedBadges),
            (int)(255 - hFrameCounter));

    if (Trainer_IsEngaging())
        fprintf(fp, "\n!! TRAINER SPOTTED YOU — engaging\n");

    fclose(fp);
}

enum { SM_IDLE = 0, SM_OPENING, SM_NAV, SM_CONFIRM, SM_YES, SM_FINISH };
static int s_savemenu_state = SM_IDLE;
static int s_savemenu_wait  = 0;
static int s_savemenu_guard = 0;

static void savemenu_finish(const char *result, const char *why) {
    FILE *fp = fopen(STATE_FILE, "w");
    if (fp) {
        fprintf(fp, "=== SAVEMENU ===\nresult=%s\n", result);
        if (why && *why) fprintf(fp, "reason=%s\n", why);
        fclose(fp);
    }
    printf("[savemenu] %s%s%s\n", result, why && *why ? ": " : "", why ? why : "");
    fflush(stdout);
    s_savemenu_state = SM_IDLE;
}

static const char *savemenu_blocked_reason(void) {
    int sc = get_scene();
    if (sc == 2 || sc == 1)            return "in battle";
    if (Text_IsOpen())                 return "text box open";
    if (Pokecenter_IsWaitingYesNo())   return "pokecenter prompt open";
    if (Trainer_IsEngaging())          return "trainer engaging";
    if (AmberScript_Scene_IsActive())  return "scene running";
    if (Player_IsSimulatingMovement()) return "scripted movement";
    if (sc != 0)                       return "not in the overworld";
    return NULL;
}

#define FRAMES_PER_TILE 20
#define PRESS  1
#define GAP    8

static void seq_battle_menu(int pos) {
    if (pos & 2) seq_push(BTN_DOWN,  PRESS, GAP);
    if (pos & 1) seq_push(BTN_RIGHT, PRESS, GAP);
    seq_push(BTN_A, PRESS, GAP);
}

static void seq_move_select(int n) {
    for (int i = 1; i < n; i++)
        seq_push(BTN_DOWN, PRESS, GAP);
    seq_push(BTN_A, PRESS, GAP);
}

#define CON_TMIDX(row, col) ((unsigned)((row) + 2) * SCROLL_MAP_W + ((col) + 2) + Map_UiColOfs())

static int con_char_to_tile(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return Font_CharToTile(0x80 + (c - 'A'));
    if (c >= 'a' && c <= 'z') return Font_CharToTile(0xA0 + (c - 'a'));
    if (c == ' ')              return BLANK_TILE_SLOT;
    if (c >= '0' && c <= '9') return Font_CharToTile(0xF6 + (c - '0'));
    if (c == '.')              return Font_CharToTile(0xE8);
    if (c == ',')              return Font_CharToTile(0xF4);
    if (c == '-')              return Font_CharToTile(0xE3);
    if (c == ':')              return Font_CharToTile(0x9C);
    if (c == '/')              return Font_CharToTile(0xF3);
    if (c == '?')              return Font_CharToTile(0xE6);
    if (c == '!')              return Font_CharToTile(0xE7);
    return BLANK_TILE_SLOT;
}

static void con_draw(void) {
    if (!s_con_overlay_enabled) return;

    gScrollTileMap[CON_TMIDX(CON_TOP_ROW, 0)] = (uint8_t)Font_CharToTile(0x79);
    for (int c = 1; c < SCREEN_WIDTH - 1; c++)
        gScrollTileMap[CON_TMIDX(CON_TOP_ROW, c)] = (uint8_t)Font_CharToTile(0x7A);
    gScrollTileMap[CON_TMIDX(CON_TOP_ROW, SCREEN_WIDTH - 1)] = (uint8_t)Font_CharToTile(0x7B);

    gScrollTileMap[CON_TMIDX(CON_IN_ROW, 0)]              = (uint8_t)Font_CharToTile(0x7C);
    gScrollTileMap[CON_TMIDX(CON_IN_ROW, SCREEN_WIDTH-1)] = (uint8_t)Font_CharToTile(0x7C);
    gScrollTileMap[CON_TMIDX(CON_IN_ROW, 1)]              = (uint8_t)con_char_to_tile(':');

    int text_cols = SCREEN_WIDTH - 4;
    int start = (s_con_len > text_cols) ? s_con_len - text_cols : 0;
    int col = 2;
    for (int i = start; i < s_con_len && col <= SCREEN_WIDTH - 3; i++, col++)
        gScrollTileMap[CON_TMIDX(CON_IN_ROW, col)] =
            (uint8_t)con_char_to_tile((unsigned char)s_con_buf[i]);

    if (col <= SCREEN_WIDTH - 2) {
        gScrollTileMap[CON_TMIDX(CON_IN_ROW, col)] =
            (s_con_blink < CON_BLINK_PERIOD / 2)
            ? (uint8_t)Font_CharToTile(0x7A)
            : BLANK_TILE_SLOT;
        col++;
    }
    for (; col <= SCREEN_WIDTH - 2; col++)
        gScrollTileMap[CON_TMIDX(CON_IN_ROW, col)] = BLANK_TILE_SLOT;
}

static void process_cmd(const char *cmd) {
    s_last_cmd_valid = 1;
    char verb[32] = {0};
    int  n = 1;
    sscanf(cmd, "%31s %d", verb, &n);
    if (n < 1) n = 1;

    seq_clear();

    if (DebugSuite_TryCommand(cmd)) {
        write_state();
        return;
    }

    if (strcmp(verb, "playjohto") == 0) {

        char track[32] = {0};
        cli_parse_arg(cmd, 1, track, sizeof(track));
        johto_music_id_t id = JohtoMusic_ForTrackName(track);
        if (id == JOHTO_MUSIC_NONE) {
            printf("[cli] playjohto: unrecognized track '%s'\n", track);
        } else {
            JohtoMusic_Play(id);
            printf("[cli] playjohto: playing '%s'\n", track);
        }
        write_state();
        return;
    }

    if (strcmp(verb, "amberscript") == 0) {
        char arg[16] = {0};
        cli_parse_arg(cmd, 1, arg, sizeof(arg));
        if (strcmp(arg, "on") == 0) {
            AmberScript_SetEnabled(1);
            printf("[cli] amberscript: ON\n");
        } else if (strcmp(arg, "off") == 0) {
            AmberScript_SetEnabled(0);
            printf("[cli] amberscript: OFF\n");
        } else {
            printf("[cli] amberscript: %s\n", AmberScript_IsEnabled() ? "ON" : "OFF");
        }
        write_state();
        return;
    }

    if (strcmp(verb, "npcshow") == 0 || strcmp(verb, "npchide") == 0) {

        int x = -1, y = -1;
        int hide = (verb[3] == 'h');
        if (sscanf(cmd, "%*s %d %d", &x, &y) != 2) {
            printf("[cli] %s usage: %s <declared_x> <declared_y>\n", verb, verb);
        } else {
            AmberScript_MapNpcSaveRuntime((int)wCurMap, x, y, 0, 0, 0, hide, 0);
            printf("[cli] %s: npc declared at (%d,%d) on map %d -> %s"
                   " (re-enter the map to see it)\n",
                   verb, x, y, (int)wCurMap, hide ? "hidden" : "shown");
        }
        return;
    }
    if (strcmp(verb, "setevent") == 0) {

        char arg[64] = {0};
        int n = -1;
        sscanf(cmd, "%*s %63s", arg);
        if (arg[0] >= '0' && arg[0] <= '9') {
            n = atoi(arg);
        } else {
            uint16_t id;
            if (EventFlagIdByName(arg, &id)) n = id;
        }
        if (n < 0 || n >= NUM_EVENTS) {
            printf("[cli] setevent: '%s' isn't a valid flag number (0-%d) or recognized EVENT_* name\n",
                   arg, NUM_EVENTS - 1);
        } else {

            SetEvent((uint16_t)n);
            printf("[cli] setevent %s (%d): set\n", arg, n);
        }
        write_state();
        return;
    }
    if (strcmp(verb, "clearevent") == 0) {

        char arg[64] = {0};
        int n = -1;
        sscanf(cmd, "%*s %63s", arg);
        if (arg[0] >= '0' && arg[0] <= '9') {
            n = atoi(arg);
        } else {
            uint16_t id;
            if (EventFlagIdByName(arg, &id)) n = id;
        }
        if (n < 0 || n >= NUM_EVENTS) {
            printf("[cli] clearevent: '%s' isn't a valid flag number (0-%d) or recognized EVENT_* name\n",
                   arg, NUM_EVENTS - 1);
        } else {

            ClearEvent((uint16_t)n);
            printf("[cli] clearevent %s (%d): cleared\n", arg, n);
        }
        write_state();
        return;
    }

    if (strcmp(verb, "resetoak") == 0) {

        static const uint16_t oak_seq[] = {
            EVENT_OAK_APPEARED_IN_PALLET,
            EVENT_FOLLOWED_OAK_INTO_LAB,
            EVENT_FOLLOWED_OAK_INTO_LAB_2,
            EVENT_OAK_ASKED_TO_CHOOSE_MON,
            EVENT_GOT_STARTER,
            EVENT_BATTLED_RIVAL_IN_OAKS_LAB,
            EVENT_GOT_OAKS_PARCEL,
            EVENT_OAK_GOT_PARCEL,
            EVENT_1ST_ROUTE22_RIVAL_BATTLE,
            EVENT_2ND_ROUTE22_RIVAL_BATTLE,
            EVENT_ROUTE22_RIVAL_WANTS_BATTLE,
        };
        for (unsigned i = 0; i < sizeof(oak_seq) / sizeof(oak_seq[0]); i++)
            ClearEvent(oak_seq[i]);

        extern void AmberScript_RearmSceneTriggers(void);
        AmberScript_RearmSceneTriggers();
        printf("[cli] resetoak: cleared Oak intro sequence (%u events) + re-armed triggers\n",
               (unsigned)(sizeof(oak_seq) / sizeof(oak_seq[0])));
        fflush(stdout);
        write_state();
        return;
    }

    if (strcmp(verb, "skipoak") == 0) {

        static const uint16_t oak_done_seq[] = {
            EVENT_OAK_APPEARED_IN_PALLET,
            EVENT_FOLLOWED_OAK_INTO_LAB,
            EVENT_FOLLOWED_OAK_INTO_LAB_2,
            EVENT_OAK_ASKED_TO_CHOOSE_MON,
            EVENT_GOT_STARTER,
            EVENT_BATTLED_RIVAL_IN_OAKS_LAB,
            EVENT_GOT_OAKS_PARCEL,
        };
        for (unsigned i = 0; i < sizeof(oak_done_seq) / sizeof(oak_done_seq[0]); i++)
            SetEvent(oak_done_seq[i]);
        ClearEvent(EVENT_OAK_GOT_PARCEL);
        extern void AmberScript_RearmSceneTriggers(void);
        AmberScript_RearmSceneTriggers();
        printf("[cli] skipoak: marked Oak intro + starter + rival battle + parcel pickup as done "
               "(%u events set); OAK_GOT_PARCEL left clear so the delivery scene still fires\n",
               (unsigned)(sizeof(oak_done_seq) / sizeof(oak_done_seq[0])));
        fflush(stdout);
        write_state();
        return;
    }

    if (strcmp(verb, "btrans_zoom") == 0) {

        extern void BattleTransition_SetZoomMode(int on);
        extern int  BattleTransition_GetZoomMode(void);
        char arg[16] = {0};
        cli_parse_arg(cmd, 1, arg, sizeof(arg));
        if (strcmp(arg, "on") == 0)       BattleTransition_SetZoomMode(1);
        else if (strcmp(arg, "off") == 0) BattleTransition_SetZoomMode(0);
        else BattleTransition_SetZoomMode(!BattleTransition_GetZoomMode());
        printf("[cli] btrans_zoom: %s\n",
               BattleTransition_GetZoomMode() ? "ON (cinematic zoom)"
                                              : "OFF (ROM flash/wipe)");
        write_state();
        return;
    }

    if (strcmp(verb, "noclip") == 0) {

        char arg[16] = {0};
        cli_parse_arg(cmd, 1, arg, sizeof(arg));
        if (strcmp(arg, "on") == 0) {
            gNoClip = 1;
            printf("[cli] noclip: ON\n");
        } else if (strcmp(arg, "off") == 0) {
            gNoClip = 0;
            printf("[cli] noclip: OFF\n");
        } else {
            gNoClip = !gNoClip;
            printf("[cli] noclip: %s\n", gNoClip ? "ON" : "OFF");
        }
        write_state();
        return;
    }
    if (strcmp(verb, "passable_at") == 0) {

        char xs[16] = {0}, ys[16] = {0};
        cli_parse_arg(cmd, 1, xs, sizeof(xs));
        cli_parse_arg(cmd, 2, ys, sizeof(ys));
        {
            int gx = atoi(xs), gy = atoi(ys);
            int tx = gx * 2, ty = gy * 2 + 1;
            int final_answer = Map_IsTilePassableAt(gx, gy);
            uint8_t override_passable = 0;
            int has_override = AmberScript_IsEnabled() &&
                AmberScript_GetPassableOverrideAt(tx, ty, &override_passable);
            uint8_t tid = Map_GetTile(tx, ty);
            int raw_passable = Tile_IsPassable(tid);
            static const char *kCandidates[] = {
                "mod_runtime/map_export/passable_at.txt",
                "../mod_runtime/map_export/passable_at.txt",
            };
            FILE *pf = NULL;
            for (size_t ci = 0; ci < sizeof(kCandidates) / sizeof(kCandidates[0]) && !pf; ci++)
                pf = fopen(kCandidates[ci], "w");
            if (pf) {
                fprintf(pf, "gx=%d gy=%d tx=%d ty=%d map=%d\n", gx, gy, tx, ty, (int)wCurMap);
                fprintf(pf, "Map_IsTilePassableAt result: %d\n", final_answer);
                fprintf(pf, "AmberScript_GetPassableOverrideAt: has_override=%d value=%d\n",
                        has_override, override_passable);
                fprintf(pf, "raw resolved tile_gfx id at (tx,ty): %d, Tile_IsPassable(that id): %d\n",
                        tid, raw_passable);
                fclose(pf);
            }
            printf("[cli] passable_at %d,%d: final=%d override=%d/%d raw_tid=%d raw_passable=%d\n",
                   gx, gy, final_answer, has_override, override_passable, tid, raw_passable);
        }
        write_state();
        return;
    }

    {
        int allow_when_text_open =
            (strcmp(verb, "state") == 0) ||
            (strcmp(verb, "scene_npc") == 0) ||
            (strcmp(verb, "scene-npc") == 0) ||
            (strcmp(verb, "scenenpc") == 0) ||
            (strcmp(verb, "npcscene") == 0) ||
            (strcmp(verb, "scene_trigger") == 0) ||
            (strcmp(verb, "scene_run") == 0) ||
            (strcmp(verb, "scene_stop") == 0) ||
            (strcmp(verb, "dsl_bank") == 0);
    if ((Text_IsOpen() || Pokecenter_IsWaitingYesNo()) && !allow_when_text_open) {
        if (strcmp(verb, "a") == 0 || strcmp(verb, "interact") == 0) {
            for (int i = 0; i < n; i++) seq_push(BTN_A, PRESS, GAP);
        } else if (strcmp(verb, "b") == 0 || strcmp(verb, "back") == 0) {
            seq_push(BTN_B, PRESS, GAP);
        } else {
            printf("[cli] Text is open — only a/b accepted\n");
            write_state();
            return;
        }
        if (s_seq_len > 0) {
            s_wait_remaining = 20;
            s_pending_write  = 1;
        }
        return;
    }
    }

    if (AmberScript_IsEnabled() && AmberScript_Dispatch(cmd)) {
        return;
    }

    if      (strcmp(verb, "up")    == 0) seq_push(BTN_UP,    n * FRAMES_PER_TILE, 5);
    else if (strcmp(verb, "down")  == 0) seq_push(BTN_DOWN,  n * FRAMES_PER_TILE, 5);
    else if (strcmp(verb, "left")  == 0) seq_push(BTN_LEFT,  n * FRAMES_PER_TILE, 5);
    else if (strcmp(verb, "right") == 0) seq_push(BTN_RIGHT, n * FRAMES_PER_TILE, 5);
    else if (strcmp(verb, "a") == 0 || strcmp(verb, "interact") == 0) {
        for (int i = 0; i < n; i++) seq_push(BTN_A, PRESS, GAP);
    } else if (strcmp(verb, "b") == 0 || strcmp(verb, "back") == 0)
        seq_push(BTN_B,      PRESS, GAP);
    else if (strcmp(verb, "start") == 0 || strcmp(verb, "menu") == 0)
        seq_push(BTN_START,  PRESS, GAP);
    else if (strcmp(verb, "select") == 0)
        seq_push(BTN_SELECT, PRESS, GAP);
    else if (strcmp(verb, "wait")  == 0)
        seq_push(0, n, 0);
    else if (strcmp(verb, "state") == 0) {
        write_state();
        return;
    }
    else if (strcmp(verb, "cleartrade") == 0) {

        char cv[32] = {0};
        int which = -1;
        sscanf(cmd, "%31s %d", cv, &which);
        if (which < 0) {
            wCompletedInGameTradeFlags = 0;
        } else if (which < NUM_NPC_TRADES) {
            wCompletedInGameTradeFlags &= (uint16_t)~(1u << which);
        }
        FILE *fp = fopen(STATE_FILE, "w");
        if (fp) {
            fprintf(fp, "=== CLEARTRADE ===\ncleared=%s flags=0x%04X\n",
                    which < 0 ? "all" : "one", wCompletedInGameTradeFlags);
            for (int i = 0; i < NUM_NPC_TRADES; i++)
                fprintf(fp, "  %d %-10s give=%s get=%s %s\n", i, gTradeMons[i].nick,
                        Pokemon_GetNameBySpecies(gTradeMons[i].give),
                        Pokemon_GetNameBySpecies(gTradeMons[i].get),
                        (wCompletedInGameTradeFlags & (1u << i)) ? "DONE" : "-");
            fclose(fp);
        }
        return;
    }
    else if (strcmp(verb, "trademovie") == 0) {

        extern int Game_BeginTradeAnim(void);
        char tv[32] = {0};
        int give = 0, get = 0;
        sscanf(cmd, "%31s %d %d", tv, &give, &get);
        if (give <= 0) give = SPECIES_NIDORINO;
        if (get  <= 0) get  = SPECIES_NIDORINA;
        memset(&gTradeAnim, 0, sizeof gTradeAnim);
        gTradeAnim.player_species = (uint8_t)give;
        gTradeAnim.enemy_species  = (uint8_t)get;
        snprintf(gTradeAnim.player_ot, sizeof gTradeAnim.player_ot, "%s",
                 (const char *)wPlayerName);
        snprintf(gTradeAnim.enemy_ot, sizeof gTradeAnim.enemy_ot, "TRAINER");
        snprintf(gTradeAnim.enemy_trainer, sizeof gTradeAnim.enemy_trainer, "TERRY");
        gTradeAnim.player_otid = wPlayerID;
        gTradeAnim.enemy_otid  = 12345;
        int ok = Game_BeginTradeAnim();
        FILE *fp = fopen(STATE_FILE, "w");
        if (fp) {
            fprintf(fp, "=== TRADEMOVIE ===\nstarted=%d give=%d get=%d\n", ok, give, get);
            fclose(fp);
        }
        return;
    }
    else if (strcmp(verb, "mapname") == 0) {
        FILE *fp = fopen(STATE_FILE, "w");
        if (fp) {
            const char *name = AmberScript_MapBank_NameForRealId(wCurMap);
            fprintf(fp, "=== MAPNAME ===\nwCurMap=%d name=%s streamed=%d\n",
                    wCurMap, name ? name : "(null)",
                    AmberScript_MapBank_HasStreamedForRealId(wCurMap));
            fclose(fp);
        }
        return;
    }
    else if (strcmp(verb, "use_cut") == 0) {
        FILE *fp = fopen(STATE_FILE, "w");
        int result = FieldMove_UseCutFromMenu();
        if (fp) {
            fprintf(fp, "=== USE_CUT ===\nresult=%d (0=nothing to cut, 1=cutting)\n", result);
            fclose(fp);
        }
        return;
    }
    else if (strcmp(verb, "pairblockcheck") == 0) {
        FILE *fp = fopen(STATE_FILE, "w");
        if (fp) {
            static const int ddx[4] = { 0,  0, -1,  1};
            static const int ddy[4] = { 1, -1,  0,  0};
            int nx = (int)wXCoord + ddx[gPlayerFacing & 3];
            int ny = (int)wYCoord + ddy[gPlayerFacing & 3];
            int blocked = AmberScript_IsPairBlockedAt((int)wXCoord * 2, (int)wYCoord * 2 + 1, nx * 2, ny * 2 + 1);
            fprintf(fp, "=== PAIRBLOCKCHECK ===\ncur=(%d,%d) ahead=(%d,%d) blocked=%d\n",
                    (int)wXCoord, (int)wYCoord, nx, ny, blocked);
            fclose(fp);
        }
        return;
    }
    else if (strcmp(verb, "ledgecheck") == 0) {
        FILE *fp = fopen(STATE_FILE, "w");
        if (fp) {
            static const int ddx[4] = { 0,  0, -1,  1};
            static const int ddy[4] = { 1, -1,  0,  0};
            int nx = (int)wXCoord + ddx[gPlayerFacing & 3];
            int ny = (int)wYCoord + ddy[gPlayerFacing & 3];
            int dirs = 0;
            int got = AmberScript_GetLedgeOverrideAt(nx * 2, ny * 2 + 1, &dirs);
            fprintf(fp, "=== LEDGECHECK ===\nwCurMap=%d pkscript=%d facing=%d cur=(%d,%d) ahead=(%d,%d) got=%d dirs=%d\n",
                    wCurMap, AmberScript_IsEnabled(), gPlayerFacing,
                    (int)wXCoord, (int)wYCoord, nx, ny, got, dirs);
            fclose(fp);
        }
        return;
    }
    else if (strcmp(verb, "color") == 0) {

        FILE *fp;
        char a1[32] = {0}, a2[32] = {0};
        cli_parse_arg(cmd, 1, a1, sizeof(a1));
        cli_parse_arg(cmd, 2, a2, sizeof(a2));
        if (strcmp(a1, "off") == 0) {

            GbcColor_SetEnabled(0);
            Gen1Color_SetEnabled(0);
        } else if (strcmp(a1, "on") == 0) {
            GbcColor_SetEnabled(1);
            Gen1Color_SetEnabled(1);
            GbcColor_MarkDirty();
        } else if (strcmp(a1, "curve") == 0) {
            Display_SetColorCurve(atoi(a2));
        } else if (strcmp(a1, "lcd") == 0) {

            if (strcmp(a2, "off") == 0) Display_SetLCDGhostingMode(DISPLAY_LCD_GHOSTING_OFF);
            else if (strcmp(a2, "sameboy") == 0) Display_SetLCDGhostingMode(DISPLAY_LCD_GHOSTING_SAMEBOY_ACCURATE);
            else if (strcmp(a2, "persistence") == 0 || strcmp(a2, "on") == 0) Display_SetLCDGhostingMode(DISPLAY_LCD_GHOSTING_PERSISTENCE);
        } else if (strcmp(a1, "gen2text") == 0) {

            Font_SetGen2Enabled(strcmp(a2, "off") != 0);
            Font_Load();
        } else if (strcmp(a1, "tickrate") == 0) {

            extern unsigned long gPlayTimeFrames;
            static unsigned long s_last_frames = 0;
            static long s_last_sec = 0;
            long now = (long)time(NULL);
            if (s_last_sec && now > s_last_sec) {
                unsigned long dframes = gPlayTimeFrames - s_last_frames;
                long dsec = now - s_last_sec;
                printf("[tickrate] %lu ticks in %lds = %.1f/sec "
                       "(loop intends ~62.5; GB VBlank is 59.73)\n",
                       dframes, dsec, (double)dframes / (double)dsec);
                fflush(stdout);
            } else {
                printf("[tickrate] baseline taken -- run again in ~5s\n");
                fflush(stdout);
            }
            s_last_frames = gPlayTimeFrames;
            s_last_sec = now;
        } else if (strcmp(a1, "gen2fadedelay") == 0) {

            CrystalFade_SetDelay(atoi(a2));
        } else if (strcmp(a1, "gen2account") == 0) {

            Menu_SetGen2Account(strcmp(a2, "off") != 0);
        } else if (strcmp(a1, "gen2frame") == 0) {

            Font_SetGen2Frame(atoi(a2));
            Font_Load();
        } else if (strcmp(a1, "gen1color") == 0) {

            Gen1Color_SetEnabled(strcmp(a2, "off") != 0);
        } else if (strcmp(a1, "mono") == 0) {

            int mono = strcmp(a2, "off") != 0;
            GbcColor_SetEnabled(!mono);
            Gen1Color_SetEnabled(1);
            if (!mono) GbcColor_MarkDirty();
        } else if (strcmp(a1, "dump") == 0) {

            FILE *d = fopen("bugs/colordump.txt", "w");
            if (d) {
                int fn, fd_, fw;
                Display_GetColorFade(&fn, &fd_, &fw);
                fprintf(d, "enabled=%d active=%d curve=%d pos_attr=%d fade=%d/%d white=%d\n",
                        GbcColor_IsEnabled(), Display_GetColorMode(),
                        Display_GetColorCurve(), Display_GetPositionAttrMode(),
                        fn, fd_, fw);
                fprintf(d, "scene=%d wCurMap=%d wIsInBattle=%d wTrainerClass=%d\n",
                        Game_GetScene(), wCurMap, wIsInBattle, wTrainerClass);
                fprintf(d, "battleMon.species=%d enemyMon.species=%d\n",
                        wBattleMon.species, wEnemyMon.species);
                fprintf(d, "\n-- BG palettes (RGB555) --\n");
                for (int p = 0; p < 8; p++) {
                    fprintf(d, "bg[%d]:", p);
                    for (int c = 0; c < 4; c++)
                        fprintf(d, " %04X", Display_GetBGColorEntry(p, c));
                    fprintf(d, "\n");
                }
                fprintf(d, "\n-- OBJ palettes (RGB555) --\n");
                for (int p = 0; p < Display_GetNumOBJPalettes(); p++) {
                    fprintf(d, "obj[%2d]:", p);
                    for (int c = 0; c < 4; c++)
                        fprintf(d, " %04X", Display_GetOBJColorEntry(p, c));
                    fprintf(d, "\n");
                }
                fprintf(d, "\n-- position attr map (rows 0-17, cols 0-19) --\n");
                for (int r = 0; r < SCREEN_HEIGHT; r++) {
                    fprintf(d, "%2d: ", r);
                    for (int c = 0; c < SCREEN_WIDTH; c++)
                        fprintf(d, "%X", Display_GetPositionAttr(c, r) & 0xF);
                    fprintf(d, "\n");
                }
                fprintf(d, "\n-- visible OAM (idx y x tile flags pal) --\n");
                for (int i = 0; i < MAX_SPRITES; i++) {
                    if (wShadowOAM[i].y == 0 || wShadowOAM[i].y >= 160) continue;
                    fprintf(d, "%3d: y=%3d x=%3d tile=%3d flags=%02X pal=%d\n",
                            i, wShadowOAM[i].y, wShadowOAM[i].x,
                            wShadowOAM[i].tile, wShadowOAM[i].flags,
                            wShadowOAM[i].flags & 0x0F);
                }
                fclose(d);
            }
        }
        fp = fopen(STATE_FILE, "w");
        if (fp) {
            static const char *kUi[] = {"gen2", "gen1"};

            static const char *kSpr[] = {"?", "crystal", "gen1"};

            fprintf(fp, "=== COLOR ===\nenabled=%d active=%d curve=%d lcd_mode=%d pos_attr=%d "
                        "gen1color=%d mono=%d ui=%s sprites=%s optmenu=%d exp_px=%d\n",
                    GbcColor_IsEnabled(), Display_GetColorMode(),
                    Display_GetColorCurve(), Display_GetLCDGhostingMode(), Display_GetPositionAttrMode(),
                    Gen1Color_IsEnabled(),
                    (!GbcColor_IsEnabled() && Gen1Color_IsEnabled()),

                    kUi[Gen1Color_UiStyle() & 1],
                    (unsigned)Gen1Color_SpriteStyle() <
                        sizeof kSpr / sizeof kSpr[0]
                        ? kSpr[Gen1Color_SpriteStyle()] : "?",
                    PresentationMenu_IsOpen(),
                    Gen1Color_ExpBarPixels());

            fprintf(fp, "fontstyle=%d (0=gen1 1=gen2)\n", Font_GetStyle());
            fprintf(fp, "bgpal:");
            for (int s = 0; s < 8; s++) {
                fprintf(fp, " %d=", s);
                for (int c = 0; c < 4; c++) {
                    uint16_t v = Display_GetBGColorEntry(s, c);
                    fprintf(fp, "%s%u,%u,%u", c ? "/" : "",
                            v & 31u, (v >> 5) & 31u, (v >> 10) & 31u);
                }
            }
            fprintf(fp, "\n");

            if (Display_GetPositionAttrMode()) {
                int seen[8] = {0};
                for (int r = 0; r < SCREEN_HEIGHT; r++)
                    for (int c = 0; c < SCREEN_WIDTH; c++)
                        seen[Display_GetPositionAttr(c, r) & 7] = 1;
                fprintf(fp, "attrbox_pals:");
                for (int i = 0; i < 8; i++) if (seen[i]) fprintf(fp, " %d", i);
                fprintf(fp, "\n");
            }

            {
                const char *nm = AmberScript_MapBank_NameForRealId(wCurMap);
                fprintf(fp, "map: real=%u name=%s logical=%d\n",
                        (unsigned)wCurMap, nm ? nm : "(none)",
                        nm ? GbcColor_MapIdForName(nm) : (int)wCurMap);
            }

            fprintf(fp, "gen2res: outstanding=%d\n", Gen2Res_Outstanding());
            for (int i = 0; i < Gen2Res_Outstanding(); i++) {
                int lay = 0, knd = 0, first = 0, cnt = 0;
                const char *own = Gen2Res_Describe(i, &lay, &knd, &first, &cnt);
                if (own)
                    fprintf(fp, "  layer=%d kind=%d [%d..%d] %s\n",
                            lay, knd, first, first + cnt - 1, own);
            }
            fclose(fp);
        }
        return;
    }
    else if (strcmp(verb, "sprite") == 0) {

        char a1[16] = {0};
        cli_parse_arg(cmd, 1, a1, sizeof(a1));
        Gen1Color_SetSpriteStyle(strcmp(a1, "crystal") == 0 ? G1C_SPRITES_CRYSTAL
                                                             : G1C_SPRITES_GEN1);
        printf("[cli] sprite: %s\n", Gen1Color_SpriteStyle() == G1C_SPRITES_CRYSTAL ? "crystal" : "gen1");
        return;
    }
    else if (strcmp(verb, "palette") == 0) {

        char a1[16] = {0};
        int style;
        cli_parse_arg(cmd, 1, a1, sizeof(a1));
        style = strcmp(a1, "sgb") == 0 ? G1C_MONPAL_SGB
                                       : G1C_MONPAL_ENHANCED;
        Gen1Color_SetMonPalStyle(style);
        style = Gen1Color_MonPalStyle();
        printf("[cli] palette: %s\n", style == G1C_MONPAL_SGB ? "sgb"
                                                                : "enhanced");
        return;
    }
    else if (strcmp(verb, "overworld") == 0) {

        char a1[16] = {0};
        int style;
        cli_parse_arg(cmd, 1, a1, sizeof(a1));
        style = strcmp(a1, "sgb")       == 0 ? GBC_OVERWORLD_RED_SGB
              : strcmp(a1, "autocolor") == 0 ? GBC_OVERWORLD_RED_AUTOCOLOR
                                              : GBC_OVERWORLD_DEFAULT;
        GbcColor_SetOverworldStyle(style);
        style = GbcColor_OverworldStyle();
        printf("[cli] overworld: %s\n", style == GBC_OVERWORLD_RED_SGB ? "sgb"
                                       : style == GBC_OVERWORLD_RED_AUTOCOLOR ? "autocolor"
                                                                               : "crystal");
        return;
    }
    else if (strcmp(verb, "ui") == 0) {

        char a1[16] = {0};
        cli_parse_arg(cmd, 1, a1, sizeof(a1));
        Gen1Color_SetUiStyle(strcmp(a1, "gen1") == 0 ? G1C_UI_GEN1 : G1C_UI_GEN2);
        printf("[cli] ui: %s\n", Gen1Color_UiStyle() == G1C_UI_GEN1 ? "gen1" : "gen2");
        return;
    }
    else if (strcmp(verb, "cries") == 0) {

        char a1[16] = {0};
        cli_parse_arg(cmd, 1, a1, sizeof(a1));
        Audio_SetCryStyle(strcmp(a1, "crystal") == 0 ? AUDIO_CRIES_CRYSTAL
                                                      : AUDIO_CRIES_GEN1);
        printf("[cli] cries: %s\n", Audio_GetCryStyle() == AUDIO_CRIES_CRYSTAL ? "crystal" : "gen1");
        return;
    }
    else if (strcmp(verb, "cachestats") == 0) {
        FILE *fp = fopen(STATE_FILE, "w");
        if (fp) {
            long total = g_subtile_cache_hits + g_subtile_cache_misses;
            fprintf(fp, "=== CACHESTATS ===\nhits=%ld misses=%ld total=%ld miss_pct=%.1f\n",
                    g_subtile_cache_hits, g_subtile_cache_misses, total,
                    total > 0 ? (100.0 * g_subtile_cache_misses / total) : 0.0);
            fclose(fp);
        }
        return;
    }
    else if (strcmp(verb, "propcount") == 0) {
        FILE *fp = fopen(STATE_FILE, "w");
        if (fp) {
            int counter_value = 0, real_count = 0, total_used = 0;
            AmberScript_DebugTilePropCount(wCurMap, &counter_value, &real_count, &total_used);
            fprintf(fp, "=== PROPCOUNT ===\nmap=%d counter=%d real=%d total_used_all_maps=%d\n",
                    wCurMap, counter_value, real_count, total_used);
            fclose(fp);
        }
        return;
    }
    else if (strcmp(verb, "music_state") == 0) {
        FILE *fp = fopen(STATE_FILE, "w");
        if (fp) {
            fprintf(fp, "=== MUSIC_STATE ===\nkanto_playing=%d johto_playing=%d\n",
                    Music_IsPlaying(), JohtoMusic_IsPlaying());
            fclose(fp);
        }
        return;
    }
    else if (strcmp(verb, "tile_info") == 0 || strcmp(verb, "tileinfo") == 0) {

        int px = (int)wXCoord, py = (int)wYCoord;
        static const struct { const char *label; int dx, dy; } dirs[] = {
            { "HERE ", 0,  0 },
            { "UP   ", 0, -1 },
            { "DOWN ", 0,  1 },
            { "LEFT ",-1,  0 },
            { "RIGHT", 1,  0 },
        };
        printf("[tile_info] player @ game(%d,%d)  tile(%d,%d)\n",
               px, py, px*2, py*2+1);
        for (int i = 0; i < 5; i++) {
            int gx = px + dirs[i].dx;
            int gy = py + dirs[i].dy;
            uint8_t tid = Map_GetGameTile(gx, gy);
            printf("  %s  game(%2d,%2d)  tile(%2d,%2d)  id=0x%02X  %s\n",
                   dirs[i].label, gx, gy, gx*2, gy*2+1, tid,
                   Tile_IsPassable(tid) ? "PASS" : "SOLID");
        }
        return;
    }
    else if (strcmp(verb, "tile_prop_at") == 0 || strcmp(verb, "tileprop") == 0) {

        char norm[64], xs[16] = {0}, ys[16] = {0};
        scene_normalize_coord_args(cmd + strlen(verb), norm, sizeof(norm));
        if (sscanf(norm, "%15s %15s", xs, ys) == 2) {
            int x = atoi(xs), y = atoi(ys);
            char out[512];
            AmberScript_DebugDumpTilePropAt(x, y, out, sizeof(out));
            printf("[tile_prop_at] %s\n", out);
        } else {
            printf("[cli] tile_prop_at usage: tile_prop_at <x> <y>\n");
        }
        return;
    }
    else if (strcmp(verb, "tile_copy") == 0 || strcmp(verb, "copy_tile") == 0) {
        char norm[192], x1[32], y1[32], x2[32], y2[32];
        int sx = 0, sy = 0, dx = 0, dy = 0;
        scene_normalize_coord_args(cmd + strlen(verb), norm, sizeof(norm));
        if (sscanf(norm, "%31s %31s %31s %31s", x1, y1, x2, y2) != 4 ||
            !scene_parse_coord_expr(x1, 1, &sx) || !scene_parse_coord_expr(y1, 0, &sy) ||
            !scene_parse_coord_expr(x2, 1, &dx) || !scene_parse_coord_expr(y2, 0, &dy)) {
            printf("[cli] tile_copy usage: tile_copy <src_x> <src_y> <dst_x> <dst_y>\n");
        } else if (scene_tile_copy(sx, sy, dx, dy)) {
            printf("[cli] tile_copy: (%d,%d) -> (%d,%d)\n", sx, sy, dx, dy);
        } else {
            printf("[cli] tile_copy: failed\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "tile_save") == 0) {
        char name[32] = {0};
        if (!cli_parse_arg(cmd, 1, name, sizeof(name))) {
            printf("[cli] tile_save usage: tile_save <name>\n");
        } else if (scene_tile_save_right_of_player(name)) {
            printf("[cli] tile_save: saved '%s' from (%d,%d)\n", name, (int)wXCoord + 1, (int)wYCoord);
        } else {
            printf("[cli] tile_save: failed\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "tile_place_custom") == 0 || strcmp(verb, "tile_place") == 0) {
        char name[32] = {0};
        int x = 0, y = 0;
        if (!scene_parse_named_coord_args(cmd + strlen(verb), name, sizeof(name), &x, &y)) {
            printf("[cli] tile_place_custom usage: tile_place_custom <name> <x> <y>\n");
        } else if (scene_tile_place_custom(name, x, y)) {
            printf("[cli] tile_place_custom: '%s' -> (%d,%d)\n", name, x, y);
        } else if (scene_block_place_custom(name, x, y)) {
            int slot = scene_saved_block_find(name);
            int count = (slot >= 0) ? s_scene_saved_blocks[slot].cell_count : 0;
            printf("[cli] tile_place_custom: block '%s' -> (%d,%d), cells=%d\n", name, x, y, count);
        } else {
            printf("[cli] tile_place_custom: failed for '%s'\n", name);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "block_save") == 0) {
        char name[32] = {0};
        int sx = 0, sy = 0, ex = 0, ey = 0;
        if (!scene_parse_block_save_args(cmd + strlen(verb), name, sizeof(name), &sx, &sy, &ex, &ey)) {
            printf("[cli] block_save usage: block_save <name> start <x> <y> end <x> <y>\n");
        } else if (scene_block_save(name, sx, sy, ex, ey)) {
            printf("[cli] block_save: saved '%s' from (%d,%d) to (%d,%d)\n", name, sx, sy, ex, ey);
        } else {
            printf("[cli] block_save: failed for '%s'\n", name);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "block_place_custom") == 0 || strcmp(verb, "block_place") == 0) {
        char name[32] = {0};
        int x = 0, y = 0;
        if (!scene_parse_named_coord_args(cmd + strlen(verb), name, sizeof(name), &x, &y)) {
            printf("[cli] block_place_custom usage: block_place_custom <name> <x> <y>\n");
        } else if (scene_block_place_custom(name, x, y)) {
            int slot = scene_saved_block_find(name);
            int count = (slot >= 0) ? s_scene_saved_blocks[slot].cell_count : 0;
            printf("[cli] block_place_custom: '%s' -> (%d,%d), cells=%d\n", name, x, y, count);
        } else {
            printf("[cli] block_place_custom: failed for '%s'\n", name);
        }
        write_state();
        return;
    }

    else if (strcmp(verb, "setlevel") == 0) {

        int slot = 1, level = 20;
        sscanf(cmd, "%*s %d %d", &slot, &level);
        slot--;
        if (slot < 0 || slot >= wPartyCount) {
            printf("[cli] setlevel: slot out of range (party has %d)\n", wPartyCount);
        } else if (level < 1 || level > 100) {
            printf("[cli] setlevel: level must be 1-100\n");
        } else {
            uint8_t species = wPartyMons[slot].base.species;
            Pokemon_InitMon(&wPartyMons[slot], species, (uint8_t)level);
            printf("[cli] setlevel: slot %d (%s) → Lv%d\n",
                   slot + 1,
                   Pokemon_GetName(Species_Dex(species)),
                   level);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "animlab") == 0) {
        char mode[16] = "start";
        int level = 50;
        sscanf(cmd, "%*s %15s %d", mode, &level);

        if (strcmp(mode, "start") == 0 || strcmp(mode, "on") == 0) {
            if (level < 5 || level > 100) level = 50;
            s_animlab_move_id = 1;
            s_animlab_loops   = 0;
            animlab_start_battle(level);
        } else if (strcmp(mode, "stop") == 0 || strcmp(mode, "off") == 0) {
            s_animlab_enabled = 0;
            printf("[cli] animlab: stopped (battle remains under manual control)\n");
        } else if (strcmp(mode, "status") == 0) {
            printf("[cli] animlab: %s (next move %d, loops %d, level %d)\n",
                   s_animlab_enabled ? "ON" : "OFF",
                   s_animlab_move_id, s_animlab_loops, s_animlab_level);
        } else {
            printf("[cli] animlab: use 'animlab start [level]', 'animlab stop', or 'animlab status'\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "hittrace") == 0) {
        char arg[16] = {0};
        sscanf(cmd, "%*s %15s", arg);
        if (strcmp(arg, "on") == 0) {
            Battle_HitTraceEnable(1);
            printf("[cli] hittrace: ON\n");
        } else if (strcmp(arg, "off") == 0) {
            Battle_HitTraceEnable(0);
            printf("[cli] hittrace: OFF\n");
        } else if (strcmp(arg, "reset") == 0) {
            Battle_HitTraceReset();
            printf("[cli] hittrace: reset\n");
        } else if (strcmp(arg, "status") == 0 || arg[0] == '\0') {
            battle_hittrace_t ht = Battle_GetLastHitTrace();
            printf("[cli] hittrace: %s seq=%lu move=0x%02X effect=0x%02X missed=%u reason=%s\n",
                   Battle_HitTraceIsEnabled() ? "ON" : "OFF",
                   (unsigned long)ht.seq, ht.move_num, ht.move_effect, ht.missed,
                   hittrace_reason_name(ht.reason));
        } else {
            printf("[cli] hittrace: use 'hittrace on|off|reset|status'\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "autowin") == 0) {
        char arg[16] = {0};
        sscanf(cmd, "%*s %15s", arg);
        if (strcmp(arg, "on") == 0 || strcmp(arg, "1") == 0) {
            s_autowin_enabled = 1;
            printf("[cli] autowin: ON (first player move each battle auto-wins)\n");
        } else if (strcmp(arg, "off") == 0 || strcmp(arg, "0") == 0) {
            s_autowin_enabled = 0;
            printf("[cli] autowin: OFF\n");
        } else if (strcmp(arg, "status") == 0 || arg[0] == '\0') {
            printf("[cli] autowin: %s\n", s_autowin_enabled ? "ON" : "OFF");
        } else {
            printf("[cli] autowin: use 'autowin on|off|status'\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "fight") == 0) {

        seq_battle_menu(0);
        if (n >= 1 && n <= 4)
            seq_move_select(n);
    }
    else if (strcmp(verb, "run")  == 0) seq_battle_menu(3);
    else if (strcmp(verb, "pkmn") == 0 || strcmp(verb, "pokemon") == 0)
        seq_battle_menu(1);
    else if (strcmp(verb, "bag")  == 0 || strcmp(verb, "item") == 0)
        seq_battle_menu(2);

    else if (strcmp(verb, "teleport") == 0 || strcmp(verb, "tele") == 0 ||
             strcmp(verb, "mw") == 0) {
        static const struct { const char *name; int map, x, y; } kPlaces[] = {

            { "pallet",           0,   5,  6 },
            { "pallet_town",      0,   5,  6 },
            { "viridian",         1,  23, 26 },
            { "viridian_city",    1,  23, 26 },
            { "pewter",           2,  13, 26 },
            { "pewter_city",      2,  13, 26 },
            { "cerulean",         3,  19, 18 },
            { "cerulean_city",    3,  19, 18 },
            { "vermilion",        5,  11,  4 },
            { "vermilion_city",   5,  11,  4 },
            { "lavender",         4,   3,  6 },
            { "lavender_town",    4,   3,  6 },
            { "celadon",          6,  41, 10 },
            { "celadon_city",     6,  41, 10 },
            { "fuchsia",          7,  19, 28 },
            { "fuchsia_city",     7,  19, 28 },
            { "cinnabar",         8,  11, 12 },
            { "cinnabar_island",  8,  11, 12 },
            { "indigo",           9,   9,  6 },
            { "indigo_plateau",   9,   9,  6 },
            { "saffron",         10,   9, 30 },
            { "saffron_city",    10,   9, 30 },
            { "route_4_fly",     15,  11,  6 },
            { "route_10_fly",    21,  11, 20 },

            { "viridian_gym",    52,   8,  9 },
            { "pewter_gym",      54,   8,  9 },
            { "cerulean_gym",    65,   8,  9 },
            { "vermilion_gym",   92,   8,  9 },
            { "celadon_gym",    135,   8,  9 },
            { "fuchsia_gym",    166,   8,  9 },
            { "saffron_gym",    178,   8,  9 },
            { "cinnabar_gym",   234,   8,  9 },

            { "oaks_lab",        37,  12, 11 },
            { "oaks_lab",        37,  12, 11 },
            { "viridian_forest", 51,  14, 40 },
            { "mt_moon",         59,  14, 10 },
            { "rock_tunnel",    155,  14, 10 },
            { "pokemon_tower",  142,   8,  9 },
            { "silph_co",       200,   8,  9 },
            { "safari_zone",    217,  28, 20 },

            { "route_1",         12,  14, 70 },
            { "route_2",         13,  14, 10 },
            { "route_3",         14,  14, 10 },
            { "route_4",         15,  14, 10 },
            { "route_5",         16,  14, 10 },
            { "route_6",         17,  14, 70 },
            { "route_7",         18,  14, 10 },
            { "route_8",         19,  14, 70 },
            { "route_9",         20,  14, 10 },
            { "route_10",        21,  14, 10 },
            { "route_11",        22,  14, 10 },
            { "route_12",        23,  14, 10 },
            { "route_24",        33,  14, 10 },
            { "route_25",        34,  14, 10 },
            { NULL, 0, 0, 0 }
        };

        int map_id, x, y;
        char name_arg[64] = {0};
        sscanf(cmd, "%*s %63s", name_arg);

        int found = 0;
        if (name_arg[0] && !(name_arg[0] >= '0' && name_arg[0] <= '9')) {

            for (int k = 0; name_arg[k]; k++)
                if (name_arg[k] >= 'A' && name_arg[k] <= 'Z') name_arg[k] += 32;

            for (int k = 0; kPlaces[k].name; k++) {
                if (cli_name_eq_loose(name_arg, kPlaces[k].name)) {
                    map_id = kPlaces[k].map;
                    x      = kPlaces[k].x;
                    y      = kPlaces[k].y;
                    found  = 1;
                    break;
                }
            }
            if (!found) {
                printf("[cli] Unknown location: %s\n", name_arg);
                write_state();
                return;
            }
        } else {

            map_id = 0; x = 3; y = 3;
            { char m[16]="0", xs[16]="3", ys[16]="3";
              sscanf(cmd, "%*s %15s %15s %15s", m, xs, ys);
              map_id = (int)strtol(m,  NULL, 0);
              x      = (int)strtol(xs, NULL, 0);
              y      = (int)strtol(ys, NULL, 0); }
        }

        DebugCLI_TeleportToRealMap((uint8_t)map_id, x, y);

        printf("[cli] teleport → map %d (%d,%d)\n", map_id, x, y);
        write_state();
        return;
    }
    else if (strcmp(verb, "givebadge") == 0) {

        char tok[16] = {0};
        sscanf(cmd, "%*s %15s", tok);
        int n = (int)strtol(tok, NULL, 0);
        int bit = -1;
        if (n >= 0 && n <= 7) bit = n;
        else if (n >= 1 && n <= 8) bit = n - 1;
        if (bit < 0 || bit > 7) {
            printf("[cli] givebadge: n must be 0-7 or 1-8\n");
        } else {
            wObtainedBadges |= (uint8_t)(1u << bit);
            printf("[cli] givebadge %d (bit %d) → wObtainedBadges=0x%02X\n", n, bit, wObtainedBadges);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "removebadge") == 0) {

        char tok[16] = {0};
        sscanf(cmd, "%*s %15s", tok);
        int n = (int)strtol(tok, NULL, 0);
        int bit = -1;
        if (n >= 0 && n <= 7) bit = n;
        else if (n >= 1 && n <= 8) bit = n - 1;
        if (bit < 0 || bit > 7) {
            printf("[cli] removebadge: n must be 0-7 or 1-8\n");
        } else {
            wObtainedBadges &= (uint8_t)~(1u << bit);
            printf("[cli] removebadge %d (bit %d) → wObtainedBadges=0x%02X\n", n, bit, wObtainedBadges);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "clearguardchecks") == 0) {

        ClearEvent(EVENT_PASSED_CASCADEBADGE_CHECK);
        ClearEvent(EVENT_PASSED_THUNDERBADGE_CHECK);
        ClearEvent(EVENT_PASSED_RAINBOWBADGE_CHECK);
        ClearEvent(EVENT_PASSED_SOULBADGE_CHECK);
        ClearEvent(EVENT_PASSED_MARSHBADGE_CHECK);
        ClearEvent(EVENT_PASSED_VOLCANOBADGE_CHECK);
        ClearEvent(EVENT_PASSED_EARTHBADGE_CHECK);
        printf("[cli] clearguardchecks — cleared Route 23 guard pass flags\n");
        write_state();
        return;
    }
    else if (strcmp(verb, "quicksave") == 0) {
        char key[96] = {0};
        char path[160] = {0};
        if (!cli_parse_arg(cmd, 1, key, sizeof(key))) strcpy(key, "1");
        cli_build_state_path(key, path, sizeof(path));
        if (cli_backup_state_if_exists(path) != 0)
            printf("[cli] quicksave backup failed (continuing): %s\n", path);
        if (Save_StateWrite(path) == 0)
            printf("[cli] quicksave %s -> %s\n", key, path);
        else
            printf("[cli] quicksave failed: %s\n", path);
        write_state();
        return;
    }
    else if (strcmp(verb, "quickload") == 0) {
        char key[96] = {0};
        char path[160] = {0};
        if (!cli_parse_arg(cmd, 1, key, sizeof(key))) strcpy(key, "1");
        cli_build_state_path(key, path, sizeof(path));
        if (Save_StateLoad(path) == 0) {
            cli_reload_after_state_load();
            printf("[cli] quickload %s <- %s\n", key, path);
        } else {
            printf("[cli] quickload failed: %s\n", path);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "csave") == 0) {
        char sub[32] = {0};
        char name[96] = {0};
        char path[160] = {0};
        if (!cli_parse_arg(cmd, 1, sub, sizeof(sub))) {
            printf("[cli] csave usage: csave create <name> | csave load <name>\n");
            write_state();
            return;
        }
        if (!cli_parse_arg(cmd, 2, name, sizeof(name))) {
            printf("[cli] csave: missing name\n");
            write_state();
            return;
        }
        cli_build_state_path(name, path, sizeof(path));
        if (strcmp(sub, "create") == 0 || strcmp(sub, "save") == 0) {
            if (cli_backup_state_if_exists(path) != 0)
                printf("[cli] csave backup failed (continuing): %s\n", path);
            if (Save_StateWrite(path) == 0)
                printf("[cli] csave create %s -> %s\n", name, path);
            else
                printf("[cli] csave create failed: %s\n", path);
        } else if (strcmp(sub, "load") == 0) {
            if (Save_StateLoad(path) == 0) {
                cli_reload_after_state_load();
                printf("[cli] csave load %s <- %s\n", name, path);
            } else {
                printf("[cli] csave load failed: %s\n", path);
            }
        } else {
            printf("[cli] csave usage: csave create <name> | csave load <name>\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "replay") == 0) {
        char sub[32] = {0};
        char name[96] = {0};
        char final_path[192] = {0};
        if (!cli_parse_arg(cmd, 1, sub, sizeof(sub))) {
            printf("[cli] replay usage: replay record <name> | replay stop | replay play <name> | replay status\n");
            write_state();
            return;
        }
        if (strcmp(sub, "record") == 0) {
            if (!cli_parse_arg(cmd, 2, name, sizeof(name))) {
                printf("[cli] replay record: missing name\n");
            } else if (replay_start_record(name) == 0) {
                printf("[cli] replay record: started '%s'\n", name);
            } else {
                printf("[cli] replay record: failed\n");
            }
        } else if (strcmp(sub, "stop") == 0) {
            int did = 0;
            if (s_replay_recording) {
                if (replay_stop_record(final_path, sizeof(final_path)) == 0)
                    printf("[cli] replay stop: saved %s\n", final_path);
                else
                    printf("[cli] replay stop: failed to finalize recording\n");
                did = 1;
            }
            if (s_replay_playing) {
                replay_reset_playback();
                printf("[cli] replay stop: playback stopped\n");
                did = 1;
            }
            if (!did) printf("[cli] replay stop: nothing active\n");
        } else if (strcmp(sub, "play") == 0) {
            if (!cli_parse_arg(cmd, 2, name, sizeof(name))) {
                printf("[cli] replay play: missing name\n");
            } else if (replay_start_play(name) == 0) {
                printf("[cli] replay play: started '%s' (%lu frames)\n",
                       name, (unsigned long)s_replay_play_len);
            } else {
                printf("[cli] replay play: failed for '%s'\n", name);
            }
        } else if (strcmp(sub, "status") == 0) {
            printf("[cli] replay status: record=%s play=%s frame=%lu/%lu\n",
                   s_replay_recording ? "ON" : "OFF",
                   s_replay_playing ? "ON" : "OFF",
                   (unsigned long)s_replay_play_pos,
                   (unsigned long)s_replay_play_len);
        } else {
            printf("[cli] replay usage: replay record <name> | replay stop | replay play <name> | replay status\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "setflag") == 0) {

        char tok[16] = {0};
        sscanf(cmd, "%*s %15s", tok);
        int flag = (int)strtol(tok, NULL, 0);
        SetEvent((uint16_t)flag);
        printf("[cli] setflag %d → set\n", flag);
        write_state();
        return;
    }
    else if (strcmp(verb, "clearflag") == 0) {

        char tok[16] = {0};
        sscanf(cmd, "%*s %15s", tok);
        int flag = (int)strtol(tok, NULL, 0);
        ClearEvent((uint16_t)flag);
        printf("[cli] clearflag %d → cleared\n", flag);
        write_state();
        return;
    }
    else if (strcmp(verb, "seafoam_reset") == 0) {

        ClearEvent(EVENT_SEAFOAM1_BOULDER1_DOWN_HOLE);
        ClearEvent(EVENT_SEAFOAM1_BOULDER2_DOWN_HOLE);
        ClearEvent(EVENT_SEAFOAM2_BOULDER1_DOWN_HOLE);
        ClearEvent(EVENT_SEAFOAM2_BOULDER2_DOWN_HOLE);
        ClearEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE);
        ClearEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE);
        ClearEvent(EVENT_SEAFOAM4_BOULDER1_DOWN_HOLE);
        ClearEvent(EVENT_SEAFOAM4_BOULDER2_DOWN_HOLE);
        ClearEvent(EVENT_IN_SEAFOAM_ISLANDS);
        cli_reload_after_state_load();

        ClearEvent(EVENT_SEAFOAM1_BOULDER1_DOWN_HOLE);
        ClearEvent(EVENT_SEAFOAM1_BOULDER2_DOWN_HOLE);
        ClearEvent(EVENT_SEAFOAM2_BOULDER1_DOWN_HOLE);
        ClearEvent(EVENT_SEAFOAM2_BOULDER2_DOWN_HOLE);
        ClearEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE);
        ClearEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE);
        ClearEvent(EVENT_SEAFOAM4_BOULDER1_DOWN_HOLE);
        ClearEvent(EVENT_SEAFOAM4_BOULDER2_DOWN_HOLE);
        Map_BuildScrollView();
        NPC_BuildView(gScrollPxX, gScrollPxY);
        printf("[cli] seafoam_reset: Seafoam flags now: S1[%d,%d] S2[%d,%d] S3[%d,%d] S4[%d,%d] IN=%d\n",
               CheckEvent(EVENT_SEAFOAM1_BOULDER1_DOWN_HOLE),
               CheckEvent(EVENT_SEAFOAM1_BOULDER2_DOWN_HOLE),
               CheckEvent(EVENT_SEAFOAM2_BOULDER1_DOWN_HOLE),
               CheckEvent(EVENT_SEAFOAM2_BOULDER2_DOWN_HOLE),
               CheckEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE),
               CheckEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE),
               CheckEvent(EVENT_SEAFOAM4_BOULDER1_DOWN_HOLE),
               CheckEvent(EVENT_SEAFOAM4_BOULDER2_DOWN_HOLE),
               CheckEvent(EVENT_IN_SEAFOAM_ISLANDS));
        write_state();
        return;
    }
    else if (strcmp(verb, "victoryroad_reset") == 0 ||
             strcmp(verb, "victory_road_reset") == 0 ||
             strcmp(verb, "vr_reset") == 0) {

        ClearEvent(EVENT_VICTORY_ROAD_1_BOULDER_ON_SWITCH);
        ClearEvent(EVENT_VICTORY_ROAD_2_BOULDER_ON_SWITCH1);
        ClearEvent(EVENT_VICTORY_ROAD_2_BOULDER_ON_SWITCH2);
        ClearEvent(EVENT_VICTORY_ROAD_3_BOULDER_ON_SWITCH1);
        ClearEvent(EVENT_VICTORY_ROAD_3_BOULDER_ON_SWITCH2);

        ClearEvent(EVENT_BEAT_VICTORY_ROAD_1_TRAINER_0);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_1_TRAINER_1);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_2_TRAINER_0);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_2_TRAINER_1);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_2_TRAINER_2);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_2_TRAINER_3);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_2_TRAINER_4);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_3_TRAINER_0);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_3_TRAINER_1);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_3_TRAINER_2);
        ClearEvent(EVENT_BEAT_VICTORY_ROAD_3_TRAINER_3);
        ClearEvent(EVENT_BEAT_MOLTRES);

        cli_reload_after_state_load();
        Map_BuildScrollView();
        NPC_BuildView(gScrollPxX, gScrollPxY);

        printf("[cli] victoryroad_reset: switches=[1F:%d 2F:%d,%d 3F:%d,%d] moltres=%d\n",
               CheckEvent(EVENT_VICTORY_ROAD_1_BOULDER_ON_SWITCH),
               CheckEvent(EVENT_VICTORY_ROAD_2_BOULDER_ON_SWITCH1),
               CheckEvent(EVENT_VICTORY_ROAD_2_BOULDER_ON_SWITCH2),
               CheckEvent(EVENT_VICTORY_ROAD_3_BOULDER_ON_SWITCH1),
               CheckEvent(EVENT_VICTORY_ROAD_3_BOULDER_ON_SWITCH2),
               CheckEvent(EVENT_BEAT_MOLTRES));
        write_state();
        return;
    }
    else if (strcmp(verb, "seafoam_state") == 0) {

        char mode[32] = {0};
        if (!cli_parse_arg(cmd, 1, mode, sizeof(mode))) {
            printf("[cli] seafoam_state usage: seafoam_state <reset|b3_current|b4_current>\n");
            write_state();
            return;
        }
        if (strcmp(mode, "reset") == 0) {
            process_cmd("seafoam_reset");
            return;
        }
        if (strcmp(mode, "b3_current") == 0) {
            SetEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE);
            SetEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE);
            ClearEvent(EVENT_SEAFOAM4_BOULDER1_DOWN_HOLE);
            ClearEvent(EVENT_SEAFOAM4_BOULDER2_DOWN_HOLE);
            cli_reload_after_state_load();
            Map_BuildScrollView();
            NPC_BuildView(gScrollPxX, gScrollPxY);
            printf("[cli] seafoam_state b3_current: set flags S3=1,1 S4=0,0 (position unchanged)\n");
            write_state();
            return;
        }
        if (strcmp(mode, "b4_current") == 0) {
            SetEvent(EVENT_SEAFOAM3_BOULDER1_DOWN_HOLE);
            SetEvent(EVENT_SEAFOAM3_BOULDER2_DOWN_HOLE);
            SetEvent(EVENT_SEAFOAM4_BOULDER1_DOWN_HOLE);
            SetEvent(EVENT_SEAFOAM4_BOULDER2_DOWN_HOLE);
            cli_reload_after_state_load();
            Map_BuildScrollView();
            NPC_BuildView(gScrollPxX, gScrollPxY);
            printf("[cli] seafoam_state b4_current: set flags S3=1,1 S4=1,1 (position unchanged)\n");
            write_state();
            return;
        }
        printf("[cli] seafoam_state: unknown mode '%s'\n", mode);
        write_state();
        return;
    }
    else if (strcmp(verb, "unstuck") == 0) {

        int moved = 0;
        if (wCurMap < NUM_MAPS) {
            const map_events_t *ev = &gMapEvents[wCurMap];
            if (ev->warps && ev->num_warps > 0) {
                int best_i = -1;
                int best_d = 0x7fffffff;
                for (int i = 0; i < ev->num_warps; i++) {
                    int wx = (int)ev->warps[i].x;
                    int wy = (int)ev->warps[i].y;
                    int d = abs((int)wXCoord - wx) + abs((int)wYCoord - wy);
                    if (d < best_d) {
                        best_d = d;
                        best_i = i;
                    }
                }
                if (best_i >= 0) {
                    static const int kAdj[4][2] = {
                        { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 }
                    };
                    int wx = (int)ev->warps[best_i].x;
                    int wy = (int)ev->warps[best_i].y;
                    for (int a = 0; a < 4; a++) {
                        int nx = wx + kAdj[a][0];
                        int ny = wy + kAdj[a][1];
                        if (nx < 0 || ny < 0 ||
                            nx >= (int)wCurMapWidth * 2 ||
                            ny >= (int)wCurMapHeight * 2) continue;
                        if (!Tile_IsPassable(Map_GetGameTile(nx, ny))) continue;
                        if (NPC_IsBlocked(nx, ny)) continue;
                        wXCoord = (uint8_t)nx;
                        wYCoord = (uint8_t)ny;
                        wWalkBikeSurfState = 0;
                        gNoClip = 0;
                        cli_reload_after_state_load();
                        Map_BuildScrollView();
                        NPC_BuildView(gScrollPxX, gScrollPxY);
                        printf("[cli] unstuck: moved near nearest warp (%d,%d) -> (%d,%d)\n",
                               wx, wy, nx, ny);
                        moved = 1;
                        break;
                    }
                }
            }
        }
        if (!moved) {
            wWalkBikeSurfState = 0;
            gNoClip = 0;
            Game_WarpToRealMap(0x28, 6, 8);
            printf("[cli] unstuck: fallback to OAKS_LAB @ (6,8)\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "battle_seed") == 0) {
        char tok[32] = {0};
        uint8_t seed;
        if (!cli_parse_arg(cmd, 1, tok, sizeof(tok))) {
            printf("[cli] battle_seed usage: battle_seed <0-255>\n");
            write_state();
            return;
        }
        seed = (uint8_t)strtol(tok, NULL, 0);
        hRandomAdd = seed;
        hRandomSub = (uint8_t)~seed;
        printf("[cli] battle_seed: hRandomAdd=0x%02X hRandomSub=0x%02X\n", hRandomAdd, hRandomSub);
        write_state();
        return;
    }
    else if (strcmp(verb, "rng_state") == 0) {
        printf("[cli] rng_state: add=0x%02X sub=0x%02X frame=%u\n",
               hRandomAdd, hRandomSub, (unsigned)hFrameCounter);
        write_state();
        return;
    }
    else if (strcmp(verb, "sprint_holdb") == 0) {
        char sub[16] = {0};
        if (!cli_parse_arg(cmd, 1, sub, sizeof(sub))) strcpy(sub, "status");
        if (strcmp(sub, "on") == 0 || strcmp(sub, "1") == 0) {
            Player_SetHoldBSprintEnabled(1);
            printf("[cli] sprint_holdb: ON (hold X/B to move 2x)\n");
        } else if (strcmp(sub, "off") == 0 || strcmp(sub, "0") == 0) {
            Player_SetHoldBSprintEnabled(0);
            printf("[cli] sprint_holdb: OFF\n");
        } else if (strcmp(sub, "status") == 0) {
            printf("[cli] sprint_holdb: %s\n",
                   Player_GetHoldBSprintEnabled() ? "ON" : "OFF");
        } else {
            printf("[cli] sprint_holdb usage: sprint_holdb on|off|status\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "npc_walkoff") == 0) {
        debugcli_start_npc_walkoff(1);
        write_state();
        return;
    }
    else if (strcmp(verb, "dsl_bank") == 0) {
        char sub[16] = {0};
        if (!cli_parse_arg(cmd, 1, sub, sizeof(sub))) strcpy(sub, "status");
        if (strcmp(sub, "on") == 0) {
            s_dsl_bank_enabled = 1;
            dsl_bank_load();
            dsl_bank_mark_runtime_tiles_banked();
            dsl_bank_ensure_current_map_spawns();
            dsl_bank_ensure_current_map_tiles();
            s_dsl_bank_last_map = wCurMap;
            dsl_bank_write_cfg();
            dsl_bank_save();
            printf("[cli] dsl_bank: ON (persisting scene NPCs, custom tiles, and tile placements)\n");
        } else if (strcmp(sub, "off") == 0) {
            s_dsl_bank_enabled = 0;
            dsl_bank_write_cfg();
            printf("[cli] dsl_bank: OFF\n");
        } else if (strcmp(sub, "save") == 0) {
            dsl_bank_mark_runtime_tiles_banked();
            dsl_bank_save();
            printf("[cli] dsl_bank: saved\n");
        } else if (strcmp(sub, "load") == 0) {
            dsl_bank_load();
            dsl_bank_ensure_current_map_spawns();
            dsl_bank_ensure_current_map_tiles();
            s_dsl_bank_last_map = wCurMap;
            printf("[cli] dsl_bank: loaded\n");
        } else if (strcmp(sub, "clear") == 0) {
            for (int pi = 0; pi < (int)(sizeof(kDslBankDataPaths) / sizeof(kDslBankDataPaths[0])); pi++)
                remove(kDslBankDataPaths[pi]);
            memset(s_scene_npc_bindings, 0, sizeof(s_scene_npc_bindings));
            memset(s_scene_saved_tiles, 0, sizeof(s_scene_saved_tiles));
            memset(s_scene_saved_blocks, 0, sizeof(s_scene_saved_blocks));
            for (int i = 0; i < SCENE_TILE_PROP_MAX; i++) {
                if (s_scene_tile_props[i].used && s_scene_tile_props[i].banked)
                    memset(&s_scene_tile_props[i], 0, sizeof(s_scene_tile_props[i]));
            }
            Map_BuildScrollView();
            printf("[cli] dsl_bank: cleared\n");
        } else if (strcmp(sub, "status") == 0) {
            int npc_cnt = 0, tile_cnt = 0, custom_tile_cnt = 0, custom_block_cnt = 0;
            for (int i = 0; i < SCENE_ACTOR_MAX; i++) if (s_scene_npc_bindings[i].used) npc_cnt++;
            for (int i = 0; i < SCENE_TILE_PROP_MAX; i++) if (s_scene_tile_props[i].used && s_scene_tile_props[i].banked) tile_cnt++;
            for (int i = 0; i < SCENE_SAVED_TILE_MAX; i++) if (s_scene_saved_tiles[i].used) custom_tile_cnt++;
            for (int i = 0; i < SCENE_SAVED_BLOCK_MAX; i++) if (s_scene_saved_blocks[i].used) custom_block_cnt++;
            printf("[cli] dsl_bank: %s, npcs=%d, tile_placements=%d, custom_tiles=%d, custom_blocks=%d\n",
                   s_dsl_bank_enabled ? "ON" : "OFF", npc_cnt, tile_cnt, custom_tile_cnt, custom_block_cnt);
        } else {
            printf("[cli] dsl_bank usage: dsl_bank on|off|status|save|load|clear\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "scene_npc") == 0 ||
             strcmp(verb, "scene-npc") == 0 ||
             strcmp(verb, "scenenpc") == 0 ||
             strcmp(verb, "npcscene") == 0) {
        char sub[16] = {0};
        if (!cli_parse_arg(cmd, 1, sub, sizeof(sub))) {
            printf("[cli] scene_npc usage:\n");
            printf("      scene_npc add <name> <scene> <sprite> <x_expr> <y_expr>\n");
            printf("      scene_npc list\n");
            printf("      scene_npc clear [name]\n");
            write_state();
            return;
        }
        if (strcmp(sub, "add") == 0) {
            char name[24] = {0}, scene[64] = {0}, sprite[32] = {0}, xexpr[32] = {0}, yexpr[32] = {0};
            int x = 0, y = 0, sprite_id = -1, idx = -1, slot = -1, ncmd;
            if (!cli_parse_arg(cmd, 2, name, sizeof(name)) ||
                !cli_parse_arg(cmd, 3, scene, sizeof(scene)) ||
                !cli_parse_arg(cmd, 4, sprite, sizeof(sprite)) ||
                !cli_parse_arg(cmd, 5, xexpr, sizeof(xexpr)) ||
                !cli_parse_arg(cmd, 6, yexpr, sizeof(yexpr))) {
                printf("[cli] scene_npc add usage: scene_npc add <name> <scene> <sprite> <x_expr> <y_expr>\n");
                write_state();
                return;
            }
            if (!scene_parse_coord_expr(xexpr, 1, &x) || !scene_parse_coord_expr(yexpr, 0, &y)) {
                printf("[cli] scene_npc add: bad coordinate expression(s)\n");
                write_state();
                return;
            }
            sprite_id = scene_parse_sprite(sprite);
            if (sprite_id < 0) {
                printf("[cli] scene_npc add: bad sprite '%s'\n", sprite);
                write_state();
                return;
            }
            ncmd = scene_load_file(scene);
            if (ncmd <= 0) {
                printf("[cli] scene_npc add: scene '%s' not found or empty\n", scene);
                write_state();
                return;
            }
            idx = NPC_DebugSpawn((uint8_t)sprite_id, x, y, 0, 0);
            if (idx < 0) {
                printf("[cli] scene_npc add: failed to spawn NPC\n");
                write_state();
                return;
            }
            slot = scene_npc_binding_alloc();
            if (slot < 0) {
                printf("[cli] scene_npc add: no free bindings\n");
                NPC_DebugDespawn(idx);
                write_state();
                return;
            }
            memset(&s_scene_npc_bindings[slot], 0, sizeof(s_scene_npc_bindings[slot]));
            s_scene_npc_bindings[slot].used = 1;
            s_scene_npc_bindings[slot].npc_idx = idx;
            s_scene_npc_bindings[slot].map_id = wCurMap;
            s_scene_npc_bindings[slot].sprite_id = (uint8_t)sprite_id;
            s_scene_npc_bindings[slot].tile_x = x;
            s_scene_npc_bindings[slot].tile_y = y;
            snprintf(s_scene_npc_bindings[slot].name, sizeof(s_scene_npc_bindings[slot].name), "%s", name);
            snprintf(s_scene_npc_bindings[slot].scene, sizeof(s_scene_npc_bindings[slot].scene), "%s", scene);
            printf("[cli] scene_npc add: '%s' idx=%d map=%u (%d,%d) -> scene '%s'\n",
                   name, idx, (unsigned)wCurMap, x, y, scene);
            if (s_dsl_bank_enabled) dsl_bank_save();
        } else if (strcmp(sub, "list") == 0) {
            int any = 0;
            for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
                if (!s_scene_npc_bindings[i].used) continue;
                any = 1;
                printf("[cli] scene_npc[%d]: name='%s' idx=%d map=%u tile=(%d,%d) sprite=0x%02X scene='%s'\n",
                       i, s_scene_npc_bindings[i].name, s_scene_npc_bindings[i].npc_idx,
                       (unsigned)s_scene_npc_bindings[i].map_id,
                       s_scene_npc_bindings[i].tile_x, s_scene_npc_bindings[i].tile_y,
                       (unsigned)s_scene_npc_bindings[i].sprite_id,
                       s_scene_npc_bindings[i].scene);
            }
            if (!any) printf("[cli] scene_npc list: empty\n");
        } else if (strcmp(sub, "clear") == 0) {
            char name[24] = {0};
            int cleared = 0;
            if (!cli_parse_arg(cmd, 2, name, sizeof(name))) {
                for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
                    if (!s_scene_npc_bindings[i].used) continue;
                    NPC_DebugDespawn(s_scene_npc_bindings[i].npc_idx);
                    memset(&s_scene_npc_bindings[i], 0, sizeof(s_scene_npc_bindings[i]));
                    cleared++;
                }
                printf("[cli] scene_npc clear: cleared all (%d)\n", cleared);
            } else {
                for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
                    if (!s_scene_npc_bindings[i].used) continue;
                    if (strcmp(s_scene_npc_bindings[i].name, name) != 0) continue;
                    NPC_DebugDespawn(s_scene_npc_bindings[i].npc_idx);
                    memset(&s_scene_npc_bindings[i], 0, sizeof(s_scene_npc_bindings[i]));
                    cleared++;
                }
                printf("[cli] scene_npc clear: name='%s' cleared=%d\n", name, cleared);
            }
            if (s_dsl_bank_enabled) dsl_bank_save();
        } else {
            printf("[cli] scene_npc usage:\n");
            printf("      scene_npc add <name> <scene> <sprite> <x_expr> <y_expr>\n");
            printf("      scene_npc list\n");
            printf("      scene_npc clear [name]\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "script_trace") == 0) {
        char sub[16] = {0};
        if (!cli_parse_arg(cmd, 1, sub, sizeof(sub))) strcpy(sub, "status");
        if (strcmp(sub, "on") == 0) {
            s_script_trace_enabled = 1;
            s_script_trace_to_file = 1;
            cli_script_trace_reset_latches();
            printf("[cli] script_trace: ON (file=%s)\n", SCRIPT_TRACE_LOG_PATH);
        } else if (strcmp(sub, "off") == 0) {
            s_script_trace_enabled = 0;
            s_script_trace_to_file = 0;
            printf("[cli] script_trace: OFF\n");
        } else if (strcmp(sub, "file_on") == 0) {
            s_script_trace_to_file = 1;
            printf("[cli] script_trace: file logging ON (%s)\n", SCRIPT_TRACE_LOG_PATH);
        } else if (strcmp(sub, "file_off") == 0) {
            s_script_trace_to_file = 0;
            printf("[cli] script_trace: file logging OFF\n");
        } else if (strcmp(sub, "status") == 0) {
            printf("[cli] script_trace: %s, file=%s\n",
                   s_script_trace_enabled ? "ON" : "OFF",
                   s_script_trace_to_file ? "ON" : "OFF");
        } else {
            printf("[cli] script_trace usage: script_trace on|off|status|file_on|file_off\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "bulbaspriteswap") == 0) {
        int ncmd;
        scene_reset_runtime();
        ncmd = scene_load_file("python_bulbasaur_sprite_inject_poc");
        if (ncmd <= 0) {
            printf("[cli] bulbaspriteswap: failed to load scene 'python_bulbasaur_sprite_inject_poc'\n");
        } else {
            s_scene_active = 1;
            s_scene_pc = 0;
            printf("[cli] bulbaspriteswap: loaded 'python_bulbasaur_sprite_inject_poc' (%d command(s))\n", ncmd);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "scene_run") == 0) {
        char name[64] = {0};
        int ncmd;
        if (!cli_parse_arg(cmd, 1, name, sizeof(name))) {
            printf("[cli] scene_run usage: scene_run <name>  (loads mod_runtime/scenes/<name>.scene)\n");
            write_state();
            return;
        }
        scene_reset_runtime();
        ncmd = scene_load_file(name);
        if (ncmd <= 0) {
            printf("[cli] scene_run: failed to load scene '%s'\n", name);
        } else {
            s_scene_active = 1;
            s_scene_pc = 0;
            printf("[cli] scene_run: loaded '%s' (%d command(s))\n", name, ncmd);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "march_debug") == 0) {
        char mode[16] = {0};
        if (sscanf(cmd, "%*s %15s", mode) != 1) {
            printf("[cli] march_debug usage: march_debug on | march_debug off (currently %s)\n",
                   AmberScript_GetMarchDebug() ? "on" : "off");
            write_state();
            return;
        }
        if (strcmp(mode, "on") == 0) {
            AmberScript_SetMarchDebug(1);
            printf("[cli] march_debug on -- every scene `march` will log its full queue and "
                   "every dispatched step's formation to pokered_log.txt\n");
        } else if (strcmp(mode, "off") == 0) {
            AmberScript_SetMarchDebug(0);
            printf("[cli] march_debug off\n");
        } else {
            printf("[cli] march_debug usage: march_debug on | march_debug off\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "pylaw") == 0) {
        char mode[16] = {0};
        int idx = -1;
        char script[120] = {0};
        if (sscanf(cmd, "%*s %15s %d %119s", mode, &idx, script) < 1) {
            printf("[cli] pylaw usage: pylaw on <npc_idx> <script> | pylaw off | pylaw status\n");
            write_state();
            return;
        }
        if (strcmp(mode, "on") == 0) {
            if (idx < 0 || idx >= NPC_GetCount()) {
                printf("[cli] pylaw: npc_idx out of range\n");
            } else {
                s_py_law_enabled = 1;
                s_py_law_npc_idx = idx;
                s_py_law_frame_accum = 0;
                s_py_law_elapsed_sec = 0;
                snprintf(s_py_law_script, sizeof(s_py_law_script), "%s", script);
                printf("[cli] pylaw on npc=%d script=%s\n", idx, s_py_law_script);
            }
        } else if (strcmp(mode, "off") == 0) {
            s_py_law_enabled = 0;
            printf("[cli] pylaw off\n");
        } else {
            printf("[cli] pylaw status: %s npc=%d script=%s elapsed=%u\n",
                   s_py_law_enabled ? "on" : "off", s_py_law_npc_idx, s_py_law_script,
                   (unsigned)s_py_law_elapsed_sec);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "pylaw_spawn") == 0) {
        char sprite[24] = {0};
        char x[32] = {0}, y[32] = {0}, script[120] = {0};
        int sx, sy, spr, idx;
        if (sscanf(cmd, "%*s %23s %31s %31s %119s", sprite, x, y, script) != 4) {
            printf("[cli] pylaw_spawn usage: pylaw_spawn <sprite> <x_expr> <y_expr> <script>\n");
            write_state();
            return;
        }
        spr = scene_parse_sprite(sprite);
        if (spr <= 0) {
            printf("[cli] pylaw_spawn: bad sprite '%s'\n", sprite);
            write_state();
            return;
        }
        sx = cli_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
        sy = cli_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
        if (!cli_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) sx = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
        if (!cli_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) sy = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
        idx = NPC_DebugSpawn((uint8_t)spr, sx, sy, 0, 0);
        if (idx < 0) {
            printf("[cli] pylaw_spawn: failed to spawn npc\n");
        } else {
            s_py_law_enabled = 1;
            s_py_law_npc_idx = idx;
            s_py_law_frame_accum = 0;
            s_py_law_elapsed_sec = 0;
            snprintf(s_py_law_script, sizeof(s_py_law_script), "%s", script);
            printf("[cli] pylaw_spawn: npc=%d at (%d,%d) sprite=0x%02X script=%s\n",
                   idx, sx, sy, spr, s_py_law_script);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "scene_trigger") == 0) {
        char sub[16] = {0};
        if (!cli_parse_arg(cmd, 1, sub, sizeof(sub))) {
            printf("[cli] scene_trigger usage:\n");
            printf("      scene_trigger set <scene> trigger_point <x_expr> <y_expr> [map]\n");
            printf("      scene_trigger set <scene> trigger_point <x_expr> <y_expr> when event_set|event_clear <event> [map]\n");
            printf("      scene_trigger list\n");
            printf("      scene_trigger clear [scene]\n");
            printf("      x_expr/y_expr: number or player.x[+/-n], player.y[+/-n]\n");
            write_state();
            return;
        }
        if (strcmp(sub, "set") == 0) {
            char scene[64] = {0}, marker[24] = {0}, xexpr[32] = {0}, yexpr[32] = {0}, tok6[64] = {0}, maptok[64] = {0};
            int map_id = (int)wCurMap;
            int x = 0, y = 0;
            int ncmd, slot;
            uint8_t cond_kind = 0;
            uint16_t cond_event = 0;
            if (!cli_parse_arg(cmd, 2, scene, sizeof(scene)) ||
                !cli_parse_arg(cmd, 3, marker, sizeof(marker)) ||
                !cli_parse_arg(cmd, 4, xexpr, sizeof(xexpr)) ||
                !cli_parse_arg(cmd, 5, yexpr, sizeof(yexpr))) {
                printf("[cli] scene_trigger set usage: scene_trigger set <scene> trigger_point <x_expr> <y_expr> [map]\n");
                write_state();
                return;
            }
            if (strcmp(marker, "trigger_point") != 0) {
                printf("[cli] scene_trigger set: expected marker 'trigger_point'\n");
                write_state();
                return;
            }
            if (!scene_parse_coord_expr(xexpr, 1, &x) || !scene_parse_coord_expr(yexpr, 0, &y)) {
                printf("[cli] scene_trigger set: bad coordinate expression(s)\n");
                write_state();
                return;
            }
            if (cli_parse_arg(cmd, 6, tok6, sizeof(tok6))) {
                if (strcmp(tok6, "when") == 0 || strcmp(tok6, "if") == 0) {
                    char condtok[24] = {0}, eventtok[96] = {0}, mapafter[64] = {0};
                    if (!cli_parse_arg(cmd, 7, condtok, sizeof(condtok)) ||
                        !cli_parse_arg(cmd, 8, eventtok, sizeof(eventtok))) {
                        printf("[cli] scene_trigger set: usage ... when event_set|event_clear <event> [map]\n");
                        write_state();
                        return;
                    }
                    if (strcmp(condtok, "event_set") == 0 || strcmp(condtok, "set") == 0) cond_kind = 1;
                    else if (strcmp(condtok, "event_clear") == 0 || strcmp(condtok, "clear") == 0) cond_kind = 2;
                    else {
                        printf("[cli] scene_trigger set: expected event_set or event_clear\n");
                        write_state();
                        return;
                    }
                    if (!cli_resolve_event_token(eventtok, &cond_event)) {
                        printf("[cli] scene_trigger set: unknown event '%s'\n", eventtok);
                        write_state();
                        return;
                    }
                    if (cli_parse_arg(cmd, 9, mapafter, sizeof(mapafter))) {
                        if (!cli_resolve_map_token(mapafter, &map_id)) {
                            printf("[cli] scene_trigger set: unknown map '%s'\n", mapafter);
                            write_state();
                            return;
                        }
                    }
                } else {
                    snprintf(maptok, sizeof(maptok), "%s", tok6);
                    if (!cli_resolve_map_token(maptok, &map_id)) {
                        printf("[cli] scene_trigger set: unknown map '%s'\n", maptok);
                        write_state();
                        return;
                    }
                }
            }

            ncmd = scene_load_file(scene);
            if (ncmd <= 0) {
                printf("[cli] scene_trigger set: scene '%s' not found or empty\n", scene);
                write_state();
                return;
            }
            slot = scene_trigger_find_slot(scene, map_id, x, y);
            if (slot < 0) slot = scene_trigger_alloc_slot();
            if (slot < 0) {
                printf("[cli] scene_trigger set: no free trigger slots (max %d)\n", SCENE_TRIGGER_MAX);
                write_state();
                return;
            }
            s_scene_triggers[slot].used = 1;
            snprintf(s_scene_triggers[slot].scene, sizeof(s_scene_triggers[slot].scene), "%s", scene);
            s_scene_triggers[slot].map_id = (uint8_t)map_id;
            s_scene_triggers[slot].x = x;
            s_scene_triggers[slot].y = y;
            s_scene_triggers[slot].armed = 1;
            s_scene_triggers[slot].cond_kind = cond_kind;
            s_scene_triggers[slot].cond_event = cond_event;
            if (cond_kind == 1) {
                printf("[cli] scene_trigger set: '%s' @ map=%u (%d,%d) when event_set %s (%u)\n",
                       scene, (unsigned)map_id, x, y, EventFlagName(cond_event), (unsigned)cond_event);
            } else if (cond_kind == 2) {
                printf("[cli] scene_trigger set: '%s' @ map=%u (%d,%d) when event_clear %s (%u)\n",
                       scene, (unsigned)map_id, x, y, EventFlagName(cond_event), (unsigned)cond_event);
            } else {
                printf("[cli] scene_trigger set: '%s' @ map=%u (%d,%d)\n", scene, (unsigned)map_id, x, y);
            }
        } else if (strcmp(sub, "list") == 0) {
            int any = 0;
            for (int i = 0; i < SCENE_TRIGGER_MAX; i++) {
                if (!s_scene_triggers[i].used) continue;
                any = 1;
                printf("[cli] scene_trigger[%d]: '%s' map=%u (%d,%d) armed=%d",
                       i, s_scene_triggers[i].scene, (unsigned)s_scene_triggers[i].map_id,
                       s_scene_triggers[i].x, s_scene_triggers[i].y, s_scene_triggers[i].armed);
                if (s_scene_triggers[i].cond_kind == 1) {
                    printf(" when event_set %s (%u)",
                           EventFlagName(s_scene_triggers[i].cond_event),
                           (unsigned)s_scene_triggers[i].cond_event);
                } else if (s_scene_triggers[i].cond_kind == 2) {
                    printf(" when event_clear %s (%u)",
                           EventFlagName(s_scene_triggers[i].cond_event),
                           (unsigned)s_scene_triggers[i].cond_event);
                }
                printf("\n");
            }
            if (!any) printf("[cli] scene_trigger list: empty\n");
        } else if (strcmp(sub, "clear") == 0) {
            char scene[64] = {0};
            int cleared = 0;
            if (!cli_parse_arg(cmd, 2, scene, sizeof(scene))) {
                memset(s_scene_triggers, 0, sizeof(s_scene_triggers));
                printf("[cli] scene_trigger clear: cleared all trigger points\n");
                write_state();
                return;
            }
            for (int i = 0; i < SCENE_TRIGGER_MAX; i++) {
                if (s_scene_triggers[i].used && strcmp(s_scene_triggers[i].scene, scene) == 0) {
                    memset(&s_scene_triggers[i], 0, sizeof(s_scene_triggers[i]));
                    cleared++;
                }
            }
            printf("[cli] scene_trigger clear: cleared %d trigger(s) for '%s'\n", cleared, scene);
        } else {
            printf("[cli] scene_trigger usage:\n");
            printf("      scene_trigger set <scene> trigger_point <x_expr> <y_expr> [map]\n");
            printf("      scene_trigger set <scene> trigger_point <x_expr> <y_expr> when event_set|event_clear <event> [map]\n");
            printf("      scene_trigger list\n");
            printf("      scene_trigger clear [scene]\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "scene_stop") == 0) {
        wJoyIgnore = 0;
        scene_reset_runtime();
        printf("[cli] scene_stop: stopped scene runner\n");
        write_state();
        return;
    }
    else if (strcmp(verb, "story_guard") == 0) {
        char name[32] = {0};
        int missing = 0;
#define GUARD_EVENT(ev, label) do { if (!CheckEvent((ev))) { printf("[story_guard] missing event: %s (%u)\n", (label), (unsigned)(ev)); missing++; } } while (0)
#define GUARD_BADGE(bit, label) do { if (!(wObtainedBadges & (1u << (bit)))) { printf("[story_guard] missing badge: %s\n", (label)); missing++; } } while (0)
        if (!cli_parse_arg(cmd, 1, name, sizeof(name))) {
            printf("[cli] story_guard usage: story_guard <brock|misty|surge|erika|koga|blaine|bike_gate|list>\n");
            write_state();
            return;
        }
        if (strcmp(name, "list") == 0 || strcmp(name, "help") == 0) {
            printf("[cli] story_guard targets: brock, misty, surge, erika, koga, blaine, bike_gate\n");
            write_state();
            return;
        } else if (strcmp(name, "brock") == 0) {
            GUARD_EVENT(EVENT_GOT_POKEDEX, "EVENT_GOT_POKEDEX");
        } else if (strcmp(name, "misty") == 0) {
            GUARD_EVENT(EVENT_GOT_POKEDEX, "EVENT_GOT_POKEDEX");
            GUARD_EVENT(EVENT_BEAT_BROCK, "EVENT_BEAT_BROCK");
            GUARD_BADGE(BADGE_BOULDER, "BOULDER");
        } else if (strcmp(name, "surge") == 0) {
            GUARD_EVENT(EVENT_BEAT_MISTY, "EVENT_BEAT_MISTY");
            GUARD_BADGE(BADGE_CASCADE, "CASCADE");
            GUARD_EVENT(EVENT_GOT_HM01, "EVENT_GOT_HM01");
        } else if (strcmp(name, "erika") == 0) {
            GUARD_EVENT(EVENT_BEAT_LT_SURGE, "EVENT_BEAT_LT_SURGE");
            GUARD_BADGE(BADGE_THUNDER, "THUNDER");
        } else if (strcmp(name, "koga") == 0) {
            GUARD_EVENT(EVENT_BEAT_ERIKA, "EVENT_BEAT_ERIKA");
            GUARD_BADGE(BADGE_RAINBOW, "RAINBOW");
        } else if (strcmp(name, "blaine") == 0) {
            GUARD_EVENT(EVENT_BEAT_KOGA, "EVENT_BEAT_KOGA");
            GUARD_BADGE(BADGE_SOUL, "SOUL");
        } else if (strcmp(name, "bike_gate") == 0) {
            if (Inventory_GetQty(ITEM_BICYCLE) == 0) {
                printf("[story_guard] missing item: BICYCLE\n");
                missing++;
            }
        } else {
            printf("[cli] story_guard: unknown target '%s'\n", name);
            write_state();
            return;
        }
        if (missing == 0) printf("[story_guard] %s: OK\n", name);
        else printf("[story_guard] %s: %d prerequisite(s) missing\n", name, missing);
#undef GUARD_EVENT
#undef GUARD_BADGE
        write_state();
        return;
    }
    else if (strcmp(verb, "eventdiff") == 0) {
        char sub[16] = {0};
        if (!cli_parse_arg(cmd, 1, sub, sizeof(sub))) strcpy(sub, "show");
        if (strcmp(sub, "snapshot") == 0 || strcmp(sub, "snap") == 0) {
            s_eventdiff.valid = 1;
            s_eventdiff.badges = wObtainedBadges;
            s_eventdiff.map = wCurMap;
            s_eventdiff.x = wXCoord;
            s_eventdiff.y = wYCoord;
            s_eventdiff.party_count = wPartyCount;
            for (int i = 0; i < 15; i++)
                s_eventdiff.key_flags[i] = (uint8_t)CheckEvent(s_eventdiff_keys[i]);
            printf("[cli] eventdiff: snapshot captured (map=%u pos=%u,%u badges=0x%02X)\n",
                   s_eventdiff.map, s_eventdiff.x, s_eventdiff.y, s_eventdiff.badges);
        } else if (strcmp(sub, "show") == 0 || strcmp(sub, "diff") == 0) {
            if (!s_eventdiff.valid) {
                printf("[cli] eventdiff: no snapshot; run 'eventdiff snapshot'\n");
            } else {
                printf("[cli] eventdiff: map %u->%u, pos (%u,%u)->(%u,%u), badges 0x%02X->0x%02X, party %u->%u\n",
                       s_eventdiff.map, wCurMap, s_eventdiff.x, s_eventdiff.y, wXCoord, wYCoord,
                       s_eventdiff.badges, wObtainedBadges, s_eventdiff.party_count, wPartyCount);
                for (int i = 0; i < 15; i++) {
                    uint8_t now = (uint8_t)CheckEvent(s_eventdiff_keys[i]);
                    if (now != s_eventdiff.key_flags[i]) {
                        printf("  flag %u: %u -> %u\n",
                               (unsigned)s_eventdiff_keys[i], s_eventdiff.key_flags[i], now);
                    }
                }
            }
        } else {
            printf("[cli] eventdiff usage: eventdiff snapshot | eventdiff show\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "trainer_reset") == 0) {
        char tok[64] = {0};
        char mode[8] = {0};
        if (!cli_parse_arg(cmd, 1, tok, sizeof(tok))) strcpy(tok, "here");
        (void)cli_parse_arg(cmd, 2, mode, sizeof(mode));
        if (cli_is_numeric_token(tok) && (strcmp(mode, "set") == 0 || strcmp(mode, "clear") == 0 || strcmp(mode, "1") == 0 || strcmp(mode, "0") == 0)) {
            int flag = (int)strtol(tok, NULL, 0);
            if (strcmp(mode, "set") == 0 || strcmp(mode, "1") == 0) {
                SetEvent((uint16_t)flag);
                printf("[cli] trainer_reset: set event %d (trainer beaten)\n", flag);
            } else {
                ClearEvent((uint16_t)flag);
                printf("[cli] trainer_reset: cleared event %d (trainer unbeaten)\n", flag);
            }
        } else {
            int map_id = -1;
            int cleared = 0;
            if (!cli_resolve_map_token(tok, &map_id)) {
            printf("[cli] trainer_reset: unknown map '%s' (use map id, map name, or 'here')\n", tok);
                write_state();
                return;
            }
            if (map_id < 0 || map_id >= NUM_MAPS) {
                printf("[cli] trainer_reset: map out of range (%d)\n", map_id);
                write_state();
                return;
            }
            {
                const map_events_t *ev = &gMapEvents[map_id];
                for (int i = 0; i < ev->num_trainers; i++) {
                    ClearEvent(ev->trainers[i].flag_bit);
                    cleared++;
                }
            }
            if (wCurMap == (uint8_t)map_id) {
                NPC_Load();
                Trainer_LoadMap();
            }
            printf("[cli] trainer_reset: cleared %d trainer flag(s) on map %d (%s)\n",
                   cleared, map_id, gMapTable[map_id].name);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "gym_reset") == 0) {
        char gym[16] = {0};
        if (!cli_parse_arg(cmd, 1, gym, sizeof(gym))) {
            printf("[cli] gym_reset usage: gym_reset <brock|misty|surge|erika|koga|blaine|all>\n");
            write_state();
            return;
        }
        if (strcmp(gym, "all") == 0) {
            cli_gym_clear("brock");
            cli_gym_clear("misty");
            cli_gym_clear("surge");
            cli_gym_clear("erika");
            cli_gym_clear("koga");
            cli_gym_clear("blaine");
            printf("[cli] gym_reset: cleared Brock..Blaine leader/trainer/TM progress\n");
        } else {
            cli_gym_clear(gym);
            {
                uint8_t gym_map; int gym_x, gym_y;
                if (strcmp(gym, "brock") == 0) { gym_map = 0x36; gym_x = 4; gym_y = 2; }
                else if (strcmp(gym, "misty") == 0) { gym_map = 0x41; gym_x = 4; gym_y = 8; }
                else if (strcmp(gym, "surge") == 0) { gym_map = 0x5C; gym_x = 5; gym_y = 3; }
                else if (strcmp(gym, "erika") == 0) { gym_map = 0x87; gym_x = 4; gym_y = 12; }
                else if (strcmp(gym, "koga") == 0) { gym_map = 0x9D; gym_x = 4; gym_y = 11; }
                else if (strcmp(gym, "blaine") == 0) { gym_map = 0xA6; gym_x = 3; gym_y = 4; }
                else {
                    printf("[cli] gym_reset: unknown gym '%s'\n", gym);
                    write_state();
                    return;
                }
                Game_WarpToRealMap(gym_map, gym_x, gym_y);
            }
            printf("[cli] gym_reset: cleared %s progress and teleported to leader\n", gym);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "e4_reset") == 0 ||
             strcmp(verb, "elite4_reset") == 0 ||
             strcmp(verb, "elitefour_reset") == 0) {
        ClearEvent(EVENT_BEAT_LORELEIS_ROOM_TRAINER_0);
        ClearEvent(EVENT_AUTOWALKED_INTO_LORELEIS_ROOM);
        ClearEvent(EVENT_BEAT_BRUNOS_ROOM_TRAINER_0);
        ClearEvent(EVENT_AUTOWALKED_INTO_BRUNOS_ROOM);
        ClearEvent(EVENT_BEAT_AGATHAS_ROOM_TRAINER_0);
        ClearEvent(EVENT_AUTOWALKED_INTO_AGATHAS_ROOM);
        ClearEvent(EVENT_BEAT_LANCES_ROOM_TRAINER_0);
        ClearEvent(EVENT_BEAT_LANCE);
        ClearEvent(EVENT_LANCES_ROOM_LOCK_DOOR);

        if (wCurMap == 0xae || wCurMap == 0xf5 || wCurMap == 0xf6 ||
            wCurMap == 0xf7 || wCurMap == 0x71 || wCurMap == 0x78) {
            Map_Load(wCurMap);
            NPC_Load();
            Trainer_LoadMap();
        }

        printf("[cli] e4_reset: cleared Elite Four room beat/autowalk/lock flags\n");
        write_state();
        return;
    }
    else if (strcmp(verb, "tmflow") == 0) {
        char gym[16] = {0};
        char state[16] = {0};
        if (!cli_parse_arg(cmd, 1, gym, sizeof(gym)) || !cli_parse_arg(cmd, 2, state, sizeof(state))) {
            printf("[cli] tmflow usage: tmflow <brock|misty|surge|erika|koga|blaine> <pre|post|done|reset>\n");
            write_state();
            return;
        }
        if (strcmp(state, "reset") == 0 || strcmp(state, "pre") == 0) {
            cli_gym_clear(gym);
            printf("[cli] tmflow: %s set to pre-battle/reset state\n", gym);
        } else {
            if (strcmp(gym, "brock") == 0) {
                SetEvent(EVENT_BEAT_BROCK); wObtainedBadges |= (1u << BADGE_BOULDER);
                if (strcmp(state, "done") == 0) SetEvent(EVENT_GOT_TM34); else ClearEvent(EVENT_GOT_TM34);
            } else if (strcmp(gym, "misty") == 0) {
                SetEvent(EVENT_BEAT_MISTY); wObtainedBadges |= (1u << BADGE_CASCADE);
                if (strcmp(state, "done") == 0) SetEvent(EVENT_GOT_TM11); else ClearEvent(EVENT_GOT_TM11);
            } else if (strcmp(gym, "surge") == 0) {
                SetEvent(EVENT_BEAT_LT_SURGE); wObtainedBadges |= (1u << BADGE_THUNDER);
                if (strcmp(state, "done") == 0) SetEvent(EVENT_GOT_TM24); else ClearEvent(EVENT_GOT_TM24);
            } else if (strcmp(gym, "erika") == 0) {
                SetEvent(EVENT_BEAT_ERIKA); wObtainedBadges |= (1u << BADGE_RAINBOW);
                if (strcmp(state, "done") == 0) SetEvent(EVENT_GOT_TM21); else ClearEvent(EVENT_GOT_TM21);
            } else if (strcmp(gym, "koga") == 0) {
                SetEvent(EVENT_BEAT_KOGA); wObtainedBadges |= (1u << BADGE_SOUL);
                if (strcmp(state, "done") == 0) SetEvent(EVENT_GOT_TM06); else ClearEvent(EVENT_GOT_TM06);
            } else if (strcmp(gym, "blaine") == 0) {
                SetEvent(EVENT_BEAT_BLAINE); wObtainedBadges |= (1u << BADGE_VOLCANO);
                if (strcmp(state, "done") == 0) SetEvent(EVENT_GOT_TM38); else ClearEvent(EVENT_GOT_TM38);
            }
            printf("[cli] tmflow: %s set to %s-TM state\n", gym, state);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "gym_badges_clear") == 0) {
        int keep = 0;
        sscanf(cmd, "%*s %d", &keep);
        if (keep < 0) keep = 0;
        if (keep > 8) keep = 8;
        wObtainedBadges &= (uint8_t)((keep == 0) ? 0 : ((1u << keep) - 1u));
        if (keep <= 0) cli_gym_clear("brock");
        if (keep <= 1) cli_gym_clear("misty");
        if (keep <= 2) cli_gym_clear("surge");
        if (keep <= 3) cli_gym_clear("erika");
        if (keep <= 4) cli_gym_clear("koga");
        if (keep <= 5) cli_gym_clear("blaine");
        printf("[cli] gym_badges_clear: kept first %d badge(s), cleared later gym progress\n", keep);
        write_state();
        return;
    }
    else if (strcmp(verb, "giveitem") == 0) {

        static const struct { const char *name; uint8_t id; } kItems[] = {
            { "master_ball",   0x01 }, { "ultra_ball",    0x02 },
            { "great_ball",    0x03 }, { "poke_ball",     0x04 },
            { "town_map",      0x05 }, { "bicycle",       0x06 },
            { "pokedex",       0x09 }, { "moon_stone",    0x0A },
            { "antidote",      0x0B }, { "burn_heal",     0x0C },
            { "ice_heal",      0x0D }, { "awakening",     0x0E },
            { "parlyz_heal",   0x0F }, { "full_restore",  0x10 },
            { "max_potion",    0x11 }, { "hyper_potion",  0x12 },
            { "super_potion",  0x13 }, { "potion",        0x14 },
            { "escape_rope",   0x1D }, { "repel",         0x1E },
            { "fire_stone",    0x20 }, { "thunder_stone", 0x21 },
            { "water_stone",   0x22 }, { "leaf_stone",    0x2F },
            { "hp_up",         0x23 }, { "protein",       0x24 },
            { "iron",          0x25 }, { "carbos",        0x26 },
            { "calcium",       0x27 }, { "rare_candy",    0x28 },
            { "poke_doll",     0x33 }, { "full_heal",     0x34 },
            { "revive",        0x35 }, { "max_revive",    0x36 },
            { "super_repel",   0x38 }, { "max_repel",     0x39 },
            { "oaks_parcel",   0x46 }, { "parcel",        0x46 },
            { "poke_flute",    0x49 }, { "pokeflute",     0x49 },
            { "hm01",          0xC4 }, { "tm01",          0xC9 },
            { NULL, 0 }
        };
        char id_str[32] = {0};
        int qty = 1;
        sscanf(cmd, "%*s %31s %d", id_str, &qty);
        if (qty < 1) qty = 1;

        int id = -1;

        char lc[32] = {0};
        for (int k = 0; id_str[k] && k < 31; k++)
            lc[k] = (id_str[k] >= 'A' && id_str[k] <= 'Z') ? id_str[k]+32 : id_str[k];
        for (int k = 0; kItems[k].name; k++) {
            if (strcmp(lc, kItems[k].name) == 0) { id = kItems[k].id; break; }
        }

        if (id < 0) {
            char uc[32] = {0};
            for (int k = 0; id_str[k] && k < 31; k++) {
                char c = id_str[k];
                if (c == ' ' || c == '-') c = '_';
                uc[k] = (char)toupper((unsigned char)c);
            }
            for (unsigned k = 0; k < NUM_ITEM_NAMES; k++)
                if (strcmp(uc, kItemNames[k].name) == 0) { id = kItemNames[k].id; break; }
        }

        if (id < 0) id = (int)strtol(id_str, NULL, 0);

        if (id <= 0 || id > 255) {
            printf("[cli] giveitem: unknown item '%s'\n", id_str);
        } else if (Inventory_Add((uint8_t)id, (uint8_t)qty) == 0) {
            printf("[cli] giveitem 0x%02X x%d → added\n", id, qty);
        } else {
            printf("[cli] giveitem: bag full\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "givetm") == 0) {

        if (n < 1 || n > 50) {
            printf("[cli] givetm: n must be 1-50\n");
        } else {
            uint8_t id = (uint8_t)(TM01 + n - 1);
            if (Inventory_Add(id, 1) == 0)
                printf("[cli] givetm %d → TM%02d (0x%02X) added\n", n, n, id);
            else
                printf("[cli] givetm: bag full\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "givehm") == 0) {

        if (n < 1 || n > 5) {
            printf("[cli] givehm: n must be 1-5\n");
        } else {
            uint8_t id = (uint8_t)(HM01 + n - 1);
            if (Inventory_Add(id, 1) == 0)
                printf("[cli] givehm %d → HM%02d (0x%02X) added\n", n, n, id);
            else
                printf("[cli] givehm: bag full\n");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "dex_fill") == 0) {

        char which[16] = {0};
        sscanf(cmd, "%*s %15s", which);
        int do_seen  = (which[0] == '\0' || strcmp(which, "all") == 0 || strcmp(which, "seen")  == 0);
        int do_owned = (which[0] == '\0' || strcmp(which, "all") == 0 || strcmp(which, "owned") == 0);
        if (!do_seen && !do_owned) {
            printf("[cli] dex_fill: usage: dex_fill [seen|owned|all]\n");
        } else {
            if (do_seen)  for (int i = 0; i < 19; i++) wPokedexSeen[i]  = 0xFF;
            if (do_owned) for (int i = 0; i < 19; i++) wPokedexOwned[i] = 0xFF;

            if (do_seen)  wPokedexSeen[18]  &= 0x7F;
            if (do_owned) wPokedexOwned[18] &= 0x7F;
            printf("[cli] dex_fill: %s%s%s set to complete (151/151)\n",
                   do_seen ? "seen" : "", (do_seen && do_owned) ? " + " : "", do_owned ? "owned" : "");
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "savemenu") == 0) {

        const char *why = savemenu_blocked_reason();
        if (why) {
            savemenu_finish("NO", why);
            return;
        }
        if (s_savemenu_state != SM_IDLE) {
            savemenu_finish("NO", "a save is already in progress");
            return;
        }
        s_savemenu_state = SM_OPENING;
        s_savemenu_wait  = 0;

        s_savemenu_guard = 900;
        printf("[savemenu] driving START -> SAVE -> YES\n");
        fflush(stdout);
        return;
    }
    else if (strcmp(verb, "options") == 0 || strcmp(verb, "presmenu") == 0) {

        char a1[16] = {0};
        cli_parse_arg(cmd, 1, a1, sizeof(a1));
        if (strcmp(a1, "off") == 0)     PresentationMenu_Close();
        else if (strcmp(a1, "on") == 0) PresentationMenu_Open();
        else                            PresentationMenu_Toggle();
        printf("[cli] options: %s\n", PresentationMenu_IsOpen() ? "open" : "closed");
        write_state();
        return;
    }
    else if (strcmp(verb, "screenshot") == 0 || strcmp(verb, "shot") == 0) {

        char a1[192] = {0};
        cli_parse_arg(cmd, 1, a1, sizeof(a1));
        const char *path = a1[0] ? a1 : "bugs/shot.bmp";
        int ok = (Display_SaveScreenshot(path) == 0);
        printf("[cli] screenshot: %s %s\n", ok ? "wrote" : "FAILED", path);
        FILE *fp = fopen(STATE_FILE, "w");
        if (fp) {
            fprintf(fp, "=== SCREENSHOT ===\n%s %s\n",
                    ok ? "wrote" : "FAILED", path);
            fclose(fp);
        }
        return;
    }
    else if (strcmp(verb, "wild") == 0) {

        int dex = 19, level = 5;
        sscanf(cmd, "%*s %d %d", &dex, &level);
        if (dex < 1 || dex > 151 || level < 1 || level > 100) {
            printf("[cli] wild: usage wild <dex 1-151> [level 1-100]\n");
        } else if (wPartyCount == 0) {
            printf("[cli] wild: party is empty -- givemon first\n");
        } else {
            Game_StartWildBattleScripted(gDexToSpecies[dex], (uint8_t)level);
            printf("[cli] wild: %s Lv%d\n", Pokemon_GetName(dex), level);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "givemon") == 0) {

        int dex = 1, level = 20;
        sscanf(cmd, "%*s %d %d", &dex, &level);
        if (dex < 1 || dex > 151) {
            printf("[cli] givemon: dex must be 1-151\n");
        } else if (level < 1 || level > 100) {
            printf("[cli] givemon: level must be 1-100\n");
        } else {
            uint8_t species = gDexToSpecies[dex];
            int slot = (wPartyCount < 6) ? wPartyCount : 5;
            Pokemon_InitMon(&wPartyMons[slot], species, (uint8_t)level);
            memcpy(wPartyMonOT[slot], wPlayerName, NAME_LENGTH);
            Pokemon_EncodeNameString(Pokemon_GetNameBySpecies(species), wPartyMonNicks[slot]);
            wPartySpecies[slot] = species;
            if (slot + 1 < PARTY_LENGTH) wPartySpecies[slot + 1] = 0xFF;
            if (wPartyCount < 6) wPartyCount++;
            printf("[cli] givemon: slot %d → %s Lv%d (species 0x%02X)\n",
                   slot + 1, Pokemon_GetName(dex), level, species);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "givemonx") == 0) {

        char s_tok[40] = {0};
        int level = 20;
        int sid;
        if (sscanf(cmd, "%*s %39s %d", s_tok, &level) < 1) {
            printf("[cli] givemonx usage: givemonx <species> [level]\n");
            return;
        }
        sid = cli_resolve_species_id(s_tok);
        if (sid <= 0 || sid > 255) {
            printf("[cli] givemonx: bad species '%s'\n", s_tok);
            return;
        }
        if (level < 1 || level > 100) {
            printf("[cli] givemonx: level must be 1-100\n");
            return;
        }
        {
            int slot = (wPartyCount < 6) ? wPartyCount : 5;
            Pokemon_InitMon(&wPartyMons[slot], (uint8_t)sid, (uint8_t)level);
            memcpy(wPartyMonOT[slot], wPlayerName, NAME_LENGTH);
            Pokemon_EncodeNameString(Pokemon_GetNameBySpecies((uint8_t)sid), wPartyMonNicks[slot]);
            wPartySpecies[slot] = (uint8_t)sid;
            if (slot + 1 < PARTY_LENGTH) wPartySpecies[slot + 1] = 0xFF;
            if (wPartyCount < 6) wPartyCount++;
            printf("[cli] givemonx: slot %d -> %s Lv%d (species 0x%02X)\n",
                   slot + 1, Pokemon_GetNameBySpecies((uint8_t)sid), level, sid);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "magneton50") == 0) {

        uint8_t species = gDexToSpecies[82];
        int slot = (wPartyCount < 6) ? wPartyCount : 5;
        Pokemon_InitMon(&wPartyMons[slot], species, 50);
        if (wPartyCount < 6) wPartyCount++;

        wPartyMons[slot].base.moves[0] = 85;
        wPartyMons[slot].base.moves[1] = 86;
        wPartyMons[slot].base.moves[2] = 87;
        wPartyMons[slot].base.moves[3] = 48;
        wPartyMons[slot].base.pp[0] = gMoves[85].pp;
        wPartyMons[slot].base.pp[1] = gMoves[86].pp;
        wPartyMons[slot].base.pp[2] = gMoves[87].pp;
        wPartyMons[slot].base.pp[3] = gMoves[48].pp;
        wPartyMons[slot].base.hp = wPartyMons[slot].max_hp;
        wPartyMons[slot].base.status = 0;

        printf("[cli] magneton50: slot %d -> MAGNETON Lv50 (THUNDERBOLT / THUNDER WAVE / THUNDER / SUPERSONIC)\n",
               slot + 1);
        write_state();
        return;
    }
    else if (strcmp(verb, "bulba15") == 0 || strcmp(verb, "givemon_bulba15") == 0) {

        uint8_t species = gDexToSpecies[1];
        Pokemon_InitMon(&wPartyMons[0], species, 15);
        if (wPartyCount < 1) wPartyCount = 1;

        wPartyMons[0].base.moves[0] = 79;
        wPartyMons[0].base.moves[1] = 75;
        wPartyMons[0].base.moves[2] = 73;
        wPartyMons[0].base.moves[3] = 22;
        wPartyMons[0].base.pp[0] = gMoves[79].pp;
        wPartyMons[0].base.pp[1] = gMoves[75].pp;
        wPartyMons[0].base.pp[2] = gMoves[73].pp;
        wPartyMons[0].base.pp[3] = gMoves[22].pp;
        wPartyMons[0].base.hp = wPartyMons[0].max_hp;
        wPartyMons[0].base.status = 0;

        printf("[cli] bulba15: slot 1 -> BULBASAUR Lv15 (SLEEP POWDER / RAZOR LEAF / LEECH SEED / VINE WHIP)\n");
        write_state();
        return;
    }
    else if (strcmp(verb, "squirtle15") == 0 || strcmp(verb, "givemon_squirtle15") == 0) {

        uint8_t species = gDexToSpecies[7];
        Pokemon_InitMon(&wPartyMons[0], species, 15);
        if (wPartyCount < 1) wPartyCount = 1;

        wPartyMons[0].base.moves[0] = 145;
        wPartyMons[0].base.moves[1] = 55;
        wPartyMons[0].base.moves[2] = 33;
        wPartyMons[0].base.moves[3] = 39;
        wPartyMons[0].base.pp[0] = gMoves[145].pp;
        wPartyMons[0].base.pp[1] = gMoves[55].pp;
        wPartyMons[0].base.pp[2] = gMoves[33].pp;
        wPartyMons[0].base.pp[3] = gMoves[39].pp;
        wPartyMons[0].base.hp = wPartyMons[0].max_hp;
        wPartyMons[0].base.status = 0;

        printf("[cli] squirtle15: slot 1 -> SQUIRTLE Lv15 (BUBBLE / WATER GUN / TACKLE / TAIL WHIP)\n");
        write_state();
        return;
    }
    else if (strcmp(verb, "teachmove") == 0) {

        int slot = 1;
        char move_str[16] = {0};
        sscanf(cmd, "%*s %d %15s", &slot, move_str);
        int move_id = (int)strtol(move_str, NULL, 0);
        slot--;
        if (slot < 0 || slot >= (int)wPartyCount) {
            printf("[cli] teachmove: slot must be 1-%d\n", wPartyCount);
        } else if (move_id < 1 || move_id > 0xA5) {
            printf("[cli] teachmove: invalid move id 0x%02X\n", move_id);
        } else {
            uint8_t *moves = wPartyMons[slot].base.moves;
            int target = 0;
            for (int i = 0; i < 4; i++) {
                if (moves[i] == 0) { target = i; break; }
                if (i == 3) target = 0;
            }
            moves[target] = (uint8_t)move_id;
            printf("[cli] teachmove: slot %d move[%d] = 0x%02X\n",
                   slot + 1, target, move_id);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "teach") == 0) {

        int slot = 1;
        char move_str[32] = {0};
        sscanf(cmd, "%*s %d %31s", &slot, move_str);
        slot--;

        int move_id = cli_resolve_move_id(move_str);

        if (slot < 0 || slot >= (int)wPartyCount) {
            printf("[cli] teach: slot must be 1-%d\n", wPartyCount);
        } else if (move_id < 1 || move_id > 0xA5) {
            printf("[cli] teach: unknown move '%s'\n", move_str);
        } else {
            uint8_t *moves = wPartyMons[slot].base.moves;
            uint8_t *pp    = wPartyMons[slot].base.pp;
            int target = 3;
            for (int i = 0; i < 4; i++) {
                if (moves[i] == 0) { target = i; break; }
            }
            moves[target] = (uint8_t)move_id;
            pp[target]    = gMoves[move_id].pp;
            printf("[cli] teach: slot %d move[%d] = 0x%02X (pp=%d)\n",
                   slot + 1, target, move_id, pp[target]);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "movetestteam1") == 0) {

        static const char *const kMoveNames24[24] = {
            "ABSORB", "ACID", "ACID ARMOR", "AGILITY",
            "AMNESIA", "AURORA BEAM", "BARRAGE", "BARRIER",
            "BIDE", "BIND", "BITE", "BLIZZARD",
            "BODY SLAM", "BONE CLUB", "BONEMERANG", "BUBBLE",
            "BUBBLEBEAM", "CLAMP", "COMET PUNCH", "CONFUSE RAY",
            "CONFUSION", "CONSTRICT", "CONVERSION", "COUNTER"
        };
        uint8_t move_ids[24];
        int ok = 1;
        for (int i = 0; i < 24; i++) {
            int id = cli_resolve_move_id(kMoveNames24[i]);
            if (id < 1 || id > 0xA5) {
                printf("[cli] movetestteam1: failed to resolve move '%s'\n", kMoveNames24[i]);
                ok = 0;
                break;
            }
            move_ids[i] = (uint8_t)id;
        }
        if (!ok) {
            write_state();
            return;
        }

        wPartyCount = PARTY_LENGTH;
        for (int p = 0; p < PARTY_LENGTH; p++) {
            Pokemon_InitMon(&wPartyMons[p], STARTER1, 50);
            wPartyMons[p].base.status = 0;
            wPartyMons[p].base.hp = wPartyMons[p].max_hp;
            for (int m = 0; m < 4; m++) {
                uint8_t mid = move_ids[p * 4 + m];
                wPartyMons[p].base.moves[m] = mid;
                wPartyMons[p].base.pp[m] = gMoves[mid].pp;
            }
        }

        if (wPlayerMonNumber == 0) {
            for (int m = 0; m < 4; m++) {
                wBattleMon.moves[m] = wPartyMons[0].base.moves[m];
                wBattleMon.pp[m] = wPartyMons[0].base.pp[m];
            }
        }

        printf("[cli] movetestteam1: set 6-mon party with 24 move-animation test moves (slots 1-24)\n");
        write_state();
        return;
    }
    else if (strcmp(verb, "movetestteam2") == 0) {

        static const char *const kMoveNames24[24] = {
            "CRABHAMMER", "CUT", "DEFENSE CURL", "DIG",
            "DISABLE", "DIZZY PUNCH", "DOUBLE KICK", "DOUBLE SLAP",
            "DOUBLE TEAM", "DOUBLE-EDGE", "DRAGON RAGE", "DREAM EATER",
            "DRILL PECK", "EARTHQUAKE", "EGG BOMB", "EMBER",
            "EXPLOSION", "FIRE BLAST", "FIRE PUNCH", "FIRE SPIN",
            "FISSURE", "FLAMETHROWER", "FLASH", "FLY"
        };
        static const uint8_t kSpecies[PARTY_LENGTH] = {
            SPECIES_BULBASAUR,
            SPECIES_CHARMANDER,
            SPECIES_SQUIRTLE,
            SPECIES_PIKACHU,
            SPECIES_EEVEE,
            SPECIES_DRATINI
        };
        uint8_t move_ids[24];
        int ok = 1;
        for (int i = 0; i < 24; i++) {
            int id = cli_resolve_move_id(kMoveNames24[i]);
            if (id < 1 || id > 0xA5) {
                printf("[cli] movetestteam2: failed to resolve move '%s'\n", kMoveNames24[i]);
                ok = 0;
                break;
            }
            move_ids[i] = (uint8_t)id;
        }
        if (!ok) {
            write_state();
            return;
        }

        wPartyCount = PARTY_LENGTH;
        for (int p = 0; p < PARTY_LENGTH; p++) {
            Pokemon_InitMon(&wPartyMons[p], kSpecies[p], 50);
            wPartyMons[p].base.status = 0;
            wPartyMons[p].base.hp = wPartyMons[p].max_hp;
            for (int m = 0; m < 4; m++) {
                uint8_t mid = move_ids[p * 4 + m];
                wPartyMons[p].base.moves[m] = mid;
                wPartyMons[p].base.pp[m] = gMoves[mid].pp;
            }
        }

        if (wPlayerMonNumber == 0) {
            for (int m = 0; m < 4; m++) {
                wBattleMon.moves[m] = wPartyMons[0].base.moves[m];
                wBattleMon.pp[m] = wPartyMons[0].base.pp[m];
            }
        }

        printf("[cli] movetestteam2: set 6 unique mons with move-animation test moves (slots 25-48)\n");
        write_state();
        return;
    }
    else if (strcmp(verb, "sethealth") == 0) {

        int slot = 0, hp = 0;
        sscanf(cmd, "%*s %d %d", &slot, &hp);
        slot--;
        if (slot < 0 || slot >= (int)wPartyCount) {
            printf("[cli] sethealth: slot must be 1-%d\n", wPartyCount);
        } else {
            if (hp < 0) hp = 0;
            if (hp > (int)wPartyMons[slot].max_hp) hp = (int)wPartyMons[slot].max_hp;
            wPartyMons[slot].base.hp = (uint16_t)hp;

            if (slot == (int)wPlayerMonNumber) wBattleMon.hp = (uint16_t)hp;
            printf("[cli] sethealth: slot %d HP → %d/%d\n",
                   slot + 1, hp, (int)wPartyMons[slot].max_hp);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "poison") == 0) {

        int slot = 1;
        sscanf(cmd, "%*s %d", &slot);
        slot--;
        if (slot < 0 || slot >= (int)wPartyCount) {
            printf("[cli] poison: slot must be 1-%d\n", wPartyCount);
        } else {
            Poison_DebugApply(slot);
            printf("[cli] poison: slot %d now poisoned (status=0x%02X, HP=%d/%d)\n",
                   slot + 1, wPartyMons[slot].base.status,
                   (int)wPartyMons[slot].base.hp, (int)wPartyMons[slot].max_hp);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "healparty") == 0) {

        for (int i = 0; i < (int)wPartyCount; i++) {
            wPartyMons[i].base.hp     = wPartyMons[i].max_hp;
            wPartyMons[i].base.status = 0;
        }
        printf("[cli] healparty: all %d mons fully healed\n", wPartyCount);
        write_state();
        return;
    }
    else if (strcmp(verb, "pc_send") == 0) {

        int slot = 0;
        sscanf(cmd, "%*s %d", &slot);
        slot--;
        if (slot < 0 || slot >= (int)wPartyCount) {
            printf("[cli] pc_send: slot must be 1-%d\n", wPartyCount);
        } else if (wPartyCount <= 1) {
            printf("[cli] pc_send: can't deposit your last party mon\n");
        } else if (!Pokemon_DepositPartyMonToBox(slot)) {
            printf("[cli] pc_send: current box is full\n");
        } else {
            printf("[cli] pc_send: deposited party slot %d to BOX %d\n",
                   slot + 1, (wCurrentBoxNum % NUM_BOXES) + 1);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "setmoney") == 0) {

        int amount = 0;
        sscanf(cmd, "%*s %d", &amount);
        if (amount < 0)      amount = 0;
        if (amount > 999999) amount = 999999;
        int a = amount;
        wPlayerMoney[2] = (uint8_t)((a % 10) | ((a / 10 % 10) << 4)); a /= 100;
        wPlayerMoney[1] = (uint8_t)((a % 10) | ((a / 10 % 10) << 4)); a /= 100;
        wPlayerMoney[0] = (uint8_t)((a % 10) | ((a / 10 % 10) << 4));
        printf("[cli] setmoney: $%d\n", amount);
        write_state();
        return;
    }
    else if (strcmp(verb, "setcoins") == 0) {

        int amount = 0;
        sscanf(cmd, "%*s %d", &amount);
        if (amount < 0)    amount = 0;
        if (amount > 9999) amount = 9999;
        int a = amount;
        wPlayerCoins[1] = (uint8_t)((a % 10) | ((a / 10 % 10) << 4)); a /= 100;
        wPlayerCoins[0] = (uint8_t)((a % 10) | ((a / 10 % 10) << 4));
        printf("[cli] setcoins: %d coins\n", amount);
        write_state();
        return;
    }
    else if (strcmp(verb, "listbag") == 0) {

        printf("[cli] bag (%d items):\n", wNumBagItems);
        for (int i = 0; i < (int)wNumBagItems; i++) {
            uint8_t id  = wBagItems[i * 2];
            uint8_t qty = wBagItems[i * 2 + 1];
            char name[20];
            Inventory_DecodeASCII(id, name, sizeof(name));
            printf("  [%d] 0x%02X %-18s x%d\n", i + 1, id, name, qty);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "safari_state") == 0 ||
             strcmp(verb, "safari-state") == 0 ||
             strcmp(verb, "safaristate") == 0 ||
             strcmp(verb, "safari") == 0) {

        uint16_t steps = 0;
        uint8_t balls = 0;
        uint8_t script_state = 0;
        char steps_arg[32] = {0};
        SafariZoneScripts_DebugGetState(&steps, &balls, &script_state);
        if (cli_parse_arg(cmd, 1, steps_arg, sizeof(steps_arg))) {
            int new_steps = atoi(steps_arg);
            if (new_steps < 0) new_steps = 0;
            if (new_steps > 502) new_steps = 502;
            SafariZoneScripts_DebugSetState((uint16_t)new_steps);
            SafariZoneScripts_DebugGetState(&steps, &balls, &script_state);
            printf("[cli] safari_state: steps set to %u\n", (unsigned)steps);
            write_state();
            return;
        }
        printf("[cli] safari_state: in_zone=%d game_over=%d map=0x%02X pos=(%d,%d) steps=%u balls=%u script=%u\n",
               CheckEvent(EVENT_IN_SAFARI_ZONE) ? 1 : 0,
               CheckEvent(EVENT_SAFARI_GAME_OVER) ? 1 : 0,
               (unsigned)wCurMap, (int)wXCoord, (int)wYCoord,
               (unsigned)steps, (unsigned)balls, (unsigned)script_state);
        write_state();
        return;
    }
    else if (strcmp(verb, "checkpoint") == 0) {

        char cp[32] = {0};
        sscanf(cmd, "%*s %31s", cp);

        cli_force_interrupt_runtime();

        if (strcmp(cp, "verify") == 0) {
            char target[32] = {0};
            char temp_path[160] = "bugs/cli_checkpoint_verify_tmp.state";
            if (!cli_parse_arg(cmd, 2, target, sizeof(target)) || target[0] == '\0') {
                printf("[cli] checkpoint verify usage: checkpoint verify <name>\n");
                write_state();
                return;
            }
            if (strcmp(target, "verify") == 0) {
                printf("[cli] checkpoint verify: refusing recursive target 'verify'\n");
                write_state();
                return;
            }
            if (Save_StateWrite(temp_path) != 0) {
                printf("[cli] checkpoint verify: failed to save temp state\n");
                write_state();
                return;
            }
            s_eventdiff.valid = 1;
            s_eventdiff.badges = wObtainedBadges;
            s_eventdiff.map = wCurMap;
            s_eventdiff.x = wXCoord;
            s_eventdiff.y = wYCoord;
            s_eventdiff.party_count = wPartyCount;
            for (int i = 0; i < 15; i++)
                s_eventdiff.key_flags[i] = (uint8_t)CheckEvent(s_eventdiff_keys[i]);

            {
                char runbuf[64];
                snprintf(runbuf, sizeof(runbuf), "checkpoint %s", target);
                process_cmd(runbuf);
            }

            printf("[cli] checkpoint verify %s:\n", target);
            printf("  map %u->%u, pos (%u,%u)->(%u,%u), badges 0x%02X->0x%02X, party %u->%u\n",
                   s_eventdiff.map, wCurMap, s_eventdiff.x, s_eventdiff.y, wXCoord, wYCoord,
                   s_eventdiff.badges, wObtainedBadges, s_eventdiff.party_count, wPartyCount);
            for (int i = 0; i < 15; i++) {
                uint8_t now = (uint8_t)CheckEvent(s_eventdiff_keys[i]);
                if (now != s_eventdiff.key_flags[i]) {
                    printf("  flag %u: %u -> %u\n",
                           (unsigned)s_eventdiff_keys[i], s_eventdiff.key_flags[i], now);
                }
            }

            if (Save_StateLoad(temp_path) == 0) cli_reload_after_state_load();
            remove(temp_path);
            write_state();
            return;
        }

        if (strcmp(cp, "parcel_ready") == 0) {

            SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
            SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
            SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
            SetEvent(EVENT_GOT_STARTER);
            SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);

            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 5);
                wPartyCount = 1;
            }

            extern void Map_Load(uint8_t map_id);
            Game_WarpToRealMap(0x2a, 3, 6);
            printf("[cli] checkpoint: parcel_ready — at Viridian Mart\n");
        } else if (strcmp(cp, "pokedex_ready") == 0) {

            SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
            SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
            SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
            SetEvent(EVENT_GOT_STARTER);
            SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
            SetEvent(EVENT_GOT_OAKS_PARCEL);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 5);
                wPartyCount = 1;
            }
            extern void Map_Load(uint8_t map_id);
            Game_WarpToRealMap(0x28, 6, 8);

            if (Inventory_GetQty(ITEM_OAKS_PARCEL) == 0)
                Inventory_Add(ITEM_OAKS_PARCEL, 1);
            printf("[cli] checkpoint: pokedex_ready — at Oak's Lab with parcel\n");
        } else if (strcmp(cp, "mt_moon") == 0) {

            SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
            SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
            SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
            SetEvent(EVENT_GOT_STARTER);
            SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
            SetEvent(EVENT_GOT_OAKS_PARCEL);
            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);

            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 25);
                wPartyCount = 1;
            }
            extern void Map_Load(uint8_t map_id);
            Game_WarpToRealMap(0x3d, 13, 10);
            printf("[cli] checkpoint: mt_moon — at Mt. Moon B2F, south of fossils\n");
        } else if (strcmp(cp, "cerulean") == 0) {

            SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
            SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
            SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
            SetEvent(EVENT_GOT_STARTER);
            SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
            SetEvent(EVENT_GOT_OAKS_PARCEL);
            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 25);
                wPartyCount = 1;
            }
            extern void Map_Load(uint8_t map_id);
            Game_WarpToRealMap(3, 20, 8);
            printf("[cli] checkpoint: cerulean — at Cerulean City bridge (rival trigger at y=6), beat_rival=%d\n",
                   CheckEvent(EVENT_BEAT_CERULEAN_RIVAL));
        } else if (strcmp(cp, "route22") == 0) {

            SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
            SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
            SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
            SetEvent(EVENT_GOT_STARTER);
            SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
            SetEvent(EVENT_GOT_OAKS_PARCEL);
            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_1ST_ROUTE22_RIVAL_BATTLE);
            SetEvent(EVENT_ROUTE22_RIVAL_WANTS_BATTLE);
            ClearEvent(EVENT_BEAT_ROUTE22_RIVAL_1ST_BATTLE);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 10);
                wPartyCount = 1;
            }
            extern void Map_Load(uint8_t map_id);
            Game_WarpToRealMap(0x21, 31, 5);
            printf("[cli] checkpoint: route22 — on Route 22, walk left to trigger rival\n");
        } else if (strcmp(cp, "brock") == 0) {

            SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
            SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
            SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
            SetEvent(EVENT_GOT_STARTER);
            SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
            SetEvent(EVENT_GOT_OAKS_PARCEL);
            SetEvent(EVENT_GOT_POKEDEX);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 15);
                wPartyCount = 1;
            }
            ClearEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            ClearEvent(EVENT_BEAT_BROCK);
            Game_WarpToRealMap(0x36, 4, 2);
            printf("[cli] checkpoint: brock — inside Pewter Gym\n");
        } else if (strcmp(cp, "misty") == 0) {
            SetEvent(EVENT_GOT_OAKS_PARCEL);
            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
            ClearEvent(EVENT_BEAT_MISTY);
            ClearEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            ClearEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            ClearEvent(EVENT_GOT_TM11);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 25);
                wPartyCount = 1;
            }
            Game_WarpToRealMap(0x41, 4, 8);
            printf("[cli] checkpoint: misty — inside Cerulean Gym\n");
        } else if (strcmp(cp, "cerulean_rocket") == 0) {

            SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
            SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
            SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
            SetEvent(EVENT_GOT_STARTER);
            SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
            SetEvent(EVENT_GOT_OAKS_PARCEL);
            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            SetEvent(EVENT_GOT_SS_TICKET);
            ClearEvent(EVENT_BEAT_CERULEAN_ROCKET_THIEF);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 25);
                wPartyCount = 1;
            }
            extern void Map_Load(uint8_t map_id);
            Game_WarpToRealMap(3, 30, 9);
            printf("[cli] checkpoint: cerulean_rocket — at Rocket thief trigger (30,9) in Cerulean City\n");
        } else if (strcmp(cp, "ss_anne_hm") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            ClearEvent(EVENT_SS_ANNE_LEFT);
            Inventory_Add(ITEM_SS_TICKET, 1);
            Inventory_Add(HM01, 1);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 25);
                wPartyCount = 1;
            }
            extern void Map_Load(uint8_t map_id);
            Game_WarpToRealMap(0x5F, 26, 1);
            printf("[cli] checkpoint: ss_anne_hm — inside SS Anne 1F, walk north to dock then departure\n");
        } else if (strcmp(cp, "liftkey_reset") == 0) {

            ClearEvent(EVENT_BEAT_ROCKET_HIDEOUT_4_TRAINER_2);
            ClearEvent(EVENT_ROCKET_DROPPED_LIFT_KEY);
            if (0xCA < 248)
                wPickedUpItems[0xCA] &= (uint16_t)~(1u << 4);

            if (wCurMap == 0xCA) {
                extern void NPC_Load(void);
                extern void RocketHideoutB4FScripts_OnMapLoad(void);
                NPC_Load();
                RocketHideoutB4FScripts_OnMapLoad();
            }
            printf("[cli] checkpoint: liftkey_reset — cleared flags 441/715 and B4F Lift Key pickup bit\n");
        } else if (strcmp(cp, "giovanni_reset") == 0) {

            ClearEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
            if (0xCA < 248)
                wPickedUpItems[0xCA] &= (uint16_t)~(1u << 3);

            if (wCurMap == 0xCA) {
                extern void NPC_Load(void);
                extern void RocketHideoutB4FScripts_OnMapLoad(void);
                NPC_Load();
                RocketHideoutB4FScripts_OnMapLoad();
            }
            printf("[cli] checkpoint: giovanni_reset — cleared Giovanni flag and B4F Silph Scope pickup bit\n");
        } else if (strcmp(cp, "giovanni_ready") == 0) {

            SetEvent(EVENT_FOUND_ROCKET_HIDEOUT);
            SetEvent(EVENT_ROCKET_DROPPED_LIFT_KEY);
            SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_4_TRAINER_0);
            SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_4_TRAINER_1);
            ClearEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
            if (Inventory_GetQty(0x4A) == 0)
                Inventory_Add(0x4A, 1);
            if (0xCA < 248)
                wPickedUpItems[0xCA] &= (uint16_t)~(1u << 3);

            Game_WarpToRealMap(0xCA, 25, 6);
            printf("[cli] checkpoint: giovanni_ready — teleported to B4F in front of Giovanni, Lift Key granted\n");
        } else if (strcmp(cp, "silph_ready") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            SetEvent(EVENT_SS_ANNE_LEFT);
            SetEvent(EVENT_2ND_LOCK_OPENED);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_GOT_TM24);
            SetEvent(EVENT_BEAT_ERIKA);
            SetEvent(EVENT_GOT_TM21);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
            SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
            SetEvent(EVENT_RESCUED_MR_FUJI);
            SetEvent(EVENT_GOT_POKE_FLUTE);
            SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);

            wObtainedBadges |= (1u << BADGE_BOULDER);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            wObtainedBadges |= (1u << BADGE_RAINBOW);

            if (Inventory_GetQty(0x4A) == 0)
                Inventory_Add(0x4A, 1);
            if (Inventory_GetQty(0x30) == 0)
                Inventory_Add(0x30, 1);
            if (Inventory_GetQty(HM01) == 0)
                Inventory_Add(HM01, 1);
            if (Inventory_GetQty(ITEM_POKE_FLUTE) == 0)
                Inventory_Add(ITEM_POKE_FLUTE, 1);

            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 42);
                wPartyCount = 1;
            }

            Game_WarpToRealMap(0xCF, 19, 0);
            printf("[cli] checkpoint: silph_ready — inside Silph Co 2F with Card Key + expected pre-Silph flags\n");
        } else if (strcmp(cp, "silph_rival_ready") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            SetEvent(EVENT_SS_ANNE_LEFT);
            SetEvent(EVENT_2ND_LOCK_OPENED);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_GOT_TM24);
            SetEvent(EVENT_BEAT_ERIKA);
            SetEvent(EVENT_GOT_TM21);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
            SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
            SetEvent(EVENT_RESCUED_MR_FUJI);
            SetEvent(EVENT_GOT_POKE_FLUTE);
            SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);
            ClearEvent(EVENT_BEAT_SILPH_CO_RIVAL);
            ClearEvent(EVENT_BEAT_SILPH_CO_GIOVANNI);
            ClearEvent(EVENT_GOT_MASTER_BALL);

            wObtainedBadges |= (1u << BADGE_BOULDER);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            wObtainedBadges |= (1u << BADGE_RAINBOW);

            if (Inventory_GetQty(0x30) == 0)
                Inventory_Add(0x30, 1);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 45);
                wPartyCount = 1;
            }

            Game_WarpToRealMap(0xD4, 3, 3);
            printf("[cli] checkpoint: silph_rival_ready — Silph Co 7F, one step before rival trigger\n");
        } else if (strcmp(cp, "silph_giovanni_ready") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            SetEvent(EVENT_SS_ANNE_LEFT);
            SetEvent(EVENT_2ND_LOCK_OPENED);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_GOT_TM24);
            SetEvent(EVENT_BEAT_ERIKA);
            SetEvent(EVENT_GOT_TM21);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
            SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
            SetEvent(EVENT_RESCUED_MR_FUJI);
            SetEvent(EVENT_GOT_POKE_FLUTE);
            SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);
            SetEvent(EVENT_BEAT_SILPH_CO_RIVAL);
            ClearEvent(EVENT_BEAT_SILPH_CO_GIOVANNI);
            ClearEvent(EVENT_GOT_MASTER_BALL);

            wObtainedBadges |= (1u << BADGE_BOULDER);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            wObtainedBadges |= (1u << BADGE_RAINBOW);

            if (Inventory_GetQty(0x30) == 0)
                Inventory_Add(0x30, 1);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 47);
                wPartyCount = 1;
            }

            Game_WarpToRealMap(0xEB, 6, 14);
            printf("[cli] checkpoint: silph_giovanni_ready — Silph Co 11F, one step before Giovanni trigger\n");
        } else if (strcmp(cp, "silph_lapras_ready") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            SetEvent(EVENT_SS_ANNE_LEFT);
            SetEvent(EVENT_2ND_LOCK_OPENED);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_GOT_TM24);
            SetEvent(EVENT_BEAT_ERIKA);
            SetEvent(EVENT_GOT_TM21);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
            SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
            SetEvent(EVENT_RESCUED_MR_FUJI);
            SetEvent(EVENT_GOT_POKE_FLUTE);
            SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);
            ClearEvent(EVENT_BEAT_SILPH_CO_GIOVANNI);
            ClearEvent(EVENT_GOT_MASTER_BALL);
            ClearEvent(EVENT_GOT_LAPRAS);

            wObtainedBadges |= (1u << BADGE_BOULDER);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            wObtainedBadges |= (1u << BADGE_RAINBOW);

            if (Inventory_GetQty(0x30) == 0)
                Inventory_Add(0x30, 1);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 44);
                wPartyCount = 1;
            }

            Game_WarpToRealMap(0xD4, 1, 6);
            printf("[cli] checkpoint: silph_lapras_ready — Silph Co 7F, one step before Lapras worker\n");
        } else if (strcmp(cp, "silph_entry_locked") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            SetEvent(EVENT_SS_ANNE_LEFT);
            SetEvent(EVENT_2ND_LOCK_OPENED);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_GOT_TM24);
            SetEvent(EVENT_BEAT_ERIKA);
            SetEvent(EVENT_GOT_TM21);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
            SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
            SetEvent(EVENT_GOT_POKE_FLUTE);
            SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);
            ClearEvent(EVENT_RESCUED_MR_FUJI);
            ClearEvent(EVENT_RESCUED_MR_FUJI_2);
            ClearEvent(EVENT_BEAT_SILPH_CO_GIOVANNI);
            ClearEvent(EVENT_BEAT_SILPH_CO_RIVAL);
            ClearEvent(EVENT_GOT_MASTER_BALL);
            ClearEvent(EVENT_GOT_LAPRAS);

            wObtainedBadges |= (1u << BADGE_BOULDER);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            wObtainedBadges |= (1u << BADGE_RAINBOW);

            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 45);
                wPartyCount = 1;
            }

            Game_WarpToRealMap(0x0A, 18, 23);
            printf("[cli] checkpoint: silph_entry_locked — Saffron, outside Silph with blocker active\n");
        } else if (strcmp(cp, "silph_entry_unlocked") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            SetEvent(EVENT_SS_ANNE_LEFT);
            SetEvent(EVENT_2ND_LOCK_OPENED);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_GOT_TM24);
            SetEvent(EVENT_BEAT_ERIKA);
            SetEvent(EVENT_GOT_TM21);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
            SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
            SetEvent(EVENT_GOT_POKE_FLUTE);
            SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);
            SetEvent(EVENT_RESCUED_MR_FUJI);
            SetEvent(EVENT_RESCUED_MR_FUJI_2);
            ClearEvent(EVENT_BEAT_SILPH_CO_GIOVANNI);
            ClearEvent(EVENT_BEAT_SILPH_CO_RIVAL);
            ClearEvent(EVENT_GOT_MASTER_BALL);
            ClearEvent(EVENT_GOT_LAPRAS);

            wObtainedBadges |= (1u << BADGE_BOULDER);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            wObtainedBadges |= (1u << BADGE_RAINBOW);

            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 45);
                wPartyCount = 1;
            }

            Game_WarpToRealMap(0x0A, 18, 23);
            printf("[cli] checkpoint: silph_entry_unlocked — Saffron, Silph entrance unlocked\n");
        } else if (strcmp(cp, "surge") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            SetEvent(EVENT_SS_ANNE_LEFT);
            SetEvent(EVENT_2ND_LOCK_OPENED);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
            ClearEvent(EVENT_BEAT_LT_SURGE);
            ClearEvent(EVENT_GOT_TM24);
            Inventory_Add(HM01, 1);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 35);
                wPartyCount = 1;
            }
            Game_WarpToRealMap(0x5C, 5, 3);
            printf("[cli] checkpoint: surge — inside Vermilion Gym, facing Lt. Surge\n");
        } else if (strcmp(cp, "sabrina") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            SetEvent(EVENT_SS_ANNE_LEFT);
            SetEvent(EVENT_2ND_LOCK_OPENED);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_GOT_TM24);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            SetEvent(EVENT_BEAT_ERIKA);
            SetEvent(EVENT_GOT_TM21);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
            wObtainedBadges |= (1u << BADGE_RAINBOW);
            SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
            SetEvent(EVENT_BEAT_SILPH_CO_GIOVANNI);
            SetEvent(EVENT_RESCUED_MR_FUJI);
            SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);
            SetEvent(EVENT_GOT_POKE_FLUTE);
            ClearEvent(EVENT_BEAT_SABRINA);
            ClearEvent(EVENT_GOT_TM46);
            ClearEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_0);
            ClearEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_1);
            ClearEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_2);
            ClearEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_3);
            ClearEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_4);
            ClearEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_5);
            ClearEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_6);

            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 48);
                wPartyCount = 1;
            }

            Game_WarpToRealMap(0xB2, 9, 9);
            printf("[cli] checkpoint: sabrina — inside Saffron Gym, facing Sabrina\n");
        } else if (strcmp(cp, "erika") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            SetEvent(EVENT_SS_ANNE_LEFT);
            SetEvent(EVENT_2ND_LOCK_OPENED);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_GOT_TM24);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            ClearEvent(EVENT_BEAT_ERIKA);
            ClearEvent(EVENT_GOT_TM21);
            ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
            ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
            ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
            ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
            ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
            ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
            ClearEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 40);
                wPartyCount = 1;
            }
            Game_WarpToRealMap(0x86, 4, 4);
            printf("[cli] checkpoint: erika — inside Celadon Gym, facing Erika\n");
        } else if (strcmp(cp, "koga") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            SetEvent(EVENT_SS_ANNE_LEFT);
            SetEvent(EVENT_2ND_LOCK_OPENED);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_GOT_TM24);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            SetEvent(EVENT_BEAT_ERIKA);
            SetEvent(EVENT_GOT_TM21);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
            wObtainedBadges |= (1u << BADGE_RAINBOW);
            ClearEvent(EVENT_BEAT_KOGA);
            ClearEvent(EVENT_GOT_TM06);
            ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_0);
            ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_1);
            ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_2);
            ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_3);
            ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_4);
            ClearEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_5);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 45);
                wPartyCount = 1;
            }
            Game_WarpToRealMap(0x9D, 4, 11);
            printf("[cli] checkpoint: koga — inside Fuchsia Gym, facing Koga\n");
        } else if (strcmp(cp, "blaine") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            SetEvent(EVENT_SS_ANNE_LEFT);
            SetEvent(EVENT_2ND_LOCK_OPENED);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_GOT_TM24);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            SetEvent(EVENT_BEAT_ERIKA);
            SetEvent(EVENT_GOT_TM21);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
            wObtainedBadges |= (1u << BADGE_RAINBOW);
            SetEvent(EVENT_BEAT_KOGA);
            SetEvent(EVENT_GOT_TM06);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_5);
            wObtainedBadges |= (1u << BADGE_SOUL);
            ClearEvent(EVENT_BEAT_BLAINE);
            ClearEvent(EVENT_GOT_TM38);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 52);
                wPartyCount = 1;
            }
            Game_WarpToRealMap(0xA6, 3, 4);
            printf("[cli] checkpoint: blaine — inside Cinnabar Gym, facing Blaine\n");
        } else if (strcmp(cp, "erika_post") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            SetEvent(EVENT_SS_ANNE_LEFT);
            SetEvent(EVENT_2ND_LOCK_OPENED);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_GOT_TM24);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            SetEvent(EVENT_BEAT_ERIKA);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
            ClearEvent(EVENT_GOT_TM21);
            wObtainedBadges |= (1u << BADGE_RAINBOW);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 45);
                wPartyCount = 1;
            }
            Game_WarpToRealMap(0x86, 4, 4);
            printf("[cli] checkpoint: erika_post — Erika beaten, TM21 not obtained\n");
        } else if (strcmp(cp, "koga_post") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            SetEvent(EVENT_SS_ANNE_LEFT);
            SetEvent(EVENT_2ND_LOCK_OPENED);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_GOT_TM24);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            SetEvent(EVENT_BEAT_ERIKA);
            SetEvent(EVENT_GOT_TM21);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
            wObtainedBadges |= (1u << BADGE_RAINBOW);
            SetEvent(EVENT_BEAT_KOGA);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_5);
            ClearEvent(EVENT_GOT_TM06);
            wObtainedBadges |= (1u << BADGE_SOUL);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 50);
                wPartyCount = 1;
            }
            Game_WarpToRealMap(0x9D, 4, 11);
            printf("[cli] checkpoint: koga_post — Koga beaten, TM06 not obtained\n");
        } else if (strcmp(cp, "blaine_post") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            SetEvent(EVENT_SS_ANNE_LEFT);
            SetEvent(EVENT_2ND_LOCK_OPENED);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_GOT_TM24);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            SetEvent(EVENT_BEAT_ERIKA);
            SetEvent(EVENT_GOT_TM21);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
            wObtainedBadges |= (1u << BADGE_RAINBOW);
            SetEvent(EVENT_BEAT_KOGA);
            SetEvent(EVENT_GOT_TM06);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_5);
            wObtainedBadges |= (1u << BADGE_SOUL);
            SetEvent(EVENT_BEAT_BLAINE);
            ClearEvent(EVENT_GOT_TM38);
            wObtainedBadges |= (1u << BADGE_VOLCANO);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 56);
                wPartyCount = 1;
            }
            Game_WarpToRealMap(0xA6, 3, 4);
            printf("[cli] checkpoint: blaine_post — Blaine beaten, TM38 not obtained\n");
        } else if (strcmp(cp, "post_giovanni_victory") == 0 ||
                   strcmp(cp, "post-giovanni-victory") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_BEAT_ERIKA);
            SetEvent(EVENT_BEAT_KOGA);
            SetEvent(EVENT_BEAT_SABRINA);
            SetEvent(EVENT_BEAT_BLAINE);
            SetEvent(EVENT_BEAT_VIRIDIAN_GYM_GIOVANNI);

            SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_5);
            SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_6);
            SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_7);

            wObtainedBadges |= (1u << BADGE_BOULDER);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            wObtainedBadges |= (1u << BADGE_RAINBOW);
            wObtainedBadges |= (1u << BADGE_SOUL);
            wObtainedBadges |= (1u << BADGE_MARSH);
            wObtainedBadges |= (1u << BADGE_VOLCANO);
            wObtainedBadges |= (1u << BADGE_EARTH);

            SetEvent(EVENT_BEAT_ROUTE22_RIVAL_1ST_BATTLE);
            ClearEvent(EVENT_1ST_ROUTE22_RIVAL_BATTLE);
            SetEvent(EVENT_2ND_ROUTE22_RIVAL_BATTLE);
            SetEvent(EVENT_ROUTE22_RIVAL_WANTS_BATTLE);
            ClearEvent(EVENT_BEAT_ROUTE22_RIVAL_2ND_BATTLE);

            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 55);
                wPartyCount = 1;
            }

            extern void Map_Load(uint8_t map_id);
            Game_WarpToRealMap(0x21, 31, 5);
            printf("[cli] checkpoint: post_giovanni_victory — Route 22 rival-2 armed; walk left to trigger\n");
        } else if (strcmp(cp, "route23_guard_reset") == 0 ||
                   strcmp(cp, "badge_guard_reset") == 0) {

            ClearEvent(EVENT_PASSED_CASCADEBADGE_CHECK);
            ClearEvent(EVENT_PASSED_THUNDERBADGE_CHECK);
            ClearEvent(EVENT_PASSED_RAINBOWBADGE_CHECK);
            ClearEvent(EVENT_PASSED_SOULBADGE_CHECK);
            ClearEvent(EVENT_PASSED_MARSHBADGE_CHECK);
            ClearEvent(EVENT_PASSED_VOLCANOBADGE_CHECK);
            ClearEvent(EVENT_PASSED_EARTHBADGE_CHECK);

            wObtainedBadges &= (uint8_t)~(1u << BADGE_BOULDER);
            ClearEvent(EVENT_BEAT_BROCK);
            printf("[cli] checkpoint: route23_guard_reset — cleared Route 23 pass flags + Boulder badge/Brock win\n");
        } else if (strcmp(cp, "sabrina_post") == 0) {

            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            SetEvent(EVENT_BEAT_CERULEAN_RIVAL);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            SetEvent(EVENT_GOT_SS_TICKET);
            SetEvent(EVENT_BEAT_SS_ANNE_RIVAL);
            SetEvent(EVENT_RUBBED_CAPTAINS_BACK);
            SetEvent(EVENT_GOT_HM01);
            SetEvent(EVENT_SS_ANNE_LEFT);
            SetEvent(EVENT_2ND_LOCK_OPENED);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_GOT_TM24);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            SetEvent(EVENT_BEAT_ERIKA);
            SetEvent(EVENT_GOT_TM21);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
            SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
            wObtainedBadges |= (1u << BADGE_RAINBOW);
            SetEvent(EVENT_BEAT_ROCKET_HIDEOUT_GIOVANNI);
            SetEvent(EVENT_BEAT_SILPH_CO_GIOVANNI);
            SetEvent(EVENT_RESCUED_MR_FUJI);
            SetEvent(EVENT_GAVE_SAFFRON_GUARDS_DRINK);
            SetEvent(EVENT_GOT_POKE_FLUTE);
            SetEvent(EVENT_BEAT_SABRINA);
            SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_1);
            SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_2);
            SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_3);
            SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_4);
            SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_5);
            SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_6);
            ClearEvent(EVENT_GOT_TM46);
            wObtainedBadges |= (1u << BADGE_MARSH);

            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 50);
                wPartyCount = 1;
            }

            Game_WarpToRealMap(0xB2, 9, 9);
            printf("[cli] checkpoint: sabrina_post — Sabrina beaten, TM46 not obtained\n");
        } else if (strcmp(cp, "gym_badges4") == 0) {

            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_BEAT_ERIKA);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            wObtainedBadges |= (1u << BADGE_RAINBOW);
            ClearEvent(EVENT_BEAT_KOGA);
            ClearEvent(EVENT_BEAT_BLAINE);
            printf("[cli] checkpoint: gym_badges4 — set first 4 gym wins/badges\n");
        } else if (strcmp(cp, "gym_badges5") == 0) {

            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_LT_SURGE);
            SetEvent(EVENT_BEAT_ERIKA);
            SetEvent(EVENT_BEAT_KOGA);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            wObtainedBadges |= (1u << BADGE_RAINBOW);
            wObtainedBadges |= (1u << BADGE_SOUL);
            ClearEvent(EVENT_BEAT_BLAINE);
            printf("[cli] checkpoint: gym_badges5 — set first 5 gym wins/badges\n");
        } else if (strcmp(cp, "gym_badges1") == 0) {
            SetEvent(EVENT_BEAT_BROCK);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            ClearEvent(EVENT_BEAT_MISTY);
            ClearEvent(EVENT_BEAT_LT_SURGE);
            ClearEvent(EVENT_BEAT_ERIKA);
            ClearEvent(EVENT_BEAT_KOGA);
            ClearEvent(EVENT_BEAT_BLAINE);
            printf("[cli] checkpoint: gym_badges1 — set Brock win/badge only\n");
        } else if (strcmp(cp, "gym_badges2") == 0) {
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_MISTY);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            ClearEvent(EVENT_BEAT_LT_SURGE);
            ClearEvent(EVENT_BEAT_ERIKA);
            ClearEvent(EVENT_BEAT_KOGA);
            ClearEvent(EVENT_BEAT_BLAINE);
            printf("[cli] checkpoint: gym_badges2 — set Brock+Misty wins/badges\n");
        } else if (strcmp(cp, "gym_badges3") == 0) {
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_LT_SURGE);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            wObtainedBadges |= (1u << BADGE_THUNDER);
            ClearEvent(EVENT_BEAT_ERIKA);
            ClearEvent(EVENT_BEAT_KOGA);
            ClearEvent(EVENT_BEAT_BLAINE);
            printf("[cli] checkpoint: gym_badges3 — set Brock+Misty+Surge wins/badges\n");
        } else if (strcmp(cp, "brock_post") == 0) {

            SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
            SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
            SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
            SetEvent(EVENT_GOT_STARTER);
            SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
            SetEvent(EVENT_GOT_OAKS_PARCEL);
            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            ClearEvent(EVENT_GOT_TM34);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 18);
                wPartyCount = 1;
            }
            Game_WarpToRealMap(0x36, 4, 2);
            printf("[cli] checkpoint: brock_post — Brock beaten, TM34 not obtained\n");
        } else if (strcmp(cp, "misty_post") == 0) {

            SetEvent(EVENT_OAK_APPEARED_IN_PALLET);
            SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
            SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
            SetEvent(EVENT_GOT_STARTER);
            SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
            SetEvent(EVENT_GOT_OAKS_PARCEL);
            SetEvent(EVENT_GOT_POKEDEX);
            SetEvent(EVENT_BEAT_BROCK);
            SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);
            wObtainedBadges |= (1u << BADGE_BOULDER);
            SetEvent(EVENT_BEAT_MISTY);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
            SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
            ClearEvent(EVENT_GOT_TM11);
            wObtainedBadges |= (1u << BADGE_CASCADE);
            if (wPartyCount == 0) {
                Pokemon_InitMon(&wPartyMons[0], STARTER1, 26);
                wPartyCount = 1;
            }
            Game_WarpToRealMap(0x41, 4, 8);
            printf("[cli] checkpoint: misty_post — Misty beaten, TM11 not obtained\n");
        } else if (strcmp(cp, "list") == 0 || strcmp(cp, "help") == 0) {
            printf("[cli] checkpoint list:\n");
            printf("  parcel_ready, pokedex_ready, route22, brock, mt_moon, cerulean,\n");
            printf("  misty, cerulean_rocket, ss_anne_hm, surge, erika, koga, sabrina, blaine,\n");
            printf("  brock_post, misty_post, erika_post, koga_post, sabrina_post, blaine_post,\n");
            printf("  post_giovanni_victory,\n");
            printf("  route23_guard_reset (alias: badge_guard_reset),\n");
            printf("  gym_badges1, gym_badges2, gym_badges3, gym_badges4, gym_badges5,\n");
            printf("  liftkey_reset, giovanni_reset, giovanni_ready, silph_ready,\n");
            printf("  silph_entry_locked, silph_entry_unlocked,\n");
            printf("  silph_rival_ready, silph_giovanni_ready, silph_lapras_ready\n");
        } else {
            printf("[cli] Unknown checkpoint: %s\n"
                   "      Use: checkpoint list\n", cp);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "giveteam") == 0) {

        static const uint8_t kTeam[] = {
            SPECIES_MEWTWO,
            SPECIES_DRAGONITE,
            SPECIES_ALAKAZAM,
            SPECIES_MACHAMP,
            SPECIES_GENGAR,
            SPECIES_LAPRAS,
        };
        wPartyCount = 6;
        for (int i = 0; i < 6; i++)
            Pokemon_InitMon(&wPartyMons[i], kTeam[i], 100);
        printf("[cli] giveteam — party set to Mewtwo/Dragonite/Alakazam/Machamp/Gengar/Lapras @ lv100\n");
        write_state();
        return;
    }
    else if (strcmp(verb, "addexp") == 0) {

        int slot = 1, amount = 0;
        sscanf(cmd, "%*s %d %d", &slot, &amount);
        slot--;
        if (slot < 0 || slot >= (int)wPartyCount) {
            printf("[cli] addexp: slot must be 1-%d\n", wPartyCount);
        } else if (amount <= 0) {
            printf("[cli] addexp: amount must be > 0\n");
        } else {
            Battle_AddExpDirect((uint8_t)slot, (uint32_t)amount);
        }
        write_state();
        return;
    }
    else if (strcmp(verb, "exprate") == 0) {

        int rate = 100;
        sscanf(cmd, "%*s %d", &rate);
        if (rate < 1 || rate > 9999) {
            printf("[cli] exprate: value must be 1-9999 (100=1×, 200=2×, 300=3×)\n");
        } else {
            gDebugExpRate = rate;
            printf("[cli] exprate: exp multiplier set to %d%% (%d.%02dx)\n",
                   rate, rate / 100, rate % 100);
        }
        write_state();
        return;
    }
    else {
        printf("[cli] Unknown command: %s\n", verb);
        s_last_cmd_valid = 0;
        write_state();
        return;
    }

    if (s_seq_len > 0) {
        s_wait_remaining = 20;
        s_pending_write  = 1;
    } else {
        write_state();
    }
}

#define CLI_POLL_EVERY 6

static void poll_cmd_file(void) {

    static unsigned poll_tick;
    if ((poll_tick++ % CLI_POLL_EVERY) != 0) return;

    FILE *fp = fopen(CMD_FILE, "r");
    if (!fp) return;

    char line[128] = {0};
    fgets(line, sizeof(line), fp);
    fclose(fp);
    remove(CMD_FILE);

    char *nl = strchr(line, '\n');
    if (nl) *nl = '\0';
    if (*line == '\0') return;

    printf("[cli] cmd: %s\n", line);
    {
        char logline[CLI_HIST_WIDTH + 1];
        snprintf(logline, sizeof(logline), "> %s", line);
        s_last_cmd_hist_slot = cli_hist_push(logline);
    }
    process_cmd(line);
    if (s_last_cmd_hist_slot >= 0) {
        s_hist_color[s_last_cmd_hist_slot] =
            (uint8_t)(s_last_cmd_valid ? CLI_HIST_COLOR_OK : CLI_HIST_COLOR_ERROR);
        s_last_cmd_hist_slot = -1;
    }
}

void DebugCLI_PumpButtons(void) {
    if (s_seq_pos < s_seq_len) {
        gCliButtons = s_seq[s_seq_pos++];
        gCliFrames  = 1;
    }
}

static void savemenu_tick(void) {
    const char *why;
    if (s_savemenu_state == SM_IDLE) return;

    if (--s_savemenu_guard <= 0) {
        savemenu_finish("NO", "timed out waiting for the menu");
        return;
    }
    if (s_savemenu_wait > 0) { s_savemenu_wait--; return; }

    switch (s_savemenu_state) {
    case SM_OPENING:

        why = savemenu_blocked_reason();
        if (why) { savemenu_finish("NO", why); return; }
        if (!Menu_IsOpen()) {
            seq_push(BTN_START, PRESS, GAP);
            s_savemenu_wait = GAP;
            return;
        }
        s_savemenu_state = SM_NAV;
        return;

    case SM_NAV:
        if (!Menu_IsOpen()) { savemenu_finish("NO", "menu closed unexpectedly"); return; }
        if (Menu_CursorRow() != Menu_SaveRowIndex()) {
            seq_push(BTN_DOWN, PRESS, GAP);
            s_savemenu_wait = GAP;
            return;
        }
        s_savemenu_state = SM_CONFIRM;
        return;

    case SM_CONFIRM:
        seq_push(BTN_A, PRESS, GAP);
        s_savemenu_wait = GAP;
        s_savemenu_state = SM_YES;
        return;

    case SM_YES:

        if (!Menu_SaveAwaitingYesNo()) { s_savemenu_wait = 1; return; }
        seq_push(BTN_A, PRESS, GAP);
        s_savemenu_wait = GAP;
        s_savemenu_state = SM_FINISH;
        return;

    case SM_FINISH:

        if (Menu_SaveFlowState() != 0) { s_savemenu_wait = 1; return; }
        if (Menu_IsOpen()) { seq_push(BTN_B, PRESS, GAP); s_savemenu_wait = GAP; return; }
        savemenu_finish("SAVED", NULL);
        return;

    default:
        s_savemenu_state = SM_IDLE;
        return;
    }
}

void DebugCLI_Tick(void) {
    savemenu_tick();

    AmberScript_Tick();

    dsl_bank_init_if_needed();
    dsl_startup_run_if_enabled();
    if (s_dsl_bank_enabled && s_dsl_bank_last_map != wCurMap) {
        dsl_bank_ensure_current_map_spawns();
        dsl_bank_ensure_current_map_tiles();
        s_dsl_bank_last_map = wCurMap;
    }

    if (s_seq_pos < s_seq_len) {
        gCliButtons = s_seq[s_seq_pos++];
        gCliFrames  = 1;
    }

    if (s_replay_playing) {
        if (s_replay_play_pos < s_replay_play_len) {
            gCliButtons = s_replay_play_buf[s_replay_play_pos++];
            gCliFrames  = 1;
        } else {
            replay_reset_playback();
            printf("[cli] replay: playback complete\n");
            s_pending_write = 1;
            s_wait_remaining = 4;
        }
    }

    if (s_temp_npc_walkoff_active) {
        int tx, ty;
        int sy;
        if (s_temp_npc_walkoff_idx < 0 || s_temp_npc_walkoff_idx >= NPC_GetCount()) {
            s_temp_npc_walkoff_active = 0;
            s_temp_npc_walkoff_idx = -1;
            s_temp_npc_walkoff_phase = 0;
            s_temp_npc_walkoff_pretext_frames = 0;
            wJoyIgnore = 0;
        } else if (s_temp_npc_walkoff_phase == 1) {
            NPC_SetFacing(s_temp_npc_walkoff_idx,
                (wPlayerDirection == 4) ? 1 :
                (wPlayerDirection == 8) ? 2 :
                (wPlayerDirection == 12) ? 3 : 0);
            if (s_temp_npc_walkoff_pretext_frames > 0) {
                s_temp_npc_walkoff_pretext_frames--;
            } else if (!Text_IsOpen()) {
                Text_ShowASCII("Hello! I am a\ndebug_cli NPC\ntest!\fHope this works!@");
                s_temp_npc_walkoff_phase = 2;
            }
        } else if (s_temp_npc_walkoff_phase == 2) {
            NPC_SetFacing(s_temp_npc_walkoff_idx,
                (wPlayerDirection == 4) ? 1 :
                (wPlayerDirection == 8) ? 2 :
                (wPlayerDirection == 12) ? 3 : 0);
            if (!Text_IsOpen()) {
                wJoyIgnore = 0;
                s_temp_npc_walkoff_phase = 3;
            }
        } else if (s_temp_npc_walkoff_phase == 3 && !NPC_IsWalking(s_temp_npc_walkoff_idx)) {
            NPC_GetTilePos(s_temp_npc_walkoff_idx, &tx, &ty);
            sy = ty * 2 + 1 - gCamY;
            if (sy < 0) {
                NPC_DebugDespawn(s_temp_npc_walkoff_idx);
                s_temp_npc_walkoff_active = 0;
                s_temp_npc_walkoff_idx = -1;
                s_temp_npc_walkoff_phase = 0;
                s_temp_npc_walkoff_pretext_frames = 0;
                printf("[cli] npc_walkoff: despawned after leaving top of screen\n");
            } else {
                NPC_DoScriptedStep(s_temp_npc_walkoff_idx, 1);
            }
        }
    }

    if (s_py_law_enabled) {
        s_py_law_frame_accum++;
        if (s_py_law_frame_accum >= 60) {
            s_py_law_frame_accum = 0;
            py_law_tick_once();
            s_py_law_elapsed_sec++;
        }
    }

    if (s_scene_active) {
        scene_track_actor_positions();
        if (s_scene_battlestart_pending) {
            if (Text_IsOpen()) {
                s_scene_battlestart_saw_text = 1;
            } else if (!s_scene_battlestart_delay) {

                s_scene_battlestart_delay = 1;
            } else {
                const char *defeat_text = s_scene_cmds[s_scene_pc].text;
                scene_start_trainer_battle(s_scene_battlestart_tc, s_scene_battlestart_tn, defeat_text);
                s_scene_wait_battle = 1;
                s_scene_battlestart_pending = 0;
                s_scene_battlestart_saw_text = 0;
                s_scene_battlestart_delay = 0;
                s_scene_pc++;
                printf("[cli] scene battlestart: class=%d no=%d\n",
                       s_scene_battlestart_tc, s_scene_battlestart_tn);
            }
        } else if (s_scene_wait_say) {
            if (Text_IsOpen()) s_scene_say_opened = 1;
            if (s_scene_say_opened && !Text_IsOpen()) {
                s_scene_wait_say = 0;
                s_scene_say_opened = 0;
            }
        } else if (s_scene_wait_battle) {
            int sc = get_scene();
            if (sc == 2 || BattleUI_IsActive())
                s_scene_battle_started = 1;

            if (s_scene_battle_started && sc == 0 && !BattleUI_IsActive()) {
                scene_restore_spawned_actors_after_battle();
                s_scene_wait_battle = 0;
                s_scene_battle_started = 0;
            }
        } else if (s_scene_wait_battleend_text) {
            if (!Text_IsOpen()) {
                s_scene_wait_battleend_text = 0;
                printf("[scene] battlend\n");
            }
        } else if (s_scene_wait_yesno) {
            YesNo_Tick();
            if (!YesNo_IsOpen()) {
                s_scene_last_yesno = YesNo_GetResult() ? 1 : 0;
                s_scene_wait_yesno = 0;
                if (s_scene_yesno_restore_joyignore) {
                    wJoyIgnore = s_scene_yesno_prev_joyignore;
                    s_scene_yesno_restore_joyignore = 0;
                }
                if (s_scene_yesno_restore_scripted_movement) {
                    gScriptedMovement = s_scene_yesno_prev_scripted_movement;
                    s_scene_yesno_restore_scripted_movement = 0;
                }
                printf("[cli] scene ask result: %s\n", s_scene_last_yesno ? "yes" : "no");
            }
        } else if (s_scene_wait > 0) {
            s_scene_wait--;
        } else if (s_scene_move_steps_left > 0) {
            if (s_scene_move_actor < 0 || s_scene_move_actor >= SCENE_ACTOR_MAX || !s_scene_actors[s_scene_move_actor].used) {
                s_scene_move_steps_left = 0;
            } else {
                int npc_idx = s_scene_actors[s_scene_move_actor].npc_idx;
                if (npc_idx < 0 || npc_idx >= NPC_GetCount()) {
                    s_scene_move_steps_left = 0;
                } else if (!NPC_IsWalking(npc_idx)) {
                    NPC_DoScriptedStep(npc_idx, s_scene_move_dir);
                    s_scene_move_steps_left--;
                }
            }
            if (s_scene_move_steps_left <= 0) s_scene_pc++;
        } else {
            if (s_scene_pc < 0 || s_scene_pc >= s_scene_cmd_count) {
                s_scene_active = 0;
                wJoyIgnore = 0;
                printf("[scene] finished\n");
            } else {
                scene_cmd_t *cmd = &s_scene_cmds[s_scene_pc];
                switch (cmd->op) {
                    case SCOP_SPAWN: {
                        int idx = -1;
                        int spawned_new = 0;
                        int ai_prev = scene_find_actor(cmd->actor);
                        if (ai_prev >= 0 && s_scene_actors[ai_prev].used) {
                            idx = s_scene_actors[ai_prev].npc_idx;
                        }
                        if (idx < 0) {

                            idx = NPC_FindAtTile(cmd->b, cmd->c);
                        }
                        if (idx < 0) {
                            idx = NPC_DebugSpawn((uint8_t)cmd->a, cmd->b, cmd->c, 0, 0);
                            if (idx >= 0) spawned_new = 1;
                        }
                        {
                            int ai = scene_add_actor(cmd->actor, idx);
                            if (ai >= 0) {

                                if (spawned_new) s_scene_actors[ai].spawned_by_scene = 1;
                                s_scene_actors[ai].sprite_id = (uint8_t)cmd->a;
                                s_scene_actors[ai].last_x = cmd->b;
                                s_scene_actors[ai].last_y = cmd->c;
                            }
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_DESPAWN: {
                        int ai = scene_find_actor(cmd->actor);
                        if (ai >= 0) NPC_DebugDespawn(s_scene_actors[ai].npc_idx);
                        if (ai >= 0) s_scene_actors[ai].spawned_by_scene = 0;
                        s_scene_pc++;
                    } break;
                    case SCOP_FACE: {
                        int ai = scene_find_actor(cmd->actor);
                        if (ai >= 0) {
                            int dir = cmd->a;
                            if (dir == -2) {
                                NPC_FacePlayer(s_scene_actors[ai].npc_idx);
                            } else {
                                NPC_SetFacing(s_scene_actors[ai].npc_idx, dir);
                            }
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_MOVE: {
                        int ai = scene_find_actor(cmd->actor);
                        if (ai < 0) { s_scene_pc++; break; }
                        s_scene_move_actor = ai;
                        s_scene_move_dir = cmd->a;
                        s_scene_move_steps_left = (cmd->b < 0) ? 0 : cmd->b;
                        if (s_scene_move_steps_left <= 0) s_scene_pc++;
                    } break;
                    case SCOP_SAY:
                        if (!Text_IsOpen() && !s_scene_wait_say) {
                            Text_ShowASCII(cmd->text);
                            s_scene_wait_say = 1;
                            s_scene_say_opened = Text_IsOpen() ? 1 : 0;
                            s_scene_pc++;
                        }
                        break;
                    case SCOP_ASK:
                        if (!Text_IsOpen() && !YesNo_IsOpen()) {
                            s_scene_yesno_prev_joyignore = wJoyIgnore;
                            s_scene_yesno_restore_joyignore = 1;
                            s_scene_yesno_prev_scripted_movement = gScriptedMovement;
                            s_scene_yesno_restore_scripted_movement = 1;

                            gScriptedMovement = 1;
                            wJoyIgnore = (uint8_t)(wJoyIgnore & (uint8_t)~(PAD_UP | PAD_DOWN));
                            YesNo_Show(cmd->text);
                            s_scene_wait_yesno = 1;
                            s_scene_pc++;
                        }
                        break;
                    case SCOP_BATTLESTART: {
                        if (cmd->c == 1) {
                            scene_start_custom_trainer_battle(cmd);
                            s_scene_wait_battle = 1;
                            s_scene_pc++;
                            printf("[cli] scene battlestart: custom team count=%u\n", (unsigned)cmd->team_count);
                        } else {
                            int tc = cmd->a;
                            int tn = cmd->b;
                            if (tc == -1) {
                                if (!scene_pick_random_map_trainer(&tc, &tn)) {
                                    tc = 34; tn = 1;
                                }
                            }
                            if (tc < 1) tc = 34;
                            if (tc > NUM_TRAINERS) tc = NUM_TRAINERS;
                            if (tn < 1) tn = 1;
                            s_scene_battlestart_tc = tc;
                            s_scene_battlestart_tn = tn;
                            s_scene_battlestart_pending = 1;
                            s_scene_battlestart_saw_text = Text_IsOpen() ? 1 : 0;
                            s_scene_battlestart_delay = 0;
                        }
                    } break;
                    case SCOP_BATTLEEND:
                        if (get_scene() == 2 || BattleUI_IsActive()) break;
                        if (Game_IsWarpFadeActive()) break;
                        if (!Text_IsOpen()) {
                            Text_ShowASCII(cmd->text);
                            s_scene_wait_battleend_text = 1;
                            s_scene_pc++;
                        }
                        break;
                    case SCOP_MUSIC:
                        Music_Play((uint8_t)cmd->a);
                        s_scene_pc++;
                        break;
                    case SCOP_WAIT:
                        s_scene_wait = (cmd->a < 0) ? 0 : cmd->a;
                        s_scene_pc++;
                        break;
                    case SCOP_WAIT_TEXT:
                        if (!Text_IsOpen()) s_scene_pc++;
                        break;
                    case SCOP_LOCK_INPUT:
                        wJoyIgnore = cmd->a ? PAD_CTRL_PAD : 0;
                        s_scene_pc++;
                        break;
                    case SCOP_TILE_COPY:
                        scene_tile_copy(cmd->a, cmd->b, cmd->c, cmd->d);
                        s_scene_pc++;
                        break;
                    case SCOP_TILE_SAVE:
                        scene_tile_save_right_of_player(cmd->text);
                        s_scene_pc++;
                        break;
                    case SCOP_TILE_PLACE_CUSTOM:
                        scene_tile_place_custom(cmd->text, cmd->a, cmd->b);
                        s_scene_pc++;
                        break;
                    case SCOP_BLOCK_SAVE:
                        scene_block_save(cmd->text, cmd->a, cmd->b, cmd->c, cmd->d);
                        s_scene_pc++;
                        break;
                    case SCOP_BLOCK_PLACE_CUSTOM:
                        scene_block_place_custom(cmd->text, cmd->a, cmd->b);
                        s_scene_pc++;
                        break;
                    case SCOP_PY_AI:
                        PyAI_SetEnabled(cmd->a, cmd->text[0] ? cmd->text : NULL);
                        printf("[scene] py_ai %s (%s)\n",
                               PyAI_IsEnabled() ? "on" : "off",
                               PyAI_GetScriptPath());
                        s_scene_pc++;
                        break;
                    case SCOP_PY_INJECT: {
                        int n = scene_exec_py_inject(cmd->text);
                        printf("[scene] py_inject: %d cmd(s) injected\n", n);
                        s_scene_pc++;
                    } break;
                    case SCOP_PY_LAW:
                        s_py_law_enabled = cmd->a ? 1 : 0;
                        s_py_law_npc_idx = cmd->b;
                        s_py_law_frame_accum = 0;
                        s_py_law_elapsed_sec = 0;
                        if (cmd->text[0]) snprintf(s_py_law_script, sizeof(s_py_law_script), "%s", cmd->text);
                        printf("[scene] py_law %s npc=%d script=%s\n",
                               s_py_law_enabled ? "on" : "off",
                               s_py_law_npc_idx, s_py_law_script);
                        s_scene_pc++;
                        break;
                    case SCOP_PY_LAW_SPAWN: {
                        int idx = NPC_DebugSpawn((uint8_t)cmd->a, cmd->b, cmd->c, 0, 0);
                        if (idx < 0) {
                            printf("[scene] py_law_spawn: failed to spawn npc\n");
                        } else {
                            s_py_law_enabled = 1;
                            s_py_law_npc_idx = idx;
                            s_py_law_frame_accum = 0;
                            s_py_law_elapsed_sec = 0;
                            snprintf(s_py_law_script, sizeof(s_py_law_script), "%s", cmd->text);
                            printf("[scene] py_law_spawn: npc=%d at (%d,%d) script=%s\n",
                                   idx, cmd->b, cmd->c, s_py_law_script);
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_TYPEMOD:
                        scene_exec_typemod_line(cmd->text);
                        s_scene_pc++;
                        break;
                    case SCOP_SPRITE_FRONT_LOAD:
                        if (SpriteMod_LoadFrontFromFile((uint8_t)cmd->a, cmd->text))
                            printf("[scene] sprite_front_load: species %d <- %s\n", cmd->a, cmd->text);
                        else
                            printf("[scene] sprite_front_load: failed for species %d from %s\n", cmd->a, cmd->text);
                        s_scene_pc++;
                        break;
                    case SCOP_SPRITE_BACK_LOAD:
                        if (SpriteMod_LoadBackFromFile((uint8_t)cmd->a, cmd->text))
                            printf("[scene] sprite_back_load: species %d <- %s\n", cmd->a, cmd->text);
                        else
                            printf("[scene] sprite_back_load: failed for species %d from %s\n", cmd->a, cmd->text);
                        s_scene_pc++;
                        break;
                    case SCOP_END:
                        s_scene_active = 0;
                        wJoyIgnore = 0;
                        printf("[scene] end\n");
                        break;
                    default:
                        s_scene_pc++;
                        break;
                }
            }
        }
    }

    if (!s_scene_active && get_scene() == 0 && !Player_IsMoving() && !Text_IsOpen()) {
        for (int i = 0; i < SCENE_TRIGGER_MAX; i++) {
            scene_trigger_t *t = &s_scene_triggers[i];
            if (!t->used) continue;
            if ((int)t->map_id != (int)wCurMap) continue;
            if (t->cond_kind == 1 && !CheckEvent(t->cond_event)) continue;
            if (t->cond_kind == 2 && CheckEvent(t->cond_event)) continue;
            if ((int)wXCoord == t->x && (int)wYCoord == t->y) {
                if (!t->armed) continue;
                scene_reset_runtime();
                int ncmd = scene_load_file(t->scene);
                if (ncmd > 0) {
                    s_scene_active = 1;
                    s_scene_pc = 0;
                    t->armed = 0;
                    printf("[cli] scene_trigger: fired '%s' @ (%d,%d)\n", t->scene, t->x, t->y);
                } else {
                    t->armed = 0;
                    printf("[cli] scene_trigger: failed to load '%s'\n", t->scene);
                }
                break;
            } else {
                t->armed = 1;
            }
        }
    }

    if (s_script_trace_enabled) {
        uint8_t map_now = wCurMap;
        uint8_t trainer_now = (uint8_t)(Trainer_IsEngaging() ? 1 : 0);
        uint8_t text_now = (uint8_t)(Text_IsOpen() ? 1 : 0);
        uint8_t gate_now = (uint8_t)(Gate_PewterIsActive() ? 1 : 0);
        uint8_t r22_now = 0;
        uint8_t r24_now = (uint8_t)(Route24Scripts_IsActive() ? 1 : 0);
        uint8_t ss_now = 0;
        uint8_t vm_now = (uint8_t)(ViridianMartScripts_IsActive() ? 1 : 0);
        uint8_t gym_now = (uint8_t)(GymScripts_IsActive() ? 1 : 0);
        uint8_t rb4f_now = (uint8_t)(RocketHideoutB4FScripts_IsActive() ? 1 : 0);

        if (map_now != s_trace_prev_map) {
            cli_script_trace_emitf("[script_trace] map=%u (%s)", map_now, gMapTable[map_now].name);
            s_trace_prev_map = map_now;
        }
        if (trainer_now != s_trace_prev_trainer_engaging) {
            cli_script_trace_emitf("[script_trace] trainer_engage=%u", trainer_now);
            s_trace_prev_trainer_engaging = trainer_now;
        }
        if (text_now != s_trace_prev_text_open) {
            cli_script_trace_emitf("[script_trace] text_open=%u", text_now);
            s_trace_prev_text_open = text_now;
        }
        if (gate_now != s_trace_prev_gate_active) {
            cli_script_trace_emitf("[script_trace] gate_active=%u", gate_now);
            s_trace_prev_gate_active = gate_now;
        }
        if (r22_now != s_trace_prev_route22_active) {
            cli_script_trace_emitf("[script_trace] route22_active=%u", r22_now);
            s_trace_prev_route22_active = r22_now;
        }
        if (r24_now != s_trace_prev_route24_active) {
            cli_script_trace_emitf("[script_trace] route24_active=%u", r24_now);
            s_trace_prev_route24_active = r24_now;
        }
        if (ss_now != s_trace_prev_ssanne_active) {
            cli_script_trace_emitf("[script_trace] ssanne_active=%u", ss_now);
            s_trace_prev_ssanne_active = ss_now;
        }
        if (vm_now != s_trace_prev_viridian_mart_active) {
            cli_script_trace_emitf("[script_trace] viridian_mart_active=%u", vm_now);
            s_trace_prev_viridian_mart_active = vm_now;
        }
        if (gym_now != s_trace_prev_gym_active) {
            cli_script_trace_emitf("[script_trace] gym_active=%u", gym_now);
            s_trace_prev_gym_active = gym_now;
        }
        if (rb4f_now != s_trace_prev_rockethideout_b4f_active) {
            cli_script_trace_emitf("[script_trace] rocket_b4f_active=%u", rb4f_now);
            s_trace_prev_rockethideout_b4f_active = rb4f_now;
        }
    }

    if (s_animlab_enabled && get_scene() == 2 ) {

        wBattleMon.hp = wBattleMon.max_hp;
        wEnemyMon.hp  = wEnemyMon.max_hp;
        wBattleMon.status = 0;
        wEnemyMon.status  = 0;
        wPlayerBattleStatus1 = wPlayerBattleStatus2 = wPlayerBattleStatus3 = 0;
        wEnemyBattleStatus1  = wEnemyBattleStatus2  = wEnemyBattleStatus3  = 0;
        animlab_set_enemy_harmless();

        if (Text_IsOpen()) {
            gCliButtons = BTN_A;
            gCliFrames  = 1;
        } else if (s_seq_pos >= s_seq_len) {
            int bui = BattleUI_GetState();
            if (bui == 10 ) {
                uint8_t move_id = (uint8_t)((s_animlab_move_id > 0 && s_animlab_move_id < NUM_MOVE_DEFS)
                    ? s_animlab_move_id : 1);
                animlab_set_player_move(move_id);

                seq_clear();
                seq_battle_menu(0);
                seq_move_select(1);

                s_animlab_move_id++;
                if (s_animlab_move_id >= NUM_MOVE_DEFS) {
                    s_animlab_move_id = 1;
                    s_animlab_loops++;
                }
            } else if (bui == 11 ) {
                seq_clear();
                seq_push(BTN_A, PRESS, GAP);
            }
        }
    }

    if (s_pending_write && s_seq_pos >= s_seq_len) {
        if (s_wait_remaining > 0) {
            s_wait_remaining--;
        } else {
            write_state();
            s_pending_write = 0;
        }
    }

    if (s_replay_recording && s_replay_rec_fp) {
        uint8_t raw = hJoyInput;
        fwrite(&raw, 1, 1, s_replay_rec_fp);
    }

    if (s_seq_pos >= s_seq_len && gCliFrames == 0) {
        if (++s_poll_timer >= 30) {
            s_poll_timer = 0;
            poll_cmd_file();
        }
    }
}

int DebugCLI_OnNpcInteracted(int npc_idx) {
    int bi;
    int ncmd;
    if (npc_idx < 0) return 0;
    if (s_py_law_enabled && npc_idx == s_py_law_npc_idx) {
        if (!Text_IsOpen()) {
            uint32_t day_sec = s_py_law_elapsed_sec % 300u;
            if (day_sec >= 150u) {
                Text_ShowASCII("Its past noon!@");
            } else {
                Text_ShowASCII("Its the morning!@");
            }
        }
        return 1;
    }
    bi = scene_npc_binding_find_by_idx(npc_idx);
    if (bi < 0) return 0;
    if (s_scene_active) return 1;
    scene_reset_runtime();
    ncmd = scene_load_file(s_scene_npc_bindings[bi].scene);
    if (ncmd <= 0) {
        printf("[cli] scene_npc interact: failed to load scene '%s'\n",
               s_scene_npc_bindings[bi].scene);
        return 1;
    }
    s_scene_active = 1;
    s_scene_pc = 0;
    printf("[cli] scene_npc interact: '%s' -> scene '%s' (%d command(s))\n",
           s_scene_npc_bindings[bi].name, s_scene_npc_bindings[bi].scene, ncmd);
    return 1;
}

void DebugCLI_PostRender(void) {
    if (s_scene_wait_yesno && YesNo_IsOpen())
        YesNo_PostRender();
}

void DebugCLI_PollExternal(void) {
    poll_cmd_file();
}

int DebugCLI_SceneIsActive(void) {
    return s_scene_active;
}

void DebugCLI_ConsoleOpen(void) {
    if (s_con_open) return;
    if (s_con_overlay_enabled) {
        for (int c = 0; c < SCREEN_WIDTH; c++) {
            s_con_saved[c]                = gScrollTileMap[CON_TMIDX(CON_TOP_ROW, c)];
            s_con_saved[SCREEN_WIDTH + c] = gScrollTileMap[CON_TMIDX(CON_IN_ROW,  c)];
        }
    }
    s_con_len    = 0;
    s_con_buf[0] = '\0';
    s_con_blink  = 0;
    s_con_open   = 1;
    con_draw();
}

void DebugCLI_ConsoleClose(void) {
    if (s_con_always_open) return;
    if (!s_con_open) return;
    if (s_con_overlay_enabled) {
        for (int c = 0; c < SCREEN_WIDTH; c++) {
            gScrollTileMap[CON_TMIDX(CON_TOP_ROW, c)] = s_con_saved[c];
            gScrollTileMap[CON_TMIDX(CON_IN_ROW,  c)] = s_con_saved[SCREEN_WIDTH + c];
        }
    }
    s_con_open = 0;
}

int DebugCLI_ConsoleIsOpen(void) {
    return s_con_open;
}

void DebugCLI_ConsoleAddChar(char c) {
    if (!s_con_open || s_con_len >= CON_BUFMAX) return;
    s_con_buf[s_con_len++] = c;
    s_con_buf[s_con_len]   = '\0';
    s_con_blink = 0;
    con_draw();
}

void DebugCLI_ConsoleBackspace(void) {
    if (!s_con_open || s_con_len == 0) return;
    s_con_buf[--s_con_len] = '\0';
    s_con_blink = 0;
    con_draw();
}

void DebugCLI_ConsoleExecute(void) {
    if (!s_con_open) return;
    if (s_con_len > 0) {
        printf("[console] %s\n", s_con_buf);
        {
            char logline[CLI_HIST_WIDTH + 1];
            snprintf(logline, sizeof(logline), "> %s", s_con_buf);
            s_last_cmd_hist_slot = cli_hist_push(logline);
        }
        process_cmd(s_con_buf);
        if (s_last_cmd_hist_slot >= 0) {
            s_hist_color[s_last_cmd_hist_slot] =
                (uint8_t)(s_last_cmd_valid ? CLI_HIST_COLOR_OK : CLI_HIST_COLOR_ERROR);
            s_last_cmd_hist_slot = -1;
        }
    }
    if (s_con_always_open) {
        s_con_len = 0;
        s_con_buf[0] = '\0';
        s_con_blink = 0;
        con_draw();
    } else {
        DebugCLI_ConsoleClose();
    }
}

void DebugCLI_ConsoleRender(void) {
    if (!s_con_open) return;
    s_con_blink = (s_con_blink + 1) % CON_BLINK_PERIOD;
    con_draw();
}

void DebugCLI_ConsoleSetOverlayEnabled(int enabled) {
    s_con_overlay_enabled = enabled ? 1 : 0;
}

void DebugCLI_ConsoleSetAlwaysOpen(int enabled) {
    s_con_always_open = enabled ? 1 : 0;
    if (s_con_always_open && !s_con_open) DebugCLI_ConsoleOpen();
}

const char *DebugCLI_ConsoleGetBuffer(void) {
    return s_con_buf;
}
