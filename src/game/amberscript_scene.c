
#include "amberscript_scene.h"
#include "amberscript_core.h"
#include "debug_trace.h"
#include "amberscript_tilemod.h"
#include "amberscript_mapbank.h"
#include "../platform/game_version.h"

#include "overworld.h"
#include "npc.h"
#include "warp.h"
#include "text.h"
#include "glitches.h"
#include "yesno.h"
#include "bikeshop_menu.h"
#include "pokemon.h"
#include "music.h"
#include "player.h"
#include "trainer_sight.h"
#include "rival_starter.h"
#include "gate_scripts.h"
#include "route24_scripts.h"
#include "viridian_mart_scripts.h"
#include "gym_scripts.h"
#include "rockethideout_b4f_scripts.h"
#include "battle/battle_ui.h"
#include "battle/battle_loop.h"
#include "battle/battle_init.h"
#include "debug_cli.h"
#include "py_ai_bridge.h"
#include "type_mod.h"
#include "species_mod.h"
#include "sprite_mod.h"
#include "pokedex.h"
#include "bills_pokemon_list.h"
#include "badge.h"
#include "badge_house_menu.h"
#include "diploma.h"
#include "town_map.h"
#include "ss_anne_depart.h"
#include "blackboard.h"
#include "bag_list_choice.h"
#include "prize_list_choice.h"
#include "money_box.h"
#include "coin_box.h"
#include "fossil_popup.h"
#include "rom_text.h"
#include "dex_rating.h"
#include "trade.h"
#include "naming_screen.h"
#include "../data/font_data.h"
#include "../platform/audio.h"
#include "inventory.h"
#include "safari_zone_scripts.h"
#include "../data/item_names_gen.h"
#include "../platform/display.h"
#include "../platform/hardware.h"
#include "../platform/input.h"
#include "constants.h"
#include "../data/trainer_sprites.h"
#include "../data/trainer_data.h"
#include "../data/moves_data.h"
#include "../data/base_stats.h"
#include "../data/map_data.h"
#include "../data/event_data.h"
#include "../data/event_flag_names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "battle/battle.h"
#include "gbc_color.h"

#ifdef _WIN32
#define PKS_POPEN _popen
#define PKS_PCLOSE _pclose
#else
#define PKS_POPEN popen
#define PKS_PCLOSE pclose
#endif

extern int Game_GetScene(void);
extern void Game_SetScene(int s);
extern int Game_IsWarpFadeActive(void);
extern void Game_StartTrainerBattleScripted(uint8_t trainer_class, uint8_t trainer_no);
extern void Game_StartWildBattleScripted(uint8_t species, uint8_t level);
extern void Game_StartCustomTrainerBattleScripted(uint8_t trainer_class,
                                                   uint8_t music_id,
                                                   const uint8_t species[6],
                                                   const uint8_t level[6],
                                                   const uint8_t moves[6][4],
                                                   uint8_t count);

#define SCENE_ACTOR_MAX 256
typedef struct scene_npc_binding_t {
    int used;
    int npc_idx;
    uint8_t map_id;
    uint8_t sprite_id;
    int tile_x;
    int tile_y;
    char name[24];
    char scene[64];

    uint8_t auto_spawn;

    uint8_t tile_only;
} scene_npc_binding_t;
static scene_npc_binding_t s_scene_npc_bindings[SCENE_ACTOR_MAX];

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

#define SCENE_CMD_MAX 128
#define SCENE_DEF_MAX 16
#define SCENE_DEF_LINE_MAX 32

#define SCENE_TEXT_MAX 384
typedef enum scene_op_t {
    SCOP_NOP = 0,
    SCOP_SPAWN,
    SCOP_DESPAWN,
    SCOP_FACE,
    SCOP_MOVE,
    SCOP_MOVE_TO_PLAYER,
    SCOP_SAY,
    SCOP_SAY_HOLD,
    SCOP_CLOSE_TEXT,
    SCOP_ASK,
    SCOP_PRICED_CHOICE,
    SCOP_BATTLESTART,
    SCOP_WILDBATTLE,
    SCOP_BATTLEEND,
    SCOP_MUSIC,
    SCOP_MUSIC_FROM_LOOP,
    SCOP_MUSIC_RIVAL_ALT,
    SCOP_WAIT,
    SCOP_WAIT_TEXT,
    SCOP_WAIT_SFX,
    SCOP_WAIT_CRY,
    SCOP_WAIT_MUSIC,
    SCOP_LOCK_INPUT,
    SCOP_FULLRATE,
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
    SCOP_TILE_ART_LOAD,

    SCOP_QUEUE,
    SCOP_MARCH,
    SCOP_WALK_TO_X,

    SCOP_SIM_WALK,
    SCOP_SET_EVENT,
    SCOP_GIVE_BADGE,

    SCOP_LET,

    SCOP_IF,
    SCOP_JUMP,

    SCOP_GIVE,
    SCOP_HIDE,
    SCOP_SHOW,
    SCOP_REFRESH_NPCS,
    SCOP_REFRESH_TILES,
    SCOP_SERVICE,
    SCOP_ENGAGE_TRAINER,
    SCOP_HEAL,
    SCOP_FADE_OUT_WHITE,
    SCOP_FADE_IN_WHITE,
    SCOP_FADE_OUT_BLACK,
    SCOP_FADE_IN_BLACK,
    SCOP_SHOW_DEX,
    SCOP_SHOW_TOWNMAP,
    SCOP_SHIP_DEPART,
    SCOP_SHOW_BLACKBOARD,
    SCOP_SHOW_LINK_CABLE_HELP,
    SCOP_SHOW_DEX_RATING,
    SCOP_BILLS_DEX_LIST,
    SCOP_DIPLOMA,
    SCOP_BADGE_HOUSE_MENU,
    SCOP_SHOW_FOSSIL,
    SCOP_FOSSIL_SELECT,
    SCOP_FOSSIL_NAMES,
    SCOP_SHOW_MONEY,
    SCOP_HIDE_MONEY,
    SCOP_SHOW_COIN_BOX,
    SCOP_HIDE_COIN_BOX,
    SCOP_NAME,
    SCOP_SFX,
    SCOP_CRY,
    SCOP_PLACE,
    SCOP_SFX_ON_CLOSE,
    SCOP_SFX_ON_PRINT,
    SCOP_CRY_ON_PRINT,
    SCOP_GIVE_ITEM,
    SCOP_LIST_CHOICE,
    SCOP_PRIZE_LIST,
    SCOP_GIVE_POKEMON,
    SCOP_TAKE_ITEM,
    SCOP_PAY,
    SCOP_TAKE_COINS,
    SCOP_GIVE_COINS,
    SCOP_TRADE,
    SCOP_TRADE_CUSTOM,
    SCOP_EMOTE,
    SCOP_ENTER_SAFARI,
    SCOP_LEAVE_SAFARI,
    SCOP_GYM_LEADER,
    SCOP_WARP,
    SCOP_WARP_PAD,
    SCOP_END
} scene_op_t;
typedef struct scene_cmd_t {
    scene_op_t op;
    char actor[24];
    int a, b, c, d;
    char text[SCENE_TEXT_MAX];
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
    int persist;
    uint8_t sprite_id;
    int last_x;
    int last_y;

    int has_key;
    int key_x, key_y;
} scene_actor_t;
static int s_scene_active = 0;

static int s_scene_fullrate = 0;
static scene_cmd_t s_scene_cmds[SCENE_CMD_MAX];
static int s_scene_cmd_count = 0;
static int s_scene_pc = 0;
static int s_scene_wait = 0;
static int s_scene_wait_yesno = 0;
static int s_scene_wait_priced_choice = 0;
static int s_scene_wait_say = 0;
static int s_scene_say_opened = 0;

static int s_scene_say_auto = 0;

static int s_scene_wait_print = 0;
static int s_scene_print_opened = 0;
static int s_scene_wait_battle = 0;

static int s_scene_wait_engage_pretext = 0;
static int s_scene_wait_battleend_text = 0;

static int s_scene_input_locked = 0;
static int s_scene_wait_dex = 0;
static int s_scene_wait_townmap = 0;
static int s_scene_wait_ship_depart = 0;
static int s_scene_wait_blackboard = 0;
static int s_scene_wait_link_cable_help = 0;
static int s_scene_wait_list_choice = 0;
static uint8_t s_scene_last_list_choice = 0;
static int s_scene_wait_prize_list = 0;
static int s_scene_last_prize_choice = 0;

static uint8_t  s_prize_entries[3] = {0, 0, 0};
static uint16_t s_prize_prices[3]  = {0, 0, 0};

static int pks_prize_level_for(uint8_t species) {
    if (!gPrizeMonLevels) return 0;
    for (uint32_t i = 0; i + 1 < gPrizeMonLevels_count; i += 2)
        if (gPrizeMonLevels[i] == species) return gPrizeMonLevels[i + 1];
    return 0;
}

static uint16_t pks_prize_bcd(uint8_t hi, uint8_t lo) {
    return (uint16_t)(((hi >> 4) & 15) * 1000 + (hi & 15) * 100 +
                      ((lo >> 4) & 15) * 10   + (lo & 15));
}

static int s_scene_frozen_npc = -1;
static int s_scene_frozen_npc_movetype = -1;

static int s_scene_frozen_map = -1;
static int s_scene_frozen_sprite = -1;
static int s_scene_wait_fossil = 0;
static int s_scene_wait_trade = 0;
static int s_scene_wait_bills_dex_list = 0;
static int s_scene_wait_badge_house = 0;
static int s_scene_wait_diploma = 0;

static int s_scene_fade_active = 0;
static int s_scene_fade_step   = 0;
static int s_scene_fade_timer  = 0;

static const uint8_t kSceneFadeOutToWhite[3][3] = {
    {0x90, 0x80, 0x90}, {0x40, 0x40, 0x40}, {0x00, 0x00, 0x00},
};
static const uint8_t kSceneFadeInFromWhite[3][3] = {
    {0x40, 0x40, 0x40}, {0x90, 0x80, 0x90}, {0xE4, 0xD0, 0xE0},
};

static const uint8_t kSceneFadeOutToBlack[4][3] = {
    {0xE4, 0xD0, 0xE0}, {0xF9, 0xE4, 0xE4}, {0xFE, 0xFE, 0xF8}, {0xFF, 0xFF, 0xFF},
};
static const uint8_t kSceneFadeInFromBlack[4][3] = {
    {0xFF, 0xFF, 0xFF}, {0xFE, 0xFE, 0xF8}, {0xF9, 0xE4, 0xE4}, {0xE4, 0xD0, 0xE0},
};
#define SCENE_FADE_TICKS_PER_STEP 8
static int s_scene_fade_steps = 3;
static int s_scene_wait_dex_delay = 0;
static int s_scene_wait_emote = 0;
static int s_scene_emote_npc = -1;
static int s_scene_wait_name = 0;
static int s_scene_battle_started = 0;
static int s_scene_battlestart_pending = 0;
static int s_scene_battlestart_saw_text = 0;
static int s_scene_battlestart_delay = 0;
static int s_scene_battlestart_tc = 0;
static int s_scene_battlestart_tn = 0;
static int s_scene_battlestart_noblackout = 0;
static int s_scene_last_yesno = -1;

static int s_scene_last_give_full = 0;

static int s_scene_last_battle_result = -1;

static int s_scene_last_give_mon_full = 0;

static int s_scene_last_give_mon_boxed = 0;
static uint8_t s_scene_yesno_prev_joyignore = 0;
static int s_scene_yesno_restore_joyignore = 0;
static int s_scene_yesno_prev_scripted_movement = 0;
static int s_scene_yesno_restore_scripted_movement = 0;
static int s_scene_move_steps_left = 0;
static int s_scene_move_dir = 0;
static int s_scene_move_actor = -1;

static int s_scene_move_awaiting_stop = 0;

#define SCENE_MARCH_STEPS_MAX 48
#define SCENE_STEP_FACE 4
#define SCENE_MARCH_FACE_BASE 100
#define SCENE_MARCH_PAUSE (-1)
typedef struct scene_march_t {
    int used;
    char actor[24];
    int is_player;
    int steps[SCENE_MARCH_STEPS_MAX];
    int len;
    int idx;
} scene_march_t;
static scene_march_t s_march[SCENE_ACTOR_MAX + 1];
static int s_scene_march_active = 0;
static void scene_march_clear(void);

static int s_march_dbg_enabled = 1;
void AmberScript_SetMarchDebug(int on) { s_march_dbg_enabled = on ? 1 : 0; }
int  AmberScript_GetMarchDebug(void) { return s_march_dbg_enabled; }

static FILE *s_march_dbg_file = NULL;
static void march_dbg_vlog(const char *fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);
    vprintf(fmt, ap);
    if (!s_march_dbg_file) s_march_dbg_file = fopen("march_debug.log", "w");
    if (s_march_dbg_file) {
        vfprintf(s_march_dbg_file, fmt, ap2);
        fflush(s_march_dbg_file);
    }
    va_end(ap2);
}
static void march_dbg_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    march_dbg_vlog(fmt, ap);
    va_end(ap);
}
static int s_march_dbg_frame = -1;
static const char *march_dir_name(int e) {
    if (e < 0) return "PAUSE";
    if (e >= SCENE_MARCH_FACE_BASE) {
        static const char *fn[] = {"FACE_DOWN","FACE_UP","FACE_LEFT","FACE_RIGHT"};
        int d = e - SCENE_MARCH_FACE_BASE;
        return (d >= 0 && d < 4) ? fn[d] : "FACE_?";
    }
    static const char *dn[] = {"DOWN","UP","LEFT","RIGHT"};
    return (e >= 0 && e < 4) ? dn[e] : "?";
}
static void scene_march_dbg_snapshot(const char *tag);

static int s_scene_wtx_active = 0;
static int s_scene_wtx_is_player = 0;
static int s_scene_wtx_actor = -1;
static int s_scene_wtx_target = 0;

static int s_scene_sim_walk_active = 0;
static int8_t s_scene_sim_walk_seq[32];

#define SCENE_VAR_MAX 16
typedef struct scene_var_t { char name[16]; int val; } scene_var_t;
static scene_var_t s_scene_vars[SCENE_VAR_MAX];
static int s_scene_var_count = 0;

static char s_scene_text_buf[SCENE_TEXT_MAX];

static int s_scene_ctx_npc = -1;

static int scene_var_find(const char *name) {
    for (int i = 0; i < s_scene_var_count; i++)
        if (strcmp(s_scene_vars[i].name, name) == 0) return i;
    return -1;
}
static void scene_var_set(const char *name, int val) {
    int i = scene_var_find(name);
    if (i < 0) {
        if (s_scene_var_count >= SCENE_VAR_MAX) return;
        i = s_scene_var_count++;
        snprintf(s_scene_vars[i].name, sizeof(s_scene_vars[i].name), "%s", name);
    }
    s_scene_vars[i].val = val;
}

typedef struct { const char *p; int ok; } eval_st;

static int scene_ctx_species(void);
static int pks_resolve_event_token(const char *tok, uint16_t *out_event);
static uint32_t pks_money_get(void);
static uint32_t pks_coins_get(void);
static int pks_resolve_item_id(const char *tok);

static void eval_ws(eval_st *e) { while (*e->p == ' ' || *e->p == '\t') e->p++; }

static int eval_word(eval_st *e, const char *w) {
    eval_ws(e);
    size_t n = strlen(w);
    if (strncmp(e->p, w, n) != 0) return 0;
    char after = e->p[n];
    if (isalnum((unsigned char)after) || after == '_' || after == '.') return 0;
    e->p += n;
    return 1;
}

static int eval_read_ident(eval_st *e, char *buf, int cap) {
    eval_ws(e);
    int i = 0;
    while ((isalnum((unsigned char)*e->p) || *e->p == '_' || *e->p == '.') && i < cap - 1)
        buf[i++] = *e->p++;
    buf[i] = '\0';
    return i;
}

static int scene_resolve_species(const char *name) {
    uint8_t sid = 0;
    if (!name || !*name) return 0;

    if (!strncmp(name, "MONSTER", 7) || !strncmp(name, "monster", 7)) {
        char *e2;
        long d2 = strtol(name + 7, &e2, 10);
        if (e2 != name + 7 && (*e2 == '\0' || *e2 == ' ' || *e2 == '\t')
            && d2 >= 1 && d2 <= 151)
            return gDexToSpecies[d2];
    }
    if (SpeciesMod_ResolveSpeciesToken(name, &sid) && sid) return (int)sid;
    char needle[40]; int ni = 0;
    for (int i = 0; name[i] && ni < 39; i++) {
        char c = (char)tolower((unsigned char)name[i]);
        if (c != ' ' && c != '_') needle[ni++] = c;
    }
    needle[ni] = '\0';
    if (ni == 0) return 0;
    for (int dex = 1; dex <= 151; dex++) {
        const char *nm = Pokemon_GetName((uint8_t)dex);
        if (!nm) continue;
        char norm[40]; int nn = 0;
        for (int i = 0; nm[i] && nn < 39; i++) {
            char c = (char)tolower((unsigned char)nm[i]);
            if (c != ' ' && c != '_') norm[nn++] = c;
        }
        norm[nn] = '\0';
        if (strcmp(needle, norm) == 0) return gDexToSpecies[dex];
    }
    return 0;
}

static int eval_resolve_ident(const char *name) {
    int vi = scene_var_find(name);
    if (vi >= 0) return s_scene_vars[vi].val;
    if (!strcmp(name, "yes") || !strcmp(name, "true"))  return 1;
    if (!strcmp(name, "no")  || !strcmp(name, "false")) return 0;
    if (!strcmp(name, "answer")) return s_scene_last_yesno > 0 ? 1 : 0;
    if (!strcmp(name, "bag_full")) return s_scene_last_give_full;

    if (!strcmp(name, "battle_result")) return s_scene_last_battle_result;
    if (!strcmp(name, "battle_caught")) return s_scene_last_battle_result == BATTLE_OUTCOME_CAUGHT;
    if (!strcmp(name, "battle_won"))    return s_scene_last_battle_result == BATTLE_OUTCOME_WILD_VICTORY;

    if (!strcmp(name, "party_full")) return s_scene_last_give_mon_full;
    if (!strcmp(name, "sent_to_box")) return s_scene_last_give_mon_boxed;
    if (!strcmp(name, "box_num")) return (int)(wCurrentBoxNum % NUM_BOXES) + 1;

    if (!strcmp(name, "chosen_item")) return (int)s_scene_last_list_choice;

    if (!strcmp(name, "fossil_item")) return (int)wFossilItem;
    if (!strcmp(name, "fossil_mon"))  return (int)wFossilMon;

    if (!strcmp(name, "prize_choice")) return s_scene_last_prize_choice;

    if (!strcmp(name, "prize_price") || !strcmp(name, "prize_species") ||
        !strcmp(name, "prize_level")) {
        int c = s_scene_last_prize_choice;
        if (c < 1 || c > 3) return 0;
        if (!strcmp(name, "prize_price"))   return s_prize_prices[c - 1];
        if (!strcmp(name, "prize_species")) return s_prize_entries[c - 1];
        return pks_prize_level_for(s_prize_entries[c - 1]);
    }
    if (!strcmp(name, "coins")) return (int)pks_coins_get();

    if (!strcmp(name, "trade_result")) return Trade_GetResult();
    if (!strcmp(name, "money")) return (int)pks_money_get();
    if (!strcmp(name, "music_playing")) return Music_IsPlaying() ? 1 : 0;
    if (!strcmp(name, "self.species") || !strcmp(name, "ball.species"))
        return scene_ctx_species();
    if (!strcmp(name, "self.x") || !strcmp(name, "ball.x") ||
        !strcmp(name, "self.y") || !strcmp(name, "ball.y")) {
        if (s_scene_ctx_npc < 0) return 0;
        int tx = 0, ty = 0;
        NPC_GetTilePos(s_scene_ctx_npc, &tx, &ty);
        return (name[5] == 'x') ? tx : ty;
    }
    if (!strcmp(name, "player.x")) return (int)wXCoord;
    if (!strcmp(name, "player.y")) return (int)wYCoord;

    if (!strcmp(name, "player.facing")) return (int)gPlayerFacing;

    if (!strcmp(name, "glitches")) return Glitches_IsEnabled();

    if (!strcmp(name, "held_dir")) {
        uint8_t raw = Input_RawHeld();
        if (raw & PAD_DOWN)  return 0;
        if (raw & PAD_UP)    return 1;
        if (raw & PAD_LEFT)  return 2;
        if (raw & PAD_RIGHT) return 3;
        return -1;
    }
    if (!strcmp(name, "badges")) {

        uint8_t v = wObtainedBadges, c = 0;
        while (v) { c += v & 1; v >>= 1; }
        return (int)c;
    }
    if (!strcmp(name, "dex_owned")) {

        int c = 0;
        for (int i = 0; i < 19; i++) {
            uint8_t v = wPokedexOwned[i];
            while (v) { c += v & 1; v >>= 1; }
        }
        return c;
    }
    if (!strcmp(name, "party.species")) return (int)wPartySpecies[0];
    if (!strcmp(name, "rival.starter")) return RivalStarter_Get();

    uint16_t ev;
    if (pks_resolve_event_token(name, &ev)) return CheckEvent(ev) ? 1 : 0;
    {
        int sp = scene_resolve_species(name);
        if (sp > 0) return sp;
    }
    {
        int it = pks_resolve_item_id(name);
        if (it > 0) return it;
    }
    return 0;
}

static int eval_or_(eval_st *e);

static int eval_primary(eval_st *e) {
    eval_ws(e);
    if (*e->p == '(') { e->p++; int v = eval_or_(e); eval_ws(e); if (*e->p == ')') e->p++; else e->ok = 0; return v; }
    if (*e->p == '-') { e->p++; return -eval_primary(e); }
    if (eval_word(e, "not")) return !eval_primary(e);
    if (isdigit((unsigned char)*e->p)) return (int)strtol(e->p, (char **)&e->p, 0);
    char id[48];
    if (!eval_read_ident(e, id, sizeof id)) { e->ok = 0; return 0; }
    eval_ws(e);
    if (*e->p == '(') {
        e->p++;
        int arg = (*e->p == ')') ? 0 : eval_or_(e);
        eval_ws(e); if (*e->p == ')') e->p++; else e->ok = 0;
        if (!strcmp(id, "has_item")) {

            return (arg > 0 && Inventory_GetQty((uint8_t)arg) > 0) ? 1 : 0;
        }
        if (!strcmp(id, "has_coins")) {

            return ((uint32_t)arg <= pks_coins_get()) ? 1 : 0;
        }
        if (!strcmp(id, "beats")) {

            int c = scene_resolve_species("CHARMANDER");
            int s = scene_resolve_species("SQUIRTLE");
            int b = scene_resolve_species("BULBASAUR");
            if (arg == c) return s;
            if (arg == s) return b;
            if (arg == b) return c;
            return arg;
        }
        if (!strcmp(id, "trainer_beaten")) {

            return AmberScript_IsTrainerBeatenAt(wCurMap, arg / 100, arg % 100);
        }
        return arg;
    }
    return eval_resolve_ident(id);
}

static int eval_mul(eval_st *e) {
    int v = eval_primary(e);
    for (;;) { eval_ws(e);
        if (*e->p == '*') { e->p++; v *= eval_primary(e); }
        else if (*e->p == '/') { e->p++; int d = eval_primary(e); v = d ? v / d : 0; }
        else break; }
    return v;
}
static int eval_add(eval_st *e) {
    int v = eval_mul(e);
    for (;;) { eval_ws(e);
        if (*e->p == '+') { e->p++; v += eval_mul(e); }
        else if (*e->p == '-') { e->p++; v -= eval_mul(e); }
        else break; }
    return v;
}
static int eval_cmp(eval_st *e) {
    int v = eval_add(e);
    for (;;) { eval_ws(e);
        if      (!strncmp(e->p, "==", 2)) { e->p += 2; v = (v == eval_add(e)); }
        else if (!strncmp(e->p, "!=", 2)) { e->p += 2; v = (v != eval_add(e)); }
        else if (!strncmp(e->p, "<=", 2)) { e->p += 2; v = (v <= eval_add(e)); }
        else if (!strncmp(e->p, ">=", 2)) { e->p += 2; v = (v >= eval_add(e)); }
        else if (*e->p == '<')            { e->p += 1; v = (v <  eval_add(e)); }
        else if (*e->p == '>')            { e->p += 1; v = (v >  eval_add(e)); }
        else if (eval_word(e, "is"))      { v = (v == eval_add(e)); }
        else break; }
    return v;
}
static int eval_and(eval_st *e) {
    int v = eval_cmp(e);
    while (eval_word(e, "and")) { int r = eval_cmp(e); v = (v && r); }
    return v;
}
static int eval_or_(eval_st *e) {
    int v = eval_and(e);
    while (eval_word(e, "or")) { int r = eval_and(e); v = (v || r); }
    return v;
}

static int scene_eval(const char *src, int *ok) {
    eval_st e = { src ? src : "", 1 };
    int v = eval_or_(&e);
    eval_ws(&e);
    if (*e.p != '\0') e.ok = 0;
    if (ok) *ok = e.ok;
    return v;
}

static int scene_ctx_species(void) {
    (void)s_scene_ctx_npc;
    return 0;
}

static void scene_interp_text(const char *src, char *dst, int dstsz) {
    int di = 0;
    for (int si = 0; src[si] && di < dstsz - 1; ) {
        if (src[si] == '{') {
            const char *close = strchr(src + si, '}');
            if (close) {

                char inner[24];
                int il = (int)(close - (src + si + 1));
                if (il > 0 && il < (int)sizeof(inner)) {
                    memcpy(inner, src + si + 1, il);
                    inner[il] = '\0';

                    if (strncmp(inner, "mon:", 4) == 0) {
                        int vi = scene_var_find(inner + 4);
                        if (vi >= 0) {
                            const char *nm = Pokemon_GetNameBySpecies((uint8_t)s_scene_vars[vi].val);
                            di += snprintf(dst + di, dstsz - di, "%s", nm ? nm : "?");
                            si = (int)(close - src) + 1;
                            continue;
                        }
                    } else if (strcmp(inner, "box_num") == 0) {

                        di += snprintf(dst + di, dstsz - di, "%d",
                                        (int)(wCurrentBoxNum % NUM_BOXES) + 1);
                        si = (int)(close - src) + 1;
                        continue;
                    } else {

                        int vi = scene_var_find(inner);
                        if (vi >= 0) {
                            di += snprintf(dst + di, dstsz - di, "%d", s_scene_vars[vi].val);
                            si = (int)(close - src) + 1;
                            continue;
                        }
                    }
                }
            }
        }
        dst[di++] = src[si++];
    }
    dst[di] = '\0';
}

static uint8_t s_scene_map = 0;
static scene_actor_t s_scene_actors[SCENE_ACTOR_MAX];
typedef struct scene_def_t {
    int used;
    char name[32];
    char lines[SCENE_DEF_LINE_MAX][192];
    int line_count;
} scene_def_t;
static scene_def_t s_scene_defs[SCENE_DEF_MAX];

#define SCENE_TRIGGER_MAX 256
typedef struct scene_trigger_t {
    int used;
    char scene[64];
    uint8_t map_id;
    int x;
    int y;
    int armed;
    uint8_t cond_kind;
    uint16_t cond_event;
    uint8_t cond_kind2;
    uint16_t cond_event2;

    int is_onload;

    int is_watch;

    int fired_x;
    int fired_y;
} scene_trigger_t;
static scene_trigger_t s_scene_triggers[SCENE_TRIGGER_MAX];

#define SCENE_ZONE_LATCH_MAX 8
#define SCENE_ZONE_LATCH_TILES 8
typedef struct {
    int used;
    uint8_t map_id;
    uint16_t event;
    int ntiles;
    int x[SCENE_ZONE_LATCH_TILES];
    int y[SCENE_ZONE_LATCH_TILES];
} scene_zone_latch_t;
static scene_zone_latch_t s_scene_zone_latches[SCENE_ZONE_LATCH_MAX];

static int s_scene_map_recheck_pending = 0;

void AmberScript_Scene_NotifyMapLoaded(void) {
    s_scene_map_recheck_pending = 1;
}

void AmberScript_Scene_NotifyBattleEnded(void) {
    s_scene_map_recheck_pending = 1;
}

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

static int scene_npc_binding_find_by_idx(int npc_idx) {
    int tx = 0, ty = 0;
    int cur_tx = 0, cur_ty = 0;
    int have_cur_pos = (npc_idx >= 0 && npc_idx < NPC_GetCount());
    if (have_cur_pos) NPC_GetTilePos(npc_idx, &cur_tx, &cur_ty);

    if (have_cur_pos) {
        for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
            if (!s_scene_npc_bindings[i].used) continue;
            if (s_scene_npc_bindings[i].tile_only) continue;
            if (s_scene_npc_bindings[i].auto_spawn) continue;
            if (s_scene_npc_bindings[i].map_id != wCurMap) continue;
            int live = AmberScript_MapFindLiveNpcByDeclaredTile(
                (int)wCurMap, s_scene_npc_bindings[i].tile_x,
                s_scene_npc_bindings[i].tile_y);
            if (live < 0 || live != npc_idx) continue;
            s_scene_npc_bindings[i].npc_idx = npc_idx;
            return i;
        }
    }
    for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
        if (!s_scene_npc_bindings[i].used) continue;
        if (s_scene_npc_bindings[i].tile_only) continue;
        if (s_scene_npc_bindings[i].npc_idx != npc_idx) continue;
        if (s_scene_npc_bindings[i].map_id != wCurMap) continue;

        if (have_cur_pos && (s_scene_npc_bindings[i].tile_x != cur_tx || s_scene_npc_bindings[i].tile_y != cur_ty)) {
            s_scene_npc_bindings[i].npc_idx = -1;
            continue;
        }
        return i;
    }
    if (have_cur_pos) {
        tx = cur_tx; ty = cur_ty;
        for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
            if (!s_scene_npc_bindings[i].used) continue;
            if (s_scene_npc_bindings[i].tile_only) continue;
            if (s_scene_npc_bindings[i].map_id != wCurMap) continue;
            if (s_scene_npc_bindings[i].tile_x != tx || s_scene_npc_bindings[i].tile_y != ty) continue;
            s_scene_npc_bindings[i].npc_idx = npc_idx;
            return i;
        }
    }
    return -1;
}

