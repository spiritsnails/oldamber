
#include "pokecenter.h"
#include "assetpack_bind.h"
#include "text.h"
#include "rom_text.h"
#include "constants.h"
#include "music.h"
#include "map_music.h"
#include "npc.h"
#include "player.h"
#include "overworld.h"
#include "../data/map_data.h"
#include "amberscript_mapbank.h"
#include <string.h>
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "../platform/display.h"
#include "../data/font_data.h"
#include "../data/moves_data.h"
#include "overworld.h"

#include <stdint.h>
#include <stdio.h>

typedef enum {
    PC_IDLE = 0,
    PC_WAIT_WELCOME,
    PC_YESNO,
    PC_WAIT_NEED,
    PC_NURSE_TURN,
    PC_HEALING,
    PC_HEALING_JINGLE,
    PC_HEALING_WAIT32,
    PC_NURSE_BOW,
    PC_WAIT_HEALED,
    PC_WAIT_FAREWELL,
    PC_WAIT_DECLINE,
} pc_state_t;

static pc_state_t g_state     = PC_IDLE;
static int        g_cursor    = 0;
static int        g_heal_timer = 0;
static int        g_heal_mon   = 0;
static int        g_nurse_npc  = -1;
static int        g_flash_count = 0;
static int        g_flash_timer = 0;
static int        g_flash_on    = 0;
static int        g_used_pokecenter = 0;
static int        g_machine_px  = 0;
static int        g_machine_py  = 0;
static int        g_ball_count  = 0;

#define MACHINE_OAM_BASE  72
#define MACHINE_OAM_COUNT  7
#define OAM_PAL1          0x10
#define OAM_XFLIP         0x20

#define OBP1_NORMAL  0xE0
#define OBP1_FLASH   0xC8

static void pc_set_obp1(uint8_t obp1) {
    Display_SetPalette(0xE4, 0xD0, obp1);
}

static void pc_machine_oam_set(int idx, int spx, int spy, uint8_t tile, uint8_t flags) {
    wShadowOAM[MACHINE_OAM_BASE + idx].y     = (uint8_t)(spy + OAM_Y_OFS);
    wShadowOAM[MACHINE_OAM_BASE + idx].x     = (uint8_t)(spx + OAM_X_OFS);
    wShadowOAM[MACHINE_OAM_BASE + idx].tile  = tile;
    wShadowOAM[MACHINE_OAM_BASE + idx].flags = flags;
}

static void pc_machine_oam_clear(void) {
    for (int i = 0; i < MACHINE_OAM_COUNT; i++)
        wShadowOAM[MACHINE_OAM_BASE + i].y = 0;
}

#define YESNO_COL   11
#define YESNO_ROW    6
#define YESNO_W      9
#define YESNO_H      6
#define TMIDX(r, c) (((r) + 2) * SCROLL_MAP_W + ((c) + 2) + Map_UiColOfs())

#define BC_TL   0x79u
#define BC_H    0x7Au
#define BC_TR   0x7Bu
#define BC_V    0x7Cu
#define BC_BL   0x7Du
#define BC_BR   0x7Eu
#define BC_SP   0x7Fu
#define BC_CUR  0xEDu

static void pc_set_tile(int col, int row, uint8_t tile) {
    if (col < 0 || col >= SCREEN_WIDTH || row < 0 || row >= SCREEN_HEIGHT) return;
    gScrollTileMap[TMIDX(row, col)] = tile;
}

static uint8_t s_saved_yesno[YESNO_H * YESNO_W];
static int s_saved_yesno_valid = 0;

static void pc_save_yesno_tiles(void) {
    for (int r = 0; r < YESNO_H; r++) {
        for (int c = 0; c < YESNO_W; c++) {
            s_saved_yesno[r * YESNO_W + c] = gScrollTileMap[TMIDX(YESNO_ROW + r, YESNO_COL + c)];
        }
    }
    s_saved_yesno_valid = 1;
}

static void pc_restore_yesno_tiles(void) {
    if (!s_saved_yesno_valid) return;
    for (int r = 0; r < YESNO_H; r++) {
        for (int c = 0; c < YESNO_W; c++) {
            gScrollTileMap[TMIDX(YESNO_ROW + r, YESNO_COL + c)] = s_saved_yesno[r * YESNO_W + c];
        }
    }
    s_saved_yesno_valid = 0;
}

static uint8_t pc_char_tile(char c) {
    if (c >= 'A' && c <= 'Z') return (uint8_t)Font_CharToTile((unsigned char)(0x80 + (c - 'A')));
    if (c >= 'a' && c <= 'z') return (uint8_t)Font_CharToTile((unsigned char)(0xA0 + (c - 'a')));
    if (c >= '0' && c <= '9') return (uint8_t)Font_CharToTile((unsigned char)(0xF6 + (c - '0')));
    if (c == '!') return (uint8_t)Font_CharToTile(0xE7u);
    return (uint8_t)Font_CharToTile(BC_SP);
}

