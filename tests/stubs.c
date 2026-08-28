
#include <stdint.h>
#include "anim_sfx_capture.h"
#include "data/move_sfx_structs.h"
#include "assetpack_bind.h"
#include "sfx_names.h"
#include "platform/audio.h"
#include <string.h>

uint8_t gTestDisplayLastBGP = 0;
uint8_t gTestDisplayLastOBP0 = 0;
uint8_t gTestDisplayLastOBP1 = 0;
uint8_t gTestDisplayBGPHistory[256];
int gTestDisplayBGPHistoryCount = 0;

int  Display_Init(void)  { return 0; }
void Display_Quit(void)  {}
void Display_Render(void) {}

int  Display_FrameWidth(void)   { return 160; }
int  Display_ContentOriginX(void) { return 0; }
void Display_SetFrameWidth(int px) { (void)px; }

int  Display_AuthoredFrame(void)      { return 0; }
void Display_SetAuthoredFrame(int on) { (void)on; }
void Display_LoadTileset(const uint8_t *g, int n) { (void)g; (void)n; }
void Display_LoadTile(uint8_t id, const uint8_t *g) { (void)id; (void)g; }
void Display_LoadSpriteTile(uint8_t id, const uint8_t *g) { (void)id; (void)g; }
void Display_SetPalette(uint8_t a, uint8_t b, uint8_t c) {
    gTestDisplayLastBGP = a;
    gTestDisplayLastOBP0 = b;
    gTestDisplayLastOBP1 = c;
}
void Display_GetTile(uint8_t id, uint8_t out[16]) { (void)id; (void)out; }
const uint8_t *Display_GetSpriteTile(uint8_t tile_id) { (void)tile_id; return NULL; }
uint16_t Display_GetBGColorEntry(int slot, int color) { (void)slot; (void)color; return 0; }
uint16_t Display_GetOBJColorEntry(int slot, int color) { (void)slot; (void)color; return 0; }
int Display_ColorMode(void) { return 0; }
void Display_RenderScrolled(int px, int py, const uint8_t *m, int s) { (void)px;(void)py;(void)m;(void)s; }
void Display_SetOverlayEnabled(int on) { (void)on; }
void Display_ClearOverlay(void) {}
void Display_SetOverlayTile(int tx, int ty, uint32_t rgba) { (void)tx;(void)ty;(void)rgba; }

int gTestDisplayShakeX = 0;
int gTestDisplayShakeY = 0;
void Display_SetShakeOffset(int ox, int oy) {
    gTestDisplayShakeX = ox;
    gTestDisplayShakeY = oy;
}
int  Display_SaveScreenshot(const char *path) { (void)path; return 0; }
void Display_SetBGP(uint8_t bgp) {
    gTestDisplayLastBGP = bgp;
    if (gTestDisplayBGPHistoryCount < (int)sizeof(gTestDisplayBGPHistory)) {
        gTestDisplayBGPHistory[gTestDisplayBGPHistoryCount++] = bgp;
    }
}

void Display_SetOBP0(uint8_t obp0) { gTestDisplayLastOBP0 = obp0; }
void Display_LoadMapPalette(void) {}
void Display_SetBandXPx(int row_start, int num_rows, int px) { (void)row_start;(void)num_rows;(void)px; }
void Display_SetWindowOverSprites(int on) { (void)on; }
void Display_SetWavyPhase(int enabled, int phase) { (void)enabled; (void)phase; }
void Display_SetOBP1(uint8_t obp1) { (void)obp1; }
void Display_SetOBJColorPermute(int slot, int dmg_pal) { (void)slot; (void)dmg_pal; }

void Display_SetWindowTile(int col, int row, uint8_t tile) { (void)col;(void)row;(void)tile; }

