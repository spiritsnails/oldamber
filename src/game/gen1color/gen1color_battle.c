
#include "gen1color_battle.h"
#include "../party_menu.h"
#include "../pokedex.h"
#include "../gen2_species.h"
#include "gen1color_scene.h"
#include "../battle/battle_ui.h"
#include "../battle/battle_exp.h"
#include "../text.h"
#include "../../platform/audio.h"
#include "../constants.h"
#include "../overworld.h"
#include "../pokemon.h"
#include "../party_menu.h"
#include "../gbc_color.h"
#include "../../platform/hardware.h"
#include "../../platform/display.h"
#include "../../data/base_stats.h"
#include "../../data/font_data.h"
#include "crystal_battle_gfx.h"
#include "crystal_mon_pics.h"
#include "crystal_trainer_pics.h"
#include "crystal_pic_anim.h"
#include "../gen2_resources.h"
#include "../../data/pokemon_sprites.h"
#include "../../data/trainer_sprites.h"
#include "../sprite_mod.h"
#include "../trainer_sight.h"
#include "johto_trainers.h"
#include "data/gbc_palettes.h"
#include "../../data/move_anim_tiles.h"
#include "../../data/moves_data.h"
#include <string.h>
#include <stdio.h>

#define G1C_GEN1_DEX_COUNT 152

#define G1C_HPBAR_FIRST_CHAR 0x62

#define G1C_EXP_ROW        11
#define G1C_EXP_RIGHT_COL  17
#define G1C_EXP_TILES       8
#define G1C_EXP_MAX_PX     64

#define G1C_PIC_TILES              49
#define G1C_ENEMY_SPR_TILE_BASE     0
#define G1C_PLAYER_BG_TILE_BASE    53
#define G1C_ENEMY_OAM_BASE         53
#define G1C_PLAYER_SLIDE_OAM_BASE   4
#define G1C_PLAYER_SLIDE_TILE_BASE 53
#define G1C_ANIM_TILE_BASE       0x31

static int s_enabled = 0;

static void g1c_restore_gen1_gfx(void);

static void Gen1Color_ReleaseExpBarBox(void);

void Gen1Color_SetEnabled(int on) {
    on = on ? 1 : 0;
    if (on == s_enabled) return;
    s_enabled = on;
    if (!on) {

        g1c_restore_gen1_gfx();
        Display_SetPositionAttrMode(0);

        CrystalPicAnim_Stop();
        GbcColor_SetBattleMonPalOverride(0, 0);

        GbcColor_SetBattleSuperPalettes(0, 0);

        GbcColor_MarkDirty();

        Gen2Res_ReleaseAll();

        Gen1Color_ReleaseExpBarBox();
    }
}

int Gen1Color_IsEnabled(void) { return s_enabled; }

static int s_sprite_style = G1C_SPRITES_GEN1;

static int g1c_johto_trainer_class(void) {
    int idx = (int)gEngagedJohtoParty - 1;
    if (idx < 0 || idx >= JOHTO_TRAINER_COUNT) return 0;
    int jc = gJohtoTrainers[idx].class_id;
    if (jc <= 0 || jc >= CRYSTAL_TRAINER_CLASS_COUNT) return 0;
    return gCrystalTrainerPicByClassValid[jc] ? jc : 0;
}

void Gen1Color_SetSpriteStyle(int style) {

    if (style < G1C_SPRITES_FIRST || style > G1C_SPRITES_LAST)
        style = G1C_SPRITES_GEN1;
    if (style == s_sprite_style) return;
    s_sprite_style = style;

    CrystalPicAnim_Stop();

    Gen2Res_ReleaseLayer(GEN2_LAYER_SPRITES);

    if (style != G1C_SPRITES_CRYSTAL)
        GbcColor_SetBattleMonPalOverride(0, 0);
}

int Gen1Color_SpriteStyle(void) { return s_sprite_style; }

static int s_ui_style = G1C_UI_GEN1;

