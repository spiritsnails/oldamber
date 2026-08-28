
#include "gym_scripts.h"
#include "rom_text.h"
#include "assetpack_bind.h"
#include "badge.h"
#include "text.h"
#include "music.h"
#include "trainer_sight.h"
#include "battle/battle_ui.h"
#include "inventory.h"
#include "player.h"
#include "amberscript_mapbank.h"
#include "amberscript_tilemod.h"
#include "../data/event_constants.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include <stdio.h>
#include <stdint.h>

extern uint8_t wPlayerName[];

#define MAP_VIRIDIAN_GYM 0x2d
#define DIR_DOWN  0
#define DIR_UP    1
#define DIR_LEFT  2
#define DIR_RIGHT 3

typedef struct {
    uint8_t x, y;
    const int8_t *seq;
} viridian_spin_coord_t;

static int s_viridian_spin_active = 0;
static const int8_t *s_viridian_spin_seq = 0;
static int s_viridian_spin_idx = 0;

static int seq_last_idx(const int8_t *seq) {
    int i = 0;
    while (seq[i] != -1) i++;
    return i - 1;
}

static const int8_t kVGymM1[]  = { DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    -1 };
static const int8_t kVGymM2[]  = { DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  -1 };
static const int8_t kVGymM3[]  = { DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  -1 };
static const int8_t kVGymM4[]  = { DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kVGymM5[]  = { DIR_DOWN,  DIR_DOWN,  -1 };
static const int8_t kVGymM6[]  = { DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  -1 };
static const int8_t kVGymM7[]  = { DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kVGymM8[]  = { DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, -1 };
static const int8_t kVGymM9[]  = { DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    -1 };
static const int8_t kVGymM10[] = { DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    -1 };
static const int8_t kVGymM11[] = { DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  -1 };
static const int8_t kVGymM12[] = { DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  DIR_LEFT,  -1 };

static const viridian_spin_coord_t kVGymSpin[] = {
    { 19, 11, kVGymM1  },
    { 19,  1, kVGymM2  },
    { 18,  2, kVGymM3  },
    { 11,  2, kVGymM4  },
    { 16, 10, kVGymM5  },
    {  4,  6, kVGymM6  },
    {  5, 13, kVGymM7  },
    {  4, 14, kVGymM8  },
    {  0, 15, kVGymM9  },
    {  1, 15, kVGymM10 },
    { 13, 16, kVGymM11 },
    { 13, 17, kVGymM12 },
};

static const int8_t *find_viridian_spin_seq(uint8_t x, uint8_t y) {
    for (int i = 0; i < (int)(sizeof(kVGymSpin) / sizeof(kVGymSpin[0])); i++) {
        if (kVGymSpin[i].x == x && kVGymSpin[i].y == y) return kVGymSpin[i].seq;
    }
    return 0;
}

static const uint8_t kGymIdle_t060[16] = { 0xFF,0x00, 0x80,0x00, 0x80,0x00, 0x80,0x01, 0x80,0x03, 0x80,0x07, 0x80,0x0E, 0x80,0x1C };
static const uint8_t kGymIdle_t061[16] = { 0xFF,0x00, 0x01,0x00, 0x01,0x00, 0x01,0x80, 0x01,0xC0, 0x01,0xE0, 0x01,0x70, 0x01,0x38 };
static const uint8_t kGymIdle_t076[16] = { 0x80,0x1C, 0x80,0x0E, 0x80,0x07, 0x80,0x03, 0x80,0x01, 0x80,0x00, 0x80,0x00, 0xFF,0x00 };
static const uint8_t kGymIdle_t077[16] = { 0x01,0x38, 0x01,0x70, 0x01,0xE0, 0x01,0xC0, 0x01,0x80, 0x01,0x00, 0x01,0x00, 0xFF,0x00 };

static void apply_viridian_spinner_flicker(void) {
    int steps = Player_GetSimulatedStepsRemaining();

    int animated = (steps >= 0) && (((steps >> 1) & 1) == 0);

    AmberScript_SetSubtilePixels("viridiangym_t060", animated ? kSpinnerFrame1 : kGymIdle_t060);
    AmberScript_SetSubtilePixels("viridiangym_t061", animated ? kSpinnerFrame3 : kGymIdle_t061);
    AmberScript_SetSubtilePixels("viridiangym_t076", animated ? kSpinnerFrame0 : kGymIdle_t076);
    AmberScript_SetSubtilePixels("viridiangym_t077", animated ? kSpinnerFrame2 : kGymIdle_t077);
}

static char kBrockDefeatQuote[96];

#define kBrockBadgeInfo (RomText("_PewterGymBrockBoulderBadgeInfoText"))

#define BROCK_CLASS              34
#define BROCK_NO                  1
#define PEWTER_TRAINER_CLASS      5
#define PEWTER_TRAINER_NO         1

#define MISTY_CLASS              35
#define MISTY_NO                  1
#define CERULEAN_TRAINER0_CLASS   6
#define SURGE_CLASS              36
#define SURGE_NO                  1
#define ERIKA_CLASS              37
#define ERIKA_NO                  1
#define KOGA_CLASS               38
#define KOGA_NO                   1
#define SABRINA_CLASS            40
#define SABRINA_NO                1
#define JUGGLER_CLASS            21
#define TAMER_CLASS              22
#define BLACKBELT_CLASS          24
#define PSYCHIC_CLASS            19
#define CHANNELER_CLASS          45
#define COOLTRAINER_M_CLASS      31
#define CERULEAN_TRAINER0_NO      1
#define CERULEAN_TRAINER1_CLASS  15
#define CERULEAN_TRAINER1_NO      1
#define BLAINE_CLASS              39
#define BLAINE_NO                  1
#define GIOVANNI_CLASS            29
#define GIOVANNI_NO                3

typedef enum {
    GS_IDLE = 0,

    GS_GYM_LEADER_JINGLE_WAIT,

    GS_BROCK_PRE_TEXT,
    GS_BROCK_PRE_WAIT,
    GS_BROCK_START_BATTLE,
    GS_BROCK_POST_TEXT,
    GS_BROCK_POST_WAIT,
    GS_BROCK_TM_TEXT,
    GS_BROCK_TM_WAIT,
    GS_BROCK_TM_EXPLAIN,
    GS_BROCK_TM_EXP_WAIT,

    GS_MISTY_PRE_TEXT,
    GS_MISTY_PRE_WAIT,
    GS_MISTY_POST_TEXT,
    GS_MISTY_POST_WAIT,
    GS_MISTY_TM_WAIT,
    GS_MISTY_TM_EXPLAIN,
    GS_MISTY_TM_EXP_WAIT,

    GS_GYM_TRAINER_PRE_TEXT,
    GS_GYM_TRAINER_PRE_WAIT,
    GS_GYM_TRAINER_POST_TEXT,
    GS_GYM_TRAINER_POST_WAIT,

    GS_SURGE_PRE_TEXT,
    GS_SURGE_PRE_WAIT,
    GS_SURGE_POST_TEXT,
    GS_SURGE_POST_WAIT,
    GS_SURGE_TM_WAIT,
    GS_SURGE_TM_EXPLAIN,
    GS_SURGE_TM_EXP_WAIT,

    GS_ERIKA_PRE_TEXT,
    GS_ERIKA_PRE_WAIT,
    GS_ERIKA_POST_TEXT,
    GS_ERIKA_POST_WAIT,
    GS_ERIKA_TM_WAIT,
    GS_ERIKA_TM_EXPLAIN,
    GS_ERIKA_TM_EXP_WAIT,

    GS_SABRINA_PRE_TEXT,
    GS_SABRINA_PRE_WAIT,
    GS_SABRINA_POST_TEXT,
    GS_SABRINA_POST_WAIT,
    GS_SABRINA_TM_WAIT,
    GS_SABRINA_TM_EXPLAIN,
    GS_SABRINA_TM_EXP_WAIT,

    GS_KOGA_PRE_TEXT,
    GS_KOGA_PRE_WAIT,
    GS_KOGA_POST_TEXT,
    GS_KOGA_POST_WAIT,
    GS_KOGA_TM_NOROOM_WAIT,
    GS_KOGA_TM_WAIT,
    GS_KOGA_TM_EXPLAIN,
    GS_KOGA_TM_EXP_WAIT,

    GS_BLAINE_PRE_TEXT,
    GS_BLAINE_PRE_WAIT,
    GS_BLAINE_POST_TEXT,
    GS_BLAINE_POST_WAIT,
    GS_BLAINE_TM_WAIT,
    GS_BLAINE_TM_EXPLAIN,
    GS_BLAINE_TM_EXP_WAIT,

    GS_GIOVANNI_PRE_TEXT,
    GS_GIOVANNI_PRE_WAIT,
    GS_GIOVANNI_POST_TEXT,
    GS_GIOVANNI_POST_WAIT,
    GS_GIOVANNI_TM_WAIT,
    GS_GIOVANNI_TM_EXPLAIN,
    GS_GIOVANNI_TM_EXP_WAIT,

    GS_GUIDE_TEXT,
    GS_GUIDE_WAIT,
} GymState;

static GymState     gState                  = GS_IDLE;
static int          gPostFadeTimer          = 0;
static int          gPendingBattle          = 0;
static uint8_t      gPendingClass           = 0;
static uint8_t      gPendingNo              = 0;
       int          gGymTrainerBattlePending = 0;

static int          s_gym_jingle_wait       = 0;
static uint8_t      s_gym_pending_leader_no = 0;

static uint8_t      gGymTrainerClass        = 0;
static uint8_t      gGymTrainerNo           = 0;
       uint32_t     gGymTrainerVictoryEvent = 0;
       const char  *gGymTrainerEndText      = NULL;
static const char  *gGymTrainerAfterText    = NULL;

#define kBrockPre (RomText("PewterGymBrockText.PreBattleText"))

#define kBrockPost (RomText("PewterGymBrockWaitTakeThisText"))

#define kBrockTMExplain (RomText("_TM34ExplanationText"))

#define kBrockAfter (RomText("PewterGymBrockText.PostBattleAdviceText"))

#define kGymTrainerPre (RomText("PewterGymCooltrainerMBattleText"))

#define kGymTrainerEnd (RomText("PewterGymCooltrainerMEndBattleText"))

#define kGymTrainerAfter (RomText("PewterGymCooltrainerMAfterBattleText"))

#define kMistyPre (RomText("CeruleanGymMistyText.PreBattleText"))

#define kMistyDefeatQuote (RomTextPrefixed("MISTY: ", "CeruleanGymMistyReceivedCascadeBadgeText"))

static char kMistyBadgeRecv[48];

#define kMistyBadgeInfo (RomText("CeruleanGymMistyCascadeBadgeInfoText"))

#define kMistyAfter (RomText("CeruleanGymMistyText.TM11ExplanationText"))

#define kCeruleanTrainer0Pre   (RomText("CeruleanGymBattleText1"))
#define kCeruleanTrainer0End (RomText("CeruleanGymEndBattleText1"))
#define kCeruleanTrainer0After (RomText("CeruleanGymAfterBattleText1"))

#define kCeruleanTrainer1Pre   (RomText("CeruleanGymBattleText2"))
#define kCeruleanTrainer1End (RomText("CeruleanGymEndBattleText2"))
#define kCeruleanTrainer1After (RomText("CeruleanGymAfterBattleText2"))

#define kCeruleanGuidePre (RomText("CeruleanGymGymGuideText.ChampInMakingText"))

#define kCeruleanGuidePost (RomText("CeruleanGymGymGuideText.BeatMistyText"))

#define kSurgePre (RomText("VermilionGymLTSurgeText.PreBattleText"))

#define kSurgePost (RomText("VermilionGymLTSurgeThunderBadgeInfoText"))

#define kSurgeAfter (RomText("VermilionGymLTSurgeText.PostBattleAdviceText"))

static char kSurgeDefeatQuote[96];

static char kSurgeBadgeRecv[48];

#define kSurgeTMExplain (RomText("_TM24ExplanationText"))

#define kErikaPre (RomText("CeladonGymErikaText.PreBattleText"))

#define kErikaDefeatQuote (RomTextPrefixed("ERIKA: ", "CeladonGymErikaText.ReceivedRainbowBadgeText"))

static char kErikaBadgeRecv[48];

#define kErikaBadgeInfo (RomText("CeladonGymRainbowBadgeInfoText"))

#define kErikaTMExplain (RomText("_TM21ExplanationText"))

#define kErikaAfter (RomText("CeladonGymErikaText.PostBattleAdviceText"))

#define kKogaPre (RomText("FuchsiaGymKogaText.BeforeBattleText"))

#define kKogaDefeatQuote (RomTextPrefixed("KOGA: ", "FuchsiaGymKogaText.ReceivedSoulBadgeText"))

static char kKogaBadgeRecv[48];
#define kKogaBadgeInfo (RomText("FuchsiaGymKogaSoulBadgeInfoText"))
#define kKogaTMExplain (RomText("_FuchsiaGymKogaTM06ExplanationText"))
#define kKogaTMNoRoom (RomText("FuchsiaGymKogaTM06NoRoomText"))
#define kKogaAfter (RomText("FuchsiaGymKogaText.PostBattleAdviceText"))

#define kBlainePre (RomText("CinnabarGymBlaineText.PreBattleText"))

#define kBlaineDefeatQuote (RomTextPrefixed("BLAINE: ", "CinnabarGymBlaineText.ReceivedVolcanoBadgeText"))

static char kBlaineBadgeRecv[48];
#define kBlaineBadgeInfo (RomText("CinnabarGymBlaineVolcanoBadgeInfoText"))
#define kBlaineTMNoRoom (RomText("CinnabarGymBlaineTM38NoRoomText"))

static const char *kBlaineTMExplain(void) {
    static char buf[128];
    if (!buf[0])
        snprintf(buf, sizeof buf, "%s\f%s\f%s",
                 RomTextPage("CinnabarGymBlaineReceivedTM38Text", 1),
                 RomTextPage("CinnabarGymBlaineReceivedTM38Text", 2),
                 RomTextPage("CinnabarGymBlaineReceivedTM38Text", 3));
    return buf;
}
#define kBlaineAfter (RomText("CinnabarGymBlaineText.PostBattleAdviceText"))

#define kSabrinaPre (RomText("SaffronGymSabrinaText.Text"))

#define kSabrinaDefeatQuote (RomTextPrefixed("SABRINA: ", "SaffronGymSabrinaText.ReceivedMarshBadgeText"))
static char kSabrinaBadgeRecv[48];
#define kSabrinaBadgeInfo (RomText("SaffronGymSabrinaMarshBadgeInfoText"))
#define kSabrinaTMExplain (RomText("_TM46ExplanationText"))
#define kSabrinaTMNoRoom (RomText("SaffronGymSabrinaTM46NoRoomText"))
#define kSabrinaAfter (RomText("SaffronGymSabrinaText.PostBattleAdviceText"))

#define kSaffronGuidePre (RomText("SaffronGymGymGuideText.ChampInMakingText"))
#define kSaffronGuidePost (RomText("SaffronGymGymGuideText.BeatSabrinaText"))

#define kSaffronT0Pre (RomText("SaffronGymChanneler1BattleText"))
#define kSaffronT0End (RomText("Route14CooltrainerM1EndBattleText"))
#define kSaffronT0After (RomText("SaffronGymChanneler1AfterBattleText"))
#define kSaffronT1Pre (RomText("SaffronGymYoungster1BattleText"))
#define kSaffronT1End (RomText("SaffronGymYoungster1EndBattleText"))
#define kSaffronT1After (RomText("SaffronGymYoungster1AfterBattleText"))
#define kSaffronT2Pre (RomText("SaffronGymChanneler2BattleText"))
#define kSaffronT2End (RomText("SaffronGymChanneler2EndBattleText"))
#define kSaffronT2After (RomText("SaffronGymChanneler2AfterBattleText"))
#define kSaffronT3Pre (RomText("SaffronGymYoungster2BattleText"))
#define kSaffronT3End (RomText("SaffronGymYoungster2EndBattleText"))
#define kSaffronT3After (RomText("SaffronGymYoungster2AfterBattleText"))
#define kSaffronT4Pre (RomText("SaffronGymChanneler3BattleText"))
#define kSaffronT4End (RomText("SaffronGymChanneler3EndBattleText"))
#define kSaffronT4After (RomText("SaffronGymChanneler3AfterBattleText"))
#define kSaffronT5Pre (RomText("SaffronGymYoungster3BattleText"))
#define kSaffronT5End (RomText("SaffronGymYoungster3EndBattleText"))
#define kSaffronT5After (RomText("SaffronGymYoungster3AfterBattleText"))
#define kSaffronT6Pre (RomText("SaffronGymYoungster4BattleText"))
#define kSaffronT6End (RomText("SaffronGymYoungster4EndBattleText"))
#define kSaffronT6After (RomText("SaffronGymYoungster4AfterBattleText"))

#define kFuchsiaT1Pre (RomText("FuchsiaGymRocker1BattleText"))
#define kFuchsiaT1End (RomText("FuchsiaGymRocker1EndBattleText"))
#define kFuchsiaT1After (RomText("FuchsiaGymRocker1AfterBattleText"))

#define kFuchsiaT2Pre (RomText("FuchsiaGymRocker2BattleText"))
#define kFuchsiaT2End (RomText("FuchsiaGymRocker2EndBattleText"))
#define kFuchsiaT2After (RomText("FuchsiaGymRocker2AfterBattleText"))

#define kFuchsiaT3Pre (RomText("FuchsiaGymRocker3BattleText"))
#define kFuchsiaT3End (RomText("FuchsiaGymRocker3EndBattleText"))
#define kFuchsiaT3After (RomText("FuchsiaGymRocker3AfterBattleText"))

#define kFuchsiaT4Pre (RomText("FuchsiaGymRocker4BattleText"))
#define kFuchsiaT4End (RomText("FuchsiaGymRocker4EndBattleText"))
#define kFuchsiaT4After (RomText("FuchsiaGymRocker4AfterBattleText"))

#define kFuchsiaT5Pre (RomText("FuchsiaGymRocker5BattleText"))
#define kFuchsiaT5End (RomText("FuchsiaGymRocker5EndBattleText"))
#define kFuchsiaT5After (RomText("FuchsiaGymRocker5AfterBattleText"))

#define kFuchsiaT6Pre (RomText("FuchsiaGymRocker6BattleText"))
#define kFuchsiaT6End (RomText("FuchsiaGymRocker6EndBattleText"))
#define kFuchsiaT6After (RomText("FuchsiaGymRocker6AfterBattleText"))

#define kFuchsiaGuidePre (RomText("FuchsiaGymGymGuideText.ChampInMakingText"))

#define kFuchsiaGuidePost (RomText("FuchsiaGymGymGuideText.BeatKogaText"))

#define kGiovanniPre (RomText("ViridianGymGiovanniText.PreBattleText"))

#define kGiovanniDefeatQuote (RomTextPrefixed("GIOVANNI: ", "ViridianGymGiovanniText.ReceivedEarthBadgeText"))

static char kGiovanniBadgeRecv[48];

#define kGiovanniBadgeInfo (RomText("ViridianGymGiovanniEarthBadgeInfoText"))

#define kGiovanniTMExplain (RomText("ViridianGymGiovanniTM27ExplanationText"))

#define kGiovanniTMNoRoom (RomText("ViridianGymGiovanniTM27NoRoomText"))

#define kGiovanniAfter (RomText("ViridianGymGiovanniText.PostBattleAdviceText"))

#define kViridianGuidePre (RomText("ViridianGymGuidePreBattleText"))

#define kViridianGuidePost (RomText("ViridianGymGuidePostBattleText"))

#define kViridianT0Pre (RomText("ViridianGymCooltrainerM1BattleText"))
#define kViridianT0End (RomText("ViridianGymCooltrainerM1EndBattleText"))
#define kViridianT0After (RomText("ViridianGymCooltrainerM1AfterBattleText"))
#define kViridianT1Pre (RomText("ViridianGymHiker1BattleText"))
#define kViridianT1End (RomText("ViridianGymHiker1EndBattleText"))
#define kViridianT1After (RomText("ViridianGymHiker1AfterBattleText"))
#define kViridianT2Pre (RomText("ViridianGymRocker1BattleText"))
#define kViridianT2End (RomText("ViridianGymRocker1EndBattleText"))
#define kViridianT2After (RomText("ViridianGymRocker1AfterBattleText"))
#define kViridianT3Pre (RomText("ViridianGymHiker2BattleText"))
#define kViridianT3End (RomText("ViridianGymHiker2EndBattleText"))
#define kViridianT3After (RomText("ViridianGymHiker2AfterBattleText"))
#define kViridianT4Pre (RomText("ViridianGymCooltrainerM2BattleText"))
#define kViridianT4End (RomText("ViridianGymCooltrainerM2EndBattleText"))
#define kViridianT4After (RomText("ViridianGymCooltrainerM2AfterBattleText"))
#define kViridianT5Pre (RomText("ViridianGymHiker3BattleText"))
#define kViridianT5End (RomText("PokemonMansion3FSuperNerdEndBattleText"))
#define kViridianT5After (RomText("ViridianGymHiker3AfterBattleText"))
#define kViridianT6Pre (RomText("ViridianGymRocker2BattleText"))
#define kViridianT6End (RomText("ViridianGymRocker2EndBattleText"))
#define kViridianT6After (RomText("ViridianGymRocker2AfterBattleText"))
#define kViridianT7Pre (RomText("ViridianGymCooltrainerM3BattleText"))
#define kViridianT7End (RomText("ViridianGymCooltrainerM3EndBattleText"))
#define kViridianT7After (RomText("ViridianGymCooltrainerM3AfterBattleText"))

#define kGuidePre (RomText("PewterGymGuidePreAdviceText"))

#define kGuidePost (RomText("PewterGymGuidePostBattleText"))

void GymScripts_OnMapLoad(void) {

}

void GymScripts_Tick(void) {
    switch (gState) {
    case GS_IDLE:
        return;

    case GS_GYM_LEADER_JINGLE_WAIT:
        if (--s_gym_jingle_wait > 0) return;
        wGymLeaderNo   = s_gym_pending_leader_no;
        gPendingBattle = 1;
        gState         = GS_IDLE;
        return;

    case GS_BROCK_PRE_TEXT:

        gState = GS_BROCK_PRE_WAIT;
        return;

    case GS_BROCK_PRE_WAIT:
        if (Text_IsOpen()) return;

        {

            snprintf(kBrockDefeatQuote, sizeof(kBrockDefeatQuote),
                     "BROCK: %s\f%s",
                     RomTextPage("PewterGymBrockReceivedBoulderBadgeText", 0),
                     RomTextPage("PewterGymBrockReceivedBoulderBadgeText", 1));

            gTrainerAfterText = kBrockDefeatQuote;
            BattleUI_SetBadgeRecvText(
                RomTextPage("PewterGymBrockReceivedBoulderBadgeText", 2));
            BattleUI_SetBadgeInfoText(kBrockBadgeInfo);
        }

        s_gym_pending_leader_no = 1;
        gPendingClass   = BROCK_CLASS;
        gPendingNo      = BROCK_NO;
        Trainer_PlayEncounterMusic(BROCK_CLASS);
        s_gym_jingle_wait = 24;
        gState          = GS_GYM_LEADER_JINGLE_WAIT;
        return;

    case GS_BROCK_START_BATTLE:

        gState = GS_IDLE;
        return;

    case GS_BROCK_POST_TEXT:
        if (Text_IsOpen()) return;
        if (gPostFadeTimer > 0) { gPostFadeTimer--; return; }
        Text_ShowASCII(kBrockPost);
        gState = GS_BROCK_POST_WAIT;
        return;

    case GS_BROCK_POST_WAIT:
        if (Text_IsOpen()) return;

        Inventory_Add(TM01 + 33, 1);
        Audio_PlaySFX_GetItem1();
        Text_ShowASCII(RomText("_PewterGymReceivedTM34Text"));
        gState = GS_BROCK_TM_WAIT;
        return;

    case GS_BROCK_TM_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_BROCK_TM_EXPLAIN;
        return;

    case GS_BROCK_TM_EXPLAIN:
        Text_ShowASCII(kBrockTMExplain);
        gState = GS_BROCK_TM_EXP_WAIT;
        return;

    case GS_BROCK_TM_EXP_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_IDLE;
        return;

    case GS_GYM_TRAINER_PRE_TEXT:
        gState = GS_GYM_TRAINER_PRE_WAIT;
        return;

    case GS_MISTY_PRE_TEXT:
        gState = GS_MISTY_PRE_WAIT;
        return;

    case GS_MISTY_PRE_WAIT:
        if (Text_IsOpen()) return;

        {
            char player_ascii[12] = "RED";
            for (int i = 0; i < 11; i++) {
                uint8_t c = wPlayerName[i];
                if (c == 0x50) break;
                if (c >= 0x80 && c <= 0x99)      player_ascii[i] = (char)('A' + c - 0x80);
                else if (c >= 0xA0 && c <= 0xB9) player_ascii[i] = (char)('a' + c - 0xA0);
                else { player_ascii[i] = '?'; }
                player_ascii[i + 1] = '\0';
            }
            snprintf(kMistyBadgeRecv, sizeof(kMistyBadgeRecv),
                     "%s received\nthe CASCADEBADGE!", player_ascii);
        }
        gTrainerAfterText = kMistyDefeatQuote;
        BattleUI_SetBadgeRecvText(kMistyBadgeRecv);

        s_gym_pending_leader_no = 2;
        gPendingClass  = MISTY_CLASS;
        gPendingNo     = MISTY_NO;
        Trainer_PlayEncounterMusic(MISTY_CLASS);
        s_gym_jingle_wait = 24;
        gState         = GS_GYM_LEADER_JINGLE_WAIT;
        return;

    case GS_MISTY_POST_TEXT:

        if (Text_IsOpen()) return;
        if (gPostFadeTimer > 0) { gPostFadeTimer--; return; }
        Text_ShowASCII(kMistyBadgeInfo);
        gState = GS_MISTY_POST_WAIT;
        return;

    case GS_MISTY_POST_WAIT:

        if (Text_IsOpen()) return;
        Inventory_Add(TM01 + 10, 1);
        SetEvent(EVENT_GOT_TM11);
        Audio_PlaySFX_GetItem1();
        Text_ShowASCII(RomText("_CeruleanGymMistyReceivedTM11Text"));
        gState = GS_MISTY_TM_WAIT;
        return;

    case GS_MISTY_TM_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_MISTY_TM_EXPLAIN;
        return;

    case GS_MISTY_TM_EXPLAIN:

        Text_ShowASCII(kMistyAfter);
        gState = GS_MISTY_TM_EXP_WAIT;
        return;

    case GS_MISTY_TM_EXP_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_IDLE;
        return;

    case GS_SURGE_PRE_TEXT:
        gState = GS_SURGE_PRE_WAIT;
        return;

    case GS_SURGE_PRE_WAIT:
        if (Text_IsOpen()) return;

        {
            char playerName[12] = "RED";
            for (int i = 0; i < 11; i++) {
                uint8_t c = wPlayerName[i];
                if (c == 0x50) break;
                if (c >= 0x80 && c <= 0x99)      playerName[i] = (char)('A' + c - 0x80);
                else if (c >= 0xA0 && c <= 0xB9) playerName[i] = (char)('a' + c - 0xA0);
                else { playerName[i] = '?'; }
                playerName[i + 1] = '\0';
            }
            snprintf(kSurgeDefeatQuote, sizeof(kSurgeDefeatQuote),
                "LT.SURGE: Whoa!"
                "\fYou're the real\ndeal, kid!"
                "\fFine then, take\nthe THUNDERBADGE!");
            snprintf(kSurgeBadgeRecv, sizeof(kSurgeBadgeRecv),
                "%s received\nthe THUNDERBADGE!", playerName);
            gTrainerAfterText = kSurgeDefeatQuote;
            BattleUI_SetBadgeRecvText(kSurgeBadgeRecv);

        }
        s_gym_pending_leader_no = 3;
        gPendingClass  = SURGE_CLASS;
        gPendingNo     = SURGE_NO;
        Trainer_PlayEncounterMusic(SURGE_CLASS);
        s_gym_jingle_wait = 24;
        gState         = GS_GYM_LEADER_JINGLE_WAIT;
        return;

    case GS_SURGE_POST_TEXT:
        if (Text_IsOpen()) return;
        if (gPostFadeTimer > 0) { gPostFadeTimer--; return; }
        Text_ShowASCII(kSurgePost);
        gState = GS_SURGE_POST_WAIT;
        return;

    case GS_SURGE_POST_WAIT:

        if (Text_IsOpen()) return;
        Text_SetItemName(TM01 + 23);
        Inventory_Add(TM01 + 23, 1);
        SetEvent(EVENT_GOT_TM24);
        Audio_PlaySFX_GetItem1();
        Text_ShowASCII(RomText("_VermilionGymLTSurgeReceivedTM24Text"));
        gState = GS_SURGE_TM_WAIT;
        return;

    case GS_SURGE_TM_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_SURGE_TM_EXPLAIN;
        return;

    case GS_SURGE_TM_EXPLAIN:

        Text_ShowASCII(kSurgeTMExplain);
        gState = GS_SURGE_TM_EXP_WAIT;
        return;

    case GS_SURGE_TM_EXP_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_IDLE;
        return;

    case GS_ERIKA_PRE_TEXT:
        gState = GS_ERIKA_PRE_WAIT;
        return;

    case GS_ERIKA_PRE_WAIT:
        if (Text_IsOpen()) return;

        {
            char playerName[12] = "RED";
            for (int i = 0; i < 11; i++) {
                uint8_t c = wPlayerName[i];
                if (c == 0x50) break;
                if (c >= 0x80 && c <= 0x99)      playerName[i] = (char)('A' + c - 0x80);
                else if (c >= 0xA0 && c <= 0xB9) playerName[i] = (char)('a' + c - 0xA0);
                else { playerName[i] = '?'; }
                playerName[i + 1] = '\0';
            }
            snprintf(kErikaBadgeRecv, sizeof(kErikaBadgeRecv),
                     "%s received\nthe RAINBOWBADGE!", playerName);
        }
        gTrainerAfterText = kErikaDefeatQuote;
        BattleUI_SetBadgeRecvText(kErikaBadgeRecv);
        s_gym_pending_leader_no = 4;
        gPendingClass  = ERIKA_CLASS;
        gPendingNo     = ERIKA_NO;
        Trainer_PlayEncounterMusic(ERIKA_CLASS);
        s_gym_jingle_wait = 24;
        gState         = GS_GYM_LEADER_JINGLE_WAIT;
        return;

    case GS_ERIKA_POST_TEXT:
        if (Text_IsOpen()) return;
        if (gPostFadeTimer > 0) { gPostFadeTimer--; return; }
        Text_ShowASCII(kErikaBadgeInfo);
        gState = GS_ERIKA_POST_WAIT;
        return;

    case GS_ERIKA_POST_WAIT:
        if (Text_IsOpen()) return;
        Text_SetItemName(TM01 + 20);
        Inventory_Add(TM01 + 20, 1);
        SetEvent(EVENT_GOT_TM21);
        Audio_PlaySFX_GetItem1();
        Text_ShowASCII(RomText("_CeladonGymReceivedTM21Text"));
        gState = GS_ERIKA_TM_WAIT;
        return;

    case GS_ERIKA_TM_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_ERIKA_TM_EXPLAIN;
        return;

    case GS_ERIKA_TM_EXPLAIN:
        Text_ShowASCII(kErikaTMExplain);
        gState = GS_ERIKA_TM_EXP_WAIT;
        return;

    case GS_ERIKA_TM_EXP_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_IDLE;
        return;

    case GS_SABRINA_PRE_TEXT:
        gState = GS_SABRINA_PRE_WAIT;
        return;

    case GS_SABRINA_PRE_WAIT:
        if (Text_IsOpen()) return;
        {
            char playerName[12] = "RED";
            for (int i = 0; i < 11; i++) {
                uint8_t c = wPlayerName[i];
                if (c == 0x50) break;
                if (c >= 0x80 && c <= 0x99)      playerName[i] = (char)('A' + c - 0x80);
                else if (c >= 0xA0 && c <= 0xB9) playerName[i] = (char)('a' + c - 0xA0);
                else { playerName[i] = '?'; }
                playerName[i + 1] = '\0';
            }
            snprintf(kSabrinaBadgeRecv, sizeof(kSabrinaBadgeRecv),
                     "%s received\nthe MARSHBADGE!", playerName);
        }
        gTrainerAfterText = kSabrinaDefeatQuote;
        BattleUI_SetBadgeRecvText(kSabrinaBadgeRecv);
        s_gym_pending_leader_no = 6;
        gPendingClass  = SABRINA_CLASS;
        gPendingNo     = SABRINA_NO;
        Trainer_PlayEncounterMusic(SABRINA_CLASS);
        s_gym_jingle_wait = 24;
        gState         = GS_GYM_LEADER_JINGLE_WAIT;
        return;

    case GS_SABRINA_POST_TEXT:
        if (Text_IsOpen()) return;
        if (gPostFadeTimer > 0) { gPostFadeTimer--; return; }
        Text_ShowASCII(kSabrinaBadgeInfo);
        gState = GS_SABRINA_POST_WAIT;
        return;

    case GS_SABRINA_POST_WAIT:
        if (Text_IsOpen()) return;
        if (Inventory_Add(TM01 + 45, 1) != 0) {
            Text_ShowASCII(kSabrinaTMNoRoom);
            gState = GS_SABRINA_TM_EXP_WAIT;
            return;
        }
        SetEvent(EVENT_GOT_TM46);
        Audio_PlaySFX_GetItem1();
        Text_ShowASCII(RomText("_SaffronGymSabrinaReceivedTM46Text"));
        gState = GS_SABRINA_TM_WAIT;
        return;

    case GS_SABRINA_TM_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_SABRINA_TM_EXPLAIN;
        return;

    case GS_SABRINA_TM_EXPLAIN:
        Text_ShowASCII(kSabrinaTMExplain);
        gState = GS_SABRINA_TM_EXP_WAIT;
        return;

    case GS_SABRINA_TM_EXP_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_IDLE;
        return;

    case GS_KOGA_PRE_TEXT:
        gState = GS_KOGA_PRE_WAIT;
        return;

    case GS_KOGA_PRE_WAIT:
        if (Text_IsOpen()) return;
        {
            char playerName[12] = "RED";
            for (int i = 0; i < 11; i++) {
                uint8_t c = wPlayerName[i];
                if (c == 0x50) break;
                if (c >= 0x80 && c <= 0x99)      playerName[i] = (char)('A' + c - 0x80);
                else if (c >= 0xA0 && c <= 0xB9) playerName[i] = (char)('a' + c - 0xA0);
                else { playerName[i] = '?'; }
                playerName[i + 1] = '\0';
            }
            snprintf(kKogaBadgeRecv, sizeof(kKogaBadgeRecv),
                     "%s received\nthe SOULBADGE!", playerName);
        }
        gTrainerAfterText = kKogaDefeatQuote;
        BattleUI_SetBadgeRecvText(kKogaBadgeRecv);
        s_gym_pending_leader_no = 5;
        gPendingClass  = KOGA_CLASS;
        gPendingNo     = KOGA_NO;
        Trainer_PlayEncounterMusic(KOGA_CLASS);
        s_gym_jingle_wait = 24;
        gState         = GS_GYM_LEADER_JINGLE_WAIT;
        return;

    case GS_KOGA_POST_TEXT:
        if (Text_IsOpen()) return;
        if (gPostFadeTimer > 0) { gPostFadeTimer--; return; }
        Text_ShowASCII(kKogaBadgeInfo);
        gState = GS_KOGA_POST_WAIT;
        return;

    case GS_KOGA_POST_WAIT:
        if (Text_IsOpen()) return;
        if (Inventory_Add(TM01 + 5, 1) != 0) {
            Text_ShowASCII(kKogaTMNoRoom);
            gState = GS_KOGA_TM_NOROOM_WAIT;
            return;
        }
        SetEvent(EVENT_GOT_TM06);
        Audio_PlaySFX_GetItem1();
        Text_SetItemName(TM01 + 5);
        Text_ShowASCII(RomText("_FuchsiaGymKogaReceivedTM06Text"));
        gState = GS_KOGA_TM_WAIT;
        return;

    case GS_KOGA_TM_NOROOM_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_IDLE;
        return;

    case GS_KOGA_TM_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_KOGA_TM_EXPLAIN;
        return;

    case GS_KOGA_TM_EXPLAIN:
        Text_ShowASCII(kKogaTMExplain);
        gState = GS_KOGA_TM_EXP_WAIT;
        return;

    case GS_KOGA_TM_EXP_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_IDLE;
        return;

    case GS_BLAINE_PRE_TEXT:
        gState = GS_BLAINE_PRE_WAIT;
        return;

    case GS_BLAINE_PRE_WAIT:
        if (Text_IsOpen()) return;
        {
            char playerName[12] = "RED";
            for (int i = 0; i < 11; i++) {
                uint8_t c = wPlayerName[i];
                if (c == 0x50) break;
                if (c >= 0x80 && c <= 0x99)      playerName[i] = (char)('A' + c - 0x80);
                else if (c >= 0xA0 && c <= 0xB9) playerName[i] = (char)('a' + c - 0xA0);
                else { playerName[i] = '?'; }
                playerName[i + 1] = '\0';
            }
            snprintf(kBlaineBadgeRecv, sizeof(kBlaineBadgeRecv),
                     "%s received\nthe VOLCANOBADGE!", playerName);
        }
        gTrainerAfterText = kBlaineDefeatQuote;
        BattleUI_SetBadgeRecvText(kBlaineBadgeRecv);
        s_gym_pending_leader_no = 7;
        gPendingClass  = BLAINE_CLASS;
        gPendingNo     = BLAINE_NO;
        Trainer_PlayEncounterMusic(BLAINE_CLASS);
        s_gym_jingle_wait = 24;
        gState         = GS_GYM_LEADER_JINGLE_WAIT;
        return;

    case GS_BLAINE_POST_TEXT:
        if (Text_IsOpen()) return;
        if (gPostFadeTimer > 0) { gPostFadeTimer--; return; }
        Text_ShowASCII(kBlaineBadgeInfo);
        gState = GS_BLAINE_POST_WAIT;
        return;

    case GS_BLAINE_POST_WAIT:
        if (Text_IsOpen()) return;
        if (Inventory_Add(TM01 + 37, 1) != 0) {
            Text_ShowASCII(kBlaineTMNoRoom);
            gState = GS_BLAINE_TM_EXP_WAIT;
            return;
        }
        SetEvent(EVENT_GOT_TM38);
        Audio_PlaySFX_GetItem1();
        Text_SetItemName(TM01 + 37);
        Text_ShowASCII(RomText("_CinnabarGymBlaineReceivedTM38Text"));
        gState = GS_BLAINE_TM_WAIT;
        return;

    case GS_BLAINE_TM_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_BLAINE_TM_EXPLAIN;
        return;

    case GS_BLAINE_TM_EXPLAIN:
        Text_ShowASCII(kBlaineTMExplain());
        gState = GS_BLAINE_TM_EXP_WAIT;
        return;

    case GS_BLAINE_TM_EXP_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_IDLE;
        return;

    case GS_GIOVANNI_PRE_TEXT:
        gState = GS_GIOVANNI_PRE_WAIT;
        return;

    case GS_GIOVANNI_PRE_WAIT:
        if (Text_IsOpen()) return;
        {
            char playerName[12] = "RED";
            for (int i = 0; i < 11; i++) {
                uint8_t c = wPlayerName[i];
                if (c == 0x50) break;
                if (c >= 0x80 && c <= 0x99)      playerName[i] = (char)('A' + c - 0x80);
                else if (c >= 0xA0 && c <= 0xB9) playerName[i] = (char)('a' + c - 0xA0);
                else { playerName[i] = '?'; }
                playerName[i + 1] = '\0';
            }
            snprintf(kGiovanniBadgeRecv, sizeof(kGiovanniBadgeRecv),
                     "%s received\nthe EARTHBADGE!", playerName);
        }
        gTrainerAfterText = kGiovanniDefeatQuote;
        BattleUI_SetBadgeRecvText(kGiovanniBadgeRecv);
        s_gym_pending_leader_no = 8;
        gPendingClass  = GIOVANNI_CLASS;
        gPendingNo     = GIOVANNI_NO;
        Trainer_PlayEncounterMusic(GIOVANNI_CLASS);
        s_gym_jingle_wait = 24;
        gState         = GS_GYM_LEADER_JINGLE_WAIT;
        return;

    case GS_GIOVANNI_POST_TEXT:
        if (Text_IsOpen()) return;
        if (gPostFadeTimer > 0) { gPostFadeTimer--; return; }
        Text_ShowASCII(kGiovanniBadgeInfo);
        gState = GS_GIOVANNI_POST_WAIT;
        return;

    case GS_GIOVANNI_POST_WAIT:
        if (Text_IsOpen()) return;
        if (CheckEvent(EVENT_GOT_TM27)) {
            gState = GS_IDLE;
            return;
        }
        if (Inventory_Add(TM01 + 26, 1) != 0) {
            Text_ShowASCII(kGiovanniTMNoRoom);
            gState = GS_GIOVANNI_TM_EXP_WAIT;
            return;
        }
        SetEvent(EVENT_GOT_TM27);
        Audio_PlaySFX_GetItem1();
        Text_ShowASCII(RomText("_ViridianGymGiovanniReceivedTM27Text"));
        gState = GS_GIOVANNI_TM_WAIT;
        return;

    case GS_GIOVANNI_TM_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_GIOVANNI_TM_EXPLAIN;
        return;

    case GS_GIOVANNI_TM_EXPLAIN:
        Text_ShowASCII(kGiovanniTMExplain);
        gState = GS_GIOVANNI_TM_EXP_WAIT;
        return;

    case GS_GIOVANNI_TM_EXP_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_IDLE;
        return;

    case GS_GYM_TRAINER_PRE_WAIT:
        if (Text_IsOpen()) return;

        gTrainerAfterText         = gGymTrainerEndText ? gGymTrainerEndText : kGymTrainerEnd;
        gPendingClass            = gGymTrainerClass;
        gPendingNo               = gGymTrainerNo;
        gPendingBattle           = 1;
        gGymTrainerBattlePending = 1;
        gState                   = GS_IDLE;
        return;

    case GS_GYM_TRAINER_POST_TEXT:
        if (Text_IsOpen()) return;
        if (gPostFadeTimer > 0) { gPostFadeTimer--; return; }
        Text_ShowASCII(gGymTrainerEndText ? gGymTrainerEndText : kGymTrainerEnd);
        gState = GS_GYM_TRAINER_POST_WAIT;
        return;

    case GS_GYM_TRAINER_POST_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_IDLE;
        return;

    case GS_GUIDE_TEXT:
        gState = GS_GUIDE_WAIT;
        return;

    case GS_GUIDE_WAIT:
        if (Text_IsOpen()) return;
        gState = GS_IDLE;
        return;
    }
}

int GymScripts_IsActive(void) {
    return gState != GS_IDLE;
}

int GymScripts_GetPendingBattle(uint8_t *class_out, uint8_t *no_out) {
    if (!gPendingBattle) return 0;
    gPendingBattle = 0;
    *class_out = gPendingClass;
    *no_out    = gPendingNo;
    return 1;
}

#define POST_FADE_WAIT 17

void GymScripts_OnVictory(void) {
    gPostFadeTimer = POST_FADE_WAIT;
    if (wGymLeaderNo == 1) {
        SetEvent(EVENT_BEAT_BROCK);
        SetEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0);

        AmberScript_MarkAllTrainersDefeated("PewterGym");
        wObtainedBadges |= (1u << BADGE_BOULDER);
        wGymLeaderNo = 0;
        gState = GS_BROCK_POST_TEXT;
    } else if (wGymLeaderNo == 2) {
        SetEvent(EVENT_BEAT_MISTY);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1);
        AmberScript_MarkAllTrainersDefeated("CeruleanGym");
        wObtainedBadges |= (1u << BADGE_CASCADE);
        wGymLeaderNo = 0;
        gState = GS_MISTY_POST_TEXT;
    } else if (wGymLeaderNo == 3) {
        SetEvent(EVENT_BEAT_LT_SURGE);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VERMILION_GYM_TRAINER_2);
        AmberScript_MarkAllTrainersDefeated("VermilionGym");
        wObtainedBadges |= (1u << BADGE_THUNDER);
        wGymLeaderNo = 0;
        gState = GS_SURGE_POST_TEXT;
    } else if (wGymLeaderNo == 4) {
        SetEvent(EVENT_BEAT_ERIKA);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_CELADON_GYM_TRAINER_6);
        AmberScript_MarkAllTrainersDefeated("CeladonGym");
        wObtainedBadges |= (1u << BADGE_RAINBOW);
        wGymLeaderNo = 0;
        gState = GS_ERIKA_POST_TEXT;
    } else if (wGymLeaderNo == 5) {
        SetEvent(EVENT_BEAT_KOGA);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_5);
        AmberScript_MarkAllTrainersDefeated("FuchsiaGym");
        wObtainedBadges |= (1u << BADGE_SOUL);
        wGymLeaderNo = 0;
        gState = GS_KOGA_POST_TEXT;
    } else if (wGymLeaderNo == 6) {
        SetEvent(EVENT_BEAT_SABRINA);
        SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_6);
        AmberScript_MarkAllTrainersDefeated("SaffronGym");
        wObtainedBadges |= (1u << BADGE_MARSH);
        wGymLeaderNo = 0;
        gState = GS_SABRINA_POST_TEXT;
    } else if (wGymLeaderNo == 7) {
        SetEvent(EVENT_BEAT_BLAINE);

        SetEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_CINNABAR_GYM_TRAINER_6);
        AmberScript_MarkAllTrainersDefeated("CinnabarGym");
        wObtainedBadges |= (1u << BADGE_VOLCANO);
        wGymLeaderNo = 0;
        gState = GS_BLAINE_POST_TEXT;
    } else if (wGymLeaderNo == 8) {
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_GIOVANNI);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_0);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_1);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_2);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_3);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_4);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_5);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_6);
        SetEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_7);
        AmberScript_MarkAllTrainersDefeated("ViridianGym");

        ClearEvent(EVENT_1ST_ROUTE22_RIVAL_BATTLE);
        SetEvent(EVENT_2ND_ROUTE22_RIVAL_BATTLE);
        SetEvent(EVENT_ROUTE22_RIVAL_WANTS_BATTLE);
        wObtainedBadges |= (1u << BADGE_EARTH);
        wGymLeaderNo = 0;
        gState = GS_GIOVANNI_POST_TEXT;
    } else {
        wGymLeaderNo = 0;
    }
}