static int scene_npc_binding_alloc(void) {
    for (int i = 0; i < SCENE_ACTOR_MAX; i++)
        if (!s_scene_npc_bindings[i].used) return i;
    return -1;
}

void AmberScript_Scene_ClearNpcBindingsForMap(uint8_t map_id) {
    for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
        if (s_scene_npc_bindings[i].used && s_scene_npc_bindings[i].map_id == map_id)
            memset(&s_scene_npc_bindings[i], 0, sizeof(s_scene_npc_bindings[i]));
    }
}

void AmberScript_Scene_ClearTriggersForMap(uint8_t map_id) {
    for (int i = 0; i < SCENE_TRIGGER_MAX; i++) {
        if (s_scene_triggers[i].used && (uint8_t)s_scene_triggers[i].map_id == map_id)
            memset(&s_scene_triggers[i], 0, sizeof(s_scene_triggers[i]));
    }
}

void AmberScript_Scene_ClearAllMapBindings(void) {
    memset(s_scene_triggers, 0, sizeof(s_scene_triggers));
    memset(s_scene_npc_bindings, 0, sizeof(s_scene_npc_bindings));
}

static int scene_saved_tile_alloc(const char *name) {
    int slot = -1;
    for (int i = 0; i < SCENE_SAVED_TILE_MAX; i++) {
        if (s_scene_saved_tiles[i].used && strcmp(s_scene_saved_tiles[i].name, name) == 0) return i;
    }
    for (int i = 0; i < SCENE_SAVED_TILE_MAX; i++) {
        if (!s_scene_saved_tiles[i].used) { slot = i; break; }
    }
    return slot;
}

static int scene_saved_block_alloc(const char *name) {
    int slot = -1;
    for (int i = 0; i < SCENE_SAVED_BLOCK_MAX; i++) {
        if (s_scene_saved_blocks[i].used && strcmp(s_scene_saved_blocks[i].name, name) == 0) return i;
    }
    for (int i = 0; i < SCENE_SAVED_BLOCK_MAX; i++) {
        if (!s_scene_saved_blocks[i].used) { slot = i; break; }
    }
    return slot;
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

static int scene_tile_prop_alloc_slot(void) {
    for (int i = 0; i < SCENE_TILE_PROP_MAX; i++)
        if (!s_scene_tile_props[i].used) return i;
    return -1;
}

static int s_dsl_bank_enabled = 0;
static int s_dsl_bank_init_done = 0;
static uint8_t s_dsl_bank_last_map = 0xFF;

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

            if (!b->auto_spawn) continue;
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

    for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
        if (s_scene_npc_bindings[i].used && s_scene_npc_bindings[i].auto_spawn)
            memset(&s_scene_npc_bindings[i], 0, sizeof(s_scene_npc_bindings[i]));
    }
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

            s_scene_npc_bindings[slot].auto_spawn = (sprite_id != 0) ? 1 : 0;
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

        if (idx < 0 && b->auto_spawn) idx = NPC_DebugSpawn(b->sprite_id, b->tile_x, b->tile_y, 0, 0);
        if (idx >= 0) b->npc_idx = idx;
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

static int s_script_trace_enabled = 0;
static int s_script_trace_to_file = 0;
#define PKS_SCRIPT_TRACE_LOG_PATH "bugs/script_trace.log"
static void script_trace_reset_latches(void);

void AmberScript_SetScriptTrace(int on) {
    s_script_trace_enabled = on ? 1 : 0;
    s_script_trace_to_file = s_script_trace_enabled;
    if (s_script_trace_enabled) script_trace_reset_latches();
}

int AmberScript_GetScriptTrace(void) { return s_script_trace_enabled; }

static int s_py_law_enabled = 0;
static int s_py_law_npc_idx = -1;
static int s_py_law_frame_accum = 0;
static uint32_t s_py_law_elapsed_sec = 0;
static char s_py_law_script[160] = {0};

static char *pks_trim(char *s) {
    char *e;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) e--;
    *e = '\0';
    return s;
}

static void pks_normalize_ascii(char *s) {
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

        if (buf[ri] == 0xC2 && buf[ri + 1] == 0xA5) {
            buf[wi++] = 0xA5;
            ri += 2;
            continue;
        }
        ri++;
    }
    buf[wi] = '\0';
}

