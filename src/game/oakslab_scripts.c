
#include "oakslab_scripts.h"
#include "rom_text.h"
#include "npc.h"
#include "player.h"
#include "text.h"
#include "music.h"
#include "warp.h"
#include "pokemon.h"
#include "overworld.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../platform/audio.h"
#include "../data/event_constants.h"
#include "../data/base_stats.h"
#include "../data/font_data.h"
#include "inventory.h"
#include "pokedex.h"
#include "trainer_sight.h"
#include "naming_screen.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "map_music.h"

#define MAP_OAKS_LAB 0x28

static uint8_t encode_char(char c) {
    if (c >= 'A' && c <= 'Z') return 0x80 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 0xA0 + (c - 'a');
    if (c == '#') return 0x54;
    if (c == ' ') return 0x7F;
    if (c == '.') return 0xE8;
    if (c == '-') return 0xE3;
    if (c == '!') return 0xE7;
    if (c == '?') return 0xE6;
    if (c == '\n') return 0x4E;
    return 0x7F;
}

static void encode_string(const char *src, uint8_t *dst, int max_len) {
    int i = 0;
    while (i < max_len - 1 && *src) {
        *dst++ = encode_char(*src++);
        i++;
    }
    *dst = 0x50;
}

#define OAKSLAB_RIVAL_IDX       0
#define OAKSLAB_CHARMANDER_IDX  1
#define OAKSLAB_SQUIRTLE_IDX    2
#define OAKSLAB_BULBASAUR_IDX   3
#define OAKSLAB_OAK1_IDX        4
#define OAKSLAB_POKEDEX1_IDX    5
#define OAKSLAB_POKEDEX2_IDX    6
#define OAKSLAB_OAK2_IDX        7

#define DIR_DOWN  0
#define DIR_UP    1
#define DIR_LEFT  2
#define DIR_RIGHT 3

#define CHALLENGE_TRIGGER_Y  7

#define RIVAL_EXIT_STEPS  5

typedef enum {
    OLS_IDLE = 0,

    OLS_OAK_ENTER,
    OLS_OAK_SWAP,

    OLS_PLAYER_ENTER_SETUP,
    OLS_PLAYER_ENTER_WALK,
    OLS_FOLLOWED,

    OLS_SPEECH_SHOW,
    OLS_SPEECH_WAIT,

    OLS_BALL_CANT_YET,
    OLS_BALL_CANT_WAIT,
    OLS_BALL_LAST_MON,
    OLS_BALL_LAST_WAIT,
    OLS_DEX_SHOW,
    OLS_DEX_WAIT,
    OLS_DEX_RESTORE,
    OLS_BALL_CONFIRM,
    OLS_BALL_CONFIRM_WAIT,
    OLS_BALL_YESNO,
    OLS_BALL_DECLINED,
    OLS_BALL_DECLINED_WAIT,
    OLS_BALL_ENERGETIC,
    OLS_BALL_ENERGETIC_WAIT,
    OLS_BALL_NICK_PROMPT,
    OLS_BALL_NICK_YESNO,
    OLS_BALL_NICK_WAIT,
    OLS_BALL_RECEIVED,
    OLS_BALL_RECEIVED_WAIT,

    OLS_RIVAL_WALK,
    OLS_RIVAL_WALK_WAIT,
    OLS_RIVAL_TAKES,
    OLS_RIVAL_TAKES_WAIT,
    OLS_RIVAL_RCVD,
    OLS_RIVAL_RCVD_WAIT,

    OLS_AWAIT_CHALLENGE,
    OLS_CHALLENGE_TEXT,
    OLS_CHALLENGE_WAIT,
    OLS_RIVAL_APPROACH,
    OLS_RIVAL_APPROACH_WAIT,
    OLS_BATTLE_TRIGGER,

    OLS_POST_BATTLE_SETUP,
    OLS_POST_BATTLE_DELAY,
    OLS_POST_BATTLE_TEXT,
    OLS_POST_BATTLE_WAIT,
    OLS_RIVAL_EXIT_WALK,
    OLS_RIVAL_EXIT_WAIT,
    OLS_RIVAL_GONE,

    OLS_PARCEL_TEXT1,
    OLS_PARCEL_TEXT2,
    OLS_RIVAL_ARRIVE_TEXT,
    OLS_OAK_REQUEST_TEXT,
    OLS_POKEDEX_TEXT,
    OLS_GIVE_POKEDEX,
    OLS_GAVE_POKEDEX_TEXT,
    OLS_OAK_DREAM_TEXT,
    OLS_RIVAL_LEAVE_TEXT,
    OLS_RIVAL_LEAVE_WALK,
    OLS_RIVAL_LEAVE_DONE,

    OLS_PARCEL_SHOW_RIVAL,
    OLS_RIVAL_WALK_IN,
} oakslab_state_t;

static oakslab_state_t gState          = OLS_IDLE;
static int gOakEnterSteps              = 0;
static int gPlayerEnterSteps           = 0;
static int gSpeechIdx                  = 0;
static int gDelay                      = 0;

static uint8_t gSelectedSpecies        = 0;
static uint8_t gRivalStarterSpecies    = 0;
static int     gSelectedBallIdx        = 0;
static int     gRivalBallIdx           = 0;
static int     gYesNoCursor            = 0;
static int     gShowDataDelay          = 0;