void GymScripts_BrockInteract(void) {
    if (CheckEvent(EVENT_BEAT_BROCK)) {
        Text_ShowASCII(kBrockAfter);
        return;
    }
    Text_ShowASCII(kBrockPre);
    gState = GS_BROCK_PRE_TEXT;
}

void GymScripts_GymTrainerInteract(void) {
    if (CheckEvent(EVENT_BEAT_PEWTER_GYM_TRAINER_0)) {
        Text_ShowASCII(kGymTrainerAfter);
        return;
    }
    gGymTrainerClass        = PEWTER_TRAINER_CLASS;
    gGymTrainerNo           = PEWTER_TRAINER_NO;
    gGymTrainerVictoryEvent = EVENT_BEAT_PEWTER_GYM_TRAINER_0;
    gGymTrainerEndText      = kGymTrainerEnd;
    gGymTrainerAfterText    = kGymTrainerAfter;
    Text_ShowASCII(kGymTrainerPre);
    gState = GS_GYM_TRAINER_PRE_TEXT;
}

void GymScripts_SetTrainerPending(uint8_t cls, uint8_t no, uint32_t flag,
                                   const char *end_text, const char *after_text,
                                   const char *pre_text) {
    gGymTrainerClass        = cls;
    gGymTrainerNo           = no;
    gGymTrainerVictoryEvent = flag;
    gGymTrainerEndText      = end_text;
    gGymTrainerAfterText    = after_text;
    Text_ShowASCII(pre_text);
    gState = GS_GYM_TRAINER_PRE_TEXT;
}

