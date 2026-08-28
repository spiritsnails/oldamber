
#include "party_menu.h"
#include "battle/battle_ui.h"
#include "crystal_icon_anim.h"
#include "crystal_fade.h"
#include "data/gbc_palettes.h"
#include "crystal_icons.h"
#include "gbc_color.h"
#include "gen2_species.h"
#include "summary_screen.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../platform/audio.h"
#include "../data/font_data.h"
#include "../data/base_stats.h"
#include "../data/party_icon_data.h"
#include "../data/moves_data.h"
#include "constants.h"
#include "overworld.h"
#include "field_moves.h"
#include "escape_anim.h"
#include "town_map.h"
#include "pokemon.h"
#include "text.h"
#include "rom_text.h"
#include "tmhm.h"
#include "battle/battle_exp.h"
#include <stdio.h>
#include <string.h>
#include "gen2_species.h"

extern uint8_t     wPartyCount;
extern party_mon_t wPartyMons[PARTY_LENGTH];
extern uint8_t     wPartyMonNicks[PARTY_LENGTH][NAME_LENGTH];
extern uint8_t     wPartySpecies[PARTY_LENGTH + 1];
extern uint8_t     wWhichPokemon;

static int g_open        = 0;

static uint8_t g_evo_stone;

static uint8_t g_evo_stone_pending;

static int g_force       = 0;
static int g_cursor      = 0;

static int g_saved_cursor = 0;

#define PM_INPUT_LOCKOUT_FRAMES 30
static int g_input_lockout = 0;
static int g_selected    = -1;
static int g_anim_tick   = 0;
static int g_anim_frame  = 0;
static int g_submenu     = 0;
static int g_sub_cursor  = 0;
static int g_in_summary  = 0;
static int g_switching   = 0;
static int g_switch_from = -1;

static int g_swap_anim   = 0;
static int g_swap_timer  = 0;

#define SWAP_HOLD_FRAMES 14

#define SWAP_GAP_FRAMES 6
static int g_resulting   = 0;
static int g_result_timer = 0;

static int      g_hp_anim = 0;
static int      g_hp_anim_slot = 0;
static uint16_t g_hp_anim_old  = 0;
static uint16_t g_hp_anim_new  = 0;
static uint16_t g_hp_anim_max  = 0;
static uint16_t g_hp_anim_healed = 0;
static int      g_hp_anim_t = 0;
static int      g_hp_anim_T = 1;

static int      g_heal_msg_custom = 0;
static char     g_heal_msg1[24];
static char     g_heal_msg2[24];
static int g_result_keep_sprites = 0;

static int         g_need_move     = 0;
static const char *g_move_prompt1  = NULL;
static const char *g_move_prompt2  = NULL;
static int         g_move_select   = 0;
static int         g_move_cursor   = 0;
static int         g_move_count    = 0;
static int         g_move_slot[4];
static pm_moveuse_fn g_move_apply   = NULL;
static int         g_move_result   = 0;
static int         g_move_result_disp = 0;

#define PM_OPEN_FADE_FRAMES 14
static int g_open_fade = 0;

static int g_close_fade = 0;
#define PM_CLOSE_FADE_FRAMES 14

#define PM_PAL_NORMAL()  Display_SetPalette(0xE4, 0xD0, 0xE0)
#define PM_PAL_WHITE()   Display_SetPalette(0x00, 0x00, 0x00)
static int g_pending_close_after_text = 0;
static int g_pending_close_kind = 0;
static int g_strength_whiteout = 0;
static int g_strength_fade_step = 0;
static int g_strength_fade_timer = 0;

typedef enum {
    PM_FIELD_ACTION_NONE = 0,
    PM_FIELD_ACTION_CUT,
    PM_FIELD_ACTION_SURF,
    PM_FIELD_ACTION_FLY,
    PM_FIELD_ACTION_FLASH,
    PM_FIELD_ACTION_STRENGTH,
    PM_FIELD_ACTION_DIG,
    PM_FIELD_ACTION_TELEPORT,
} pm_field_action_t;

typedef struct {
    uint8_t           move_id;
    pm_field_action_t action;
    const char       *label;
    uint8_t           leftmost;
} pm_field_move_t;

static pm_field_move_t g_field_moves[4];
static int             g_field_move_count = 0;

#define PM_FIELD_LEFTMOST_DEFAULT 12
static int g_field_leftmost = PM_FIELD_LEFTMOST_DEFAULT;

static int pm_party_mon_knows_move(int slot, uint8_t move_id) {
    if (slot < 0 || slot >= (int)wPartyCount) return 0;
    for (int i = 0; i < 4; i++) {
        if (wPartyMons[slot].base.moves[i] == move_id) return 1;
    }
    return 0;
}

#define PM_OAM_BASE  0

static uint8_t pm_icon_type(int slot) {
    if (slot >= (int)wPartyCount) return ICON_MON;
    uint8_t species = wPartyMons[slot].base.species;
    uint8_t dex = gSpeciesToDex[species];
    if (dex == 0 || dex > 151) return ICON_MON;
    return gMonPartyIconType[dex];
}

static int pm_gen2(void);
static int pm_hp_pixels(uint16_t hp, uint16_t max_hp);

static void pm_draw_slot_hp(int slot, uint16_t hp, uint16_t max_hp);

#define PM2_NAME_COL   3
#define PM2_HPNUM_COL 13
#define PM2_STATUS_COL 5
#define PM2_LEVEL_COL  8
#define PM2_HPBAR_COL 11
#define PM2_CANCEL_COL 1
#define PM2_TOP_ROW    1

#define PM2_ICON_TILE_BASE 68
#define PM2_ICON_TILE(slot) (PM2_ICON_TILE_BASE + (slot) * CRYSTAL_ICON_TILES)

static void pm_load_icons_gen2(void) {
    for (int s = 0; s < PARTY_LENGTH; s++) {
        int icon = 0;
        if (s < (int)wPartyCount)
            icon = CrystalIcon_ForDex(Species_Dex(wPartyMons[s].base.species));
        for (int t = 0; t < CRYSTAL_ICON_TILES; t++)
            Display_LoadSpriteTile((uint8_t)(PM2_ICON_TILE(s) + t),
                                   gCrystalIcon[icon][t]);
    }
}

static void pm_write_slot_oam_gen2(int slot) {
    int base_oam = PM_OAM_BASE + slot * 4;
    int dx = 0, dy = 0;
    if (slot >= (int)wPartyCount) {
        for (int i = 0; i < 4; i++) {
            wShadowOAM[base_oam + i].y = 0;
            wShadowOAM[base_oam + i].x = 0;
            wShadowOAM[base_oam + i].tile = 0;
            wShadowOAM[base_oam + i].flags = 0;
        }
        return;
    }
    CrystalIconAnim_Offset(slot, &dx, &dy);
    {

        uint8_t oam_y = (uint8_t)(slot * 16 + 0x1C - 8 + dy);
        uint8_t oam_x = (uint8_t)(8 * 2 - 8 + dx);
        int tb = PM2_ICON_TILE(slot) + CrystalIconAnim_Frame(slot) * 4;
        wShadowOAM[base_oam+0] = (oam_entry_t){ oam_y,   oam_x,   (uint8_t)(tb+0), 0 };
        wShadowOAM[base_oam+1] = (oam_entry_t){ oam_y,   (uint8_t)(oam_x+8), (uint8_t)(tb+1), 0 };
        wShadowOAM[base_oam+2] = (oam_entry_t){ (uint8_t)(oam_y+8), oam_x, (uint8_t)(tb+2), 0 };
        wShadowOAM[base_oam+3] = (oam_entry_t){ (uint8_t)(oam_y+8), (uint8_t)(oam_x+8), (uint8_t)(tb+3), 0 };
    }
}

