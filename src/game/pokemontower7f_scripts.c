#include "pokemontower7f_scripts.h"
#include "rom_text.h"
#include "text.h"
#include "npc.h"
#include "warp.h"
#include "music.h"
#include "player.h"
#include "trainer_sight.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"

#define MAP_POKEMON_TOWER_7F 0x94
#define MAP_MR_FUJIS_HOUSE   0x95

#define ROCKET_CLASS 30
#define ROCKET_NO     3

#define NPC_ROCKET1 0
#define NPC_ROCKET2 1
#define NPC_ROCKET3 2
#define NPC_MR_FUJI 3

#define DIR_DOWN  0
#define DIR_UP    1
#define DIR_LEFT  2
#define DIR_RIGHT 3

typedef enum {
    PT7_IDLE = 0,
    PT7_SPOTTED,
    PT7_PENDING_BATTLE,
    PT7_POST_BATTLE_TEXT,
    PT7_ROCKET_EXIT,
    PT7_HIDE_ROCKET,
    PT7_MR_FUJI_WARP,
} PT7State;

typedef struct {
    int x, y;
    const int *seq;
} ExitPath;

static PT7State g_state = PT7_IDLE;
static int g_pending_battle = 0;
static int g_battle_active = 0;
static int g_active_rocket = -1;
static uint16_t g_active_flag = 0;
static const char *g_pre_text = 0;
static const char *g_defeat_text = 0;
static const char *g_post_text = 0;
static const int *g_exit_seq = 0;
static int g_exit_idx = 0;
static int g_post_text_started = 0;
static int g_spotted_timer = 0;
static int g_spotted_shown = 0;

#define kRocket1Pre (RomText("PokemonTower7FRocket1BattleText"))

#define kRocket1Defeat (RomTextPrefixed("ROCKET: ", "PokemonTower7FRocket1EndBattleText"))
#define kRocket1After (RomText("PokemonTower7FRocket1AfterBattleText"))
#define kRocket2Pre (RomText("PokemonTower7FRocket2BattleText"))
#define kRocket2Defeat (RomTextPrefixed("ROCKET: ", "PokemonTower7FRocket2EndBattleText"))
#define kRocket2After (RomText("PokemonTower7FRocket2AfterBattleText"))

#define kRocket3Pre (RomText("_PokemonTower7FRocket3BattleText"))

#define kRocket3Defeat (RomTextPrefixed("ROCKET: ", "PokemonTower7FRocket3EndBattleText"))
#define kRocket3After (RomText("_PokemonTower7FRocket3AfterBattleText"))
#define kFujiRescueText (RomText("PokemonTower7FMrFujiText.RescueText"))

static const int kExitRightDown[]     = { DIR_RIGHT, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_LEFT, -1 };
static const int kExitDownRight[]     = { DIR_DOWN, DIR_RIGHT, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, -1 };
static const int kExitDown[]          = { DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, -1 };
static const int kExitLeftDown[]      = { DIR_LEFT, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, -1 };
static const int kExitDownLeft[]      = { DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_LEFT, DIR_DOWN, DIR_DOWN, -1 };
static const int kExitRightDownLong[] = { DIR_RIGHT, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, DIR_DOWN, -1 };

static const ExitPath kExitPaths[] = {
    {  9, 12, kExitRightDown     },
    { 10, 11, kExitDownRight     },
    { 11, 11, kExitDown          },
    { 12, 11, kExitDown          },
    { 12, 10, kExitLeftDown      },
    { 11,  9, kExitDownLeft      },
    { 10,  9, kExitDown          },
    {  9,  9, kExitDown          },
    {  9,  8, kExitRightDownLong },
    { 10,  7, kExitDown          },
    { 11,  7, kExitDown          },
    { 12,  7, kExitDown          },
};

static const int kNoMove[] = { -1 };

static const int *choose_exit_path(void) {
    int px = (int)wXCoord;
    int py = (int)wYCoord;
    for (int i = 0; i < (int)(sizeof(kExitPaths) / sizeof(kExitPaths[0])); i++) {
        if (kExitPaths[i].x == px && kExitPaths[i].y == py) return kExitPaths[i].seq;
    }
    return kNoMove;
}

static int in_sight_cone(int nx, int ny, int facing, int px, int py, int range) {
    switch (facing) {
    case DIR_RIGHT:
        return (py == ny && px > nx && px - nx <= range);
    case DIR_LEFT:
        return (py == ny && px < nx && nx - px <= range);
    case DIR_UP:
        return (px == nx && py < ny && ny - py <= range);
    case DIR_DOWN:
        return (px == nx && py > ny && py - ny <= range);
    default:
        return 0;
    }
}

