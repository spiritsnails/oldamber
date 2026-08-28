
#include "pallet_scripts.h"
#include "rom_text.h"
#include "text.h"
#include "music.h"
#include "npc.h"
#include "player.h"
#include "trainer_sight.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../data/event_constants.h"

#include <stdlib.h>
#include <stdio.h>

#define MAP_PALLET_TOWN     0x00
#define OAK_NPC_IDX         0

#define DIR_DOWN    0
#define DIR_UP      1
#define DIR_LEFT    2
#define DIR_RIGHT   3
#define DIR_CHANGE_FACING  4

#define kOakHeyWait (RomText("_PalletTownOakHeyWaitDontGoOutText"))

#define kOakItsUnsafe (RomText("PalletTownOakText.ItsUnsafeText"))

typedef struct { int dir; int count; } rle_entry_t;

static const rle_entry_t kOakWalkToLab[] = {
    { DIR_DOWN,  5 },
    { DIR_LEFT,  1 },
    { DIR_DOWN,  5 },
    { DIR_RIGHT, 3 },
    { DIR_UP,    1 },
    { DIR_CHANGE_FACING, 1 },
    { -1, 0 }
};

static const rle_entry_t kPlayerWalkToLab[] = {
    { DIR_UP,    2 },
    { DIR_RIGHT, 3 },
    { DIR_DOWN,  5 },
    { DIR_LEFT,  1 },
    { DIR_DOWN,  6 },
    { -1, 0 }
};

static int find_path_to_player(int oak_x, int oak_y,
                               int player_x, int player_y,
                               int *dirs)
{

    int ydist = abs(oak_y - player_y);
    int xdist = abs(oak_x - player_x);

    if (ydist > 0) ydist--;

    int ydir = (player_y < oak_y) ? DIR_UP : DIR_DOWN;
    int xdir = (player_x < oak_x) ? DIR_LEFT : DIR_RIGHT;

    int yprog = 0, xprog = 0;
    int count = 0;

    while (yprog < ydist || xprog < xdist) {
        int yrem = ydist - yprog;
        int xrem = xdist - xprog;
        if (yrem == 0 && xrem == 0) break;

        if (xrem >= yrem && xprog < xdist) {
            dirs[count++] = xdir;
            xprog++;
        } else {
            dirs[count++] = ydir;
            yprog++;
        }
    }
    return count;
}

static int decode_rle(const rle_entry_t *rle, int *dirs) {
    int n = 0;
    for (int i = 0; rle[i].dir != -1; i++) {
        for (int j = 0; j < rle[i].count; j++)
            dirs[n++] = rle[i].dir;
    }
    return n;
}

static void reverse_dirs(int *dirs, int count) {
    for (int i = 0; i < count / 2; i++) {
        int tmp = dirs[i];
        dirs[i] = dirs[count - 1 - i];
        dirs[count - 1 - i] = tmp;
    }
}

typedef enum {
    PS_IDLE = 0,

    PS_TRIGGER,

    PS_TEXT_HEY_WAIT,
    PS_TEXT_HEY_WAIT_CLOSE,
    PS_EMOTE,
    PS_SHOW_OAK,

    PS_OAK_FACE_UP,
    PS_OAK_WALK_PATH,
    PS_OAK_WALK_WAIT,

    PS_OAK_ARRIVED,
    PS_TEXT_UNSAFE,
    PS_TEXT_UNSAFE_CLOSE,

    PS_MOVE_OAK_LEFT,
    PS_MOVE_OAK_LEFT_WAIT,
    PS_MOVE_PLAYER_LEFT,
    PS_MOVE_PLAYER_LEFT_WAIT,
    PS_WALK_TO_LAB,
    PS_WALK_TO_LAB_STEP,
    PS_WALK_DONE_WAIT,

    PS_DONE,
} PalletState;

static PalletState gState = PS_IDLE;
static int gTimer = 0;

static int sOakPath[32];
static int sOakPathLen = 0;
static int sOakPathIdx = 0;

static int sMoveLeftSteps = 0;
static int sMoveLeftIdx = 0;

static int sOakLabDirs[32];
static int sOakLabLen = 0;
static int sOakLabIdx = 0;

static int sPlayerLabDirs[32];
static int sPlayerLabLen = 0;
static int sPlayerLabIdx = 0;

