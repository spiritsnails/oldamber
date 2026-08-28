#include "badge.h"
#include "../platform/hardware.h"

int Badge_Has(int bit) {
    return (wObtainedBadges >> bit) & 1;
}

void Badge_Set(int bit) {
    wObtainedBadges |= (uint8_t)(1u << bit);
}