static void begin_rocket_talk(int rocket_idx, uint16_t flag, const char *pre_text,
                              const char *defeat_text, const char *post_text,
                              int from_sight) {
    if (wCurMap != MAP_POKEMON_TOWER_7F) return;
    if (g_state != PT7_IDLE) return;
    if (CheckEvent(flag)) return;
    g_active_rocket = rocket_idx;
    g_active_flag = flag;
    g_pre_text = pre_text;
    g_defeat_text = defeat_text;
    g_post_text = post_text;
    g_post_text_started = 0;
    g_spotted_shown = 0;
    if (from_sight) {
        g_spotted_timer = 60;
        g_state = PT7_SPOTTED;
    } else if (g_pre_text) {
        Text_ShowASCII(g_pre_text);
        g_state = PT7_PENDING_BATTLE;
    } else {
        g_pending_battle = 1;
        g_state = PT7_IDLE;
    }
}

void PokemonTower7FScripts_OnMapLoad(void) {
    if (wCurMap != MAP_POKEMON_TOWER_7F) {
        g_state = PT7_IDLE;
        g_pending_battle = 0;
        g_battle_active = 0;
        g_active_rocket = -1;
        g_active_flag = 0;
        g_pre_text = 0;
        g_defeat_text = 0;
        g_post_text = 0;
        g_exit_seq = 0;
        g_exit_idx = 0;
        g_post_text_started = 0;
        g_spotted_timer = 0;
        g_spotted_shown = 0;
        Emote_Hide();
        return;
    }
    if (CheckEvent(EVENT_BEAT_POKEMONTOWER_7_TRAINER_0)) NPC_HideSprite(NPC_ROCKET1);
    if (CheckEvent(EVENT_BEAT_POKEMONTOWER_7_TRAINER_1)) NPC_HideSprite(NPC_ROCKET2);
    if (CheckEvent(EVENT_BEAT_POKEMONTOWER_7_TRAINER_2)) NPC_HideSprite(NPC_ROCKET3);
    if (CheckEvent(EVENT_RESCUED_MR_FUJI)) NPC_HideSprite(NPC_MR_FUJI);

    NPC_SetFacing(NPC_ROCKET1, DIR_RIGHT);
    NPC_SetFacing(NPC_ROCKET2, DIR_LEFT);
    NPC_SetFacing(NPC_ROCKET3, DIR_RIGHT);
}

void PokemonTower7FScripts_StepCheck(void) {
    int px, py;
    if (wCurMap != MAP_POKEMON_TOWER_7F) return;
    if (g_state != PT7_IDLE) return;
    if (Text_IsOpen()) return;
    if (Trainer_IsEngaging()) return;

    px = (int)wXCoord;
    py = (int)wYCoord;
    if (!CheckEvent(EVENT_BEAT_POKEMONTOWER_7_TRAINER_0) &&
        in_sight_cone(9, 11, DIR_RIGHT, px, py, 3)) {
        begin_rocket_talk(NPC_ROCKET1, EVENT_BEAT_POKEMONTOWER_7_TRAINER_0,
                          kRocket1Pre, kRocket1Defeat, kRocket1After, 1);
        return;
    }
    if (!CheckEvent(EVENT_BEAT_POKEMONTOWER_7_TRAINER_1) &&
        in_sight_cone(12, 9, DIR_LEFT, px, py, 3)) {
        begin_rocket_talk(NPC_ROCKET2, EVENT_BEAT_POKEMONTOWER_7_TRAINER_1,
                          kRocket2Pre, kRocket2Defeat, kRocket2After, 1);
        return;
    }
    if (!CheckEvent(EVENT_BEAT_POKEMONTOWER_7_TRAINER_2) &&
        in_sight_cone(9, 7, DIR_RIGHT, px, py, 3)) {
        begin_rocket_talk(NPC_ROCKET3, EVENT_BEAT_POKEMONTOWER_7_TRAINER_2,
                          kRocket3Pre, kRocket3Defeat, kRocket3After, 1);
        return;
    }
}

