
#include "speed_settings.h"

extern int Game_GetScene(void);

static int s_move_anim = SPEED_NORMAL;
static int s_overworld = SPEED_NORMAL;
static int s_text      = SPEED_NORMAL;
static int s_misc_anim = SPEED_NORMAL;
static int s_battle    = SPEED_NORMAL;
static int s_transition = SPEED_NORMAL;
static int s_hp_bar    = SPEED_NORMAL;

static int s_rom_text_scroll = 0;

int  SpeedSettings_RomTextScroll(void)      { return s_rom_text_scroll; }
void SpeedSettings_SetRomTextScroll(int on) { s_rom_text_scroll = on ? 1 : 0; }

int SpeedSettings_MoveAnim(void) { return s_move_anim; }

void SpeedSettings_SetMoveAnim(int steps_per_frame) {

    if (steps_per_frame < SPEED_UNCAPPED) steps_per_frame = SPEED_UNCAPPED;
    if (steps_per_frame > SPEED_MAX)      steps_per_frame = SPEED_MAX;
    s_move_anim = steps_per_frame;
}

int SpeedSettings_MiscAnim(void) { return s_misc_anim; }

void SpeedSettings_SetMiscAnim(int steps_per_frame) {
    if (steps_per_frame < SPEED_UNCAPPED) steps_per_frame = SPEED_UNCAPPED;
    if (steps_per_frame > SPEED_MAX)      steps_per_frame = SPEED_MAX;
    s_misc_anim = steps_per_frame;
}

int SpeedSettings_Transition(void) { return s_transition; }

void SpeedSettings_SetTransition(int steps_per_frame) {
    if (steps_per_frame < SPEED_UNCAPPED) steps_per_frame = SPEED_UNCAPPED;
    if (steps_per_frame > SPEED_MAX)      steps_per_frame = SPEED_MAX;
    s_transition = steps_per_frame;
}

int SpeedSettings_Battle(void) { return s_battle; }

void SpeedSettings_SetBattle(int steps_per_frame) {
    if (steps_per_frame < SPEED_UNCAPPED) steps_per_frame = SPEED_UNCAPPED;
    if (steps_per_frame > SPEED_MAX)      steps_per_frame = SPEED_MAX;
    s_battle = steps_per_frame;

    SpeedSettings_SetMoveAnim(steps_per_frame);
    SpeedSettings_SetHpBar(steps_per_frame);
    SpeedSettings_SetMiscAnim(steps_per_frame);
    SpeedSettings_SetTransition(steps_per_frame);
}

int SpeedSettings_BattleIsCustom(void) {
    return !(s_move_anim  == s_battle &&
             s_hp_bar     == s_battle &&
             s_misc_anim  == s_battle &&
             s_transition == s_battle);
}

int SpeedSettings_Text(void) { return s_text; }

void SpeedSettings_SetText(int steps_per_frame) {

    if (steps_per_frame < SPEED_UNCAPPED) steps_per_frame = SPEED_UNCAPPED;
    if (steps_per_frame > SPEED_MAX)      steps_per_frame = SPEED_MAX;
    s_text = steps_per_frame;
}

int SpeedSettings_Overworld(void) { return s_overworld; }

void SpeedSettings_SetOverworld(int multiple) {

    if (multiple < SPEED_UNCAPPED) multiple = SPEED_UNCAPPED;
    if (multiple > SPEED_MAX)      multiple = SPEED_MAX;
    s_overworld = multiple;
}

int SpeedSettings_OverworldFramePct(void) {

    if (Game_GetScene() != 0) return 100;
    if (s_overworld == SPEED_UNCAPPED) return 0;
    return 100 * s_overworld;
}

int SpeedSettings_HpBar(void) { return s_hp_bar; }

void SpeedSettings_SetHpBar(int steps_per_frame) {

    if (steps_per_frame < SPEED_UNCAPPED) steps_per_frame = SPEED_UNCAPPED;
    if (steps_per_frame > SPEED_MAX)     steps_per_frame = SPEED_MAX;
    s_hp_bar = steps_per_frame;
}
