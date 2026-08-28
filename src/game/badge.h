#pragma once
#include <stdint.h>

#define BADGE_BOULDER  0
#define BADGE_CASCADE  1
#define BADGE_THUNDER  2
#define BADGE_RAINBOW  3
#define BADGE_SOUL     4
#define BADGE_MARSH    5
#define BADGE_VOLCANO  6
#define BADGE_EARTH    7

int  Badge_Has(int bit);
void Badge_Set(int bit);