void Gen1Color_SetUiStyle(int style) {

    style = (style == G1C_UI_GEN2) ? G1C_UI_GEN2 : G1C_UI_GEN1;
    if (style == s_ui_style) return;
    s_ui_style = style;

    g1c_restore_gen1_gfx();

    Gen2Res_ReleaseLayer(GEN2_LAYER_UI);
    if (style == G1C_UI_GEN1) {

        Display_SetPositionAttrMode(0);
        Display_ClearAttrBoxes(0);
        GbcColor_MarkDirty();
    }
}

int Gen1Color_UiStyle(void) { return s_ui_style; }

static int s_monpal_style = G1C_MONPAL_ENHANCED;

void Gen1Color_SetMonPalStyle(int style) {
    s_monpal_style = (style == G1C_MONPAL_SGB)
                    ? style : G1C_MONPAL_ENHANCED;

    GbcColor_SetMonPalStyle(s_monpal_style);

    if (s_monpal_style == G1C_MONPAL_SGB)
        Display_SetColorCurve(GBC_CURVE_SAMEBOY_HW);
}

int Gen1Color_MonPalStyle(void) { return s_monpal_style; }

static void g1c_crystal_anim_update(void) {
    static int s_prev_black = 0;
    static int s_prev_on_field = 0;

    int black = G1CScene_PaletteBlack();
    int on_field = BattleUI_EnemyMonOnField();
    int is_mon = (BattleUI_EnemyPicKind() == BUI_ENEMY_PIC_MON);
    int edex = Species_Dex(wEnemyMon.species);

    if (s_sprite_style != G1C_SPRITES_CRYSTAL) {
        s_prev_black = black;
        s_prev_on_field = on_field;
        return;
    }

    if (on_field && !s_prev_on_field && is_mon)
        CrystalPicAnim_Start(edex, CRYSTAL_ANIM_SLOW);
    else if (!black && s_prev_black && is_mon && on_field)
        CrystalPicAnim_Start(edex, CRYSTAL_ANIM_NORMAL);

    s_prev_black = black;
    s_prev_on_field = on_field;

    if (!is_mon || !on_field) CrystalPicAnim_Stop();
    CrystalPicAnim_Tick();
}

void Gen1Color_LoadBattleGfx(void) {

    for (int i = 0; i < HPBARANDSTATUSGRAPHICS_TILES; i++) {
        int slot = Font_CharToTile((unsigned char)(G1C_HPBAR_FIRST_CHAR + i));
        Display_LoadTile((uint8_t)slot, gHpBarAndStatusGraphics[i]);
    }

    {
        static const uint8_t kBlankTile[16] = { 0 };
        Display_LoadTile(BLANK_TILE_SLOT, kBlankTile);
    }

    for (int i = 0; i < FONTGRAPHICS_TILES; i++) {
        int slot = Font_CharToTile((unsigned char)(0x80 + i));
        Display_LoadTile((uint8_t)slot, gFontGraphics[i]);
    }

    for (int i = 0; i < BATTLEHUDTILES1_TILES; i++)
        Display_LoadTile((uint8_t)Font_CharToTile((unsigned char)(0x6D + i)),
                         gBattleHudTiles1[i]);
    for (int i = 0; i < BATTLEHUDTILES2_TILES; i++)
        Display_LoadTile((uint8_t)Font_CharToTile((unsigned char)(0x73 + i)),
                         gBattleHudTiles2[i]);
    for (int i = 0; i < BATTLEHUDTILES3_TILES; i++)
        Display_LoadTile((uint8_t)Font_CharToTile((unsigned char)(0x76 + i)),
                         gBattleHudTiles3[i]);

    for (int i = 0; i < EXPBARGRAPHICS_TILES; i++)
        Display_LoadTile((uint8_t)(G1C_EXPBAR_SLOT_BASE + i), gEXPBarGraphics[i]);
}

static void g1c_restore_gen1_gfx(void) {
    Font_Load();
    Font_LoadHudTiles();
}

