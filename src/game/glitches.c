
#include "glitches.h"

static int s_enabled = 1;

int  Glitches_IsEnabled(void)    { return s_enabled; }
void Glitches_SetEnabled(int on) { s_enabled = on ? 1 : 0; }