static int gRivalWalkDirs[8];
static int gRivalWalkLen               = 0;
static int gRivalWalkStep              = 0;

#define MAX_APPROACH_STEPS 16
static int gRivalApproachDirs[MAX_APPROACH_STEPS];
static int gRivalApproachLen           = 0;
static int gRivalApproachStep          = 0;

static int gRivalExitStep              = 0;

static int gRivalSavedX                = 0;
static int gRivalSavedY                = 0;

static int     gPendingBattle          = 0;
static uint8_t gPendingBattleClass     = 0;
static uint8_t gPendingBattleNo        = 0;

static int gRivalExitPending           = 0;

static int gRivalPokedexExitStep       = 0;

#define RIVAL_WALK_IN_STEPS 7
static int gRivalWalkInStep            = 0;

static void set_player_facing(int dir) {
    gPlayerFacing = dir & 3;
    Player_SyncOAM();
}

static void update_player_watch_rival_exit(void) {

    if (gRivalExitStep == 0) {
        if ((int)wXCoord == 4) set_player_facing(DIR_RIGHT);
        else                   set_player_facing(DIR_LEFT);
    } else if (gRivalExitStep == 1) {
        set_player_facing(DIR_DOWN);
    }
}

#define kRivalFedUp (RomText("OaksLabRivalFedUpWithWaitingText.Text"))

#define kOakChooseMon (RomText("OaksLabOakChooseMonText.Text"))

#define kRivalWhatAboutMe (RomText("OaksLabRivalWhatAboutMeText.Text"))

#define kOakBePatient (RomText("OaksLabOakBePatientText.Text"))

#define INTRO_SPEECH_COUNT 4
static const char *IntroSpeech(int idx) {
    switch (idx) {
    case 0: return kRivalFedUp;
    case 1: return kOakChooseMon;
    case 2: return kRivalWhatAboutMe;
    case 3: return kOakBePatient;
    default: return "";
    }
}

#define kThoseArePokeballs (RomText("OaksLabThoseArePokeBallsText"))

#define kLastMon (RomText("OaksLabLastMonText"))

#define kRivalIllTakeThisOne (RomText("OaksLabRivalIllTakeThisOneText.Text"))

#define kRivalIllTakeYouOn (RomText("_OaksLabRivalIllTakeYouOnText"))

#define kRivalIPickedTheWrongPokemon (RomTextPrefixed("{RIVAL}: ", "OaksLabRivalIPickedTheWrongPokemonText"))

#define kRivalSmellYouLater (RomText("OaksLabRivalSmellYouLaterText.Text"))

#define kRivalGramps (RomText("OaksLabRivalGrampsText"))

#define kOakParcelText1 (RomText("_OaksLabOak1DeliverParcelText"))

#define kOakParcelText2 (RomText("_OaksLabOak1ParcelThanksText"))

#define kRivalWhatDidYouCall (RomText("OaksLabRivalWhatDidYouCallMeForText"))

#define kOakRequest (RomText("OaksLabOakIHaveARequestText"))

#define kOakPokedexText (RomText("_OaksLabOakMyInventionPokedexText"))

#define kOakGivesPokedex (RomText("OaksLabOakGotPokedexText"))

#define kOakDreamText (RomText("OaksLabOakThatWasMyDreamText"))

#define kRivalLeaveText (RomText("OaksLabRivalLeaveItAllToMeText"))

#define RIVAL_POKEDEX_EXIT_STEPS 5

static uint8_t gConfirmText[80];
static uint8_t gEnergeticText[80];
static uint8_t gPlayerRcvdText[80];
static uint8_t gRivalRcvdText[80];
static uint8_t gNickPromptText[96];
static int gStarterPartySlot = -1;
static int gYesNoWaitForTextClose = 0;

#define YESNO_COL  14
#define YESNO_ROW   8
#define YESNO_W     6
#define YESNO_H     4

#define BC_TL  0x79u
#define BC_H   0x7Au
#define BC_TR  0x7Bu
#define BC_V   0x7Cu
#define BC_BL  0x7Du
#define BC_BR  0x7Eu
#define BC_SP  0x7Fu
#define BC_CUR 0xEDu

static void yesno_set_tile(int col, int row, uint8_t tile) {
    if (col < 0 || col >= SCREEN_WIDTH || row < 0 || row >= SCREEN_HEIGHT) return;
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = tile;
}