int Gen1Color_ExpBarPixels(void) {
    const party_mon_t *p = &wPartyMons[wPlayerMonNumber];
    uint8_t species = wBattleMon.species;
    uint8_t dex = Species_Dex(species);
    if (dex == 0 || dex >= 152) return 0;
    uint8_t gr = gBaseStats[dex].growth_rate;
    uint8_t lv = wBattleMon.level;
    if (lv >= MAX_LEVEL) return G1C_EXP_MAX_PX;

    uint32_t base   = CalcExpForLevel(gr, lv);
    uint32_t next   = CalcExpForLevel(gr, (uint8_t)(lv + 1));
    uint32_t curexp = exp_to_u32(p->base.exp);
    if (next <= base) return G1C_EXP_MAX_PX;
    uint32_t cur    = (curexp > base) ? (curexp - base) : 0;
    uint32_t needed = next - base;
    if (cur > needed) cur = needed;
    return (int)((uint64_t)cur * G1C_EXP_MAX_PX / needed);
}

#define G1C_EXP_ST_IDLE   0
#define G1C_EXP_ST_ARMED  1
#define G1C_EXP_ST_FILL   2
#define G1C_EXP_ST_HOLD   3
#define G1C_EXP_HOLD_FRAMES 0x20

static int      s_exp_shown = -1;
static int      s_exp_goal  = 0;
static int      s_exp_state = G1C_EXP_ST_IDLE;
static int      s_exp_hold  = 0;
static uint8_t  s_exp_kind;
static uint8_t  s_exp_to_full;
static int      s_exp_owns_box;
static uint32_t s_exp_cue_seen;
static uint8_t  s_exp_slot;

void Gen1Color_ResetExpBar(void) {
    battleexp_barcue_t cue;
    BattleExp_GetBarCue(&cue);
    s_exp_shown    = -1;
    s_exp_goal     = 0;
    s_exp_state    = G1C_EXP_ST_IDLE;
    s_exp_hold     = 0;
    s_exp_owns_box = 0;
    s_exp_slot     = wPlayerMonNumber;
    s_exp_cue_seen = cue.seq;
}

static void Gen1Color_ReleaseExpBarBox(void) {
    if (!s_exp_owns_box) return;
    s_exp_owns_box = 0;
    s_exp_state    = G1C_EXP_ST_IDLE;
    Text_Close();
}

int Gen1Color_ExpBarPixelsShown(void) {
    return (s_exp_shown < 0) ? Gen1Color_ExpBarPixels() : s_exp_shown;
}

static void g1c_exp_update(void) {
    battleexp_barcue_t cue;
    int target = Gen1Color_ExpBarPixels();

    if (s_ui_style != G1C_UI_GEN2) {
        s_exp_shown    = target;
        s_exp_slot     = wPlayerMonNumber;
        s_exp_state    = G1C_EXP_ST_IDLE;
        s_exp_owns_box = 0;
        BattleExp_GetBarCue(&cue);
        s_exp_cue_seen = cue.seq;
        return;
    }

    BattleExp_GetBarCue(&cue);

    if (s_exp_shown < 0) {
        s_exp_shown    = target;
        s_exp_slot     = wPlayerMonNumber;
        s_exp_cue_seen = cue.seq;
        return;
    }

    if (wPlayerMonNumber != s_exp_slot) {
        s_exp_slot  = wPlayerMonNumber;
        s_exp_shown = target;
        s_exp_state = G1C_EXP_ST_IDLE;
    }

    if (cue.seq != s_exp_cue_seen) {
        s_exp_cue_seen = cue.seq;

        if (cue.slot == wPlayerMonNumber) {
            if (cue.kind == BEXP_ANIM_SETTLE) {
                s_exp_shown = target;
                s_exp_state = G1C_EXP_ST_IDLE;
            } else if (wBattleMon.level < MAX_LEVEL) {

                s_exp_kind    = cue.kind;
                s_exp_to_full = cue.to_full;
                s_exp_state   = G1C_EXP_ST_ARMED;

                s_exp_owns_box = Text_IsOpen();
                if (s_exp_owns_box) Text_HoldAfterPrompt();
            }
        }
    }

    switch (s_exp_state) {
    case G1C_EXP_ST_ARMED:

        if (s_exp_owns_box) {
            if (!Text_IsHeldAfterPrompt()) {

                if (!Text_IsOpen()) s_exp_owns_box = 0;
                break;
            }
        }

        if (s_exp_kind == BEXP_ANIM_LEVEL) {
            s_exp_shown = 0;
            s_exp_goal  = G1C_EXP_MAX_PX;
        } else {
            s_exp_goal  = s_exp_to_full ? G1C_EXP_MAX_PX : target;
        }
        Audio_PlaySFX_HealHP();
        s_exp_hold  = G1C_EXP_HOLD_FRAMES;
        s_exp_state = G1C_EXP_ST_FILL;
        break;

    case G1C_EXP_ST_FILL:
        if (s_exp_shown < s_exp_goal) s_exp_shown++;
        if (s_exp_shown >= s_exp_goal) s_exp_state = G1C_EXP_ST_HOLD;
        break;

    case G1C_EXP_ST_HOLD:
        if (--s_exp_hold > 0) break;
        if (s_exp_owns_box) {
            s_exp_owns_box = 0;
            Text_Close();
        }
        s_exp_state = G1C_EXP_ST_IDLE;
        break;

    default:
        break;
    }
}