void GymScripts_OnGymTrainerVictory(void) {
    if (gGymTrainerVictoryEvent) SetEvent(gGymTrainerVictoryEvent);
    gPostFadeTimer = POST_FADE_WAIT;
    gState = GS_GYM_TRAINER_POST_TEXT;
}

int GymScripts_ConsumeGymTrainer(void) {
    int v = gGymTrainerBattlePending;
    gGymTrainerBattlePending = 0;
    return v;
}

void GymScripts_MistyInteract(void) {
    if (CheckEvent(EVENT_BEAT_MISTY)) {
        Text_ShowASCII(kMistyAfter);
        gState = GS_GUIDE_TEXT;
        return;
    }
    Text_ShowASCII(kMistyPre);
    gState = GS_MISTY_PRE_TEXT;
}

void GymScripts_CeruleanTrainer0Interact(void) {
    if (CheckEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_0)) {
        Text_ShowASCII(kCeruleanTrainer0After);
        gState = GS_GUIDE_TEXT;
        return;
    }
    gGymTrainerClass        = CERULEAN_TRAINER0_CLASS;
    gGymTrainerNo           = CERULEAN_TRAINER0_NO;
    gGymTrainerVictoryEvent = EVENT_BEAT_CERULEAN_GYM_TRAINER_0;
    gGymTrainerEndText      = kCeruleanTrainer0End;
    gGymTrainerAfterText    = kCeruleanTrainer0After;
    Text_ShowASCII(kCeruleanTrainer0Pre);
    gState = GS_GYM_TRAINER_PRE_TEXT;
}

