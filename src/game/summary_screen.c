
#include "summary_screen.h"
#include "assetpack_bind.h"
#include "../platform/hardware.h"
#include "../platform/display.h"
#include "../platform/audio.h"
#include "../data/font_data.h"
#include "../data/base_stats.h"
#include "../data/moves_data.h"
#include "../data/pokemon_sprites.h"
#include "constants.h"
#include "type_mod.h"
#include "sprite_mod.h"
#include "mon_pic.h"
#include "gbc_color.h"
#include "crystal_stats_screen.h"
#include "crystal_fade.h"
#include "gen2_resources.h"
#include "gen1color/crystal_pic_anim.h"
#include "johto_music.h"
#include "gen2_species.h"
#include "overworld.h"
#include "pokemon.h"
#include <stdio.h>
#include <string.h>

static void ss_borrow_obj_pal0(void);
static void ss_return_obj_pal0(void);

extern int         gScrollPxX, gScrollPxY;
extern uint8_t     wPartyCount;
extern party_mon_t wPartyMons[PARTY_LENGTH];
extern uint8_t     wPartyMonNicks[PARTY_LENGTH][NAME_LENGTH];
extern uint8_t     wPartyMonOT[PARTY_LENGTH][NAME_LENGTH];

typedef enum { SS_CLOSED = 0, SS_PAGE1, SS_PAGE2, SS_PAGE3 } ss_state_t;
static ss_state_t s_state = SS_CLOSED;
static int        s_slot  = 0;

#define SS_SPR_TILE_BASE  0
#define SS_SPR_OAM_BASE   0
#define SS_SPR_PX_X       8
#define SS_SPR_PX_Y       0

static void ss_put(int col, int row, int tile_idx) {
    if ((unsigned)col >= SCREEN_WIDTH || (unsigned)row >= SCREEN_HEIGHT) return;
    gScrollTileMap[(row + 2) * SCROLL_MAP_W + (col + 2) + Map_UiColOfs()] = (uint8_t)tile_idx;
}

static void ss_gb(int col, int row, uint8_t gb_char) {
    ss_put(col, row, Font_CharToTile(gb_char));
}

static int ss_ascii_tile(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return Font_CharToTile(0x80 + (c - 'A'));
    if (c >= 'a' && c <= 'z') return Font_CharToTile(0xA0 + (c - 'a'));
    if (c >= '0' && c <= '9') return Font_CharToTile(0xF6 + (c - '0'));
    if (c == ' ')  return BLANK_TILE_SLOT;
    if (c == '.')  return Font_CharToTile(0xE8);
    if (c == '!')  return Font_CharToTile(0xE7);
    if (c == '?')  return Font_CharToTile(0xE6);
    if (c == ',')  return Font_CharToTile(0xF4);
    if (c == '-')  return Font_CharToTile(0xE3);
    if (c == '\'') return Font_CharToTile(0xE0);
    if (c == '/')  return Font_CharToTile(0xF3);
    return BLANK_TILE_SLOT;
}

static void ss_ascii(int col, int row, const char *s) {
    for (; *s; s++, col++)
        ss_put(col, row, ss_ascii_tile((unsigned char)*s));
}

static void ss_num(int col, int row, uint32_t val, int width) {
    char buf[9];
    snprintf(buf, sizeof(buf), "%*u", width, (unsigned)val);
    for (int i = 0; i < width; i++) {
        char c = buf[i];
        ss_put(col + i, row,
               c == ' ' ? BLANK_TILE_SLOT
                        : Font_CharToTile(0xF6 + (c - '0')));
    }
}

static void ss_nick(int col, int row, int slot) {
    const uint8_t *nick = wPartyMonNicks[slot];
    if (nick[0] != 0x00) {
        for (int i = 0; i < 10; i++) {
            uint8_t ch = nick[i];
            if (ch == 0x50) {
                for (int j = i; j < 10; j++) ss_put(col + j, row, BLANK_TILE_SLOT);
                return;
            }
            ss_put(col + i, row, Font_CharToTile(ch));
        }
    } else {

        uint8_t dex = Species_Dex(wPartyMons[slot].base.species);
        const char *name = Pokemon_GetName(dex);
        int len = (int)strlen(name);
        for (int i = 0; i < 10; i++) {
            ss_put(col + i, row,
                   i < len ? ss_ascii_tile((unsigned char)name[i]) : BLANK_TILE_SLOT);
        }
    }
}