static int g1c_hp_bar_color_px(int px) {
    if (px >= 27) return 0;
    if (px >= 10) return 1;
    return 2;
}

static int g1c_hp_bar_color(int side, int hp, int max_hp) {
    int px;
    if (!BattleUI_HpBarAnim(side, &px, NULL, NULL))
        px = G1CScene_HpPixels(hp, max_hp);
    return g1c_hp_bar_color_px(px);
}

static void g1c_apply_palettes(void) {
    int pdex, edex;

    if (G1CScene_PaletteBlack()) {
        GbcColor_SetPalBattleBlack();
        return;
    }

    if (BattleUI_IsEvolutionScreen()) return;

    pdex = Species_Dex(wBattleMon.species);
    edex = Species_Dex(wEnemyMon.species);

    if (G1CScene_RedOnScreen() || !BattleUI_PlayerMonIsOut()) pdex = 0;

    if (BattleUI_EnemyPicKind() == BUI_ENEMY_PIC_TRAINER) edex = 0;

    GbcColor_SetBattleSuperPalettes(0, 0);

    static uint16_t g2pcol[4], g2ecol[4];
    const uint16_t *gen2_pp = 0, *gen2_ep = 0;
    if (pdex >= G1C_GEN1_DEX_COUNT && pdex < CRYSTAL_MON_COUNT) {
        g2pcol[0] = 0x7FFF;  g2pcol[1] = gCrystalMonPalette[pdex][0];
        g2pcol[2] = gCrystalMonPalette[pdex][1];  g2pcol[3] = 0x0000;
        gen2_pp = g2pcol;
    }
    if (edex >= G1C_GEN1_DEX_COUNT && edex < CRYSTAL_MON_COUNT) {
        g2ecol[0] = 0x7FFF;  g2ecol[1] = gCrystalMonPalette[edex][0];
        g2ecol[2] = gCrystalMonPalette[edex][1];  g2ecol[3] = 0x0000;
        gen2_ep = g2ecol;
    }

    static uint16_t jcol[4];
    const uint16_t *johto_ep = 0;
    if (BattleUI_EnemyPicKind() == BUI_ENEMY_PIC_TRAINER) {
        int jc = g1c_johto_trainer_class();
        if (jc > 0) {
            jcol[0] = 0x7FFF;  jcol[1] = gCrystalTrainerPaletteByClass[jc][0];
            jcol[2] = gCrystalTrainerPaletteByClass[jc][1];  jcol[3] = 0x0000;
            johto_ep = jcol;
        }
    }

    const uint16_t *sgb_pp = 0, *sgb_ep = 0;
    if (s_monpal_style == G1C_MONPAL_SGB) {
        if (pdex > 0 && pdex < G1C_GEN1_DEX_COUNT) sgb_pp = GbcColor_MonPaletteRGB(pdex);
        if (edex > 0 && edex < G1C_GEN1_DEX_COUNT) sgb_ep = GbcColor_MonPaletteRGB(edex);
    }

    if (s_sprite_style == G1C_SPRITES_CRYSTAL) {
        static uint16_t pcol[4], ecol[4];
        const uint16_t *pp = 0, *ep = 0;
        if (pdex > 0 && pdex < CRYSTAL_MON_COUNT) {
            pcol[0] = 0x7FFF;  pcol[1] = gCrystalMonPalette[pdex][0];
            pcol[2] = gCrystalMonPalette[pdex][1];  pcol[3] = 0x0000;
            pp = pcol;
        }
        if (edex > 0 && edex < CRYSTAL_MON_COUNT) {
            ecol[0] = 0x7FFF;  ecol[1] = gCrystalMonPalette[edex][0];
            ecol[2] = gCrystalMonPalette[edex][1];  ecol[3] = 0x0000;
            ep = ecol;
        } else if (edex == 0) {

            int tc = gEngagedTrainerClass;
            if (tc > 0 && tc < CRYSTAL_TRAINER_PIC_COUNT
                && gCrystalTrainerPalValid[tc]) {
                ecol[0] = 0x7FFF;  ecol[1] = gCrystalTrainerPalette[tc][0];
                ecol[2] = gCrystalTrainerPalette[tc][1];  ecol[3] = 0x0000;
                ep = ecol;
            }
        }
        if (gen2_pp) pp = gen2_pp;
        if (gen2_ep) ep = gen2_ep;
        if (johto_ep) ep = johto_ep;
        GbcColor_SetBattleMonPalOverride(pp, ep);
    } else {
        GbcColor_SetBattleMonPalOverride(
            gen2_pp ? gen2_pp : sgb_pp,
            johto_ep ? johto_ep : (gen2_ep ? gen2_ep : sgb_ep));
    }

    if (s_ui_style == G1C_UI_GEN1)
        GbcColor_SetPalBattleGen1(pdex, edex, wTrainerClass,
                                  g1c_hp_bar_color(1, wBattleMon.hp, wBattleMon.max_hp),
                                  g1c_hp_bar_color(0, wEnemyMon.hp,  wEnemyMon.max_hp));
    else
        GbcColor_SetPalBattle(pdex, edex, wTrainerClass,
                              g1c_hp_bar_color(1, wBattleMon.hp, wBattleMon.max_hp),
                              g1c_hp_bar_color(0, wEnemyMon.hp,  wEnemyMon.max_hp));
}