void PokemonTower7FScripts_Tick(void) {
    if (wCurMap != MAP_POKEMON_TOWER_7F) return;
    switch (g_state) {
    case PT7_SPOTTED:
        if (!g_spotted_shown) {
            Emote_ShowOnNPC(g_active_rocket);
            Music_Play(MUSIC_MEET_EVIL_TRAINER);
            g_spotted_shown = 1;
        }
        if (--g_spotted_timer > 0) return;
        Emote_Hide();
        if (g_pre_text) {
            Text_ShowASCII(g_pre_text);
            g_state = PT7_PENDING_BATTLE;
        } else {
            g_pending_battle = 1;
            g_state = PT7_IDLE;
        }
        return;
    case PT7_PENDING_BATTLE:
        if (Text_IsOpen()) return;
        if (g_defeat_text) gTrainerAfterText = g_defeat_text;
        g_pending_battle = 1;
        g_state = PT7_IDLE;
        return;
    case PT7_POST_BATTLE_TEXT:
        if (!g_post_text_started) {
            if (g_post_text) Text_ShowASCII(g_post_text);
            g_post_text_started = 1;
            return;
        }
        if (Text_IsOpen()) return;
        g_exit_seq = choose_exit_path();
        g_exit_idx = 0;
        Player_IgnoreInputFrames(1);
        if (g_exit_seq[g_exit_idx] != -1) {
            NPC_DoScriptedStep(g_active_rocket, g_exit_seq[g_exit_idx++]);
            g_state = PT7_ROCKET_EXIT;
        } else {
            g_state = PT7_HIDE_ROCKET;
        }
        return;
    case PT7_ROCKET_EXIT:
        Player_IgnoreInputFrames(1);
        if (NPC_IsWalking(g_active_rocket)) return;
        if (g_exit_seq && g_exit_seq[g_exit_idx] != -1) {
            NPC_DoScriptedStep(g_active_rocket, g_exit_seq[g_exit_idx++]);
            return;
        }
        g_state = PT7_HIDE_ROCKET;
        return;
    case PT7_HIDE_ROCKET:
        Player_IgnoreInputFrames(1);
        NPC_HideSprite(g_active_rocket);
        g_state = PT7_IDLE;
        g_active_rocket = -1;
        g_active_flag = 0;
        g_pre_text = 0;
        g_defeat_text = 0;
        g_post_text = 0;
        g_exit_seq = 0;
        g_exit_idx = 0;
        g_post_text_started = 0;
        g_spotted_timer = 0;
        g_spotted_shown = 0;
        Emote_Hide();
        return;
    case PT7_MR_FUJI_WARP:
        if (Text_IsOpen()) return;
        Warp_ForceTeleport(MAP_MR_FUJIS_HOUSE, 3, 7);
        g_state = PT7_IDLE;
        return;
    default:
        return;
    }
}

int PokemonTower7FScripts_IsActive(void) {
    return wCurMap == MAP_POKEMON_TOWER_7F && g_state != PT7_IDLE;
}

int PokemonTower7FScripts_GetPendingBattle(uint8_t *class_out, uint8_t *no_out) {
    if (wCurMap != MAP_POKEMON_TOWER_7F) return 0;
    if (!g_pending_battle) return 0;
    g_pending_battle = 0;
    *class_out = ROCKET_CLASS;
    *no_out = ROCKET_NO;
    g_battle_active = 1;
    return 1;
}

int PokemonTower7FScripts_ConsumeBattle(void) {
    if (!g_battle_active) return 0;
    g_battle_active = 0;
    return 1;
}

void PokemonTower7FScripts_OnVictory(void) {
    if (!g_active_flag || g_active_rocket < 0) return;
    SetEvent(g_active_flag);
    g_state = PT7_POST_BATTLE_TEXT;
    g_post_text_started = 0;
}

void PokemonTower7FScripts_OnDefeat(void) {
    g_state = PT7_IDLE;
    g_active_rocket = -1;
    g_active_flag = 0;
    g_pre_text = 0;
    g_defeat_text = 0;
    g_post_text = 0;
    g_post_text_started = 0;
    g_spotted_timer = 0;
    g_spotted_shown = 0;
    Emote_Hide();
}

void PokemonTower7F_Rocket1Script(void) {
    begin_rocket_talk(NPC_ROCKET1, EVENT_BEAT_POKEMONTOWER_7_TRAINER_0,
                      kRocket1Pre, kRocket1Defeat, kRocket1After, 0);
}

void PokemonTower7F_Rocket2Script(void) {
    begin_rocket_talk(NPC_ROCKET2, EVENT_BEAT_POKEMONTOWER_7_TRAINER_1,
                      kRocket2Pre, kRocket2Defeat, kRocket2After, 0);
}

void PokemonTower7F_Rocket3Script(void) {
    begin_rocket_talk(NPC_ROCKET3, EVENT_BEAT_POKEMONTOWER_7_TRAINER_2,
                      kRocket3Pre, kRocket3Defeat, kRocket3After, 0);
}