static void ss_gbstr(int col, int row, const uint8_t *s) {
    for (; *s != 0x50; s++, col++)
        ss_put(col, row, Font_CharToTile(*s));
}

static void ss_ot_name(int col, int row, int slot) {
    const uint8_t *ot = wPartyMonOT[slot];
    for (int i = 0; i < 7; i++) {
        uint8_t ch = ot[i];
        if (ch == 0x50 || ch == 0x00) {
            for (int j = i; j < 7; j++) ss_put(col + j, row, BLANK_TILE_SLOT);
            return;
        }
        ss_put(col + i, row, Font_CharToTile(ch));
    }
}

static void ss_level(int col, int row, uint8_t level) {
    if (level >= 100) {
        ss_put(col,   row, Font_CharToTile(0xF6 + (level / 100)));
        ss_put(col+1, row, Font_CharToTile(0xF6 + ((level / 10) % 10)));
        ss_put(col+2, row, Font_CharToTile(0xF6 + (level % 10)));
    } else {
        ss_gb(col,    row, 0x6E);
        ss_put(col+1, row, Font_CharToTile(0xF6 + (level / 10)));
        ss_put(col+2, row, Font_CharToTile(0xF6 + (level % 10)));
    }
}

static int ss_hp_bar_color(uint16_t hp, uint16_t max_hp) {
    int pixels = max_hp ? ((int)hp * 48 / (int)max_hp) : 0;
    if (hp > 0 && pixels == 0) pixels = 1;
    if (pixels > 48) pixels = 48;
    if (pixels > 24) return 0;
    if (pixels >  9) return 1;
    return 2;
}

static void ss_hp_bar(int col, int row, uint16_t hp, uint16_t max_hp) {
    int pixels = max_hp ? ((int)hp * 48 / (int)max_hp) : 0;
    if (hp > 0 && pixels == 0) pixels = 1;
    if (pixels > 48) pixels = 48;

    ss_gb(col,   row, 0x71);
    ss_gb(col+1, row, 0x62);
    for (int i = 0; i < 6; i++) {
        int seg = pixels - i * 8;
        uint8_t tile = (seg <= 0) ? 0x63u
                     : (seg >= 8) ? 0x6Bu
                     :              (uint8_t)(0x63u + (unsigned)seg);
        ss_gb(col+2+i, row, tile);
    }
    ss_gb(col+8, row, 0x6D);

    ss_num(col+1, row+1, hp,     3);
    ss_gb( col+4, row+1, 0xF3);
    ss_num(col+5, row+1, max_hp, 3);
}

static const uint8_t k_fnt[3] = {0x85, 0x8D, 0x93};
static const uint8_t k_slp[3] = {0x92, 0x8B, 0x8F};
static const uint8_t k_psn[3] = {0x8F, 0x92, 0x8D};
static const uint8_t k_brn[3] = {0x81, 0x91, 0x8D};
static const uint8_t k_frz[3] = {0x85, 0x91, 0x99};
static const uint8_t k_par[3] = {0x8F, 0x80, 0x91};

static void ss_status(int col, int row, uint8_t status, uint16_t hp) {
    const uint8_t *s = NULL;
    if (!hp)                  s = k_fnt;
    else if (status & 0x07)   s = k_slp;
    else if (status & (1<<3)) s = k_psn;
    else if (status & (1<<4)) s = k_brn;
    else if (status & (1<<5)) s = k_frz;
    else if (status & (1<<6)) s = k_par;
    for (int i = 0; i < 3; i++) {
        if (s) ss_gb(col + i, row, s[i]);
        else   ss_put(col + i, row, BLANK_TILE_SLOT);
    }
}

static const char *ss_type_name(uint8_t type_id) {
    static const char *names[27] = {
        "NORMAL","FIGHTING","FLYING","POISON","GROUND","ROCK","BIRD","BUG","GHOST",
        "","","","","","","","","","","",
        "FIRE","WATER","GRASS","ELECTRIC","PSYCHIC","ICE","DRAGON"
    };
    {
        const char *dyn = TypeMod_GetTypeName(type_id);
        if (dyn && *dyn) return dyn;
    }
    if (type_id < 27) return names[type_id];
    return "";
}