void GymScripts_CeruleanTrainer1Interact(void) {
    if (CheckEvent(EVENT_BEAT_CERULEAN_GYM_TRAINER_1)) {
        Text_ShowASCII(kCeruleanTrainer1After);
        gState = GS_GUIDE_TEXT;
        return;
    }
    gGymTrainerClass        = CERULEAN_TRAINER1_CLASS;
    gGymTrainerNo           = CERULEAN_TRAINER1_NO;
    gGymTrainerVictoryEvent = EVENT_BEAT_CERULEAN_GYM_TRAINER_1;
    gGymTrainerEndText      = kCeruleanTrainer1End;
    gGymTrainerAfterText    = kCeruleanTrainer1After;
    Text_ShowASCII(kCeruleanTrainer1Pre);
    gState = GS_GYM_TRAINER_PRE_TEXT;
}

void GymScripts_CeruleanGuideInteract(void) {
    if (CheckEvent(EVENT_BEAT_MISTY)) {
        Text_ShowASCII(kCeruleanGuidePost);
    } else {
        Text_ShowASCII(kCeruleanGuidePre);
    }
    gState = GS_GUIDE_TEXT;
}

void GymScripts_SurgeInteract(void) {
    if (CheckEvent(EVENT_BEAT_LT_SURGE)) {
        Text_ShowASCII(kSurgeAfter);
        gState = GS_GUIDE_TEXT;
        return;
    }
    Text_ShowASCII(kSurgePre);
    gState = GS_SURGE_PRE_TEXT;
}