static int g1c_anim_palmap_index(void) {

    static int probe = -2;
    if (probe == -2) {
        int n = MOVE_ANIM_TILESET0_TILES < MOVE_ANIM_TILESET1_TILES
              ? MOVE_ANIM_TILESET0_TILES : MOVE_ANIM_TILESET1_TILES;
        probe = -1;
        for (int i = 0; i < n; i++)
            if (memcmp(gMoveAnimTileset0[i], gMoveAnimTileset1[i], 16) != 0) {
                probe = i;
                break;
            }
    }
    if (probe < 0) return -1;
    {
        const uint8_t *cur = Display_GetSpriteTile((uint8_t)(G1C_ANIM_TILE_BASE + probe));
        if (memcmp(cur, gMoveAnimTileset1[probe], 16) == 0) return 1;
        if (memcmp(cur, gMoveAnimTileset0[probe], 16) == 0) return 0;
    }
    return -1;
}

static int g1c_anim_move_type(void) {
    uint8_t mv = hWhoseTurn ? wEnemyMoveNum : wPlayerMoveNum;
    int type = (mv < NUM_MOVE_DEFS) ? (int)gMoves[mv].type : -1;
    return GbcColor_AnimTypeForMove(mv, type);
}

static void g1c_stamp_anim_palettes(void) {

    if (G1CScene_RedOnScreen()) return;

    int map = g1c_anim_palmap_index();
    if (map < 0) return;

    for (uint16_t e = BattleUI_GetAnimOAMStart(); e <= BattleUI_GetAnimOAMEnd(); e++) {
        if (e >= MAX_SPRITES) break;
        if (wShadowOAM[e].y == 0) continue;

        uint8_t t = wShadowOAM[e].tile;
        int pal;

        int rom = BattleUI_PoofRomTile(t);
        if (rom >= 0) t = (uint8_t)(G1C_ANIM_TILE_BASE + rom);
        else if (t < G1C_ANIM_TILE_BASE || t >= GBC_ANIM_TILESET_PAL_MAP_SIZE) continue;

        pal = GbcColor_AnimSpritePalette(map, t, g1c_anim_move_type());
        if (pal < 0) continue;
        wShadowOAM[e].flags = (uint8_t)((wShadowOAM[e].flags & ~GBC_OBJ_PAL_MASK)
                                        | (uint8_t)(pal & GBC_OBJ_PAL_MASK));
    }
}