static void ss_box(int x, int y, int b, int c) {
    ss_gb(x,     y,     0x79);
    for (int i = 1; i <= c; i++) ss_gb(x+i, y,     0x7A);
    ss_gb(x+c+1, y,     0x7B);
    for (int r = 1; r <= b; r++) {
        ss_gb(x,     y+r, 0x7C);
        for (int i = 1; i <= c; i++) ss_put(x+i, y+r, BLANK_TILE_SLOT);
        ss_gb(x+c+1, y+r, 0x7C);
    }
    ss_gb(x,     y+b+1, 0x7D);
    for (int i = 1; i <= c; i++) ss_gb(x+i, y+b+1, 0x7A);
    ss_gb(x+c+1, y+b+1, 0x7E);
}

static void ss_line_box(int col, int row, int b_tiles, int c_tiles) {
    for (int i = 0; i < b_tiles; i++)
        ss_gb(col, row + i, 0x78);
    ss_gb(col,           row + b_tiles, 0x77);
    for (int i = 1; i <= c_tiles; i++)
        ss_gb(col - i,   row + b_tiles, 0x76);
    ss_gb(col - c_tiles - 1, row + b_tiles, 0x6F);
}

static void ss_clear_sprite_oam(void);

static int ss_gen2(void);

static void ss_load_sprite(uint8_t species, uint8_t dex) {
    int has_override = (SpriteMod_GetFrontTile(species, 0) != NULL);
    ss_clear_sprite_oam();

    if (s_slot >= 0 && s_slot < (int)wPartyCount)
        GbcColor_SetPalStatusScreen((int)dex,
            ss_hp_bar_color(wPartyMons[s_slot].base.hp, wPartyMons[s_slot].max_hp));

    if (!has_override && !MonPic_Exists(dex)) return;
    for (int i = 0; i < POKEMON_FRONT_CANVAS_TILES; i++)
    {
        const uint8_t *tile = SpriteMod_GetFrontTile(species, i);

        if (!tile && ss_gen2() && MonPic_CrystalExists(dex))
            tile = MonPic_CrystalFrontTile(dex, i);
        Display_LoadSpriteTile((uint8_t)(SS_SPR_TILE_BASE + i),
                               tile ? tile : MonPic_FrontTile(dex, i));
    }

    for (int ty = 0; ty < 7; ty++) {
        for (int tx = 0; tx < 7; tx++) {
            int idx = SS_SPR_OAM_BASE + ty * 7 + tx;
            wShadowOAM[idx].y    = (uint8_t)(SS_SPR_PX_Y + ty * 8 + OAM_Y_OFS);
            wShadowOAM[idx].x    = (uint8_t)(SS_SPR_PX_X + tx * 8 + OAM_X_OFS);
            wShadowOAM[idx].tile = (uint8_t)(SS_SPR_TILE_BASE + ty * 7 + (6 - tx));
            wShadowOAM[idx].flags = OAM_FLAG_FLIP_X;
        }
    }
}

static void ss_clear_sprite_oam(void) {
    for (int i = SS_SPR_OAM_BASE; i < SS_SPR_OAM_BASE + 49; i++) {
        wShadowOAM[i].y    = 0;
        wShadowOAM[i].x    = 0;
        wShadowOAM[i].tile = 0;
        wShadowOAM[i].flags = 0;
    }
}

static int ss_gen2(void) { return Font_GetStyle() == FONT_STYLE_GEN2; }

static const char *const SS_OWNER = "summary_screen";

enum { SS_ANIM_OFF = 0, SS_ANIM_PLAY1, SS_ANIM_WAIT, SS_ANIM_PLAY2 };
#define SS_ANIM_WAIT_FRAMES 18
static int s_anim_step;
static int s_anim_wait;