void GymScripts_ErikaInteract(void) {
    if (CheckEvent(EVENT_BEAT_ERIKA)) {
        if (CheckEvent(EVENT_GOT_TM21)) {
            Text_ShowASCII(kErikaAfter);
            gState = GS_GUIDE_TEXT;
            return;
        }

        gPostFadeTimer = 0;
        gState = GS_ERIKA_POST_TEXT;
        return;
    }
    Text_ShowASCII(kErikaPre);
    gState = GS_ERIKA_PRE_TEXT;
}

void GymScripts_KogaInteract(void) {
    if (CheckEvent(EVENT_BEAT_KOGA)) {
        if (CheckEvent(EVENT_GOT_TM06)) {
            Text_ShowASCII(kKogaAfter);
            gState = GS_GUIDE_TEXT;
            return;
        }
        gPostFadeTimer = 0;
        gState = GS_KOGA_POST_TEXT;
        return;
    }
    Text_ShowASCII(kKogaPre);
    gState = GS_KOGA_PRE_TEXT;
}

void GymScripts_BlaineInteract(void) {
    if (CheckEvent(EVENT_BEAT_BLAINE)) {
        if (CheckEvent(EVENT_GOT_TM38)) {
            Text_ShowASCII(kBlaineAfter);
            gState = GS_GUIDE_TEXT;
            return;
        }
        gPostFadeTimer = 0;
        gState = GS_BLAINE_POST_TEXT;
        return;
    }
    Text_ShowASCII(kBlainePre);
    gState = GS_BLAINE_PRE_TEXT;
}