static void g1c_stamp_obj_palettes(void) {
    for (int i = 0; i < G1C_PIC_TILES; i++) {
        int e = G1C_ENEMY_OAM_BASE + i;
        if (e >= MAX_SPRITES) break;
        wShadowOAM[e].flags = (uint8_t)((wShadowOAM[e].flags & ~GBC_OBJ_PAL_MASK)
                                        | GBC_OBJ_PAL_ENEMY_MON);
    }

    if (G1CScene_RedOnScreen()) {
        for (int i = 0; i < G1C_PIC_TILES; i++) {
            int p = G1C_PLAYER_SLIDE_OAM_BASE + i;
            if (p >= MAX_SPRITES) break;
            if (wShadowOAM[p].y == 0) continue;
            if (wShadowOAM[p].tile != (uint8_t)(G1C_PLAYER_SLIDE_TILE_BASE + i)) continue;

            uint8_t pal = (uint8_t)GBC_OBJ_PAL_PLAYER_MON;
            if (GbcColor_BattleAutoColor() && G1CScene_PaletteBlack() && i < 7 * 3)
                pal = 0u;
            wShadowOAM[p].flags = (uint8_t)((wShadowOAM[p].flags & ~GBC_OBJ_PAL_MASK)
                                            | pal);
        }
    }
}

