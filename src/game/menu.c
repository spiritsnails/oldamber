
#include "menu.h"
#include "rom_text.h"
#include "trainer_card.h"
#include "bag_menu.h"
#include "party_menu.h"
#include "pokedex.h"
#include "text.h"
#include "overworld.h"
#include "npc.h"
#include "player.h"
#include "safari_zone_scripts.h"
#include "../data/font_data.h"
#include "crystal_menus.h"
#include "crystal_options.h"
#include "crystal_font.h"
#include "johto_music.h"
#include "../data/event_constants.h"
#include "../platform/hardware.h"
#include "../platform/audio.h"
#include "../platform/save.h"
#include <string.h>
#include <stdio.h>

#define BOX_COL_L      10
#define BOX_COL_R      19
#define BOX_ROW_T       0

#define ITEM_COL       12
#define ARROW_COL      11
#define ITEM_ROW_FIRST  2
#define ITEM_ROW_STEP   2
#define NUM_ITEMS_NO_DEX  6
#define NUM_ITEMS_DEX     7

#define CHAR_TERM  0x50
#define CHAR_SPACE 0x7F
#define CHAR_ARROW 0xED

static const uint8_t kStrPokedex[] = {0x8F,0x8E,0x8A,0xBA,0x83,0x84,0x97,CHAR_TERM};
static const uint8_t kStrPokemon[] = {0x8F,0x8E,0x8A,0xBA,0x8C,0x8E,0x8D,CHAR_TERM};
static const uint8_t kStrItem[]    = {0x88,0x93,0x84,0x8C,CHAR_TERM};
static const uint8_t kStrSave[]    = {0x92,0x80,0x95,0x84,CHAR_TERM};
static const uint8_t kStrOption[]  = {0x8E,0x8F,0x93,0x88,0x8E,0x8D,CHAR_TERM};
static const uint8_t kStrExit[]    = {0x84,0x97,0x88,0x93,CHAR_TERM};

static int gMenuOpen     = 0;
static int gMenuCursor   = 0;
static int gMenuNumItems = NUM_ITEMS_NO_DEX;
static int gMenuHasDex   = 0;

#define MENU_OPEN_TEXT_DELAY 15
static int gMenuOpenDelay = 0;

typedef enum {
    SAVE_NONE = 0,
    SAVE_INFO_DELAY,
    SAVE_TEXT,
    SAVE_CHOOSE,
    SAVE_NOW_SAVING,
    SAVE_SAVED,
} save_state_t;
static save_state_t gSaveState = SAVE_NONE;
static int gSaveDelay  = 0;
static int gSaveChoice = 0;
static uint8_t s_yn_saved[5 * 6];

static int gOptState = 0;
static int gOptRow   = 0;
static int gOptSel[3] = {0, 0, 0};

static int gOptStandalone = 0;

static void menu_redraw_over_field(void);

static void opt_close(void) {
    gOptState = 0;
    if (gOptStandalone) return;
    menu_redraw_over_field();
}

static void smset(int col, int row, uint8_t tile) {

    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfsRight()] = tile;
}

static void draw_box(void) {
    int L = BOX_COL_L, R = BOX_COL_R;
    int T = BOX_ROW_T;
    int B = ITEM_ROW_FIRST + (gMenuNumItems - 1) * ITEM_ROW_STEP + 1;

    smset(L, T, (uint8_t)Font_CharToTile(0x79));
    for (int c = L + 1; c < R; c++)
        smset(c, T, (uint8_t)Font_CharToTile(0x7A));
    smset(R, T, (uint8_t)Font_CharToTile(0x7B));

    for (int r = T + 1; r < B; r++) {
        smset(L, r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = L + 1; c < R; c++)
            smset(c, r, (uint8_t)Font_CharToTile(CHAR_SPACE));
        smset(R, r, (uint8_t)Font_CharToTile(0x7C));
    }

    smset(L, B, (uint8_t)Font_CharToTile(0x7D));
    for (int c = L + 1; c < R; c++)
        smset(c, B, (uint8_t)Font_CharToTile(0x7A));
    smset(R, B, (uint8_t)Font_CharToTile(0x7E));
}

