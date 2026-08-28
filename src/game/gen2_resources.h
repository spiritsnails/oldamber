#pragma once
#include <stdint.h>

typedef enum {
    GEN2_LAYER_UI = 0,
    GEN2_LAYER_SPRITES,
    GEN2_LAYER_PALETTES,
    GEN2_LAYERS
} gen2_layer_t;

typedef enum {
    GEN2_RES_BG_TILES = 0,
    GEN2_RES_OBJ_TILES,
    GEN2_RES_BG_PAL,
    GEN2_RES_OBJ_PAL,
    GEN2_RES_KINDS
} gen2_res_kind_t;

int  Gen2Res_Borrow(gen2_layer_t layer, gen2_res_kind_t kind,
                    int first, int count, const char *owner);

void Gen2Res_Return(gen2_res_kind_t kind, int first, const char *owner);

void Gen2Res_ReturnAll(const char *owner);

void Gen2Res_ReleaseLayer(gen2_layer_t layer);

void Gen2Res_ReleaseAll(void);

int  Gen2Res_Outstanding(void);

const char *Gen2Res_Describe(int i, int *layer, int *kind,
                             int *first, int *count);