static void pc_put_label(int col, int row, const char *s) {
    int c = col;
    while (*s) {
        pc_set_tile(c++, row, pc_char_tile(*s++));
    }
}

static void pc_draw_yesno_box(void) {
    if (!s_saved_yesno_valid) pc_save_yesno_tiles();

    for (int r = 1; r < YESNO_H - 1; r++) {
        for (int c = 1; c < YESNO_W - 1; c++) {
            pc_set_tile(YESNO_COL + c, YESNO_ROW + r, (uint8_t)Font_CharToTile(BC_SP));
        }
        pc_set_tile(YESNO_COL, YESNO_ROW + r, (uint8_t)Font_CharToTile(BC_V));
        pc_set_tile(YESNO_COL + YESNO_W - 1, YESNO_ROW + r, (uint8_t)Font_CharToTile(BC_V));
    }

    pc_set_tile(YESNO_COL,             YESNO_ROW,     (uint8_t)Font_CharToTile(BC_TL));
    for (int c = 1; c < YESNO_W - 1; c++)
        pc_set_tile(YESNO_COL + c,     YESNO_ROW,     (uint8_t)Font_CharToTile(BC_H));
    pc_set_tile(YESNO_COL + YESNO_W-1,YESNO_ROW,     (uint8_t)Font_CharToTile(BC_TR));

    pc_set_tile(YESNO_COL + 1,         YESNO_ROW + 2,
                g_cursor == 0 ? (uint8_t)Font_CharToTile(BC_CUR)
                              : (uint8_t)Font_CharToTile(BC_SP));
    pc_put_label(YESNO_COL + 2, YESNO_ROW + 2, "HEAL");

    pc_set_tile(YESNO_COL + 1,         YESNO_ROW + 4,
                g_cursor == 1 ? (uint8_t)Font_CharToTile(BC_CUR)
                              : (uint8_t)Font_CharToTile(BC_SP));
    pc_put_label(YESNO_COL + 2, YESNO_ROW + 4, "CANCEL");

    pc_set_tile(YESNO_COL,             YESNO_ROW + YESNO_H - 1, (uint8_t)Font_CharToTile(BC_BL));
    for (int c = 1; c < YESNO_W - 1; c++)
        pc_set_tile(YESNO_COL + c,     YESNO_ROW + YESNO_H - 1, (uint8_t)Font_CharToTile(BC_H));
    pc_set_tile(YESNO_COL + YESNO_W-1,YESNO_ROW + YESNO_H - 1, (uint8_t)Font_CharToTile(BC_BR));

    NPC_HideOverUITiles();
    Player_HideIfOverUI();
}

static void pc_clear_yesno_box(void) {
    pc_restore_yesno_tiles();
}

void Pokecenter_HealPartyFull(void) {
    for (int i = 0; i < wPartyCount && i < 6; i++) {
        party_mon_t *mon = &wPartyMons[i];

        mon->base.status = 0;

        for (int m = 0; m < 4; m++) {
            uint8_t move_id = mon->base.moves[m];
            if (move_id == 0 || move_id >= NUM_MOVE_DEFS) continue;

            uint8_t pp_ups    = (mon->base.pp[m] >> 6) & 0x03;
            uint8_t base_pp   = gMoves[move_id].pp;
            uint8_t bonus     = (uint8_t)(pp_ups * (base_pp / 5));
            uint16_t new_pp   = (uint16_t)base_pp + bonus;
            if (new_pp > 63) new_pp = 63;
            mon->base.pp[m]   = (uint8_t)((pp_ups << 6) | (uint8_t)new_pp);
        }

        mon->base.hp = mon->max_hp;
    }
}

void Pokecenter_Start(void) {
    g_state     = PC_WAIT_WELCOME;
    g_cursor    = 0;
    g_nurse_npc = NPC_GetLastInteracted();
    Text_KeepTilesOnClose();

    if (!g_used_pokecenter) {
        g_used_pokecenter = 1;

        static char welcome_buf[128];
        snprintf(welcome_buf, sizeof(welcome_buf), "%s\f%s",
                 RomText("_PokemonCenterWelcomeText"), RomText("_ShallWeHealYourPokemonText"));
        Text_ShowASCII(welcome_buf);
    } else {
        Text_ShowASCII(RomText("_PokemonCenterWelcomeText"));
    }
}

int Pokecenter_IsActive(void) {
    return g_state != PC_IDLE;
}

int Pokecenter_IsWaitingYesNo(void) {
    return g_state == PC_YESNO;
}

int  Pokecenter_GetUsedFlag(void)     { return g_used_pokecenter; }
void Pokecenter_SetUsedFlag(int used) { g_used_pokecenter = used ? 1 : 0; }