void GymScripts_GiovanniInteract(void) {
    if (CheckEvent(EVENT_BEAT_VIRIDIAN_GYM_GIOVANNI)) {
        if (CheckEvent(EVENT_GOT_TM27)) {
            Text_ShowASCII(kGiovanniAfter);
            gState = GS_GUIDE_TEXT;
            return;
        }
        gPostFadeTimer = 0;
        gState = GS_GIOVANNI_POST_TEXT;
        return;
    }
    Text_ShowASCII(kGiovanniPre);
    gState = GS_GIOVANNI_PRE_TEXT;
}

void GymScripts_ViridianTrainer0Interact(void) {
    if (CheckEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_0)) { Text_ShowASCII(kViridianT0After); gState = GS_GUIDE_TEXT; return; }
    GymScripts_SetTrainerPending(COOLTRAINER_M_CLASS, 9, EVENT_BEAT_VIRIDIAN_GYM_TRAINER_0, kViridianT0End, kViridianT0After, kViridianT0Pre);
}
void GymScripts_ViridianTrainer1Interact(void) {
    if (CheckEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_1)) { Text_ShowASCII(kViridianT1After); gState = GS_GUIDE_TEXT; return; }
    GymScripts_SetTrainerPending(BLACKBELT_CLASS, 6, EVENT_BEAT_VIRIDIAN_GYM_TRAINER_1, kViridianT1End, kViridianT1After, kViridianT1Pre);
}
void GymScripts_ViridianTrainer2Interact(void) {
    if (CheckEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_2)) { Text_ShowASCII(kViridianT2After); gState = GS_GUIDE_TEXT; return; }
    GymScripts_SetTrainerPending(TAMER_CLASS, 3, EVENT_BEAT_VIRIDIAN_GYM_TRAINER_2, kViridianT2End, kViridianT2After, kViridianT2Pre);
}
void GymScripts_ViridianTrainer3Interact(void) {
    if (CheckEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_3)) { Text_ShowASCII(kViridianT3After); gState = GS_GUIDE_TEXT; return; }
    GymScripts_SetTrainerPending(BLACKBELT_CLASS, 7, EVENT_BEAT_VIRIDIAN_GYM_TRAINER_3, kViridianT3End, kViridianT3After, kViridianT3Pre);
}
void GymScripts_ViridianTrainer4Interact(void) {
    if (CheckEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_4)) { Text_ShowASCII(kViridianT4After); gState = GS_GUIDE_TEXT; return; }
    GymScripts_SetTrainerPending(COOLTRAINER_M_CLASS, 10, EVENT_BEAT_VIRIDIAN_GYM_TRAINER_4, kViridianT4End, kViridianT4After, kViridianT4Pre);
}
void GymScripts_ViridianTrainer5Interact(void) {
    if (CheckEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_5)) { Text_ShowASCII(kViridianT5After); gState = GS_GUIDE_TEXT; return; }
    GymScripts_SetTrainerPending(BLACKBELT_CLASS, 8, EVENT_BEAT_VIRIDIAN_GYM_TRAINER_5, kViridianT5End, kViridianT5After, kViridianT5Pre);
}
void GymScripts_ViridianTrainer6Interact(void) {
    if (CheckEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_6)) { Text_ShowASCII(kViridianT6After); gState = GS_GUIDE_TEXT; return; }
    GymScripts_SetTrainerPending(TAMER_CLASS, 4, EVENT_BEAT_VIRIDIAN_GYM_TRAINER_6, kViridianT6End, kViridianT6After, kViridianT6Pre);
}
void GymScripts_ViridianTrainer7Interact(void) {
    if (CheckEvent(EVENT_BEAT_VIRIDIAN_GYM_TRAINER_7)) { Text_ShowASCII(kViridianT7After); gState = GS_GUIDE_TEXT; return; }
    GymScripts_SetTrainerPending(COOLTRAINER_M_CLASS, 1, EVENT_BEAT_VIRIDIAN_GYM_TRAINER_7, kViridianT7End, kViridianT7After, kViridianT7Pre);
}

