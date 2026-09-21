
#include "glitches.h"

static int s_enabled = 1;
static int s_trainer_fly_enabled = 1;
static int s_missingno_enabled = 1;

int  Glitches_IsEnabled(void)    { return s_enabled; }
void Glitches_SetEnabled(int on) { s_enabled = on ? 1 : 0; }

int Glitches_TrainerFlyEnabled(void) {
    return s_enabled && s_trainer_fly_enabled;
}

void Glitches_SetTrainerFlyEnabled(int on) {
    s_trainer_fly_enabled = on ? 1 : 0;
}

int Glitches_MissingNoEnabled(void) {
    return s_enabled && s_missingno_enabled;
}

void Glitches_SetMissingNoEnabled(int on) {
    s_missingno_enabled = on ? 1 : 0;
}