void Pokecenter_Tick(void) {

    switch (g_state) {

    case PC_WAIT_WELCOME:

        g_cursor = 0;
        pc_draw_yesno_box();
        g_state = PC_YESNO;
        break;

    case PC_YESNO:

        if (hJoyPressed & PAD_UP) {
            if (g_cursor > 0) { g_cursor--; pc_draw_yesno_box(); }
        }
        if (hJoyPressed & PAD_DOWN) {
            if (g_cursor < 1) { g_cursor++; pc_draw_yesno_box(); }
        }

        if (hJoyPressed & PAD_A) {
            pc_clear_yesno_box();
            if (g_cursor == 0) {

                wLastBlackoutMap = wLastMap;
                wLastHealTownMap = wLastMap;
                {

                    const char *nm = AmberScript_MapBank_NameForRealId(wLastMap);
                    if (nm) {
                        strncpy(wLastHealTownName, nm, sizeof(wLastHealTownName) - 1);
                        wLastHealTownName[sizeof(wLastHealTownName) - 1] = '\0';
                    } else {
                        wLastHealTownName[0] = '\0';
                    }
                }
                Map_BuildScrollView();
                Text_ShowASCII(RomText("_NeedYourPokemonText"));
                g_state = PC_WAIT_NEED;
            } else {

                Text_ShowASCII(RomText("_PokemonCenterFarewellText"));
                g_state = PC_WAIT_DECLINE;
            }
        }
        if (hJoyPressed & PAD_B) {

            pc_clear_yesno_box();
            Text_ShowASCII(RomText("_PokemonCenterFarewellText"));
            g_state = PC_WAIT_DECLINE;
        }
        break;

    case PC_WAIT_NEED:

        Map_BuildScrollView();
        Pokecenter_HealPartyFull();
        Music_Stop();
        NPC_SetFacing(g_nurse_npc, 2);

        Display_LoadSpriteTile(0x7C, kHealMachineTiles);
        Display_LoadSpriteTile(0x7D, kHealMachineTiles + 16);

        { int npx, npy; NPC_GetScreenPos(g_nurse_npc, &npx, &npy);
          g_machine_px = npx - 28;
          g_machine_py = npy - 8; }
        g_ball_count = 0;

        pc_machine_oam_set(0, g_machine_px, g_machine_py, 0x7C, OAM_PAL1);
        g_heal_timer = 35;
        g_state = PC_NURSE_TURN;
        break;

    case PC_NURSE_TURN:

        if (--g_heal_timer > 0) break;
        g_heal_mon   = 0;
        g_heal_timer = 0;
        g_state = PC_HEALING;
        break;

    case PC_HEALING:

        if (--g_heal_timer > 0) break;
        if (g_heal_mon < wPartyCount && g_heal_mon < 6) {
            Audio_PlaySFX_HealingMachine();

            static const int8_t ball_dx[6] = {-4, +4, -4, +4, -4, +4};
            static const int8_t ball_dy[6] = { 7,  7, 12, 12, 17, 17};
            static const uint8_t ball_fl[6] = { OAM_PAL1, OAM_PAL1|OAM_XFLIP,
                                                 OAM_PAL1, OAM_PAL1|OAM_XFLIP,
                                                 OAM_PAL1, OAM_PAL1|OAM_XFLIP };
            int m = g_heal_mon;
            pc_machine_oam_set(1 + m,
                               g_machine_px + ball_dx[m],
                               g_machine_py + ball_dy[m],
                               0x7D, ball_fl[m]);
            g_heal_mon++;
            g_heal_timer = 30;
        } else {

            Music_Play(MUSIC_PKMN_HEALED);
            g_flash_count = 0;
            g_flash_timer = 0;
            g_flash_on    = 0;
            g_state = PC_HEALING_JINGLE;
        }
        break;

    case PC_HEALING_JINGLE:

        if (g_flash_count < 8) {
            if (--g_flash_timer <= 0) {
                g_flash_on ^= 1;
                pc_set_obp1(g_flash_on ? OBP1_FLASH : OBP1_NORMAL);
                g_flash_timer = 10;
                g_flash_count++;
            }
        } else {
            if (g_flash_on) { g_flash_on = 0; pc_set_obp1(OBP1_NORMAL); }
            if (Music_IsPlaying()) break;
            g_heal_timer = 32;
            g_state = PC_HEALING_WAIT32;
        }
        break;

    case PC_HEALING_WAIT32:

        if (--g_heal_timer > 0) break;
        pc_machine_oam_clear();
        NPC_SetFacing(g_nurse_npc, 0);

        MapMusic_Restart();
        Text_ShowASCII(RomText("_PokemonFightingFitText"));
        g_state = PC_WAIT_HEALED;
        break;

    case PC_WAIT_HEALED:

        NPC_SetFacing(g_nurse_npc, 1);
        g_heal_timer = 20;
        g_state = PC_NURSE_BOW;
        break;

    case PC_NURSE_BOW:

        if (--g_heal_timer > 0) break;
        NPC_SetFacing(g_nurse_npc, 0);
        Text_ShowASCII(RomText("_PokemonCenterFarewellText"));
        g_state = PC_WAIT_FAREWELL;
        break;

    case PC_WAIT_FAREWELL:
    case PC_WAIT_DECLINE:

        g_state = PC_IDLE;
        break;

    case PC_IDLE:
    default:
        break;
    }
}