void Display_SetColorMode(int on) { (void)on; }
int  Display_GetColorMode(void) { return 0; }
void Display_SetColorCurve(int curve) { (void)curve; }
int  Display_GetColorCurve(void) { return 0; }
void Display_SetBGColorPalette(int slot, const uint16_t rgb555[4]) { (void)slot; (void)rgb555; }
void Display_SetBGColorEntry(int slot, int color, uint16_t rgb555) { (void)slot;(void)color;(void)rgb555; }
void Display_SetOBJColorPalette(int slot, const uint16_t rgb555[4]) { (void)slot; (void)rgb555; }
void Display_SetTileAttr(uint8_t tile_id, uint8_t attr) { (void)tile_id; (void)attr; }
uint8_t Display_GetTileAttr(uint8_t tile_id) { (void)tile_id; return 0; }
void Display_SetTileAttrs(uint8_t first, const uint8_t *attrs, int count) { (void)first;(void)attrs;(void)count; }
void Display_FillTileAttrs(uint8_t first, int count, uint8_t attr) { (void)first;(void)count;(void)attr; }
void Display_SetColorFade(int num, int den) { (void)num; (void)den; }
void Display_SetPositionAttrMode(int on) { (void)on; }
int  Display_GetPositionAttrMode(void) { return 0; }
void Display_FillAttrBox(int col, int row, int w, int h, uint8_t attr) { (void)col;(void)row;(void)w;(void)h;(void)attr; }
void Display_ClearAttrBoxes(uint8_t attr) { (void)attr; }

int  Audio_Init(void)  { return 0; }
void Audio_Quit(void)  {}
void Audio_Update(void) {}
void Audio_UpdateMusic(void) {}
void Audio_UpdateSfx(void) {}
void Audio_SetLowHealthAlarm(int on) { (void)on; }
void Audio_DisableLowHealthAlarm(void) {}
void Audio_ResetLowHealthAlarm(void) {}
void Audio_WriteReg(int ch, int reg, uint8_t val) { (void)ch;(void)reg;(void)val; }
void Audio_WriteNR50(uint8_t v) { (void)v; }
void Audio_WriteNR51(uint8_t v) { (void)v; }
void Audio_SetWaveInstrument(int idx) { (void)idx; }
void Audio_SetWaveRaw(const uint8_t pattern[16]) { (void)pattern; }
void Audio_SetMixVolume(uint8_t level) { (void)level; }
void Audio_SetMixVolumeImmediate(uint8_t level) { (void)level; }
float Audio_GetMixLevel(void) { return 1.0f; }
void Audio_PlaySFX_PressAB(void) {}
void Audio_PlaySFX_Ledge(void) {}
void Audio_PlaySFX_GoInside(void) {}
void Audio_PlaySFX_GoOutside(void) {}
void Audio_PlaySFX_StartMenu(void) {}
void Audio_PlaySFX_TurnOnPC(void) {}
void Audio_PlaySFX_EnterPC(void) {}
void Audio_PlaySFX_TurnOffPC(void) {}
void Audio_PlaySFX_WithdrawDeposit(void) {}
void Audio_PlaySFX_BattleHit(uint8_t dmg_mult) { (void)dmg_mult; }
void Audio_PlaySFX_SilphScope(void) {}
void Audio_PlaySFX_BallToss(void) {}
void Audio_PlaySFX_Shrink(void) {}
void Audio_PlaySFX_CaughtMon(void) {}
void Audio_PlaySFX_DexRating(void) {}
void Audio_PlaySFX_HealAilment(void) {}
void Audio_PlaySFX_GetItem2(void) {}
void Audio_PlaySFX_SlotsNewSpin(void) {}
void Audio_PlaySFX_SlotsStopWheel(void) {}
void Audio_PlaySFX_SlotsReward(void) {}
void Audio_PlaySFX_Battle24(void) {}
void Audio_PlaySFX_Battle28(void) {}
void Audio_PlaySFX_Battle29(void) {}
void Audio_PlaySFX_Battle2A(void) {}
void Audio_PlaySFX_Battle0D(void) {}
void Audio_PlaySFX_FaintFallOnly(void) {}