static void ss_gen2_apply_pals(int slot) {
    if (!GbcColor_IsEnabled()) return;
    const party_mon_t *mon = &wPartyMons[slot];

    int px = mon->max_hp ? (int)((uint32_t)mon->base.hp * 48u / mon->max_hp) : 0;
    int band = (px > 24) ? 0 : (px > 9) ? 1 : 2;

    Display_SetBGColorPalette(0, gCrystalStatsHPBarPals[band]);

    Display_SetBGColorPalette(1, GbcColor_MonPaletteRGB(Species_Dex(mon->base.species)));
    Display_SetBGColorPalette(2, gCrystalStatsExpBarPal[0]);
    for (int p = 0; p < CRYSTAL_STATS_PALS; p++)
        Display_SetBGColorPalette(3 + p, gCrystalStatsPagePals[p]);

    {
        uint16_t objpal[4];
        if (MonPic_CrystalPalette(Species_Dex(mon->base.species), objpal)) {

            Gen2Res_Borrow(GEN2_LAYER_SPRITES, GEN2_RES_OBJ_PAL, 0, 1, SS_OWNER);
            ss_borrow_obj_pal0();
            Display_SetOBJColorPalette(0, objpal);
        }
    }

    Display_SetPositionAttrMode(1);
    Display_ClearAttrBoxes(0);
    Display_FillAttrBox(0, 0, SCREEN_WIDTH, 8, 1);
    Display_FillAttrBox(10, 16, 10, 1, 2);
    Display_FillAttrBox(13, 5, 2, 2, 3);
    Display_FillAttrBox(15, 5, 2, 2, 4);
    Display_FillAttrBox(17, 5, 2, 2, 5);
    Display_SetColorMode(1);
}

static void ss_gen2_close_restore(void);

static void ss_gen2_release_pals(void) {

    if (!GbcColor_IsEnabled()) return;
    Display_SetPositionAttrMode(0);
    Display_ClearAttrBoxes(0);

}

static void ss_gen2_page_color(int page) {
    if (!GbcColor_IsEnabled()) return;
    if (page < 0 || page >= CRYSTAL_STATS_PALS) return;
    Display_SetBGColorEntry(0, 0, gCrystalStatsPageColor[page]);
    Display_SetBGColorEntry(2, 0, gCrystalStatsPageColor[page]);
}

static uint32_t ss_exp_to_next(const party_mon_t *mon, uint32_t cur_exp) {
    base_stats_t bs;
    if (mon->level >= 100) return 0;
    if (!Species_GetBaseStats(mon->base.species, &bs)) return 0;
    uint32_t next = CalcExpForLevel(bs.growth_rate, (uint8_t)(mon->level + 1));
    return (next > cur_exp) ? (next - cur_exp) : 0;
}

static void ss_stile(int col, int row, uint8_t id);

static void ss_draw_pink_page(int slot) {
    const party_mon_t *mon = &wPartyMons[slot];
    uint16_t hp = mon->base.hp, max_hp = mon->max_hp;

    ss_hp_bar(0, 9, hp, max_hp);
    ss_stile(8, 9, 0x41);

    ss_ascii(0, 12, "STATUS/");
    ss_ascii(0, 13, "TYPE/");

    ss_status(6, 13, mon->base.status, hp);

    for (int i = 0; i < 10; i++) ss_stile(9, 8 + i, 0x31);

    ss_ascii(10, 9,  gCrystalStatsExpPoints);
    ss_ascii(10, 12, gCrystalStatsLevelUp);
    ss_ascii(14, 14, gCrystalStatsTo);
    ss_level(17, 14, (uint8_t)(mon->level < 100 ? mon->level + 1 : mon->level));

    {
        uint32_t exp = ((uint32_t)mon->base.exp[0] << 16)
                     | ((uint32_t)mon->base.exp[1] << 8)
                     |  (uint32_t)mon->base.exp[2];
        ss_num(13, 10, exp, 7);
        ss_num(13, 13, ss_exp_to_next(mon, exp), 7);
    }

    ss_stile(10, 16, 0x40);
    ss_stile(19, 16, 0x41);
}

#define SS2_TILE_BASE   96
#define SS2_PAGE_TILES  17
#define SS2_EXPBAR_BASE (SS2_TILE_BASE + SS2_PAGE_TILES)

static int ss2_tile(uint8_t id) {
    if (id >= 0x31 && id <= 0x41) return SS2_TILE_BASE + (id - 0x31);
    if (id >= 0x55 && id <= 0x5C) return SS2_EXPBAR_BASE + (id - 0x55);
    return Font_CharToTile(id);
}