static uint8_t yesno_char(char c) {
    if (c >= 'A' && c <= 'Z') return (uint8_t)Font_CharToTile((unsigned char)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return (uint8_t)Font_CharToTile((unsigned char)(0xA0 + (c - 'a')));
    if (c >= '0' && c <= '9') return (uint8_t)Font_CharToTile((unsigned char)(0xF6 + (c - '0')));
    return (uint8_t)Font_CharToTile(BC_SP);
}

static void yesno_draw(void) {

    yesno_set_tile(YESNO_COL,               YESNO_ROW,     (uint8_t)Font_CharToTile(BC_TL));
    for (int c = 1; c < YESNO_W - 1; c++)
        yesno_set_tile(YESNO_COL + c,       YESNO_ROW,     (uint8_t)Font_CharToTile(BC_H));
    yesno_set_tile(YESNO_COL + YESNO_W - 1, YESNO_ROW,    (uint8_t)Font_CharToTile(BC_TR));

    yesno_set_tile(YESNO_COL,               YESNO_ROW + 1, (uint8_t)Font_CharToTile(BC_V));
    yesno_set_tile(YESNO_COL + 1,           YESNO_ROW + 1,
                   gYesNoCursor == 0 ? (uint8_t)Font_CharToTile(BC_CUR)
                                     : (uint8_t)Font_CharToTile(BC_SP));
    yesno_set_tile(YESNO_COL + 2,           YESNO_ROW + 1, yesno_char('Y'));
    yesno_set_tile(YESNO_COL + 3,           YESNO_ROW + 1, yesno_char('E'));
    yesno_set_tile(YESNO_COL + 4,           YESNO_ROW + 1, yesno_char('S'));
    yesno_set_tile(YESNO_COL + YESNO_W - 1, YESNO_ROW + 1, (uint8_t)Font_CharToTile(BC_V));

    yesno_set_tile(YESNO_COL,               YESNO_ROW + 2, (uint8_t)Font_CharToTile(BC_V));
    yesno_set_tile(YESNO_COL + 1,           YESNO_ROW + 2,
                   gYesNoCursor == 1 ? (uint8_t)Font_CharToTile(BC_CUR)
                                     : (uint8_t)Font_CharToTile(BC_SP));
    yesno_set_tile(YESNO_COL + 2,           YESNO_ROW + 2, yesno_char('N'));
    yesno_set_tile(YESNO_COL + 3,           YESNO_ROW + 2, yesno_char('O'));
    yesno_set_tile(YESNO_COL + 4,           YESNO_ROW + 2, (uint8_t)Font_CharToTile(BC_SP));
    yesno_set_tile(YESNO_COL + YESNO_W - 1, YESNO_ROW + 2, (uint8_t)Font_CharToTile(BC_V));

    yesno_set_tile(YESNO_COL,               YESNO_ROW + 3, (uint8_t)Font_CharToTile(BC_BL));
    for (int c = 1; c < YESNO_W - 1; c++)
        yesno_set_tile(YESNO_COL + c,       YESNO_ROW + 3, (uint8_t)Font_CharToTile(BC_H));
    yesno_set_tile(YESNO_COL + YESNO_W - 1, YESNO_ROW + 3, (uint8_t)Font_CharToTile(BC_BR));
}

static void yesno_clear(void) {
    for (int r = 0; r < YESNO_H; r++)
        for (int c = 0; c < YESNO_W; c++)
            yesno_set_tile(YESNO_COL + c, YESNO_ROW + r, (uint8_t)Font_CharToTile(BC_SP));
}

static int prompt_requires_scroll(const char *s) {
    int lines = 1;
    if (!s) return 0;
    while (*s) {
        if (*s == '\f') return 1;
        if (*s == '\n') {
            lines++;
            if (lines > 2) return 1;
        }
        s++;
    }
    return 0;
}

static int find_path(int from_x, int from_y, int to_x, int to_y, int *dirs) {
    int ydist = abs(from_y - to_y);
    int xdist = abs(from_x - to_x);
    if (ydist > 0) ydist--;
    int ydir = (to_y < from_y) ? DIR_UP   : DIR_DOWN;
    int xdir = (to_x < from_x) ? DIR_LEFT : DIR_RIGHT;
    int yprog = 0, xprog = 0, count = 0;
    while (yprog < ydist || xprog < xdist) {
        int yrem = ydist - yprog, xrem = xdist - xprog;
        if (xrem >= yrem && xprog < xdist) { dirs[count++] = xdir; xprog++; }
        else                               { dirs[count++] = ydir; yprog++; }
        if (count >= MAX_APPROACH_STEPS - 1) break;
    }
    return count;
}

static void hide_chosen_balls(void) {
    if (CheckEvent(EVENT_HIDE_STARTER_BALL_1)) NPC_HideSprite(OAKSLAB_CHARMANDER_IDX);
    if (CheckEvent(EVENT_HIDE_STARTER_BALL_2)) NPC_HideSprite(OAKSLAB_SQUIRTLE_IDX);
    if (CheckEvent(EVENT_HIDE_STARTER_BALL_3)) NPC_HideSprite(OAKSLAB_BULBASAUR_IDX);
}

static void set_ball_toggle(int ball_idx) {
    switch (ball_idx) {
    case OAKSLAB_CHARMANDER_IDX: SetEvent(EVENT_HIDE_STARTER_BALL_1); break;
    case OAKSLAB_SQUIRTLE_IDX:   SetEvent(EVENT_HIDE_STARTER_BALL_2); break;
    case OAKSLAB_BULBASAUR_IDX:  SetEvent(EVENT_HIDE_STARTER_BALL_3); break;
    }
}

int OaksLabScripts_IsActive(void) {
    return gState != OLS_IDLE;
}

void OaksLabScripts_PostRender(void) {
    if (gState == OLS_BALL_YESNO || gState == OLS_BALL_NICK_YESNO) {
        if (!gYesNoWaitForTextClose || !Text_IsOpen())
            yesno_draw();
    }
}

int OaksLabScripts_GetPendingBattle(uint8_t *class_out, uint8_t *no_out) {
    if (!gPendingBattle) return 0;
    *class_out   = gPendingBattleClass;
    *no_out      = gPendingBattleNo;
    gPendingBattle = 0;
    return 1;
}

static void ball_callback(uint8_t player_species, uint8_t rival_species,
                          int player_ball_idx, int rival_ball_idx) {

    if (CheckEvent(EVENT_GOT_STARTER)) {
        NPC_SetFacing(OAKSLAB_OAK1_IDX, DIR_DOWN);
        gState = OLS_BALL_LAST_MON;
        return;
    }

    if (!CheckEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON)) {
        gState = OLS_BALL_CANT_YET;
        return;
    }

    gSelectedSpecies     = player_species;
    gRivalStarterSpecies = rival_species;
    gSelectedBallIdx     = player_ball_idx;
    gRivalBallIdx        = rival_ball_idx;

    const char *confirm_sym = "OaksLabYouWantCharmanderText.Text";
    if (player_species == STARTER2) confirm_sym = "OaksLabYouWantSquirtleText.Text";
    if (player_species == STARTER3) confirm_sym = "OaksLabYouWantBulbasaurText.Text";
    snprintf((char*)gConfirmText, sizeof(gConfirmText), "%s", RomText(confirm_sym));

    uint8_t p_dex = gSpeciesToDex[player_species];
    uint8_t r_dex = gSpeciesToDex[rival_species];
    RomTextSplice((char*)gPlayerRcvdText, sizeof(gPlayerRcvdText),
                 "_OaksLabReceivedMonText", "{badge}", Pokemon_GetName(p_dex));
    RomTextSplice((char*)gRivalRcvdText, sizeof(gRivalRcvdText),
                 "_OaksLabRivalReceivedMonText", "{badge}", Pokemon_GetName(r_dex));

    snprintf((char*)gEnergeticText, sizeof(gEnergeticText),
             "%s", RomText("OaksLabMonEnergeticText"));
    RomTextSplice((char*)gNickPromptText, sizeof(gNickPromptText),
                 "DoYouWantToNicknameText", "{badge}", Pokemon_GetName(p_dex));
    gStarterPartySlot = -1;

    NPC_SetFacing(OAKSLAB_OAK1_IDX,  DIR_DOWN);
    NPC_SetFacing(OAKSLAB_RIVAL_IDX, DIR_RIGHT);

    gScriptedMovement = 1;

    gState = OLS_DEX_SHOW;
}

