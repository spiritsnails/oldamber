#include "pokemontower6f_scripts.h"
#include "rom_text.h"
#include "text.h"
#include "player.h"
#include "amberscript_mapbank.h"
#include "../data/map_data.h"
#include "../platform/audio.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"
#include "battle/battle_loop.h"
#include <string.h>

#define MAP_POKEMON_TOWER_6F 0x93
#define MAROWAK_LEVEL 30

static int map_is_tower_6f(uint8_t map) {
    if (map == MAP_POKEMON_TOWER_6F) return 1;
    if (map < PKS_VIRTUAL_MAP_FIRST || map > PKS_VIRTUAL_MAP_LAST) return 0;
    {
        const char *n = AmberScript_MapBank_NameForRealId(map);

        return n && strcasecmp(n, "PokemonTower6F") == 0;
    }
}

#define DIR_RIGHT 3

typedef enum {
    PT6_IDLE = 0,
    PT6_PENDING_BATTLE,
    PT6_POST_WIN_TEXT1,
    PT6_POST_WIN_CRY_WAIT,
    PT6_POST_WIN_TEXT2,
    PT6_LOSS_STEP,
} PT6State;

static PT6State g_state = PT6_IDLE;
static int g_pending_battle = 0;
static int g_battle_active = 0;
static int g_wait_frames = 0;
static int g_loss_step_started = 0;

#define kBeGoneText (RomText("_PokemonTower6FBeGoneText"))

#define kGhostWasCubonesMother (RomText("PokemonTower6FGhostWasCubonesMotherText"))

#define kSoulWasCalmed (RomText("PokemonTower6FSoulWasCalmedText"))

static int at_marowak_trigger(void) {
    return (int)wXCoord == 10 && (int)wYCoord == 16;
}

void PokemonTower6FScripts_StartGhostEncounter(void) {
    if (g_state != PT6_IDLE) return;
    Text_ShowASCII(kBeGoneText);
    g_state = PT6_PENDING_BATTLE;
}

void PokemonTower6FScripts_OnMapLoad(void) {
    if (!map_is_tower_6f(wCurMap)) {
        g_state = PT6_IDLE;
        g_pending_battle = 0;
        g_battle_active = 0;
        g_wait_frames = 0;
        g_loss_step_started = 0;
        return;
    }
}

void PokemonTower6FScripts_StepCheck(void) {
    (void)at_marowak_trigger;
}

void PokemonTower6FScripts_Tick(void) {
    if (!map_is_tower_6f(wCurMap)) return;

    switch (g_state) {
    case PT6_PENDING_BATTLE:
        if (Text_IsOpen()) return;
        g_pending_battle = 1;
        g_state = PT6_IDLE;
        return;
    case PT6_POST_WIN_TEXT1:
        if (!Text_IsOpen()) {
            Text_ShowASCII(kGhostWasCubonesMother);
            g_state = PT6_POST_WIN_CRY_WAIT;
            Audio_PlayCry(SPECIES_MAROWAK);
            g_wait_frames = 30;
        }
        return;
    case PT6_POST_WIN_CRY_WAIT:
        if (Text_IsOpen()) return;
        if (Audio_IsCryPlaying()) return;
        if (g_wait_frames > 0) { g_wait_frames--; return; }
        Text_ShowASCII(kSoulWasCalmed);
        g_state = PT6_POST_WIN_TEXT2;
        return;
    case PT6_POST_WIN_TEXT2:
        if (Text_IsOpen()) return;
        g_state = PT6_IDLE;
        return;
    case PT6_LOSS_STEP:
        if (!g_loss_step_started) {
            Player_DoScriptedStep(DIR_RIGHT);
            g_loss_step_started = 1;
            return;
        }
        if (Player_IsMoving()) return;
        g_wait_frames = 10;
        g_loss_step_started = 0;
        g_state = PT6_IDLE;
        return;
    default:
        return;
    }
}

int PokemonTower6FScripts_IsActive(void) {
    return map_is_tower_6f(wCurMap) && g_state != PT6_IDLE;
}

int PokemonTower6FScripts_GetPendingBattle(uint8_t *species_out, uint8_t *level_out) {
    if (!map_is_tower_6f(wCurMap)) return 0;
    if (!g_pending_battle) return 0;
    g_pending_battle = 0;
    *species_out = SPECIES_MAROWAK;
    *level_out = MAROWAK_LEVEL;
    g_battle_active = 1;
    return 1;
}

int PokemonTower6FScripts_ConsumeBattle(void) {
    if (!g_battle_active) return 0;
    g_battle_active = 0;
    return 1;
}

void PokemonTower6FScripts_OnBattleOutcome(uint8_t battle_outcome) {
    if (battle_outcome == BATTLE_OUTCOME_WILD_VICTORY) {
        SetEvent(EVENT_BEAT_GHOST_MAROWAK);
        g_state = PT6_POST_WIN_TEXT1;
    } else {
        g_state = PT6_LOSS_STEP;
    }
}