static void ss_stile(int col, int row, uint8_t id) {
    ss_put(col, row, ss_gen2() ? ss2_tile(id) : Font_CharToTile(id));
}

static int s_g2_gfx_dirty;

#define SS2_TILE_SPAN 25

static void ss_gen2_load_font(void) {

    Gen2Res_Borrow(GEN2_LAYER_UI, GEN2_RES_BG_TILES,
                   SS2_TILE_BASE, SS2_TILE_SPAN, SS_OWNER);

    s_g2_gfx_dirty = 1;
    for (int s = 0; s < CRYSTAL_STATS_GFX_SETS; s++) {
        const crystal_stats_gfx_t *g = &gCrystalStatsGFX[s];
        for (int t = 0; t < g->count; t++)
            Display_LoadTile((uint8_t)ss2_tile((uint8_t)(g->dest_tile + t)),
                             g->tiles[t]);
    }
}

static void ss_anim_upload(int dex) {
    const uint8_t *map = CrystalPicAnim_FrameMap(dex);
    for (int i = 0; i < POKEMON_FRONT_CANVAS_TILES; i++) {

        Display_LoadSpriteTile((uint8_t)(SS_SPR_TILE_BASE + i),
                               MonPic_CrystalAnimTile(dex, i, map));
    }
}

static void ss_anim_start(int slot) {
    const party_mon_t *mon = &wPartyMons[slot];
    int dex = Species_Dex(mon->base.species);
    s_anim_step = SS_ANIM_OFF;
    if (!ss_gen2() || !MonPic_CrystalExists(dex)) return;

    uint8_t st = mon->base.status;
    int silent = (mon->base.hp == 0) || (st & STATUS_FRZ) || (st & STATUS_SLP_MASK);
    if (!silent) JohtoAudio_PlayCry(mon->base.species);

    CrystalPicAnim_StartOwner(CRYSTAL_ANIM_OWNER_SUMMARY,
                              dex, CRYSTAL_ANIM_NORMAL);
    s_anim_step = SS_ANIM_PLAY1;
}

static void ss_anim_tick(int slot) {
    if (s_anim_step == SS_ANIM_OFF) return;
    int dex = Species_Dex(wPartyMons[slot].base.species);

    if (s_anim_step == SS_ANIM_WAIT) {
        if (--s_anim_wait > 0) return;
        CrystalPicAnim_StartOwner(CRYSTAL_ANIM_OWNER_SUMMARY,
                                  dex, CRYSTAL_ANIM_NORMAL);
        s_anim_step = SS_ANIM_PLAY2;
        return;
    }

    CrystalPicAnim_Tick();
    ss_anim_upload(dex);
    if (CrystalPicAnim_Running()) return;

    if (s_anim_step == SS_ANIM_PLAY1) {
        s_anim_wait = SS_ANIM_WAIT_FRAMES;
        s_anim_step = SS_ANIM_WAIT;
    } else {
        s_anim_step = SS_ANIM_OFF;
    }
}

static uint16_t s_saved_obj0[4];
static int      s_saved_obj0_valid = 0;

static void ss_borrow_obj_pal0(void) {
    if (!GbcColor_IsEnabled() || s_saved_obj0_valid) return;
    for (int i = 0; i < 4; i++) s_saved_obj0[i] = Display_GetOBJColorEntry(0, i);
    s_saved_obj0_valid = 1;
}

static void ss_return_obj_pal0(void) {
    if (!s_saved_obj0_valid) return;
    s_saved_obj0_valid = 0;
    if (!GbcColor_IsEnabled()) return;
    Display_SetOBJColorPalette(0, s_saved_obj0);
}

static void ss_gen2_close_restore(void) {

    ss_return_obj_pal0();

    s_anim_step = SS_ANIM_OFF;
    CrystalPicAnim_StopOwner(CRYSTAL_ANIM_OWNER_SUMMARY);
    if (!s_g2_gfx_dirty) return;
    s_g2_gfx_dirty = 0;
    ss_gen2_release_pals();

    Gen2Res_ReturnAll(SS_OWNER);
    Font_Load();
}

static void ss_page_square(int col, uint8_t base) {
    ss_stile(col,     5, base);
    ss_stile(col + 1, 5, (uint8_t)(base + 1));
    ss_stile(col,     6, (uint8_t)(base + 2));
    ss_stile(col + 1, 6, (uint8_t)(base + 3));
}