static void print_str(int col, int row, const uint8_t *s) {
    for (; *s != CHAR_TERM; s++, col++)
        smset(col, row, (uint8_t)Font_CharToTile(*s));
}

static void print_player_name(int col, int row) {
    for (int i = 0; i < NAME_LENGTH; i++) {
        uint8_t b = wPlayerName[i];
        if (b == 0x00 || b == CHAR_TERM) break;
        smset(col++, row, (uint8_t)Font_CharToTile(b));
    }
}

#define G2_DESC_COL   0
#define G2_DESC_ROW   13
#define G2_DESC_W    10
#define G2_DESC_H     5
#define G2_DESC_TEXT_ROW 14

static int menu_ascii_tile(unsigned char c);

static int menu_gen2(void) { return Font_GetStyle() == FONT_STYLE_GEN2; }

static int s_menu_account = 0;

void Menu_SetGen2Account(int on) { s_menu_account = on ? 1 : 0; }
int  Menu_Gen2Account(void) { return s_menu_account; }

static const char *g2_slot_id(int i) {
    static const char *const kWithDex[] = {
        "POKEDEX", "MONSTER", "PACK", "STATUS", "SAVE", "OPTION", "EXIT" };
    if (!gMenuHasDex) i++;
    if (i < 0 || i >= (int)(sizeof kWithDex / sizeof kWithDex[0])) return 0;
    return kWithDex[i];
}

static const crystal_menu_item_t *g2_item(const char *id) {
    if (!id) return 0;
    for (int i = 0; i < CRYSTAL_START_ITEMS; i++)
        if (strcmp(gCrystalStartMenu[i].id, id) == 0) return &gCrystalStartMenu[i];
    return 0;
}

static int g2_ascii(int col, int row, const char *s) {
    static const uint8_t kPoke[] = {0x8F, 0x8E, 0x8A, 0xBA};
    for (; *s && *s != '\n'; s++) {
        if (*s == '#') {
            for (int i = 0; i < 4; i++)
                smset(col++, row, (uint8_t)Font_CharToTile(kPoke[i]));
        } else if (*s == '\'') {
            smset(col++, row, (uint8_t)Font_CharToTile(0xE0));
        } else {
            smset(col++, row, (uint8_t)menu_ascii_tile((unsigned char)*s));
        }
    }
    return col;
}

static void g2_draw_desc(int item) {
    const crystal_menu_item_t *it;
    if (!s_menu_account) return;
    it = g2_item(g2_slot_id(item));
    for (int r = G2_DESC_ROW; r < G2_DESC_ROW + G2_DESC_H; r++)
        for (int c = G2_DESC_COL; c < G2_DESC_COL + G2_DESC_W; c++)
            smset(c, r, (uint8_t)Font_CharToTile(CHAR_SPACE));
    if (!it || !it->desc) return;
    const char *nl = strchr(it->desc, '\n');
    g2_ascii(G2_DESC_COL, G2_DESC_TEXT_ROW, it->desc);
    if (nl) g2_ascii(G2_DESC_COL, G2_DESC_TEXT_ROW + 1, nl + 1);
}

static void draw_items(void) {
    int row = ITEM_ROW_FIRST;
    if (menu_gen2()) {
        for (int i = 0; i < gMenuNumItems; i++, row += ITEM_ROW_STEP) {
            const char *id = g2_slot_id(i);
            const crystal_menu_item_t *it = g2_item(id);

            if (id && strcmp(id, "STATUS") == 0) print_player_name(ITEM_COL, row);
            else if (it) g2_ascii(ITEM_COL, row, it->label);
        }
        return;
    }
    if (gMenuHasDex) {
        print_str(ITEM_COL, row, kStrPokedex); row += ITEM_ROW_STEP;
    }
    print_str(ITEM_COL, row, kStrPokemon); row += ITEM_ROW_STEP;
    print_str(ITEM_COL, row, kStrItem);    row += ITEM_ROW_STEP;
    print_player_name(ITEM_COL, row);      row += ITEM_ROW_STEP;
    print_str(ITEM_COL, row, kStrSave);    row += ITEM_ROW_STEP;
    print_str(ITEM_COL, row, kStrOption);  row += ITEM_ROW_STEP;
    print_str(ITEM_COL, row, kStrExit);
}