anim_sfx_event_t gAnimSfxEvents[ANIM_SFX_MAX];
int gAnimSfxCount = 0;

static void anim_sfx_record(const char *symbol, int is_cry, uint8_t species,
                            int8_t pitch, uint8_t tempo) {
    if (gAnimSfxCount >= ANIM_SFX_MAX) return;
    gAnimSfxEvents[gAnimSfxCount].symbol = symbol;
    gAnimSfxEvents[gAnimSfxCount].is_cry = is_cry;
    gAnimSfxEvents[gAnimSfxCount].species = species;
    gAnimSfxEvents[gAnimSfxCount].pitch = pitch;
    gAnimSfxEvents[gAnimSfxCount].tempo = tempo;
    gAnimSfxCount++;
}

int gAnimSfxSimulate = 0;
int gAnimSfxRemaining = 0;
static int sAnimSfxCh[4];

static int anim_sfx_note_frames(int len, unsigned tempo) {
    int f = (int)(((unsigned)(len + 1) * tempo) >> 8);
    return f > 0 ? f : 1;
}

static int anim_sfx_channel_frames(const sfx_channel_t *ch, unsigned tempo) {
    int total = 0, guard = 0;
    int loop_rem[64];
    uint16_t pos = 0;
    int i;
    for (i = 0; i < 64; i++) loop_rem[i] = -1;

    while (pos < ch->cmd_count && guard++ < 20000) {
        const move_sfx_cmd_t *c;
        if ((uint32_t)(ch->cmd_first + pos) >= gSfxCmds_count) break;
        c = &gSfxCmds[ch->cmd_first + pos];
        if (c->type == MOVE_SFX_CMD_SQUARE_NOTE || c->type == MOVE_SFX_CMD_NOISE_NOTE) {
            total += anim_sfx_note_frames((int)c->p0, tempo);
            pos++;
        } else if (c->type == MOVE_SFX_CMD_SOUND_LOOP) {
            int count = (int)c->p0, target = (int)c->p1;
            if (count == 0) break;
            if (pos < 64 && loop_rem[pos] < 0) loop_rem[pos] = count - 1;
            if (pos < 64 && loop_rem[pos] > 0) {
                loop_rem[pos]--;
                pos = (uint16_t)(target >= 0 ? target : 0);
                continue;
            }
            if (pos < 64) loop_rem[pos] = -1;
            pos++;
        } else if (c->type == MOVE_SFX_CMD_SOUND_RET) {
            break;
        } else {
            pos++;
        }
    }
    return total;
}

static void anim_sfx_start(uint16_t sfx_index, unsigned tempo) {
    const sfx_def_t *def;
    int c;
    if (sfx_index >= gSfxDefs_count) return;
    def = &gSfxDefs[sfx_index];
    for (c = 0; c < (int)def->channel_count; c++) {
        const sfx_channel_t *chd;
        int hw;
        if ((uint32_t)(def->chan_first + c) >= gSfxChannels_count) break;
        chd = &gSfxChannels[def->chan_first + c];
        hw = (int)chd->hw_channel - 5;
        if (chd->hw_channel == 8u) hw = 3;
        if (hw < 0 || hw > 3) continue;

        sAnimSfxCh[hw] = anim_sfx_channel_frames(chd, tempo) + 1;
    }
}

void AnimSfx_AdvanceFrame(void) {
    int c, best = 0;
    for (c = 0; c < 4; c++) {
        if (sAnimSfxCh[c] > 0) sAnimSfxCh[c]--;
        if (sAnimSfxCh[c] > best) best = sAnimSfxCh[c];
    }
    gAnimSfxRemaining = best;
}

int Audio_IsMoveSFXPlaying(void) {
    int c;
    for (c = 0; c < 4; c++) if (sAnimSfxCh[c] > 0) return 1;
    return 0;
}