static void pm_write_slot_oam(int slot, int frame) {
    int base_oam = PM_OAM_BASE + slot * 4;
    uint8_t icon = pm_icon_type(slot);
    uint8_t tile_base = (uint8_t)(icon << 2);
    if (frame) tile_base = (uint8_t)(tile_base + ICON_ICONOFFSET);

    uint8_t oam_y = (uint8_t)(slot * 16 + OAM_Y_OFS);
    uint8_t oam_x = (uint8_t)(1 * 8 + OAM_X_OFS);

    if (slot >= (int)wPartyCount) {

        for (int i = 0; i < 4; i++) {
            wShadowOAM[base_oam + i].y = 0;
            wShadowOAM[base_oam + i].x = 0;
            wShadowOAM[base_oam + i].tile = 0;
            wShadowOAM[base_oam + i].flags = 0;
        }
        return;
    }

    if (icon == ICON_HELIX) {

        wShadowOAM[base_oam+0] = (oam_entry_t){ oam_y,   oam_x,   tile_base+0, 0 };
        wShadowOAM[base_oam+1] = (oam_entry_t){ oam_y,   oam_x+8, tile_base+1, 0 };
        wShadowOAM[base_oam+2] = (oam_entry_t){ oam_y+8, oam_x,   tile_base+2, 0 };
        wShadowOAM[base_oam+3] = (oam_entry_t){ oam_y+8, oam_x+8, tile_base+3, 0 };
    } else {

        wShadowOAM[base_oam+0] = (oam_entry_t){ oam_y,   oam_x,   tile_base+0, 0 };
        wShadowOAM[base_oam+1] = (oam_entry_t){ oam_y,   oam_x+8, tile_base+0, OAM_FLAG_FLIP_X };
        wShadowOAM[base_oam+2] = (oam_entry_t){ oam_y+8, oam_x,   tile_base+2, 0 };
        wShadowOAM[base_oam+3] = (oam_entry_t){ oam_y+8, oam_x+8, tile_base+2, OAM_FLAG_FLIP_X };
    }
}

static int pm_hp_band(uint16_t hp, uint16_t max_hp) {
    int px = pm_hp_pixels(hp, max_hp);
    if (px > 24) return CRYSTAL_HP_GREEN;
    if (px > 9)  return CRYSTAL_HP_YELLOW;
    return CRYSTAL_HP_RED;
}

static void pm_write_all_oam(int frame) {
    if (pm_gen2()) {

        CrystalIconAnim_SetSelected(g_cursor);
        for (int i = 0; i < PARTY_LENGTH; i++)
            pm_write_slot_oam_gen2(i);
        (void)frame;
        return;
    }
    for (int i = 0; i < PARTY_LENGTH; i++)
        pm_write_slot_oam(i, (i == g_cursor) ? frame : 0);
}

static int g_pm2_pals_applied = 0;

static uint8_t s_pm_saved_attr[SCREEN_HEIGHT][SCREEN_WIDTH];
static int     s_pm_saved_posattr = 0;
static int     s_pm_attr_saved    = 0;