static void set_cursor(int item, uint8_t tile) {
    smset(ARROW_COL, ITEM_ROW_FIRST + item * ITEM_ROW_STEP, tile);

    if (menu_gen2() && tile != (uint8_t)Font_CharToTile(CHAR_SPACE))
        g2_draw_desc(item);
}

static void menu_occlude_npcs_behind_box(void) {

    const int box_left_px  = (BOX_COL_L + Map_UiColOfsRight()) * 8;
    int box_bottom_row = ITEM_ROW_FIRST + (gMenuNumItems - 1) * ITEM_ROW_STEP + 1;
    int box_bottom_px  = (box_bottom_row + 1) * 8;
    for (int i = 4; i < 68; i++) {
        if (wShadowOAM[i].y == 0) continue;
        int sx = (int)wShadowOAM[i].x - 8;
        int sy = (int)wShadowOAM[i].y - 16;
        if (sx + 8 > box_left_px && sy < box_bottom_px)
            wShadowOAM[i].y = 0;
    }
}

void Menu_Open(void) {
    gMenuHasDex  = CheckEvent(EVENT_GOT_POKEDEX) ? 1 : 0;
    gMenuNumItems = gMenuHasDex ? NUM_ITEMS_DEX : NUM_ITEMS_NO_DEX;
    gMenuOpen    = 1;
    gMenuCursor  = 0;
    gSaveState   = SAVE_NONE;
    gOptState    = 0;

    draw_box();
    menu_occlude_npcs_behind_box();
    gMenuOpenDelay = MENU_OPEN_TEXT_DELAY;
}

int Menu_IsOpen(void) {
    return gMenuOpen;
}

static void menu_close(void) {
    gMenuOpen = 0;
    Map_BuildScrollView();
    NPC_BuildView(0, 0);
}

static void draw_safari_box(void);

static void menu_redraw_over_field(void) {
    Map_ReloadGfx();
    Font_Load();
    NPC_ReloadTiles();
    Map_BuildScrollView();
    NPC_BuildView(0, 0);
    Player_SyncOAM();
    draw_box();
    draw_items();
    draw_safari_box();
    set_cursor(gMenuCursor, (uint8_t)Font_CharToTile(CHAR_ARROW));
    menu_occlude_npcs_behind_box();
}

void Menu_DrawBackdropForBag(void) {
    menu_redraw_over_field();
}

void Menu_ResumeFromBag(void) {
    gMenuOpen = 1;
    menu_redraw_over_field();
}

extern unsigned long gPlayTimeFrames;

static int menu_ascii_tile(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return (int)Font_CharToTile(0x80 + (c - 'A'));
    if (c >= 'a' && c <= 'z') return (int)Font_CharToTile(0xA0 + (c - 'a'));
    if (c >= '0' && c <= '9') return (int)Font_CharToTile(0xF6 + (c - '0'));
    if (c == ':') return (int)Font_CharToTile(0x9C);
    if (c == '.') return (int)Font_CharToTile(0xE8);
    if (c == '!') return (int)Font_CharToTile(0xE7);
    if (c == '#') return (int)Font_CharToTile(0xEB);
    if (c == '/') return (int)Font_CharToTile(0xF3);
    return (int)Font_CharToTile(CHAR_SPACE);
}
static void menu_ascii(int col, int row, const char *s) {
    for (; *s; s++, col++) smset(col, row, (uint8_t)menu_ascii_tile((unsigned char)*s));
}

static void draw_textbox(int L, int T, int R, int B) {
    smset(L, T, (uint8_t)Font_CharToTile(0x79));
    smset(R, T, (uint8_t)Font_CharToTile(0x7B));
    smset(L, B, (uint8_t)Font_CharToTile(0x7D));
    smset(R, B, (uint8_t)Font_CharToTile(0x7E));
    for (int c = L + 1; c < R; c++) {
        smset(c, T, (uint8_t)Font_CharToTile(0x7A));
        smset(c, B, (uint8_t)Font_CharToTile(0x7A));
    }
    for (int r = T + 1; r < B; r++) {
        smset(L, r, (uint8_t)Font_CharToTile(0x7C));
        smset(R, r, (uint8_t)Font_CharToTile(0x7C));
        for (int c = L + 1; c < R; c++) smset(c, r, (uint8_t)Font_CharToTile(CHAR_SPACE));
    }
}

