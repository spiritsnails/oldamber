#include "launcher_location_names.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const struct { const char *internal; const char *display; } kAliases[] = {
    { "OaksLab", "Oak's Lab" },
    { "RedsHouse1F", "Red's House 1F" },
    { "RedsHouse2F", "Red's House 2F" },
    { "BluesHouse", "Blue's House" },
    { "BillsHouse", "Bill's House" },
    { "MrFujisHouse", "Mr. Fuji's House" },
    { "MrPsychicsHouse", "Mr. Psychic's House" },
    { "CopycatsHouse1F", "Copycat's House 1F" },
    { "CopycatsHouse2F", "Copycat's House 2F" },
    { "FuchsiaBillsGrandpasHouse", "Fuchsia - Bill's Grandpa's House" },
    { "SSAnne1F", "S.S. Anne 1F" },
    { "SSAnne2F", "S.S. Anne 2F" },
    { "SSAnne3F", "S.S. Anne 3F" },
    { "SSAnneB1F", "S.S. Anne B1F" },
    { "SSAnneBow", "S.S. Anne Bow" },
    { "SSAnneCaptainsRoom", "S.S. Anne - Captain's Room" },
    { "SSAnneKitchen", "S.S. Anne Kitchen" },
    { NULL, NULL }
};

void LauncherLocation_DisplayName(const char *vmap, char *out, size_t n) {
    size_t used = 0;
    if (!out || !n) return;
    out[0] = '\0';
    if (!vmap || !vmap[0]) return;
    for (int i = 0; kAliases[i].internal; i++) {
        if (strcmp(vmap, kAliases[i].internal) == 0) {
            snprintf(out, n, "%s", kAliases[i].display);
            return;
        }
    }
    for (size_t i = 0; vmap[i] && used + 1 < n; i++) {
        unsigned char c = (unsigned char)vmap[i];
        unsigned char prev = i ? (unsigned char)vmap[i - 1] : 0;
        unsigned char next = (unsigned char)vmap[i + 1];
        int split = i && (
            (isupper(c) && (islower(prev) ||
             (isupper(prev) && next && islower(next)))) ||
            (isdigit(c) && !isdigit(prev) && prev != 'B') ||
            (!isdigit(c) && isdigit(prev) && c != 'F'));
        if (split && used + 2 < n) out[used++] = ' ';
        out[used++] = (char)c;
    }
    out[used] = '\0';
}
