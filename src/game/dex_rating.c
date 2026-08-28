#include "dex_rating.h"

#include "rom_text.h"
#include "../platform/audio.h"
#include "../platform/hardware.h"

#include <stdio.h>

#define DEX_FLAG_BYTES 19

void DexRating_Counts(int *seen, int *owned) {
    int s = 0, o = 0;
    for (int i = 0; i < DEX_FLAG_BYTES; i++) {
        uint8_t sb = wPokedexSeen[i], ob = wPokedexOwned[i];
        while (sb) { s += sb & 1; sb >>= 1; }
        while (ob) { o += ob & 1; ob >>= 1; }
    }
    if (seen)  *seen  = s;
    if (owned) *owned = o;
}

static const char *const kDexRatingTiers[16] = {
    "DexRatingText_Own0To9",     "DexRatingText_Own10To19",
    "DexRatingText_Own20To29",   "DexRatingText_Own30To39",
    "DexRatingText_Own40To49",   "DexRatingText_Own50To59",
    "DexRatingText_Own60To69",   "DexRatingText_Own70To79",
    "DexRatingText_Own80To89",   "DexRatingText_Own90To99",
    "DexRatingText_Own100To109", "DexRatingText_Own110To119",
    "DexRatingText_Own120To129", "DexRatingText_Own130To139",
    "DexRatingText_Own140To149", "DexRatingText_Own150To151",
};

const char *DexRating_Text(void) {

    static char buf[320];
    char head[192], nseen[8], nown[8];
    int seen = 0, owned = 0, tier;

    DexRating_Counts(&seen, &owned);
    tier = owned / 10;
    if (tier > 15) tier = 15;

    snprintf(nseen, sizeof nseen, "%3d", seen);
    snprintf(nown,  sizeof nown,  "%3d", owned);
    RomTextSpliceN(head, sizeof head, "_DexCompletionText",
                   "{req}",      nseen,
                   "{num:FFDC}", nown, NULL);

    snprintf(buf, sizeof buf, "%s\f%s", head, RomText(kDexRatingTiers[tier]));
    return buf;
}

void DexRating_PlaySfx(void) {
    static const uint8_t kOwnedMonValues[6] = { 10, 40, 60, 90, 120, 150 };
    int owned = 0, i;

    DexRating_Counts(NULL, &owned);
    for (i = 0; i < 6; i++)
        if (owned < (int)kOwnedMonValues[i]) break;

    switch (i) {
    case 0:  Audio_PlaySFX_Denied();     break;
    case 1:  Audio_PlaySFX_DexRating();  break;
    case 2:  Audio_PlaySFX_GetItem1();   break;
    case 3:  Audio_PlaySFX_CaughtMon();  break;
    case 4:  Audio_PlaySFX_LevelUp();    break;
    case 5:  Audio_PlaySFX_GetKeyItem(); break;
    default: Audio_PlaySFX_GetItem2();   break;
    }
}
