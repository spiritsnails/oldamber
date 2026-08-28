
#include <stdio.h>
#include "static_encounter.h"
#include "amberscript_mapbank.h"
#include "amberscript_core.h"
#include "text.h"
#include "player.h"
#include "npc.h"
#include "overworld.h"
#include "trainer_sight.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"

extern void Game_StartWildBattleScripted(uint8_t species, uint8_t level);
extern int  Game_GetScene(void);
extern int  BattleUI_IsActive(void);

enum {
    SE_IDLE = 0,
    SE_TEXT_WAIT,
    SE_CRY_WAIT,
    SE_JINGLE_HOLD,
    SE_BATTLE_START,
    SE_BATTLE_WAIT
};

#define SE_JINGLE_TICKS 48

static int      s_state = SE_IDLE;
static int      s_index = -1;
static uint8_t  s_map   = 0;
static uint8_t  s_species = 0;
static uint8_t  s_level = 0;
static uint16_t s_flag  = 0;
static int      s_cry   = 0;

static int      s_battle_started = 0;
static int      s_jingle_timer = 0;

void StaticEncounter_Reset(void) {
    s_state = SE_IDLE;
    s_index = -1;
    s_jingle_timer = 0;
    s_battle_started = 0;
}

int StaticEncounter_IsActive(void) {
    return s_state != SE_IDLE;
}

void AmberScript_StaticEncounterInteract(void) {
    int fx, fy, index = -1;
    int species = 0, level = 0, cry = 0;
    uint16_t flag = 0;
    const char *text = NULL;

    if (s_state != SE_IDLE) return;

    Player_GetFacingTile(&fx, &fy);
    if (!AmberScript_GetStaticEncounterAt(wCurMap, fx, fy, &index)) return;
    if (!AmberScript_GetStaticEncounterInfo(wCurMap, index, &species, &level,
                                            &flag, &cry, &text)) return;

    s_index   = index;
    s_map     = wCurMap;
    s_species = (uint8_t)species;
    s_level   = (uint8_t)level;
    s_flag    = flag;
    s_cry     = cry;

    if (text && text[0]) {
        Text_ShowASCII(text);
        s_state = SE_TEXT_WAIT;
    } else {
        s_state = SE_JINGLE_HOLD;
    }
}

void StaticEncounter_Tick(void) {
    switch (s_state) {
    case SE_IDLE:
        return;

    case SE_TEXT_WAIT:
        if (Text_IsOpen()) return;
        if (s_cry) {

            Audio_PlayCry(s_species);
            s_state = SE_CRY_WAIT;
            return;
        }
        s_state = SE_JINGLE_HOLD;
        return;

    case SE_CRY_WAIT:
        if (Audio_IsCryPlaying()) return;
        s_state = SE_JINGLE_HOLD;
        return;

    case SE_JINGLE_HOLD:

        if (s_jingle_timer == 0) Trainer_PlayEncounterMusic(s_species);
        if (++s_jingle_timer < SE_JINGLE_TICKS) return;
        s_state = SE_BATTLE_START;
        return;

    case SE_BATTLE_START:
        Game_StartWildBattleScripted(s_species, s_level);
        s_battle_started = 0;
        s_state = SE_BATTLE_WAIT;
        return;

    case SE_BATTLE_WAIT:
        if (Game_GetScene() == 2 || BattleUI_IsActive()) {
            s_battle_started = 1;
            return;
        }
        if (!s_battle_started) return;

        if (wCurMap == s_map) {
            if (s_flag) SetEvent(s_flag);

            NPC_Load();
            NPC_BuildView(gScrollPxX, gScrollPxY);
            printf("[static_encounter] resolved: map=%u idx=%d species=%u -> flag %u set\n",
                   (unsigned)s_map, s_index, (unsigned)s_species, (unsigned)s_flag);
        }
        StaticEncounter_Reset();
        return;
    }
}