static const char *anim_sfx_name(uint16_t sfx_index) {
    return sfx_index < SFX_NAME_COUNT ? kSfxNames[sfx_index] : "?";
}

int Audio_PlaySfx(uint16_t sfx_index) {
    return Audio_PlaySfxModified(sfx_index, 0, 0);
}

int Audio_PlaySfxModified(uint16_t sfx_index, int8_t pitch_add, uint8_t tempo_mod) {
    anim_sfx_record(anim_sfx_name(sfx_index), 0, 0, pitch_add, tempo_mod);
    if (gAnimSfxSimulate) {
        anim_sfx_start(sfx_index, (unsigned)tempo_mod + 0x80u);
    }
    return 1;
}

int DebugCLI_IsAutoWinEnabled(void) { return 0; }

#ifndef BATTLE_UI_REAL
void BagMenu_Tick(void) {}
int BagMenu_IsOpen(void) { return 0; }
void BagMenu_OpenBattle(void) {}
uint8_t BagMenu_GetSelected(void) { return 0; }
#endif
void Audio_SetMoveSfxDebug(int on) { (void)on; }
int  Audio_IsMoveSfxDebug(void) { return 0; }
void Audio_PlaySFX_BallPoof(void) {}
void Audio_PlaySFX_Faint(void) {}
void Audio_PlaySFX_Run(void) {}
void Audio_PlaySFX_Cut(void) {}
void Audio_PlaySFX_PushBoulder(void) {}
void Audio_PlaySFX_Switch(void) {}
void Audio_PlaySFX_Swap(void) {}
void Audio_PlaySFX_TeleportExit1(void) {}
void Audio_PlaySFX_TeleportExit2(void) {}
void Audio_PlaySFX_TeleportEnter1(void) {}
void Audio_PlaySFX_TeleportEnter2(void) {}
void Audio_PlaySFX_ArrowTiles(void) {}
void Audio_PlaySFX_Denied(void) {}
void Audio_PlaySFX_HealingMachine(void) {}
void Audio_PlaySFX_LevelUp(void) {}
void Audio_PlaySFX_Purchase(void) {}
void Audio_PlaySFX_Collision(void) {}
void Audio_PlaySFX_CollisionRetrigger(void) {}

int  Game_GetScene(void)           { return 0; }
int  Game_BeginTradeAnim(void)     { return 0; }
int  Game_BeginFieldEvolution(void){ return 0; }

void Audio_PlaySFX_SafariZonePA(void) {}
void Audio_PlaySFX_TradeMachine(void) {}
void Audio_PlaySFX_HealHP(void) {}
void Audio_PlaySFX_Tink(void) {}
void Audio_PlaySFX_GetKeyItem(void) {}
int  Audio_IsSFXPlaying_GetKeyItem(void) { return 0; }
void Audio_PlaySFX_SSAnneHorn(void) {}
int  Audio_IsSFXPlaying_SSAnneHorn(void) { return 0; }
int  Audio_IsSFXPlaying(void)            { return 0; }
void Audio_PlayCry(uint8_t species) { (void)species; }
void Audio_PlayCryModified(uint8_t species, int8_t pitch_add, uint8_t tempo_add) {

    anim_sfx_record("CRY", 1, species, pitch_add, tempo_add);
}
int  Audio_IsCryPlaying(void) { return 0; }

int  Audio_StillSounding(void) { return 0; }

int  Save_Load(void) { return -1; }
int  Save_Save(void) { return  0; }

void Input_Init(void)  {}
void Input_Quit(void)  {}
void Input_Update(void) {}

void Audio_PlaySFX_GetItem1(void) {}

void Display_SetBlockIDOverlay(int e) { (void)e; }
int  Display_GetBlockIDOverlay(void)  { return 0; }
void Display_SetBlockIDQueryFn(int (*fn)(int bx, int by)) { (void)fn; }
void Display_SetBlockIDCam(int tx, int ty) { (void)tx; (void)ty; }
