#pragma once
#include <stdint.h>

#define DISPLAY_SCALE   3

void Display_SetSpeedPct(int pct);

void Display_SetWindowScale(int scale);
int  Display_WindowScale(void);

int  Display_WindowScaleApplies(void);

void Display_RefreshWindowScale(void);

int  Display_Init(void);

void Display_RequestBackendRestart(void);
int  Display_BackendRestartPending(void);
void Display_ApplyBackendRestart(void);

void Display_SetFullscreen(int on);
void Display_ToggleFullscreen(void);
int  Display_IsFullscreen(void);

int  Display_IsSteamDeck(void);

void Display_SuspendFullscreenForOverlay(int suspended);

int  Display_HasInputFocus(void);
void Display_SetDebugRenderMode(int on);

void Display_SetFrameWidth(int px);
int  Display_FrameWidth(void);
int  Display_ContentOriginX(void);

void Display_SetWidescreen(int on);
int  Display_Widescreen(void);
int  Display_WantFrameWidth(int content_supports_wide);

void Display_SetAuthoredFrame(int on);

typedef enum {
    DISPLAY_BOX_EXTEND = 0,
    DISPLAY_BOX_BLACK,
} display_box_fill_t;

void Display_SetLetterboxFrame(int on, display_box_fill_t fill);
int  Display_AuthoredFrame(void);

void Display_SetAuthoredBleedRows(int tile_row, int num_rows);

int  Display_AuthoredRightEdgeCol(void);
void Display_Render(void);

void Display_RenderScrolled(int px, int py, const uint8_t *tile_map, int stride);
void Display_LoadTileset(const uint8_t *gfx, int num_tiles);

int  Display_LoadedTileCount(void);
void Display_LoadTile(uint8_t tile_id, const uint8_t *gfx);
void Display_GetTile(uint8_t tile_id, uint8_t out[16]);
void Display_LoadSpriteTile(uint8_t tile_id, const uint8_t *gfx);

const uint8_t *Display_GetSpriteTile(uint8_t tile_id);
void Display_SetPalette(uint8_t bgp, uint8_t obp0, uint8_t obp1);
void Display_SetBGP(uint8_t bgp);
void Display_SetOBP0(uint8_t obp0);
void Display_SetOBP1(uint8_t obp1);
uint8_t Display_GetBGP(void);
uint8_t Display_GetOBP0(void);
uint8_t Display_GetOBP1(void);

void Display_LoadMapPalette(void);

void Display_SetColorMode(int on);
int  Display_ColorMode(void);
int  Display_GetColorMode(void);

void Display_SetBGColorPalette(int slot, const uint16_t rgb555[4]);
void Display_SetBGColorEntry(int slot, int color, uint16_t rgb555);
void Display_SetOBJColorPalette(int slot, const uint16_t rgb555[4]);

void Display_SetOBJColorPermute(int slot, int dmg_pal);

void Display_SetTileAttr(uint8_t tile_id, uint8_t attr);
uint8_t Display_GetTileAttr(uint8_t tile_id);
void Display_SetTileAttrs(uint8_t first, const uint8_t *attrs, int count);
void Display_FillTileAttrs(uint8_t first, int count, uint8_t attr);

void Display_SetColorFade(int num, int den);

void Display_SetSgbBorderLogicalSize(int on);

void Display_SetSgbFlashCompat(int on);

int  Display_SgbFlashCompat(void);

typedef enum {
    DISPLAY_MONO_PAL_PORT = 0,
    DISPLAY_MONO_PAL_GREY,
    DISPLAY_MONO_PAL_DMG,
    DISPLAY_MONO_PAL_MGB,
    DISPLAY_MONO_PAL_GBL,
    DISPLAY_MONO_PAL_CUSTOM,
    DISPLAY_MONO_PAL_COUNT
} display_mono_pal_t;

void Display_SetMonoPalette(int pal);
int  Display_MonoPalette(void);

void Display_SetCustomShade(int shade, int r, int g, int b);
void Display_GetCustomShade(int shade, int *r, int *g, int *b);

void  Display_SetLightTemperature(float t);
float Display_LightTemperature(void);

int   Display_GetTint(int *r256, int *g256, int *b256);

#define GBC_CURVE_LINEAR          0
#define GBC_CURVE_GAMBATTE        1
#define GBC_CURVE_SAMEBOY_CURVE   2
#define GBC_CURVE_SAMEBOY_MELLOW  3
#define GBC_CURVE_SAMEBOY_HW      4
#define GBC_CURVE_SAMEBOY_SOFT    5

#define GBC_CURVE_LCD_PANEL       6

#define GBC_CURVE_SAMEBOY_REDUCE  7
#define GBC_CURVE_SAMEBOY_HARSH   8
#define GBC_CURVE_SAMEBOY_BOOST   9

#define GBC_CURVE_SAMEBOY_SGB    10

int Display_SgbChannelCurve(int v5);

void Display_SetColorCurve(int curve);
int  Display_GetColorCurve(void);

enum {
    DISPLAY_LCD_GHOSTING_OFF = 0,
    DISPLAY_LCD_GHOSTING_PERSISTENCE,
    DISPLAY_LCD_GHOSTING_SAMEBOY_ACCURATE,
};

void Display_SetLCDGhosting(int on);
int  Display_GetLCDGhosting(void);
void Display_SetLCDGhostingMode(int mode);
int  Display_GetLCDGhostingMode(void);

void Display_SetPositionAttrMode(int on);
int  Display_GetPositionAttrMode(void);

uint16_t Display_GetBGColorEntry(int slot, int color);
uint16_t Display_GetOBJColorEntry(int slot, int color);
uint8_t  Display_GetPositionAttr(int col, int row);
int      Display_GetNumOBJPalettes(void);
void     Display_GetColorFade(int *num, int *den, int *white);

void Display_FillAttrBox(int col, int row, int w, int h, uint8_t attr);
void Display_ClearAttrBoxes(uint8_t attr);

void Display_SetShakeOffset(int ox, int oy);

int  Display_FindDarkestPixel(int x0, int y0, int w, int h, int *out_x, int *out_y);
void Display_ZoomBegin(int focus_x, int focus_y);
void Display_ZoomSetScale(int scale_q8);
void Display_ZoomEnd(void);

void Display_SetWavyPhase(int enabled, int phase);

void Display_SetBandXPx(int row_start, int num_rows, int px);

void Display_SetWindowOverSprites(int on);

int  Display_SaveScreenshot(const char *path);

void Display_SetHiddenWindow(int hidden);

void Display_SetOverlayEnabled(int on);
void Display_ClearOverlay(void);
void Display_SetOverlayTile(int tx, int ty, uint32_t rgba);

void Display_SetBlockIDOverlay(int enabled);
int  Display_GetBlockIDOverlay(void);
void Display_SetBlockIDQueryFn(int (*fn)(int bx, int by));
void Display_SetBlockIDCam(int cam_tx, int cam_ty);

void Display_SetSpeedBadge(const char *label);

void Display_SetSuspendOverlay(const uint32_t *(*compose)(int *w, int *h,
                                                          int *gx, int *gy,
                                                          int *gw, int *gh));

struct SDL_Renderer;

struct SDL_Renderer *Display_GetRenderer(void);

void Display_GetOutputSize(int *w, int *h);

void Display_RestoreLogicalSize(void);

void Display_ApplyScalingMode(void);

void Display_BlitGameFrameTo(uint32_t *dst, int dst_w, int dst_h,
                             int x, int y, int w, int h);
int  Display_SuspendOverlayActive(void);

void Display_SetSuspendDim(int alpha_0_255);

void Display_Quit(void);