void OaksLabScripts_OnMapLoad(void) {
    if (wCurMap != MAP_OAKS_LAB) return;

    return;
    Warp_HasDoorStep();

    gState             = OLS_IDLE;
    gOakEnterSteps     = 0;
    gPlayerEnterSteps  = 0;
    gSpeechIdx         = 0;
    gDelay             = 0;

    if (CheckEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB) && !gRivalExitPending) {
        NPC_HideSprite(OAKSLAB_RIVAL_IDX);
    } else {
        NPC_ShowSprite(OAKSLAB_RIVAL_IDX);
    }

    if (!CheckEvent(EVENT_OAK_APPEARED_IN_PALLET)) {
        NPC_ShowSprite(OAKSLAB_OAK1_IDX);
        NPC_HideSprite(OAKSLAB_OAK2_IDX);
        return;
    }

    if (CheckEvent(EVENT_FOLLOWED_OAK_INTO_LAB)) {
        NPC_ShowSprite(OAKSLAB_OAK1_IDX);
        NPC_HideSprite(OAKSLAB_OAK2_IDX);
        hide_chosen_balls();

        if (gRivalExitPending) {
            NPC_ShowSprite(OAKSLAB_RIVAL_IDX);
            gState = OLS_POST_BATTLE_SETUP;
            return;
        }

        if (CheckEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB)) {
            NPC_HideSprite(OAKSLAB_RIVAL_IDX);

            if (CheckEvent(EVENT_GOT_POKEDEX)) {
                NPC_HideSprite(OAKSLAB_POKEDEX1_IDX);
                NPC_HideSprite(OAKSLAB_POKEDEX2_IDX);
            }
            return;
        }

        if (CheckEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON)) {
            if (CheckEvent(EVENT_GOT_STARTER)) {

                if (wRivalStarter != 0) {
                    gState = OLS_AWAIT_CHALLENGE;
                }
            }
        }
        return;
    }

    NPC_HideSprite(OAKSLAB_OAK1_IDX);
    NPC_ShowSprite(OAKSLAB_OAK2_IDX);
    gScriptedMovement = 1;
    gState = OLS_OAK_ENTER;
}

