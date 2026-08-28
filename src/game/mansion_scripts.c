#include "mansion_scripts.h"
#include "rom_text.h"
#include "amberscript_tilemod.h"
#include "constants.h"
#include "overworld.h"
#include "player.h"
#include "text.h"
#include "yesno.h"
#include "../platform/audio.h"
#include "../platform/hardware.h"
#include "../data/event_constants.h"

#define MAP_POKEMON_MANSION_1F  0xA5
#define MAP_POKEMON_MANSION_2F  0xD6
#define MAP_POKEMON_MANSION_3F  0xD7
#define MAP_POKEMON_MANSION_B1F 0xD8

#define DIR_UP 1

typedef enum {
    MANSION_IDLE = 0,
    MANSION_WAIT_YESNO,
} mansion_state_t;

static mansion_state_t s_state = MANSION_IDLE;

#define kSwitchPrompt (RomText("PokemonMansion1FSwitchText.Text"))
#define kSwitchPressed (RomText("PokemonMansion1FSwitchText.PressedText"))
#define kSwitchNotPressed (RomText("PokemonMansion1FSwitchText.NotPressedText"))

static void apply_map_blocks(uint8_t map_id) {
    const char *st = CheckEvent(EVENT_MANSION_SWITCH_ON) ? "on" : "off";
    #define MANSION_GATE(pfx, bx, by) \
        AmberScript_PlaceSwapBlock((pfx), st, (bx), (by))

    if (map_id == MAP_POKEMON_MANSION_1F) {
        MANSION_GATE("mansion1f_gate_a", 12, 6);
        MANSION_GATE("mansion1f_gate_b",  8, 3);
        MANSION_GATE("mansion1f_gate_c", 10, 8);
        MANSION_GATE("mansion1f_gate_d", 13, 13);
        return;
    }

    if (map_id == MAP_POKEMON_MANSION_2F) {
        MANSION_GATE("mansion2f_gate_a", 4, 2);
        MANSION_GATE("mansion2f_gate_b", 9, 4);
        MANSION_GATE("mansion2f_gate_c", 3, 11);
        return;
    }

    if (map_id == MAP_POKEMON_MANSION_3F) {
        MANSION_GATE("mansion3f_gate_a", 7, 2);
        MANSION_GATE("mansion3f_gate_b", 7, 5);
        return;
    }

    if (map_id == MAP_POKEMON_MANSION_B1F) {
        MANSION_GATE("mansionb1f_gate_a", 13, 8);
        MANSION_GATE("mansionb1f_gate_b",  6, 11);
        MANSION_GATE("mansionb1f_gate_c",  4, 3);
        MANSION_GATE("mansionb1f_gate_d",  8, 8);
    }
    #undef MANSION_GATE
}

void MansionScripts_SwitchInteract(void) {
    if ((gPlayerFacing & 3) != DIR_UP) return;
    if (s_state != MANSION_IDLE) return;
    YesNo_Show(kSwitchPrompt);
    s_state = MANSION_WAIT_YESNO;
}

static uint8_t mansion_cur_map(void) {
    int real = Map_CurrentRealId();
    return (uint8_t)(real >= 0 ? real : (int)wCurMap);
}

void MansionScripts_OnMapLoad(void) {
    s_state = MANSION_IDLE;
    apply_map_blocks(mansion_cur_map());
}

void MansionScripts_Tick(void) {
    if (s_state != MANSION_WAIT_YESNO) return;
    if (YesNo_IsOpen()) return;

    if (YesNo_GetResult()) {
        if (CheckEvent(EVENT_MANSION_SWITCH_ON)) ClearEvent(EVENT_MANSION_SWITCH_ON);
        else SetEvent(EVENT_MANSION_SWITCH_ON);
        Audio_PlaySFX_GoInside();
        Text_ShowASCII(kSwitchPressed);
        apply_map_blocks(mansion_cur_map());
        Map_BuildScrollView();
    } else {
        Text_ShowASCII(kSwitchNotPressed);
    }
    s_state = MANSION_IDLE;
}

int MansionScripts_IsActive(void) {
    return s_state != MANSION_IDLE;
}