void GymScripts_ViridianGuideInteract(void) {
    if (CheckEvent(EVENT_BEAT_VIRIDIAN_GYM_GIOVANNI)) Text_ShowASCII(kViridianGuidePost);
    else Text_ShowASCII(kViridianGuidePre);
    gState = GS_GUIDE_TEXT;
}

void GymScripts_SabrinaInteract(void) {
    if (CheckEvent(EVENT_BEAT_SABRINA)) {
        if (CheckEvent(EVENT_GOT_TM46)) {
            Text_ShowASCII(kSabrinaAfter);
            gState = GS_GUIDE_TEXT;
            return;
        }
        gPostFadeTimer = 0;
        gState = GS_SABRINA_POST_TEXT;
        return;
    }
    Text_ShowASCII(kSabrinaPre);
    gState = GS_SABRINA_PRE_TEXT;
}

void GymScripts_SaffronTrainer0Interact(void) {
    if (CheckEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_0)) { Text_ShowASCII(kSaffronT0After); gState = GS_GUIDE_TEXT; return; }
    GymScripts_SetTrainerPending(CHANNELER_CLASS, 22, EVENT_BEAT_SAFFRON_GYM_TRAINER_0, kSaffronT0End, kSaffronT0After, kSaffronT0Pre);
}
void GymScripts_SaffronTrainer1Interact(void) {
    if (CheckEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_1)) { Text_ShowASCII(kSaffronT1After); gState = GS_GUIDE_TEXT; return; }
    GymScripts_SetTrainerPending(PSYCHIC_CLASS, 1, EVENT_BEAT_SAFFRON_GYM_TRAINER_1, kSaffronT1End, kSaffronT1After, kSaffronT1Pre);
}
void GymScripts_SaffronTrainer2Interact(void) {
    if (CheckEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_2)) { Text_ShowASCII(kSaffronT2After); gState = GS_GUIDE_TEXT; return; }
    GymScripts_SetTrainerPending(CHANNELER_CLASS, 23, EVENT_BEAT_SAFFRON_GYM_TRAINER_2, kSaffronT2End, kSaffronT2After, kSaffronT2Pre);
}
void GymScripts_SaffronTrainer3Interact(void) {
    if (CheckEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_3)) { Text_ShowASCII(kSaffronT3After); gState = GS_GUIDE_TEXT; return; }
    GymScripts_SetTrainerPending(PSYCHIC_CLASS, 2, EVENT_BEAT_SAFFRON_GYM_TRAINER_3, kSaffronT3End, kSaffronT3After, kSaffronT3Pre);
}
void GymScripts_SaffronTrainer4Interact(void) {
    if (CheckEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_4)) { Text_ShowASCII(kSaffronT4After); gState = GS_GUIDE_TEXT; return; }
    GymScripts_SetTrainerPending(CHANNELER_CLASS, 24, EVENT_BEAT_SAFFRON_GYM_TRAINER_4, kSaffronT4End, kSaffronT4After, kSaffronT4Pre);
}
void GymScripts_SaffronTrainer5Interact(void) {
    if (CheckEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_5)) { Text_ShowASCII(kSaffronT5After); gState = GS_GUIDE_TEXT; return; }
    GymScripts_SetTrainerPending(PSYCHIC_CLASS, 3, EVENT_BEAT_SAFFRON_GYM_TRAINER_5, kSaffronT5End, kSaffronT5After, kSaffronT5Pre);
}
void GymScripts_SaffronTrainer6Interact(void) {
    if (CheckEvent(EVENT_BEAT_SAFFRON_GYM_TRAINER_6)) { Text_ShowASCII(kSaffronT6After); gState = GS_GUIDE_TEXT; return; }
    GymScripts_SetTrainerPending(PSYCHIC_CLASS, 4, EVENT_BEAT_SAFFRON_GYM_TRAINER_6, kSaffronT6End, kSaffronT6After, kSaffronT6Pre);
}
void GymScripts_SaffronGuideInteract(void) {
    if (CheckEvent(EVENT_BEAT_SABRINA)) Text_ShowASCII(kSaffronGuidePost);
    else Text_ShowASCII(kSaffronGuidePre);
    gState = GS_GUIDE_TEXT;
}