static int menu_count_bits(const uint8_t *p, int nbytes) {
    int n = 0;
    for (int i = 0; i < nbytes; i++) { uint8_t b = p[i]; while (b) { n += b & 1; b >>= 1; } }
    return n;
}

static void draw_num(int col, int row, int val, int digits) {
    char buf[8];
    if (val < 0) val = 0;
    snprintf(buf, sizeof(buf), "%*d", digits, val);
    for (int i = 0; i < digits; i++)
        smset(col + i, row, buf[i] == ' ' ? (uint8_t)Font_CharToTile(CHAR_SPACE)
                                          : (uint8_t)Font_CharToTile(0xF6 + (buf[i] - '0')));
}

static void draw_num_keep(int col, int row, int val, int digits) {
    char buf[8];
    if (val < 0) val = 0;
    snprintf(buf, sizeof(buf), "%*d", digits, val);
    for (int i = 0; i < digits; i++)
        if (buf[i] != ' ')
            smset(col + i, row, (uint8_t)Font_CharToTile(0xF6 + (buf[i] - '0')));
}

static void draw_safari_box(void) {
    if (!SafariZoneScripts_MapShowsStepCounter(wCurMap)) return;
    draw_textbox(0, 0, 8, 4);
    draw_num_keep(1, 1, (int)wSafariSteps, 3);
    menu_ascii(4, 1, "/500");
    menu_ascii(1, 3, "BALL");
    smset(5, 3, (uint8_t)Font_CharToTile(0xF1));
    smset(6, 3, (uint8_t)Font_CharToTile(0xF1));
    smset(7, 3, (uint8_t)Font_CharToTile(CHAR_SPACE));
    if (wNumSafariBalls < 10)
        smset(5, 3, (uint8_t)Font_CharToTile(CHAR_SPACE));
    draw_num_keep(6, 3, (int)wNumSafariBalls, 2);
}

static void draw_save_info_box(void) {

    draw_textbox(4, 0, 19, 9);
    menu_ascii(5, 2, "PLAYER");
    print_player_name(12, 2);
    menu_ascii(5, 4, "BADGES");
    draw_num(17, 4, menu_count_bits(&wObtainedBadges, 1), 2);
    print_str(5, 6, kStrPokedex);
    draw_num(16, 6, menu_count_bits(wPokedexOwned, 19), 3);
    menu_ascii(5, 8, "TIME");
    unsigned long secs = gPlayTimeFrames / 60;
    int hrs  = (int)(secs / 3600); if (hrs > 999) hrs = 999;
    int mins = (int)((secs / 60) % 60);
    draw_num(13, 8, hrs, 3);
    menu_ascii(16, 8, ":");
    smset(17, 8, (uint8_t)Font_CharToTile(0xF6 + (mins / 10)));
    smset(18, 8, (uint8_t)Font_CharToTile(0xF6 + (mins % 10)));
}

static void draw_save_msg(const char *l1, const char *l2) {
    draw_textbox(0, 12, 19, 17);
    if (l1) menu_ascii(1, 14, l1);
    if (l2) menu_ascii(1, 16, l2);
}

static void draw_saved_game_msg(void) {
    draw_textbox(0, 12, 19, 17);
    int col = 1;
    for (int i = 0; i < NAME_LENGTH; i++) {
        uint8_t b = wPlayerName[i];
        if (b == 0x00 || b == CHAR_TERM) break;
        smset(col++, 14, (uint8_t)Font_CharToTile(b));
    }
    menu_ascii(col, 14, " saved");
    menu_ascii(1, 16, "the game!");
}

static uint8_t smget(int col, int row) {
    return gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfsRight()];
}

