#include "victory_road_scripts.h"
#include "rom_text.h"
#include "amberscript_tilemod.h"
#include "amberscript_mapbank.h"
#include "overworld.h"
#include "player.h"
#include "npc.h"
#include "text.h"
#include "constants.h"
#include "battle/battle_loop.h"
#include "../platform/audio.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"
#include <stdint.h>

#define MAP_VICTORY_ROAD_1F 0x6C
#define MAP_VICTORY_ROAD_2F 0xC2
#define MAP_VICTORY_ROAD_3F 0xC6

typedef struct {
    uint8_t map_id;
    int x;
    int y;
    uint16_t event_id;
} boulder_switch_t;

static const boulder_switch_t kSwitchCoords[] = {
    { MAP_VICTORY_ROAD_1F, 17, 13, EVENT_VICTORY_ROAD_1_BOULDER_ON_SWITCH },
    { MAP_VICTORY_ROAD_2F,  1, 16, EVENT_VICTORY_ROAD_2_BOULDER_ON_SWITCH1 },
    { MAP_VICTORY_ROAD_2F,  9, 16, EVENT_VICTORY_ROAD_2_BOULDER_ON_SWITCH2 },
    { MAP_VICTORY_ROAD_3F,  3,  5, EVENT_VICTORY_ROAD_3_BOULDER_ON_SWITCH1 },
};

typedef struct {
    uint8_t map_id;
    int x;
    int y;
    uint16_t event_id;
} boulder_hole_t;

static const boulder_hole_t kHoleCoords[] = {

    { MAP_VICTORY_ROAD_3F, 23, 15, EVENT_VICTORY_ROAD_3_BOULDER_ON_SWITCH2 },
};
static int s_pending_hide_3f_hole_boulder = 0;
static int s_pending_hide_3f_hole_delay = 0;
static int s_moltres_pending_battle = 0;
static int s_moltres_battle_ready = 0;
static int s_moltres_battle_active = 0;
#define kMoltresBattleText (RomText("_PowerPlantZapdosBattleText"))

static int is_victory_road_map(uint8_t map_id) {
    return map_id == MAP_VICTORY_ROAD_1F ||
           map_id == MAP_VICTORY_ROAD_2F ||
           map_id == MAP_VICTORY_ROAD_3F;
}

static void apply_victory_road_boulder_visibility(uint8_t map_id) {
    const int dropped_from_3f = CheckEvent(EVENT_VICTORY_ROAD_3_BOULDER_ON_SWITCH2);

    if (map_id == MAP_VICTORY_ROAD_3F) {
        if (dropped_from_3f) NPC_HideSprite(7);
        else                 NPC_ShowSprite(7);
        return;
    }

    if (map_id == MAP_VICTORY_ROAD_2F) {
        if (CheckEvent(EVENT_BEAT_MOLTRES)) NPC_HideSprite(5);
        else NPC_ShowSprite(5);
        if (dropped_from_3f) NPC_ShowSprite(8);
        else                 NPC_HideSprite(8);
    }
}

static void apply_victory_road_blocks(uint8_t map_id) {
    if (map_id == MAP_VICTORY_ROAD_1F) {

        AmberScript_PlaceSwapBlock(
            "victoryroad1f_sw",
            CheckEvent(EVENT_VICTORY_ROAD_1_BOULDER_ON_SWITCH) ? "on" : "off",
            4, 6);
        return;
    }

    if (map_id == MAP_VICTORY_ROAD_2F) {

        AmberScript_PlaceSwapBlock(
            "victoryroad2f_sw1",
            CheckEvent(EVENT_VICTORY_ROAD_2_BOULDER_ON_SWITCH1) ? "on" : "off",
            3, 4);
        AmberScript_PlaceSwapBlock(
            "victoryroad2f_sw2",
            CheckEvent(EVENT_VICTORY_ROAD_2_BOULDER_ON_SWITCH2) ? "on" : "off",
            11, 7);
        return;
    }

    if (map_id == MAP_VICTORY_ROAD_3F) {

        AmberScript_PlaceSwapBlock(
            "victoryroad3f_sw1",
            CheckEvent(EVENT_VICTORY_ROAD_3_BOULDER_ON_SWITCH1) ? "on" : "off",
            3, 5);
    }
}