void OaksLabScripts_Tick(void) {
    if (wCurMap != MAP_OAKS_LAB) return;

    return;

    switch (gState) {

    case OLS_IDLE:
        if (gRivalExitPending) {
            gState = OLS_POST_BATTLE_SETUP;
        }
        return;

    case OLS_OAK_ENTER:
        if (NPC_IsWalking(OAKSLAB_OAK2_IDX)) return;
        if (gOakEnterSteps >= 3) { gState = OLS_OAK_SWAP; return; }
        NPC_DoScriptedStep(OAKSLAB_OAK2_IDX, DIR_UP);
        gOakEnterSteps++;
        return;

    case OLS_OAK_SWAP:
        if (NPC_IsWalking(OAKSLAB_OAK2_IDX)) return;
        NPC_HideSprite(OAKSLAB_OAK2_IDX);
        NPC_ShowSprite(OAKSLAB_OAK1_IDX);
        gDelay = 3;
        gState = OLS_PLAYER_ENTER_SETUP;
        return;

    case OLS_PLAYER_ENTER_SETUP:
        if (gDelay-- > 0) return;
        gScriptedMovement = 1;
        gPlayerEnterSteps = 8;
        NPC_SetFacing(OAKSLAB_RIVAL_IDX, DIR_DOWN);
        NPC_SetFacing(OAKSLAB_OAK1_IDX,  DIR_DOWN);
        gState = OLS_PLAYER_ENTER_WALK;
        return;

    case OLS_PLAYER_ENTER_WALK:
        if (Player_IsMoving()) return;
        if (gPlayerEnterSteps <= 0) { gState = OLS_FOLLOWED; return; }
        Player_DoScriptedStep(DIR_UP);
        gPlayerEnterSteps--;
        return;

    case OLS_FOLLOWED:
        if (Player_IsMoving()) return;
        SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB);
        SetEvent(EVENT_FOLLOWED_OAK_INTO_LAB_2);
        NPC_ShowSprite(OAKSLAB_RIVAL_IDX);
        NPC_SetFacing(OAKSLAB_RIVAL_IDX, DIR_UP);
        MapMusic_Restart();
        gSpeechIdx = 0;
        gState = OLS_SPEECH_SHOW;
        return;

    case OLS_SPEECH_SHOW:
        if (gSpeechIdx >= INTRO_SPEECH_COUNT) {
            SetEvent(EVENT_OAK_ASKED_TO_CHOOSE_MON);
            gScriptedMovement = 0;
            gState = OLS_IDLE;
            return;
        }
        Text_ShowASCII(IntroSpeech(gSpeechIdx));
        gState = OLS_SPEECH_WAIT;
        return;

    case OLS_SPEECH_WAIT:
        if (Text_IsOpen()) return;
        gSpeechIdx++;
        gState = OLS_SPEECH_SHOW;
        return;

    case OLS_BALL_CANT_YET:
        Text_ShowASCII(kThoseArePokeballs);
        gState = OLS_BALL_CANT_WAIT;
        return;

    case OLS_BALL_CANT_WAIT:
        if (Text_IsOpen()) return;
        gState = OLS_IDLE;
        return;

    case OLS_BALL_LAST_MON:
        Text_ShowASCII(kLastMon);
        gState = OLS_BALL_LAST_WAIT;
        return;

    case OLS_BALL_LAST_WAIT:
        if (Text_IsOpen()) return;
        gState = OLS_IDLE;
        return;

    case OLS_DEX_SHOW: {

        uint8_t saved0 = wPokedexOwned[0];
        wPokedexOwned[0] |= (1u << 0) | (1u << 1) | (1u << 3);
        wPokedexOwned[0] |= (1u << 6);
        uint8_t dex = gSpeciesToDex[gSelectedSpecies];
        Pokedex_ShowData(dex);

        wPokedexOwned[0] = saved0;
        gShowDataDelay = 10;
        gState = OLS_DEX_WAIT;
        return;
    }

    case OLS_DEX_WAIT:

        if (Pokedex_IsShowingData()) return;
        gState = OLS_DEX_RESTORE;
        return;

    case OLS_DEX_RESTORE:

        if (gShowDataDelay > 0) {
            if (gShowDataDelay == 10) {

                Map_ReloadGfx();
                Font_Load();
                NPC_ReloadTiles();
                Display_SetPalette(0xE4, 0xD0, 0xE0);
            }
            gShowDataDelay--;
            return;
        }

        Text_KeepTilesOnClose();
        gYesNoCursor = 0;
        gYesNoWaitForTextClose = prompt_requires_scroll((const char*)gConfirmText);
        Text_ShowASCII((char*)gConfirmText);
        gState = OLS_BALL_YESNO;
        return;

    case OLS_BALL_CONFIRM:

        if (Text_IsOpen()) return;

        gYesNoCursor = 0;
        yesno_draw();
        gState = OLS_BALL_YESNO;
        return;

    case OLS_BALL_CONFIRM_WAIT:

        gState = OLS_BALL_YESNO;
        return;

    case OLS_BALL_YESNO:
        if (gYesNoWaitForTextClose && Text_IsOpen()) return;
        if (hJoyPressed & PAD_UP) {
            if (gYesNoCursor > 0) { gYesNoCursor--; yesno_draw(); }
        }
        if (hJoyPressed & PAD_DOWN) {
            if (gYesNoCursor < 1) { gYesNoCursor++; yesno_draw(); }
        }
        if (hJoyPressed & (PAD_A | PAD_B)) {
            yesno_clear();
            gYesNoWaitForTextClose = 0;
            Map_BuildScrollView();
            if ((hJoyPressed & PAD_A) && gYesNoCursor == 0) {

                NPC_HideSprite(gSelectedBallIdx);
                set_ball_toggle(gSelectedBallIdx);
                Pokemon_AddToParty(gSelectedSpecies, 5);
                gStarterPartySlot = (int)wPartyCount - 1;
                Pokedex_SetOwned(gSelectedSpecies);
                Text_ShowASCII((char*)gEnergeticText);
                gState = OLS_BALL_ENERGETIC;
            } else {

                Text_ShowASCII(RomText("PokemartBuyingGreetingText"));
                gState = OLS_BALL_DECLINED;
            }
        }
        return;

    case OLS_BALL_DECLINED:

        if (Text_IsOpen()) return;

        gScriptedMovement = 0;
        gState = OLS_IDLE;
        return;

    case OLS_BALL_DECLINED_WAIT:

        gState = OLS_IDLE;
        return;

    case OLS_BALL_ENERGETIC:
        if (Text_IsOpen()) return;

        Audio_PlaySFX_GetKeyItem();
        Text_ShowASCII((char*)gPlayerRcvdText);
        gState = OLS_BALL_ENERGETIC_WAIT;
        return;

    case OLS_BALL_ENERGETIC_WAIT:
        if (Text_IsOpen()) return;
        gYesNoCursor = 0;
        Text_KeepTilesOnClose();
        gYesNoWaitForTextClose = prompt_requires_scroll((const char*)gNickPromptText);
        Text_ShowASCII((char*)gNickPromptText);
        gState = OLS_BALL_NICK_YESNO;
        return;

    case OLS_BALL_NICK_PROMPT:
        if (Text_IsOpen()) return;
        yesno_draw();
        gState = OLS_BALL_NICK_YESNO;
        return;

    case OLS_BALL_NICK_YESNO:
        if (gYesNoWaitForTextClose && Text_IsOpen()) return;
        if (hJoyPressed & PAD_UP) {
            if (gYesNoCursor > 0) { gYesNoCursor--; yesno_draw(); }
        }
        if (hJoyPressed & PAD_DOWN) {
            if (gYesNoCursor < 1) { gYesNoCursor++; yesno_draw(); }
        }
        if (hJoyPressed & (PAD_A | PAD_B)) {
            int chose_yes = (hJoyPressed & PAD_A) && gYesNoCursor == 0;
            yesno_clear();
            gYesNoWaitForTextClose = 0;
            Map_BuildScrollView();
            if (chose_yes &&
                gStarterPartySlot >= 0 && gStarterPartySlot < PARTY_LENGTH) {
                NamingScreen_Open(NAME_MON_SCREEN, gSelectedSpecies, wPartyMonNicks[gStarterPartySlot]);
                gState = OLS_BALL_NICK_WAIT;
            } else {
                gState = OLS_BALL_RECEIVED;
            }
        }
        return;

    case OLS_BALL_NICK_WAIT:
        if (NamingScreen_IsOpen()) return;
        gState = OLS_BALL_RECEIVED;
        return;

    case OLS_BALL_RECEIVED:

        {

            static const int middle_ball_1[] = {
                DIR_DOWN, DIR_DOWN, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_UP
            };
            static const int middle_ball_2[] = {
                DIR_DOWN, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT
            };
            static const int right_ball_1[] = {
                DIR_DOWN, DIR_DOWN, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_UP
            };
            static const int right_ball_2[] = {
                DIR_DOWN, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT, DIR_RIGHT
            };
            static const int left_ball_1[] = {
                DIR_DOWN, DIR_RIGHT, DIR_RIGHT
            };
            static const int left_ball_2[] = {
                DIR_RIGHT
            };
            const int *src = NULL;

            if (gSelectedSpecies == STARTER1) {
                if ((int)wYCoord == 4) {
                    src = middle_ball_1;
                    gRivalWalkLen = (int)(sizeof(middle_ball_1) / sizeof(middle_ball_1[0]));
                } else {
                    src = middle_ball_2;
                    gRivalWalkLen = (int)(sizeof(middle_ball_2) / sizeof(middle_ball_2[0]));
                }
            } else if (gSelectedSpecies == STARTER2) {
                if ((int)wYCoord == 4) {
                    src = right_ball_1;
                    gRivalWalkLen = (int)(sizeof(right_ball_1) / sizeof(right_ball_1[0]));
                } else {
                    src = right_ball_2;
                    gRivalWalkLen = (int)(sizeof(right_ball_2) / sizeof(right_ball_2[0]));
                }
            } else {
                if ((int)wXCoord == 9) {
                    src = left_ball_2;
                    gRivalWalkLen = (int)(sizeof(left_ball_2) / sizeof(left_ball_2[0]));
                } else {
                    src = left_ball_1;
                    gRivalWalkLen = (int)(sizeof(left_ball_1) / sizeof(left_ball_1[0]));
                }
            }

            for (int i = 0; i < gRivalWalkLen; i++) {
                gRivalWalkDirs[i] = src[i];
            }
            gRivalWalkStep = 0;
        }
        NPC_SetFacing(OAKSLAB_RIVAL_IDX, DIR_DOWN);
        gState = OLS_RIVAL_WALK;
        return;

    case OLS_BALL_RECEIVED_WAIT:

        gState = OLS_RIVAL_WALK;
        return;

    case OLS_RIVAL_WALK:
        if (NPC_IsWalking(OAKSLAB_RIVAL_IDX)) return;
        if (gRivalWalkStep >= gRivalWalkLen) {
            gState = OLS_RIVAL_TAKES;
            return;
        }
        NPC_DoScriptedStep(OAKSLAB_RIVAL_IDX, gRivalWalkDirs[gRivalWalkStep]);
        gRivalWalkStep++;
        return;

    case OLS_RIVAL_WALK_WAIT:

        gState = OLS_RIVAL_TAKES;
        return;

    case OLS_RIVAL_TAKES:
        if (NPC_IsWalking(OAKSLAB_RIVAL_IDX)) return;
        NPC_SetFacing(OAKSLAB_RIVAL_IDX, DIR_UP);
        Text_ShowASCII(kRivalIllTakeThisOne);
        gState = OLS_RIVAL_TAKES_WAIT;
        return;

    case OLS_RIVAL_TAKES_WAIT:
        if (Text_IsOpen()) return;

        NPC_HideSprite(gRivalBallIdx);
        set_ball_toggle(gRivalBallIdx);
        wRivalStarter = gRivalStarterSpecies;
        SetEvent(EVENT_GOT_STARTER);
        Text_ShowASCII((char*)gRivalRcvdText);
        gState = OLS_RIVAL_RCVD;
        return;

    case OLS_RIVAL_RCVD:
        if (Text_IsOpen()) return;
        gState = OLS_RIVAL_RCVD_WAIT;
        return;

    case OLS_RIVAL_RCVD_WAIT:

        gScriptedMovement = 0;
        gState = OLS_AWAIT_CHALLENGE;
        return;

    case OLS_AWAIT_CHALLENGE:

        if (wYCoord < CHALLENGE_TRIGGER_Y) return;
        gScriptedMovement = 1;
        NPC_SetFacing(OAKSLAB_RIVAL_IDX, DIR_DOWN);
        NPC_SetFacing(OAKSLAB_OAK1_IDX,  DIR_DOWN);
        Music_Play(MUSIC_MEET_RIVAL);
        Text_ShowASCII(kRivalIllTakeYouOn);
        gState = OLS_CHALLENGE_TEXT;
        return;

    case OLS_CHALLENGE_TEXT:
        if (Text_IsOpen()) return;
        gState = OLS_CHALLENGE_WAIT;
        return;

    case OLS_CHALLENGE_WAIT: {

        int rx, ry;
        NPC_GetTilePos(OAKSLAB_RIVAL_IDX, &rx, &ry);
        gRivalApproachLen  = find_path(rx, ry, (int)wXCoord, (int)wYCoord,
                                       gRivalApproachDirs);
        gRivalApproachStep = 0;
        gState = OLS_RIVAL_APPROACH;
        return;
    }

    case OLS_RIVAL_APPROACH:
        if (NPC_IsWalking(OAKSLAB_RIVAL_IDX)) return;
        if (gRivalApproachStep >= gRivalApproachLen) {
            gState = OLS_RIVAL_APPROACH_WAIT;
            return;
        }
        NPC_DoScriptedStep(OAKSLAB_RIVAL_IDX, gRivalApproachDirs[gRivalApproachStep]);
        gRivalApproachStep++;
        return;

    case OLS_RIVAL_APPROACH_WAIT:
        if (NPC_IsWalking(OAKSLAB_RIVAL_IDX)) return;
        gState = OLS_BATTLE_TRIGGER;
        return;

    case OLS_BATTLE_TRIGGER: {

        uint8_t trainer_no;
        if      (wRivalStarter == STARTER2) trainer_no = 1;
        else if (wRivalStarter == STARTER3) trainer_no = 2;
        else                                trainer_no = 3;

        NPC_GetTilePos(OAKSLAB_RIVAL_IDX, &gRivalSavedX, &gRivalSavedY);
        gRivalExitPending    = 1;

        gTrainerAfterText    = kRivalIPickedTheWrongPokemon;

        gPendingBattleClass  = OPP_RIVAL1 - OPP_ID_OFFSET;
        gPendingBattleNo     = trainer_no;
        gPendingBattle       = 1;
        gState               = OLS_IDLE;
        return;
    }

    case OLS_POST_BATTLE_SETUP:

        NPC_SetTilePos(OAKSLAB_RIVAL_IDX, gRivalSavedX, gRivalSavedY);
        gScriptedMovement = 1;
        set_player_facing(DIR_UP);
        NPC_SetFacing(OAKSLAB_RIVAL_IDX, DIR_DOWN);
        Pokemon_HealParty();
        SetEvent(EVENT_BATTLED_RIVAL_IN_OAKS_LAB);
        gDelay = 20;
        gState = OLS_POST_BATTLE_DELAY;
        return;

    case OLS_POST_BATTLE_DELAY:
        if (gDelay-- > 0) return;
        Text_ShowASCII(kRivalSmellYouLater);
        gRivalExitStep = 0;
        gState = OLS_POST_BATTLE_TEXT;
        return;

    case OLS_POST_BATTLE_TEXT:
        if (Text_IsOpen()) return;
        Music_PlayRivalAlternateStart();
        NPC_SetFacing(OAKSLAB_RIVAL_IDX, DIR_DOWN);
        update_player_watch_rival_exit();
        gState = OLS_RIVAL_EXIT_WAIT;
        return;

    case OLS_RIVAL_EXIT_WAIT:

        if (NPC_IsWalking(OAKSLAB_RIVAL_IDX)) return;
        if (gRivalExitStep == 0) {

            int exit_dir = ((int)wXCoord == 4) ? DIR_RIGHT : DIR_LEFT;
            NPC_DoScriptedStep(OAKSLAB_RIVAL_IDX, exit_dir);
            gRivalExitStep++;
            update_player_watch_rival_exit();
        } else if (gRivalExitStep <= RIVAL_EXIT_STEPS) {

            NPC_DoScriptedStep(OAKSLAB_RIVAL_IDX, DIR_DOWN);
            gRivalExitStep++;
            update_player_watch_rival_exit();
        } else {
            gState = OLS_RIVAL_GONE;
        }
        return;

    case OLS_RIVAL_EXIT_WALK:
    case OLS_POST_BATTLE_WAIT:

        gState = OLS_RIVAL_EXIT_WAIT;
        return;

    case OLS_RIVAL_GONE:
        if (NPC_IsWalking(OAKSLAB_RIVAL_IDX)) return;
        NPC_HideSprite(OAKSLAB_RIVAL_IDX);
        gRivalExitPending = 0;
        gScriptedMovement = 0;
        MapMusic_Restart();
        gState = OLS_IDLE;
        return;

    case OLS_PARCEL_TEXT1:
        if (Text_IsOpen()) return;

        Inventory_Remove(ITEM_OAKS_PARCEL, 1);
        Text_ShowASCII(kOakParcelText2);
        gState = OLS_PARCEL_TEXT2;
        return;

    case OLS_PARCEL_TEXT2:

        if (Text_IsOpen()) return;
        Music_PlayRivalAlternateStart();
        Text_ShowASCII(kRivalGramps);
        gState = OLS_PARCEL_SHOW_RIVAL;
        return;

    case OLS_PARCEL_SHOW_RIVAL:

        if (Text_IsOpen()) return;
        NPC_SetTilePos(OAKSLAB_RIVAL_IDX, 4, 10);
        NPC_SetFacing(OAKSLAB_RIVAL_IDX, DIR_UP);
        NPC_ShowSprite(OAKSLAB_RIVAL_IDX);
        gRivalWalkInStep = 0;
        gState = OLS_RIVAL_WALK_IN;
        return;

    case OLS_RIVAL_WALK_IN:

        if (NPC_IsWalking(OAKSLAB_RIVAL_IDX)) return;
        if (gRivalWalkInStep < RIVAL_WALK_IN_STEPS) {
            NPC_DoScriptedStep(OAKSLAB_RIVAL_IDX, DIR_UP);
            gRivalWalkInStep++;
        } else {
            gState = OLS_RIVAL_ARRIVE_TEXT;
        }
        return;

    case OLS_RIVAL_ARRIVE_TEXT:

        if (NPC_IsWalking(OAKSLAB_RIVAL_IDX)) return;
        NPC_SetFacing(OAKSLAB_RIVAL_IDX, DIR_UP);
        Text_ShowASCII(kRivalWhatDidYouCall);
        gState = OLS_OAK_REQUEST_TEXT;
        return;

    case OLS_OAK_REQUEST_TEXT:
        if (Text_IsOpen()) return;
        Text_ShowASCII(kOakRequest);
        gState = OLS_POKEDEX_TEXT;
        return;

    case OLS_POKEDEX_TEXT:

        if (Text_IsOpen()) return;
        Text_ShowASCII(kOakPokedexText);
        gState = OLS_GIVE_POKEDEX;
        return;

    case OLS_GIVE_POKEDEX:

        if (Text_IsOpen()) return;
        Inventory_Add(ITEM_POKEDEX, 1);
        SetEvent(EVENT_OAK_GOT_PARCEL);
        Text_ShowASCII(kOakGivesPokedex);
        gState = OLS_GAVE_POKEDEX_TEXT;
        return;

    case OLS_GAVE_POKEDEX_TEXT:

        if (Text_IsOpen()) return;
        SetEvent(EVENT_GOT_POKEDEX);
        NPC_HideSprite(OAKSLAB_POKEDEX1_IDX);
        NPC_HideSprite(OAKSLAB_POKEDEX2_IDX);
        Text_ShowASCII(kOakDreamText);
        gState = OLS_OAK_DREAM_TEXT;
        return;

    case OLS_OAK_DREAM_TEXT:

        if (Text_IsOpen()) return;
        Text_ShowASCII(kRivalLeaveText);
        gState = OLS_RIVAL_LEAVE_TEXT;
        return;

    case OLS_RIVAL_LEAVE_TEXT:

        if (Text_IsOpen()) return;
        NPC_SetFacing(OAKSLAB_RIVAL_IDX, DIR_DOWN);
        Music_PlayRivalAlternateStart();
        gRivalPokedexExitStep = 0;
        gState = OLS_RIVAL_LEAVE_WALK;
        return;

    case OLS_RIVAL_LEAVE_WALK:
        if (NPC_IsWalking(OAKSLAB_RIVAL_IDX)) return;
        if (gRivalPokedexExitStep >= RIVAL_POKEDEX_EXIT_STEPS) {
            NPC_HideSprite(OAKSLAB_RIVAL_IDX);
            gState = OLS_RIVAL_LEAVE_DONE;
            return;
        }
        NPC_DoScriptedStep(OAKSLAB_RIVAL_IDX, DIR_DOWN);
        gRivalPokedexExitStep++;
        return;

    case OLS_RIVAL_LEAVE_DONE:
        gScriptedMovement = 0;
        MapMusic_Restart();

        SetEvent(EVENT_1ST_ROUTE22_RIVAL_BATTLE);
        ClearEvent(EVENT_2ND_ROUTE22_RIVAL_BATTLE);
        SetEvent(EVENT_ROUTE22_RIVAL_WANTS_BATTLE);
        gState = OLS_IDLE;
        return;
    }
}