static void draw_save_yesno(void) {
    draw_textbox(0, 7, 5, 11);
    menu_ascii(2, 8,  "YES");
    menu_ascii(2, 10, "NO");
    smset(1, 8,  (uint8_t)Font_CharToTile(gSaveChoice == 0 ? 0xED : CHAR_SPACE));
    smset(1, 10, (uint8_t)Font_CharToTile(gSaveChoice == 1 ? 0xED : CHAR_SPACE));
}
static void save_yn_tiles(void)    { for (int r=0;r<5;r++) for (int c=0;c<6;c++) s_yn_saved[r*6+c] = smget(c, 7+r); }
static void restore_yn_tiles(void) { for (int r=0;r<5;r++) for (int c=0;c<6;c++) smset(c, 7+r, s_yn_saved[r*6+c]); }

static void menu_start_save(void) {

    for (int i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
    draw_save_info_box();
    gSaveDelay = 30;
    gSaveState = SAVE_INFO_DELAY;
}

static void menu_tick_save(void) {
    switch (gSaveState) {
    case SAVE_INFO_DELAY:
        if (--gSaveDelay > 0) return;

        wDoNotWaitForButtonPress = 1;
        Text_KeepTilesOnClose();
        Text_ShowASCII(RomText("_WouldYouLikeToSaveText"));
        gSaveState = SAVE_TEXT;
        return;
    case SAVE_TEXT:
        if (Text_IsOpen()) return;

        Text_BlitBoxToBGAndHideWindow();
        gSaveChoice = 0;
        save_yn_tiles();
        draw_save_yesno();
        gSaveState = SAVE_CHOOSE;
        return;
    case SAVE_CHOOSE:
        if (hJoyPressed & PAD_UP)   { gSaveChoice = 0; draw_save_yesno(); return; }
        if (hJoyPressed & PAD_DOWN) { gSaveChoice = 1; draw_save_yesno(); return; }
        if (hJoyPressed & PAD_A) {
            Audio_PlaySFX_PressAB();
            restore_yn_tiles();
            if (gSaveChoice == 0) {
                Save_Write();
                draw_save_msg(RomText("NowSavingString"), NULL);
                gSaveDelay = 120;
                gSaveState = SAVE_NOW_SAVING;
            } else {
                gSaveState = SAVE_NONE;
                menu_redraw_over_field();
            }
        } else if (hJoyPressed & PAD_B) {
            Audio_PlaySFX_PressAB();
            restore_yn_tiles();
            gSaveState = SAVE_NONE;
            menu_redraw_over_field();
        }
        return;
    case SAVE_NOW_SAVING:
        if (--gSaveDelay > 0) return;
        draw_saved_game_msg();
        Audio_PlaySFX_Save();
        gSaveDelay = 30;
        gSaveState = SAVE_SAVED;
        return;
    case SAVE_SAVED:
        if (Audio_IsSFXPlaying()) return;
        if (--gSaveDelay > 0) return;
        gSaveState = SAVE_NONE;
        menu_close();
        return;
    default:
        return;
    }
}

static const int kOptRowY[4]      = {3, 8, 13, 16};
static const int kTextSpeedX[3]   = {1, 7, 14};
static const int kAnimX[2]        = {1, 10};
static const int kStyleX[2]       = {1, 10};

static int opt_cursor_x(int row) {
    switch (row) {
    case 0: return kTextSpeedX[gOptSel[0]];
    case 1: return kAnimX[gOptSel[1]];
    case 2: return kStyleX[gOptSel[2]];
    default: return 1;
    }
}

static void draw_option_values(void) {
    menu_ascii(1, 3,  " FAST  MEDIUM SLOW");
    menu_ascii(1, 8,  " ON       OFF");
    menu_ascii(1, 13, " SHIFT    SET");
    menu_ascii(2, 16, "CANCEL");
    smset(kTextSpeedX[gOptSel[0]], 3,  (uint8_t)Font_CharToTile(0xEC));
    smset(kAnimX[gOptSel[1]],      8,  (uint8_t)Font_CharToTile(0xEC));
    smset(kStyleX[gOptSel[2]],     13, (uint8_t)Font_CharToTile(0xEC));
    smset(1, 16, (uint8_t)Font_CharToTile(0xEC));
    smset(opt_cursor_x(gOptRow), kOptRowY[gOptRow], (uint8_t)Font_CharToTile(0xED));
}

#define OPT_BIT_BATTLE_STYLE  (1u << 6)
#define OPT_BIT_BATTLE_ANIM   (1u << 7)
static const int kTextDelayVal[3] = {1, 3, 5};

static void menu_options_read(void) {
    int spd = wOptions & 0x07;
    gOptSel[0] = (spd == 5) ? 2 : (spd == 1) ? 0 : 1;
    gOptSel[1] = (wOptions & OPT_BIT_BATTLE_ANIM)  ? 1 : 0;
    gOptSel[2] = (wOptions & OPT_BIT_BATTLE_STYLE) ? 1 : 0;
    gTextLetterDelay = kTextDelayVal[gOptSel[0]];
}

static void menu_options_apply(void) {
    int delay = kTextDelayVal[gOptSel[0]];
    wOptions = (uint8_t)((wOptions & ~0x07) | delay);
    if (gOptSel[1]) wOptions |= OPT_BIT_BATTLE_ANIM;  else wOptions &= (uint8_t)~OPT_BIT_BATTLE_ANIM;
    if (gOptSel[2]) wOptions |= OPT_BIT_BATTLE_STYLE; else wOptions &= (uint8_t)~OPT_BIT_BATTLE_STYLE;
    gTextLetterDelay = delay;
}

#define G2OPT_ROWS      8
#define G2OPT_VAL_COL   11
#define G2OPT_FRAME_ROW 6
#define G2OPT_CANCEL    7

static int gG2Opt[G2OPT_ROWS - 1];
static int gG2OptRow = 0;

static int g2opt_name_row(int row) { return CRYSTAL_OPT_TEXT_ROW + row * 2; }

static int g2opt_count(int row) {
    switch (row) {
    case 0: return CRYSTAL_OPT_TEXTSPEED_COUNT;
    case 1: return CRYSTAL_OPT_BATTLESCENE_COUNT;
    case 2: return CRYSTAL_OPT_BATTLESTYLE_COUNT;
    case 3: return CRYSTAL_OPT_SOUND_COUNT;
    case 4: return CRYSTAL_OPT_PRINT_COUNT;
    case 5: return CRYSTAL_OPT_MENUACCOUNT_COUNT;

    case 6: return 8 < CRYSTAL_NUM_FRAMES ? 8 : CRYSTAL_NUM_FRAMES;
    default: return 0;
    }
}

static const char *g2opt_value(int row, int i) {
    switch (row) {
    case 0: return gCrystalOptTextSpeed[i];
    case 1: return gCrystalOptBattleScene[i];
    case 2: return gCrystalOptBattleStyle[i];
    case 3: return gCrystalOptSound[i];
    case 4: return gCrystalOptPrint[i];
    case 5: return gCrystalOptMenuAccount[i];
    default: return 0;
    }
}

static void g2opt_read(void) {
    int spd = wOptions & 0x07;
    gG2Opt[0] = (spd == 5) ? 2 : (spd == 1) ? 0 : 1;
    gG2Opt[1] = (wOptions & OPT_BIT_BATTLE_ANIM)  ? 1 : 0;
    gG2Opt[2] = (wOptions & OPT_BIT_BATTLE_STYLE) ? 1 : 0;
    gG2Opt[3] = JohtoAudio_GetStereo() ? 1 : 0;
    gG2Opt[4] = 2;
    gG2Opt[5] = Menu_Gen2Account() ? 1 : 0;
    gG2Opt[6] = Font_GetGen2Frame();
}

static void g2opt_apply(void) {
    int delay = kTextDelayVal[gG2Opt[0]];
    wOptions = (uint8_t)((wOptions & ~0x07) | delay);
    if (gG2Opt[1]) wOptions |= OPT_BIT_BATTLE_ANIM;
    else            wOptions &= (uint8_t)~OPT_BIT_BATTLE_ANIM;
    if (gG2Opt[2]) wOptions |= OPT_BIT_BATTLE_STYLE;
    else            wOptions &= (uint8_t)~OPT_BIT_BATTLE_STYLE;
    gTextLetterDelay = delay;
    JohtoAudio_SetStereo(gG2Opt[3]);
    Menu_SetGen2Account(gG2Opt[5]);
    Font_SetGen2Frame(gG2Opt[6]);

}

static void g2opt_draw_values(void) {
    for (int r = 0; r < G2OPT_ROWS - 1; r++) {
        int vrow = g2opt_name_row(r) + 1;
        const char *v = g2opt_value(r, gG2Opt[r]);
        if (v) {
            menu_ascii(G2OPT_VAL_COL, vrow, v);
        } else {

            char n[2];
            n[0] = (char)('1' + gG2Opt[G2OPT_FRAME_ROW]);
            n[1] = 0;
            menu_ascii(G2OPT_VAL_COL + 4, vrow, n);
        }
    }
    for (int r = 0; r < G2OPT_ROWS; r++)
        smset(1, g2opt_name_row(r), (uint8_t)Font_CharToTile(CHAR_SPACE));
    smset(1, g2opt_name_row(gG2OptRow), (uint8_t)Font_CharToTile(CHAR_ARROW));
}

static void g2opt_start(void) {
    for (int i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
    for (int r = 0; r < SCREEN_HEIGHT; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            smset(c, r, (uint8_t)Font_CharToTile(CHAR_SPACE));
    g2opt_read();
    gG2OptRow = 0;

    draw_textbox(0, 0, CRYSTAL_OPT_BOX_COLS + 1, CRYSTAL_OPT_BOX_ROWS + 1);
    {
        const char *s = gCrystalOptionsScreen;
        int row = CRYSTAL_OPT_TEXT_ROW;
        while (*s) {
            const char *nl = strchr(s, '\n');
            int len = nl ? (int)(nl - s) : (int)strlen(s);
            for (int i = 0; i < len; i++)
                smset(CRYSTAL_OPT_TEXT_COL + i, row,
                      (uint8_t)menu_ascii_tile((unsigned char)s[i]));
            row++;
            if (!nl) break;
            s = nl + 1;
        }
    }
    g2opt_draw_values();
    gOptState = 1;
}

static void g2opt_tick(void) {
    if (hJoyPressed & PAD_DOWN) {
        gG2OptRow = (gG2OptRow + 1) % G2OPT_ROWS;
        g2opt_draw_values();
        return;
    }
    if (hJoyPressed & PAD_UP) {
        gG2OptRow = (gG2OptRow + G2OPT_ROWS - 1) % G2OPT_ROWS;
        g2opt_draw_values();
        return;
    }
    if (hJoyPressed & (PAD_LEFT | PAD_RIGHT)) {
        int n = g2opt_count(gG2OptRow);
        if (n > 0) {
            int d = (hJoyPressed & PAD_RIGHT) ? 1 : -1;
            gG2Opt[gG2OptRow] = (gG2Opt[gG2OptRow] + d + n) % n;
            g2opt_apply();
            g2opt_draw_values();
        }
        return;
    }
    if ((hJoyPressed & PAD_A) && gG2OptRow == G2OPT_CANCEL) {
        Audio_PlaySFX_PressAB();
        opt_close();
        return;
    }
    if (hJoyPressed & (PAD_B | PAD_START)) {
        Audio_PlaySFX_PressAB();
        opt_close();
    }
}

static void menu_start_options(void) {
    if (menu_gen2()) { g2opt_start(); return; }

    for (int i = 0; i < MAX_SPRITES; i++) wShadowOAM[i].y = 0;
    menu_options_read();

    for (int r = 0; r < SCREEN_HEIGHT; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            smset(c, r, (uint8_t)Font_CharToTile(CHAR_SPACE));
    gOptRow = 0;
    draw_textbox(0, 0, 19, 4);   menu_ascii(1, 1,  "TEXT SPEED");
    draw_textbox(0, 5, 19, 9);   menu_ascii(1, 6,  "BATTLE ANIMATION");
    draw_textbox(0, 10, 19, 14); menu_ascii(1, 11, "BATTLE STYLE");
    draw_option_values();
    gOptState = 1;
}

static void menu_tick_options(void) {
    if (menu_gen2()) { g2opt_tick(); return; }
    if (hJoyPressed & PAD_DOWN) { gOptRow = (gOptRow + 1) & 3; draw_option_values(); return; }
    if (hJoyPressed & PAD_UP)   { gOptRow = (gOptRow + 3) & 3; draw_option_values(); return; }
    if (hJoyPressed & (PAD_LEFT | PAD_RIGHT)) {
        int right = (hJoyPressed & PAD_RIGHT) != 0;
        if (gOptRow == 0) {
            gOptSel[0] += right ? 1 : -1;
            if (gOptSel[0] < 0) gOptSel[0] = 0;
            if (gOptSel[0] > 2) gOptSel[0] = 2;
        } else if (gOptRow == 1) {
            gOptSel[1] = right ? 1 : 0;
        } else if (gOptRow == 2) {
            gOptSel[2] = right ? 1 : 0;
        }
        menu_options_apply();
        draw_option_values();
        return;
    }
    if (hJoyPressed & PAD_A) {
        if (gOptRow == 3) {
            Audio_PlaySFX_PressAB();
            opt_close();
        }
        return;
    }
    if (hJoyPressed & (PAD_B | PAD_START)) {
        Audio_PlaySFX_PressAB();
        opt_close();
        return;
    }
}

void Menu_OpenOptionsStandalone(void) {
    gOptStandalone = 1;
    menu_start_options();
}

int Menu_TickOptionsStandalone(void) {
    if (gOptState) menu_tick_options();
    if (!gOptState) { gOptStandalone = 0; return 1; }
    return 0;
}

int Menu_SaveRowIndex(void) { return gMenuHasDex ? 4 : 3; }
int Menu_CursorRow(void)    { return gMenuCursor; }

int Menu_SaveFlowState(void) { return gSaveState != SAVE_NONE; }

int Menu_SaveAwaitingYesNo(void) { return gSaveState == SAVE_CHOOSE; }

void Menu_Tick(void) {
    BagMenu_PalTrace("menu_tick");

    if (gOptState) {
        menu_tick_options();
        return;
    }

    if (gSaveState != SAVE_NONE) {
        menu_tick_save();
        return;
    }

    if (gMenuOpenDelay > 0) {
        if (--gMenuOpenDelay == 0) {
            draw_items();
            draw_safari_box();
            set_cursor(0, (uint8_t)Font_CharToTile(CHAR_ARROW));
        }
        return;
    }

    if (hJoyPressed & PAD_UP) {
        set_cursor(gMenuCursor, (uint8_t)Font_CharToTile(CHAR_SPACE));
        gMenuCursor = (gMenuCursor == 0) ? gMenuNumItems - 1 : gMenuCursor - 1;
        set_cursor(gMenuCursor, (uint8_t)Font_CharToTile(CHAR_ARROW));
        return;
    }
    if (hJoyPressed & PAD_DOWN) {
        set_cursor(gMenuCursor, (uint8_t)Font_CharToTile(CHAR_SPACE));
        gMenuCursor = (gMenuCursor == gMenuNumItems - 1) ? 0 : gMenuCursor + 1;
        set_cursor(gMenuCursor, (uint8_t)Font_CharToTile(CHAR_ARROW));
        return;
    }

    if (hJoyPressed & (PAD_B | PAD_START)) {
        Audio_PlaySFX_PressAB();
        menu_close();
        return;
    }

    if (hJoyPressed & PAD_A) {
        Audio_PlaySFX_PressAB();
        int idx = gMenuCursor;

        if (gMenuHasDex) {
            if (idx == 0) {
                menu_close();
                Pokedex_Open();
                return;
            }
            idx--;
        }
        switch (idx) {
            case 0:
                menu_close();
                NPC_HideOAM();
                PartyMenu_Open(0 );
                return;
            case 1:
                gMenuOpen = 0;
                BagMenu_Open();
                return;
            case 2:
                gMenuOpen = 0;
                TrainerCard_Open();
                return;
            case 4:
                menu_start_options();
                return;
            case 3:
                menu_start_save();
                return;
            case 5:
                menu_close();
                break;
        }
    }
}
