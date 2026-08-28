#include <string.h>

#include "bicycle.h"
#include "constants.h"
#include "text.h"
#include "rom_text.h"
#include "player.h"
#include "music.h"
#include "amberscript_mapbank.h"
#include "amberscript_core.h"
#include "overworld.h"
#include "map_music.h"
#include "seafoam_scripts.h"
#include "../data/map_data.h"
#include "../platform/hardware.h"

#define MAP_ROUTE16             0x1b
#define MAP_ROUTE17             0x1c
#define MAP_ROUTE18             0x1d
#define MAP_ROUTE23             0x22
#define MAP_INDIGO_PLATEAU      0x24
#define MAP_SEAFOAM_ISLANDS_B3F 0xa1
#define MAP_SEAFOAM_ISLANDS_B4F 0xa2

static int map_is(uint8_t map, uint8_t rom_id, const char *vmap_name) {
    if (map == rom_id) return 1;
    if (map < PKS_VIRTUAL_MAP_FIRST || map > PKS_VIRTUAL_MAP_LAST) return 0;
    {

        const char *n = AmberScript_MapBank_NameForRealId(map);
        return n && strcasecmp(n, vmap_name) == 0;
    }
}

typedef struct {
    uint8_t     map_id;
    uint8_t     y;
    uint8_t     x;
    const char *vmap_name;
} forced_bike_surf_t;

static const forced_bike_surf_t kForcedBikeOrSurfMaps[] = {
    { MAP_ROUTE16,             10, 17, "Route16" },
    { MAP_ROUTE16,             11, 17, "Route16" },
    { MAP_ROUTE18,              8, 33, "Route18" },
    { MAP_ROUTE18,              9, 33, "Route18" },
    { MAP_SEAFOAM_ISLANDS_B3F,  7, 18, "SeafoamIslandsB3F" },
    { MAP_SEAFOAM_ISLANDS_B3F,  7, 19, "SeafoamIslandsB3F" },
    { MAP_SEAFOAM_ISLANDS_B4F, 14,  4, "SeafoamIslandsB4F" },
    { MAP_SEAFOAM_ISLANDS_B4F, 14,  5, "SeafoamIslandsB4F" },
    { 0xff, 0xff, 0xff, NULL },
};

static int s_always_on_bike = 0;

int Bicycle_IsCyclingRoad(void) {
    return map_is(wCurMap, MAP_ROUTE17, "Route17");
}

static int is_bike_riding_allowed(void) {
    if (map_is(wCurMap, MAP_ROUTE23, "Route23") ||
        map_is(wCurMap, MAP_INDIGO_PLATEAU, "IndigoPlateau")) return 1;

    if (AmberScript_MapBank_IsIndoorForRealId((int)wCurMap)) return 0;

    switch (wCurMapTileset) {
        case TILESET_OVERWORLD:
        case TILESET_FOREST:
        case TILESET_UNDERGROUND:
        case TILESET_SHIP_PORT:
        case TILESET_CAVERN:
            return 1;
        default:
            return 0;
    }
}

static void forced_bike_or_surf_scan(int seafoam_only);

void Bicycle_OnMapLoad(void) {

    if (wWalkBikeSurfState == 1 && !is_bike_riding_allowed()) {
        wWalkBikeSurfState = 0;
        Bicycle_PlayDefaultMusic();
    }

    if (s_always_on_bike) return;

    forced_bike_or_surf_scan(0);
}

void Bicycle_SeafoamCurrentStepCheck(void) {
    forced_bike_or_surf_scan(1);
}

static void forced_bike_or_surf_scan(int seafoam_only) {
    for (int i = 0; kForcedBikeOrSurfMaps[i].map_id != 0xff; i++) {
        const forced_bike_surf_t *e = &kForcedBikeOrSurfMaps[i];
        int is_seafoam = (e->map_id == MAP_SEAFOAM_ISLANDS_B3F ||
                          e->map_id == MAP_SEAFOAM_ISLANDS_B4F);
        if (seafoam_only && !is_seafoam) continue;
        if (!map_is(wCurMap, e->map_id, e->vmap_name)) continue;
        if (e->x != (uint8_t)wXCoord || e->y != (uint8_t)wYCoord) continue;

        if (is_seafoam) {

            wWalkBikeSurfState = 2;
            SeafoamScripts_ArmMoveObject();
        } else {
            s_always_on_bike = 1;
            wWalkBikeSurfState = 1;
        }
        return;
    }
}

void Bicycle_PlayDefaultMusic(void) {

    MapMusic_Play();
}

void Bicycle_ClearAlwaysOnBike(void) {
    s_always_on_bike = 0;
}

int Bicycle_UseFromBag(void) {
    if (wIsInBattle) return 0;
    if (wWalkBikeSurfState == 2) return 0;

    if (wWalkBikeSurfState == 1) {

        wWalkBikeSurfState = 0;
        Bicycle_PlayDefaultMusic();
        Text_ShowASCII("Got off\nthe BICYCLE.");
        return 1;
    }

    if (!is_bike_riding_allowed()) {
        Text_ShowASCII(RomText("NoCyclingAllowedHereText"));
        return 1;
    }

    hJoyHeld = 0;
    wWalkBikeSurfState = 1;
    Bicycle_PlayDefaultMusic();
    Text_ShowASCII("Got on\nthe BICYCLE.");
    return 1;
}

int Bicycle_IsSpeedupActive(void) {
    if (wWalkBikeSurfState != 1) return 0;

    if (Bicycle_IsCyclingRoad() && (hJoyHeld & (PAD_UP | PAD_LEFT | PAD_RIGHT))) return 0;
    return 1;
}

int Bicycle_ShouldUseBikeSprite(void) {
    if (wWalkBikeSurfState != 1) return 0;
    if (!is_bike_riding_allowed()) {
        wWalkBikeSurfState = 0;
        return 0;
    }
    return 1;
}
