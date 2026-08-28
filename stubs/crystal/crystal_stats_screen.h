
#pragma once
#include <stdint.h>

#define CRYSTAL_STATS_PALS 3

extern const uint16_t gCrystalStatsPageColor[CRYSTAL_STATS_PALS];

extern const char *const gCrystalStatsStatusType;
extern const char *const gCrystalStatsOk;
extern const char *const gCrystalStatsExpPoints;
extern const char *const gCrystalStatsLevelUp;
extern const char *const gCrystalStatsTo;
extern const char *const gCrystalStatsPkrs;
extern const char *const gCrystalStatsItem;
extern const char *const gCrystalStatsThreeDashes;
extern const char *const gCrystalStatsMove;
extern const char *const gCrystalStatsIDNo;
extern const char *const gCrystalStatsOT;

extern const uint8_t gCrystalStatsStatusTypeRaw[];
extern const uint8_t gCrystalStatsOkRaw[];
extern const uint8_t gCrystalStatsExpPointsRaw[];
extern const uint8_t gCrystalStatsLevelUpRaw[];
extern const uint8_t gCrystalStatsToRaw[];
extern const uint8_t gCrystalStatsPkrsRaw[];
extern const uint8_t gCrystalStatsItemRaw[];
extern const uint8_t gCrystalStatsThreeDashesRaw[];
extern const uint8_t gCrystalStatsMoveRaw[];
extern const uint8_t gCrystalStatsIDNoRaw[];
extern const uint8_t gCrystalStatsOTRaw[];

extern const uint16_t gCrystalStatsHPBarPals[3][4];
extern const uint16_t gCrystalStatsExpBarPal[1][4];
extern const uint16_t gCrystalStatsPagePals[CRYSTAL_STATS_PALS][4];

typedef struct {
    uint8_t dest_tile;
    uint8_t count;
    const uint8_t (*tiles)[16];
} crystal_stats_gfx_t;
#define CRYSTAL_STATS_GFX_SETS 5
extern const crystal_stats_gfx_t gCrystalStatsGFX[CRYSTAL_STATS_GFX_SETS];
