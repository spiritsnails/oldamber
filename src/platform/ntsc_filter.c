
#include "ntsc_filter.h"
#include "crt_core.h"
#include <stdlib.h>
#include <string.h>

static uint32_t        *s_padded;

static struct CRT      *s_crt;
static struct NTSC_SETTINGS s_ntsc;
static uint32_t        *s_out;
static int              s_out_w, s_out_h;
static int              s_on;
static int              s_phase;

static void ntsc_release(void) {
    free(s_crt);    s_crt = NULL;
    free(s_out);    s_out = NULL;
    free(s_padded); s_padded = NULL;
    s_out_w = s_out_h = 0;
}

int NtscFilter_SetEnabled(int on) {
    if (!on) { s_on = 0; return 0; }
    s_on = 1;
    return 1;
}

int NtscFilter_IsEnabled(void) { return s_on; }

const uint32_t *NtscFilter_Apply(const uint32_t *src, int w, int h,
                                 int *out_w, int *out_h) {
    const int dw = w * NTSC_DECODE_SCALE, dh = h * NTSC_DECODE_SCALE;

    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    if (!s_on || !src || w <= 0 || h <= 0) return src;

    if (!s_crt || dw != s_out_w || dh != s_out_h) {
        ntsc_release();
        s_crt = malloc(sizeof *s_crt);
        s_out    = malloc((size_t)dw * dh * sizeof *s_out);
        s_padded = calloc((size_t)w * CRT_LINES, sizeof *s_padded);
        if (!s_crt || !s_out || !s_padded) {
            ntsc_release();
            s_on = 0;
            return src;
        }
        s_out_w = dw;
        s_out_h = dh;

        crt_init(s_crt, dw, dh, CRT_PIX_FORMAT_ABGR, (unsigned char *)s_out);

        memset(&s_ntsc, 0, sizeof s_ntsc);
        s_ntsc.format   = CRT_PIX_FORMAT_ABGR;
        s_ntsc.as_color = 1;

        s_ntsc.raw = 0;

        s_crt->blend = 1;
    }

    {
        int top = (CRT_LINES - h) / 2;
        if (top < 0) top = 0;
        memcpy(&s_padded[(size_t)top * w], src, (size_t)w * h * sizeof *src);
    }
    s_ntsc.data = (const unsigned char *)s_padded;
    s_ntsc.w    = w;
    s_ntsc.h    = CRT_LINES;

    s_ntsc.dot_crawl_offset = s_phase;
    s_phase = (s_phase + 1) % 3;

    crt_modulate(s_crt, &s_ntsc);
    crt_demodulate(s_crt, 0);
    if (out_w) *out_w = s_out_w;
    if (out_h) *out_h = s_out_h;
    return s_out;
}