void GymScripts_FuchsiaTrainer1Interact(void) {
    if (CheckEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_0)) {
        Text_ShowASCII(kFuchsiaT1After);
        gState = GS_GUIDE_TEXT;
        return;
    }
    GymScripts_SetTrainerPending(JUGGLER_CLASS, 7, EVENT_BEAT_FUCHSIA_GYM_TRAINER_0, kFuchsiaT1End, kFuchsiaT1After, kFuchsiaT1Pre);
}

void GymScripts_FuchsiaTrainer2Interact(void) {
    if (CheckEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_1)) {
        Text_ShowASCII(kFuchsiaT2After);
        gState = GS_GUIDE_TEXT;
        return;
    }
    GymScripts_SetTrainerPending(JUGGLER_CLASS, 3, EVENT_BEAT_FUCHSIA_GYM_TRAINER_1, kFuchsiaT2End, kFuchsiaT2After, kFuchsiaT2Pre);
}

void GymScripts_FuchsiaTrainer3Interact(void) {
    if (CheckEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_2)) {
        Text_ShowASCII(kFuchsiaT3After);
        gState = GS_GUIDE_TEXT;
        return;
    }
    GymScripts_SetTrainerPending(JUGGLER_CLASS, 8, EVENT_BEAT_FUCHSIA_GYM_TRAINER_2, kFuchsiaT3End, kFuchsiaT3After, kFuchsiaT3Pre);
}

void GymScripts_FuchsiaTrainer4Interact(void) {
    if (CheckEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_3)) {
        Text_ShowASCII(kFuchsiaT4After);
        gState = GS_GUIDE_TEXT;
        return;
    }
    GymScripts_SetTrainerPending(TAMER_CLASS, 1, EVENT_BEAT_FUCHSIA_GYM_TRAINER_3, kFuchsiaT4End, kFuchsiaT4After, kFuchsiaT4Pre);
}

void GymScripts_FuchsiaTrainer5Interact(void) {
    if (CheckEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_4)) {
        Text_ShowASCII(kFuchsiaT5After);
        gState = GS_GUIDE_TEXT;
        return;
    }
    GymScripts_SetTrainerPending(TAMER_CLASS, 2, EVENT_BEAT_FUCHSIA_GYM_TRAINER_4, kFuchsiaT5End, kFuchsiaT5After, kFuchsiaT5Pre);
}

void GymScripts_FuchsiaTrainer6Interact(void) {
    if (CheckEvent(EVENT_BEAT_FUCHSIA_GYM_TRAINER_5)) {
        Text_ShowASCII(kFuchsiaT6After);
        gState = GS_GUIDE_TEXT;
        return;
    }
    GymScripts_SetTrainerPending(JUGGLER_CLASS, 4, EVENT_BEAT_FUCHSIA_GYM_TRAINER_5, kFuchsiaT6End, kFuchsiaT6After, kFuchsiaT6Pre);
}

void GymScripts_FuchsiaGuideInteract(void) {
    if (CheckEvent(EVENT_BEAT_KOGA)) Text_ShowASCII(kFuchsiaGuidePost);
    else Text_ShowASCII(kFuchsiaGuidePre);
    gState = GS_GUIDE_TEXT;
}
