
#include <string.h>

#include "gen2_resources.h"
#include "../platform/display.h"

#define MAX_BORROWS 24
#define POOL_BYTES  (384 * 16)

typedef struct {
    uint8_t     used;
    uint8_t     layer;
    uint8_t     kind;
    int         first;
    int         count;
    const char *owner;
    int         off;
    int         len;
} borrow_t;

static borrow_t s_b[MAX_BORROWS];
static uint8_t  s_pool[POOL_BYTES];
static int      s_pool_used;

static int unit_bytes(int kind)
{
    switch (kind) {
    case GEN2_RES_BG_TILES:
    case GEN2_RES_OBJ_TILES: return 16;
    case GEN2_RES_BG_PAL:
    case GEN2_RES_OBJ_PAL:   return 8;
    default:                 return 0;
    }
}

static void snapshot(int kind, int first, int count, uint8_t *dst)
{
    for (int i = 0; i < count; i++) {
        uint8_t *p = dst + i * unit_bytes(kind);
        switch (kind) {
        case GEN2_RES_BG_TILES:
            Display_GetTile((uint8_t)(first + i), p);
            break;
        case GEN2_RES_OBJ_TILES:
            memcpy(p, Display_GetSpriteTile((uint8_t)(first + i)), 16);
            break;
        case GEN2_RES_BG_PAL: {
            uint16_t c[4];
            for (int k = 0; k < 4; k++) c[k] = Display_GetBGColorEntry(first + i, k);
            memcpy(p, c, sizeof c);
            break;
        }
        case GEN2_RES_OBJ_PAL: {
            uint16_t c[4];
            for (int k = 0; k < 4; k++) c[k] = Display_GetOBJColorEntry(first + i, k);
            memcpy(p, c, sizeof c);
            break;
        }
        default: break;
        }
    }
}

static void restore(int kind, int first, int count, const uint8_t *src)
{
    for (int i = 0; i < count; i++) {
        const uint8_t *p = src + i * unit_bytes(kind);
        switch (kind) {
        case GEN2_RES_BG_TILES:
            Display_LoadTile((uint8_t)(first + i), p);
            break;
        case GEN2_RES_OBJ_TILES:
            Display_LoadSpriteTile((uint8_t)(first + i), p);
            break;
        case GEN2_RES_BG_PAL: {
            uint16_t c[4];
            memcpy(c, p, sizeof c);
            Display_SetBGColorPalette(first + i, c);
            break;
        }
        case GEN2_RES_OBJ_PAL: {
            uint16_t c[4];
            memcpy(c, p, sizeof c);
            Display_SetOBJColorPalette(first + i, c);
            break;
        }
        default: break;
        }
    }
}

static int find(int kind, int first, const char *owner)
{
    for (int i = 0; i < MAX_BORROWS; i++)
        if (s_b[i].used && s_b[i].kind == kind && s_b[i].first == first &&
            s_b[i].owner == owner)
            return i;
    return -1;
}

static void drop(int i)
{
    borrow_t *b = &s_b[i];
    int off = b->off, len = b->len;
    memmove(s_pool + off, s_pool + off + len, (size_t)(s_pool_used - off - len));
    s_pool_used -= len;
    for (int j = 0; j < MAX_BORROWS; j++)
        if (s_b[j].used && s_b[j].off > off) s_b[j].off -= len;
    b->used = 0;
}

int Gen2Res_Borrow(gen2_layer_t layer, gen2_res_kind_t kind,
                   int first, int count, const char *owner)
{
    if (count <= 0 || !owner) return 0;
    if ((unsigned)kind >= GEN2_RES_KINDS) return 0;

    if (find(kind, first, owner) >= 0) return 1;

    int len = count * unit_bytes(kind);
    int slot = -1;
    for (int i = 0; i < MAX_BORROWS; i++)
        if (!s_b[i].used) { slot = i; break; }
    if (slot < 0 || len <= 0 || s_pool_used + len > POOL_BYTES)
        return 0;

    borrow_t *b = &s_b[slot];
    b->used  = 1;
    b->layer = (uint8_t)layer;
    b->kind  = (uint8_t)kind;
    b->first = first;
    b->count = count;
    b->owner = owner;
    b->off   = s_pool_used;
    b->len   = len;
    s_pool_used += len;
    snapshot(kind, first, count, s_pool + b->off);
    return 1;
}

void Gen2Res_Return(gen2_res_kind_t kind, int first, const char *owner)
{
    int i = find(kind, first, owner);
    if (i < 0) return;
    restore(s_b[i].kind, s_b[i].first, s_b[i].count, s_pool + s_b[i].off);
    drop(i);
}

void Gen2Res_ReturnAll(const char *owner)
{

    for (int i = MAX_BORROWS - 1; i >= 0; i--)
        if (s_b[i].used && s_b[i].owner == owner) {
            restore(s_b[i].kind, s_b[i].first, s_b[i].count, s_pool + s_b[i].off);
            drop(i);
        }
}

void Gen2Res_ReleaseLayer(gen2_layer_t layer)
{
    for (int i = MAX_BORROWS - 1; i >= 0; i--)
        if (s_b[i].used && s_b[i].layer == (uint8_t)layer) {
            restore(s_b[i].kind, s_b[i].first, s_b[i].count, s_pool + s_b[i].off);
            drop(i);
        }
}

void Gen2Res_ReleaseAll(void)
{
    for (int i = MAX_BORROWS - 1; i >= 0; i--)
        if (s_b[i].used) {
            restore(s_b[i].kind, s_b[i].first, s_b[i].count, s_pool + s_b[i].off);
            drop(i);
        }
    s_pool_used = 0;
}

int Gen2Res_Outstanding(void)
{
    int n = 0;
    for (int i = 0; i < MAX_BORROWS; i++) if (s_b[i].used) n++;
    return n;
}

const char *Gen2Res_Describe(int i, int *layer, int *kind, int *first, int *count)
{
    for (int j = 0; j < MAX_BORROWS; j++) {
        if (!s_b[j].used) continue;
        if (i-- > 0) continue;
        if (layer) *layer = s_b[j].layer;
        if (kind)  *kind  = s_b[j].kind;
        if (first) *first = s_b[j].first;
        if (count) *count = s_b[j].count;
        return s_b[j].owner;
    }
    return 0;
}