void VictoryRoadScripts_OnMapLoad(void) {
    {
        int vr_real = Map_CurrentRealId();
        if (vr_real < 0 || !is_victory_road_map((uint8_t)vr_real)) return;

        if (vr_real == MAP_VICTORY_ROAD_2F)
            ClearEvent(EVENT_VICTORY_ROAD_1_BOULDER_ON_SWITCH);
        apply_victory_road_boulder_visibility((uint8_t)vr_real);
        apply_victory_road_blocks((uint8_t)vr_real);
    }
}

void VictoryRoadScripts_Tick(void) {
    uint8_t pushed_map;
    int pushed_x, pushed_y;

    { int vr = Map_CurrentRealId();
      if (vr < 0 || !is_victory_road_map((uint8_t)vr)) return; }

    {
        int vr = Map_CurrentRealId();
        if (vr >= 0) apply_victory_road_blocks((uint8_t)vr);
    }

    if (s_moltres_pending_battle &&
        !Text_IsOpen() &&
        !Audio_IsCryPlaying()) {
        s_moltres_pending_battle = 0;
        s_moltres_battle_ready = 1;
    }

    if (Map_CurrentRealId() == MAP_VICTORY_ROAD_3F && s_pending_hide_3f_hole_boulder) {
        if (!NPC_IsWalking(7)) {
            if (s_pending_hide_3f_hole_delay > 0) {
                s_pending_hide_3f_hole_delay--;
            } else {
                NPC_HideSprite(7);
                Map_BuildScrollView();
                s_pending_hide_3f_hole_boulder = 0;
            }
        }
    }

    if (!Player_ConsumePushedBoulderEvent(&pushed_map, &pushed_x, &pushed_y)) return;
    if (!is_victory_road_map(pushed_map)) return;

    for (int i = 0; i < (int)(sizeof(kSwitchCoords) / sizeof(kSwitchCoords[0])); i++) {
        const boulder_switch_t *sw = &kSwitchCoords[i];
        if (sw->map_id != pushed_map) continue;
        if (sw->x != pushed_x || sw->y != pushed_y) continue;
        if (CheckEvent(sw->event_id)) return;

        SetEvent(sw->event_id);

        if (Map_CurrentRealId() == (int)pushed_map) {
            apply_victory_road_blocks(pushed_map);
            Map_BuildScrollView();
        }
        return;
    }

    for (int i = 0; i < (int)(sizeof(kHoleCoords) / sizeof(kHoleCoords[0])); i++) {
        const boulder_hole_t *h = &kHoleCoords[i];
        if (h->map_id != pushed_map) continue;
        if (h->x != pushed_x || h->y != pushed_y) continue;
        if (CheckEvent(h->event_id)) return;

        SetEvent(h->event_id);
        if (Map_CurrentRealId() == MAP_VICTORY_ROAD_3F) {
            s_pending_hide_3f_hole_boulder = 1;
            s_pending_hide_3f_hole_delay = 8;
        }
        return;
    }
}

void VictoryRoadScripts_MoltresInteract(void) {
    if (Map_CurrentRealId() != MAP_VICTORY_ROAD_2F) return;
    if (CheckEvent(EVENT_BEAT_MOLTRES)) return;
    if (s_moltres_pending_battle || s_moltres_battle_ready || s_moltres_battle_active) return;
    Text_ShowASCII(kMoltresBattleText);
    Audio_PlayCry(SPECIES_MOLTRES);
    s_moltres_pending_battle = 1;
}

int VictoryRoadScripts_ConsumeMoltresBattle(void) {
    if (!s_moltres_battle_ready) return 0;
    s_moltres_battle_ready = 0;
    s_moltres_battle_active = 1;
    return 1;
}

int VictoryRoadScripts_ConsumeMoltresPostBattle(void) {
    if (!s_moltres_battle_active) return 0;
    s_moltres_battle_active = 0;
    return 1;
}

void VictoryRoadScripts_OnMoltresBattleOutcome(uint8_t battle_outcome) {
    if (battle_outcome == BATTLE_OUTCOME_WILD_VICTORY ||
        battle_outcome == BATTLE_OUTCOME_CAUGHT) {
        SetEvent(EVENT_BEAT_MOLTRES);
        if (Map_CurrentRealId() == MAP_VICTORY_ROAD_2F) NPC_HideSprite(5);
    }
}