int PalletScripts_IsActive(void) { return gState != PS_IDLE; }

void PalletScripts_OnMapLoad(void) {

    return;

    if (wCurMap != MAP_PALLET_TOWN) {

        gState = PS_IDLE;
        return;
    }
    if (!CheckEvent(EVENT_OAK_APPEARED_IN_PALLET)
        || CheckEvent(EVENT_FOLLOWED_OAK_INTO_LAB)) {
        NPC_HideSprite(OAK_NPC_IDX);
    }

    gState = PS_IDLE;
}

void PalletScripts_Tick(void) {
    switch (gState) {

    case PS_IDLE: {

        return;
        if (wCurMap != MAP_PALLET_TOWN) return;
        if (CheckEvent(EVENT_FOLLOWED_OAK_INTO_LAB)) return;

        if (Player_IsMoving()) return;
        if ((int)wYCoord != 1) return;

        printf("[pallet] Oak encounter triggered at y=%d\n", wYCoord);

        gPlayerFacing = DIR_DOWN;

        Music_Play(MUSIC_MEET_PROF_OAK);

        gScriptedMovement = 1;

        SetEvent(EVENT_OAK_APPEARED_IN_PALLET);

        gState = PS_TEXT_HEY_WAIT;
        return;
    }

    case PS_TEXT_HEY_WAIT:

        Text_ShowASCII(kOakHeyWait);
        gState = PS_TEXT_HEY_WAIT_CLOSE;
        return;

    case PS_TEXT_HEY_WAIT_CLOSE:
        if (Text_IsOpen()) return;

        Emote_ShowOnPlayer();
        gTimer = 60;
        gState = PS_EMOTE;
        return;

    case PS_EMOTE:
        if (--gTimer > 0) return;
        Emote_Hide();
        gPlayerFacing = DIR_DOWN;

        NPC_ShowSprite(OAK_NPC_IDX);
        gState = PS_OAK_FACE_UP;
        gTimer = 3;
        return;

    case PS_SHOW_OAK:

        gState = PS_OAK_FACE_UP;
        gTimer = 3;
        return;

    case PS_OAK_FACE_UP:

        if (gTimer > 0) {
            if (gTimer == 3) NPC_SetFacing(OAK_NPC_IDX, DIR_UP);
            gTimer--;
            return;
        }

        {
            int oak_x, oak_y;
            NPC_GetTilePos(OAK_NPC_IDX, &oak_x, &oak_y);
            sOakPathLen = find_path_to_player(
                oak_x, oak_y, (int)wXCoord, (int)wYCoord, sOakPath);
            sOakPathIdx = 0;
        }
        printf("[pallet] Oak path: %d steps\n", sOakPathLen);
        gState = PS_OAK_WALK_PATH;
        return;

    case PS_OAK_WALK_PATH:

        if (NPC_IsWalking(OAK_NPC_IDX)) return;
        if (sOakPathIdx >= sOakPathLen) {

            gState = PS_OAK_ARRIVED;
            return;
        }
        NPC_DoScriptedStep(OAK_NPC_IDX, sOakPath[sOakPathIdx++]);
        return;

    case PS_OAK_WALK_WAIT:

        return;

    case PS_OAK_ARRIVED:

        if (NPC_IsWalking(OAK_NPC_IDX)) return;

        gPlayerFacing = DIR_DOWN;
        gState = PS_TEXT_UNSAFE;
        return;

    case PS_TEXT_UNSAFE:

        Text_ShowASCII(kOakItsUnsafe);
        gState = PS_TEXT_UNSAFE_CLOSE;
        return;

    case PS_TEXT_UNSAFE_CLOSE:
        if (Text_IsOpen()) return;

        {
            int px = (int)wXCoord;
            sMoveLeftSteps = (px > 10) ? (px - 10) : 0;
            sMoveLeftIdx = 0;
        }
        if (sMoveLeftSteps > 0) {
            gState = PS_MOVE_OAK_LEFT;
        } else {

            gState = PS_WALK_TO_LAB;
        }
        return;

    case PS_MOVE_OAK_LEFT:
        if (NPC_IsWalking(OAK_NPC_IDX)) return;
        if (sMoveLeftIdx >= sMoveLeftSteps) {
            gState = PS_MOVE_PLAYER_LEFT;
            return;
        }
        NPC_DoScriptedStep(OAK_NPC_IDX, DIR_LEFT);
        sMoveLeftIdx++;
        return;

    case PS_MOVE_OAK_LEFT_WAIT:

        return;

    case PS_MOVE_PLAYER_LEFT:
        if (Player_IsMoving()) return;
        if (sMoveLeftIdx > sMoveLeftSteps) {

            gState = PS_WALK_TO_LAB;
            return;
        }

        if (sMoveLeftSteps > 0) {

            sMoveLeftIdx = 0;
            gState = PS_MOVE_PLAYER_LEFT_WAIT;
        } else {
            gState = PS_WALK_TO_LAB;
        }
        return;

    case PS_MOVE_PLAYER_LEFT_WAIT:
        if (Player_IsMoving()) return;
        if (sMoveLeftIdx >= sMoveLeftSteps) {
            gState = PS_WALK_TO_LAB;
            return;
        }
        Player_DoScriptedStep(DIR_LEFT);
        sMoveLeftIdx++;
        return;

    case PS_WALK_TO_LAB: {

        sOakLabLen = decode_rle(kOakWalkToLab, sOakLabDirs);
        sOakLabIdx = 0;
        sPlayerLabLen = decode_rle(kPlayerWalkToLab, sPlayerLabDirs);

        reverse_dirs(sPlayerLabDirs, sPlayerLabLen);
        sPlayerLabIdx = 0;
        int ox, oy;
        NPC_GetTilePos(OAK_NPC_IDX, &ox, &oy);
        printf("[pallet] Walk to lab start: Oak=(%d,%d) Player=(%d,%d)\n",
               ox, oy, (int)wXCoord, (int)wYCoord);
        printf("[pallet] Oak %d steps, Player %d steps\n",
               sOakLabLen, sPlayerLabLen);
        printf("[pallet] Lab warp is at (24,23)\n");

        gState = PS_WALK_TO_LAB_STEP;
        return;
    }

    case PS_WALK_TO_LAB_STEP: {
        static const char *dname[] = {"DOWN","UP","LEFT","RIGHT","FACE"};
        int oak_busy = NPC_IsWalking(OAK_NPC_IDX);
        int player_busy = Player_IsMoving();

        if (!oak_busy && sOakLabIdx < sOakLabLen) {
            int dir = sOakLabDirs[sOakLabIdx++];
            if (dir == DIR_CHANGE_FACING) {
                NPC_SetFacing(OAK_NPC_IDX, DIR_DOWN);
                printf("[pallet] Oak step %d: CHANGE_FACING\n", sOakLabIdx-1);
            } else {
                NPC_DoScriptedStep(OAK_NPC_IDX, dir);
                int ox, oy; NPC_GetTilePos(OAK_NPC_IDX, &ox, &oy);
                printf("[pallet] Oak step %d/%d: %s → (%d,%d)\n",
                       sOakLabIdx-1, sOakLabLen, dname[dir], ox, oy);
            }
        }

        if (!player_busy && !oak_busy && sPlayerLabIdx < sPlayerLabLen) {
            int dir = sPlayerLabDirs[sPlayerLabIdx++];
            Player_DoScriptedStep(dir);
            printf("[pallet] Player step %d/%d: %s → (%d,%d)\n",
                   sPlayerLabIdx-1, sPlayerLabLen, dname[dir],
                   (int)wXCoord, (int)wYCoord);
        }

        if (sOakLabIdx >= sOakLabLen && sPlayerLabIdx >= sPlayerLabLen &&
            !NPC_IsWalking(OAK_NPC_IDX) && !Player_IsMoving()) {
            printf("[pallet] Walk to lab complete. Player=(%d,%d)\n",
                   (int)wXCoord, (int)wYCoord);
            gState = PS_WALK_DONE_WAIT;
        }
        return;
    }

    case PS_WALK_DONE_WAIT:

        if (Player_IsMoving()) return;

        NPC_HideSprite(OAK_NPC_IDX);

        gScriptedMovement = 0;

        printf("[pallet] Oak encounter complete, awaiting lab handoff\n");
        gState = PS_IDLE;
        return;

    case PS_DONE:
        gScriptedMovement = 0;
        gState = PS_IDLE;
        return;

    default:
        return;
    }
}
