
#include "crystal_icon_anim.h"
#include "crystal_icons.h"

#define FRAME_COUNT    2
#define FRAME_DURATION 8

static const uint8_t kDurationOffset[3] = { 0x00, 0x40, 0x80 };

#define OAMFRAME_DURATION_MASK 0x3F

static const int8_t kHopHeight[3] = { -2, -1, 0 };

typedef struct {
    uint8_t frame;
    uint8_t duration;
    uint8_t offset;
    uint8_t var1;
    uint8_t var2;
} icon_anim_t;

static icon_anim_t s_anim[CRYSTAL_ICON_SLOTS];
static int s_selected = 0;

static uint8_t duration_for(const icon_anim_t *a) {

    return (uint8_t)((FRAME_DURATION & OAMFRAME_DURATION_MASK) + a->offset);
}

void CrystalIconAnim_Init(int slot, int hp_color) {
    icon_anim_t *a;
    if (slot < 0 || slot >= CRYSTAL_ICON_SLOTS) return;
    if (hp_color < 0 || hp_color > 2) hp_color = CRYSTAL_HP_GREEN;
    a = &s_anim[slot];
    a->frame  = 0;
    a->offset = kDurationOffset[hp_color];
    a->duration = duration_for(a);
    a->var1 = 0;

    a->var2 = (uint8_t)hp_color;
}

void CrystalIconAnim_SetSelected(int slot) { s_selected = slot; }

void CrystalIconAnim_Offset(int slot, int *dx, int *dy) {
    int ox = 0, oy = 0;
    if (slot >= 0 && slot < CRYSTAL_ICON_SLOTS && slot == s_selected) {
        const icon_anim_t *a = &s_anim[slot];

        ox = 8;

        if (a->var1 & 0x10)
            oy = (a->var2 < 3) ? kHopHeight[a->var2] : 0;
    }
    if (dx) *dx = ox;
    if (dy) *dy = oy;
}

void CrystalIconAnim_Reset(void) {
    for (int i = 0; i < CRYSTAL_ICON_SLOTS; i++) {
        s_anim[i].frame = 0;
        s_anim[i].duration = 0;
        s_anim[i].offset = 0;
    }
}

void CrystalIconAnim_Tick(void) {
    for (int i = 0; i < CRYSTAL_ICON_SLOTS; i++) {
        icon_anim_t *a = &s_anim[i];

        a->var1++;
        if (a->duration) {
            a->duration--;
            continue;
        }
        a->frame = (uint8_t)((a->frame + 1) % FRAME_COUNT);
        a->duration = duration_for(a);
    }
}

int CrystalIconAnim_Frame(int slot) {
    if (slot < 0 || slot >= CRYSTAL_ICON_SLOTS) return 0;
    return s_anim[slot].frame;
}

int CrystalIcon_ForDex(int dex) {

    int i = dex - 1;
    if (i < 0 || i >= CRYSTAL_ICON_SPECIES) return 0;
    {
        int icon = gCrystalSpeciesIcon[i];
        return (icon >= 0 && icon < CRYSTAL_NUM_ICONS) ? icon : 0;
    }
}