static void pm_save_attr_state(void) {
    if (s_pm_attr_saved) return;
    for (int r = 0; r < SCREEN_HEIGHT; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            s_pm_saved_attr[r][c] = Display_GetPositionAttr(c, r);
    s_pm_saved_posattr = Display_GetPositionAttrMode();
    s_pm_attr_saved = 1;
}

static void pm_restore_attr_state(void) {
    if (!s_pm_attr_saved) return;
    s_pm_attr_saved = 0;
    for (int r = 0; r < SCREEN_HEIGHT; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            Display_FillAttrBox(c, r, 1, 1, s_pm_saved_attr[r][c]);
    Display_SetPositionAttrMode(s_pm_saved_posattr);
}

static int g_pm1_attrs_applied = 0;

#define PAL_SGB_GREENBAR  0x1F
#define PAL_SGB_MEWMON    0x10
#define PAL_SGB_YELLOWBAR 0x20
#define PAL_SGB_REDBAR    0x21

static void pm1_apply_hp_pals(void) {
    if (!GbcColor_IsEnabled()) return;

    if (GbcColor_BattleAutoColor()) {
        GbcColor_ApplyAutoColorAll();

        GbcColor_ApplyOverworldSpritePal(0);
        return;
    }

    g_pm1_attrs_applied = 1;

    Display_SetBGColorPalette(0, GbcColor_SuperPalette(PAL_SGB_MEWMON));
    Display_SetBGColorPalette(1, GbcColor_SuperPalette(PAL_SGB_GREENBAR));
    Display_SetBGColorPalette(2, GbcColor_SuperPalette(PAL_SGB_YELLOWBAR));
    Display_SetBGColorPalette(3, GbcColor_SuperPalette(PAL_SGB_REDBAR));

    pm_save_attr_state();
    Display_SetPositionAttrMode(1);
    Display_ClearAttrBoxes(0);
    Display_FillAttrBox(1, 0, 2, 13, 0);
    for (int i = 0; i < (int)wPartyCount && i < PARTY_LENGTH; i++) {

        int band = pm_hp_band(wPartyMons[i].base.hp, wPartyMons[i].max_hp);
        Display_FillAttrBox(5, 1 + i * 2, 7, 1, (uint8_t)(band + 1));
    }

    {
        const uint16_t *icon = GbcColor_SuperPalette(PAL_SGB_MEWMON);
        uint8_t obp0 = Display_GetOBP0();
        uint16_t obj[4];
        if (obp0 == 0x00) obp0 = 0xD0;
        for (int i = 0; i < 4; i++) obj[i] = icon[(obp0 >> (2 * i)) & 3];
        for (int i = 0; i < GBC_NUM_SPRITE_PALETTES; i++)
            Display_SetOBJColorPalette(i, obj);
    }
    Display_SetColorMode(1);
}

static void pm1_release_text_attrs(void) {
    if (!g_pm1_attrs_applied) return;
    g_pm1_attrs_applied = 0;

    pm_restore_attr_state();
    GbcColor_MarkDirty();
}

static void pm2_release_pals(void) {
    if (!g_pm2_pals_applied) return;
    g_pm2_pals_applied = 0;
    pm_restore_attr_state();
    GbcColor_MarkDirty();
}

static void pm_clear_icon_oam(void) {
    pm2_release_pals();
    pm1_release_text_attrs();

    if (wIsInBattle) BattleUI_EnemySpriteSetVisible(1);
    for (int i = PM_OAM_BASE; i < PM_OAM_BASE + PARTY_LENGTH * 4; i++) {
        wShadowOAM[i].y = 0;
        wShadowOAM[i].x = 0;
        wShadowOAM[i].tile = 0;
        wShadowOAM[i].flags = 0;
    }
}

static void pm_put(int col, int row, int tile_idx);

static void pm_clear_mon_row(int slot) {
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < 20; c++)
            pm_put(c, slot * 2 + r, BLANK_TILE_SLOT);
    int base = PM_OAM_BASE + slot * 4;
    for (int i = 0; i < 4; i++)
        wShadowOAM[base + i].y = 0;
}

static void pm_put(int col, int row, int tile_idx) {
    if ((unsigned)col >= SCREEN_WIDTH || (unsigned)row >= SCREEN_HEIGHT) return;
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = (uint8_t)tile_idx;
}

static void pm_gb(int col, int row, uint8_t gb_char) {
    pm_put(col, row, Font_CharToTile(gb_char));
}

static int pm_ascii_tile(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return Font_CharToTile(0x80 + (c - 'A'));
    if (c >= 'a' && c <= 'z') return Font_CharToTile(0xA0 + (c - 'a'));
    if (c >= '0' && c <= '9') return Font_CharToTile(0xF6 + (c - '0'));
    if (c == ' ')              return BLANK_TILE_SLOT;
    if (c == '.')              return Font_CharToTile(0xE8);
    if (c == '!')              return Font_CharToTile(0xE7);
    if (c == '?')              return Font_CharToTile(0xE6);
    if (c == ',')              return Font_CharToTile(0xF4);
    if (c == '-')              return Font_CharToTile(0xE3);
    if (c == '\'')             return Font_CharToTile(0xE0);
    if (c == '/')              return Font_CharToTile(0xF3);
    if (c == '>')              return Font_CharToTile(0xED);
    return BLANK_TILE_SLOT;
}

static void pm_ascii(int col, int row, const char *s) {
    for (; *s; s++, col++)
        pm_put(col, row, pm_ascii_tile((unsigned char)*s));
}

static void pm_num3(int col, int row, int val) {
    char buf[4];
    if (val < 0)   val = 0;
    if (val > 999) val = 999;
    snprintf(buf, sizeof(buf), "%3d", val);
    for (int i = 0; i < 3; i++) {
        char c = buf[i];
        pm_put(col + i, row,
               c == ' ' ? BLANK_TILE_SLOT
                        : Font_CharToTile(0xF6 + (c - '0')));
    }
}

static int pm_hp_pixels(uint16_t hp, uint16_t max_hp) {
    if (!max_hp || !hp) return 0;
    int px = (int)hp * 48 / (int)max_hp;
    return px < 1 ? 1 : (px > 48 ? 48 : px);
}

static int pm_hp_bar_color(int slot) {
    if (slot < 0 || slot >= (int)wPartyCount) return 0;
    int px = pm_hp_pixels(wPartyMons[slot].base.hp, wPartyMons[slot].max_hp);
    if (px >= 27) return 0;
    if (px >= 10) return 1;
    return 2;
}

static int pm_anim_rate(void) {
    static const int kPartyMonSpeeds[3] = {5, 16, 32};
    return kPartyMonSpeeds[pm_hp_bar_color(g_cursor)] + 1;
}

static void pm_draw_bar_tiles(int col, int row, int pixels) {
    for (int i = 0; i < 6; i++) {
        int     seg  = pixels - i * 8;
        uint8_t tile = (seg <= 0) ? 0x63u
                     : (seg >= 8) ? 0x6Bu
                     :              (uint8_t)(0x63u + seg);
        pm_gb(col + i, row, tile);
    }
}

static void pm_draw_hp_bar(int hp_row, uint16_t hp, uint16_t max_hp) {
    pm_gb(4,  hp_row, 0x71);
    pm_gb(5,  hp_row, 0x62);
    pm_draw_bar_tiles(6, hp_row, pm_hp_pixels(hp, max_hp));
    pm_gb(12, hp_row, 0x6C);

    pm_num3(13, hp_row, (int)hp);
    pm_gb(16, hp_row, 0xF3);
    pm_num3(17, hp_row, (int)max_hp);
}

static const uint8_t k_fnt[3] = {0x85, 0x8D, 0x93};
static const uint8_t k_slp[3] = {0x92, 0x8B, 0x8F};
static const uint8_t k_psn[3] = {0x8F, 0x92, 0x8D};
static const uint8_t k_brn[3] = {0x81, 0x91, 0x8D};
static const uint8_t k_frz[3] = {0x85, 0x91, 0x99};
static const uint8_t k_par[3] = {0x8F, 0x80, 0x91};

static void pm_draw_status(int col, int row, uint8_t status, uint16_t hp) {
    const uint8_t *s = NULL;
    if (!hp)               s = k_fnt;
    else if (status & 0x07) s = k_slp;
    else if (status & (1<<3)) s = k_psn;
    else if (status & (1<<4)) s = k_brn;
    else if (status & (1<<5)) s = k_frz;
    else if (status & (1<<6)) s = k_par;

    for (int i = 0; i < 3; i++) {
        if (s) pm_gb(col + i, row, s[i]);
        else   pm_put(col + i, row, BLANK_TILE_SLOT);
    }
}

static void pm_draw_level(int col, int row, uint8_t level) {
    if (level >= 100) {
        pm_put(col,   row, Font_CharToTile(0xF6 + (level / 100)));
        pm_put(col+1, row, Font_CharToTile(0xF6 + ((level / 10) % 10)));
        pm_put(col+2, row, Font_CharToTile(0xF6 + (level % 10)));
    } else if (level >= 10) {
        pm_gb(col,    row, 0x6E);
        pm_put(col+1, row, Font_CharToTile(0xF6 + (level / 10)));
        pm_put(col+2, row, Font_CharToTile(0xF6 + (level % 10)));
    } else {
        pm_gb(col,    row, 0x6E);
        pm_put(col+1, row, Font_CharToTile(0xF6 + level));
        pm_put(col+2, row, BLANK_TILE_SLOT);
    }
}

static void pm_draw_nick(int col, int row, int slot) {
    const uint8_t *nick = wPartyMonNicks[slot];
    if (nick[0] != 0x00) {

        for (int i = 0; i < 10; i++) {
            uint8_t ch = nick[i];
            if (ch == 0x50) {
                for (int j = i; j < 10; j++) pm_put(col + j, row, BLANK_TILE_SLOT);
                return;
            }
            pm_put(col + i, row, Font_CharToTile(ch));
        }
    } else {

        uint8_t     dex  = gSpeciesToDex[wPartyMons[slot].base.species];
        const char *name = Pokemon_GetName(dex);
        int         len  = (int)strlen(name);
        for (int i = 0; i < 10; i++) {
            if (i < len) pm_put(col + i, row, pm_ascii_tile((unsigned char)name[i]));
            else         pm_put(col + i, row, BLANK_TILE_SLOT);
        }
    }
}

static void pm_draw_msg_box_gen2(const char *line1) {
    pm_gb(0,              14, 0x79);
    pm_gb(SCREEN_WIDTH-1, 14, 0x7B);
    pm_gb(0,              17, 0x7D);
    pm_gb(SCREEN_WIDTH-1, 17, 0x7E);
    for (int c = 1; c < SCREEN_WIDTH-1; c++) {
        pm_gb(c, 14, 0x7A);
        pm_gb(c, 17, 0x7A);
    }
    for (int r = 15; r <= 16; r++) {
        pm_gb(0,              r, 0x7C);
        pm_gb(SCREEN_WIDTH-1, r, 0x7C);
        for (int c = 1; c < SCREEN_WIDTH-1; c++)
            pm_put(c, r, BLANK_TILE_SLOT);
    }
    if (line1) pm_ascii(1, 16, line1);
}

static void pm_draw_msg_box(const char *line1, const char *line2) {
    pm_gb(0,              12, 0x79);
    pm_gb(SCREEN_WIDTH-1, 12, 0x7B);
    pm_gb(0,              17, 0x7D);
    pm_gb(SCREEN_WIDTH-1, 17, 0x7E);
    for (int c = 1; c < SCREEN_WIDTH-1; c++) {
        pm_gb(c, 12, 0x7A);
        pm_gb(c, 17, 0x7A);
    }
    for (int r = 13; r <= 16; r++) {
        pm_gb(0,              r, 0x7C);
        pm_gb(SCREEN_WIDTH-1, r, 0x7C);
        for (int c = 1; c < SCREEN_WIDTH-1; c++)
            pm_put(c, r, BLANK_TILE_SLOT);
    }

    if (line1) pm_ascii(1, 14, line1);
    if (line2) pm_ascii(1, 16, line2);
}

static void pm_draw_all(void);

static void pm_draw_move_box(int cursor) {
    const int L = 4, R = 19;
    pm_gb(L, 7, 0x79); pm_gb(R, 7, 0x7B);
    pm_gb(L, 12, 0x7D); pm_gb(R, 12, 0x7E);
    for (int c = L + 1; c < R; c++) { pm_gb(c, 7, 0x7A); pm_gb(c, 12, 0x7A); }
    for (int r = 8; r <= 11; r++) {
        pm_gb(L, r, 0x7C); pm_gb(R, r, 0x7C);
        for (int c = L + 1; c < R; c++) pm_put(c, r, BLANK_TILE_SLOT);
    }

    for (int i = 0; i < 4; i++) {
        int row = 8 + i;
        uint8_t move_id = wPartyMons[g_cursor].base.moves[i];
        if (move_id > 0 && move_id < NUM_MOVE_DEFS) {
            const char *name = gMoveNames[move_id];
            int len = (int)strlen(name); if (len > 12) len = 12;
            for (int j = 0; j < 12; j++)
                pm_put(6 + j, row, (j < len) ? pm_ascii_tile((unsigned char)name[j])
                                             : BLANK_TILE_SLOT);
        } else {
            pm_gb(6, row, 0xE3);
        }
    }

    for (int r = 8; r <= 11; r++) pm_put(5, r, BLANK_TILE_SLOT);
    if (cursor >= 0 && cursor < g_move_count)
        pm_put(5, 8 + g_move_slot[cursor], Font_CharToTile(0xED));
}

static void pm_draw_move_select(int cursor) {
    pm_draw_msg_box(g_move_prompt1, g_move_prompt2);
    pm_draw_move_box(cursor);
}

static void pm_enter_move_select(int slot) {
    g_move_count = 0;
    for (int i = 0; i < 4; i++) {
        if (wPartyMons[slot].base.moves[i] == 0) continue;
        g_move_slot[g_move_count++] = i;
    }
    if (g_move_count == 0) g_move_count = 1, g_move_slot[0] = 0;
    g_move_cursor  = 0;
    g_move_select  = 1;
    pm_draw_move_select(g_move_cursor);
    pm_write_all_oam(g_anim_frame);
}

static int pm_collect_field_moves(int slot) {

    static const pm_field_move_t k_supported[] = {
        { MOVE_CUT,   PM_FIELD_ACTION_CUT,   "CUT",   12 },
        { MOVE_SURF,  PM_FIELD_ACTION_SURF,  "SURF",  12 },
        { MOVE_FLY,   PM_FIELD_ACTION_FLY,   "FLY",   12 },
        { MOVE_FLASH, PM_FIELD_ACTION_FLASH, "FLASH", 12 },
        { MOVE_STRENGTH, PM_FIELD_ACTION_STRENGTH, "STRENGTH", 10 },
        { MOVE_DIG,      PM_FIELD_ACTION_DIG,      "DIG",      12 },
        { MOVE_TELEPORT, PM_FIELD_ACTION_TELEPORT, "TELEPORT", 10 },
    };

    g_field_move_count = 0;
    g_field_leftmost   = PM_FIELD_LEFTMOST_DEFAULT;
    if (slot < 0 || slot >= (int)wPartyCount) return 0;

    if (pm_party_mon_knows_move(slot, MOVE_FLY)) {
        g_field_moves[g_field_move_count++] = (pm_field_move_t){
            MOVE_FLY, PM_FIELD_ACTION_FLY, "FLY", 12
        };
    }

    for (int i = 0; i < 4; i++) {
        uint8_t move_id = wPartyMons[slot].base.moves[i];
        if (!move_id) continue;
        if (move_id == MOVE_FLY) continue;
        for (int j = 0; j < (int)(sizeof(k_supported) / sizeof(k_supported[0])); j++) {
            if (k_supported[j].move_id != move_id) continue;
            if (g_field_move_count < (int)(sizeof(g_field_moves) / sizeof(g_field_moves[0]))) {
                g_field_moves[g_field_move_count++] = k_supported[j];
            }
            break;
        }
    }

    for (int i = 0; i < g_field_move_count; i++) {
        if ((int)g_field_moves[i].leftmost < g_field_leftmost)
            g_field_leftmost = (int)g_field_moves[i].leftmost;
    }

    return g_field_move_count;
}

static int pm_submenu_item_count(void) {
    return g_field_move_count + 3;
}

static void pm_draw_box_rect(int L, int top_row, int R, int bottom_row) {
    pm_gb(L, top_row, 0x79);
    for (int c = L + 1; c < R; c++) pm_gb(c, top_row, 0x7A);
    pm_gb(R, top_row, 0x7B);
    for (int r = top_row + 1; r < bottom_row; r++) {
        pm_gb(L, r, 0x7C);
        for (int c = L + 1; c < R; c++) pm_put(c, r, BLANK_TILE_SLOT);
        pm_gb(R, r, 0x7C);
    }
    pm_gb(L, bottom_row, 0x7D);
    for (int c = L + 1; c < R; c++) pm_gb(c, bottom_row, 0x7A);
    pm_gb(R, bottom_row, 0x7E);
}

static void pm_draw_submenu_box(int top_row) {
    pm_draw_box_rect(g_field_leftmost - 1, top_row, 19, 17);
}

static int pm_submenu_top_row(void) {
    int top = 11 - (g_field_move_count * 2);
    return top < 0 ? 0 : top;
}

static char g_reject_msg[48];
static int  g_reject_redraw = 0;

static const char *pm_party_mon_name(int slot);

static void pm_clear_deselect_area(void) {
    for (int c = 11; c < SCREEN_WIDTH; c++)
        pm_put(c, 11, BLANK_TILE_SLOT);
    for (int r = 12; r <= 17; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            pm_put(c, r, BLANK_TILE_SLOT);
}

static int pm_battle_switch_rejected(int slot) {
    if (!wIsInBattle) return 0;
    if (slot < 0 || slot >= (int)wPartyCount) return 0;

    if (slot == (int)wPlayerMonNumber) {

        snprintf(g_reject_msg, sizeof(g_reject_msg), "%s is\nalready out!",
                 pm_party_mon_name(slot));
    } else if (wPartyMons[slot].base.hp == 0) {

        snprintf(g_reject_msg, sizeof(g_reject_msg), "%s", RomText("_NoWillText"));
    } else {
        return 0;
    }
    pm_clear_deselect_area();
    Text_ShowASCII(g_reject_msg);
    g_reject_redraw = 1;
    return 1;
}

static const char *pm_party_mon_name(int slot) {
    static char name[NAME_LENGTH + 1];
    if (slot < 0 || slot >= (int)wPartyCount) return PortText("#MON");

    const uint8_t *nick = wPartyMonNicks[slot];
    if (nick[0] != 0x00 && nick[0] != 0x50) {
        int out = 0;
        for (int i = 0; i < NAME_LENGTH && out < NAME_LENGTH; i++) {
            uint8_t c = nick[i];
            if (c == 0x50) break;
            if      (c >= 0x80 && c <= 0x99) name[out++] = (char)('A' + (c - 0x80));
            else if (c >= 0xA0 && c <= 0xB9) name[out++] = (char)('a' + (c - 0xA0));
            else if (c >= 0xF6)              name[out++] = (char)('0' + (c - 0xF6));
            else if (c == 0x7F)              name[out++] = ' ';
            else if (c == 0xE8)              name[out++] = '.';
            else if (c == 0xE7)              name[out++] = '!';
            else if (c == 0xE6)              name[out++] = '?';
            else if (c == 0xE3)              name[out++] = '-';
            else if (c == 0xE0)              name[out++] = '\'';
        }
        name[out] = '\0';
        if (out > 0) return name;
    }
    return Pokemon_GetName(Species_Dex(wPartyMons[slot].base.species));
}

static void pm_draw_item_use_result(int slot, uint16_t healed, int success) {
    if (!success) {
        pm_draw_all();
        pm_draw_msg_box("It won't have", "any effect.");
        pm_write_all_oam(g_anim_frame);
        return;
    }

    char line1[20];
    char line2[20];
    snprintf(line1, sizeof(line1), "%s", pm_party_mon_name(slot));
    if (healed > 0)
        snprintf(line2, sizeof(line2), "recovered by %u!", (unsigned)healed);
    else
        snprintf(line2, sizeof(line2), "recovered.");

    pm_draw_all();
    pm_draw_msg_box(line1, line2);
    pm_write_all_oam(g_anim_frame);
}

static void pm_draw_submenu(int sub_cursor) {
    if (g_force == 2) {

        pm_draw_box_rect(11, 11, 19, 17);
        static const char *const kSwitchStatsCancel[3] = { "SWITCH", "STATS", "CANCEL" };
        for (int i = 0; i < 3; i++) {
            int row = 12 + i * 2;
            pm_put(12, row, sub_cursor == i ? Font_CharToTile(0xED) : BLANK_TILE_SLOT);
            pm_ascii(13, row, kSwitchStatsCancel[i]);
        }
        return;
    }

    int top_row = pm_submenu_top_row();
    pm_draw_submenu_box(top_row);

    const int cur_x = g_field_leftmost;
    const int txt_x = g_field_leftmost + 1;

    int row = top_row + 1;
    for (int i = 0; i < g_field_move_count; i++, row += 2) {
        pm_put(cur_x, row, sub_cursor == i ? Font_CharToTile(0xED) : BLANK_TILE_SLOT);
        pm_ascii(txt_x, row, g_field_moves[i].label);
    }

    int stats_idx  = g_field_move_count;
    int switch_idx = g_field_move_count + 1;
    int cancel_idx = g_field_move_count + 2;

    pm_put(cur_x, 12, sub_cursor == stats_idx ? Font_CharToTile(0xED) : BLANK_TILE_SLOT);
    pm_ascii(txt_x, 12, "STATS");
    pm_put(cur_x, 14, sub_cursor == switch_idx ? Font_CharToTile(0xED) : BLANK_TILE_SLOT);
    pm_ascii(txt_x, 14, "SWITCH");
    pm_put(cur_x, 16, sub_cursor == cancel_idx ? Font_CharToTile(0xED) : BLANK_TILE_SLOT);
    pm_ascii(txt_x, 16, "CANCEL");
}

static void pm_erase_cursors(void) {
    for (int i = 0; i < PARTY_LENGTH; i++)
        pm_put(0, i * 2 + 1, BLANK_TILE_SLOT);
}

static int pm_cursor_count(void) {
    int n = (int)wPartyCount;
    if (pm_gen2()) n += 1;
    return n < 1 ? 1 : n;
}

static void pm_draw_cursor(int cursor) {
    pm_erase_cursors();
    if (cursor < 0) return;
    if (pm_gen2()) {

        if (cursor < pm_cursor_count())
            pm_put(0, PM2_TOP_ROW + cursor * 2, Font_CharToTile(0xED));
        return;
    }
    if (cursor < (int)wPartyCount)
        pm_put(0, cursor * 2 + 1, Font_CharToTile(0xED));
}

static int pm_gen2(void) { return Font_GetStyle() == FONT_STYLE_GEN2; }

static void pm2_draw_hp_bar(int col, int row, int px) {
    int full = px / 8;
    int rem  = px % 8;
    pm_gb(col,     row, 0x60);
    pm_gb(col + 1, row, 0x61);
    for (int i = 0; i < 6; i++) {
        uint8_t ch;
        if (i < full)                ch = 0x6A;
        else if (i == full && rem)   ch = (uint8_t)(0x62 + rem);
        else                         ch = 0x62;
        pm_gb(col + 2 + i, row, ch);
    }
    pm_gb(col + 8, row, 0x6B);
}

static void pm2_apply_hp_pals(void) {
    if (!GbcColor_IsEnabled()) return;
    for (int p = 0; p < 4; p++)
        Display_SetBGColorPalette(p, gCrystalPartyMenuBGPals[p]);
    g_pm2_pals_applied = 1;
    pm_save_attr_state();
    Display_SetPositionAttrMode(1);
    Display_ClearAttrBoxes(0);
    for (int i = 0; i < (int)wPartyCount && i < PARTY_LENGTH; i++) {
        int band = pm_hp_band(wPartyMons[i].base.hp, wPartyMons[i].max_hp);
        Display_FillAttrBox(PM2_HPBAR_COL, PM2_TOP_ROW + 1 + i * 2,
                            8, 2, (uint8_t)(band + 1));
    }

    for (int i = 0; i < GBC_NUM_SPRITE_PALETTES; i++)
        Display_SetOBJColorPalette(i, gCrystalPartyMenuBGPals[0]);
    Display_SetColorMode(1);
}

static const char *pm_able_label(int slot) {
    if (g_evo_stone) {
        return Pokemon_CanEvolveWithStone((uint8_t)slot, g_evo_stone)
                   ? "ABLE" : "NOT ABLE";
    }
    if (g_force == PARTY_MENU_TMHM) {

        return TMHM_CanLearnActive(slot)
                   ? "ABLE"
                   : RomText("RedrawPartyMenu_.notAbleToLearnMoveText");
    }
    return NULL;
}

static void pm_draw_slot_gen2(int slot) {
    int top = PM2_TOP_ROW + slot * 2;
    for (int c = 0; c < SCREEN_WIDTH; c++) {
        pm_put(c, top,     BLANK_TILE_SLOT);
        pm_put(c, top + 1, BLANK_TILE_SLOT);
    }
    if (slot >= (int)wPartyCount) return;
    {
        const party_mon_t *mon = &wPartyMons[slot];
        uint16_t hp = mon->base.hp, max_hp = mon->max_hp;
        const char *able = pm_able_label(slot);
        pm_draw_nick(PM2_NAME_COL, top, slot);
        if (able) {

            pm_draw_level(PM2_LEVEL_COL, top + 1, mon->level);
            pm_ascii(PM2_HPBAR_COL, top + 1, able);
            return;
        }

        pm_num3(PM2_HPNUM_COL, top, (int)hp);
        pm_gb(PM2_HPNUM_COL + 3, top, 0xF3);
        pm_num3(PM2_HPNUM_COL + 4, top, (int)max_hp);
        pm_draw_status(PM2_STATUS_COL, top + 1, mon->base.status, hp);
        pm_draw_level(PM2_LEVEL_COL, top + 1, mon->level);
        pm2_draw_hp_bar(PM2_HPBAR_COL, top + 1, pm_hp_pixels(hp, max_hp));
    }
}

static void pm_draw_slot(int slot) {
    int name_row = slot * 2;
    int hp_row   = slot * 2 + 1;

    if (pm_gen2()) { pm_draw_slot_gen2(slot); return; }

    for (int c = 0; c < SCREEN_WIDTH; c++) {
        pm_put(c, name_row, BLANK_TILE_SLOT);
        pm_put(c, hp_row,   BLANK_TILE_SLOT);
    }
    if (slot >= (int)wPartyCount) return;

    const party_mon_t *mon    = &wPartyMons[slot];
    uint16_t           hp     = mon->base.hp;
    uint16_t           max_hp = mon->max_hp;

    pm_draw_nick(3,  name_row, slot);
    pm_draw_level(13, name_row, mon->level);

    {
        const char *able = pm_able_label(slot);
        if (able) {

            pm_ascii(3 + 9, hp_row, able);
            return;
        }
    }

    pm_draw_status(17, name_row, mon->base.status, hp);

    pm_draw_hp_bar(hp_row, hp, max_hp);
}

static void pm_draw_all(void) {

    int clear_rows = pm_gen2() ? SCREEN_HEIGHT : 12;

    memset(gScrollTileMap, (uint8_t)BLANK_TILE_SLOT,
           (size_t)SCROLL_MAP_W * SCROLL_MAP_H);

    for (int r = 0; r < clear_rows; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            pm_put(c, r, BLANK_TILE_SLOT);

    for (int i = 0; i < PARTY_LENGTH; i++)
        pm_draw_slot(i);

    if (pm_gen2()) {

        pm_ascii(PM2_CANCEL_COL, PM2_TOP_ROW + (int)wPartyCount * 2, "CANCEL");
        pm2_apply_hp_pals();
    }

    pm_draw_cursor(g_cursor);

    if (g_switching) {

        if (g_cursor != g_switch_from)
            pm_put(0, g_switch_from * 2 + 1, Font_CharToTile(0xEC));
        pm_draw_msg_box("SWITCH with", "which MON?");
    } else if (g_force == PARTY_MENU_ITEM_USE) {

        pm_draw_msg_box("Use item on which", NULL);
        pm_ascii(1, 16, "POK");
        pm_put(4, 16, Font_CharToTile(0xBA));
        pm_ascii(5, 16, "MON?");
    } else {

        if (pm_gen2()) {

            pm_draw_msg_box_gen2(NULL);
            pm_ascii(1, 16, "Choose a POK");
            pm_put(13, 16, Font_CharToTile(0xBA));
            pm_ascii(14, 16, "MON.");
            return;
        }
        pm_draw_msg_box(NULL, NULL);
        pm_ascii(1, 14, "Choose a POK");
        pm_put(13, 14, Font_CharToTile(0xBA));
        pm_ascii(14, 14, "MON.");
    }
}

static void pm_reopen(void) {
    g_open = 1;
    if (wIsInBattle) BattleUI_EnemySpriteSetVisible(0);
}

void PartyMenu_Open(int force) {

    if (wIsInBattle) BattleUI_EnemySpriteSetVisible(0);

    PM_PAL_WHITE();

    Font_LoadHudTiles();
    if (pm_gen2()) {
        pm_load_icons_gen2();

        CrystalIconAnim_Reset();
        for (int i = 0; i < (int)wPartyCount && i < PARTY_LENGTH; i++)
            CrystalIconAnim_Init(i, pm_hp_band(wPartyMons[i].base.hp,
                                               wPartyMons[i].max_hp));
    } else {
        PartyIcons_LoadTiles();
    }

    g_evo_stone         = g_evo_stone_pending;
    g_evo_stone_pending = 0;

    g_force       = force;
    g_open        = 1;
    g_selected    = -1;
    g_anim_tick   = 0;
    g_anim_frame  = 0;
    g_submenu     = 0;
    g_sub_cursor  = 0;
    g_in_summary  = 0;
    g_switching   = 0;
    g_switch_from = -1;
    g_resulting   = 0;
    g_result_timer = 0;
    g_hp_anim     = 0;
    g_need_move     = 0;
    g_move_prompt1  = NULL;
    g_move_prompt2  = NULL;
    g_move_select   = 0;
    g_move_apply    = NULL;
    g_move_result   = 0;
    g_pending_close_after_text = 0;
    g_pending_close_kind = 0;
    g_strength_whiteout = 0;
    g_strength_fade_step = 0;
    g_strength_fade_timer = 0;
    g_close_fade = 0;
    g_swap_anim = 0;
    g_swap_timer = 0;

    g_cursor = g_saved_cursor;

    g_input_lockout = PM_INPUT_LOCKOUT_FRAMES;
    if (g_cursor < 0) g_cursor = 0;
    if (wPartyCount > 0 && g_cursor >= (int)wPartyCount) g_cursor = (int)wPartyCount - 1;

    pm_draw_all();
    pm_write_all_oam(0);

    if (!pm_gen2()) pm1_apply_hp_pals();

    PM_PAL_WHITE();
    g_open_fade = PM_OPEN_FADE_FRAMES;
}

void PartyMenu_FinishOpenFade(void) {
    g_open_fade = 0;
    PM_PAL_NORMAL();
}

void PartyMenu_SetEvoStone(uint8_t stone_item_id) {

    g_evo_stone_pending = stone_item_id;
}

int PartyMenu_IsOpen(void) {
    return g_open;
}

int PartyMenu_GetSavedCursor(void) {
    return g_saved_cursor;
}

int PartyMenu_GetSelected(void) {
    return g_selected;
}

void PartyMenu_Tick(void) {
    if (!g_open) return;

    if (g_input_lockout > 0) {
        g_input_lockout--;
        hJoyPressed = 0;
    }

    if (pm_gen2()) {
        CrystalIconAnim_Tick();

        if (!g_in_summary) pm_write_all_oam(0);
    }

    if (g_reject_redraw) {
        g_reject_redraw = 0;
        g_submenu = 0;
        pm_draw_all();
        pm_write_all_oam(g_anim_frame);
        return;
    }

    if (g_open_fade > 0) {
        if (--g_open_fade == 0) PM_PAL_NORMAL();
        return;
    }

    if (g_close_fade > 0) {
        if (--g_close_fade == 0) {
            g_open = 0;
            pm_clear_icon_oam();
        }
        return;
    }

    if (g_swap_anim) {

        if (g_swap_timer > 0) { g_swap_timer--; return; }
        if (Audio_IsSFXPlaying()) return;
        if (g_swap_anim == 1) {

            Audio_PlaySFX_Swap();
            pm_clear_mon_row(g_cursor);
            g_swap_anim  = 2;
            g_swap_timer = SWAP_HOLD_FRAMES;
            return;
        }

        Audio_PlaySFX_Swap();
        g_swap_anim = 0;
        g_switching = 0;
        pm_draw_all();
        pm_write_all_oam(g_anim_frame);
        return;
    }

    if (g_strength_whiteout) {

        if (g_strength_fade_timer > 0) {
            g_strength_fade_timer--;
            return;
        }

        Display_LoadMapPalette();
        g_strength_whiteout = 0;
        g_selected = -1;
        g_open = 0;
        pm_clear_icon_oam();
        return;
    }

    if (g_pending_close_after_text) {
        if (--g_pending_close_after_text == 0) {
            if (g_pending_close_kind == 2) {

                g_pending_close_kind = 0;
                g_strength_whiteout = 1;
                g_strength_fade_timer = 7;
                Display_SetPalette(0, 0, 0);
                return;
            }
            if (g_pending_close_kind == 3) {

                g_pending_close_kind = 0;
                g_selected = -1;
                g_open = 0;
                pm_clear_icon_oam();
                EscapeAnim_StartToLastHealTownAfter(60);
                return;
            }
            g_pending_close_kind = 0;
            Audio_PlaySFX_PressAB();
            g_selected = -1;
            g_open = 0;
            pm_clear_icon_oam();
        }
        return;
    }

    if (g_hp_anim) {

        if (++g_anim_tick >= pm_anim_rate()) {
            g_anim_tick  = 0;
            g_anim_frame ^= 1;
        }
        g_hp_anim_t++;
        uint16_t cur;
        if (g_hp_anim_t >= g_hp_anim_T) {
            cur = g_hp_anim_new;
        } else {
            uint32_t span = (uint32_t)(g_hp_anim_new - g_hp_anim_old);
            cur = (uint16_t)(g_hp_anim_old +
                             span * (uint32_t)g_hp_anim_t / (uint32_t)g_hp_anim_T);
        }
        pm_draw_slot_hp(g_hp_anim_slot, cur, g_hp_anim_max);
        pm_write_all_oam(g_anim_frame);
        if (g_hp_anim_t >= g_hp_anim_T) {
            g_hp_anim = 0;

            if (g_heal_msg_custom) {
                g_heal_msg_custom = 0;
                PartyMenu_ShowTextResult(g_heal_msg1, g_heal_msg2);
            } else {
                PartyMenu_ShowItemUseResult(g_hp_anim_slot, g_hp_anim_healed, 1);
            }
        }
        return;
    }

    if (g_resulting) {
        if (++g_anim_tick >= pm_anim_rate()) {
            g_anim_tick  = 0;
            g_anim_frame ^= 1;
            pm_write_all_oam(g_anim_frame);
        }
        if (g_result_timer > 0) {
            g_result_timer--;
            return;
        }
        if (!(hJoyPressed & (PAD_A | PAD_B | PAD_START))) return;
        Audio_PlaySFX_PressAB();

        {
            battleexp_event_t ev;
            while (BattleExp_TakeNextEvent(&ev)) {

                if (ev.type == BEXP_EVENT_LEARN_MOVE) {
                    g_resulting = 0;
                    g_open      = 0;
                    if (!g_result_keep_sprites) pm_clear_icon_oam();
                    g_result_keep_sprites = 0;
                    TMHM_BeginLevelUpLearn((int)ev.slot, ev.move_id);
                    return;
                }
                if (ev.type != BEXP_EVENT_TEXT || !ev.text[0]) continue;

                char l1[20], l2[20];
                const char *nl = strchr(ev.text, '\n');
                if (nl) {
                    size_t n = (size_t)(nl - ev.text);
                    if (n >= sizeof(l1)) n = sizeof(l1) - 1;
                    memcpy(l1, ev.text, n); l1[n] = '\0';
                    snprintf(l2, sizeof(l2), "%s", nl + 1);
                } else {
                    snprintf(l1, sizeof(l1), "%s", ev.text);
                    l2[0] = '\0';
                }
                pm_draw_all();
                pm_draw_msg_box(l1, l2);
                pm_write_all_oam(g_anim_frame);
                g_result_timer = 30;
                return;
            }
        }

        g_resulting = 0;
        g_open = 0;

        if (!g_result_keep_sprites) pm_clear_icon_oam();
        g_result_keep_sprites = 0;
        return;
    }

    if (g_in_summary) {
        SummaryScreen_Tick();
        if (!SummaryScreen_IsOpen()) {
            g_in_summary = 0;

            GbcColor_MarkDirty();

            Font_LoadHudTiles();
            if (pm_gen2()) pm_load_icons_gen2();
            else           PartyIcons_LoadTiles();
            pm_draw_all();
            pm_write_all_oam(g_anim_frame);

            PM_PAL_NORMAL();

        }
        return;
    }

    if (g_switching) {

        if (++g_anim_tick >= pm_anim_rate()) {
            g_anim_tick  = 0;
            g_anim_frame ^= 1;
            pm_write_all_oam(g_anim_frame);
        }
        if (hJoyPressed & PAD_UP) {
            g_cursor = (g_cursor - 1 + (int)wPartyCount) % (int)wPartyCount;
            pm_draw_cursor(g_cursor);
            if (g_cursor != g_switch_from)
                pm_put(0, g_switch_from * 2 + 1, Font_CharToTile(0xEC));
            return;
        }
        if (hJoyPressed & PAD_DOWN) {
            g_cursor = (g_cursor + 1) % (int)wPartyCount;
            pm_draw_cursor(g_cursor);
            if (g_cursor != g_switch_from)
                pm_put(0, g_switch_from * 2 + 1, Font_CharToTile(0xEC));
            return;
        }
        if (hJoyPressed & PAD_A) {
            if (g_cursor != g_switch_from) {

                party_mon_t tmp_mon = wPartyMons[g_cursor];
                wPartyMons[g_cursor]       = wPartyMons[g_switch_from];
                wPartyMons[g_switch_from]  = tmp_mon;

                uint8_t tmp_nick[NAME_LENGTH];
                memcpy(tmp_nick,                      wPartyMonNicks[g_cursor],      NAME_LENGTH);
                memcpy(wPartyMonNicks[g_cursor],      wPartyMonNicks[g_switch_from], NAME_LENGTH);
                memcpy(wPartyMonNicks[g_switch_from], tmp_nick,                      NAME_LENGTH);

                uint8_t tmp_sp = wPartySpecies[g_cursor];
                wPartySpecies[g_cursor]      = wPartySpecies[g_switch_from];
                wPartySpecies[g_switch_from] = tmp_sp;
            }

            Audio_PlaySFX_PressAB();
            pm_clear_mon_row(g_switch_from);
            g_swap_anim  = 1;
            g_swap_timer = SWAP_GAP_FRAMES;
            return;
        }
        if (hJoyPressed & PAD_B) {
            Audio_PlaySFX_PressAB();
            g_switching = 0;
            pm_draw_all();
            pm_write_all_oam(g_anim_frame);
            return;
        }
        return;
    }

    if (g_move_result) {
        if (++g_anim_tick >= pm_anim_rate()) {
            g_anim_tick  = 0;
            g_anim_frame ^= 1;
            pm_write_all_oam(g_anim_frame);
        }
        if (g_result_timer > 0) { g_result_timer--; return; }
        if (!(hJoyPressed & (PAD_A | PAD_B | PAD_START))) return;
        Audio_PlaySFX_PressAB();
        g_move_result = 0;
        if (g_move_result_disp == PM_MOVEUSE_RELOOP) {

            g_move_cursor = 0;
            g_move_select = 1;
            pm_draw_move_select(g_move_cursor);
            pm_write_all_oam(g_anim_frame);
            return;
        }

        g_selected    = g_cursor;
        wWhichPokemon = (uint8_t)g_cursor;
        g_open        = 0;
        pm_clear_icon_oam();
        return;
    }

    if (g_move_select) {

        if (++g_anim_tick >= pm_anim_rate()) {
            g_anim_tick  = 0;
            g_anim_frame ^= 1;
            pm_write_all_oam(g_anim_frame);
        }
        if (hJoyPressed & PAD_UP) {
            g_move_cursor = (g_move_cursor - 1 + g_move_count) % g_move_count;
            pm_draw_move_select(g_move_cursor);
            return;
        }
        if (hJoyPressed & PAD_DOWN) {
            g_move_cursor = (g_move_cursor + 1) % g_move_count;
            pm_draw_move_select(g_move_cursor);
            return;
        }
        if (hJoyPressed & PAD_A) {

            Audio_PlaySFX_PressAB();
            int mv = g_move_slot[g_move_cursor];
            char l1[20] = "", l2[20] = "";
            pm_moveuse_disp_t disp = PM_MOVEUSE_CLOSE;
            if (g_move_apply) disp = g_move_apply(g_cursor, mv, l1, l2);
            g_move_select      = 0;
            g_move_result      = 1;
            g_move_result_disp = (int)disp;
            g_result_timer     = 50;

            pm_draw_msg_box(l1[0] ? l1 : NULL, l2[0] ? l2 : NULL);
            pm_draw_move_box(g_move_cursor);
            pm_write_all_oam(g_anim_frame);
            return;
        }
        if (hJoyPressed & PAD_B) {

            Audio_PlaySFX_PressAB();
            g_move_select = 0;
            pm_draw_all();
            pm_write_all_oam(g_anim_frame);
            return;
        }
        return;
    }

    if (g_submenu) {
        int num_items = (g_force == 2) ? 3 : pm_submenu_item_count();
        if (hJoyPressed & PAD_UP) {
            g_sub_cursor = (g_sub_cursor - 1 + num_items) % num_items;
            pm_draw_submenu(g_sub_cursor);
            return;
        }
        if (hJoyPressed & PAD_DOWN) {
            g_sub_cursor = (g_sub_cursor + 1) % num_items;
            pm_draw_submenu(g_sub_cursor);
            return;
        }
        if (hJoyPressed & PAD_A) {
            Audio_PlaySFX_PressAB();
            g_submenu = 0;
            if (g_force == 2) {

                if (g_sub_cursor == 0) {

                    if (pm_battle_switch_rejected(g_cursor)) return;
                    wWhichPokemon = (uint8_t)g_cursor;
                    g_selected    = g_cursor;
                    g_open        = 0;
                    pm_clear_icon_oam();
                } else if (g_sub_cursor == 1) {

                    g_in_summary = 1;
                    SummaryScreen_Open(g_cursor);
                } else {

                    pm_draw_all();
                    pm_write_all_oam(g_anim_frame);
                }
            } else if (g_force == PARTY_MENU_ITEM_USE) {
                wWhichPokemon = (uint8_t)g_cursor;
                g_selected    = g_cursor;
                g_open        = 0;
                pm_clear_icon_oam();
            } else {

                if (g_sub_cursor < g_field_move_count) {
                    pm_field_action_t action = g_field_moves[g_sub_cursor].action;
                    wWhichPokemon = (uint8_t)g_cursor;
                    if (action == PM_FIELD_ACTION_CUT) {

                        pm_draw_all();
                        pm_write_all_oam(g_anim_frame);
                        if (FieldMove_UseCutFromMenu()) {
                            g_selected = -1;
                            g_open     = 0;
                            pm_clear_icon_oam();
                        } else {
                            pm_draw_all();
                            pm_write_all_oam(g_anim_frame);
                        }
                    } else if (action == PM_FIELD_ACTION_SURF) {

                        pm_draw_all();
                        pm_write_all_oam(g_anim_frame);
                        int result = FieldMove_UseSurfFromMenu(g_cursor);
                        if (result == 2) {
                            g_pending_close_after_text = 2;
                            g_pending_close_kind = 1;
                            return;
                        }
                        if (result) {
                            g_selected = -1;
                            g_open     = 0;
                            pm_clear_icon_oam();
                        } else {
                            pm_draw_all();
                            pm_write_all_oam(g_anim_frame);
                        }
                    } else if (action == PM_FIELD_ACTION_FLY) {

                        pm_draw_all();
                        pm_write_all_oam(g_anim_frame);
                        wWhichPokemon = (uint8_t)g_cursor;
                        if (!FieldMove_TryFly(g_cursor)) {

                            pm_draw_all();
                            pm_write_all_oam(g_anim_frame);
                            return;
                        }
                        TownMap_OpenFly();
                        g_selected = -1;
                        g_open     = 0;
                        pm_clear_icon_oam();
                    } else if (action == PM_FIELD_ACTION_FLASH) {

                        pm_draw_all();
                        pm_write_all_oam(g_anim_frame);
                        int result = FieldMove_TryFlash(g_cursor);
                        if (result == 2) {
                            g_pending_close_after_text = 2;
                            g_pending_close_kind = 2;
                            return;
                        }
                        pm_draw_all();
                        pm_write_all_oam(g_anim_frame);
                    } else if (action == PM_FIELD_ACTION_DIG) {

                        pm_draw_all();
                        pm_write_all_oam(g_anim_frame);
                        wWhichPokemon = (uint8_t)g_cursor;
                        if (FieldMove_TryDig(g_cursor)) {
                            g_selected = -1;
                            g_open     = 0;
                            pm_clear_icon_oam();
                        } else {
                            pm_draw_all();
                            pm_write_all_oam(g_anim_frame);
                        }
                    } else if (action == PM_FIELD_ACTION_TELEPORT) {

                        pm_draw_all();
                        pm_write_all_oam(g_anim_frame);
                        wWhichPokemon = (uint8_t)g_cursor;
                        int result = FieldMove_TryTeleport(g_cursor);
                        if (result == 2) {
                            g_pending_close_after_text = 2;
                            g_pending_close_kind = 3;
                            return;
                        }
                        pm_draw_all();
                        pm_write_all_oam(g_anim_frame);
                    } else if (action == PM_FIELD_ACTION_STRENGTH) {

                        pm_draw_all();
                        pm_write_all_oam(g_anim_frame);
                        int result = FieldMove_TryStrength(g_cursor);
                        if (result == 2) {
                            g_pending_close_after_text = 2;
                            g_pending_close_kind = 2;
                            return;
                        }
                        if (!result) {
                            pm_draw_all();
                            pm_write_all_oam(g_anim_frame);
                        }
                    }
                } else if (g_sub_cursor == g_field_move_count) {

                    g_in_summary = 1;
                    SummaryScreen_Open(g_cursor);
                } else if (g_sub_cursor == g_field_move_count + 1) {

                    g_switching   = 1;
                    g_switch_from = g_cursor;
                    pm_draw_all();
                    pm_write_all_oam(g_anim_frame);
                } else {
                    pm_draw_all();
                    pm_write_all_oam(g_anim_frame);
                }
            }
            return;
        }
        if (hJoyPressed & PAD_B) {
            Audio_PlaySFX_PressAB();
            g_submenu = 0;
            pm_draw_all();
            pm_write_all_oam(g_anim_frame);
            return;
        }
        return;
    }

    if (++g_anim_tick >= pm_anim_rate()) {
        g_anim_tick  = 0;
        g_anim_frame ^= 1;
        pm_write_all_oam(g_anim_frame);
    }

    if (hJoyPressed & PAD_UP) {
        g_cursor = (g_cursor - 1 + pm_cursor_count()) % pm_cursor_count();
        g_saved_cursor = g_cursor;
        pm_draw_cursor(g_cursor);
        pm_write_all_oam(g_anim_frame);
        return;
    }
    if (hJoyPressed & PAD_DOWN) {
        g_cursor = (g_cursor + 1) % pm_cursor_count();
        g_saved_cursor = g_cursor;
        pm_draw_cursor(g_cursor);
        pm_write_all_oam(g_anim_frame);
        return;
    }
    if (hJoyPressed & PAD_A) {
        Audio_PlaySFX_PressAB();

        if (pm_gen2() && g_cursor >= (int)wPartyCount) {
            g_selected = -1;
            if (g_force == 0) {
                PM_PAL_WHITE();
                g_close_fade = PM_CLOSE_FADE_FRAMES;
            } else {
                g_open = 0;
                pm_clear_icon_oam();
            }
            return;
        }
        if (g_force == 1 || g_force == PARTY_MENU_BATTLE_SHIFT ||
            g_force == PARTY_MENU_TMHM || g_force == PARTY_MENU_ITEM_USE ||
            g_force == PARTY_MENU_TRADE) {

            if (g_force == PARTY_MENU_ITEM_USE && g_need_move) {
                pm_enter_move_select(g_cursor);
                return;
            }

            if ((g_force == 1 || g_force == PARTY_MENU_BATTLE_SHIFT) &&
                pm_battle_switch_rejected(g_cursor)) return;

            wWhichPokemon = (uint8_t)g_cursor;
            g_selected    = g_cursor;
            g_open        = 0;
            pm_clear_icon_oam();
        } else {

            if (g_force == 0) {
                pm_collect_field_moves(g_cursor);
            } else {
                g_field_move_count = 0;
                g_field_leftmost   = PM_FIELD_LEFTMOST_DEFAULT;
            }
            g_submenu    = 1;
            g_sub_cursor = 0;
            pm_draw_submenu(g_sub_cursor);
        }
        return;
    }
    if (g_force != 1 && (hJoyPressed & PAD_B)) {
        Audio_PlaySFX_PressAB();
        g_selected = -1;
        if (g_force == 0) {

            PM_PAL_WHITE();
            g_close_fade = PM_CLOSE_FADE_FRAMES;
        } else {

            g_open = 0;
            pm_clear_icon_oam();
        }
        return;
    }
}

void PartyMenu_ShowItemUseResult(int slot, uint16_t healed, int success) {
    g_resulting    = 1;
    g_result_timer = 50;
    pm_reopen();
    pm_draw_item_use_result(slot, healed, success);
}

void PartyMenu_SetHealResultMessage(const char *line1, const char *line2) {
    g_heal_msg_custom = 1;
    snprintf(g_heal_msg1, sizeof(g_heal_msg1), "%s", line1 ? line1 : "");
    snprintf(g_heal_msg2, sizeof(g_heal_msg2), "%s", line2 ? line2 : "");
}

static void pm_draw_slot_hp(int slot, uint16_t hp, uint16_t max_hp) {
    if (pm_gen2()) {
        int top = PM2_TOP_ROW + slot * 2;
        pm_num3(PM2_HPNUM_COL, top, (int)hp);
        pm_gb(PM2_HPNUM_COL + 3, top, 0xF3);
        pm_num3(PM2_HPNUM_COL + 4, top, (int)max_hp);
        pm2_draw_hp_bar(PM2_HPBAR_COL, top + 1, pm_hp_pixels(hp, max_hp));
    } else {
        pm_draw_hp_bar(slot * 2 + 1, hp, max_hp);
    }
}

void PartyMenu_AnimateItemHeal(int slot, uint16_t old_hp, uint16_t new_hp,
                              uint16_t healed) {
    pm_reopen();
    g_hp_anim_slot   = slot;
    g_hp_anim_old    = old_hp;
    g_hp_anim_new    = new_hp;
    g_hp_anim_max    = wPartyMons[slot].max_hp;
    g_hp_anim_healed = healed;
    g_hp_anim_t      = 0;

    int old_px = pm_hp_pixels(old_hp, g_hp_anim_max);
    int new_px = pm_hp_pixels(new_hp, g_hp_anim_max);
    int dpx    = new_px - old_px;
    if (dpx < 0) dpx = 0;
    g_hp_anim_T = dpx * 2;
    if (g_hp_anim_T < 1) g_hp_anim_T = 1;
    g_hp_anim = 1;

    pm_draw_slot_hp(slot, old_hp, g_hp_anim_max);
    pm_write_all_oam(g_anim_frame);
}

void PartyMenu_ShowTextResult(const char *line1, const char *line2) {
    g_resulting    = 1;
    g_result_timer = 50;
    pm_reopen();
    pm_draw_all();
    pm_draw_msg_box(line1, line2);
    pm_write_all_oam(g_anim_frame);
}

void PartyMenu_RequestMoveSelect(const char *prompt1, const char *prompt2,
                                 pm_moveuse_fn apply) {
    g_need_move    = 1;
    g_move_prompt1 = prompt1;
    g_move_prompt2 = prompt2;
    g_move_apply   = apply;
}

const char *PartyMenu_MonName(int slot) {
    return pm_party_mon_name(slot);
}

void PartyMenu_ShowStatRoseResult(int slot, const char *stat, int success) {
    g_resulting    = 1;
    g_result_timer = 50;
    pm_reopen();
    if (!success) {
        pm_draw_all();
        pm_draw_msg_box("It won't have", "any effect.");
        pm_write_all_oam(g_anim_frame);
        return;
    }
    char line1[20], line2[20];
    snprintf(line1, sizeof(line1), "%s's", pm_party_mon_name(slot));
    snprintf(line2, sizeof(line2), "%s rose.", stat);
    pm_draw_all();
    pm_draw_msg_box(line1, line2);
    pm_write_all_oam(g_anim_frame);
}

void PartyMenu_DbgDumpIcons(const char *tag) {
    static int last = -1;
    int v = (int)wShadowOAM[0].y * 100000 + (int)wShadowOAM[0].tile * 100
          + (int)wShadowOAM[2].y;
    if (v == last) return;
    last = v;
    printf("[ICONDBG] %-18s gen2=%d oam0=(y%u t%u f%02x) oam2=(y%u t%u) oam4=(y%u t%u)\n",
           tag, pm_gen2(), wShadowOAM[0].y, wShadowOAM[0].tile, wShadowOAM[0].flags,
           wShadowOAM[2].y, wShadowOAM[2].tile,
           wShadowOAM[4].y, wShadowOAM[4].tile);
    fflush(stdout);
}

void PartyMenu_RestoreIcons(void) {

    if (pm_gen2()) pm2_apply_hp_pals();
    pm_write_all_oam(g_anim_frame);
    PartyMenu_DbgDumpIcons("RestoreIcons");
}

void PartyMenu_ClearIcons(void) {
    pm_clear_icon_oam();
}

void PartyMenu_KeepIconsForEvolution(void) {
    PartyMenu_RestoreIcons();
}

void PartyMenu_ShowRareCandyResult(int slot, int success, int new_level, int evolved) {

    g_result_keep_sprites = (success && evolved);
    g_resulting    = 1;
    g_result_timer = 50;
    pm_reopen();
    if (!success) {
        pm_draw_all();
        pm_draw_msg_box("It won't have", "any effect.");
        pm_write_all_oam(g_anim_frame);
        return;
    }

    Audio_PlaySFX_GetItem1();

    char line1[20], line2[20];
    snprintf(line1, sizeof(line1), "%s grew to", pm_party_mon_name(slot));
    snprintf(line2, sizeof(line2), "level %d!", new_level);
    pm_draw_all();
    pm_draw_msg_box(line1, line2);
    pm_write_all_oam(g_anim_frame);
}