static int pks_is_numeric_token(const char *s) {
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

static void pks_norm(char *dst, size_t dst_sz, const char *src) {
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

int pks_resolve_trainer_class_id(const char *tok) {
    char want[40];
    if (!tok || !*tok) return 0;
    if (pks_is_numeric_token(tok)) {
        int v = (int)strtol(tok, NULL, 0);
        if (v >= 1 && v <= NUM_TRAINERS) return v;
        return 0;
    }
    pks_norm(want, sizeof(want), tok);
    for (int i = 0; i < NUM_TRAINERS; i++) {
        char norm[40];
        pks_norm(norm, sizeof(norm), gTrainerClassNames[i]);
        if (strcmp(want, norm) == 0) return i + 1;
    }
    return 0;
}

uint8_t pks_trainer_class_to_overworld_sprite(int trainer_class) {
    switch (trainer_class) {

        case 1:  return 0x04;
        case 2:  return 0x04;
        case 3:  return 0x06;
        case 4:  return 0x13;
        case 5:  return 0x07;
        case 6:  return 0x06;
        case 7:  return 0x0C;
        case 8:  return 0x0C;
        case 9:  return 0x0E;
        case 10: return 0x12;
        case 11: return 0x0C;
        case 12: return 0x0C;
        case 13: return 0x10;
        case 14: return 0x2F;
        case 15: return 0x22;
        case 16: return 0x12;
        case 17: return 0x0B;
        case 18: return 0x0F;
        case 19: return 0x04;
        case 20: return 0x0C;
        case 21: return 0x21;
        case 22: return 0x21;
        case 23: return 0x07;
        case 24: return 0x0E;
        case 25: return 0x02;
        case 26: return 0x03;
        case 27: return 0x10;
        case 28: return 0x20;
        case 29: return 0x17;
        case 30: return 0x18;
        case 31: return 0x07;
        case 32: return 0x06;
        case 33: return 0x3A;

        case 34: return 0x0C;
        case 35: return 0x1D;
        case 36: return 0x21;
        case 37: return 0x1B;
        case 38: return 0x30;
        case 39: return 0x0A;
        case 40: return 0x0D;
        case 41: return 0x10;
        case 42: return 0x02;
        case 43: return 0x02;
        case 44: return 0x3B;
        case 45: return 0x19;
        case 46: return 0x39;
        case 47: return 0x1E;
        default: return 0x04;
    }
}

#include "../data/sprite_names_gen.h"
#include "../data/sprite_data.h"
#include "crystal_sprites.h"
#include "../platform/debug_log.h"

int pks_parse_sprite(const char *tok) {

    static const struct { const char *name; int id; } aliases[] = {
        { "RIVAL", 0x02 }, { "POKEBALL", 0x3D }, { NULL, 0 }
    };
    if (!tok || !*tok) return -1;

    if (strncmp(tok, "crystal:", 8) == 0) {
        for (int i = 0; i < CRYSTAL_NUM_SPRITES; i++)
            if (strcmp(tok + 8, gCrystalSpriteNames[i]) == 0)
                return PKS_CRYSTAL_SPRITE_BASE + i;
        return -1;
    }
    for (int i = 0; aliases[i].name; i++)
        if (strcmp(tok, aliases[i].name) == 0) return aliases[i].id;

    for (unsigned i = 0; i < NUM_SPRITE_NAMES; i++)
        if (strcmp(tok, kSpriteNames[i].name) == 0) return kSpriteNames[i].id;

    for (int i = 0; i < CRYSTAL_NUM_SPRITES; i++)
        if (strcmp(tok, gCrystalSpriteNames[i]) == 0)
            return PKS_CRYSTAL_SPRITE_BASE + i;
    {
        int tc = pks_resolve_trainer_class_id(tok);
        if (tc > 0) return pks_trainer_class_to_overworld_sprite(tc);
    }
    if (pks_is_numeric_token(tok)) return (int)strtol(tok, NULL, 0);
    return -1;
}

static int pks_resolve_item_id(const char *tok) {
    int tmhm;
    if (!tok || !*tok) return -1;
    for (unsigned i = 0; i < NUM_ITEM_NAMES; i++)
        if (strcmp(tok, kItemNames[i].name) == 0) return kItemNames[i].id;

    tmhm = Inventory_TmHmIdFromName(tok);
    if (tmhm > 0) return tmhm;
    if (pks_is_numeric_token(tok)) return (int)strtol(tok, NULL, 0);
    return -1;
}

static uint32_t pks_money_get(void) {
    return (uint32_t)(
        ((wPlayerMoney[0] >> 4) & 0xF) * 100000u +
        (wPlayerMoney[0] & 0xF)        * 10000u  +
        ((wPlayerMoney[1] >> 4) & 0xF) * 1000u   +
        (wPlayerMoney[1] & 0xF)        * 100u     +
        ((wPlayerMoney[2] >> 4) & 0xF) * 10u      +
        (wPlayerMoney[2] & 0xF)
    );
}

static void pks_money_set(uint32_t v) {
    if (v > 999999u) v = 999999u;
    wPlayerMoney[0] = (uint8_t)(((v / 100000u) << 4) | ((v / 10000u) % 10u));
    wPlayerMoney[1] = (uint8_t)((((v / 1000u) % 10u) << 4) | ((v / 100u) % 10u));
    wPlayerMoney[2] = (uint8_t)((((v / 10u) % 10u) << 4) | (v % 10u));
}

static uint32_t pks_coins_get(void) {
    return (uint32_t)(
        ((wPlayerCoins[0] >> 4) & 0xF) * 1000u +
        (wPlayerCoins[0] & 0xF)        * 100u  +
        ((wPlayerCoins[1] >> 4) & 0xF) * 10u   +
        (wPlayerCoins[1] & 0xF)
    );
}

static void pks_coins_set(uint32_t v) {
    if (v > 9999u) v = 9999u;
    wPlayerCoins[0] = (uint8_t)(((v / 1000u) << 4) | ((v / 100u) % 10u));
    wPlayerCoins[1] = (uint8_t)((((v / 10u) % 10u) << 4) | (v % 10u));
}

static void (*pks_resolve_sfx_fn(const char *name))(void) {
    if (!name) return NULL;
    if (strcmp(name, "get_item") == 0) return Audio_PlaySFX_GetItem1;
    if (strcmp(name, "get_key_item") == 0) return Audio_PlaySFX_GetKeyItem;
    if (strcmp(name, "purchase") == 0) return Audio_PlaySFX_Purchase;

    if (strcmp(name, "switch") == 0) return Audio_PlaySFX_Switch;
    if (strcmp(name, "tink") == 0) return Audio_PlaySFX_Tink;
    if (strcmp(name, "shrink") == 0) return Audio_PlaySFX_Shrink;
    if (strcmp(name, "denied") == 0) return Audio_PlaySFX_Denied;

    if (strcmp(name, "go_inside") == 0) return Audio_PlaySFX_GoInside;
    return NULL;
}

static uint8_t s_pending_cry_species = 0;
static void pks_play_pending_cry(void) {
    Audio_PlayCry(s_pending_cry_species);
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
    fp = PKS_POPEN(cmdline, "r");
    if (!fp) return;
    while (fgets(out_line, sizeof(out_line), fp)) {
        char *s = pks_trim(out_line);
        int dir;
        pks_normalize_ascii(s);
        if (sscanf(s, "npc_step %d", &dir) == 1) {
            if (dir >= 0 && dir <= 3 && !NPC_IsWalking(s_py_law_npc_idx)) {
                NPC_DoScriptedStep(s_py_law_npc_idx, dir);
            }
        } else if (sscanf(s, "npc_face %d", &dir) == 1) {
            if (dir >= 0 && dir <= 3) NPC_SetFacing(s_py_law_npc_idx, dir);
        } else if (*s) {
            printf("[amberscript:scene] py_law out: %s\n", s);
        }
    }
    PKS_PCLOSE(fp);
}

static void script_trace_reset_latches(void) {
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

static void script_trace_log_line(const char *line) {
    FILE *fp;
    struct stat st;
    if (!line || !*line) return;
    if (!s_script_trace_to_file) return;
    if (stat(PKS_SCRIPT_TRACE_LOG_PATH, &st) == 0 && st.st_size >= (256 * 1024)) {
        remove(PKS_SCRIPT_TRACE_LOG_PATH ".1");
        rename(PKS_SCRIPT_TRACE_LOG_PATH, PKS_SCRIPT_TRACE_LOG_PATH ".1");
    }
    fp = fopen(PKS_SCRIPT_TRACE_LOG_PATH, "a");
    if (!fp) return;
    fprintf(fp, "%s\n", line);
    fclose(fp);
}

static void script_trace_emitf(const char *fmt, ...) {
    va_list ap;
    char buf[192];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("%s\n", buf);
    script_trace_log_line(buf);
}

static int pks_resolve_move_id(const char *move_str) {
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

static int pks_resolve_species_id(const char *species_str) {
    uint8_t sid = 0;
    if (!species_str || !*species_str) return 0;

    if (!strncmp(species_str, "MONSTER", 7) || !strncmp(species_str, "monster", 7)) {
        char *e2;
        long d2 = strtol(species_str + 7, &e2, 10);

        if (e2 != species_str + 7
            && (*e2 == '\0' || *e2 == ' ' || *e2 == '\t')
            && d2 >= 1 && d2 <= 151)
            return gDexToSpecies[d2];
    }
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

static int pks_resolve_event_token(const char *tok, uint16_t *out_event) {
    char want[96];
    if (!tok || !*tok || !out_event) return 0;
    if (pks_is_numeric_token(tok)) {
        long v = strtol(tok, NULL, 0);
        if (v < 0 || v > 65535) return 0;
        *out_event = (uint16_t)v;
        return 1;
    }
    pks_norm(want, sizeof(want), tok);
    for (int i = 0; i <= 4095; i++) {
        const char *name = EventFlagName((uint16_t)i);
        char norm[96], short_norm[96];
        if (!name || strcmp(name, "UNKNOWN_EVENT") == 0) continue;
        pks_norm(norm, sizeof(norm), name);
        if (strcmp(norm, want) == 0) {
            *out_event = (uint16_t)i;
            return 1;
        }
        if (strncmp(norm, "event", 5) == 0) {
            char *short_p = norm + 5;
            snprintf(short_norm, sizeof(short_norm), "%s", short_p);
            if (strcmp(short_norm, want) == 0) {
                *out_event = (uint16_t)i;
                return 1;
            }
        }
    }
    return 0;
}

static int pks_resolve_map_token(const char *tok, int *out_map_id) {
    char want[64];
    if (!tok || !*tok || !out_map_id) return 0;
    if (strcmp(tok, "here") == 0 || strcmp(tok, "current") == 0) {
        *out_map_id = (int)wCurMap;
        return 1;
    }
    if (pks_is_numeric_token(tok)) {
        *out_map_id = (int)strtol(tok, NULL, 0);
        return (*out_map_id >= 0 && *out_map_id < NUM_MAPS);
    }
    pks_norm(want, sizeof(want), tok);
    for (int i = 0; i < NUM_MAPS; i++) {
        char have[64];
        pks_norm(have, sizeof(have), gMapTable[i].name);
        if (strcmp(want, have) == 0) {
            *out_map_id = i;
            return 1;
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
        int species = pks_resolve_species_id(parts[0]);
        int level = (int)strtol(parts[1], NULL, 0);
        if (species <= 0 || species > 255) return 0;
        if (level < 1 || level > 100) return 0;
        *out_species = (uint8_t)species;
        *out_level = (uint8_t)level;
        for (int i = 0; i < 4; i++) {
            int move = pks_resolve_move_id(parts[2 + i]);
            if (move < 0 || move > 255) return 0;
            out_moves[i] = (uint8_t)move;
        }
    }
    return 1;
}

static int scene_parse_dir(const char *tok) {
    if (!tok) return -1;
    if (strcmp(tok, "down") == 0) return 0;
    if (strcmp(tok, "up") == 0) return 1;
    if (strcmp(tok, "left") == 0) return 2;
    if (strcmp(tok, "right") == 0) return 3;
    return -1;
}

static int scene_parse_movement_extra(const char *s, scene_cmd_t *cmd) {

    if (strncmp(s, "warp ", 5) == 0) {
        char wname[64] = {0}, wx[16] = {0}, wy[16] = {0};
        if (sscanf(s + 5, "%63s %15s %15s", wname, wx, wy) != 3) {
            printf("[amberscript] warp: want `warp <VmapName> <x> <y>`, got '%s'\n", s);
            return 0;
        }
        if (!pks_is_numeric_token(wx) || !pks_is_numeric_token(wy)) {
            printf("[amberscript] warp: x/y must be numeric tiles, got '%s' '%s'\n", wx, wy);
            return 0;
        }
        cmd->op = SCOP_WARP;
        snprintf(cmd->text, sizeof(cmd->text), "%s", wname);
        cmd->b = (int)strtol(wx, NULL, 0);
        cmd->c = (int)strtol(wy, NULL, 0);
        return 1;
    }

    if (strncmp(s, "warp_pad ", 9) == 0) {
        char wname[64] = {0}, wx[16] = {0}, wy[16] = {0};
        if (sscanf(s + 9, "%63s %15s %15s", wname, wx, wy) != 3) {
            printf("[amberscript] warp_pad: want `warp_pad <VmapName> <x> <y>`, got '%s'\n", s);
            return 0;
        }
        if (!pks_is_numeric_token(wx) || !pks_is_numeric_token(wy)) {
            printf("[amberscript] warp_pad: x/y must be numeric tiles, got '%s' '%s'\n", wx, wy);
            return 0;
        }
        cmd->op = SCOP_WARP_PAD;
        snprintf(cmd->text, sizeof(cmd->text), "%s", wname);
        cmd->b = (int)strtol(wx, NULL, 0);
        cmd->c = (int)strtol(wy, NULL, 0);
        return 1;
    }
    if (strncmp(s, "wildbattle ", 11) == 0) {
        char sp[32] = {0}, lv[16] = {0}, flag[16] = {0};
        int species, level;
        int nargs = sscanf(s + 11, "%31s %15s %15s", sp, lv, flag);
        if (nargs != 2 && nargs != 3) {
            printf("[amberscript] wildbattle: want `wildbattle <species> <level> [oldman]`, got '%s'\n", s);
            return 0;
        }
        species = pks_is_numeric_token(sp) ? (int)strtol(sp, NULL, 0)
                                           : scene_resolve_species(sp);
        if (species < 1 || species > 151) {
            printf("[amberscript] wildbattle: unknown species '%s'\n", sp);
            return 0;
        }
        if (!pks_is_numeric_token(lv)) {
            printf("[amberscript] wildbattle: level must be numeric, got '%s'\n", lv);
            return 0;
        }
        level = (int)strtol(lv, NULL, 0);
        if (level < 1 || level > 100) {
            printf("[amberscript] wildbattle: level %d out of range 1..100\n", level);
            return 0;
        }
        if (nargs == 3 && strcmp(flag, "oldman") != 0) {
            printf("[amberscript] wildbattle: unknown flag '%s' (only 'oldman' is recognized)\n", flag);
            return 0;
        }
        cmd->op = SCOP_WILDBATTLE;
        cmd->a = species;
        cmd->b = level;

        cmd->c = (nargs == 3) ? 1 : 0;
        return 1;
    }
    if (strncmp(s, "queue ", 6) == 0) {
        char id[24], a1[24], a2[24];
        if (sscanf(s + 6, "%23s %23s %23s", id, a1, a2) != 3) return 0;
        if (strcmp(a1, "face") == 0) {
            int d = scene_parse_dir(a2);
            if (d < 0) return 0;
            cmd->a = SCENE_STEP_FACE;
            cmd->b = d;
        } else if (strcmp(a1, "pause") == 0) {

            cmd->a = SCENE_MARCH_PAUSE;
            cmd->b = (int)strtol(a2, NULL, 0);
        } else {
            int d = scene_parse_dir(a1);
            if (d < 0) return 0;
            cmd->a = d;
            cmd->b = (int)strtol(a2, NULL, 0);
        }
        snprintf(cmd->actor, sizeof(cmd->actor), "%s", id);
        cmd->op = SCOP_QUEUE;
        return 1;
    }
    if (strncmp(s, "let ", 4) == 0) {

        const char *eq = strchr(s + 4, '=');
        if (!eq) return 0;
        char name[16];
        int n = 0;
        const char *np = s + 4;
        while (*np == ' ') np++;
        while (np < eq && (isalnum((unsigned char)*np) || *np == '_') && n < (int)sizeof(name) - 1)
            name[n++] = *np++;
        name[n] = '\0';
        if (n == 0) return 0;
        const char *expr = eq + 1;
        while (*expr == ' ') expr++;
        snprintf(cmd->actor, sizeof(cmd->actor), "%s", name);
        snprintf(cmd->text, sizeof(cmd->text), "%s", expr);
        cmd->op = SCOP_LET;
        return 1;
    }
    if (strncmp(s, "give ", 5) == 0) {

        char who[16];
        const char *p = s + 5;
        int n = 0;
        while (*p == ' ') p++;
        while (*p && *p != ' ' && n < (int)sizeof(who) - 1) who[n++] = *p++;
        who[n] = '\0';
        while (*p == ' ') p++;
        if (n == 0 || *p == '\0') return 0;
        snprintf(cmd->actor, sizeof(cmd->actor), "%s", who);
        snprintf(cmd->text, sizeof(cmd->text), "%s", p);
        cmd->op = SCOP_GIVE;
        return 1;
    }
    if (strncmp(s, "trade_custom ", 13) == 0) {

        char gtok[32] = {0}, rtok[32] = {0}, nick[NAME_LENGTH] = {0};
        int gsp, rsp;
        if (!AmberScript_ParseArg(s, 1, gtok, sizeof(gtok)) ||
            !AmberScript_ParseArg(s, 2, rtok, sizeof(rtok)))
            return 0;
        gsp = scene_resolve_species(gtok);
        rsp = scene_resolve_species(rtok);
        if (gsp <= 0 || rsp <= 0) {
            printf("[amberscript] trade_custom: bad species '%s' / '%s'\n", gtok, rtok);
            return 0;
        }

        AmberScript_ParseArg(s, 3, nick, sizeof(nick));
        cmd->op = SCOP_TRADE_CUSTOM;
        cmd->a = gsp;
        cmd->b = rsp;
        snprintf(cmd->text, sizeof(cmd->text), "%s", nick);
        return 1;
    }
    if (strncmp(s, "trade ", 6) == 0) {

        const char *p = s + 6;
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        snprintf(cmd->text, sizeof(cmd->text), "%s", p);
        cmd->op = SCOP_TRADE;
        return 1;
    }
    if (strncmp(s, "show_dex ", 9) == 0) {

        const char *p = s + 9;
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        snprintf(cmd->text, sizeof(cmd->text), "%s", p);
        cmd->op = SCOP_SHOW_DEX;
        return 1;
    }
    if (strcmp(s, "show_townmap") == 0) {

        cmd->op = SCOP_SHOW_TOWNMAP;
        return 1;
    }
    if (strcmp(s, "ship_depart") == 0) {

        cmd->op = SCOP_SHIP_DEPART;
        return 1;
    }
    if (strcmp(s, "show_blackboard") == 0) {

        cmd->op = SCOP_SHOW_BLACKBOARD;
        return 1;
    }
    if (strcmp(s, "show_link_cable_help") == 0) {

        cmd->op = SCOP_SHOW_LINK_CABLE_HELP;
        return 1;
    }
    if (strncmp(s, "show_fossil", 11) == 0) {

        const char *a = s + 11;
        while (*a == ' ') a++;
        cmd->op = SCOP_SHOW_FOSSIL;
        cmd->a = (a[0] == 'k' || a[0] == 'K') ? FOSSIL_KABUTOPS : FOSSIL_AERODACTYL;
        return 1;
    }
    if (strcmp(s, "show_dex_rating") == 0) {

        cmd->op = SCOP_SHOW_DEX_RATING;
        return 1;
    }
    if (strcmp(s, "bills_monster_list") == 0 ||
        strcmp(s, "bills_pokemon_list") == 0) {

        cmd->op = SCOP_BILLS_DEX_LIST;
        return 1;
    }
    if (strcmp(s, "diploma") == 0) {

        cmd->op = SCOP_DIPLOMA;
        return 1;
    }
    if (strcmp(s, "badge_house_menu") == 0) {

        cmd->op = SCOP_BADGE_HOUSE_MENU;
        return 1;
    }
    if (strcmp(s, "enter_safari_zone") == 0) {

        cmd->op = SCOP_ENTER_SAFARI;
        return 1;
    }
    if (strcmp(s, "leave_safari_zone") == 0) {

        cmd->op = SCOP_LEAVE_SAFARI;
        return 1;
    }
    if (strncmp(s, "gym_leader ", 11) == 0) {

        const char *p = s + 11;
        while (*p == ' ') p++;
        if (*p < '0' || *p > '9') return 0;
        cmd->op = SCOP_GYM_LEADER;
        cmd->a = atoi(p);
        return 1;
    }
    if (strcmp(s, "show_money") == 0) {

        cmd->op = SCOP_SHOW_MONEY;
        cmd->a = 0;
        return 1;
    }
    if (strcmp(s, "show_money on_yesno") == 0) {

        cmd->op = SCOP_SHOW_MONEY;
        cmd->a = 1;
        return 1;
    }
    if (strcmp(s, "hide_money") == 0) {

        cmd->op = SCOP_HIDE_MONEY;
        return 1;
    }
    if (strcmp(s, "refresh_npcs") == 0) {

        cmd->op = SCOP_REFRESH_NPCS;
        return 1;
    }
    if (strcmp(s, "refresh_tiles") == 0) {

        cmd->op = SCOP_REFRESH_TILES;
        return 1;
    }
    if (strncmp(s, "service ", 8) == 0) {

        const char *n = s + 8;
        while (*n == ' ' || *n == '\t') n++;
        if (!*n) return 0;
        cmd->op = SCOP_SERVICE;
        snprintf(cmd->text, sizeof(cmd->text), "%s", n);
        return 1;
    }
    if (strcmp(s, "show_coin_box") == 0) {

        cmd->op = SCOP_SHOW_COIN_BOX;
        return 1;
    }
    if (strcmp(s, "hide_coin_box") == 0) {

        cmd->op = SCOP_HIDE_COIN_BOX;
        return 1;
    }
    if (strncmp(s, "pay ", 4) == 0) {

        const char *p = s + 4;
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        snprintf(cmd->text, sizeof(cmd->text), "%s", p);
        cmd->op = SCOP_PAY;
        return 1;
    }
    if (strncmp(s, "take_coins ", 11) == 0) {

        const char *p = s + 11;
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        snprintf(cmd->text, sizeof(cmd->text), "%s", p);
        cmd->op = SCOP_TAKE_COINS;
        return 1;
    }
    if (strncmp(s, "give_coins ", 11) == 0) {

        const char *p = s + 11;
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        snprintf(cmd->text, sizeof(cmd->text), "%s", p);
        cmd->op = SCOP_GIVE_COINS;
        return 1;
    }
    if (strncmp(s, "name ", 5) == 0) {

        const char *p = s + 5;
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        snprintf(cmd->text, sizeof(cmd->text), "%s", p);
        cmd->op = SCOP_NAME;
        return 1;
    }
    if (strncmp(s, "sfx ", 4) == 0) {

        const char *p = s + 4;
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        snprintf(cmd->text, sizeof(cmd->text), "%s", p);
        cmd->op = SCOP_SFX;
        return 1;
    }
    if (strncmp(s, "cry ", 4) == 0) {

        const char *p = s + 4;
        while (*p == ' ') p++;
        int sid = pks_resolve_species_id(p);
        if (sid <= 0) return 0;
        cmd->op = SCOP_CRY;
        cmd->a = sid;
        return 1;
    }
    if (strncmp(s, "cry_on_print ", 13) == 0) {

        const char *p = s + 13;
        while (*p == ' ') p++;
        int sid = pks_resolve_species_id(p);
        if (sid <= 0) return 0;
        cmd->op = SCOP_CRY_ON_PRINT;
        cmd->a = sid;
        return 1;
    }
    if (strncmp(s, "sfx_on_close ", 13) == 0) {

        const char *p = s + 13;
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        snprintf(cmd->text, sizeof(cmd->text), "%s", p);
        cmd->op = SCOP_SFX_ON_CLOSE;
        return 1;
    }
    if (strncmp(s, "sfx_on_print ", 13) == 0) {

        const char *p = s + 13;
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        snprintf(cmd->text, sizeof(cmd->text), "%s", p);
        cmd->op = SCOP_SFX_ON_PRINT;
        return 1;
    }
    if (strncmp(s, "give_item ", 10) == 0) {

        const char *p = s + 10;
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        char item[32] = {0};
        int count = 1;
        sscanf(p, "%31s %d", item, &count);
        if (count < 1) count = 1;
        snprintf(cmd->text, sizeof(cmd->text), "%s", item);
        cmd->a = count;
        cmd->op = SCOP_GIVE_ITEM;
        return 1;
    }
    if (strncmp(s, "list_choice ", 12) == 0) {

        const char *p = s + 12;
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        char tmp[SCENE_TEXT_MAX];
        snprintf(tmp, sizeof(tmp), "%s", p);
        char *save = NULL;
        char *tok = strtok_r(tmp, " ", &save);
        int len = 0;
        cmd->text[0] = '\0';
        cmd->a = 0;
        while (tok) {
            if (strncmp(tok, "width=", 6) == 0) {
                cmd->a = (int)strtol(tok + 6, NULL, 0);
            } else if (len < (int)sizeof(cmd->text) - 1) {
                len += snprintf(cmd->text + len, sizeof(cmd->text) - (size_t)len,
                                "%s%s", len ? " " : "", tok);
            }
            tok = strtok_r(NULL, " ", &save);
        }
        if (cmd->text[0] == '\0') return 0;
        cmd->op = SCOP_LIST_CHOICE;
        return 1;
    }
    if (strncmp(s, "fossil_select ", 14) == 0) {

        const char *p = s + 14;
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        snprintf(cmd->text, sizeof(cmd->text), "%s", p);
        cmd->op = SCOP_FOSSIL_SELECT;
        return 1;
    }
    if (strcmp(s, "fossil_names") == 0) {

        cmd->op = SCOP_FOSSIL_NAMES;
        return 1;
    }
    if (strncmp(s, "prize_list ", 11) == 0) {

        const char *p = s + 11;
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        snprintf(cmd->text, sizeof(cmd->text), "%s", p);
        cmd->op = SCOP_PRIZE_LIST;
        return 1;
    }

    if (strncmp(s, "give_monster ", 13) == 0 ||
        strncmp(s, "give_pokemon ", 13) == 0) {

        const char *p = s + 13;
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        const char *sp_start = p;
        while (*p && *p != ' ') p++;
        if (p == sp_start) return 0;
        snprintf(cmd->actor, sizeof(cmd->actor), "%.*s", (int)(p - sp_start), sp_start);
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        snprintf(cmd->text, sizeof(cmd->text), "%s", p);
        cmd->op = SCOP_GIVE_POKEMON;
        return 1;
    }
    if (strncmp(s, "take_item ", 10) == 0) {

        const char *p = s + 10;
        while (*p == ' ') p++;
        if (*p == '\0') return 0;
        char item[32] = {0};
        int count = 1;
        sscanf(p, "%31s %d", item, &count);
        if (count < 1) count = 1;
        snprintf(cmd->text, sizeof(cmd->text), "%s", item);
        cmd->a = count;
        cmd->op = SCOP_TAKE_ITEM;
        return 1;
    }
    if (strncmp(s, "engage_trainer ", 15) == 0) {

        int ex, ey;
        if (sscanf(s + 15, "%d %d", &ex, &ey) != 2) return 0;
        cmd->a = ex; cmd->b = ey;
        cmd->op = SCOP_ENGAGE_TRAINER;
        return 1;
    }
    if (strncmp(s, "hide", 4) == 0 && (s[4] == '\0' || s[4] == ' ')) {

        const char *p = s + 4;
        while (*p == ' ') p++;
        int hx, hy;
        if (strncmp(p, "ball", 4) == 0) {
            cmd->a = -1; cmd->b = -1;
        } else if (sscanf(p, "%d %d", &hx, &hy) == 2) {
            cmd->a = hx; cmd->b = hy;
        } else {
            return 0;
        }
        cmd->op = SCOP_HIDE;
        return 1;
    }
    if (strncmp(s, "show ", 5) == 0) {

        int hx, hy;
        if (sscanf(s + 5, "%d %d", &hx, &hy) != 2) return 0;
        cmd->a = hx; cmd->b = hy;
        cmd->op = SCOP_SHOW;
        return 1;
    }
    if (strcmp(s, "heal") == 0) { cmd->op = SCOP_HEAL; return 1; }
    if (strncmp(s, "fade ", 5) == 0) {

        char dir[16] = {0}, color[16] = {0};
        int nn = sscanf(s + 5, "%15s %15s", dir, color);
        if (nn < 1) return 0;
        int is_black = color[0] && strcmp(color, "black") == 0;
        if (color[0] && !is_black && strcmp(color, "white") != 0) return 0;
        if (strcmp(dir, "out") == 0) { cmd->op = is_black ? SCOP_FADE_OUT_BLACK : SCOP_FADE_OUT_WHITE; return 1; }
        if (strcmp(dir, "in")  == 0) { cmd->op = is_black ? SCOP_FADE_IN_BLACK  : SCOP_FADE_IN_WHITE;  return 1; }
        return 0;
    }
    if (strcmp(s, "wait_music") == 0) { cmd->op = SCOP_WAIT_MUSIC; return 1; }
    if (strcmp(s, "march") == 0) { cmd->op = SCOP_MARCH; return 1; }
    if (strncmp(s, "sim_walk ", 9) == 0) {

        char dirname[16];
        int count = 1;
        int nn = sscanf(s + 9, "%15s %d", dirname, &count);
        if (nn < 1) return 0;
        int d = scene_parse_dir(dirname);
        if (d < 0) return 0;
        if (count < 1) count = 1;
        cmd->a = d;
        cmd->b = count;
        cmd->op = SCOP_SIM_WALK;
        return 1;
    }
    if (strncmp(s, "emote ", 6) == 0) {

        char who[24] = {0};
        if (sscanf(s + 6, "%23s", who) != 1) return 0;
        snprintf(cmd->actor, sizeof(cmd->actor), "%s", who);
        cmd->op = SCOP_EMOTE;
        return 1;
    }
    if (strncmp(s, "walk_to_x ", 10) == 0) {
        char id[24], xs[24];
        if (sscanf(s + 10, "%23s %23s", id, xs) != 2) return 0;
        snprintf(cmd->actor, sizeof(cmd->actor), "%s", id);
        cmd->a = (int)strtol(xs, NULL, 0);
        cmd->op = SCOP_WALK_TO_X;
        return 1;
    }
    if (strncmp(s, "set_event ", 10) == 0 || strncmp(s, "clear_event ", 12) == 0) {
        int is_set = (s[0] == 's');
        char tok[96];
        if (sscanf(s + (is_set ? 10 : 12), "%95s", tok) != 1) return 0;
        uint16_t evid;
        if (!pks_resolve_event_token(tok, &evid)) return 0;
        cmd->a = (int)evid;
        cmd->b = is_set ? 1 : 0;
        cmd->op = SCOP_SET_EVENT;
        return 1;
    }
    if (strncmp(s, "give_badge ", 11) == 0) {
        static const struct { const char *name; int bit; } kBadges[] = {
            { "BOULDER", BADGE_BOULDER }, { "CASCADE", BADGE_CASCADE },
            { "THUNDER", BADGE_THUNDER }, { "RAINBOW", BADGE_RAINBOW },
            { "SOUL",    BADGE_SOUL    }, { "MARSH",   BADGE_MARSH   },
            { "VOLCANO", BADGE_VOLCANO }, { "EARTH",   BADGE_EARTH   },
        };
        char tok[24] = {0};
        unsigned i;
        sscanf(s + 11, "%23s", tok);
        for (i = 0; i < sizeof kBadges / sizeof kBadges[0]; i++) {
            if (strcmp(tok, kBadges[i].name) != 0) continue;
            cmd->a  = kBadges[i].bit;
            cmd->op = SCOP_GIVE_BADGE;
            return 1;
        }

        printf("[amberscript] give_badge: unknown badge '%s'\n", tok);
        return 0;
    }
    return 0;
}

static int scene_parse_music_track(const char *tok) {
    static const struct { const char *name; int id; } k[] = {
        { "pallettown", MUSIC_PALLET_TOWN }, { "pokecenter", MUSIC_POKECENTER },
        { "gym", MUSIC_GYM }, { "cities1", MUSIC_CITIES1 }, { "cities2", MUSIC_CITIES2 },
        { "celadon", MUSIC_CELADON }, { "cinnabar", MUSIC_CINNABAR },
        { "vermilion", MUSIC_VERMILION }, { "lavender", MUSIC_LAVENDER },
        { "ssanne", MUSIC_SS_ANNE }, { "routes1", MUSIC_ROUTES1 },
        { "routes2", MUSIC_ROUTES2 }, { "routes3", MUSIC_ROUTES3 },
        { "routes4", MUSIC_ROUTES4 }, { "indigoplateau", MUSIC_INDIGO_PLATEAU },
        { "oakslab", MUSIC_OAKS_LAB }, { "dungeon1", MUSIC_DUNGEON1 },
        { "dungeon2", MUSIC_DUNGEON2 }, { "dungeon3", MUSIC_DUNGEON3 },
        { "pokemontower", MUSIC_POKEMON_TOWER }, { "silphco", MUSIC_SILPH_CO },
        { "safarizone", MUSIC_SAFARI_ZONE }, { "title", MUSIC_TITLE },
        { "jigglypuff", MUSIC_JIGGLYPUFF },
        { "wildbattle", MUSIC_WILD_BATTLE }, { "wild", MUSIC_WILD_BATTLE },
        { "defeatedwildmon", MUSIC_DEFEATED_WILD_MON },
        { "defeatedtrainer", MUSIC_DEFEATED_TRAINER },
        { "defeatedgymleader", MUSIC_DEFEATED_GYM_LEADER },
        { "pkmnhealed", MUSIC_PKMN_HEALED },
        { "gymleader", MUSIC_GYM_LEADER_BATTLE }, { "gymleaderbattle", MUSIC_GYM_LEADER_BATTLE },
        { "elitefour", MUSIC_GYM_LEADER_BATTLE }, { "elite4", MUSIC_GYM_LEADER_BATTLE },
        { "gymelitefour", MUSIC_GYM_LEADER_BATTLE },
        { "trainerbattle", MUSIC_TRAINER_BATTLE }, { "trainer", MUSIC_TRAINER_BATTLE },
        { "meetrival", MUSIC_MEET_RIVAL }, { "meetmaletrainer", MUSIC_MEET_MALE_TRAINER },
        { "meetfemaletrainer", MUSIC_MEET_FEMALE_TRAINER }, { "museumguy", MUSIC_MUSEUM_GUY },
        { "meeteviltrainer", MUSIC_MEET_EVIL_TRAINER }, { "meetevil", MUSIC_MEET_EVIL_TRAINER },
        { "surfing", MUSIC_SURFING }, { "surf", MUSIC_SURFING },
        { "meetprofoak", MUSIC_MEET_PROF_OAK }, { "meetoak", MUSIC_MEET_PROF_OAK },
        { "profoak", MUSIC_MEET_PROF_OAK }, { "oakappears", MUSIC_MEET_PROF_OAK },
        { "introbattle", MUSIC_INTRO_BATTLE }, { "gamecorner", MUSIC_GAME_CORNER },
        { "bikeriding", MUSIC_BIKE_RIDING }, { "bike", MUSIC_BIKE_RIDING },
        { "cinnabarmansion", MUSIC_CINNABAR_MANSION },
        { "champion", MUSIC_FINAL_BATTLE }, { "championbattle", MUSIC_FINAL_BATTLE },
        { "finalbattle", MUSIC_FINAL_BATTLE },
        { "halloffame", MUSIC_HALL_OF_FAME }, { "credits", MUSIC_CREDITS },
        { "none", MUSIC_NONE }, { "stop", MUSIC_NONE },
        { NULL, 0 }
    };
    char norm[48];
    if (!tok || !*tok) return -1;
    pks_norm(norm, sizeof(norm), tok);
    for (int i = 0; k[i].name; i++) if (strcmp(norm, k[i].name) == 0) return k[i].id;
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
    if (strncmp(s, "place ", 6) == 0) return 1;
    if (strncmp(s, "despawn ", 8) == 0) return 1;
    if (strncmp(s, "face ", 5) == 0) return 1;
    if (strncmp(s, "move ", 5) == 0) return 1;
    if (strncmp(s, "move_to_player ", 15) == 0) return 1;
    if (strncmp(s, "say ", 4) == 0) return 1;
    if (strncmp(s, "say_auto ", 9) == 0) return 1;
    if (strncmp(s, "ask ", 4) == 0) return 1;
    if (strncmp(s, "battlestart", 11) == 0) return 1;
    if (strncmp(s, "battlend", 8) == 0) return 1;
    if (strncmp(s, "hide", 4) == 0 && (s[4] == '\0' || s[4] == ' ')) return 1;
    if (strncmp(s, "show ", 5) == 0) return 1;
    if (strncmp(s, "music ", 6) == 0) return 1;
    if (strncmp(s, "music_from_loop ", 16) == 0) return 1;
    if (strncmp(s, "music_rival_alt ", 16) == 0) return 1;
    if (strncmp(s, "wait ", 5) == 0) return 1;
    if (strcmp(s, "wait_text") == 0) return 1;
    if (strcmp(s, "wait_sfx") == 0) return 1;
    if (strcmp(s, "wait_cry") == 0) return 1;
    if (strcmp(s, "lock_input on") == 0) return 1;
    if (strcmp(s, "lock_input off") == 0) return 1;
    if (strcmp(s, "fullrate on") == 0) return 1;
    if (strcmp(s, "fullrate off") == 0) return 1;
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
    if (strncmp(s, "tile_art_load ", 14) == 0) return 1;
    if (strcmp(s, "end") == 0) return 1;
    if (strncmp(s, "use ", 4) == 0) return 1;
    if (strncmp(s, "include ", 8) == 0) return 1;
    if (strncmp(s, "def ", 4) == 0) return 1;
    if (strcmp(s, "enddef") == 0) return 1;

    if (strncmp(s, "if ", 3) == 0) return 1;
    if (strncmp(s, "elif ", 5) == 0) return 1;
    if (strcmp(s, "else") == 0) return 1;
    if (strcmp(s, "stop") == 0) return 1;
    if (strncmp(s, "let ", 4) == 0) return 1;
    if (strncmp(s, "give ", 5) == 0) return 1;
    if (strncmp(s, "queue ", 6) == 0) return 1;
    if (strcmp(s, "march") == 0) return 1;
    if (strncmp(s, "walk_to_x ", 10) == 0) return 1;
    if (strncmp(s, "set_event ", 10) == 0) return 1;
    if (strncmp(s, "clear_event ", 12) == 0) return 1;
    if (strncmp(s, "show_dex ", 9) == 0) return 1;
    if (strncmp(s, "trade ", 6) == 0) return 1;
    if (strncmp(s, "trade_custom ", 13) == 0) return 1;
    if (strncmp(s, "pay ", 4) == 0) return 1;
    if (strncmp(s, "name ", 5) == 0) return 1;
    if (strncmp(s, "sfx ", 4) == 0) return 1;
    if (strncmp(s, "cry ", 4) == 0) return 1;
    if (strncmp(s, "cry_on_print ", 13) == 0) return 1;
    if (strncmp(s, "sfx_on_close ", 13) == 0) return 1;
    if (strncmp(s, "sfx_on_print ", 13) == 0) return 1;
    if (strncmp(s, "give_item ", 10) == 0) return 1;
    if (strncmp(s, "give_monster ", 13) == 0) return 1;
    if (strncmp(s, "give_pokemon ", 13) == 0) return 1;
    if (strncmp(s, "take_item ", 10) == 0) return 1;
    if (strcmp(s, "heal") == 0) return 1;
    if (strncmp(s, "fade ", 5) == 0) return 1;
    if (strcmp(s, "wait_music") == 0) return 1;
    return 0;
}

typedef struct { const char *sym; const char *text; } pks_rom_text_t;
static pks_rom_text_t *s_rom_text;
static char *s_rom_text_blob;
static int s_rom_text_n, s_rom_text_tried;

static void pks_rom_text_load(void) {

    char kPaths[2][160];
    snprintf(kPaths[0], sizeof(kPaths[0]),
             "mod_runtime/generatedmaps/%s/scene_text.tbl", GameVersion_Current());
    snprintf(kPaths[1], sizeof(kPaths[1]),
             "../mod_runtime/generatedmaps/%s/scene_text.tbl", GameVersion_Current());
    FILE *f = NULL;
    long len;
    int cap = 0;

    s_rom_text_tried = 1;
    for (size_t i = 0; i < sizeof kPaths / sizeof kPaths[0] && !f; i++)
        f = fopen(kPaths[i], "rb");
    if (!f) {
        printf("[amberscript] scene_text.tbl not found -- `say rom:` will fail. "
               "Run: tools/romimport/emit_scene_text.py\n");
        return;
    }
    fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return; }
    s_rom_text_blob = (char *)malloc((size_t)len + 1);
    if (!s_rom_text_blob) { fclose(f); return; }
    len = (long)fread(s_rom_text_blob, 1, (size_t)len, f);
    s_rom_text_blob[len] = '\0';
    fclose(f);

    for (char *p = s_rom_text_blob; *p; ) {
        char *nl = strchr(p, '\n');
        char *tab = strchr(p, '\t');
        if (nl) *nl = '\0';
        if (*p != '#' && tab && (!nl || tab < nl)) {
            if (s_rom_text_n == cap) {
                int nc = cap ? cap * 2 : 512;
                pks_rom_text_t *g = (pks_rom_text_t *)realloc(
                    s_rom_text, (size_t)nc * sizeof *g);
                if (!g) break;
                s_rom_text = g; cap = nc;
            }
            *tab = '\0';
            s_rom_text[s_rom_text_n].sym = p;

            {
                size_t tl = strlen(tab + 1);
                if (tl && (tab + 1)[tl - 1] == '\r') (tab + 1)[tl - 1] = '\0';
            }
            s_rom_text[s_rom_text_n].text = tab + 1;
            s_rom_text_n++;
        }
        if (!nl) break;
        p = nl + 1;
    }
    printf("[amberscript] scene_text.tbl: %d ROM strings\n", s_rom_text_n);
}

static void pks_rom_text_page_range(const char *full, int start, int end,
                                     char *out, size_t out_sz) {
    const char *p = full;
    for (int i = 0; i < start; i++) {
        const char *f = strstr(p, "\\f");
        if (!f) { out[0] = '\0'; return; }
        p = f + 2;
    }
    const char *stop = NULL;
    const char *q = p;
    for (int i = 0; i <= end - start; i++) {
        const char *f = strstr(q, "\\f");
        if (!f) { stop = NULL; break; }
        stop = f;
        q = f + 2;
    }
    size_t len = stop ? (size_t)(stop - p) : strlen(p);
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out, p, len);
    out[len] = '\0';
}

static void pks_rom_text_slice(const char *full, int at, int len,
                                char *out, size_t out_sz) {
    size_t flen = strlen(full);
    if (at < 0 || (size_t)at > flen) { out[0] = '\0'; return; }
    size_t n = (len < 0) ? flen - (size_t)at : (size_t)len;
    if (n >= out_sz) n = out_sz - 1;
    if ((size_t)at + n > flen) n = flen - (size_t)at;
    memcpy(out, full + at, n);
    out[n] = '\0';
}

static int pks_scene_resolve_rom_text(char *buf, size_t sz) {
    char sym[96], args[192];
    const char *sp;
    if (strncmp(buf, "rom:", 4) != 0) return 0;
    if (!s_rom_text_tried) pks_rom_text_load();

    sp = strchr(buf + 4, ' ');
    if (sp) {
        size_t n = (size_t)(sp - (buf + 4));
        if (n >= sizeof sym) n = sizeof sym - 1;
        memcpy(sym, buf + 4, n);
        sym[n] = '\0';
        snprintf(args, sizeof args, "%s", sp + 1);
    } else {
        snprintf(sym, sizeof sym, "%s", buf + 4);
        args[0] = '\0';
    }

    int page_start = -1, page_end = -1, at_off = -1, at_len = -1;
    {
        const char *p;
        if ((p = strstr(args, "page=")) != NULL) {
            int a, b;
            if (sscanf(p + 5, "%d-%d", &a, &b) == 2) { page_start = a; page_end = b; }
            else if (sscanf(p + 5, "%d", &a) == 1) { page_start = a; page_end = a; }
        }
        if ((p = strstr(args, "at=")) != NULL) sscanf(p + 3, "%d", &at_off);
        if ((p = strstr(args, "len=")) != NULL) sscanf(p + 4, "%d", &at_len);
    }

    for (int i = 0; i < s_rom_text_n; i++) {
        if (strcmp(s_rom_text[i].sym, sym) == 0) {
            if (page_start >= 0)
                pks_rom_text_page_range(s_rom_text[i].text, page_start, page_end, buf, sz);
            else if (at_off >= 0)
                pks_rom_text_slice(s_rom_text[i].text, at_off, at_len, buf, sz);
            else
                snprintf(buf, sz, "%s", s_rom_text[i].text);

            for (char *p = args; *p; ) {
                while (*p == ' ') p++;
                if (!*p) break;
                char *eq = strchr(p, '=');
                if (!eq) { while (*p && *p != ' ') p++; continue; }
                char key[64], val[128], *at;
                size_t klen0 = (size_t)(eq - p);
                if (klen0 >= sizeof key) klen0 = sizeof key - 1;
                memcpy(key, p, klen0); key[klen0] = '\0';
                char *vs = eq + 1;
                char *ve;
                if (*vs == '"') {
                    vs++;
                    ve = strchr(vs, '"');
                    p = ve ? ve + 1 : vs + strlen(vs);
                    if (!ve) ve = vs + strlen(vs);
                } else {
                    ve = strchr(vs, ' ');
                    if (!ve) ve = vs + strlen(vs);
                    p = ve;
                }
                size_t vlen0 = (size_t)(ve - vs);
                if (vlen0 >= sizeof val) vlen0 = sizeof val - 1;
                memcpy(val, vs, vlen0); val[vlen0] = '\0';

                if (!strcmp(val, "$prize")) {
                    int pc2 = s_scene_last_prize_choice;
                    uint8_t sp2 = (pc2 >= 1 && pc2 <= 3) ? s_prize_entries[pc2 - 1] : 0;
                    const char *nm2 = sp2 ? Pokemon_GetName(Species_Dex(sp2)) : NULL;
                    snprintf(val, sizeof val, "%s", nm2 ? nm2 : "");
                }

                char slot[66];
                snprintf(slot, sizeof slot, "{%s}", key);
                while ((at = strstr(buf, slot)) != NULL) {
                    size_t klen = strlen(slot), vlen = strlen(val);
                    size_t tail = strlen(at + klen);
                    if (strlen(buf) - klen + vlen >= sz) break;
                    memmove(at + vlen, at + klen, tail + 1);
                    memcpy(at, val, vlen);
                }
            }
            return 1;
        }
    }
    printf("[amberscript] say rom:%s -- no such symbol in scene_text.tbl\n",
           buf + 4);
    snprintf(buf, sz, "%s", "");
    return 1;
}

static void scene_unescape_text(char *s) {
    char out[SCENE_TEXT_MAX];
    int oi = 0;
    for (int i = 0; s[i] && oi + 1 < (int)sizeof(out); i++) {
        if (s[i] == '\\' && s[i + 1]) {
            char ch = s[i + 1];
            if (ch == 'n') { out[oi++] = '\n'; i++; continue; }
            if (ch == 'f') { out[oi++] = '\f'; i++; continue; }

            if (ch == 'c') { out[oi++] = TEXT_ASCII_CONT; i++; continue; }
            if (ch == 't') { out[oi++] = '\t'; i++; continue; }
            if (ch == '\\') { out[oi++] = '\\'; i++; continue; }
            if (ch == '"') { out[oi++] = '"'; i++; continue; }
        }

        if (s[i] == '#' && oi + 4 < (int)sizeof(out)) {
            out[oi++] = 'P'; out[oi++] = 'O'; out[oi++] = 'K';
            out[oi++] = (char)0xE9;
            continue;
        }
        if ((unsigned char)s[i] == 0xC3 && (unsigned char)s[i + 1] == 0xA9) {
            out[oi++] = (char)0xE9;
            i++;
            continue;
        }
        out[oi++] = s[i];
    }
    out[oi] = '\0';
    snprintf(s, SCENE_TEXT_MAX, "%s", out);
}

static void scene_format_dialog_text(char *s) {
    enum { MAX_COLS = 18, MAX_LINES = 2 };
    char out[SCENE_TEXT_MAX];
    int oi = 0;
    int i = 0;
    int col = 0;
    int line = 0;
    int ended = 0;

    while (s[i] && oi + 1 < (int)sizeof(out)) {
        char c = s[i];
        if (c == '@') { out[oi++] = '@'; ended = 1; break; }
        if (c == '\f') { out[oi++] = '\f'; col = 0; line = 0; i++; continue; }

        if (c == TEXT_ASCII_CONT) {
            out[oi++] = TEXT_ASCII_CONT; col = 0; line = 0; i++; continue;
        }
        if (c == '\n') {
            out[oi++] = '\n'; col = 0; line++;
            if (line >= MAX_LINES) line = MAX_LINES - 1;
            i++; continue;
        }
        if (c == ' ' || c == '\t' || c == '\r') { i++; continue; }

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
            if (wlen <= 0) { i = we; continue; }

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
                for (int k = 0; k < wlen && oi + 1 < (int)sizeof(out); k++) out[oi++] = s[ws + k];
                col = wlen;
            } else if (col + 1 + wlen <= MAX_COLS) {
                out[oi++] = ' ';
                for (int k = 0; k < wlen && oi + 1 < (int)sizeof(out); k++) out[oi++] = s[ws + k];
                col += 1 + wlen;
            } else {
                if (line == 0) { out[oi++] = '\n'; line = 1; }
                else { out[oi++] = '\f'; line = 0; }
                col = 0;
                for (int k = 0; k < wlen && oi + 1 < (int)sizeof(out); k++) out[oi++] = s[ws + k];
                col = wlen;
            }
            i = we;
        }
    }
    if (!ended && oi + 1 < (int)sizeof(out)) out[oi++] = '@';
    out[oi] = '\0';
    snprintf(s, SCENE_TEXT_MAX, "%s", out);
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
            int sid = pks_resolve_species_id(a);
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
        int sid = pks_resolve_species_id(a);
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
            int sid = pks_resolve_species_id(a);
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
        sid = pks_resolve_species_id(a);
        if (sid <= 0 || sid > 255) return;
        m1 = pks_resolve_move_id(m1s); m2 = pks_resolve_move_id(m2s); m3 = pks_resolve_move_id(m3s); m4 = pks_resolve_move_id(m4s);
        if (m1 < 0 || m1 > 255 || m2 < 0 || m2 > 255 || m3 < 0 || m3 > 255 || m4 < 0 || m4 > 255) return;
        SpeciesMod_SetStartMoves((uint8_t)sid, (uint8_t)m1, (uint8_t)m2, (uint8_t)m3, (uint8_t)m4);
        return;
    }
    {
        unsigned lv = 0;
        char ms[32] = {0};
        if (sscanf(line, "species_learn_add %31s %u %31s", a, &lv, ms) == 3) {
            int sid = pks_resolve_species_id(a);
            int mid = pks_resolve_move_id(ms);
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

static void scene_persist_hidden(int npc_idx, int hidden) {
    int decl = NPC_GetDeclIdx(npc_idx);
    int kx = 0, ky = 0;
    if (decl < 0) return;
    if (!AmberScript_GetNpcDeclaredPos(wCurMap, decl, &kx, &ky)) return;
    AmberScript_MapNpcSaveRuntime((int)wCurMap, kx, ky, 0, 0, 0, hidden, 0);
}

static void scene_persist_keyed_actors(int real_id) {
    for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
        if (s_scene_actors[i].used && s_scene_actors[i].has_key &&
            s_scene_actors[i].npc_idx >= 0) {
            int tx = 0, ty = 0;
            NPC_GetTilePos(s_scene_actors[i].npc_idx, &tx, &ty);
            int fac = NPC_GetFacing(s_scene_actors[i].npc_idx);

            int hid = NPC_IsHidden(s_scene_actors[i].npc_idx) ? 1 : 0;
            AmberScript_MapNpcSaveRuntime(real_id, s_scene_actors[i].key_x,
                                         s_scene_actors[i].key_y, tx, ty, fac, hid, 1);
        }
    }
}

static void scene_reset_runtime(void) {
    if (s_scene_frozen_npc >= 0) {

        if (s_scene_frozen_map == Map_CurrentRealId() &&
            s_scene_frozen_npc < NPC_GetCount() &&
            (int)NPC_GetSpriteId(s_scene_frozen_npc) == s_scene_frozen_sprite) {
            NPC_SetMoveType(s_scene_frozen_npc, s_scene_frozen_npc_movetype);
        }
        s_scene_frozen_npc = -1;
        s_scene_frozen_npc_movetype = -1;
        s_scene_frozen_map = -1;
        s_scene_frozen_sprite = -1;
    }
    s_scene_active = 0;
    s_scene_cmd_count = 0;
    s_scene_pc = 0;
    s_scene_wait = 0;
    s_scene_wait_yesno = 0;
    s_scene_wait_priced_choice = 0;
    s_scene_wait_say = 0;
    s_scene_say_opened = 0;
    s_scene_say_auto = 0;
    s_scene_wait_print = 0;
    s_scene_print_opened = 0;
    s_scene_wait_battle = 0;
    s_scene_wait_engage_pretext = 0;
    s_scene_wait_battleend_text = 0;
    s_scene_input_locked = 0;
    s_scene_wait_dex = 0;
    s_scene_wait_dex_delay = 0;
    if (s_scene_wait_emote) { s_scene_wait_emote = 0; Emote_Hide(); }
    s_scene_emote_npc = -1;
    s_scene_wait_townmap = 0;
    s_scene_wait_ship_depart = 0;
    s_scene_wait_blackboard = 0;
    s_scene_wait_fossil = 0;

    s_scene_wait_trade = 0;
    Trade_Abort();
    s_scene_wait_bills_dex_list = 0;
    s_scene_wait_badge_house = 0;
    s_scene_wait_link_cable_help = 0;
    s_scene_wait_list_choice = 0;
    s_scene_last_list_choice = 0;

    BagListChoice_ClearHeld();
    s_scene_wait_prize_list = 0;
    s_scene_last_prize_choice = 0;
    s_scene_wait_name = 0;
    s_scene_fade_active = 0;
    s_scene_fade_step = 0;
    s_scene_fade_timer = 0;
    s_scene_battle_started = 0;
    s_scene_battlestart_pending = 0;
    s_scene_battlestart_saw_text = 0;
    s_scene_battlestart_delay = 0;
    s_scene_battlestart_tc = 0;
    s_scene_battlestart_tn = 0;
    s_scene_battlestart_noblackout = 0;
    s_scene_last_yesno = -1;
    s_scene_last_give_full = 0;
    s_scene_last_give_mon_full = 0;
    s_scene_last_give_mon_boxed = 0;
    s_scene_fullrate = 0;
    s_scene_yesno_prev_joyignore = 0;
    s_scene_yesno_restore_joyignore = 0;
    s_scene_yesno_prev_scripted_movement = 0;
    s_scene_yesno_restore_scripted_movement = 0;
    s_scene_move_steps_left = 0;
    s_scene_move_dir = 0;
    s_scene_move_actor = -1;
    s_scene_move_awaiting_stop = 0;

    s_scene_sim_walk_active = 0;

    gScriptedMovement = 0;
    scene_march_clear();
    s_scene_wtx_active = 0;
    s_scene_wtx_is_player = 0;
    s_scene_wtx_actor = -1;
    s_scene_wtx_target = 0;
    s_scene_var_count = 0;
    s_scene_ctx_npc = -1;

    s_scene_map = wCurMap;

    scene_persist_keyed_actors(s_scene_map);
    for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
        if (s_scene_actors[i].used && s_scene_actors[i].spawned_by_scene &&
            !s_scene_actors[i].persist &&
            s_scene_actors[i].npc_idx >= 0) {
            NPC_DebugDespawn(s_scene_actors[i].npc_idx);
        }
    }
    memset(s_scene_actors, 0, sizeof(s_scene_actors));
}

int AmberScript_SceneShowingDex(void) { return s_scene_wait_dex; }

int AmberScript_SceneShowingBillsDexList(void) { return s_scene_wait_bills_dex_list; }

int AmberScript_SceneShowingBadgeHouseMenu(void) { return s_scene_wait_badge_house; }
int AmberScript_SceneShowingDiploma(void) { return s_scene_wait_diploma; }

static int scene_find_actor(const char *name) {
    for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
        if (s_scene_actors[i].used && strcmp(s_scene_actors[i].name, name) == 0)
            return i;
    }
    return -1;
}

static void scene_march_dbg_snapshot(const char *tag) {
    if (s_march_dbg_frame < 0) return;
    char line[512];
    int off = snprintf(line, sizeof(line), "[march-dbg] f=%03d %s:", s_march_dbg_frame, tag);
    for (int i = 0; i < SCENE_ACTOR_MAX + 1 && off < (int)sizeof(line); i++) {
        if (!s_march[i].used) continue;
        int x, y, busy;
        if (s_march[i].is_player) {
            x = (int)wXCoord; y = (int)wYCoord; busy = Player_IsMoving();
        } else {
            int ai = scene_find_actor(s_march[i].actor);
            if (ai < 0) continue;
            NPC_GetTilePos(s_scene_actors[ai].npc_idx, &x, &y);
            busy = NPC_IsWalking(s_scene_actors[ai].npc_idx);
        }
        off += snprintf(line + off, sizeof(line) - off, " %s=(%d,%d)%s[%d/%d]",
                         s_march[i].actor, x, y, busy ? "*" : "", s_march[i].idx, s_march[i].len);
    }
    march_dbg_log("%s\n", line);
}

int AmberScript_GetMarchActorLabelForNpcIdx(int npc_idx, char *out_ch) {
    for (int i = 0; i < SCENE_ACTOR_MAX + 1; i++) {
        if (!s_march[i].used || s_march[i].is_player) continue;
        int ai = scene_find_actor(s_march[i].actor);
        if (ai < 0 || s_scene_actors[ai].npc_idx != npc_idx) continue;
        char c = s_march[i].actor[0];
        if (c >= 'a' && c <= 'z') c -= ('a' - 'A');
        if (out_ch) *out_ch = c;
        return 1;
    }
    return 0;
}

static void scene_march_anim_frame(const char *tag) {
    if (!s_march_dbg_enabled || s_march_dbg_frame < 0) return;
    char header[512];
    int off = snprintf(header, sizeof(header), "f=%03d %s:", s_march_dbg_frame, tag);
    for (int i = 0; i < SCENE_ACTOR_MAX + 1 && off < (int)sizeof(header); i++) {
        if (!s_march[i].used) continue;
        int x, y, busy;
        if (s_march[i].is_player) {
            x = (int)wXCoord; y = (int)wYCoord; busy = Player_IsMoving();
        } else {
            int ai = scene_find_actor(s_march[i].actor);
            if (ai < 0) continue;
            NPC_GetTilePos(s_scene_actors[ai].npc_idx, &x, &y);
            busy = NPC_IsWalking(s_scene_actors[ai].npc_idx);
        }
        off += snprintf(header + off, sizeof(header) - off, " %s=(%d,%d)%s[%d/%d]",
                         s_march[i].actor, x, y, busy ? "*" : "", s_march[i].idx, s_march[i].len);
    }
    DebugCLI_WriteMarchAnimFrame(header);
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

static scene_march_t *scene_march_get(const char *name) {
    for (int i = 0; i < SCENE_ACTOR_MAX + 1; i++)
        if (s_march[i].used && strcmp(s_march[i].actor, name) == 0) return &s_march[i];
    for (int i = 0; i < SCENE_ACTOR_MAX + 1; i++)
        if (!s_march[i].used) {
            memset(&s_march[i], 0, sizeof(s_march[i]));
            s_march[i].used = 1;
            snprintf(s_march[i].actor, sizeof(s_march[i].actor), "%s", name);
            s_march[i].is_player = (strcmp(name, "player") == 0);
            return &s_march[i];
        }
    return NULL;
}

static void scene_march_clear(void) {
    memset(s_march, 0, sizeof(s_march));
    s_scene_march_active = 0;

    gScriptedMovement = 0;
}

static int scene_march_busy(const scene_march_t *q) {
    if (q->is_player) return Player_IsMoving();
    int ai = scene_find_actor(q->actor);
    if (ai < 0) return 0;
    return NPC_IsWalking(s_scene_actors[ai].npc_idx);
}

static void scene_march_dispatch(scene_march_t *q) {
    if (q->idx >= q->len) return;
    int e = q->steps[q->idx++];
    if (e == SCENE_MARCH_PAUSE) return;
    if (q->is_player) {

        if (e < SCENE_MARCH_FACE_BASE) Player_DoScriptedStepWithLedge(e);
        else gPlayerFacing = e - SCENE_MARCH_FACE_BASE;
        return;
    }
    int ai = scene_find_actor(q->actor);
    if (ai < 0) return;
    int npc = s_scene_actors[ai].npc_idx;
    if (e < SCENE_MARCH_FACE_BASE) NPC_DoScriptedStep(npc, e);
    else NPC_SetFacing(npc, e - SCENE_MARCH_FACE_BASE);
}

static int scene_parse_runtime_line(const char *line, scene_cmd_t *out_cmd) {
    scene_cmd_t cmd;
    const char *s = line;
    if (!line || !out_cmd) return 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0' || *s == '#') return 0;
    memset(&cmd, 0, sizeof(cmd));

    if (strncmp(s, "spawn ", 6) == 0) {
        char id[24], sprite[32], x[32], y[32], extra[16] = {0};
        int nf = sscanf(s + 6, "%23s %31s %31s %31s %15s", id, sprite, x, y, extra);
        if (nf < 4) return 0;
        cmd.op = SCOP_SPAWN;
        snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
        cmd.a = pks_parse_sprite(sprite);
        cmd.b = pks_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
        cmd.c = pks_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
        if (!pks_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) cmd.b = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
        if (!pks_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) cmd.c = (int)wYCoord + (int)strtol(y + 7, NULL, 0);

        cmd.d = (nf >= 5 && strcmp(extra, "keep") == 0) ? 1 : 0;
    } else if (strncmp(s, "place ", 6) == 0) {
        char id[24], x[32], y[32];
        if (sscanf(s + 6, "%23s %31s %31s", id, x, y) != 3) return 0;
        cmd.op = SCOP_PLACE;
        snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
        cmd.a = pks_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
        cmd.b = pks_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
        if (!pks_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) cmd.a = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
        if (!pks_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) cmd.b = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
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
    } else if (strncmp(s, "say ", 4) == 0 || strncmp(s, "say_auto ", 9) == 0) {

        int say_auto = (s[3] == '_');
        cmd.op = SCOP_SAY;
        cmd.a = say_auto ? 1 : 0;
        snprintf(cmd.text, sizeof(cmd.text), "%s", s + (say_auto ? 9 : 4));
        {
            size_t n = strlen(cmd.text);
            if (n >= 2 && cmd.text[0] == '"' && cmd.text[n - 1] == '"') {
                memmove(cmd.text, cmd.text + 1, n - 2);
                cmd.text[n - 2] = '\0';
            }
        }

        pks_scene_resolve_rom_text(cmd.text, sizeof(cmd.text));
        scene_unescape_text(cmd.text);
        scene_format_dialog_text(cmd.text);
    } else if (strncmp(s, "say_hold ", 9) == 0) {
        cmd.op = SCOP_SAY_HOLD;
        snprintf(cmd.text, sizeof(cmd.text), "%s", s + 9);
        {
            size_t n = strlen(cmd.text);
            if (n >= 2 && cmd.text[0] == '"' && cmd.text[n - 1] == '"') {
                memmove(cmd.text, cmd.text + 1, n - 2);
                cmd.text[n - 2] = '\0';
            }
        }

        pks_scene_resolve_rom_text(cmd.text, sizeof(cmd.text));
        scene_unescape_text(cmd.text);
        scene_format_dialog_text(cmd.text);
    } else if (strcmp(s, "close_text") == 0) {
        cmd.op = SCOP_CLOSE_TEXT;
    } else if (strcmp(s, "wait_text") == 0) {
        cmd.op = SCOP_WAIT_TEXT;
    } else if (strcmp(s, "wait_sfx") == 0) {
        cmd.op = SCOP_WAIT_SFX;
    } else if (strcmp(s, "wait_cry") == 0) {
        cmd.op = SCOP_WAIT_CRY;
    } else if (strcmp(s, "lock_input on") == 0) {
        cmd.op = SCOP_LOCK_INPUT; cmd.a = 1;
    } else if (strcmp(s, "lock_input off") == 0) {
        cmd.op = SCOP_LOCK_INPUT; cmd.a = 0;
    } else if (strcmp(s, "fullrate on") == 0) {
        cmd.op = SCOP_FULLRATE; cmd.a = 1;
    } else if (strcmp(s, "fullrate off") == 0) {
        cmd.op = SCOP_FULLRATE; cmd.a = 0;
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
        cmd.a = pks_parse_sprite(sprite);
        cmd.b = pks_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
        cmd.c = pks_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
        if (!pks_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) cmd.b = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
        if (!pks_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) cmd.c = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
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
        sid = pks_resolve_species_id(sp);
        if (sid <= 0) return 0;
        cmd.op = SCOP_SPRITE_FRONT_LOAD;
        cmd.a = sid;
        snprintf(cmd.text, sizeof(cmd.text), "%s", path);
    } else if (strncmp(s, "sprite_back_load ", 17) == 0) {
        char sp[32], path[140];
        int sid;
        if (sscanf(s + 17, "%31s %139s", sp, path) != 2) return 0;
        sid = pks_resolve_species_id(sp);
        if (sid <= 0) return 0;
        cmd.op = SCOP_SPRITE_BACK_LOAD;
        cmd.a = sid;
        snprintf(cmd.text, sizeof(cmd.text), "%s", path);
    } else if (strncmp(s, "tile_art_load ", 14) == 0) {
        char nm[24], path[140];
        if (sscanf(s + 14, "%23s %139s", nm, path) != 2) return 0;
        cmd.op = SCOP_TILE_ART_LOAD;
        snprintf(cmd.actor, sizeof(cmd.actor), "%s", nm);
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

    for (int i = 0; i < s_scene_cmd_count; i++) {
        if ((s_scene_cmds[i].op == SCOP_IF || s_scene_cmds[i].op == SCOP_JUMP) &&
            s_scene_cmds[i].a >= insert_at) {
            s_scene_cmds[i].a++;
        }
    }
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

    {
        FILE *rprobe = fopen("C:/Progra~1/Python311/python.exe", "r");
        if (rprobe) {
            fclose(rprobe);
            snprintf(runner, sizeof(runner), "%s", "C:/Progra~1/Python311/python.exe");
        }
    }
    if (!runner[0]) {
        FILE *wf = PKS_POPEN("where python 2>nul", "r");
        if (wf) {
            if (fgets(out_line, sizeof(out_line), wf)) {
                char *s = pks_trim(out_line);
                if (*s) snprintf(runner, sizeof(runner), "%s", s);
            }
            PKS_PCLOSE(wf);
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
        int rc;
        snprintf(cmdline, sizeof(cmdline),
                 "%s %s %u %d %d %u 2>&1",
                 runner, script_path,
                 (unsigned)wCurMap, (int)wXCoord, (int)wYCoord, (unsigned)wPlayerDirection);
        printf("[scene] py_inject run: %s\n", cmdline);
        fp = PKS_POPEN(cmdline, "r");
        if (!fp) return 0;
        launch_ok = 1;
        while (fgets(out_line, sizeof(out_line), fp)) {
            scene_cmd_t icmd;
            char *s = pks_trim(out_line);
            pks_normalize_ascii(s);
            if (scene_parse_runtime_line(s, &icmd)) {
                if (parsed_count < SCENE_CMD_MAX) {
                    parsed[parsed_count++] = icmd;
                }
            } else if (*s) {
                printf("[scene] py_inject out: %s\n", s);
            }
        }
        rc = PKS_PCLOSE(fp);
        fp = NULL;
        if (parsed_count > 0) {
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

        if (s_scene_actors[i].npc_idx >= 0)
            NPC_DebugDespawn(s_scene_actors[i].npc_idx);
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
    if (!cmd) return;
    wJoyIgnore = 0;
    gScriptedMovement = 0;

    if (cmd->text[0] != '\0')
        Trainer_SetDefeatText((int)cmd->a, cmd->text);
    else
        gTrainerAfterText = NULL;
    Game_StartCustomTrainerBattleScripted((uint8_t)cmd->a, (uint8_t)cmd->b, cmd->team_species, cmd->team_level, cmd->team_moves, cmd->team_count);
}

static int scene_pick_random_map_trainer(int *out_class, int *out_no) {
    const map_events_t *ev;
    int idx;
    if (!out_class || !out_no) return 0;
    if (wCurMap >= PKS_VIRTUAL_MAP_FIRST) return 0;
    ev = &gMapEvents[wCurMap];
    if (!ev->trainers || ev->num_trainers == 0) return 0;
    idx = rand() % ev->num_trainers;
    *out_class = ev->trainers[idx].trainer_class;
    *out_no = ev->trainers[idx].trainer_no;
    return 1;
}

static void scene_insert_move_to_player_moves(const char *actor_name, int ax, int ay) {
    int px = (int)wXCoord, py = (int)wYCoord;
    int ydist = abs(ay - py);
    int xdist = abs(ax - px);
    int ydir = (py < ay) ? 1  : 0 ;
    int xdir = (px < ax) ? 2  : 3 ;
    scene_cmd_t moves[16];
    int nmoves = 0;
    int yprog, xprog;

    if (ydist > 0) ydist--;
    else if (xdist > 0) xdist--;
    yprog = 0;
    xprog = 0;
    while ((yprog < ydist || xprog < xdist) && nmoves < (int)(sizeof(moves) / sizeof(moves[0]))) {
        int yrem = ydist - yprog;
        int xrem = xdist - xprog;
        int dir;
        if (xrem >= yrem && xprog < xdist) {
            dir = xdir;
            xprog++;
        } else {
            dir = ydir;
            yprog++;
        }
        if (nmoves > 0 && moves[nmoves - 1].a == dir) {
            moves[nmoves - 1].b++;
        } else {
            scene_cmd_t cmd = {0};
            cmd.op = SCOP_MOVE;
            snprintf(cmd.actor, sizeof(cmd.actor), "%s", actor_name);
            cmd.a = dir;
            cmd.b = 1;
            moves[nmoves++] = cmd;
        }
    }
    for (int i = nmoves - 1; i >= 0; i--)
        scene_inject_cmd_after_pc(&moves[i]);
    printf("[amberscript] move_to_player '%s': actor=(%d,%d) player=(%d,%d) ydist=%d xdist=%d moves=%d\n",
           actor_name, ax, ay, px, py, ydist, xdist, nmoves);
    fflush(stdout);
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

    struct { int kind; int cur_if; int head; char counter[16]; int jumps[8]; int njumps; } blk[16];
    int blktop = 0;
    int loopid = 0;
    while (fgets(line, sizeof(line), fp)) {
        char *s = pks_trim(line);
        pks_normalize_ascii(s);
        scene_cmd_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        lineno++;
        if (*s == '\0' || *s == '#') continue;
        if (s_scene_cmd_count >= SCENE_CMD_MAX) break;

        if (strncmp(s, "if ", 3) == 0 && blktop < 16 && s_scene_cmd_count < SCENE_CMD_MAX) {
            scene_cmd_t c; memset(&c, 0, sizeof c);
            c.op = SCOP_IF; c.a = -1;
            snprintf(c.text, sizeof c.text, "%s", s + 3);
            int idx = s_scene_cmd_count;
            s_scene_cmds[s_scene_cmd_count++] = c;
            blk[blktop].kind = 0; blk[blktop].cur_if = idx;
            blk[blktop].njumps = 0; blk[blktop].counter[0] = '\0'; blktop++;
            continue;
        }
        if ((strncmp(s, "elif ", 5) == 0 || strcmp(s, "else") == 0) &&
            blktop > 0 && blk[blktop - 1].kind == 0) {
            int b = blktop - 1;

            if (s_scene_cmd_count < SCENE_CMD_MAX && blk[b].njumps < 8) {
                scene_cmd_t j; memset(&j, 0, sizeof j);
                j.op = SCOP_JUMP; j.a = -1;
                blk[b].jumps[blk[b].njumps++] = s_scene_cmd_count;
                s_scene_cmds[s_scene_cmd_count++] = j;
            }

            if (blk[b].cur_if >= 0) s_scene_cmds[blk[b].cur_if].a = s_scene_cmd_count;
            if (s[0] == 'e' && s[1] == 'l' && s[2] == 'i') {
                scene_cmd_t c; memset(&c, 0, sizeof c);
                c.op = SCOP_IF; c.a = -1;
                snprintf(c.text, sizeof c.text, "%s", s + 5);
                blk[b].cur_if = s_scene_cmd_count;
                s_scene_cmds[s_scene_cmd_count++] = c;
            } else {
                blk[b].cur_if = -1;
            }
            continue;
        }
        if (strncmp(s, "while ", 6) == 0 && blktop < 16 && s_scene_cmd_count < SCENE_CMD_MAX) {
            int head = s_scene_cmd_count;
            scene_cmd_t c; memset(&c, 0, sizeof c);
            c.op = SCOP_IF; c.a = -1;
            snprintf(c.text, sizeof c.text, "%s", s + 6);
            s_scene_cmds[s_scene_cmd_count++] = c;
            blk[blktop].kind = 1; blk[blktop].cur_if = head; blk[blktop].head = head;
            blk[blktop].counter[0] = '\0'; blk[blktop].njumps = 0; blktop++;
            continue;
        }
        if (strncmp(s, "repeat ", 7) == 0 && blktop < 16 && s_scene_cmd_count + 1 < SCENE_CMD_MAX) {
            char cname[16]; snprintf(cname, sizeof cname, "__r%d", loopid++);
            scene_cmd_t l; memset(&l, 0, sizeof l); l.op = SCOP_LET;
            snprintf(l.actor, sizeof l.actor, "%s", cname);
            snprintf(l.text, sizeof l.text, "%s", s + 7);
            s_scene_cmds[s_scene_cmd_count++] = l;
            int head = s_scene_cmd_count;
            scene_cmd_t c; memset(&c, 0, sizeof c); c.op = SCOP_IF; c.a = -1;
            snprintf(c.text, sizeof c.text, "%s > 0", cname);
            s_scene_cmds[s_scene_cmd_count++] = c;
            blk[blktop].kind = 1; blk[blktop].cur_if = head; blk[blktop].head = head;
            snprintf(blk[blktop].counter, sizeof blk[blktop].counter, "%s", cname);
            blk[blktop].njumps = 0; blktop++;
            continue;
        }
        if (strcmp(s, "break") == 0) {
            int b = blktop - 1;
            while (b >= 0 && blk[b].kind != 1) b--;
            if (b >= 0 && s_scene_cmd_count < SCENE_CMD_MAX && blk[b].njumps < 8) {
                scene_cmd_t j; memset(&j, 0, sizeof j); j.op = SCOP_JUMP; j.a = -1;
                blk[b].jumps[blk[b].njumps++] = s_scene_cmd_count;
                s_scene_cmds[s_scene_cmd_count++] = j;
            }
            continue;
        }
        if (strcmp(s, "end") == 0 && blktop > 0) {
            int b = --blktop;
            if (blk[b].kind == 1) {
                if (blk[b].counter[0] && s_scene_cmd_count < SCENE_CMD_MAX) {
                    scene_cmd_t d; memset(&d, 0, sizeof d); d.op = SCOP_LET;
                    snprintf(d.actor, sizeof d.actor, "%s", blk[b].counter);
                    snprintf(d.text, sizeof d.text, "%s - 1", blk[b].counter);
                    s_scene_cmds[s_scene_cmd_count++] = d;
                }
                if (s_scene_cmd_count < SCENE_CMD_MAX) {
                    scene_cmd_t bj; memset(&bj, 0, sizeof bj); bj.op = SCOP_JUMP; bj.a = blk[b].head;
                    s_scene_cmds[s_scene_cmd_count++] = bj;
                }
                s_scene_cmds[blk[b].cur_if].a = s_scene_cmd_count;
                for (int k = 0; k < blk[b].njumps; k++)
                    s_scene_cmds[blk[b].jumps[k]].a = s_scene_cmd_count;
            } else {
                if (blk[b].cur_if >= 0) s_scene_cmds[blk[b].cur_if].a = s_scene_cmd_count;
                for (int k = 0; k < blk[b].njumps; k++)
                    s_scene_cmds[blk[b].jumps[k]].a = s_scene_cmd_count;
            }
            continue;
        }

        if (strncmp(s, "include ", 8) == 0) {
            char inc[64] = {0};
            FILE *ifp = NULL;
            char ipath[192];
            if (sscanf(s + 8, "%63s", inc) == 1) {
                static const char *kIncPaths[] = {
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
                for (int pi = 0; pi < (int)(sizeof(kIncPaths)/sizeof(kIncPaths[0])); pi++) {
                    snprintf(ipath, sizeof(ipath), kIncPaths[pi], inc);
                    ifp = fopen(ipath, "r");
                    if (ifp) break;
                }
                if (ifp) {
                    while (fgets(line, sizeof(line), ifp)) {
                        char *ds = pks_trim(line);
                        pks_normalize_ascii(ds);
                        if (strncmp(ds, "def ", 4) != 0) continue;
                        {
                            char defname[32] = {0};
                            int didx;
                            if (sscanf(ds + 4, "%31s", defname) != 1) continue;
                            didx = scene_defs_add(defname);
                            if (didx < 0) continue;
                            while (fgets(line, sizeof(line), ifp)) {
                                char *dline = pks_trim(line);
                                pks_normalize_ascii(dline);
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
                char *ds = pks_trim(line);
                pks_normalize_ascii(ds);
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
                    ct = pks_trim(cont_line);
                    pks_normalize_ascii(ct);
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
                    char id[24], sprite[32], x[32], y[32], extra[16] = {0};
                    int nf = sscanf(s + 6, "%23s %31s %31s %31s %15s", id, sprite, x, y, extra);
                    if (nf < 4) continue;
                    cmd.op = SCOP_SPAWN;
                    snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
                    cmd.a = pks_parse_sprite(sprite);
                    cmd.b = pks_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
                    cmd.c = pks_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
                    if (!pks_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) cmd.b = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
                    if (!pks_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) cmd.c = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
                    cmd.d = (nf >= 5 && strcmp(extra, "keep") == 0) ? 1 : 0;
                } else if (strncmp(s, "place ", 6) == 0) {

                    char id[24], x[32], y[32];
                    if (sscanf(s + 6, "%23s %31s %31s", id, x, y) != 3) continue;
                    cmd.op = SCOP_PLACE;
                    snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
                    cmd.a = pks_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
                    cmd.b = pks_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
                    if (!pks_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) cmd.a = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
                    if (!pks_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) cmd.b = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
                } else if (strncmp(s, "despawn ", 8) == 0) {
                    cmd.op = SCOP_DESPAWN;
                    sscanf(s + 8, "%23s", cmd.actor);
                } else if (strncmp(s, "face ", 5) == 0) {
                    char id[24], dir[24];
                    if (sscanf(s + 5, "%23s %23s", id, dir) != 2) continue;
                    cmd.op = SCOP_FACE;
                    snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
                    if (strcmp(dir, "player") == 0) cmd.a = -2; else cmd.a = scene_parse_dir(dir);
                } else if (strncmp(s, "move_to_player ", 15) == 0) {
                    char id[24];
                    if (sscanf(s + 15, "%23s", id) != 1) continue;
                    cmd.op = SCOP_MOVE_TO_PLAYER;
                    snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
                } else if (strncmp(s, "move ", 5) == 0) {
                    char id[24], dir[24], steps[24];
                    if (sscanf(s + 5, "%23s %23s %23s", id, dir, steps) != 3) continue;
                    cmd.op = SCOP_MOVE;
                    snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
                    cmd.a = scene_parse_dir(dir);
                    cmd.b = (int)strtol(steps, NULL, 0);
                } else if (strncmp(s, "say_hold ", 9) == 0) {

                    cmd.op = SCOP_SAY_HOLD;
                    snprintf(cmd.text, sizeof(cmd.text), "%s", s + 9);
                    {
                        size_t n = strlen(cmd.text);
                        if (n >= 2 && cmd.text[0] == '"' && cmd.text[n - 1] == '"') {
                            memmove(cmd.text, cmd.text + 1, n - 2);
                            cmd.text[n - 2] = '\0';
                        }
                    }
                    pks_scene_resolve_rom_text(cmd.text, sizeof(cmd.text));
                    scene_unescape_text(cmd.text);
                    scene_format_dialog_text(cmd.text);
                } else if (strcmp(s, "close_text") == 0) {
                    cmd.op = SCOP_CLOSE_TEXT;
                } else if (strncmp(s, "say ", 4) == 0 || strncmp(s, "say_auto ", 9) == 0) {
                    int say_auto = (s[3] == '_');
                    cmd.op = SCOP_SAY;
                    cmd.a = say_auto ? 1 : 0;
                    snprintf(cmd.text, sizeof(cmd.text), "%s", s + (say_auto ? 9 : 4));
                    {
                        size_t n = strlen(cmd.text);
                        if (n >= 2 && cmd.text[0] == '"' && cmd.text[n - 1] == '"') {
                            memmove(cmd.text, cmd.text + 1, n - 2);
                            cmd.text[n - 2] = '\0';
                        }
                    }

                    pks_scene_resolve_rom_text(cmd.text, sizeof(cmd.text));
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

                    pks_scene_resolve_rom_text(cmd.text, sizeof(cmd.text));
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
                            int tc = pks_resolve_trainer_class_id(btoks[slot_base]);
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
                    } else if (nt >= 1) {
                        if (strcmp(btoks[0], "random") == 0) { cmd.a = -1; cmd.b = 0; }
                        else {

                            int tc = pks_resolve_trainer_class_id(btoks[0]);
                            if (tc > 0) cmd.a = tc;
                            if (nt >= 2 && pks_is_numeric_token(btoks[1]))
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

                    pks_scene_resolve_rom_text(cmd.text, sizeof(cmd.text));
                    scene_unescape_text(cmd.text);
                    scene_format_dialog_text(cmd.text);
                } else if (strncmp(s, "music_rival_alt ", 16) == 0) {
                    const char *mode = s + 16;
                    int m2 = -1;
                    if (!strcmp(mode, "start")) m2 = 0;
                    else if (!strcmp(mode, "tempo")) m2 = 1;
                    else if (!strcmp(mode, "start_tempo")) m2 = 2;
                    if (m2 < 0) continue;
                    cmd.op = SCOP_MUSIC_RIVAL_ALT;
                    cmd.a = m2;
                } else if (strncmp(s, "music_from_loop ", 16) == 0) {
                    int mid = scene_parse_music_track(s + 16);
                    if (mid < 0) continue;
                    cmd.op = SCOP_MUSIC_FROM_LOOP;
                    cmd.a = mid;
                } else if (strncmp(s, "music ", 6) == 0) {
                    int mid = scene_parse_music_track(s + 6);
                    if (mid < 0) continue;
                    cmd.op = SCOP_MUSIC;
                    cmd.a = mid;
                } else if (strncmp(s, "wait ", 5) == 0) { cmd.op = SCOP_WAIT; cmd.a = (int)strtol(s + 5, NULL, 0); }
                else if (strcmp(s, "wait_text") == 0) { cmd.op = SCOP_WAIT_TEXT; }
                else if (strcmp(s, "wait_sfx") == 0) { cmd.op = SCOP_WAIT_SFX; }
                else if (strcmp(s, "wait_cry") == 0) { cmd.op = SCOP_WAIT_CRY; }
                else if (strcmp(s, "lock_input on") == 0) { cmd.op = SCOP_LOCK_INPUT; cmd.a = 1; }
                else if (strcmp(s, "lock_input off") == 0) { cmd.op = SCOP_LOCK_INPUT; cmd.a = 0; }
                else if (strcmp(s, "fullrate on") == 0) { cmd.op = SCOP_FULLRATE; cmd.a = 1; }
                else if (strcmp(s, "fullrate off") == 0) { cmd.op = SCOP_FULLRATE; cmd.a = 0; }
                else if (strncmp(s, "tile_copy ", 10) == 0 || strncmp(s, "copy_tile ", 10) == 0) {
                    const char *args4 = s + 10;
                    char norm[192], x1[32], y1[32], x2[32], y2[32];
                    AmberScript_NormalizeCoordArgs(args4, norm, sizeof(norm));
                    if (sscanf(norm, "%31s %31s %31s %31s", x1, y1, x2, y2) != 4) continue;
                    if (!AmberScript_ParseCoordExpr(x1, 1, &cmd.a) || !AmberScript_ParseCoordExpr(y1, 0, &cmd.b) ||
                        !AmberScript_ParseCoordExpr(x2, 1, &cmd.c) || !AmberScript_ParseCoordExpr(y2, 0, &cmd.d)) continue;
                    cmd.op = SCOP_TILE_COPY;
                }
                else if (strncmp(s, "tile_save ", 10) == 0) {
                    cmd.op = SCOP_TILE_SAVE;
                    sscanf(s + 10, "%31s", cmd.text);
                }
                else if (strncmp(s, "tile_place_custom ", 18) == 0) {
                    if (!AmberScript_ParseNamedCoordArgs(s + 18, cmd.text, sizeof(cmd.text), &cmd.a, &cmd.b)) continue;
                    cmd.op = SCOP_TILE_PLACE_CUSTOM;
                }
                else if (strncmp(s, "block_save ", 11) == 0) {
                    if (!AmberScript_ParseBlockSaveArgs(s + 11, cmd.text, sizeof(cmd.text), &cmd.a, &cmd.b, &cmd.c, &cmd.d)) continue;
                    cmd.op = SCOP_BLOCK_SAVE;
                }
                else if (strncmp(s, "block_place_custom ", 19) == 0) {
                    if (!AmberScript_ParseNamedCoordArgs(s + 19, cmd.text, sizeof(cmd.text), &cmd.a, &cmd.b)) continue;
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
                    cmd.a = pks_parse_sprite(sprite);
                    cmd.b = pks_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
                    cmd.c = pks_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
                    if (!pks_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) cmd.b = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
                    if (!pks_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) cmd.c = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
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
                    char sp[32], path2[140];
                    int sid;
                    if (sscanf(s + 18, "%31s %139s", sp, path2) != 2) continue;
                    sid = pks_resolve_species_id(sp);
                    if (sid <= 0) continue;
                    cmd.op = SCOP_SPRITE_FRONT_LOAD;
                    cmd.a = sid;
                    snprintf(cmd.text, sizeof(cmd.text), "%s", path2);
                } else if (strncmp(s, "sprite_back_load ", 17) == 0) {
                    char sp[32], path2[140];
                    int sid;
                    if (sscanf(s + 17, "%31s %139s", sp, path2) != 2) continue;
                    sid = pks_resolve_species_id(sp);
                    if (sid <= 0) continue;
                    cmd.op = SCOP_SPRITE_BACK_LOAD;
                    cmd.a = sid;
                    snprintf(cmd.text, sizeof(cmd.text), "%s", path2);
                } else if (strncmp(s, "tile_art_load ", 14) == 0) {
                    char nm[24], path2[140];
                    if (sscanf(s + 14, "%23s %139s", nm, path2) != 2) continue;
                    cmd.op = SCOP_TILE_ART_LOAD;
                    snprintf(cmd.actor, sizeof(cmd.actor), "%s", nm);
                    snprintf(cmd.text, sizeof(cmd.text), "%s", path2);
                }
                else if (scene_parse_movement_extra(s, &cmd)) {  }
                else if (strcmp(s, "end") == 0) { cmd.op = SCOP_END; }
                else continue;

                if (s_scene_cmd_count < SCENE_CMD_MAX) s_scene_cmds[s_scene_cmd_count++] = cmd;
            }
            continue;
        }

        if (strncmp(s, "spawn ", 6) == 0) {
            char id[24], sprite[32], x[32], y[32], extra[16] = {0};
            int nf = sscanf(s + 6, "%23s %31s %31s %31s %15s", id, sprite, x, y, extra);
            if (nf < 4) continue;
            cmd.op = SCOP_SPAWN;
            snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
            cmd.a = pks_parse_sprite(sprite);
            cmd.b = pks_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
            cmd.c = pks_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
            if (!pks_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) cmd.b = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
            if (!pks_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) cmd.c = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
            cmd.d = (nf >= 5 && strcmp(extra, "keep") == 0) ? 1 : 0;
        } else if (strncmp(s, "place ", 6) == 0) {

            char id[24], x[32], y[32];
            if (sscanf(s + 6, "%23s %31s %31s", id, x, y) != 3) continue;
            cmd.op = SCOP_PLACE;
            snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
            cmd.a = pks_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
            cmd.b = pks_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
            if (!pks_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) cmd.a = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
            if (!pks_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) cmd.b = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
        } else if (strncmp(s, "despawn ", 8) == 0) {
            cmd.op = SCOP_DESPAWN;
            sscanf(s + 8, "%23s", cmd.actor);
        } else if (strncmp(s, "face ", 5) == 0) {
            char id[24], dir[24];
            if (sscanf(s + 5, "%23s %23s", id, dir) != 2) continue;
            cmd.op = SCOP_FACE;
            snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
            if (strcmp(dir, "player") == 0) cmd.a = -2; else cmd.a = scene_parse_dir(dir);
        } else if (strncmp(s, "move_to_player ", 15) == 0) {
            char id[24];
            if (sscanf(s + 15, "%23s", id) != 1) continue;
            cmd.op = SCOP_MOVE_TO_PLAYER;
            snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
        } else if (strncmp(s, "move ", 5) == 0) {
            char id[24], dir[24], steps[24];
            if (sscanf(s + 5, "%23s %23s %23s", id, dir, steps) != 3) continue;
            cmd.op = SCOP_MOVE;
            snprintf(cmd.actor, sizeof(cmd.actor), "%s", id);
            cmd.a = scene_parse_dir(dir);
            cmd.b = (int)strtol(steps, NULL, 0);
        } else if (strncmp(s, "say_hold ", 9) == 0) {
            cmd.op = SCOP_SAY_HOLD;
            snprintf(cmd.text, sizeof(cmd.text), "%s", s + 9);
            {
                size_t n = strlen(cmd.text);
                if (n >= 2 && cmd.text[0] == '"' && cmd.text[n - 1] == '"') {
                    memmove(cmd.text, cmd.text + 1, n - 2);
                    cmd.text[n - 2] = '\0';
                }
            }

            pks_scene_resolve_rom_text(cmd.text, sizeof(cmd.text));
            scene_unescape_text(cmd.text);
            scene_format_dialog_text(cmd.text);
        } else if (strcmp(s, "close_text") == 0) {
            cmd.op = SCOP_CLOSE_TEXT;
        } else if (strncmp(s, "say ", 4) == 0 || strncmp(s, "say_auto ", 9) == 0) {
            int say_auto = (s[3] == '_');
            cmd.op = SCOP_SAY;
            cmd.a = say_auto ? 1 : 0;
            snprintf(cmd.text, sizeof(cmd.text), "%s", s + (say_auto ? 9 : 4));
            {
                size_t n = strlen(cmd.text);
                if (n >= 2 && cmd.text[0] == '"' && cmd.text[n - 1] == '"') {
                    memmove(cmd.text, cmd.text + 1, n - 2);
                    cmd.text[n - 2] = '\0';
                }
            }

            pks_scene_resolve_rom_text(cmd.text, sizeof(cmd.text));
            scene_unescape_text(cmd.text);
            scene_format_dialog_text(cmd.text);
        } else if (strncmp(s, "priced_choice ", 14) == 0) {

            cmd.op = SCOP_PRICED_CHOICE;
            {
                const char *p = s + 14;
                char item1[24] = {0}, item2[24] = {0};
                int price = 0;
                while (*p == ' ') p++;
                if (*p == '"') {
                    p++;
                    size_t i = 0;
                    while (*p && *p != '"' && i < sizeof(item1) - 1) item1[i++] = *p++;
                    if (*p == '"') p++;
                }
                price = (int)strtol(p, (char **)&p, 0);
                while (*p == ' ') p++;
                if (*p == '"') {
                    p++;
                    size_t i = 0;
                    while (*p && *p != '"' && i < sizeof(item2) - 1) item2[i++] = *p++;
                    if (*p == '"') p++;
                }
                snprintf(cmd.actor, sizeof(cmd.actor), "%s", item1);
                snprintf(cmd.text, sizeof(cmd.text), "%s", item2);
                cmd.a = price;
            }
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

            pks_scene_resolve_rom_text(cmd.text, sizeof(cmd.text));
            scene_unescape_text(cmd.text);
            scene_format_dialog_text(cmd.text);
        } else if (strncmp(s, "battlestart", 11) == 0) {

            char btoks[10][96];
            int nt = scene_split_args_quoted(s + 11, btoks, 10);
            cmd.op = SCOP_BATTLESTART;
            cmd.a = 34; cmd.b = 1;
            if (nt >= 1 && strcmp(btoks[0], "custom") == 0) {
                int slot_base = 1, ok = 1;
                cmd.c = 1;
                cmd.b = MUSIC_TRAINER_BATTLE;
                if (nt > 1) { int mid = scene_parse_music_track(btoks[1]); if (mid > 0) { cmd.b = mid; slot_base = 2; } }
                if (nt > slot_base) { int tc = pks_resolve_trainer_class_id(btoks[slot_base]); if (tc > 0) { cmd.a = tc; slot_base++; } }
                for (int si = 0; si < 6; si++) {
                    int tix = slot_base + si;
                    const char *slot = (tix < nt) ? btoks[tix] : "empty";
                    if (!scene_parse_team_slot(slot, &cmd.team_species[si], &cmd.team_level[si], cmd.team_moves[si])) { ok = 0; break; }
                    if (cmd.team_species[si] != 0) cmd.team_count = (uint8_t)(si + 1);
                }
                if (nt > slot_base + 6) { snprintf(cmd.text, sizeof(cmd.text), "%s", btoks[slot_base + 6]); scene_unescape_text(cmd.text); scene_format_dialog_text(cmd.text); }
                if (!ok) { printf("[scene] bad custom battlestart near '%s'\n", s); continue; }
            } else if (nt >= 1) {
                if (strcmp(btoks[0], "random") == 0) { cmd.a = -1; cmd.b = 0; }
                else {

                    int tc = pks_resolve_trainer_class_id(btoks[0]);
                    if (tc > 0) cmd.a = tc;
                    if (nt >= 2 && pks_is_numeric_token(btoks[1])) cmd.b = (int)strtol(btoks[1], NULL, 0);
                }
                if (nt >= 3) { snprintf(cmd.text, sizeof(cmd.text), "%s", btoks[2]); scene_unescape_text(cmd.text); scene_format_dialog_text(cmd.text); }

                for (int bi = 3; bi < nt; bi++)
                    if (strcmp(btoks[bi], "no_blackout") == 0) cmd.d = 1;
            }
        } else if (strncmp(s, "battlend", 8) == 0) {
            cmd.op = SCOP_BATTLEEND;
            if (sscanf(s + 8, " %159[^\n]", cmd.text) != 1) snprintf(cmd.text, sizeof(cmd.text), "Battle complete.@");
            { size_t n = strlen(cmd.text); if (n >= 2 && cmd.text[0] == '"' && cmd.text[n-1] == '"') { memmove(cmd.text, cmd.text + 1, n - 2); cmd.text[n-2] = '\0'; } }

            pks_scene_resolve_rom_text(cmd.text, sizeof(cmd.text));
            scene_unescape_text(cmd.text);
            scene_format_dialog_text(cmd.text);
        } else if (strncmp(s, "music_rival_alt ", 16) == 0) {
            const char *mode = s + 16;
            int m2 = -1;
            if (!strcmp(mode, "start")) m2 = 0;
            else if (!strcmp(mode, "tempo")) m2 = 1;
            else if (!strcmp(mode, "start_tempo")) m2 = 2;
            if (m2 < 0) continue;
            cmd.op = SCOP_MUSIC_RIVAL_ALT;
            cmd.a = m2;
        } else if (strncmp(s, "music_from_loop ", 16) == 0) {
            int mid = scene_parse_music_track(s + 16);
            if (mid < 0) continue;
            cmd.op = SCOP_MUSIC_FROM_LOOP;
            cmd.a = mid;
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
        } else if (strcmp(s, "wait_sfx") == 0) {
            cmd.op = SCOP_WAIT_SFX;
        } else if (strcmp(s, "wait_cry") == 0) {
            cmd.op = SCOP_WAIT_CRY;
        } else if (strcmp(s, "lock_input on") == 0) {
            cmd.op = SCOP_LOCK_INPUT; cmd.a = 1;
        } else if (strcmp(s, "lock_input off") == 0) {
            cmd.op = SCOP_LOCK_INPUT; cmd.a = 0;
        } else if (strcmp(s, "fullrate on") == 0) {
            cmd.op = SCOP_FULLRATE; cmd.a = 1;
        } else if (strcmp(s, "fullrate off") == 0) {
            cmd.op = SCOP_FULLRATE; cmd.a = 0;
        } else if (strncmp(s, "tile_copy ", 10) == 0 || strncmp(s, "copy_tile ", 10) == 0) {
            const char *args4 = s + 10;
            char norm[192], x1[32], y1[32], x2[32], y2[32];
            AmberScript_NormalizeCoordArgs(args4, norm, sizeof(norm));
            if (sscanf(norm, "%31s %31s %31s %31s", x1, y1, x2, y2) != 4) continue;
            if (!AmberScript_ParseCoordExpr(x1, 1, &cmd.a) || !AmberScript_ParseCoordExpr(y1, 0, &cmd.b) ||
                !AmberScript_ParseCoordExpr(x2, 1, &cmd.c) || !AmberScript_ParseCoordExpr(y2, 0, &cmd.d)) continue;
            cmd.op = SCOP_TILE_COPY;
        } else if (strncmp(s, "tile_save ", 10) == 0) {
            cmd.op = SCOP_TILE_SAVE;
            sscanf(s + 10, "%31s", cmd.text);
        } else if (strncmp(s, "tile_place_custom ", 18) == 0) {
            if (!AmberScript_ParseNamedCoordArgs(s + 18, cmd.text, sizeof(cmd.text), &cmd.a, &cmd.b)) continue;
            cmd.op = SCOP_TILE_PLACE_CUSTOM;
        } else if (strncmp(s, "block_save ", 11) == 0) {
            if (!AmberScript_ParseBlockSaveArgs(s + 11, cmd.text, sizeof(cmd.text), &cmd.a, &cmd.b, &cmd.c, &cmd.d)) continue;
            cmd.op = SCOP_BLOCK_SAVE;
        } else if (strncmp(s, "block_place_custom ", 19) == 0) {
            if (!AmberScript_ParseNamedCoordArgs(s + 19, cmd.text, sizeof(cmd.text), &cmd.a, &cmd.b)) continue;
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
            cmd.a = pks_parse_sprite(sprite);
            cmd.b = pks_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
            cmd.c = pks_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
            if (!pks_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) cmd.b = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
            if (!pks_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) cmd.c = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
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
            char sp[32], path2[140];
            int sid;
            if (sscanf(s + 18, "%31s %139s", sp, path2) != 2) continue;
            sid = pks_resolve_species_id(sp);
            if (sid <= 0) continue;
            cmd.op = SCOP_SPRITE_FRONT_LOAD;
            cmd.a = sid;
            snprintf(cmd.text, sizeof(cmd.text), "%s", path2);
        } else if (strncmp(s, "sprite_back_load ", 17) == 0) {
            char sp[32], path2[140];
            int sid;
            if (sscanf(s + 17, "%31s %139s", sp, path2) != 2) continue;
            sid = pks_resolve_species_id(sp);
            if (sid <= 0) continue;
            cmd.op = SCOP_SPRITE_BACK_LOAD;
            cmd.a = sid;
            snprintf(cmd.text, sizeof(cmd.text), "%s", path2);
        } else if (strncmp(s, "tile_art_load ", 14) == 0) {
            char nm[24], path2[140];
            if (sscanf(s + 14, "%23s %139s", nm, path2) != 2) continue;
            cmd.op = SCOP_TILE_ART_LOAD;
            snprintf(cmd.actor, sizeof(cmd.actor), "%s", nm);
            snprintf(cmd.text, sizeof(cmd.text), "%s", path2);
        } else if (scene_parse_movement_extra(s, &cmd)) {

        } else if (strcmp(s, "end") == 0 || strcmp(s, "stop") == 0) {

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

static int scene_trigger_find_slot_by_pos(int map_id, int x, int y) {
    for (int i = 0; i < SCENE_TRIGGER_MAX; i++) {
        if (!s_scene_triggers[i].used) continue;
        if ((int)s_scene_triggers[i].map_id == map_id &&
            s_scene_triggers[i].x == x &&
            s_scene_triggers[i].y == y) return i;
    }
    return -1;
}

void AmberScript_SceneTriggerSuppressAt(int map_id, int x, int y) {
    int slot = scene_trigger_find_slot_by_pos(map_id, x, y);
    if (slot < 0) return;
    s_scene_triggers[slot].armed = 0;
    s_scene_triggers[slot].fired_x = (int16_t)x;
    s_scene_triggers[slot].fired_y = (int16_t)y;
}

static int scene_trigger_alloc_slot(void) {
    for (int i = 0; i < SCENE_TRIGGER_MAX; i++) if (!s_scene_triggers[i].used) return i;
    return -1;
}

static int pks_scene_trigger_register_ex(const char *scene, int map_id, int x, int y,
                                          uint8_t cond_kind, uint16_t cond_event,
                                          uint8_t cond_kind2, uint16_t cond_event2,
                                          int is_onload, int is_watch) {
    int ncmd;
    int slot = scene_trigger_find_slot(scene, map_id, x, y);
    if (slot >= 0) return 1;
    ncmd = scene_load_file(scene);
    if (ncmd <= 0) return 0;
    slot = scene_trigger_alloc_slot();
    if (slot < 0) {

        printf("[amberscript] SCENE TRIGGER TABLE FULL (%d) -- dropped '%s' on map %d at (%d,%d)\n",
               SCENE_TRIGGER_MAX, scene, map_id, x, y);
        return -1;
    }
    s_scene_triggers[slot].used = 1;
    snprintf(s_scene_triggers[slot].scene, sizeof(s_scene_triggers[slot].scene), "%s", scene);
    s_scene_triggers[slot].map_id = (uint8_t)map_id;
    s_scene_triggers[slot].x = x;
    s_scene_triggers[slot].y = y;
    s_scene_triggers[slot].armed = 1;
    s_scene_triggers[slot].cond_kind = cond_kind;
    s_scene_triggers[slot].cond_event = cond_event;
    s_scene_triggers[slot].cond_kind2 = cond_kind2;
    s_scene_triggers[slot].cond_event2 = cond_event2;
    s_scene_triggers[slot].fired_x = -32768;
    s_scene_triggers[slot].fired_y = -32768;
    s_scene_triggers[slot].is_onload = is_onload;
    s_scene_triggers[slot].is_watch = is_watch;
    return ncmd;
}

static int pks_scene_trigger_register(const char *scene, int map_id, int x, int y,
                                       uint8_t cond_kind, uint16_t cond_event,
                                       uint8_t cond_kind2, uint16_t cond_event2) {
    return pks_scene_trigger_register_ex(scene, map_id, x, y, cond_kind, cond_event,
                                          cond_kind2, cond_event2, 0, 0);
}

int AmberScript_MapAddSceneTrigger(const char *map_name, int x, int y, const char *scene,
                                   const char *cond_kind_name, const char *cond_event_name,
                                   const char *cond_kind2_name, const char *cond_event2_name) {
    int real_id;
    uint8_t cond_kind = 0, cond_kind2 = 0;
    uint16_t cond_event = 0, cond_event2 = 0;
    if (!map_name || !*map_name || !scene || !*scene) return 0;
    real_id = AmberScript_MapBank_GetOrAssignRealId(map_name);
    if (real_id < 0) return 0;
    if (cond_kind_name && *cond_kind_name) {
        if (strcmp(cond_kind_name, "event_set") == 0) cond_kind = 1;
        else if (strcmp(cond_kind_name, "event_clear") == 0) cond_kind = 2;
        else return 0;
        if (!cond_event_name || !pks_resolve_event_token(cond_event_name, &cond_event)) return 0;
    }
    if (cond_kind2_name && *cond_kind2_name) {
        if (strcmp(cond_kind2_name, "event_set") == 0) cond_kind2 = 1;
        else if (strcmp(cond_kind2_name, "event_clear") == 0) cond_kind2 = 2;
        else return 0;
        if (!cond_event2_name || !pks_resolve_event_token(cond_event2_name, &cond_event2)) return 0;
    }
    return pks_scene_trigger_register(scene, real_id, x, y, cond_kind, cond_event, cond_kind2, cond_event2) > 0;
}

int AmberScript_MapAddZoneLatch(const char *map_name, const char *event_name,
                               const int *xs, const int *ys, int ntiles) {
    int real_id, slot = -1;
    uint16_t ev = 0;
    if (!map_name || !*map_name || !event_name || !*event_name) return 0;
    if (!xs || !ys || ntiles <= 0) return 0;
    if (ntiles > SCENE_ZONE_LATCH_TILES) return 0;
    if (!pks_resolve_event_token(event_name, &ev)) return 0;
    real_id = AmberScript_MapBank_GetOrAssignRealId(map_name);
    if (real_id < 0) return 0;
    for (int i = 0; i < SCENE_ZONE_LATCH_MAX; i++) {
        if (s_scene_zone_latches[i].used &&
            (int)s_scene_zone_latches[i].map_id == real_id &&
            s_scene_zone_latches[i].event == ev)
            return 1;
    }
    for (int i = 0; i < SCENE_ZONE_LATCH_MAX; i++)
        if (!s_scene_zone_latches[i].used) { slot = i; break; }
    if (slot < 0) {
        printf("[amberscript] zone_latch: no free slot for '%s' %s\n", map_name, event_name);
        return 0;
    }
    s_scene_zone_latches[slot].used = 1;
    s_scene_zone_latches[slot].map_id = (uint8_t)real_id;
    s_scene_zone_latches[slot].event = ev;
    s_scene_zone_latches[slot].ntiles = ntiles;
    for (int i = 0; i < ntiles; i++) {
        s_scene_zone_latches[slot].x[i] = xs[i];
        s_scene_zone_latches[slot].y[i] = ys[i];
    }
    printf("[amberscript] zone_latch: '%s' real=%d %s over %d tile(s)\n",
           map_name, real_id, event_name, ntiles);
    return 1;
}

static void scene_zone_latches_step(void) {
    for (int i = 0; i < SCENE_ZONE_LATCH_MAX; i++) {
        scene_zone_latch_t *z = &s_scene_zone_latches[i];
        if (!z->used) continue;
        if ((int)z->map_id != (int)wCurMap) continue;
        if (!CheckEvent(z->event)) continue;
        int inside = 0;
        for (int t = 0; t < z->ntiles; t++) {
            if ((int)wXCoord == z->x[t] && (int)wYCoord == z->y[t]) { inside = 1; break; }
        }
        if (inside) continue;
        ClearEvent(z->event);
        printf("[amberscript] zone_latch: left zone at (%d,%d) -> cleared event %u\n",
               (int)wXCoord, (int)wYCoord, (unsigned)z->event);
        fflush(stdout);
    }
}

int AmberScript_MapAddSceneTriggerOnLoad(const char *map_name, const char *scene,
                                         const char *cond_kind_name, const char *cond_event_name,
                                         const char *cond_kind2_name, const char *cond_event2_name) {
    int real_id;
    uint8_t cond_kind = 0, cond_kind2 = 0;
    uint16_t cond_event = 0, cond_event2 = 0;
    if (!map_name || !*map_name || !scene || !*scene) return 0;
    real_id = AmberScript_MapBank_GetOrAssignRealId(map_name);
    if (real_id < 0) return 0;
    if (cond_kind_name && *cond_kind_name) {
        if (strcmp(cond_kind_name, "event_set") == 0) cond_kind = 1;
        else if (strcmp(cond_kind_name, "event_clear") == 0) cond_kind = 2;
        else return 0;
        if (!cond_event_name || !pks_resolve_event_token(cond_event_name, &cond_event)) return 0;
    }
    if (cond_kind2_name && *cond_kind2_name) {
        if (strcmp(cond_kind2_name, "event_set") == 0) cond_kind2 = 1;
        else if (strcmp(cond_kind2_name, "event_clear") == 0) cond_kind2 = 2;
        else return 0;
        if (!cond_event2_name || !pks_resolve_event_token(cond_event2_name, &cond_event2)) return 0;
    }
    return pks_scene_trigger_register_ex(scene, real_id, 0, 0, cond_kind, cond_event,
                                          cond_kind2, cond_event2, 1, 0) > 0;
}

int AmberScript_MapAddSceneTriggerWatch(const char *map_name, const char *scene,
                                        const char *cond_kind_name, const char *cond_event_name,
                                        const char *cond_kind2_name, const char *cond_event2_name) {
    int real_id;
    uint8_t cond_kind = 0, cond_kind2 = 0;
    uint16_t cond_event = 0, cond_event2 = 0;
    if (!map_name || !*map_name || !scene || !*scene) return 0;
    real_id = AmberScript_MapBank_GetOrAssignRealId(map_name);
    if (real_id < 0) return 0;
    if (!cond_kind_name || !*cond_kind_name) {
        printf("[amberscript] scene_trigger watch '%s': needs a `when event_set|"
               "event_clear <EVENT>` gate -- an ungated watch fires every frame "
               "forever\n", scene);
        return 0;
    }
    if (strcmp(cond_kind_name, "event_set") == 0) cond_kind = 1;
    else if (strcmp(cond_kind_name, "event_clear") == 0) cond_kind = 2;
    else return 0;
    if (!cond_event_name || !pks_resolve_event_token(cond_event_name, &cond_event)) return 0;
    if (cond_kind2_name && *cond_kind2_name) {
        if (strcmp(cond_kind2_name, "event_set") == 0) cond_kind2 = 1;
        else if (strcmp(cond_kind2_name, "event_clear") == 0) cond_kind2 = 2;
        else return 0;
        if (!cond_event2_name || !pks_resolve_event_token(cond_event2_name, &cond_event2)) return 0;
    }
    return pks_scene_trigger_register_ex(scene, real_id, 0, 0, cond_kind, cond_event,
                                          cond_kind2, cond_event2, 0, 1) > 0;
}

int AmberScript_MapAddSceneNpc(const char *map_name, const char *scene, int x, int y) {
    int real_id, slot = -1;
    if (!map_name || !*map_name || !scene || !*scene) return 0;
    real_id = AmberScript_MapBank_GetOrAssignRealId(map_name);
    if (real_id < 0) return 0;
    for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
        if (!s_scene_npc_bindings[i].used) continue;
        if (s_scene_npc_bindings[i].map_id == (uint8_t)real_id &&
            s_scene_npc_bindings[i].tile_x == x && s_scene_npc_bindings[i].tile_y == y &&
            strcmp(s_scene_npc_bindings[i].scene, scene) == 0)
            return 1;
    }
    slot = scene_npc_binding_alloc();
    if (slot < 0) {
        printf("[amberscript] SCENE ACTOR TABLE FULL (%d) -- dropped scene_npc '%s' on map %d at (%d,%d)\n",
               SCENE_ACTOR_MAX, scene, real_id, x, y);
        return 0;
    }
    memset(&s_scene_npc_bindings[slot], 0, sizeof(s_scene_npc_bindings[slot]));
    s_scene_npc_bindings[slot].used = 1;
    s_scene_npc_bindings[slot].npc_idx = -1;
    s_scene_npc_bindings[slot].map_id = (uint8_t)real_id;
    s_scene_npc_bindings[slot].tile_x = x;
    s_scene_npc_bindings[slot].tile_y = y;
    snprintf(s_scene_npc_bindings[slot].name, sizeof(s_scene_npc_bindings[slot].name), "%s@%d,%d", scene, x, y);
    snprintf(s_scene_npc_bindings[slot].scene, sizeof(s_scene_npc_bindings[slot].scene), "%s", scene);
    return 1;
}

int AmberScript_MapAddSceneTile(const char *map_name, const char *scene, int x, int y) {
    int real_id, slot;
    if (!map_name || !*map_name || !scene || !*scene) return 0;
    real_id = AmberScript_MapBank_GetOrAssignRealId(map_name);
    if (real_id < 0) return 0;
    for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
        if (!s_scene_npc_bindings[i].used) continue;
        if (s_scene_npc_bindings[i].map_id == (uint8_t)real_id &&
            s_scene_npc_bindings[i].tile_x == x && s_scene_npc_bindings[i].tile_y == y &&
            s_scene_npc_bindings[i].tile_only &&
            strcmp(s_scene_npc_bindings[i].scene, scene) == 0)
            return 1;
    }
    slot = scene_npc_binding_alloc();
    if (slot < 0) {
        printf("[amberscript] SCENE ACTOR TABLE FULL (%d) -- dropped scene_tile '%s' on map %d at (%d,%d)\n",
               SCENE_ACTOR_MAX, scene, real_id, x, y);
        return 0;
    }
    memset(&s_scene_npc_bindings[slot], 0, sizeof(s_scene_npc_bindings[slot]));
    s_scene_npc_bindings[slot].used = 1;
    s_scene_npc_bindings[slot].npc_idx = -1;
    s_scene_npc_bindings[slot].map_id = (uint8_t)real_id;
    s_scene_npc_bindings[slot].tile_x = x;
    s_scene_npc_bindings[slot].tile_y = y;
    s_scene_npc_bindings[slot].tile_only = 1;
    snprintf(s_scene_npc_bindings[slot].name, sizeof(s_scene_npc_bindings[slot].name), "%s@%d,%d", scene, x, y);
    snprintf(s_scene_npc_bindings[slot].scene, sizeof(s_scene_npc_bindings[slot].scene), "%s", scene);
    return 1;
}

int AmberScript_OnTileInteracted(int fx, int fy) {
    for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
        if (!s_scene_npc_bindings[i].used) continue;
        if (!s_scene_npc_bindings[i].tile_only) continue;
        if (s_scene_npc_bindings[i].map_id != wCurMap) continue;
        if (s_scene_npc_bindings[i].tile_x != fx || s_scene_npc_bindings[i].tile_y != fy) continue;
        if (s_scene_active) return 1;
        scene_reset_runtime();
        int ncmd = scene_load_file(s_scene_npc_bindings[i].scene);
        if (ncmd <= 0) {
            printf("[amberscript] scene_tile interact: failed to load scene '%s'\n",
                   s_scene_npc_bindings[i].scene);
            return 1;
        }
        s_scene_active = 1;
        s_scene_pc = 0;
        s_scene_ctx_npc = -1;
        printf("[amberscript] scene_tile interact (%d,%d) -> scene '%s' (%d command(s))\n",
               fx, fy, s_scene_npc_bindings[i].scene, ncmd);
        return 1;
    }
    return 0;
}

void AmberScript_RearmSceneTriggers(void) {
    for (int i = 0; i < SCENE_TRIGGER_MAX; i++)
        if (s_scene_triggers[i].used) s_scene_triggers[i].armed = 1;
    if (s_scene_active) {
        scene_reset_runtime();
        gScriptedMovement = 0;
        wJoyIgnore = 0;
    }
}

int AmberScript_SceneWantsFullRate(void) {

    return s_scene_active && (s_scene_fullrate || s_scene_fade_active);
}

int AmberScript_Scene_IsActive(void) {
    return s_scene_active;
}

int AmberScript_SceneDisasm(const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    fprintf(fp, "# scene disasm: %d command(s), active=%d pc=%d\n",
            s_scene_cmd_count, s_scene_active, s_scene_pc);
    for (int i = 0; i < s_scene_cmd_count; i++) {
        const scene_cmd_t *c = &s_scene_cmds[i];
        const char *mark = (i == s_scene_pc && s_scene_active) ? ">" : " ";
        fprintf(fp, "%s%4d: op=%-3d actor='%s' a=%d b=%d c=%d d=%d",
                mark, i, (int)c->op, c->actor, c->a, c->b, c->c, c->d);
        if (c->text[0]) {
            fprintf(fp, " text='%.32s%s'", c->text,
                    strlen(c->text) > 32 ? "..." : "");
        }
        if ((c->op == SCOP_IF || c->op == SCOP_JUMP) &&
            (c->a < 0 || c->a > s_scene_cmd_count)) {
            fprintf(fp, "  !! JUMP TARGET OUT OF RANGE (0..%d)",
                    s_scene_cmd_count);
        }
        fputc('\n', fp);
    }
    fclose(fp);
    return s_scene_cmd_count;
}

void AmberScript_Scene_Tick(void) {

    extern int Game_IsOverworldTickActive(void);
    dsl_bank_init_if_needed();
    if (s_dsl_bank_enabled && s_dsl_bank_last_map != wCurMap) {
        dsl_bank_ensure_current_map_spawns();
        dsl_bank_ensure_current_map_tiles();
        s_dsl_bank_last_map = wCurMap;
    }

    if (s_py_law_enabled) {
        s_py_law_frame_accum++;
        if (s_py_law_frame_accum >= 60) {
            s_py_law_frame_accum = 0;
            py_law_tick_once();
            s_py_law_elapsed_sec++;
        }
    }

    if (Game_IsOverworldTickActive() && s_scene_active) {

        if (wCurMap != s_scene_map && !s_scene_wait_battle) {
            printf("[amberscript] scene aborted: map changed %u->%u mid-scene\n",
                   (unsigned)s_scene_map, (unsigned)wCurMap);
            fflush(stdout);
            gScriptedMovement = 0;
            wJoyIgnore = 0;

            for (int i = 0; i < SCENE_ACTOR_MAX; i++)
                s_scene_actors[i].npc_idx = -1;
            scene_reset_runtime();
            return;
        }

        {
            extern int Warp_IsPending(void);
            if (Warp_IsPending()) return;
        }

        scene_track_actor_positions();

        if (s_scene_wait_emote > 0 && --s_scene_wait_emote == 0) {
            Emote_Hide();
            s_scene_emote_npc = -1;
        }

        if (s_scene_battlestart_pending) {
            if (Text_IsOpen()) {
                s_scene_battlestart_saw_text = 1;
            } else if (!s_scene_battlestart_delay) {
                s_scene_battlestart_delay = 1;
            } else {
                const char *defeat_text = s_scene_cmds[s_scene_pc].text;

                gBattleNoBlackoutOnLoss = s_scene_battlestart_noblackout;
                scene_start_trainer_battle(s_scene_battlestart_tc, s_scene_battlestart_tn, defeat_text);
                s_scene_wait_battle = 1;
                s_scene_battlestart_pending = 0;
                s_scene_battlestart_saw_text = 0;
                s_scene_battlestart_delay = 0;
                s_scene_pc++;
                printf("[amberscript] scene battlestart: class=%d no=%d noblackout=%d\n",
                       s_scene_battlestart_tc, s_scene_battlestart_tn, s_scene_battlestart_noblackout);
            }
        } else if (s_scene_wait_say) {
            if (Text_IsOpen()) s_scene_say_opened = 1;
            if (s_scene_say_opened && !Text_IsOpen()) {

                if (s_scene_say_auto) { Text_Close(); s_scene_say_auto = 0; }
                s_scene_wait_say = 0;
                s_scene_say_opened = 0;
            }
        } else if (s_scene_wait_print) {

            if (Text_IsOpen()) s_scene_print_opened = 1;
            if (s_scene_print_opened && !Text_IsOpen()) {
                s_scene_wait_print = 0;
                s_scene_print_opened = 0;
            }
        } else if (s_scene_wait_engage_pretext) {

            if (!Trainer_IsEngaging()) {
                s_scene_wait_engage_pretext = 0;
                s_scene_wait_battle = 1;

                wJoyIgnore = 0;
            }
        } else if (s_scene_wait_battle) {
            int sc = Game_GetScene();
            if (sc == 2 || BattleUI_IsActive())
                s_scene_battle_started = 1;

            if (s_scene_battle_started && sc == 0 && !BattleUI_IsActive()
                    && !Game_IsWarpFadeActive()) {
                scene_restore_spawned_actors_after_battle();
                s_scene_wait_battle = 0;
                s_scene_battle_started = 0;

                wBattleType = 0;

                if (s_scene_input_locked) { hJoyHeld = 0; wJoyIgnore = PAD_CTRL_PAD; }
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

                Text_Close();
                if (s_scene_yesno_restore_joyignore) {
                    wJoyIgnore = s_scene_yesno_prev_joyignore;
                    s_scene_yesno_restore_joyignore = 0;
                }
                if (s_scene_yesno_restore_scripted_movement) {
                    gScriptedMovement = s_scene_yesno_prev_scripted_movement;
                    s_scene_yesno_restore_scripted_movement = 0;
                }
                printf("[amberscript] scene ask result: %s\n", s_scene_last_yesno ? "yes" : "no");
            }
        } else if (s_scene_wait_priced_choice) {
            BikeShopMenu_Tick();
            if (!BikeShopMenu_IsOpen()) {
                s_scene_last_yesno = BikeShopMenu_GetResult() ? 1 : 0;
                s_scene_wait_priced_choice = 0;
                if (s_scene_yesno_restore_joyignore) {
                    wJoyIgnore = s_scene_yesno_prev_joyignore;
                    s_scene_yesno_restore_joyignore = 0;
                }
                if (s_scene_yesno_restore_scripted_movement) {
                    gScriptedMovement = s_scene_yesno_prev_scripted_movement;
                    s_scene_yesno_restore_scripted_movement = 0;
                }
                printf("[amberscript] scene priced_choice result: %s\n", s_scene_last_yesno ? "item1" : "item2/B");
            }
        } else if (s_scene_wait_dex) {

            if (Pokedex_IsShowingData()) {

            } else if (s_scene_wait_dex_delay > 0) {
                if (s_scene_wait_dex_delay == 10) {
                    Map_ReloadGfx();
                    Font_Load();
                    NPC_ReloadTiles();
                    Display_SetPalette(0xE4, 0xD0, 0xE0);

                    GbcColor_MarkDirty();
                }
                s_scene_wait_dex_delay--;
            } else {
                s_scene_wait_dex = 0;
            }
        } else if (s_scene_wait_townmap) {

            if (!TownMap_IsOpen())
                s_scene_wait_townmap = 0;
    s_scene_wait_ship_depart = 0;
        } else if (s_scene_wait_ship_depart) {

            if (!SSAnneDepart_IsActive())
                s_scene_wait_ship_depart = 0;
        } else if (s_scene_wait_blackboard) {

            if (!Blackboard_IsOpen())
                s_scene_wait_blackboard = 0;
        } else if (s_scene_wait_link_cable_help) {

            if (!LinkCableHelp_IsOpen())
                s_scene_wait_link_cable_help = 0;
        } else if (s_scene_wait_list_choice) {

            if (!BagListChoice_IsOpen()) {
                s_scene_last_list_choice = BagListChoice_GetResult();
                s_scene_wait_list_choice = 0;
            }
        } else if (s_scene_wait_prize_list) {

            if (!PrizeListChoice_IsOpen()) {
                s_scene_last_prize_choice = PrizeListChoice_GetResult();
                s_scene_wait_prize_list = 0;
            }
        } else if (s_scene_wait_fossil) {

            if (!Fossil_IsOpen())
                s_scene_wait_fossil = 0;
        } else if (s_scene_wait_bills_dex_list) {

            if (!BillsPokemonList_IsOpen())
                s_scene_wait_bills_dex_list = 0;
        } else if (s_scene_wait_badge_house) {

            if (!BadgeHouseMenu_IsOpen())
                s_scene_wait_badge_house = 0;
            s_scene_wait_diploma = 0;
        } else if (s_scene_wait_diploma) {

            if (!Diploma_IsOpen())
                s_scene_wait_diploma = 0;
        } else if (s_scene_wait_trade) {

            Trade_Tick();
            if (!Trade_IsActive()) s_scene_wait_trade = 0;
        } else if (s_scene_wait_name) {

            if (!NamingScreen_IsOpen()) s_scene_wait_name = 0;
        } else if (s_scene_wait > 0) {
            s_scene_wait--;
        } else if (s_scene_fade_active) {

            if (--s_scene_fade_timer > 0) {

            } else {
                s_scene_fade_step++;
                const uint8_t (*tbl)[3];
                switch (s_scene_fade_active) {
                    case 1: tbl = kSceneFadeOutToWhite; break;
                    case 3: tbl = kSceneFadeOutToBlack; break;
                    case 4: tbl = kSceneFadeInFromBlack; break;
                    default: tbl = kSceneFadeInFromWhite; break;
                }
                if (s_scene_fade_step < s_scene_fade_steps) {
                    Display_SetPalette(tbl[s_scene_fade_step][0],
                                       tbl[s_scene_fade_step][1],
                                       tbl[s_scene_fade_step][2]);
                    s_scene_fade_timer = SCENE_FADE_TICKS_PER_STEP;
                } else {

                    if (s_scene_fade_active == 2 || s_scene_fade_active == 4) Display_LoadMapPalette();
                    s_scene_fade_active = 0;
                    s_scene_pc++;
                }
            }
        } else if (s_scene_wtx_active) {

            int cur, busy;
            if (s_scene_wtx_is_player) {
                cur = (int)wXCoord;
                busy = Player_IsMoving();
            } else if (s_scene_wtx_actor >= 0 && s_scene_actors[s_scene_wtx_actor].used) {
                int tx, ty;
                NPC_GetTilePos(s_scene_actors[s_scene_wtx_actor].npc_idx, &tx, &ty);
                cur = tx;
                busy = NPC_IsWalking(s_scene_actors[s_scene_wtx_actor].npc_idx);
            } else {

                s_scene_wtx_active = 0;
                if (s_scene_wtx_is_player) gScriptedMovement = 0;
                s_scene_pc++;
                cur = 0; busy = 0;
            }
            if (s_scene_wtx_active) {
                if (busy) {

                } else if (cur == s_scene_wtx_target) {
                    s_scene_wtx_active = 0;

                    if (s_scene_wtx_is_player) gScriptedMovement = 0;
                    s_scene_pc++;
                } else {
                    int dir = (cur < s_scene_wtx_target) ? 3  : 2 ;
                    if (s_scene_wtx_is_player) Player_DoScriptedStepWithLedge(dir);
                    else NPC_DoScriptedStep(s_scene_actors[s_scene_wtx_actor].npc_idx, dir);
                }
            }
        } else if (s_scene_march_active) {

            int any_npc_walking = 0;
            int busy[SCENE_ACTOR_MAX + 1];
            for (int i = 0; i < SCENE_ACTOR_MAX + 1; i++) {
                if (!s_march[i].used) { busy[i] = 0; continue; }
                busy[i] = scene_march_busy(&s_march[i]);
                if (!s_march[i].is_player && busy[i] && s_march[i].idx < s_march[i].len)
                    any_npc_walking = 1;
            }

            int dbg_dispatched = 0;
            for (int i = 0; i < SCENE_ACTOR_MAX + 1; i++) {
                if (!s_march[i].used || s_march[i].is_player) continue;
                if (!busy[i] && s_march[i].idx < s_march[i].len) {
                    if (s_march_dbg_frame >= 0)
                        march_dbg_log("[march-dbg] f=%03d DISPATCH %s -> %s\n", s_march_dbg_frame,
                               s_march[i].actor, march_dir_name(s_march[i].steps[s_march[i].idx]));
                    scene_march_dispatch(&s_march[i]);
                    dbg_dispatched = 1;
                }
            }

            for (int i = 0; i < SCENE_ACTOR_MAX + 1; i++) {
                if (!s_march[i].used || !s_march[i].is_player) continue;
                if (!busy[i] && !any_npc_walking && s_march[i].idx < s_march[i].len) {
                    if (s_march_dbg_frame >= 0)
                        march_dbg_log("[march-dbg] f=%03d DISPATCH %s -> %s\n", s_march_dbg_frame,
                               s_march[i].actor, march_dir_name(s_march[i].steps[s_march[i].idx]));
                    scene_march_dispatch(&s_march[i]);
                    dbg_dispatched = 1;
                }
            }
            if (dbg_dispatched) { scene_march_dbg_snapshot("after"); scene_march_anim_frame("after"); }
            if (s_march_dbg_frame >= 0) s_march_dbg_frame++;

            int all_done = 1;
            for (int i = 0; i < SCENE_ACTOR_MAX + 1; i++) {
                if (!s_march[i].used) continue;
                if (s_march[i].idx < s_march[i].len || scene_march_busy(&s_march[i])) {
                    all_done = 0;
                    break;
                }
            }
            if (all_done) {
                if (s_march_dbg_frame >= 0) {
                    march_dbg_log("[march-dbg] f=%03d DONE\n", s_march_dbg_frame);
                    s_march_dbg_frame = -1;
                }
                scene_march_clear();
                s_scene_pc++;
            }
        } else if (s_scene_sim_walk_active) {

            if (!Player_IsSimulatingMovement() && !Player_IsMoving()) {
                s_scene_sim_walk_active = 0;
                s_scene_pc++;
            }
        } else if (s_scene_move_steps_left > 0 || s_scene_move_awaiting_stop) {

            if (s_scene_move_actor < 0 || s_scene_move_actor >= SCENE_ACTOR_MAX || !s_scene_actors[s_scene_move_actor].used) {

                s_scene_move_steps_left = 0;
                s_scene_move_awaiting_stop = 0;
                s_scene_pc++;
            } else {
                int npc_idx = s_scene_actors[s_scene_move_actor].npc_idx;
                if (npc_idx < 0 || npc_idx >= NPC_GetCount()) {
                    s_scene_move_steps_left = 0;
                    s_scene_move_awaiting_stop = 0;
                    s_scene_pc++;
                } else {
                    if (!NPC_IsWalking(npc_idx) && s_scene_move_steps_left > 0) {

                        extern int Game_IsOverworldTickActive(void);
                        if (Game_IsOverworldTickActive()) {
                            NPC_DoScriptedStep(npc_idx, s_scene_move_dir);
                            s_scene_move_steps_left--;
                            if (s_scene_move_steps_left <= 0) s_scene_move_awaiting_stop = 1;
                        }
                    }

                    if (s_scene_move_awaiting_stop && !NPC_IsWalking(npc_idx)) {
                        s_scene_move_awaiting_stop = 0;
                        s_scene_pc++;
                    }
                }
            }
        } else {

            for (int scene_run_guard = 0; scene_run_guard < 512; scene_run_guard++) {
                if (s_scene_pc < 0 || s_scene_pc >= s_scene_cmd_count) {
                    s_scene_active = 0;
                    wJoyIgnore = 0;
                    gScriptedMovement = 0;
                    BagListChoice_ClearHeld();
                    printf("[scene] finished\n");
                    break;
                }
                int scene_pc_before = s_scene_pc;
                scene_cmd_t *cmdp = &s_scene_cmds[s_scene_pc];

                Trace_Emit(TRACE_SCENE,
                    "\"pc\":%d,\"op\":%d,\"actor\":\"%.20s\",\"a\":%d,\"b\":%d",
                    s_scene_pc, (int)cmdp->op, cmdp->actor, cmdp->a, cmdp->b);
                switch (cmdp->op) {
                    case SCOP_SPAWN: {
                        int idx = -1;
                        int spawned_new = 0;

                        int cx = cmdp->b, cy = cmdp->c;
                        AmberScript_MapNpcResolveRuntime(wCurMap, cmdp->b, cmdp->c, &cx, &cy);
                        int ai_prev = scene_find_actor(cmdp->actor);
                        if (ai_prev >= 0 && s_scene_actors[ai_prev].used) {
                            idx = s_scene_actors[ai_prev].npc_idx;
                        }
                        if (idx < 0) {
                            idx = NPC_FindAtTile(cx, cy);
                        }
                        if (idx < 0) {

                            idx = AmberScript_MapFindLiveNpcByDeclaredTile(
                                      (int)wCurMap, cmdp->b, cmdp->c);
                            if (idx >= 0) {

                                if (NPC_IsHidden(idx)) {
                                    NPC_ShowSprite(idx);
                                    scene_persist_hidden(idx, 0);
                                }
                                NPC_GetTilePos(idx, &cx, &cy);
                                printf("[amberscript] spawn '%s': nothing at (%d,%d); bound the "
                                       "npc/trainer DECLARED there, now at (%d,%d)\n",
                                       cmdp->actor, cmdp->b, cmdp->c, cx, cy);
                                fflush(stdout);
                            }
                        }
                        if (idx < 0) {
                            idx = NPC_DebugSpawn((uint8_t)cmdp->a, cx, cy, 0, 0);
                            if (idx >= 0) spawned_new = 1;

                            printf("[amberscript] spawn '%s': no npc or trainer is DECLARED at "
                                   "(%d,%d) on map %u -- created a NEW sprite. If you meant an "
                                   "existing map character, check that tile against the .block "
                                   "declaration; if you meant a scene-only extra, ignore this.\n",
                                   cmdp->actor, cmdp->b, cmdp->c, (unsigned)wCurMap);
                            fflush(stdout);
                        }
                        {
                            int ai = scene_add_actor(cmdp->actor, idx);
                            if (ai >= 0) {
                                if (spawned_new) s_scene_actors[ai].spawned_by_scene = 1;
                                if (cmdp->d) s_scene_actors[ai].persist = 1;
                                s_scene_actors[ai].sprite_id = (uint8_t)cmdp->a;
                                s_scene_actors[ai].last_x = cx;
                                s_scene_actors[ai].last_y = cy;

                                if (!spawned_new) {
                                    s_scene_actors[ai].has_key = 1;
                                    s_scene_actors[ai].key_x = cmdp->b;
                                    s_scene_actors[ai].key_y = cmdp->c;
                                }

                                if (!spawned_new && idx >= 0 && s_scene_frozen_npc < 0) {
                                    s_scene_frozen_npc = idx;
                                    s_scene_frozen_npc_movetype = NPC_GetMoveType(idx);
                                    s_scene_frozen_map = Map_CurrentRealId();
                                    s_scene_frozen_sprite = (int)NPC_GetSpriteId(idx);
                                    NPC_SetMoveType(idx, 0);
                                }
                            }
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_WARP: {

                        Warp_PlayMapChangeSound();
                        Warp_QueueTeleportVmap(cmdp->text, cmdp->b, cmdp->c);
                        printf("[amberscript] warp: -> %s (%d,%d)\n",
                               cmdp->text, cmdp->b, cmdp->c);
                        fflush(stdout);
                        s_scene_pc++;
                    } break;
                    case SCOP_WARP_PAD: {

                        Warp_PlayMapChangeSound();
                        Warp_QueueTeleportPadVmap(cmdp->text, cmdp->b, cmdp->c);
                        printf("[amberscript] warp_pad: -> %s (%d,%d)\n",
                               cmdp->text, cmdp->b, cmdp->c);
                        fflush(stdout);
                        s_scene_pc++;
                    } break;
                    case SCOP_DESPAWN: {
                        int ai = scene_find_actor(cmdp->actor);
                        if (ai >= 0) NPC_DebugDespawn(s_scene_actors[ai].npc_idx);
                        if (ai >= 0) {
                            s_scene_actors[ai].spawned_by_scene = 0;

                            s_scene_actors[ai].has_key = 0;
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_FACE: {

                        if (strcmp(cmdp->actor, "player") == 0) {

                            if (cmdp->a >= 0 && cmdp->a <= 3) {
                                gPlayerFacing = cmdp->a;
                                Player_SyncOAM();
                            }
                            s_scene_pc++;
                            break;
                        }
                        if (cmdp->actor[0] == '@') {

                            int fx = -1, fy = -1;
                            if (sscanf(cmdp->actor + 1, "%d,%d", &fx, &fy) == 2) {
                                int idx = NPC_FindAtTile(fx, fy);
                                if (idx >= 0) {
                                    extern int Game_IsOverworldTickActive(void);
                                    if (!Game_IsOverworldTickActive()) break;
                                    if (cmdp->a == -2) NPC_FacePlayer(idx);
                                    else if (cmdp->a >= 0 && cmdp->a <= 3) NPC_SetFacing(idx, cmdp->a);

                                    NPC_BuildView(gScrollPxX, gScrollPxY);
                                }
                            }
                            s_scene_pc++;
                            break;
                        }
                        {
                            int ai = scene_find_actor(cmdp->actor);
                            if (ai < 0) { s_scene_pc++; break; }

                            extern int Game_IsOverworldTickActive(void);
                            if (!Game_IsOverworldTickActive()) break;
                            int dir = cmdp->a;
                            if (dir == -2) {
                                int ox, oy;
                                NPC_GetTilePos(s_scene_actors[ai].npc_idx, &ox, &oy);
                                printf("[amberscript] face '%s' player: actor=(%d,%d) player=(%d,%d)\n",
                                       cmdp->actor, ox, oy, (int)wXCoord, (int)wYCoord);
                                fflush(stdout);
                                NPC_FacePlayer(s_scene_actors[ai].npc_idx);
                            } else {
                                NPC_SetFacing(s_scene_actors[ai].npc_idx, dir);
                            }

                            NPC_BuildView(gScrollPxX, gScrollPxY);
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_MOVE: {

                        int ai = scene_find_actor(cmdp->actor);
                        if (ai < 0 && cmdp->actor[0] == '@') {
                            int fx = -1, fy = -1, idx = -1;
                            if (sscanf(cmdp->actor + 1, "%d,%d", &fx, &fy) == 2) {
                                idx = NPC_FindAtTile(fx, fy);
                            }
                            if (idx >= 0) ai = scene_add_actor(cmdp->actor, idx);
                        }
                        if (ai < 0) { s_scene_pc++; break; }
                        s_scene_move_actor = ai;
                        s_scene_move_dir = cmdp->a;
                        s_scene_move_steps_left = (cmdp->b < 0) ? 0 : cmdp->b;
                        s_scene_move_awaiting_stop = 0;
                        if (s_scene_move_steps_left <= 0) s_scene_pc++;
                    } break;
                    case SCOP_MOVE_TO_PLAYER: {

                        int ai = scene_find_actor(cmdp->actor);
                        int ax = -1, ay = -1;
                        if (ai >= 0 && s_scene_actors[ai].used) {
                            int npc_idx = s_scene_actors[ai].npc_idx;
                            if (npc_idx >= 0 && npc_idx < NPC_GetCount())
                                NPC_GetTilePos(npc_idx, &ax, &ay);
                        }
                        if (ax >= 0 && ay >= 0) scene_insert_move_to_player_moves(cmdp->actor, ax, ay);
                        s_scene_pc++;
                    } break;
                    case SCOP_QUEUE: {

                        scene_march_t *q = scene_march_get(cmdp->actor);
                        if (q) {
                            if (cmdp->a == SCENE_STEP_FACE) {
                                if (q->len < SCENE_MARCH_STEPS_MAX)
                                    q->steps[q->len++] = SCENE_MARCH_FACE_BASE + (cmdp->b & 3);
                            } else {
                                for (int k = 0; k < cmdp->b && q->len < SCENE_MARCH_STEPS_MAX; k++)
                                    q->steps[q->len++] = cmdp->a;
                            }
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_MARCH: {

                        int any = 0;
                        for (int i = 0; i < SCENE_ACTOR_MAX + 1; i++)
                            if (s_march[i].used && s_march[i].len > 0) { any = 1; break; }
                        if (any) {
                            s_scene_march_active = 1;
                            gScriptedMovement = 1;
                            printf("[amberscript] march: begin, player=(%d,%d)\n",
                                   (int)wXCoord, (int)wYCoord);
                            if (s_march_dbg_enabled) {
                                s_march_dbg_frame = 0;
                                for (int i = 0; i < SCENE_ACTOR_MAX + 1; i++) {
                                    if (!s_march[i].used) continue;
                                    char line[512];
                                    int off = snprintf(line, sizeof(line), "[march-dbg] queue %s (%d steps):",
                                                        s_march[i].actor, s_march[i].len);
                                    for (int k = 0; k < s_march[i].len && off < (int)sizeof(line); k++)
                                        off += snprintf(line + off, sizeof(line) - off, " %s",
                                                         march_dir_name(s_march[i].steps[k]));
                                    march_dbg_log("%s\n", line);
                                }
                                scene_march_dbg_snapshot("start");
                                scene_march_anim_frame("start");
                            } else {
                                s_march_dbg_frame = -1;
                            }
                            fflush(stdout);
                        } else {
                            s_scene_pc++;
                        }
                    } break;
                    case SCOP_SIM_WALK: {

                        int n = cmdp->b;
                        if (n > (int)sizeof(s_scene_sim_walk_seq) - 1)
                            n = (int)sizeof(s_scene_sim_walk_seq) - 1;
                        for (int i = 0; i < n; i++) s_scene_sim_walk_seq[i] = (int8_t)cmdp->a;
                        s_scene_sim_walk_seq[n] = -1;
                        Player_StartSimulatedMovement(s_scene_sim_walk_seq, n - 1);
                        s_scene_sim_walk_active = 1;
                        printf("[amberscript] sim_walk: dir=%d count=%d player=(%d,%d)\n",
                               cmdp->a, n, (int)wXCoord, (int)wYCoord);
                    } break;
                    case SCOP_WALK_TO_X: {
                        s_scene_wtx_target = cmdp->a;
                        if (strcmp(cmdp->actor, "player") == 0) {
                            s_scene_wtx_is_player = 1;
                            s_scene_wtx_actor = -1;
                            gScriptedMovement = 1;
                        } else {
                            int ai = scene_find_actor(cmdp->actor);
                            if (ai < 0) { s_scene_pc++; break; }
                            s_scene_wtx_is_player = 0;
                            s_scene_wtx_actor = ai;
                        }
                        s_scene_wtx_active = 1;
                    } break;
                    case SCOP_SET_EVENT:
                        if (cmdp->b) SetEvent((uint16_t)cmdp->a);
                        else ClearEvent((uint16_t)cmdp->a);
                        s_scene_pc++;
                        break;
                    case SCOP_GIVE_BADGE:
                        Badge_Set(cmdp->a);
                        printf("[amberscript] give_badge bit=%d\n", cmdp->a);
                        s_scene_pc++;
                        break;
                    case SCOP_LET: {
                        int ok = 1;
                        int v = scene_eval(cmdp->text, &ok);
                        if (!ok)
                            printf("[amberscript] let %s: bad expression '%s'\n", cmdp->actor, cmdp->text);
                        scene_var_set(cmdp->actor, v);
                        if (s_script_trace_enabled) {
                            printf("[amberscript] let %s = %d\n", cmdp->actor, v);
                            fflush(stdout);
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_IF: {
                        int cond = scene_eval(cmdp->text, NULL);
                        if (!cond && cmdp->a >= 0 && cmdp->a <= s_scene_cmd_count)
                            s_scene_pc = cmdp->a;
                        else
                            s_scene_pc++;
                    } break;
                    case SCOP_JUMP:
                        s_scene_pc = (cmdp->a >= 0 && cmdp->a <= s_scene_cmd_count)
                                     ? cmdp->a : s_scene_cmd_count;
                        break;
                    case SCOP_GIVE: {
                        int sp = scene_eval(cmdp->text, NULL);
                        if (strcmp(cmdp->actor, "player") == 0 && sp > 0) {
                            Pokemon_AddToParty((uint8_t)sp, 5);
                            Pokedex_SetOwned(sp);
                            printf("[amberscript] give player species %d\n", sp);
                            fflush(stdout);
                        } else if (strcmp(cmdp->actor, "rival") == 0 && sp > 0) {

                            wRivalStarter = (uint8_t)sp;
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_HIDE: {
                        int hidx;
                        if (cmdp->a < 0 && cmdp->b < 0)
                            hidx = s_scene_ctx_npc;
                        else
                            hidx = NPC_FindAtTile(cmdp->a, cmdp->b);
                        if (hidx >= 0) { NPC_HideSprite(hidx); scene_persist_hidden(hidx, 1); }
                        s_scene_pc++;
                    } break;
                    case SCOP_SHOW: {
                        int sidx = NPC_FindAtTileIncludingHidden(cmdp->a, cmdp->b);
                        if (sidx >= 0) { NPC_ShowSprite(sidx); scene_persist_hidden(sidx, 0); }
                        else {

                            AmberScript_MapNpcSaveRuntime((int)wCurMap, cmdp->a, cmdp->b,
                                                          0, 0, 0, 0, 0);
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_ENGAGE_TRAINER: {

                        int eidx = NPC_FindAtTile(cmdp->a, cmdp->b);
                        printf("[amberscript] engage_trainer %d %d -> npc_idx=%d\n", cmdp->a, cmdp->b, eidx);
                        if (eidx >= 0) Trainer_EngageImmediate(eidx);
                        s_scene_wait_engage_pretext = 1;

                        s_scene_pc++;
                    } break;
                    case SCOP_EMOTE: {

                        if (strcmp(cmdp->actor, "player") == 0) {
                            s_scene_emote_npc = -2;
                            Emote_ShowOnPlayer();
                        } else {
                            int ai = scene_find_actor(cmdp->actor);
                            s_scene_emote_npc = (ai >= 0) ? s_scene_actors[ai].npc_idx : -1;
                            if (s_scene_emote_npc >= 0) Emote_ShowOnNPC(s_scene_emote_npc);
                        }

                        s_scene_wait_emote = 30;
                        s_scene_pc++;
                    } break;
                    case SCOP_HEAL:
                        Pokemon_HealParty();
                        s_scene_pc++;
                        break;
                    case SCOP_FADE_OUT_WHITE:
                    case SCOP_FADE_IN_WHITE:
                    case SCOP_FADE_OUT_BLACK:
                    case SCOP_FADE_IN_BLACK: {

                        int out = (cmdp->op == SCOP_FADE_OUT_WHITE || cmdp->op == SCOP_FADE_OUT_BLACK);
                        int black = (cmdp->op == SCOP_FADE_OUT_BLACK || cmdp->op == SCOP_FADE_IN_BLACK);
                        s_scene_fade_active = black ? (out ? 3 : 4) : (out ? 1 : 2);
                        s_scene_fade_step   = 0;
                        s_scene_fade_steps  = black ? 4 : 3;
                        s_scene_fade_timer  = SCENE_FADE_TICKS_PER_STEP;
                        const uint8_t (*tbl)[3] = black
                            ? (out ? kSceneFadeOutToBlack : kSceneFadeInFromBlack)
                            : (out ? kSceneFadeOutToWhite : kSceneFadeInFromWhite);
                        Display_SetPalette(tbl[0][0], tbl[0][1], tbl[0][2]);
                        printf("[scene] fade %s %s\n", out ? "out" : "in", black ? "black" : "white");
                        break;
                    }
                    case SCOP_TRADE_CUSTOM: {
                        if (Trade_BeginCustom((uint8_t)cmdp->a, (uint8_t)cmdp->b,
                                              cmdp->text))
                            s_scene_wait_trade = 1;
                        s_scene_pc++;
                        break;
                    }
                    case SCOP_TRADE: {
                        int which = scene_eval(cmdp->text, NULL);
                        if (which >= 0 && Trade_Begin((uint8_t)which)) {
                            s_scene_wait_trade = 1;
                        }
                        s_scene_pc++;
                        break;
                    }
                    case SCOP_SHOW_DEX: {
                        int sp = scene_eval(cmdp->text, NULL);
                        if (sp > 0) {

                            uint8_t saved0 = wPokedexOwned[0];
                            wPokedexOwned[0] |= (1u << 0) | (1u << 1) | (1u << 3) | (1u << 6);
                            Pokedex_ShowData(gSpeciesToDex[sp & 0xFF]);
                            wPokedexOwned[0] = saved0;
                            s_scene_wait_dex = 1;
                            s_scene_wait_dex_delay = 10;
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_SHOW_TOWNMAP: {

                        TownMap_Open();
                        s_scene_wait_townmap = 1;
                        s_scene_pc++;
                    } break;
                    case SCOP_SHIP_DEPART: {
                        SSAnneDepart_Start();
                        s_scene_wait_ship_depart = 1;
                        s_scene_pc++;
                    } break;
                    case SCOP_SHOW_BLACKBOARD: {

                        Blackboard_Open();
                        s_scene_wait_blackboard = 1;
                        s_scene_pc++;
                    } break;
                    case SCOP_SHOW_LINK_CABLE_HELP: {

                        LinkCableHelp_Open();
                        s_scene_wait_link_cable_help = 1;
                        s_scene_pc++;
                    } break;
                    case SCOP_LIST_CHOICE: {

                        uint8_t ids[8];
                        int n = 0;
                        char buf[SCENE_TEXT_MAX];
                        snprintf(buf, sizeof(buf), "%s", cmdp->text);
                        char *save = NULL;
                        char *tok = strtok_r(buf, " ", &save);
                        while (tok && n < 8) {
                            int iid = pks_resolve_item_id(tok);
                            if (iid > 0) ids[n++] = (uint8_t)iid;
                            else printf("[amberscript] list_choice: unknown item '%s'\n", tok);
                            tok = strtok_r(NULL, " ", &save);
                        }
                        BagListChoice_Open(ids, n, cmdp->a);
                        s_scene_wait_list_choice = 1;
                        s_scene_pc++;
                    } break;
                    case SCOP_FOSSIL_SELECT: {

                        int item = scene_eval(cmdp->text, NULL);
                        if (item > 0) {
                            const char *mon = (item == ITEM_DOME_FOSSIL)  ? "KABUTO"
                                            : (item == ITEM_HELIX_FOSSIL) ? "OMANYTE"
                                                                          : "AERODACTYL";
                            int sp = pks_resolve_species_id(mon);
                            wFossilItem = (uint8_t)item;
                            wFossilMon  = (sp > 0) ? (uint8_t)sp : 0;
                            printf("[amberscript] fossil_select: item=%d -> %s (%d)\n",
                                   item, mon, sp);
                        } else {
                            printf("[amberscript] fossil_select: bad item '%s'\n", cmdp->text);
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_FOSSIL_NAMES:

                        Text_SetMonName(wFossilMon);
                        Text_SetNameBufferItem(wFossilItem);
                        s_scene_pc++;
                        break;
                    case SCOP_PRIZE_LIST: {

                        char buf[SCENE_TEXT_MAX];
                        snprintf(buf, sizeof(buf), "%s", cmdp->text);
                        char *save = NULL;
                        char *tok = strtok_r(buf, " ", &save);
                        int kind = PRIZE_KIND_MON;
                        uint8_t entries[3] = {0, 0, 0};
                        uint16_t prices[3] = {0, 0, 0};
                        if (tok && !strcmp(tok, "item")) kind = PRIZE_KIND_ITEM;
                        tok = strtok_r(NULL, " ", &save);

                        if (tok && !strcmp(tok, "rom")) {
                            tok = strtok_r(NULL, " ", &save);
                            int menu = tok ? (int)strtol(tok, NULL, 0) : 1;
                            const uint8_t *ent  = (menu == 2) ? gPrizeMon2Entries
                                                              : gPrizeMon1Entries;
                            const uint8_t *cost = (menu == 2) ? gPrizeMon2Cost
                                                              : gPrizeMon1Cost;
                            if (!ent || !cost) {
                                printf("[amberscript] prize_list rom %d: tables not bound\n", menu);
                            } else {
                                for (int i = 0; i < 3; i++) {
                                    entries[i] = ent[i];
                                    prices[i]  = pks_prize_bcd(cost[i * 2], cost[i * 2 + 1]);
                                }
                            }
                            for (int i = 0; i < 3; i++) {
                                s_prize_entries[i] = entries[i];
                                s_prize_prices[i]  = prices[i];
                            }
                            PrizeListChoice_Open(kind, entries, prices);
                            s_scene_wait_prize_list = 1;
                            s_scene_pc++;
                            break;
                        }
                        for (int i = 0; i < 3 && tok; i++) {
                            int id = (kind == PRIZE_KIND_ITEM)
                                    ? pks_resolve_item_id(tok)
                                    : pks_resolve_species_id(tok);
                            if (id <= 0)
                                printf("[amberscript] prize_list: unknown %s '%s'\n",
                                       (kind == PRIZE_KIND_ITEM) ? "item" : "species", tok);
                            entries[i] = (uint8_t)id;
                            tok = strtok_r(NULL, " ", &save);
                            prices[i] = tok ? (uint16_t)strtol(tok, NULL, 0) : 0;
                            tok = strtok_r(NULL, " ", &save);
                        }
                        for (int i = 0; i < 3; i++) {
                            s_prize_entries[i] = entries[i];
                            s_prize_prices[i]  = prices[i];
                        }
                        PrizeListChoice_Open(kind, entries, prices);
                        s_scene_wait_prize_list = 1;
                        s_scene_pc++;
                    } break;
                    case SCOP_SHOW_FOSSIL: {

                        Fossil_Show(s_scene_cmds[s_scene_pc].a);
                        s_scene_wait_fossil = 1;
                        s_scene_pc++;
                    } break;
                    case SCOP_SHOW_DEX_RATING:

                        if (!Text_IsOpen() && !s_scene_wait_say) {
                            snprintf(s_scene_text_buf, sizeof s_scene_text_buf,
                                     "%s", DexRating_Text());
                            s_scene_say_auto = 0;

                            Text_SetPendingSFX(DexRating_PlaySfx);
                            Text_ShowASCII(s_scene_text_buf);
                            s_scene_wait_say = 1;
                            s_scene_say_opened = Text_IsOpen() ? 1 : 0;
                            s_scene_pc++;
                        }
                        break;
                    case SCOP_BILLS_DEX_LIST:

                        BillsPokemonList_Open();
                        s_scene_wait_bills_dex_list = 1;
                        s_scene_pc++;
                        break;
                    case SCOP_DIPLOMA:

                        Diploma_Open();
                        s_scene_wait_diploma = 1;
                        s_scene_pc++;
                        break;
                    case SCOP_BADGE_HOUSE_MENU:

                        BadgeHouseMenu_Open();
                        s_scene_wait_badge_house = 1;
                        s_scene_pc++;
                        break;
                    case SCOP_ENTER_SAFARI:
                        SafariZoneScripts_Enter();
                        s_scene_pc++;
                        break;
                    case SCOP_LEAVE_SAFARI:
                        SafariZoneScripts_Leave();
                        s_scene_pc++;
                        break;
                    case SCOP_GYM_LEADER:

                        wGymLeaderNo = (uint8_t)cmdp->a;
                        s_scene_pc++;
                        break;
                    case SCOP_SHOW_MONEY:
                        if (cmdp->a) YesNo_ArmMoneyBox();
                        else MoneyBox_Draw();
                        s_scene_pc++;
                        break;
                    case SCOP_HIDE_MONEY:
                        MoneyBox_Clear();
                        s_scene_pc++;
                        break;
                    case SCOP_REFRESH_NPCS: {
                        extern void NPC_Load(void);
                        NPC_Load();
                        NPC_BuildView(gScrollPxX, gScrollPxY);
                        s_scene_pc++;
                    } break;
                    case SCOP_REFRESH_TILES:
                        Map_BuildScrollView();
                        s_scene_pc++;
                        break;
                    case SCOP_SERVICE:

                        s_scene_pc++;
                        if (!AmberScript_RunService(cmdp->text)) {
                            printf("[amberscript] scene service '%s' not resolved -- nothing ran\n",
                                   cmdp->text);
                            fflush(stdout);
                        }
                        break;
                    case SCOP_SHOW_COIN_BOX:
                        CoinBox_Draw();
                        s_scene_pc++;
                        break;
                    case SCOP_HIDE_COIN_BOX:
                        CoinBox_Clear();
                        s_scene_pc++;
                        break;
                    case SCOP_NAME: {
                        int sp = scene_eval(cmdp->text, NULL);
                        int slot = (int)wPartyCount - 1;
                        if (sp > 0 && slot >= 0 && slot < PARTY_LENGTH) {
                            NamingScreen_Open(NAME_MON_SCREEN, (uint8_t)sp, wPartyMonNicks[slot]);
                            s_scene_wait_name = 1;
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_SFX: {
                        void (*fn)(void) = pks_resolve_sfx_fn(cmdp->text);
                        if (fn) fn();
                        s_scene_pc++;
                    } break;
                    case SCOP_CRY:
                        Audio_PlayCry((uint8_t)cmdp->a);
                        s_scene_pc++;
                        break;
                    case SCOP_PLACE: {

                        int ai = scene_find_actor(cmdp->actor);
                        DBG_PRINTF("[PLACEDBG] place '%s' -> (%d,%d): ai=%d used=%d npc_idx=%d\n",
                               cmdp->actor, cmdp->a, cmdp->b, ai,
                               ai >= 0 ? s_scene_actors[ai].used : -1,
                               ai >= 0 ? s_scene_actors[ai].npc_idx : -1);
                        fflush(stdout);
                        if (ai >= 0 && s_scene_actors[ai].used &&
                            s_scene_actors[ai].npc_idx >= 0) {
                            NPC_SetTilePos(s_scene_actors[ai].npc_idx, cmdp->a, cmdp->b);
                            s_scene_actors[ai].last_x = cmdp->a;
                            s_scene_actors[ai].last_y = cmdp->b;
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_SFX_ON_CLOSE: {

                        void (*fn)(void) = pks_resolve_sfx_fn(cmdp->text);
                        if (fn) Text_SetPendingSFX(fn);
                        s_scene_pc++;
                    } break;
                    case SCOP_SFX_ON_PRINT: {

                        void (*fn)(void) = pks_resolve_sfx_fn(cmdp->text);
                        if (fn) Text_SetPendingSFXOnPrint(fn);
                        s_scene_pc++;
                    } break;
                    case SCOP_CRY_ON_PRINT:

                        s_pending_cry_species = (uint8_t)cmdp->a;
                        Text_SetPendingSFXOnPrint(pks_play_pending_cry);
                        s_scene_pc++;
                        break;
                    case SCOP_GIVE_ITEM: {
                        int iid = pks_resolve_item_id(cmdp->text);
                        if (iid > 0) {

                            s_scene_last_give_full = (Inventory_Add((uint8_t)iid, (uint8_t)cmdp->a) != 0) ? 1 : 0;

                            Text_SetItemName((uint8_t)iid);
                        } else {
                            printf("[amberscript] give_item: unknown item '%s'\n", cmdp->text);
                            s_scene_last_give_full = 0;
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_GIVE_POKEMON: {
                        int sp = scene_eval(cmdp->actor, NULL);
                        int lvl = scene_eval(cmdp->text, NULL);
                        if (sp > 0 && lvl > 0) {

                            Text_SetMonName((uint8_t)sp);
                            if (wPartyCount < PARTY_LENGTH) {
                                Pokemon_AddToParty((uint8_t)sp, (uint8_t)lvl);
                                Pokedex_SetOwned(sp);
                                s_scene_last_give_mon_full = 0;
                                s_scene_last_give_mon_boxed = 0;
                            } else if (Pokemon_AddToBox((uint8_t)sp, (uint8_t)lvl)) {

                                Text_SetBoxNumber((int)(wCurrentBoxNum % NUM_BOXES) + 1);

                                Pokedex_SetOwned(sp);
                                s_scene_last_give_mon_full = 0;
                                s_scene_last_give_mon_boxed = 1;
                            } else {

                                s_scene_last_give_mon_full = 1;
                                s_scene_last_give_mon_boxed = 0;
                                Text_ShowASCII(
                                    "There's no more\nroom for #MON!\f"
                                    "The #MON BOX\nis full and can't\naccept any more!\f"
                                    "Change the BOX at\na #MON CENTER!");
                            }
                        } else {
                            printf("[amberscript] give_monster: bad species/level '%s'/'%s'\n",
                                   cmdp->actor, cmdp->text);
                            s_scene_last_give_mon_full = 0;
                            s_scene_last_give_mon_boxed = 0;
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_TAKE_ITEM: {
                        int iid = pks_resolve_item_id(cmdp->text);

                        if (iid <= 0) iid = scene_eval(cmdp->text, NULL);
                        if (iid > 0) Inventory_Remove((uint8_t)iid, (uint8_t)cmdp->a);
                        else printf("[amberscript] take_item: unknown item '%s'\n", cmdp->text);
                        s_scene_pc++;
                    } break;
                    case SCOP_PAY: {
                        int amt = scene_eval(cmdp->text, NULL);
                        if (amt > 0) {
                            uint32_t cur = pks_money_get();
                            pks_money_set(((uint32_t)amt < cur) ? (cur - (uint32_t)amt) : 0);
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_TAKE_COINS: {
                        int amt = scene_eval(cmdp->text, NULL);
                        if (amt > 0) {
                            uint32_t cur = pks_coins_get();
                            pks_coins_set(((uint32_t)amt < cur) ? (cur - (uint32_t)amt) : 0);
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_GIVE_COINS: {
                        int amt = scene_eval(cmdp->text, NULL);
                        if (amt > 0) pks_coins_set(pks_coins_get() + (uint32_t)amt);
                        s_scene_pc++;
                    } break;
                    case SCOP_SAY:
                        if (!Text_IsOpen() && !s_scene_wait_say) {
                            scene_interp_text(cmdp->text, s_scene_text_buf, sizeof s_scene_text_buf);

                            s_scene_say_auto = (cmdp->a == 1) ? 1 : 0;

                            wDoNotWaitForButtonPress = s_scene_say_auto ? 1 : 0;
                            Text_ShowASCII(s_scene_text_buf);
                            s_scene_wait_say = 1;
                            s_scene_say_opened = Text_IsOpen() ? 1 : 0;
                            s_scene_pc++;
                        }
                        break;
                    case SCOP_SAY_HOLD:

                        if (!Text_IsOpen() && !s_scene_wait_print && !s_scene_wait_say) {
                            scene_interp_text(cmdp->text, s_scene_text_buf, sizeof s_scene_text_buf);
                            wDoNotWaitForButtonPress = 1;
                            Text_ShowASCII(s_scene_text_buf);
                            s_scene_wait_print = 1;
                            s_scene_print_opened = Text_IsOpen() ? 1 : 0;
                            s_scene_pc++;
                        }
                        break;
                    case SCOP_CLOSE_TEXT:

                        Text_Close();
                        s_scene_pc++;
                        break;
                    case SCOP_ASK:
                        if (!Text_IsOpen() && !YesNo_IsOpen()) {
                            s_scene_yesno_prev_joyignore = wJoyIgnore;
                            s_scene_yesno_restore_joyignore = 1;
                            s_scene_yesno_prev_scripted_movement = gScriptedMovement;
                            s_scene_yesno_restore_scripted_movement = 1;
                            gScriptedMovement = 1;
                            wJoyIgnore = (uint8_t)(wJoyIgnore & (uint8_t)~(PAD_UP | PAD_DOWN));
                            scene_interp_text(cmdp->text, s_scene_text_buf, sizeof s_scene_text_buf);
                            YesNo_Show(s_scene_text_buf);
                            s_scene_wait_yesno = 1;
                            s_scene_pc++;
                        }
                        break;
                    case SCOP_PRICED_CHOICE:
                        if (!Text_IsOpen() && !BikeShopMenu_IsOpen()) {
                            s_scene_yesno_prev_joyignore = wJoyIgnore;
                            s_scene_yesno_restore_joyignore = 1;
                            s_scene_yesno_prev_scripted_movement = gScriptedMovement;
                            s_scene_yesno_restore_scripted_movement = 1;
                            gScriptedMovement = 1;
                            wJoyIgnore = (uint8_t)(wJoyIgnore & (uint8_t)~(PAD_UP | PAD_DOWN));
                            BikeShopMenu_Show(cmdp->actor, cmdp->text, cmdp->a);
                            s_scene_wait_priced_choice = 1;
                            s_scene_pc++;
                        }
                        break;
                    case SCOP_BATTLESTART: {
                        if (cmdp->c == 1) {
                            scene_start_custom_trainer_battle(cmdp);
                            s_scene_wait_battle = 1;
                            s_scene_pc++;
                            printf("[amberscript] scene battlestart: custom team count=%u\n", (unsigned)cmdp->team_count);
                        } else {
                            int tc = cmdp->a;
                            int tn = cmdp->b;
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
                            s_scene_battlestart_noblackout = cmdp->d;
                            s_scene_battlestart_pending = 1;
                            s_scene_battlestart_saw_text = Text_IsOpen() ? 1 : 0;
                            s_scene_battlestart_delay = 0;
                        }
                    } break;
                    case SCOP_WILDBATTLE:

                        if (Text_IsOpen()) break;
                        s_scene_last_battle_result = -1;

                        if (cmdp->c == 1) Battle_RequestOldManType();
                        Game_StartWildBattleScripted((uint8_t)cmdp->a, (uint8_t)cmdp->b);
                        s_scene_wait_battle = 1;
                        s_scene_pc++;
                        printf("[amberscript] scene wildbattle: species=%d level=%d%s\n",
                               cmdp->a, cmdp->b, (cmdp->c == 1) ? " (oldman)" : "");
                        break;
                    case SCOP_BATTLEEND:
                        if (Game_GetScene() == 2 || BattleUI_IsActive()) break;
                        if (Game_IsWarpFadeActive()) break;
                        if (!Text_IsOpen()) {
                            Text_ShowASCII(cmdp->text);
                            s_scene_wait_battleend_text = 1;
                            s_scene_pc++;
                        }
                        break;
                    case SCOP_MUSIC:
                        Music_Play((uint8_t)cmdp->a);
                        s_scene_pc++;
                        break;
                    case SCOP_MUSIC_FROM_LOOP:
                        Music_PlayFromLoop((uint8_t)cmdp->a);
                        s_scene_pc++;
                        break;
                    case SCOP_MUSIC_RIVAL_ALT:
                        if (cmdp->a == 0) Music_PlayRivalAlternateStart();
                        else if (cmdp->a == 1) Music_PlayRivalAlternateTempo();
                        else Music_PlayRivalAlternateStartAndTempo();
                        s_scene_pc++;
                        break;
                    case SCOP_WAIT:
                        s_scene_wait = (cmdp->a < 0) ? 0 : cmdp->a;
                        s_scene_pc++;
                        break;
                    case SCOP_WAIT_TEXT:
                        if (!Text_IsOpen()) s_scene_pc++;
                        break;
                    case SCOP_WAIT_SFX:

                        if (!Audio_IsSFXPlaying()) s_scene_pc++;
                        break;
                    case SCOP_WAIT_CRY:

                        if (!Audio_IsCryPlaying()) s_scene_pc++;
                        break;
                    case SCOP_WAIT_MUSIC:

                        if (!Music_IsPlaying()) s_scene_pc++;
                        break;
                    case SCOP_LOCK_INPUT:

                        if (cmdp->a) hJoyHeld = 0;
                        wJoyIgnore = cmdp->a ? PAD_CTRL_PAD : 0;
                        s_scene_input_locked = cmdp->a ? 1 : 0;
                        printf("[amberscript] lock_input %d: player=(%d,%d) hJoyHeld=%02x moving=%d\n",
                               cmdp->a, (int)wXCoord, (int)wYCoord, hJoyHeld, Player_IsMoving());
                        fflush(stdout);
                        s_scene_pc++;
                        break;
                    case SCOP_FULLRATE:

                        s_scene_fullrate = cmdp->a ? 1 : 0;
                        s_scene_pc++;
                        break;
                    case SCOP_TILE_COPY:
                        AmberScript_TileCopy(cmdp->a, cmdp->b, cmdp->c, cmdp->d);
                        s_scene_pc++;
                        break;
                    case SCOP_TILE_SAVE:
                        AmberScript_TileSaveRightOfPlayer(cmdp->text);
                        s_scene_pc++;
                        break;
                    case SCOP_TILE_PLACE_CUSTOM:
                        AmberScript_TilePlaceCustom(cmdp->text, cmdp->a, cmdp->b);
                        s_scene_pc++;
                        break;
                    case SCOP_BLOCK_SAVE:
                        AmberScript_BlockSave(cmdp->text, cmdp->a, cmdp->b, cmdp->c, cmdp->d);
                        s_scene_pc++;
                        break;
                    case SCOP_BLOCK_PLACE_CUSTOM:
                        AmberScript_BlockPlaceCustom(cmdp->text, cmdp->a, cmdp->b);
                        s_scene_pc++;
                        break;
                    case SCOP_PY_AI:
                        PyAI_SetEnabled(cmdp->a, cmdp->text[0] ? cmdp->text : NULL);
                        printf("[scene] py_ai %s (%s)\n",
                               PyAI_IsEnabled() ? "on" : "off",
                               PyAI_GetScriptPath());
                        s_scene_pc++;
                        break;
                    case SCOP_PY_INJECT: {
                        int ninj = scene_exec_py_inject(cmdp->text);
                        printf("[scene] py_inject: %d cmd(s) injected\n", ninj);
                        s_scene_pc++;
                    } break;
                    case SCOP_PY_LAW:
                        s_py_law_enabled = cmdp->a ? 1 : 0;
                        s_py_law_npc_idx = cmdp->b;
                        s_py_law_frame_accum = 0;
                        s_py_law_elapsed_sec = 0;
                        if (cmdp->text[0]) snprintf(s_py_law_script, sizeof(s_py_law_script), "%s", cmdp->text);
                        printf("[scene] py_law %s npc=%d script=%s\n",
                               s_py_law_enabled ? "on" : "off",
                               s_py_law_npc_idx, s_py_law_script);
                        s_scene_pc++;
                        break;
                    case SCOP_PY_LAW_SPAWN: {
                        int idx = NPC_DebugSpawn((uint8_t)cmdp->a, cmdp->b, cmdp->c, 0, 0);
                        if (idx < 0) {
                            printf("[scene] py_law_spawn: failed to spawn npc\n");
                        } else {
                            s_py_law_enabled = 1;
                            s_py_law_npc_idx = idx;
                            s_py_law_frame_accum = 0;
                            s_py_law_elapsed_sec = 0;
                            snprintf(s_py_law_script, sizeof(s_py_law_script), "%s", cmdp->text);
                            printf("[scene] py_law_spawn: npc=%d at (%d,%d) script=%s\n",
                                   idx, cmdp->b, cmdp->c, s_py_law_script);
                        }
                        s_scene_pc++;
                    } break;
                    case SCOP_TYPEMOD:
                        scene_exec_typemod_line(cmdp->text);
                        s_scene_pc++;
                        break;
                    case SCOP_SPRITE_FRONT_LOAD:
                        if (SpriteMod_LoadFrontFromFile((uint8_t)cmdp->a, cmdp->text))
                            printf("[scene] sprite_front_load: species %d <- %s\n", cmdp->a, cmdp->text);
                        else
                            printf("[scene] sprite_front_load: failed for species %d from %s\n", cmdp->a, cmdp->text);
                        s_scene_pc++;
                        break;
                    case SCOP_SPRITE_BACK_LOAD:
                        if (SpriteMod_LoadBackFromFile((uint8_t)cmdp->a, cmdp->text))
                            printf("[scene] sprite_back_load: species %d <- %s\n", cmdp->a, cmdp->text);
                        else
                            printf("[scene] sprite_back_load: failed for species %d from %s\n", cmdp->a, cmdp->text);
                        s_scene_pc++;
                        break;
                    case SCOP_TILE_ART_LOAD:
                        if (AmberScript_LoadCustomTileArt(cmdp->actor, cmdp->text))
                            printf("[scene] tile_art_load: '%s' <- %s\n", cmdp->actor, cmdp->text);
                        else
                            printf("[scene] tile_art_load: failed for '%s' from %s\n", cmdp->actor, cmdp->text);
                        s_scene_pc++;
                        break;
                    case SCOP_END:

                        scene_persist_keyed_actors(s_scene_map);
                        s_scene_active = 0;
                        wJoyIgnore = 0;
                        gScriptedMovement = 0;

                        BagListChoice_ClearHeld();
                        printf("[scene] end\n");
                        break;
                    default:
                        s_scene_pc++;
                        break;
                }

                if (!s_scene_active) break;
                if (s_scene_wait_say || s_scene_wait_yesno || s_scene_wait_priced_choice || s_scene_wait_battle || s_scene_wait_engage_pretext ||
                    s_scene_wait_dex || s_scene_wait_townmap || s_scene_wait_ship_depart || s_scene_wait_blackboard || s_scene_wait_fossil || s_scene_wait_trade || s_scene_wait_name || s_scene_wait_bills_dex_list || s_scene_wait_badge_house ||
                    s_scene_wait_link_cable_help || s_scene_wait_list_choice || s_scene_wait_prize_list ||
                    s_scene_wait_print ||

                    s_scene_battlestart_pending || s_scene_wait_battleend_text ||
                    s_scene_wait > 0 || s_scene_fade_active || s_scene_wtx_active ||
                    s_scene_march_active ||
                    s_scene_move_steps_left > 0 || s_scene_move_awaiting_stop)
                    break;
                if (s_scene_pc == scene_pc_before) break;
            }
        }
    }

    if (Game_IsOverworldTickActive() && !s_scene_active && Game_GetScene() == 0 && s_scene_map_recheck_pending) {
        for (int i = 0; i < SCENE_TRIGGER_MAX; i++) {
            scene_trigger_t *t = &s_scene_triggers[i];
            if (!t->used || !t->is_onload) continue;
            if ((int)t->map_id != (int)wCurMap) continue;
            s_scene_map_recheck_pending = 0;
            if (t->cond_kind == 1 && !CheckEvent(t->cond_event)) break;
            if (t->cond_kind == 2 && CheckEvent(t->cond_event)) break;
            if (t->cond_kind2 == 1 && !CheckEvent(t->cond_event2)) break;
            if (t->cond_kind2 == 2 && CheckEvent(t->cond_event2)) break;
            scene_reset_runtime();
            int ncmd = scene_load_file(t->scene);
            if (ncmd > 0) {
                s_scene_active = 1;
                s_scene_pc = 0;
                printf("[amberscript] scene_trigger: fired (onload) '%s' map=%u\n",
                       t->scene, (unsigned)wCurMap);
                fflush(stdout);
            }
            break;
        }
    }

    if (Game_IsOverworldTickActive() && !s_scene_active && Game_GetScene() == 0 &&
        !Player_IsMoving() && !Text_IsOpen()) {
        for (int i = 0; i < SCENE_TRIGGER_MAX; i++) {
            scene_trigger_t *t = &s_scene_triggers[i];
            if (!t->used || !t->is_watch) continue;
            if ((int)t->map_id != (int)wCurMap) continue;
            if (t->cond_kind == 1 && !CheckEvent(t->cond_event)) continue;
            if (t->cond_kind == 2 && CheckEvent(t->cond_event)) continue;
            if (t->cond_kind2 == 1 && !CheckEvent(t->cond_event2)) continue;
            if (t->cond_kind2 == 2 && CheckEvent(t->cond_event2)) continue;
            scene_reset_runtime();
            {
                int ncmd = scene_load_file(t->scene);
                if (ncmd > 0) {
                    s_scene_active = 1;
                    s_scene_pc = 0;
                    printf("[amberscript] scene_trigger: fired (watch) '%s' map=%u\n",
                           t->scene, (unsigned)wCurMap);
                    fflush(stdout);
                }
            }
            break;
        }
    }

    if (Game_IsOverworldTickActive() && !s_scene_active && Game_GetScene() == 0 && !Player_IsMoving()) {
        scene_zone_latches_step();
    }

    if (Game_IsOverworldTickActive() && !s_scene_active && Game_GetScene() == 0 &&
        !Player_IsMoving() && !Text_IsOpen() && wIsInBattle == 0 &&

        !Game_IsWarpFadeActive()) {
        const char *pend = Trainer_PeekPendingAfterBattleScene();
        if (pend) {
            char scene[64];
            snprintf(scene, sizeof(scene), "%s", pend);
            Trainer_ClearPendingAfterBattleScene();
            scene_reset_runtime();
            if (scene_load_file(scene) > 0) {
                s_scene_active = 1;
                s_scene_pc = 0;
                hJoyHeld = 0;
                wJoyIgnore = PAD_CTRL_PAD;
                printf("[amberscript] after_battle: running '%s'\n", scene);
                fflush(stdout);
            } else {
                printf("[amberscript] after_battle: failed to load '%s'\n", scene);
                fflush(stdout);
            }
        }
    }

    if (Game_IsOverworldTickActive() && !s_scene_active && Game_GetScene() == 0 && !Player_IsMoving() && !Text_IsOpen()) {
        for (int i = 0; i < SCENE_TRIGGER_MAX; i++) {
            scene_trigger_t *t = &s_scene_triggers[i];
            if (!t->used) continue;
            if ((int)t->map_id != (int)wCurMap) continue;
            if (t->cond_kind == 1 && !CheckEvent(t->cond_event)) continue;
            if (t->cond_kind == 2 && CheckEvent(t->cond_event)) continue;
            if (t->cond_kind2 == 1 && !CheckEvent(t->cond_event2)) continue;
            if (t->cond_kind2 == 2 && CheckEvent(t->cond_event2)) continue;
            if ((t->x < 0 || (int)wXCoord == t->x) && (t->y < 0 || (int)wYCoord == t->y)) {

                if (!t->armed && t->x < 0 && t->y < 0 &&
                    ((int)wXCoord != t->fired_x || (int)wYCoord != t->fired_y)) {
                    t->armed = 1;
                }
                if (!t->armed) continue;
                scene_reset_runtime();
                int ncmd = scene_load_file(t->scene);
                if (ncmd > 0) {
                    s_scene_active = 1;
                    s_scene_pc = 0;
                    t->armed = 0;
                    t->fired_x = (int)wXCoord;
                    t->fired_y = (int)wYCoord;

                    hJoyHeld = 0;
                    wJoyIgnore = PAD_CTRL_PAD;
                    printf("[amberscript] scene_trigger: fired '%s' @ (%d,%d) player=(%d,%d) hJoyHeld=%02x wJoyIgnore=%02x moving=%d\n",
                           t->scene, t->x, t->y, (int)wXCoord, (int)wYCoord, hJoyHeld, wJoyIgnore, Player_IsMoving());
                    fflush(stdout);
                } else {
                    t->armed = 0;
                    printf("[amberscript] scene_trigger: failed to load '%s'\n", t->scene);
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
            script_trace_emitf("[script_trace] map=%u (%s)", map_now, gMapTable[map_now].name);
            s_trace_prev_map = map_now;
        }
        if (trainer_now != s_trace_prev_trainer_engaging) {
            script_trace_emitf("[script_trace] trainer_engage=%u", trainer_now);
            s_trace_prev_trainer_engaging = trainer_now;
        }
        if (text_now != s_trace_prev_text_open) {
            script_trace_emitf("[script_trace] text_open=%u", text_now);
            s_trace_prev_text_open = text_now;
        }
        if (gate_now != s_trace_prev_gate_active) {
            script_trace_emitf("[script_trace] gate_active=%u", gate_now);
            s_trace_prev_gate_active = gate_now;
        }
        if (r22_now != s_trace_prev_route22_active) {
            script_trace_emitf("[script_trace] route22_active=%u", r22_now);
            s_trace_prev_route22_active = r22_now;
        }
        if (r24_now != s_trace_prev_route24_active) {
            script_trace_emitf("[script_trace] route24_active=%u", r24_now);
            s_trace_prev_route24_active = r24_now;
        }
        if (ss_now != s_trace_prev_ssanne_active) {
            script_trace_emitf("[script_trace] ssanne_active=%u", ss_now);
            s_trace_prev_ssanne_active = ss_now;
        }
        if (vm_now != s_trace_prev_viridian_mart_active) {
            script_trace_emitf("[script_trace] viridian_mart_active=%u", vm_now);
            s_trace_prev_viridian_mart_active = vm_now;
        }
        if (gym_now != s_trace_prev_gym_active) {
            script_trace_emitf("[script_trace] gym_active=%u", gym_now);
            s_trace_prev_gym_active = gym_now;
        }
        if (rb4f_now != s_trace_prev_rockethideout_b4f_active) {
            script_trace_emitf("[script_trace] rocket_b4f_active=%u", rb4f_now);
            s_trace_prev_rockethideout_b4f_active = rb4f_now;
        }
    }
}

void AmberScript_Scene_OnBattleOutcome(int outcome) {
    if (!s_scene_active) return;
    s_scene_last_battle_result = outcome;
}

void AmberScript_Scene_Abort(void) {
    if (!s_scene_active) return;

    scene_reset_runtime();
}

int AmberScript_OnNpcInteracted(int npc_idx) {
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
        printf("[amberscript] scene_npc interact: failed to load scene '%s'\n",
               s_scene_npc_bindings[bi].scene);
        return 1;
    }
    s_scene_active = 1;
    s_scene_pc = 0;
    s_scene_ctx_npc = npc_idx;

    s_scene_frozen_npc = npc_idx;
    s_scene_frozen_npc_movetype = NPC_GetMoveType(npc_idx);
    s_scene_frozen_map = Map_CurrentRealId();
    s_scene_frozen_sprite = (int)NPC_GetSpriteId(npc_idx);
    NPC_SetMoveType(npc_idx, 0);
    printf("[amberscript] scene_npc interact: '%s' -> scene '%s' (%d command(s))\n",
           s_scene_npc_bindings[bi].name, s_scene_npc_bindings[bi].scene, ncmd);
    return 1;
}

int AmberScript_Scene_TryHandle(const char *cmd, const char *verb, int n) {
    (void)n;

    if (strcmp(verb, "dsl_bank") == 0) {
        char sub[16] = {0};
        if (!AmberScript_ParseArg(cmd, 1, sub, sizeof(sub))) strcpy(sub, "status");
        if (strcmp(sub, "on") == 0) {
            s_dsl_bank_enabled = 1;
            dsl_bank_load();
            dsl_bank_mark_runtime_tiles_banked();
            dsl_bank_ensure_current_map_spawns();
            dsl_bank_ensure_current_map_tiles();
            s_dsl_bank_last_map = wCurMap;
            dsl_bank_write_cfg();
            dsl_bank_save();
            printf("[amberscript] dsl_bank: ON (persisting scene NPCs, custom tiles, and tile placements)\n");
        } else if (strcmp(sub, "off") == 0) {
            s_dsl_bank_enabled = 0;
            dsl_bank_write_cfg();
            printf("[amberscript] dsl_bank: OFF\n");
        } else if (strcmp(sub, "save") == 0) {
            dsl_bank_mark_runtime_tiles_banked();
            dsl_bank_save();
            printf("[amberscript] dsl_bank: saved\n");
        } else if (strcmp(sub, "load") == 0) {
            dsl_bank_load();
            dsl_bank_ensure_current_map_spawns();
            dsl_bank_ensure_current_map_tiles();
            s_dsl_bank_last_map = wCurMap;
            printf("[amberscript] dsl_bank: loaded\n");
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
            printf("[amberscript] dsl_bank: cleared\n");
        } else if (strcmp(sub, "status") == 0) {
            int npc_cnt = 0, tile_cnt = 0, custom_tile_cnt = 0, custom_block_cnt = 0;
            for (int i = 0; i < SCENE_ACTOR_MAX; i++) if (s_scene_npc_bindings[i].used) npc_cnt++;
            for (int i = 0; i < SCENE_TILE_PROP_MAX; i++) if (s_scene_tile_props[i].used && s_scene_tile_props[i].banked) tile_cnt++;
            for (int i = 0; i < SCENE_SAVED_TILE_MAX; i++) if (s_scene_saved_tiles[i].used) custom_tile_cnt++;
            for (int i = 0; i < SCENE_SAVED_BLOCK_MAX; i++) if (s_scene_saved_blocks[i].used) custom_block_cnt++;
            printf("[amberscript] dsl_bank: %s, npcs=%d, tile_placements=%d, custom_tiles=%d, custom_blocks=%d\n",
                   s_dsl_bank_enabled ? "ON" : "OFF", npc_cnt, tile_cnt, custom_tile_cnt, custom_block_cnt);
        } else {
            printf("[amberscript] dsl_bank usage: dsl_bank on|off|status|save|load|clear\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "script_trace") == 0) {
        char sub[16] = {0};
        if (!AmberScript_ParseArg(cmd, 1, sub, sizeof(sub))) strcpy(sub, "status");
        if (strcmp(sub, "on") == 0) {
            s_script_trace_enabled = 1;
            s_script_trace_to_file = 1;
            script_trace_reset_latches();
            printf("[amberscript] script_trace: ON (file=%s)\n", PKS_SCRIPT_TRACE_LOG_PATH);
        } else if (strcmp(sub, "off") == 0) {
            s_script_trace_enabled = 0;
            s_script_trace_to_file = 0;
            printf("[amberscript] script_trace: OFF\n");
        } else if (strcmp(sub, "file_on") == 0) {
            s_script_trace_to_file = 1;
            printf("[amberscript] script_trace: file logging ON (%s)\n", PKS_SCRIPT_TRACE_LOG_PATH);
        } else if (strcmp(sub, "file_off") == 0) {
            s_script_trace_to_file = 0;
            printf("[amberscript] script_trace: file logging OFF\n");
        } else if (strcmp(sub, "status") == 0) {
            printf("[amberscript] script_trace: %s, file=%s\n",
                   s_script_trace_enabled ? "ON" : "OFF",
                   s_script_trace_to_file ? "ON" : "OFF");
        } else {
            printf("[amberscript] script_trace usage: script_trace on|off|status|file_on|file_off\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "pylaw") == 0) {
        char mode[16] = {0};
        int idx = -1;
        char script[120] = {0};
        if (sscanf(cmd, "%*s %15s %d %119s", mode, &idx, script) < 1) {
            printf("[amberscript] pylaw usage: pylaw on <npc_idx> <script> | pylaw off | pylaw status\n");
            AmberScript_WriteState();
            return 1;
        }
        if (strcmp(mode, "on") == 0) {
            if (idx < 0 || idx >= NPC_GetCount()) {
                printf("[amberscript] pylaw: npc_idx out of range\n");
            } else {
                s_py_law_enabled = 1;
                s_py_law_npc_idx = idx;
                s_py_law_frame_accum = 0;
                s_py_law_elapsed_sec = 0;
                snprintf(s_py_law_script, sizeof(s_py_law_script), "%s", script);
                printf("[amberscript] pylaw on npc=%d script=%s\n", idx, s_py_law_script);
            }
        } else if (strcmp(mode, "off") == 0) {
            s_py_law_enabled = 0;
            printf("[amberscript] pylaw off\n");
        } else {
            printf("[amberscript] pylaw status: %s npc=%d script=%s elapsed=%u\n",
                   s_py_law_enabled ? "on" : "off", s_py_law_npc_idx, s_py_law_script,
                   (unsigned)s_py_law_elapsed_sec);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "pylaw_spawn") == 0) {
        char sprite[24] = {0};
        char x[32] = {0}, y[32] = {0}, script[120] = {0};
        int sx, sy, spr, idx;
        if (sscanf(cmd, "%*s %23s %31s %31s %119s", sprite, x, y, script) != 4) {
            printf("[amberscript] pylaw_spawn usage: pylaw_spawn <sprite> <x_expr> <y_expr> <script>\n");
            AmberScript_WriteState();
            return 1;
        }
        spr = pks_parse_sprite(sprite);
        if (spr <= 0) {
            printf("[amberscript] pylaw_spawn: bad sprite '%s'\n", sprite);
            AmberScript_WriteState();
            return 1;
        }
        sx = pks_is_numeric_token(x) ? (int)strtol(x, NULL, 0) : 0;
        sy = pks_is_numeric_token(y) ? (int)strtol(y, NULL, 0) : 0;
        if (!pks_is_numeric_token(x) && strncmp(x, "player+", 7) == 0) sx = (int)wXCoord + (int)strtol(x + 7, NULL, 0);
        if (!pks_is_numeric_token(y) && strncmp(y, "player+", 7) == 0) sy = (int)wYCoord + (int)strtol(y + 7, NULL, 0);
        idx = NPC_DebugSpawn((uint8_t)spr, sx, sy, 0, 0);
        if (idx < 0) {
            printf("[amberscript] pylaw_spawn: failed to spawn npc\n");
        } else {
            s_py_law_enabled = 1;
            s_py_law_npc_idx = idx;
            s_py_law_frame_accum = 0;
            s_py_law_elapsed_sec = 0;
            snprintf(s_py_law_script, sizeof(s_py_law_script), "%s", script);
            printf("[amberscript] pylaw_spawn: npc=%d at (%d,%d) sprite=0x%02X script=%s\n",
                   idx, sx, sy, spr, s_py_law_script);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "scene_npc") == 0 ||
        strcmp(verb, "scene-npc") == 0 ||
        strcmp(verb, "scenenpc") == 0 ||
        strcmp(verb, "npcscene") == 0) {
        char sub[16] = {0};
        if (!AmberScript_ParseArg(cmd, 1, sub, sizeof(sub))) {
            printf("[amberscript] scene_npc usage:\n");
            printf("      scene_npc add <name> <scene> <sprite> <x_expr> <y_expr>\n");
            printf("      scene_npc list\n");
            printf("      scene_npc clear [name]\n");
            AmberScript_WriteState();
            return 1;
        }
        if (strcmp(sub, "add") == 0) {
            char name[24] = {0}, scene[64] = {0}, sprite[32] = {0}, xexpr[32] = {0}, yexpr[32] = {0};
            int x = 0, y = 0, sprite_id = -1, idx = -1, slot = -1, ncmd;
            if (!AmberScript_ParseArg(cmd, 2, name, sizeof(name)) ||
                !AmberScript_ParseArg(cmd, 3, scene, sizeof(scene)) ||
                !AmberScript_ParseArg(cmd, 4, sprite, sizeof(sprite)) ||
                !AmberScript_ParseArg(cmd, 5, xexpr, sizeof(xexpr)) ||
                !AmberScript_ParseArg(cmd, 6, yexpr, sizeof(yexpr))) {
                printf("[amberscript] scene_npc add usage: scene_npc add <name> <scene> <sprite> <x_expr> <y_expr>\n");
                AmberScript_WriteState();
                return 1;
            }
            if (!AmberScript_ParseCoordExpr(xexpr, 1, &x) || !AmberScript_ParseCoordExpr(yexpr, 0, &y)) {
                printf("[amberscript] scene_npc add: bad coordinate expression(s)\n");
                AmberScript_WriteState();
                return 1;
            }
            sprite_id = pks_parse_sprite(sprite);
            if (sprite_id < 0) {
                printf("[amberscript] scene_npc add: bad sprite '%s'\n", sprite);
                AmberScript_WriteState();
                return 1;
            }
            ncmd = scene_load_file(scene);
            if (ncmd <= 0) {
                printf("[amberscript] scene_npc add: scene '%s' not found or empty\n", scene);
                AmberScript_WriteState();
                return 1;
            }
            idx = NPC_DebugSpawn((uint8_t)sprite_id, x, y, 0, 0);
            if (idx < 0) {
                printf("[amberscript] scene_npc add: failed to spawn NPC\n");
                AmberScript_WriteState();
                return 1;
            }
            slot = scene_npc_binding_alloc();
            if (slot < 0) {
                printf("[amberscript] scene_npc add: no free bindings\n");
                NPC_DebugDespawn(idx);
                AmberScript_WriteState();
                return 1;
            }
            memset(&s_scene_npc_bindings[slot], 0, sizeof(s_scene_npc_bindings[slot]));
            s_scene_npc_bindings[slot].used = 1;
            s_scene_npc_bindings[slot].npc_idx = idx;
            s_scene_npc_bindings[slot].map_id = wCurMap;
            s_scene_npc_bindings[slot].sprite_id = (uint8_t)sprite_id;
            s_scene_npc_bindings[slot].tile_x = x;
            s_scene_npc_bindings[slot].tile_y = y;
            s_scene_npc_bindings[slot].auto_spawn = 1;
            snprintf(s_scene_npc_bindings[slot].name, sizeof(s_scene_npc_bindings[slot].name), "%s", name);
            snprintf(s_scene_npc_bindings[slot].scene, sizeof(s_scene_npc_bindings[slot].scene), "%s", scene);
            printf("[amberscript] scene_npc add: '%s' idx=%d map=%u (%d,%d) -> scene '%s'\n",
                   name, idx, (unsigned)wCurMap, x, y, scene);
            if (s_dsl_bank_enabled) dsl_bank_save();
        } else if (strcmp(sub, "list") == 0) {
            int any = 0;
            for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
                if (!s_scene_npc_bindings[i].used) continue;
                any = 1;
                printf("[amberscript] scene_npc[%d]: name='%s' idx=%d map=%u tile=(%d,%d) sprite=0x%02X scene='%s'\n",
                       i, s_scene_npc_bindings[i].name, s_scene_npc_bindings[i].npc_idx,
                       (unsigned)s_scene_npc_bindings[i].map_id,
                       s_scene_npc_bindings[i].tile_x, s_scene_npc_bindings[i].tile_y,
                       (unsigned)s_scene_npc_bindings[i].sprite_id,
                       s_scene_npc_bindings[i].scene);
            }
            if (!any) printf("[amberscript] scene_npc list: empty\n");
        } else if (strcmp(sub, "clear") == 0) {
            char name[24] = {0};
            int cleared = 0;
            if (!AmberScript_ParseArg(cmd, 2, name, sizeof(name))) {
                for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
                    if (!s_scene_npc_bindings[i].used) continue;
                    NPC_DebugDespawn(s_scene_npc_bindings[i].npc_idx);
                    memset(&s_scene_npc_bindings[i], 0, sizeof(s_scene_npc_bindings[i]));
                    cleared++;
                }
                printf("[amberscript] scene_npc clear: cleared all (%d)\n", cleared);
            } else {
                for (int i = 0; i < SCENE_ACTOR_MAX; i++) {
                    if (!s_scene_npc_bindings[i].used) continue;
                    if (strcmp(s_scene_npc_bindings[i].name, name) != 0) continue;
                    NPC_DebugDespawn(s_scene_npc_bindings[i].npc_idx);
                    memset(&s_scene_npc_bindings[i], 0, sizeof(s_scene_npc_bindings[i]));
                    cleared++;
                }
                printf("[amberscript] scene_npc clear: name='%s' cleared=%d\n", name, cleared);
            }
            if (s_dsl_bank_enabled) dsl_bank_save();
        } else {
            printf("[amberscript] scene_npc usage:\n");
            printf("      scene_npc add <name> <scene> <sprite> <x_expr> <y_expr>\n");
            printf("      scene_npc list\n");
            printf("      scene_npc clear [name]\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "bulbaspriteswap") == 0) {
        int ncmd;
        scene_reset_runtime();
        ncmd = scene_load_file("python_bulbasaur_sprite_inject_poc");
        if (ncmd <= 0) {
            printf("[amberscript] bulbaspriteswap: failed to load scene 'python_bulbasaur_sprite_inject_poc'\n");
        } else {
            s_scene_active = 1;
            s_scene_pc = 0;
            printf("[amberscript] bulbaspriteswap: loaded 'python_bulbasaur_sprite_inject_poc' (%d command(s))\n", ncmd);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "npcdump") == 0) {
        NPC_DebugDump();
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "scene_run") == 0) {
        char name[64] = {0};
        int ncmd;
        if (!AmberScript_ParseArg(cmd, 1, name, sizeof(name))) {
            printf("[amberscript] scene_run usage: scene_run <name>  (loads mod_runtime/scenes/<name>.scene)\n");
            AmberScript_WriteState();
            return 1;
        }
        scene_reset_runtime();
        ncmd = scene_load_file(name);
        if (ncmd <= 0) {
            printf("[amberscript] scene_run: failed to load scene '%s'\n", name);
        } else {
            s_scene_active = 1;
            s_scene_pc = 0;
            printf("[amberscript] scene_run: loaded '%s' (%d command(s))\n", name, ncmd);
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "scene_trigger") == 0) {
        char sub[16] = {0};
        if (!AmberScript_ParseArg(cmd, 1, sub, sizeof(sub))) {
            printf("[amberscript] scene_trigger usage:\n");
            printf("      scene_trigger set <scene> trigger_point <x_expr> <y_expr> [map]\n");
            printf("      scene_trigger set <scene> trigger_point <x_expr> <y_expr> when event_set|event_clear <event> [map]\n");
            printf("      scene_trigger list\n");
            printf("      scene_trigger clear [scene]\n");
            printf("      x_expr/y_expr: number or player.x[+/-n], player.y[+/-n]\n");
            AmberScript_WriteState();
            return 1;
        }
        if (strcmp(sub, "set") == 0) {
            char scene[64] = {0}, marker[24] = {0}, xexpr[32] = {0}, yexpr[32] = {0}, tok6[64] = {0}, maptok[64] = {0};
            int map_id = (int)wCurMap;
            int x = 0, y = 0;
            int ncmd;
            uint8_t cond_kind = 0;
            uint16_t cond_event = 0;
            if (!AmberScript_ParseArg(cmd, 2, scene, sizeof(scene)) ||
                !AmberScript_ParseArg(cmd, 3, marker, sizeof(marker)) ||
                !AmberScript_ParseArg(cmd, 4, xexpr, sizeof(xexpr)) ||
                !AmberScript_ParseArg(cmd, 5, yexpr, sizeof(yexpr))) {
                printf("[amberscript] scene_trigger set usage: scene_trigger set <scene> trigger_point <x_expr> <y_expr> [map]\n");
                AmberScript_WriteState();
                return 1;
            }
            if (strcmp(marker, "trigger_point") != 0) {
                printf("[amberscript] scene_trigger set: expected marker 'trigger_point'\n");
                AmberScript_WriteState();
                return 1;
            }
            if (!AmberScript_ParseCoordExprOrAny(xexpr, 1, &x) || !AmberScript_ParseCoordExprOrAny(yexpr, 0, &y)) {
                printf("[amberscript] scene_trigger set: bad coordinate expression(s)\n");
                AmberScript_WriteState();
                return 1;
            }
            if (AmberScript_ParseArg(cmd, 6, tok6, sizeof(tok6))) {
                if (strcmp(tok6, "when") == 0 || strcmp(tok6, "if") == 0) {
                    char condtok[24] = {0}, eventtok[96] = {0}, mapafter[64] = {0};
                    if (!AmberScript_ParseArg(cmd, 7, condtok, sizeof(condtok)) ||
                        !AmberScript_ParseArg(cmd, 8, eventtok, sizeof(eventtok))) {
                        printf("[amberscript] scene_trigger set: usage ... when event_set|event_clear <event> [map]\n");
                        AmberScript_WriteState();
                        return 1;
                    }
                    if (strcmp(condtok, "event_set") == 0 || strcmp(condtok, "set") == 0) cond_kind = 1;
                    else if (strcmp(condtok, "event_clear") == 0 || strcmp(condtok, "clear") == 0) cond_kind = 2;
                    else {
                        printf("[amberscript] scene_trigger set: expected event_set or event_clear\n");
                        AmberScript_WriteState();
                        return 1;
                    }
                    if (!pks_resolve_event_token(eventtok, &cond_event)) {
                        printf("[amberscript] scene_trigger set: unknown event '%s'\n", eventtok);
                        AmberScript_WriteState();
                        return 1;
                    }
                    if (AmberScript_ParseArg(cmd, 9, mapafter, sizeof(mapafter))) {
                        if (!pks_resolve_map_token(mapafter, &map_id)) {
                            printf("[amberscript] scene_trigger set: unknown map '%s'\n", mapafter);
                            AmberScript_WriteState();
                            return 1;
                        }
                    }
                } else {
                    snprintf(maptok, sizeof(maptok), "%s", tok6);
                    if (!pks_resolve_map_token(maptok, &map_id)) {
                        printf("[amberscript] scene_trigger set: unknown map '%s'\n", maptok);
                        AmberScript_WriteState();
                        return 1;
                    }
                }
            }
            ncmd = pks_scene_trigger_register(scene, map_id, x, y, cond_kind, cond_event, 0, 0);
            if (ncmd == 0) {
                printf("[amberscript] scene_trigger set: scene '%s' not found or empty\n", scene);
                AmberScript_WriteState();
                return 1;
            }
            if (ncmd < 0) {
                printf("[amberscript] scene_trigger set: no free trigger slots (max %d)\n", SCENE_TRIGGER_MAX);
                AmberScript_WriteState();
                return 1;
            }
            if (cond_kind == 1) {
                printf("[amberscript] scene_trigger set: '%s' @ map=%u (%d,%d) when event_set %s (%u)\n",
                       scene, (unsigned)map_id, x, y, EventFlagName(cond_event), (unsigned)cond_event);
            } else if (cond_kind == 2) {
                printf("[amberscript] scene_trigger set: '%s' @ map=%u (%d,%d) when event_clear %s (%u)\n",
                       scene, (unsigned)map_id, x, y, EventFlagName(cond_event), (unsigned)cond_event);
            } else {
                printf("[amberscript] scene_trigger set: '%s' @ map=%u (%d,%d)\n", scene, (unsigned)map_id, x, y);
            }
        } else if (strcmp(sub, "list") == 0) {
            int any = 0;
            for (int i = 0; i < SCENE_TRIGGER_MAX; i++) {
                if (!s_scene_triggers[i].used) continue;
                any = 1;
                printf("[amberscript] scene_trigger[%d]: '%s' map=%u (%d,%d) armed=%d",
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
                if (s_scene_triggers[i].cond_kind2 == 1) {
                    printf(" and event_set %s (%u)",
                           EventFlagName(s_scene_triggers[i].cond_event2),
                           (unsigned)s_scene_triggers[i].cond_event2);
                } else if (s_scene_triggers[i].cond_kind2 == 2) {
                    printf(" and event_clear %s (%u)",
                           EventFlagName(s_scene_triggers[i].cond_event2),
                           (unsigned)s_scene_triggers[i].cond_event2);
                }
                printf("\n");
            }
            if (!any) printf("[amberscript] scene_trigger list: empty\n");
        } else if (strcmp(sub, "clear") == 0) {
            char scene[64] = {0};
            int cleared = 0;
            if (!AmberScript_ParseArg(cmd, 2, scene, sizeof(scene))) {
                memset(s_scene_triggers, 0, sizeof(s_scene_triggers));
                printf("[amberscript] scene_trigger clear: cleared all trigger points\n");
                AmberScript_WriteState();
                return 1;
            }
            for (int i = 0; i < SCENE_TRIGGER_MAX; i++) {
                if (s_scene_triggers[i].used && strcmp(s_scene_triggers[i].scene, scene) == 0) {
                    memset(&s_scene_triggers[i], 0, sizeof(s_scene_triggers[i]));
                    cleared++;
                }
            }
            printf("[amberscript] scene_trigger clear: cleared %d trigger(s) for '%s'\n", cleared, scene);
        } else {
            printf("[amberscript] scene_trigger usage:\n");
            printf("      scene_trigger set <scene> trigger_point <x_expr> <y_expr> [map]\n");
            printf("      scene_trigger set <scene> trigger_point <x_expr> <y_expr> when event_set|event_clear <event> [map]\n");
            printf("      scene_trigger list\n");
            printf("      scene_trigger clear [scene]\n");
        }
        AmberScript_WriteState();
        return 1;
    }
    if (strcmp(verb, "scene_stop") == 0) {
        wJoyIgnore = 0;
        scene_reset_runtime();
        printf("[amberscript] scene_stop: stopped scene runner\n");
        AmberScript_WriteState();
        return 1;
    }

    return 0;
}