static void g1c_load_mon_pics(void) {

    uint8_t pic_species = BattleUI_EnemyPicSpecies();
    int edex = Species_Dex(pic_species ? pic_species : wEnemyMon.species);
    int pdex = Species_Dex(wBattleMon.species);

    if (s_sprite_style == G1C_SPRITES_GEN1) {
        switch (BattleUI_EnemyPicKind()) {
        case BUI_ENEMY_PIC_MON:
            if (edex > 0 && edex < 152) {
                for (int i = 0; i < G1C_PIC_TILES; i++) {
                    const uint8_t *t = SpriteMod_GetFrontTile(wEnemyMon.species, i);
                    Display_LoadSpriteTile((uint8_t)(G1C_ENEMY_SPR_TILE_BASE + i),
                                           t ? t : gPokemonFrontSprite[edex][i]);
                }
            } else if (edex >= 152 && edex < CRYSTAL_MON_COUNT) {

                const crystal_pic_t *p = &gCrystalMonPic[edex];
                for (int i = 0; i < G1C_PIC_TILES; i++)
                    Display_LoadSpriteTile((uint8_t)(G1C_ENEMY_SPR_TILE_BASE + i),
                                           gCrystalPicTiles[p->tile_base + i]);
            }
            break;
        case BUI_ENEMY_PIC_TRAINER: {
            int tc = (int)gEngagedTrainerClass - 1;
            if (tc >= 0 && tc < NUM_TRAINERS)
                for (int i = 0; i < TRAINER_CANVAS_TILES; i++)
                    Display_LoadSpriteTile((uint8_t)(G1C_ENEMY_SPR_TILE_BASE + i),
                                           gTrainerFrontSprite[tc][i]);
            break;
        }
        default:
            break;
        }

        if (!BattleUI_IsEvolutionScreen() && BattleUI_PlayerMonIsOut()) {
            if (pdex > 0 && pdex < 152) {
                for (int i = 0; i < G1C_PIC_TILES; i++) {
                    const uint8_t *t = SpriteMod_GetBackTile(wBattleMon.species, i);
                    Display_LoadTile((uint8_t)(G1C_PLAYER_BG_TILE_BASE + i),
                                     t ? t : gPokemonBackSprite[pdex][i]);
                }
            } else if (pdex >= 152 && pdex < CRYSTAL_MON_COUNT) {
                for (int i = 0; i < G1C_PIC_TILES; i++)
                    Display_LoadTile((uint8_t)(G1C_PLAYER_BG_TILE_BASE + i),
                                     gCrystalMonBackPic[pdex][i]);
            }
        }

        if (G1CScene_RedOnScreen())
            for (int i = 0; i < 49; i++)
                Display_LoadSpriteTile((uint8_t)(G1C_PLAYER_SLIDE_TILE_BASE + i),
                                       BattleUI_PlayerBackTile(i));
        return;
    }

    switch (BattleUI_EnemyPicKind()) {
    case BUI_ENEMY_PIC_MON:

        if ((s_sprite_style == G1C_SPRITES_CRYSTAL || edex >= G1C_GEN1_DEX_COUNT)
            && edex > 0 && edex < CRYSTAL_MON_COUNT) {

            const crystal_pic_t *p = &gCrystalMonPic[edex];
            const uint8_t *map = CrystalPicAnim_FrameMap(edex);
            static const uint8_t blank[16] = {0};
            for (int i = 0; i < G1C_PIC_TILES; i++) {
                int t = map ? map[i] : i;
                const uint8_t *px = (t < p->tile_count)
                                  ? gCrystalPicTiles[p->tile_base + t] : blank;
                Display_LoadSpriteTile((uint8_t)(G1C_ENEMY_SPR_TILE_BASE + i), px);
            }
        }

        break;
    case BUI_ENEMY_PIC_TRAINER: {

        int tc = gEngagedTrainerClass;

        int jc = g1c_johto_trainer_class();
        if (jc > 0) {

            for (int i = 0; i < G1C_PIC_TILES; i++)
                Display_LoadSpriteTile((uint8_t)(G1C_ENEMY_SPR_TILE_BASE + i),
                                       gCrystalTrainerPicByClass[jc][i]);
        } else if (s_sprite_style == G1C_SPRITES_CRYSTAL
            && tc > 0 && tc < CRYSTAL_TRAINER_PIC_COUNT
            && gCrystalTrainerPicValid[tc]) {
            for (int i = 0; i < G1C_PIC_TILES; i++)
                Display_LoadSpriteTile((uint8_t)(G1C_ENEMY_SPR_TILE_BASE + i),
                                       gCrystalTrainerPic[tc][i]);
        }

        break;
    }
    default:
        break;
    }

    if (!BattleUI_IsEvolutionScreen() && BattleUI_PlayerMonIsOut() &&
        (s_sprite_style == G1C_SPRITES_CRYSTAL || pdex >= G1C_GEN1_DEX_COUNT)
        && pdex > 0 && pdex < CRYSTAL_MON_COUNT) {
        for (int i = 0; i < G1C_PIC_TILES; i++)
            Display_LoadTile((uint8_t)(G1C_PLAYER_BG_TILE_BASE + i),
                             gCrystalMonBackPic[pdex][i]);
    }

    if (G1CScene_RedOnScreen()) {
        for (int i = 0; i < G1C_PIC_TILES; i++)
            Display_LoadSpriteTile((uint8_t)(G1C_PLAYER_SLIDE_TILE_BASE + i),
                                   BattleUI_PlayerBackTile(i));
    }
}

void Gen1Color_Tick(void) {
    if (!s_enabled) return;

    {
        static int s_was_in_battle = 0;
        int in = BattleUI_IsActive();
        if (in && !s_was_in_battle) {
            G1CScene_ResetIntro();
            Gen1Color_ResetExpBar();
            CrystalPicAnim_Stop();
        }
        s_was_in_battle = in;

        if (!in) { CrystalPicAnim_StopOwner(CRYSTAL_ANIM_OWNER_BATTLE); return; }
    }

    if (PartyMenu_IsOpen() || Pokedex_IsShowingData()) return;

    if (s_ui_style == G1C_UI_GEN2) Gen1Color_LoadBattleGfx();

    g1c_crystal_anim_update();
    g1c_load_mon_pics();

    g1c_exp_update();

    if (s_ui_style == G1C_UI_GEN1) G1CScene_DrawGen1();
    else                           G1CScene_Draw();

    BattleUI_SetPlayerNameCentered(s_ui_style == G1C_UI_GEN1);

    if (!GbcColor_IsEnabled()) return;

    g1c_apply_palettes();

    if (!GbcColor_BattleAutoColor()) {
        GbcColor_LoadAttackPalettes();
        g1c_stamp_anim_palettes();
    }
    g1c_stamp_obj_palettes();
}