static void ss_page_indicators(int page) {
    static const int kCol[3] = { 13, 15, 17 };
    for (int i = 0; i < 3; i++) ss_page_square(kCol[i], 0x36);
    if (page >= 0 && page < 3) ss_page_square(kCol[page], 0x3A);
}

static void ss_gen2_upper_half(int slot) {
    const party_mon_t *mon = &wPartyMons[slot];
    uint8_t dex = Species_Dex(mon->base.species);

    for (int r = 0; r < SCREEN_HEIGHT; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            ss_put(c, r, BLANK_TILE_SLOT);

    ss_gb(8, 0, 0x74);
    ss_gb(9, 0, 0xE8);

    {
        char buf[8];
        snprintf(buf, sizeof buf, "%03u", (unsigned)dex);
        ss_ascii(10, 0, buf);
    }
    ss_level(14, 0, mon->level);
    ss_nick(8, 2, slot);

    ss_gb(9, 4, 0xF3);
    ss_ascii(10, 4, Pokemon_GetName(dex));

    for (int c = 0; c < SCREEN_WIDTH; c++) ss_gb(c, 7, 0x62);

    ss_gb(12, 6, 0x71);
    ss_gb(19, 6, 0xED);

    ss_load_sprite(mon->base.species, dex);
}

static void ss_clear_lower(void) {
    for (int row = 8; row < SCREEN_HEIGHT; row++)
        for (int col = 0; col < SCREEN_WIDTH; col++)
            ss_put(col, row, BLANK_TILE_SLOT);
}

static void ss_draw_green_page(int slot) {
    const party_mon_t *mon = &wPartyMons[slot];
    ss_clear_lower();
    ss_ascii(0, 8,  gCrystalStatsItem);

    ss_ascii(8, 8, gCrystalStatsThreeDashes);

    ss_ascii(0, 10, gCrystalStatsMove);
    for (int i = 0; i < NUM_MOVES; i++) {
        int name_row = 10 + i * 2;
        int pp_row   = 11 + i * 2;
        uint8_t mv = mon->base.moves[i];
        if (mv == 0 || mv >= NUM_MOVE_DEFS) {

            ss_ascii(8, name_row, "-");
            ss_ascii(12, pp_row, "--");
            continue;
        }
        ss_ascii(8, name_row, gMoveNames[mv]);

        ss_stile(12, pp_row, 0x3E);
        ss_stile(13, pp_row, 0x3E);

        uint8_t pp_ups = mon->base.pp[i] >> 6;
        ss_num(15, pp_row, mon->base.pp[i] & PP_MASK, 2);
        ss_gb (17, pp_row, 0xF3);
        ss_num(18, pp_row, (uint8_t)(gMoves[mv].pp * (5 + pp_ups) / 5), 2);
    }
}

static void ss_draw_blue_page(int slot) {
    const party_mon_t *mon = &wPartyMons[slot];
    ss_clear_lower();
    ss_gbstr(0, 9,  gCrystalStatsIDNoRaw);
    ss_gbstr(0, 12, gCrystalStatsOTRaw);

    {
        char buf[8];
        snprintf(buf, sizeof buf, "%05u", (unsigned)mon->base.ot_id);
        ss_ascii(2, 10, buf);
    }
    ss_ot_name(2, 13, slot);
    for (int i = 0; i < 10; i++) ss_stile(10, 8 + i, 0x31);

    {
        static const char *const kLbl[5] = {
            "ATTACK", "DEFENSE", "SPCL.ATK", "SPCL.DEF", "SPEED"
        };

        const uint16_t v[5] = { mon->atk, mon->def, mon->spc, mon->spc, mon->spd };
        for (int i = 0; i < 5; i++) {
            ss_ascii(11, 8 + i * 2, kLbl[i]);
            ss_num(17, 9 + i * 2, v[i], 3);
        }
    }
}

static void ss_draw_page1(int slot) {
    if (ss_gen2()) {

        ss_gen2_load_font();
        ss_gen2_upper_half(slot);
        ss_page_indicators(0);
        ss_gen2_apply_pals(slot);
        ss_gen2_page_color(0);
        ss_draw_pink_page(slot);
        return;
    }
    const party_mon_t *mon = &wPartyMons[slot];

    uint8_t dex = Species_Dex(mon->base.species);

    for (int r = 0; r < SCREEN_HEIGHT; r++)
        for (int c = 0; c < SCREEN_WIDTH; c++)
            ss_put(c, r, BLANK_TILE_SLOT);

    ss_line_box(19, 1, 6, 10);

    ss_gb(1, 7, 0x74);
    ss_gb(2, 7, 0xF2);

    if (dex > 0 && dex <= 151) {
        ss_put(3, 7, Font_CharToTile(0xF6 + (dex / 100)));
        ss_put(4, 7, Font_CharToTile(0xF6 + ((dex / 10) % 10)));
        ss_put(5, 7, Font_CharToTile(0xF6 + (dex % 10)));
    }

    ss_nick(9, 1, slot);

    ss_level(14, 2, mon->level);

    ss_hp_bar(11, 3, mon->base.hp, mon->max_hp);

    ss_ascii(9, 6, "STATUS/");
    ss_status(16, 6, mon->base.status, mon->base.hp);

    if (mon->base.hp > 0 && (mon->base.status & 0x7F) == 0) {
        ss_gb(16, 6, 0x8E);
        ss_gb(17, 6, 0x8A);
    }

    ss_box(0, 8, 8, 8);

    ss_ascii(1, 9,  "ATTACK");
    ss_ascii(1, 11, "DEFENSE");
    ss_ascii(1, 13, "SPEED");
    ss_ascii(1, 15, "SPECIAL");

    ss_num(6, 10, mon->atk, 3);
    ss_num(6, 12, mon->def, 3);
    ss_num(6, 14, mon->spd, 3);
    ss_num(6, 16, mon->spc, 3);

    ss_line_box(19, 9, 8, 6);

    ss_ascii(10, 9,  "TYPE1/");
    ss_ascii(10, 11, "TYPE2/");

    ss_gb(10, 13, 0x73);
    ss_gb(11, 13, 0x74);
    ss_gb(12, 13, 0xF3);
    ss_ascii(10, 15, "OT/");

    ss_ascii(11, 10, ss_type_name(mon->base.type1));
    if (mon->base.type2 != mon->base.type1)
        ss_ascii(11, 12, ss_type_name(mon->base.type2));

    ss_num(12, 14, mon->base.ot_id, 5);

    ss_ot_name(12, 16, slot);

    ss_load_sprite(mon->base.species, dex);
}

static void ss_draw_page2(int slot) {
    if (ss_gen2()) {
        ss_page_indicators(1);
        ss_gen2_page_color(1);
        ss_draw_green_page(slot);
        return;
    }
    const party_mon_t *mon = &wPartyMons[slot];

    uint32_t cur_exp = ((uint32_t)mon->base.exp[0] << 16)
                     | ((uint32_t)mon->base.exp[1] <<  8)
                     |  (uint32_t)mon->base.exp[2];

    uint32_t exp_to_next = ss_exp_to_next(mon, cur_exp);

    for (int r = 2; r <= 6; r++)
        for (int c = 9; c <= 18; c++)
            ss_put(c, r, BLANK_TILE_SLOT);

    ss_nick(9, 1, slot);

    ss_gb(19, 3, 0x78);

    ss_ascii(9, 3, "EXP POINTS");
    ss_ascii(9, 5, "LEVEL UP");
    ss_num(12, 4, cur_exp,    7);
    ss_num(7,  6, exp_to_next, 7);

    ss_gb(14, 6, 0x70);
    if (mon->level < 100)
        ss_level(16, 6, (uint8_t)(mon->level + 1));
    else
        ss_level(16, 6, 100);

    ss_box(0, 8, 8, 18);

    for (int i = 0; i < NUM_MOVES; i++) {
        int name_row = 9  + i * 2;
        int pp_row   = 10 + i * 2;
        uint8_t move_id = mon->base.moves[i];

        if (move_id > 0 && move_id < NUM_MOVE_DEFS) {

            const char *name = gMoveNames[move_id];

            int len = (int)strlen(name);
            for (int j = 0; 2 + j <= 18; j++) {
                ss_put(2 + j, name_row,
                       j < len ? ss_ascii_tile((unsigned char)name[j])
                               : BLANK_TILE_SLOT);
            }

            ss_gb(11, pp_row, 0x72);
            ss_gb(12, pp_row, 0x72);

            uint8_t cur_pp  = mon->base.pp[i] & PP_MASK;
            uint8_t pp_ups  = mon->base.pp[i] >> 6;
            uint8_t max_pp  = (uint8_t)(gMoves[move_id].pp * (5 + pp_ups) / 5);
            ss_num(14, pp_row, cur_pp, 2);
            ss_gb( 16, pp_row, 0xF3);
            ss_num(17, pp_row, max_pp, 2);
        } else {

            ss_ascii(2, name_row, "------");
            for (int j = 6; 2 + j <= 18; j++)
                ss_put(2 + j, name_row, BLANK_TILE_SLOT);
            ss_gb(11, pp_row, 0xE3);
            ss_gb(12, pp_row, 0xE3);
            ss_gb(14, pp_row, 0xE3);
            ss_gb(15, pp_row, 0xE3);
            ss_gb(16, pp_row, 0xF3);
            ss_gb(17, pp_row, 0xE3);
            ss_gb(18, pp_row, 0xE3);
        }
    }
}

void SummaryScreen_Open(int slot) {
    if (slot < 0 || slot >= (int)wPartyCount) return;
    Font_LoadHudTiles();

    static const uint8_t kVBarGlyph[16] = {
        0x18,0x18, 0x18,0x18, 0x18,0x18, 0x18,0x18,
        0x18,0x18, 0x18,0x18, 0x18,0x18, 0x18,0x18 };
    static const uint8_t kPTile[16] = {
        0x00,0x00, 0xFC,0xFC, 0xC6,0xC6, 0xC6,0xC6,
        0xC6,0xC6, 0xFC,0xFC, 0xC0,0xC0, 0xC0,0xC0 };

    if (!ss_gen2()) {
        Display_LoadTile((uint8_t)Font_CharToTile(0x72), kPTile);
        Display_LoadTile((uint8_t)Font_CharToTile(0x73), kIdGlyph);
        Display_LoadTile((uint8_t)Font_CharToTile(0x74), kNoGlyph);
        Display_LoadTile((uint8_t)Font_CharToTile(0x78), kVBarGlyph);
    }

    gScrollPxX = 0;
    gScrollPxY = 0;

    Display_SetPalette(0xE4, 0xE4, 0xE4);

    ss_borrow_obj_pal0();
    GbcColor_AutoColorMonPicPal(0);

    s_slot  = slot;

    s_state = SS_PAGE1;
    ss_draw_page1(slot);
    ss_anim_start(slot);
}

int SummaryScreen_IsOpen(void) {
    return s_state != SS_CLOSED;
}

void SummaryScreen_Tick(void) {
    if (s_state == SS_CLOSED) return;

    ss_anim_tick(s_slot);

    if (s_state == SS_PAGE1) {
        if (hJoyPressed & PAD_A) {
            Audio_PlaySFX_PressAB();
            s_state = SS_PAGE2;
            ss_draw_page2(s_slot);
        } else if (hJoyPressed & PAD_B) {
            Audio_PlaySFX_PressAB();
            ss_clear_sprite_oam();
            ss_gen2_close_restore();
            s_state = SS_CLOSED;
        }
    } else if (s_state == SS_PAGE2) {

        if ((hJoyPressed & PAD_A) && ss_gen2()) {
            Audio_PlaySFX_PressAB();
            s_state = SS_PAGE3;
            ss_page_indicators(2);
            ss_gen2_page_color(2);
            ss_draw_blue_page(s_slot);
        } else if ((hJoyPressed & PAD_A) || (hJoyPressed & PAD_B)) {
            Audio_PlaySFX_PressAB();
            ss_clear_sprite_oam();
            ss_gen2_close_restore();
            s_state = SS_CLOSED;
        }
    } else {
        if ((hJoyPressed & PAD_A) || (hJoyPressed & PAD_B)) {
            Audio_PlaySFX_PressAB();
            ss_clear_sprite_oam();
            ss_gen2_close_restore();
            s_state = SS_CLOSED;
        }
    }
}
