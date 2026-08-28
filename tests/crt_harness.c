/* SPDX-License-Identifier: MIT
 *
 * crt_harness.c, headless measurement harness for the CRT renderer.
 * Original OldAmber code.
 *
 * The properties measured here are the ones that do not need a camera, a
 * reference display, or an opinion: black stays black, neutral gray stays
 * neutral under both mask orders, the mask does not change mean brightness, no
 * channel clips, and the render cost is known at each output resolution.
 *
 * It renders with the real shaders on the real GPU and reads the result back.
 * A CPU reimplementation of the shader maths would only test the copy, which is
 * how a shader that halved the brightness of every pixel once passed a green
 * build: nothing had looked at a rendered pixel. test_mask_energy and
 * test_corner_radius_brightness are the two that would catch that.
 *
 * Usage:
 *     crt_harness              measure and assert; exit 1 on any failure
 *     crt_harness --perf       also time the passes at several viewport sizes
 *     crt_harness --dump DIR   write each measured frame as a BMP for eyeballing
 */

#include "../src/platform/crt_renderer.h"
#include "../src/platform/crt_renderer_gl.h"
#include "../src/platform/gl_api.h"

#include <SDL.h>

#ifdef main
#  undef main
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SRC_W 256
#define SRC_H 224

static SDL_Window   *s_window;
static SDL_GLContext s_ctx;
static GLuint        s_src_tex;
static int           s_vp_w, s_vp_h;
static int           s_fail;
static const char   *s_dump_dir;

static char s_shader_dir[512];

const char *DisplayGL_ShaderDir(void) {
    FILE *f;
    char probe[600];
    const char *cands[] = { "shaders/", "../shaders/", "../../shaders/" };
    size_t i;

    if (s_shader_dir[0]) return s_shader_dir;
    for (i = 0; i < sizeof cands / sizeof cands[0]; i++) {
        snprintf(probe, sizeof probe, "%scrt/tube.frag", cands[i]);
        f = fopen(probe, "rb");
        if (f) {
            fclose(f);
            snprintf(s_shader_dir, sizeof s_shader_dir, "%s", cands[i]);
            return s_shader_dir;
        }
    }
    return NULL;
}

static void check(int ok, const char *what) {
    printf("  %-58s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) s_fail = 1;
}

static void checkf(int ok, const char *fmt, double a, double b) {
    char buf[160];
    snprintf(buf, sizeof buf, fmt, a, b);
    check(ok, buf);
}

static void upload_flat(int r, int g, int b) {
    static uint32_t px[SRC_W * SRC_H];
    int i;

    uint32_t v = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | 0xFF000000u;
    for (i = 0; i < SRC_W * SRC_H; i++) px[i] = v;

    glBindTexture(GL_TEXTURE_2D, s_src_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SRC_W, SRC_H, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void upload_bright_square(int level, int half) {
    static uint32_t px[SRC_W * SRC_H];
    uint32_t on  = (uint32_t)level | ((uint32_t)level << 8) |
                   ((uint32_t)level << 16) | 0xFF000000u;
    uint32_t off = 0xFF000000u;
    int x, y;

    for (y = 0; y < SRC_H; y++) {
        for (x = 0; x < SRC_W; x++) {
            int inside = (x >= SRC_W / 2 - half && x < SRC_W / 2 + half &&
                          y >= SRC_H / 2 - half && y < SRC_H / 2 + half);
            px[y * SRC_W + x] = inside ? on : off;
        }
    }
    glBindTexture(GL_TEXTURE_2D, s_src_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SRC_W, SRC_H, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static const crt_frame_desc_t *s_desc_override;

static double s_clock;

static void fill_desc(crt_frame_desc_t *d) {
    if (s_desc_override) { *d = *s_desc_override; return; }
    memset(d, 0, sizeof *d);
    d->texture_width  = SRC_W;
    d->texture_height = SRC_H;
    d->raster_width   = SRC_W;
    d->raster_height  = SRC_H;
    d->texture_active_rect = (crt_rect_i_t){ 0, 0, SRC_W, SRC_H };
    d->raster_active_rect  = (crt_rect_i_t){ 0, 0, SRC_W, SRC_H };
    d->pixel_aspect_num = 8;
    d->pixel_aspect_den = 7;
    d->scan_mode      = CRT_SCAN_PROGRESSIVE;
    d->field          = CRT_FIELD_NONE;
    d->input_transfer = CRT_TRANSFER_SRGB;
    d->input_gamma    = 2.2f;
    d->source_refresh_hz = 59.7275f;
    d->frame_number   = 1;
}

static double decode_srgb(double code) {
    double c = code / 255.0;
    if (c <= 0.04045) return c / 12.92;
    return pow((c + 0.055) / 1.055, 2.4);
}

typedef struct {
    double r, g, b;
    double lr, lg, lb;
    double max_r, max_g, max_b;
    long   clipped;
    long   samples;
} measure_t;

static void dump_bmp(const char *name, const unsigned char *rgba, int w, int h);
static void glBindFramebuffer_compat(void);

static double s_region[4] = { 0.25, 0.25, 0.75, 0.75 };

static void set_region(double x0, double y0, double x1, double y1) {
    s_region[0] = x0; s_region[1] = y0; s_region[2] = x1; s_region[3] = y1;
}

static void reset_region(void) { set_region(0.25, 0.25, 0.75, 0.75); }

static int render_and_measure(const char *label, measure_t *out) {
    static unsigned char *buf;
    crt_frame_desc_t desc;
    int x0, y0, x1, y1, x, y;
    double sr = 0, sg = 0, sb = 0;
    long n = 0;

    fill_desc(&desc);

    glBindFramebuffer_compat();
    glViewport(0, 0, s_vp_w, s_vp_h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    if (CrtRendererGL_Draw(s_src_tex, &desc, 0, 0, s_vp_w, s_vp_h,
                           s_clock) != 1) {
        printf("  %-58s %s\n", label, "DRAW FAILED");
        s_fail = 1;
        return 0;
    }
    glFinish();

    {
        static size_t cap;
        size_t need = (size_t)s_vp_w * s_vp_h * 4;
        if (need > cap) {
            unsigned char *nb = (unsigned char *)realloc(buf, need);
            if (!nb) return 0;
            buf = nb;
            cap = need;
        }
    }
    if (!buf) return 0;
    glReadPixels(0, 0, s_vp_w, s_vp_h, GL_RGBA, GL_UNSIGNED_BYTE, buf);

    if (s_dump_dir && label) dump_bmp(label, buf, s_vp_w, s_vp_h);

    memset(out, 0, sizeof *out);
    x0 = (int)(s_region[0] * s_vp_w); x1 = (int)(s_region[2] * s_vp_w);
    y0 = (int)(s_region[1] * s_vp_h); y1 = (int)(s_region[3] * s_vp_h);
    if (x1 <= x0 || y1 <= y0) return 0;
    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            const unsigned char *p = buf + ((size_t)y * s_vp_w + x) * 4;

            out->lr += decode_srgb(p[0]);
            out->lg += decode_srgb(p[1]);
            out->lb += decode_srgb(p[2]);
            sr += p[0]; sg += p[1]; sb += p[2];
            if (p[0] > out->max_r) out->max_r = p[0];
            if (p[1] > out->max_g) out->max_g = p[1];
            if (p[2] > out->max_b) out->max_b = p[2];
            if (p[0] == 255 || p[1] == 255 || p[2] == 255) out->clipped++;
            n++;
        }
    }
    out->samples = n;
    out->r = sr / n; out->g = sg / n; out->b = sb / n;
    out->lr /= n; out->lg /= n; out->lb /= n;
    return 1;
}

static double luma_of(const measure_t *m) {
    return 0.2126 * m->r + 0.7152 * m->g + 0.0722 * m->b;
}

static double linear_luma_of(const measure_t *m) {
    return 0.2126 * m->lr + 0.7152 * m->lg + 0.0722 * m->lb;
}

static void neutral_params(crt_params_t *p);

static double measure_row_modulation(const char *label) {
    static unsigned char *buf;
    crt_frame_desc_t desc;
    double lo = 1e9, hi = -1e9;
    int y, x, x0, x1, y0, y1;
    size_t need;

    fill_desc(&desc);
    glBindFramebuffer_compat();
    glViewport(0, 0, s_vp_w, s_vp_h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (CrtRendererGL_Draw(s_src_tex, &desc, 0, 0, s_vp_w, s_vp_h, 0.0) != 1)
        return -1.0;
    glFinish();

    need = (size_t)s_vp_w * s_vp_h * 4;
    buf = (unsigned char *)realloc(buf, need);
    if (!buf) return -1.0;
    glReadPixels(0, 0, s_vp_w, s_vp_h, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    if (s_dump_dir && label) dump_bmp(label, buf, s_vp_w, s_vp_h);

    x0 = s_vp_w * 3 / 8; x1 = s_vp_w * 5 / 8;
    y0 = s_vp_h / 2;     y1 = y0 + s_vp_h / 16;
    for (y = y0; y < y1; y++) {
        double row = 0.0;
        for (x = x0; x < x1; x++) {
            const unsigned char *p = buf + ((size_t)y * s_vp_w + x) * 4;
            row += 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
        }
        row /= (x1 - x0);
        if (row < lo) lo = row;
        if (row > hi) hi = row;
    }
    return (hi + lo) > 1e-6 ? (hi - lo) / (hi + lo) : 0.0;
}

static double measure_profile_width(double frac, double *out_energy) {
    static unsigned char *buf;
    crt_frame_desc_t desc;
    double prof[512];
    double lo = 1e9, hi = -1e9, level, energy = 0.0;
    int n = 0, above = 0, i, x, y, x0, x1, y0;
    size_t need;

    fill_desc(&desc);
    glBindFramebuffer_compat();
    glViewport(0, 0, s_vp_w, s_vp_h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (CrtRendererGL_Draw(s_src_tex, &desc, 0, 0, s_vp_w, s_vp_h, s_clock) != 1)
        return -1.0;
    glFinish();

    need = (size_t)s_vp_w * s_vp_h * 4;
    buf = (unsigned char *)realloc(buf, need);
    if (!buf) return -1.0;
    glReadPixels(0, 0, s_vp_w, s_vp_h, GL_RGBA, GL_UNSIGNED_BYTE, buf);

    x0 = s_vp_w * 3 / 8; x1 = s_vp_w * 5 / 8;
    y0 = s_vp_h / 2;
    n = s_vp_h / 8; if (n > 512) n = 512;

    for (i = 0; i < n; i++) {
        double row = 0.0;
        y = y0 + i;
        for (x = x0; x < x1; x++) {
            const unsigned char *p = buf + ((size_t)y * s_vp_w + x) * 4;
            row += 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
        }
        row /= (x1 - x0);
        prof[i] = row;
        if (row < lo) lo = row;
        if (row > hi) hi = row;
        energy += decode_srgb(row);
    }
    if (out_energy) *out_energy = energy / n;
    if (hi - lo < 1e-6) return 1.0;

    level = lo + (hi - lo) * frac;
    for (i = 0; i < n; i++) if (prof[i] >= level) above++;
    return (double)above / (double)n;
}

static void test_beam_shape(void) {
    crt_params_t p;
    double e_narrow, e_wide, e_mid;
    double w_narrow, w_mid, w_wide;
    double sh_low, sh_high, fwhm_low, fwhm_high;
    double m_dim, m_bright;

    printf("beam width and shape (flat 50%% field)\n");

    upload_flat(128, 128, 128);

    neutral_params(&p);
    p.beam_sigma_min = p.beam_sigma_max = 0.15f;
    CrtRenderer_SetParams(&p);
    w_narrow = measure_profile_width(0.5, &e_narrow);

    neutral_params(&p);
    p.beam_sigma_min = p.beam_sigma_max = 0.30f;
    CrtRenderer_SetParams(&p);
    w_mid = measure_profile_width(0.5, &e_mid);

    neutral_params(&p);
    p.beam_sigma_min = p.beam_sigma_max = 0.50f;
    CrtRenderer_SetParams(&p);
    w_wide = measure_profile_width(0.5, &e_wide);

    printf("    sigma 0.15 -> FWHM %.3f lines, energy %.4f\n", w_narrow, e_narrow);
    printf("    sigma 0.30 -> FWHM %.3f lines, energy %.4f\n", w_mid, e_mid);
    printf("    sigma 0.50 -> FWHM %.3f lines, energy %.4f\n", w_wide, e_wide);

    check(w_mid > w_narrow && w_wide > w_mid,
          "FWHM grows with beam sigma");
    check(fabs(e_mid - e_narrow) / (e_narrow + 1e-9) < 0.05 &&
          fabs(e_wide - e_narrow) / (e_narrow + 1e-9) < 0.05,
          "line-integrated energy is unchanged by beam width");

    neutral_params(&p);
    p.beam_sigma_min = p.beam_sigma_max = 0.30f;
    p.beam_shape_exponent = 1.5f;
    CrtRenderer_SetParams(&p);
    fwhm_low = measure_profile_width(0.5, NULL);
    sh_low   = measure_profile_width(0.75, NULL);

    neutral_params(&p);
    p.beam_sigma_min = p.beam_sigma_max = 0.30f;
    p.beam_shape_exponent = 4.0f;
    CrtRenderer_SetParams(&p);
    fwhm_high = measure_profile_width(0.5, NULL);
    sh_high   = measure_profile_width(0.75, NULL);

    printf("    shape 1.5 -> FWHM %.3f, w75 %.3f (ratio %.3f)\n",
           fwhm_low, sh_low, sh_low / (fwhm_low + 1e-9));
    printf("    shape 4.0 -> FWHM %.3f, w75 %.3f (ratio %.3f)\n",
           fwhm_high, sh_high, sh_high / (fwhm_high + 1e-9));

    check(sh_high / (fwhm_high + 1e-9) > sh_low / (fwhm_low + 1e-9),
          "a higher shape exponent flattens the top and steepens the sides");

    neutral_params(&p);
    p.beam_sigma_min = 0.15f;
    p.beam_sigma_max = 0.50f;
    p.beam_luma_exponent = 1.0f;
    CrtRenderer_SetParams(&p);

    upload_flat(60, 60, 60);
    m_dim = measure_row_modulation(NULL);
    upload_flat(240, 240, 240);
    m_bright = measure_row_modulation(NULL);

    printf("    dim field modulation %.4f, bright field %.4f\n", m_dim, m_bright);
    check(m_bright < m_dim * 0.5,
          "a brightly driven line is wider, so it merges more");
    check(m_dim > 0.1,
          "and a dim line still shows distinct scanlines");

    upload_flat(128, 128, 128);
}

static void test_scanlines(void) {
    crt_params_t p;
    int i;

    printf("scanline formation (row modulation on a FLAT field)\n");

    for (i = 0; i < 3; i++) {
        static const int levels[3] = { 64, 128, 230 };
        double tight, wide;
        char msg[160];

        upload_flat(levels[i], levels[i], levels[i]);

        neutral_params(&p);
        p.beam_sigma_min = p.beam_sigma_max = 0.20f;
        CrtRenderer_SetParams(&p);
        tight = measure_row_modulation(i == 1 ? "scanlines_tight" : NULL);

        neutral_params(&p);
        p.beam_sigma_min = p.beam_sigma_max = 0.65f;
        CrtRenderer_SetParams(&p);
        wide = measure_row_modulation(NULL);

        printf("    level %3d: tight beam %.4f, wide beam %.4f\n",
               levels[i], tight, wide);

        snprintf(msg, sizeof msg,
                 "level %d: a narrow beam produces scanlines (%.4f, want >0.02)",
                 levels[i], tight);
        check(tight > 0.02, msg);

        snprintf(msg, sizeof msg,
                 "level %d: a wide beam merges them (%.4f < %.4f)",
                 levels[i], wide, tight);
        check(wide < tight, msg);
    }
}

static void neutral_params(crt_params_t *p) {
    memset(p, 0, sizeof *p);
    p->profile = CRT_PROFILE_CLEAN;
    p->quality = CRT_QUALITY_FULL;
    p->aspect_policy = CRT_ASPECT_RAW;
    p->output_transfer = CRT_TRANSFER_SRGB;
    p->output_gamma = 2.2f;
    p->paper_white = 1.0f;
    p->brightness_compensation = 1.0f;
    p->reconstruction = CRT_RECON_NEAREST;
    p->reconstruction_radius = 1.0f;
    p->reconstruction_sharpness = 0.5f;
    p->reconstruction_anti_ringing = 0.0f;
    p->spot_sigma_dark = 0.0f;
    p->spot_sigma_bright = 0.0f;
    p->spot_luma_exponent = 1.0f;
    p->sample_phase_x = 0.5f;

    p->beam_sigma_min = 0.65f;
    p->beam_sigma_max = 0.65f;
    p->beam_luma_exponent = 1.0f;
    p->beam_shape_exponent = 2.0f;
    p->scanline_phase = 0.5f;
    p->scanline_strength = 1.0f;
    p->mask_layout = CRT_MASK_NONE;
    p->mask_pitch_pixels = 3.0f;
    p->mask_strength = 0.0f;
    p->mask_aperture = 0.0f;
    p->mask_row_ratio = 0.75f;
    p->mask_coord_mode = CRT_MASK_COORD_OUTPUT_GRID;
    p->bloom_tail = 2.0f;
    p->diffusion_tail = 2.0f;
    p->halation_tail = 2.0f;
    p->halation_tint_r = p->halation_tint_g = p->halation_tint_b = 1.0f;
    p->convergence_radial = 0.0f;
    p->focus_edge = 0.0f;
    p->face_geometry = CRT_FACE_FLAT;
    p->raster_pincushion = 0.0f;
    p->raster_keystone = 0.0f;
    p->raster_rotation = 0.0f;
    p->headroom_policy = CRT_HEADROOM_CLIP;
    p->black_level = 0.0f;
    p->paper_white = 1.0f;
    p->corner_softness = 0.20f;
}

static void test_black_and_white(void) {
    crt_params_t p;
    measure_t m;

    printf("levels\n");
    neutral_params(&p);
    CrtRenderer_SetParams(&p);

    upload_flat(0, 0, 0);
    if (render_and_measure("black", &m)) {
        checkf(m.r < 1.0 && m.g < 1.0 && m.b < 1.0,
               "black stays black (mean %.2f, max %.0f)", m.r, m.max_r);
    }

    upload_flat(255, 255, 255);
    if (render_and_measure("white", &m)) {
        checkf(m.r > 250.0 && m.g > 250.0 && m.b > 250.0,
               "white stays white (mean %.1f, expected >250)", m.r, 255.0);
    }

    upload_flat(128, 128, 128);
    if (render_and_measure("gray50", &m)) {
        checkf(fabs(m.r - 128.0) < 3.0,
               "50%% gray round-trips (%.1f, expected 128 +/- 3)", m.r, 128.0);
        checkf(fabs(m.r - m.g) < 1.0 && fabs(m.g - m.b) < 1.0,
               "50%% gray stays neutral (r-g %.2f, g-b %.2f)",
               m.r - m.g, m.g - m.b);
    }
}

static void test_corner_radius_brightness(void) {
    crt_params_t p;
    measure_t flat, rounded;

    printf("geometry independence\n");
    upload_flat(200, 200, 200);

    neutral_params(&p);
    p.corner_radius = 0.0f;
    CrtRenderer_SetParams(&p);
    if (!render_and_measure("corner_none", &flat)) return;

    neutral_params(&p);
    p.corner_radius = 0.06f;
    CrtRenderer_SetParams(&p);
    if (!render_and_measure("corner_round", &rounded)) return;

    checkf(fabs(luma_of(&flat) - luma_of(&rounded)) < 1.0,
           "corner radius does not change interior brightness (%.1f vs %.1f)",
           luma_of(&flat), luma_of(&rounded));

    neutral_params(&p);
    p.warp_x = p.warp_y = 0.08f;
    p.corner_radius = 0.06f;
    CrtRenderer_SetParams(&p);
    if (!render_and_measure("curved", &rounded)) return;
    checkf(fabs(luma_of(&flat) - luma_of(&rounded)) < 2.0,
           "curvature does not change interior brightness (%.1f vs %.1f)",
           luma_of(&flat), luma_of(&rounded));
}

static void test_mask_energy(void) {
    static const struct { const char *name; crt_mask_layout_t layout; } kinds[] = {
        { "aperture grille", CRT_MASK_APERTURE_GRILLE },
        { "slot",            CRT_MASK_SLOT },
        { "shadow",          CRT_MASK_SHADOW },
    };
    crt_params_t p;
    measure_t off, on;
    size_t i;
    char buf[160];

    printf("mask energy (linear light, 50%% gray)\n");
    upload_flat(128, 128, 128);

    neutral_params(&p);
    CrtRenderer_SetParams(&p);
    if (!render_and_measure("mask_off", &off)) return;

    for (i = 0; i < sizeof kinds / sizeof kinds[0]; i++) {
        int order;
        for (order = 0; order < 2; order++) {
            double a, b;
            neutral_params(&p);
            p.mask_layout = kinds[i].layout;
            p.mask_order  = order ? CRT_MASK_BGR : CRT_MASK_RGB;
            p.mask_pitch_pixels = 6.0f;
            p.mask_strength = 1.0f;

            p.mask_aperture = 1.0f;
            p.mask_row_ratio = 1.2f;
            CrtRenderer_SetParams(&p);
            if (!render_and_measure(NULL, &on)) continue;

            a = linear_luma_of(&off);
            b = linear_luma_of(&on);
            snprintf(buf, sizeof buf,
                     "%s %s: emitted light unchanged (%.4f vs %.4f)",
                     kinds[i].name, order ? "BGR" : "RGB", a, b);
            check(fabs(a - b) < 0.01, buf);

            snprintf(buf, sizeof buf,
                     "%s %s: neutral gray stays neutral (r-b %.2f)",
                     kinds[i].name, order ? "BGR" : "RGB", on.r - on.b);
            check(fabs(on.r - on.b) < 3.0 && fabs(on.g - on.b) < 3.0, buf);

            snprintf(buf, sizeof buf,
                     "%s %s: nothing clipped at 50%% gray (%ld of %ld px)",
                     kinds[i].name, order ? "BGR" : "RGB",
                     on.clipped, on.samples);
            check(on.clipped == 0, buf);
        }
    }
}

static void test_phosphor_render(void) {
    crt_params_t p;
    measure_t plain, shifted;

    printf("phosphor rendering\n");

    upload_flat(255, 255, 255);
    neutral_params(&p);
    p.phosphor = CRT_PHOSPHOR_SMPTE_C;
    CrtRenderer_SetParams(&p);
    if (render_and_measure("phosphor_white", &shifted)) {
        checkf(fabs(shifted.r - shifted.g) < 1.5 && fabs(shifted.g - shifted.b) < 1.5,
               "SMPTE-C leaves white neutral (r-g %.2f, g-b %.2f)",
               shifted.r - shifted.g, shifted.g - shifted.b);
    }

    upload_flat(128, 128, 128);
    if (render_and_measure(NULL, &shifted)) {
        checkf(fabs(shifted.r - shifted.g) < 1.5 && fabs(shifted.g - shifted.b) < 1.5,
               "SMPTE-C leaves mid gray neutral (r-g %.2f, g-b %.2f)",
               shifted.r - shifted.g, shifted.g - shifted.b);
    }

    upload_flat(0, 255, 0);
    neutral_params(&p);
    CrtRenderer_SetParams(&p);
    if (!render_and_measure(NULL, &plain)) return;

    neutral_params(&p);
    p.phosphor = CRT_PHOSPHOR_SMPTE_C;
    CrtRenderer_SetParams(&p);
    if (!render_and_measure("phosphor_green", &shifted)) return;

    checkf(fabs(plain.r - shifted.r) > 2.0 || fabs(plain.b - shifted.b) > 2.0,
           "SMPTE-C measurably shifts saturated green (dr %.1f, db %.1f)",
           plain.r - shifted.r, plain.b - shifted.b);

    upload_flat(128, 128, 128);
    neutral_params(&p);
    CrtRenderer_SetParams(&p);
    if (!render_and_measure(NULL, &plain)) return;

    neutral_params(&p);
    p.tube_gamma = 2.4f;
    CrtRenderer_SetParams(&p);
    if (!render_and_measure(NULL, &shifted)) return;
    checkf(shifted.r < plain.r - 2.0,
           "2.4 tube gamma darkens mid gray (%.1f -> %.1f)", plain.r, shifted.r);

    upload_flat(255, 255, 255);
    if (render_and_measure(NULL, &shifted)) {
        checkf(shifted.r > 250.0,
               "tube gamma leaves white alone (%.1f, >250)", shifted.r, 255.0);
    }
    upload_flat(0, 0, 0);
    if (render_and_measure(NULL, &shifted)) {
        checkf(shifted.r < 1.0,
               "tube gamma leaves black alone (%.2f, <1)", shifted.r, 0.0);
    }
}

static double measure_bloom_spill(const char *label) {
    measure_t dark;

    set_region(0.300, 0.40, 0.343, 0.60);
    render_and_measure(label, &dark);
    reset_region();

    return linear_luma_of(&dark);
}

static void test_bloom(void) {
    crt_params_t p;
    double none, shipped, lower;
    int i;

    printf("bloom reach (LINEAR light spilled onto dark area beside a bright square)\n");

    upload_bright_square(255, 40);

    neutral_params(&p);
    p.bloom_strength = 0.0f;
    CrtRenderer_SetParams(&p);
    none = measure_bloom_spill("bloom_off");

    neutral_params(&p);
    p.bloom_threshold     = 0.75f;
    p.bloom_knee          = 0.20f;
    p.bloom_radius_pixels = 6.0f;
    p.bloom_strength      = 0.18f;
    CrtRenderer_SetParams(&p);
    shipped = measure_bloom_spill("bloom_shipped");

    neutral_params(&p);
    p.bloom_threshold     = 0.25f;
    p.bloom_knee          = 0.15f;
    p.bloom_radius_pixels = 8.0f;
    p.bloom_strength      = 0.35f;
    CrtRenderer_SetParams(&p);
    lower = measure_bloom_spill("bloom_lower_threshold");

    printf("    threshold 0.75 / strength 0.18 (was shipped): %+.5f\n",
           shipped - none);
    printf("    threshold 0.25 / strength 0.35             : %+.5f\n",
           lower - none);

    check(shipped >= none, "bloom never darkens the surround");
    check(lower > shipped, "lowering the threshold increases reach");

    for (i = 0; i < 3; i++) {
        static const crt_profile_id_t ids[3] = {
            CRT_PROFILE_CLEAN, CRT_PROFILE_SGB_CONSUMER, CRT_PROFILE_SGB_RGB_PVM
        };
        double lift;
        CrtRenderer_SetCurve(CRT_CURVE_PROFILE);
        CrtRenderer_SetProfile(ids[i]);
        lift = measure_bloom_spill(NULL) - none;
        printf("    profile %-8s: %+.5f linear\n",
               CrtRenderer_ProfileName(ids[i]), lift);
        if (ids[i] != CRT_PROFILE_CLEAN) {
            char msg[128];

            snprintf(msg, sizeof msg,
                     "%s bloom is actually visible (%+.5f linear, want >0.002)",
                     CrtRenderer_ProfileName(ids[i]), lift);
            check(lift > 0.002, msg);
        }
    }

    printf("    linear threshold -> encoded level a pixel must exceed:\n");
    for (i = 0; i < 4; i++) {
        static const double t[4] = { 0.75, 0.50, 0.25, 0.10 };
        printf("      %.2f linear = %3.0f / 255 encoded\n",
               t[i], 255.0 * pow(t[i], 1.0 / 2.2));
    }
}

static void test_headroom(void) {
    static const int levels[5] = { 64, 128, 191, 230, 255 };
    crt_params_t p;
    measure_t off, clipd, preserved;
    int i;

    printf("headroom across the tone scale (slot mask, strength 0.45)\n");
    printf("    level   policy           mean-luma-err   clipped px\n");

    for (i = 0; i < 5; i++) {
        double e_clip, e_pres;

        upload_flat(levels[i], levels[i], levels[i]);

        neutral_params(&p);
        CrtRenderer_SetParams(&p);
        if (!render_and_measure(NULL, &off)) continue;

        neutral_params(&p);
        p.mask_layout = CRT_MASK_SLOT;
        p.mask_pitch_pixels = 6.0f;
        p.mask_strength = 0.45f;
        p.headroom_policy = CRT_HEADROOM_CLIP;
        CrtRenderer_SetParams(&p);
        if (!render_and_measure(NULL, &clipd)) continue;

        neutral_params(&p);
        p.mask_layout = CRT_MASK_SLOT;
        p.mask_pitch_pixels = 6.0f;
        p.mask_strength = 0.45f;
        p.headroom_policy = CRT_HEADROOM_PRESERVE_PEAKS;
        CrtRenderer_SetParams(&p);
        if (!render_and_measure(NULL, &preserved)) continue;

        e_clip = (linear_luma_of(&clipd) - linear_luma_of(&off)) /
                 (linear_luma_of(&off) + 1e-9);
        e_pres = (linear_luma_of(&preserved) - linear_luma_of(&off)) /
                 (linear_luma_of(&off) + 1e-9);

        printf("    %3d     CLIP             %+7.2f%%        %ld\n",
               levels[i], 100.0 * e_clip, clipd.clipped);
        printf("            PRESERVE_PEAKS   %+7.2f%%        %ld\n",
               100.0 * e_pres, preserved.clipped);

        {
            char msg[160];
            snprintf(msg, sizeof msg,
                     "level %d: PRESERVE_PEAKS clips nothing (%ld px)",
                     levels[i], preserved.clipped);
            check(preserved.clipped == 0, msg);

            snprintf(msg, sizeof msg,
                     "level %d: PRESERVE_PEAKS keeps white neutral (r-b %.2f)",
                     levels[i], preserved.r - preserved.b);
            check(fabs(preserved.r - preserved.b) < 2.0, msg);
        }
    }

    upload_flat(255, 255, 255);
    neutral_params(&p);
    p.mask_layout = CRT_MASK_SLOT;
    p.mask_pitch_pixels = 6.0f;
    p.mask_strength = 0.45f;
    p.headroom_policy = CRT_HEADROOM_PRESERVE_PEAKS;
    CrtRenderer_SetParams(&p);
    if (render_and_measure(NULL, &preserved)) {
        printf("    cost: white at 255 renders as %.0f/255 "
               "(mask peak %.3f, so 1/peak of full scale)\n",
               preserved.r,
               (double)CrtRenderer_MaskPeak(CRT_MASK_SLOT, 0.45f, 6.0f));
    }
}

static void test_output_transfer(void) {
    static const int darks[3] = { 4, 8, 16 };
    crt_params_t p;
    measure_t srgb, power;
    int i;

    printf("output transfer near black\n");

    for (i = 0; i < 3; i++) {
        char msg[160];

        upload_flat(darks[i], darks[i], darks[i]);

        neutral_params(&p);
        p.output_transfer = CRT_TRANSFER_SRGB;
        CrtRenderer_SetParams(&p);
        if (!render_and_measure(NULL, &srgb)) continue;

        neutral_params(&p);
        p.output_transfer = CRT_TRANSFER_GAMMA;
        p.output_gamma = 2.2f;
        CrtRenderer_SetParams(&p);
        if (!render_and_measure(NULL, &power)) continue;

        printf("    input %2d/255 -> sRGB %5.1f, pow(1/2.2) %5.1f\n",
               darks[i], srgb.r, power.r);

        snprintf(msg, sizeof msg,
                 "input %d round-trips exactly through sRGB (%.1f)",
                 darks[i], srgb.r);
        check(fabs(srgb.r - (double)darks[i]) < 1.5, msg);

        snprintf(msg, sizeof msg,
                 "input %d differs under a power curve (%.1f vs %.1f)",
                 darks[i], power.r, srgb.r);
        check(power.r > srgb.r + 2.0, msg);
    }
}

static void upload_vertical_grating(int hi_level, int lo_level) {
    static uint32_t px[SRC_W * SRC_H];
    int x, y;
    for (y = 0; y < SRC_H; y++) {
        for (x = 0; x < SRC_W; x++) {
            int v = (x & 1) ? hi_level : lo_level;
            px[y * SRC_W + x] = (uint32_t)v | ((uint32_t)v << 8) |
                                ((uint32_t)v << 16) | 0xFF000000u;
        }
    }
    glBindTexture(GL_TEXTURE_2D, s_src_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SRC_W, SRC_H, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static double measure_column_modulation(const char *label) {
    static unsigned char *buf;
    crt_frame_desc_t desc;
    double lo = 1e9, hi = -1e9;
    int x, y, x0, x1, y0, y1;
    size_t need;

    fill_desc(&desc);
    glBindFramebuffer_compat();
    glViewport(0, 0, s_vp_w, s_vp_h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (CrtRendererGL_Draw(s_src_tex, &desc, 0, 0, s_vp_w, s_vp_h, 0.0) != 1)
        return -1.0;
    glFinish();

    need = (size_t)s_vp_w * s_vp_h * 4;
    buf = (unsigned char *)realloc(buf, need);
    if (!buf) return -1.0;
    glReadPixels(0, 0, s_vp_w, s_vp_h, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    if (s_dump_dir && label) dump_bmp(label, buf, s_vp_w, s_vp_h);

    x0 = s_vp_w / 2; x1 = x0 + s_vp_w / 16;
    y0 = s_vp_h * 3 / 8; y1 = s_vp_h * 5 / 8;
    for (x = x0; x < x1; x++) {
        double col = 0.0;
        for (y = y0; y < y1; y++) {
            const unsigned char *p = buf + ((size_t)y * s_vp_w + x) * 4;
            col += 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
        }
        col /= (y1 - y0);
        if (col < lo) lo = col;
        if (col > hi) hi = col;
    }
    return (hi + lo) > 1e-6 ? (hi - lo) / (hi + lo) : 0.0;
}

static void test_horizontal_response(void) {
    crt_params_t p;
    double no_spot, small_spot, big_spot, sharp, soft;

    printf("horizontal response (contrast across a 1-pixel vertical grating)\n");

    upload_vertical_grating(230, 16);

    neutral_params(&p);
    p.reconstruction = CRT_RECON_GAUSSIAN;
    p.reconstruction_radius = 1.0f;
    CrtRenderer_SetParams(&p);
    no_spot = measure_column_modulation("hgrating_nospot");

    neutral_params(&p);
    p.reconstruction = CRT_RECON_GAUSSIAN;
    p.reconstruction_radius = 1.0f;
    p.spot_sigma_dark = p.spot_sigma_bright = 0.30f;
    CrtRenderer_SetParams(&p);
    small_spot = measure_column_modulation(NULL);

    neutral_params(&p);
    p.reconstruction = CRT_RECON_GAUSSIAN;
    p.reconstruction_radius = 1.0f;
    p.spot_sigma_dark = p.spot_sigma_bright = 0.60f;
    CrtRenderer_SetParams(&p);
    big_spot = measure_column_modulation("hgrating_bigspot");

    printf("    spot 0.00 -> %.4f    spot 0.30 -> %.4f    spot 0.60 -> %.4f\n",
           no_spot, small_spot, big_spot);
    check(small_spot < no_spot, "a spot reduces horizontal contrast");
    check(big_spot < small_spot, "a wider spot reduces it further");

    neutral_params(&p);
    p.reconstruction = CRT_RECON_GAUSSIAN;
    p.reconstruction_radius = 1.0f;
    p.reconstruction_sharpness = 0.9f;
    CrtRenderer_SetParams(&p);
    sharp = measure_column_modulation(NULL);

    neutral_params(&p);
    p.reconstruction = CRT_RECON_GAUSSIAN;
    p.reconstruction_radius = 1.0f;
    p.reconstruction_sharpness = 0.1f;
    CrtRenderer_SetParams(&p);
    soft = measure_column_modulation(NULL);

    printf("    sharpness 0.9 -> %.4f    sharpness 0.1 -> %.4f\n", sharp, soft);
    check(sharp > soft, "reconstruction sharpness moves contrast on its own");

    neutral_params(&p);
    p.reconstruction = CRT_RECON_GAUSSIAN;
    p.reconstruction_radius = 1.0f;
    p.spot_sigma_dark = 0.10f;
    p.spot_sigma_bright = 0.70f;
    p.spot_luma_exponent = 1.0f;
    CrtRenderer_SetParams(&p);
    {
        double luma_dependent = measure_column_modulation(NULL);
        printf("    luminance-dependent spot (0.10 dark / 0.70 bright) -> %.4f\n",
               luma_dependent);
        check(luma_dependent < no_spot,
              "a luminance-dependent spot still limits contrast");
        check(luma_dependent > big_spot,
              "and limits it less than a uniformly wide one");
    }
}

static void upload_padded_green(int tex_w, int tex_h, crt_rect_i_t active) {
    uint32_t *px = (uint32_t *)malloc((size_t)tex_w * tex_h * 4);
    int x, y;
    if (!px) return;

    for (y = 0; y < tex_h; y++) {
        for (x = 0; x < tex_w; x++) {
            int inside = (x >= active.x && x < active.x + active.w &&
                          y >= active.y && y < active.y + active.h);

            px[y * tex_w + x] = inside ? (0x00u | (200u << 8) | (0u << 16) | 0xFF000000u)
                                       : (200u | (0u << 8)   | (0u << 16) | 0xFF000000u);
        }
    }
    glBindTexture(GL_TEXTURE_2D, s_src_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_w, tex_h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    free(px);
}

static void test_active_rect_sampling(void) {
    crt_params_t p;
    crt_frame_desc_t d;
    crt_rect_i_t tex_active = { 16, 32, 240, 192 };
    measure_t m;

    printf("active rectangle sampling\n");

    upload_padded_green(512, 512, tex_active);

    memset(&d, 0, sizeof d);
    d.texture_width = 512;  d.texture_height = 512;
    d.raster_width  = 256;  d.raster_height  = 224;
    d.texture_active_rect = tex_active;

    d.raster_active_rect  = (crt_rect_i_t){ 8, 16, 240, 192 };
    d.pixel_aspect_num = 8; d.pixel_aspect_den = 7;
    d.input_transfer = CRT_TRANSFER_SRGB;
    d.input_gamma = 2.2f;
    d.source_refresh_hz = 59.7275f;
    d.frame_number = 1;

    check(CrtRenderer_ValidateFrame(&d, NULL), "padded/cropped descriptor is valid");

    neutral_params(&p);
    CrtRenderer_SetParams(&p);
    s_desc_override = &d;
    if (render_and_measure("active_rect", &m)) {
        printf("    centre reads r=%.1f g=%.1f b=%.1f (want green, no red)\n",
               m.r, m.g, m.b);
        check(m.g > 150.0, "the active rectangle's own content is displayed");
        check(m.r < 8.0,
              "no padding bleeds in from outside the active rectangle");
    }
    s_desc_override = NULL;
}

typedef struct {
    const char *name;
    void (*apply)(crt_params_t *p);
} sens_case_t;

static void sens_mask(crt_params_t *p)      { p->mask_layout = CRT_MASK_SLOT;
                                              p->mask_pitch_pixels = 6.0f;
                                              p->mask_strength = 0.8f; }
static void sens_triads(crt_params_t *p)    { p->mask_layout = CRT_MASK_APERTURE_GRILLE;
                                              p->mask_strength = 0.8f;
                                              p->mask_triads_per_picture_height = 90.0f; }
static void sens_beam_min(crt_params_t *p)  { p->beam_sigma_min = 0.15f;
                                              p->beam_sigma_max = 0.15f; }
static void sens_beam_shape(crt_params_t *p){ p->beam_sigma_min = p->beam_sigma_max = 0.25f;
                                              p->beam_shape_exponent = 4.0f; }
static void sens_scanline(crt_params_t *p)  { p->beam_sigma_min = p->beam_sigma_max = 0.20f;
                                              p->scanline_strength = 0.0f; }
static void sens_phase(crt_params_t *p)     { p->beam_sigma_min = p->beam_sigma_max = 0.20f;
                                              p->scanline_phase = 0.0f; }
static void sens_spot(crt_params_t *p)      { p->spot_sigma_dark = 0.6f;
                                              p->spot_sigma_bright = 0.6f; }
static void sens_sharp(crt_params_t *p)     { p->reconstruction_sharpness = 0.95f; }
static void sens_recon(crt_params_t *p)     { p->reconstruction = CRT_RECON_LANCZOS2; }
static void sens_radius(crt_params_t *p)    { p->reconstruction = CRT_RECON_GAUSSIAN;
                                              p->reconstruction_radius = 3.0f; }
static void sens_tube_gamma(crt_params_t *p){ p->tube_gamma = 2.8f; }
static void sens_phosphor(crt_params_t *p)  { p->phosphor = CRT_PHOSPHOR_SMPTE_C; }
static void sens_black(crt_params_t *p)     { p->black_level = 0.05f; }
static void sens_paper(crt_params_t *p)     { p->paper_white = 0.5f; }
static void sens_bright(crt_params_t *p)    { p->brightness_compensation = 0.5f; }
static void sens_outgamma(crt_params_t *p)  { p->output_transfer = CRT_TRANSFER_GAMMA;
                                              p->output_gamma = 1.2f; }
static void sens_warp(crt_params_t *p)      { p->face_geometry = CRT_FACE_SPHERICAL;
                                              p->warp_x = p->warp_y = 0.15f; }

static void sens_facetype(crt_params_t *p)  { p->face_geometry = CRT_FACE_CYLINDRICAL; }
static void sens_pincushion(crt_params_t *p){ p->raster_pincushion = 0.10f; }
static void sens_keystone(crt_params_t *p)  { p->raster_keystone = 0.10f; }
static void sens_rotation(crt_params_t *p)  { p->raster_rotation = 0.05f; }
static void sens_overscan(crt_params_t *p)  { p->overscan_x = p->overscan_y = 0.15f; }
static void sens_center(crt_params_t *p)    { p->center_x = 0.05f; }
static void sens_corner(crt_params_t *p)    { p->corner_radius = 0.4f; }
static void sens_vignette(crt_params_t *p)  { p->vignette_strength = 1.0f; }
static void sens_conv(crt_params_t *p)      { p->convergence_r_x = 3.0f;
                                              p->convergence_b_x = -3.0f; }
static void sens_convradial(crt_params_t *p){ p->convergence_radial = 3.0f; }
static void sens_focusedge(crt_params_t *p) { p->focus_edge = 1.0f; }
static void sens_bloom(crt_params_t *p)     { p->bloom_threshold = 0.1f;
                                              p->bloom_strength = 1.0f;
                                              p->bloom_radius_lines = 4.0f; }
static void sens_halation(crt_params_t *p)  { p->halation_strength = 1.0f;
                                              p->halation_threshold = 0.1f;
                                              p->halation_radius_lines = 8.0f; }
static void sens_diffusion(crt_params_t *p) { p->diffusion_strength = 1.0f;
                                              p->diffusion_radius_lines = 5.0f; }
static void sens_halthresh(crt_params_t *p) { p->halation_strength = 1.0f;
                                              p->halation_radius_lines = 8.0f;
                                              p->halation_threshold = 0.9f; }
static void sens_haltint(crt_params_t *p)   { p->halation_strength = 1.0f;
                                              p->halation_threshold = 0.1f;
                                              p->halation_radius_lines = 8.0f;
                                              p->halation_tint_g = 0.3f;
                                              p->halation_tint_b = 0.2f; }
static void sens_bloomtail(crt_params_t *p) { p->bloom_threshold = 0.1f;
                                              p->bloom_strength = 1.0f;
                                              p->bloom_radius_lines = 4.0f;
                                              p->bloom_tail = 0.9f; }
static void sens_headroom(crt_params_t *p)  { p->mask_layout = CRT_MASK_SLOT;
                                              p->mask_pitch_pixels = 6.0f;
                                              p->mask_strength = 0.8f;
                                              p->headroom_policy = CRT_HEADROOM_PRESERVE_PEAKS; }
static void sens_aniring(crt_params_t *p)   { p->reconstruction = CRT_RECON_LANCZOS2;
                                              p->reconstruction_anti_ringing = 1.0f; }
static void sens_maskorder(crt_params_t *p) { p->mask_layout = CRT_MASK_APERTURE_GRILLE;
                                              p->mask_pitch_pixels = 9.0f;
                                              p->mask_strength = 1.0f;
                                              p->mask_order = CRT_MASK_BGR; }
static void sens_aperture(crt_params_t *p)  { p->mask_layout = CRT_MASK_APERTURE_GRILLE;
                                              p->mask_pitch_pixels = 9.0f;
                                              p->mask_strength = 1.0f;
                                              p->mask_aperture = 1.0f; }
static void sens_rowratio(crt_params_t *p)  { p->mask_layout = CRT_MASK_SLOT;
                                              p->mask_pitch_pixels = 9.0f;
                                              p->mask_strength = 1.0f;
                                              p->mask_row_ratio = 3.0f; }
static void sens_coordmode(crt_params_t *p) { p->mask_layout = CRT_MASK_APERTURE_GRILLE;
                                              p->mask_pitch_pixels = 9.0f;
                                              p->mask_strength = 1.0f;

                                              p->face_geometry = CRT_FACE_SPHERICAL;
                                              p->warp_x = p->warp_y = 0.12f;
                                              p->mask_coord_mode = CRT_MASK_COORD_TUBE_SURFACE; }
static void sens_maskphase(crt_params_t *p) { p->mask_layout = CRT_MASK_APERTURE_GRILLE;
                                              p->mask_pitch_pixels = 9.0f;
                                              p->mask_strength = 1.0f;
                                              p->mask_phase_x = 4.5f; }
static void sens_spotluma(crt_params_t *p)  { p->spot_sigma_dark = 0.05f;
                                              p->spot_sigma_bright = 0.7f;
                                              p->spot_luma_exponent = 4.0f; }
static void sens_beamluma(crt_params_t *p)  { p->beam_sigma_min = 0.12f;
                                              p->beam_sigma_max = 0.60f;
                                              p->beam_luma_exponent = 4.0f; }
static void sens_samplephase(crt_params_t *p){ p->sample_phase_x = 0.0f; }
static void sens_quality(crt_params_t *p)   { p->quality = CRT_QUALITY_FAST;
                                              p->beam_sigma_min = p->beam_sigma_max = 0.6f; }

static const sens_case_t SENS[] = {
    { "mask_layout / mask_strength", sens_mask },
    { "mask_triads_per_picture_height", sens_triads },
    { "mask_order",                  sens_maskorder },
    { "mask_phase_x",                sens_maskphase },
    { "mask_aperture",               sens_aperture },
    { "mask_row_ratio",              sens_rowratio },
    { "mask_coord_mode",             sens_coordmode },
    { "headroom_policy",             sens_headroom },
    { "beam_sigma_min/max",          sens_beam_min },
    { "beam_shape_exponent",         sens_beam_shape },
    { "beam_luma_exponent",          sens_beamluma },
    { "scanline_strength",           sens_scanline },
    { "scanline_phase",              sens_phase },
    { "spot_sigma_dark/bright",      sens_spot },
    { "spot_luma_exponent",          sens_spotluma },
    { "reconstruction",              sens_recon },
    { "reconstruction_radius",       sens_radius },
    { "reconstruction_sharpness",    sens_sharp },
    { "reconstruction_anti_ringing", sens_aniring },
    { "sample_phase_x",              sens_samplephase },
    { "tube_gamma",                  sens_tube_gamma },
    { "phosphor",                    sens_phosphor },
    { "black_level",                 sens_black },
    { "paper_white",                 sens_paper },
    { "brightness_compensation",     sens_bright },
    { "output_transfer/gamma",       sens_outgamma },
    { "warp_x/y",                    sens_warp },
    { "face_geometry",               sens_facetype },
    { "raster_pincushion",           sens_pincushion },
    { "raster_keystone",             sens_keystone },
    { "raster_rotation",             sens_rotation },
    { "overscan_x/y",                sens_overscan },
    { "center_x",                    sens_center },
    { "corner_radius",               sens_corner },
    { "vignette_strength",           sens_vignette },
    { "convergence_r/b",             sens_conv },
    { "convergence_radial",          sens_convradial },
    { "focus_edge",                  sens_focusedge },
    { "bloom_*",                     sens_bloom },
    { "halation_*",                  sens_halation },
    { "diffusion_*",                 sens_diffusion },
    { "halation_threshold",          sens_halthresh },
    { "halation_tint",               sens_haltint },
    { "bloom_tail",                  sens_bloomtail },
    { "quality",                     sens_quality },
};

static double frame_difference(const crt_params_t *a, const crt_params_t *b) {
    static unsigned char *buf_a, *buf_b;
    crt_frame_desc_t d;
    size_t need = (size_t)s_vp_w * s_vp_h * 4, i;
    double diff = 0.0;

    buf_a = (unsigned char *)realloc(buf_a, need);
    buf_b = (unsigned char *)realloc(buf_b, need);
    if (!buf_a || !buf_b) return -1.0;

    fill_desc(&d);
    CrtRenderer_SetParams(a);
    glBindFramebuffer_compat();
    glViewport(0, 0, s_vp_w, s_vp_h);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    if (CrtRendererGL_Draw(s_src_tex, &d, 0, 0, s_vp_w, s_vp_h, 0.0) != 1) return -1.0;
    glFinish();
    glReadPixels(0, 0, s_vp_w, s_vp_h, GL_RGBA, GL_UNSIGNED_BYTE, buf_a);

    CrtRenderer_SetParams(b);
    glBindFramebuffer_compat();
    glViewport(0, 0, s_vp_w, s_vp_h);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    if (CrtRendererGL_Draw(s_src_tex, &d, 0, 0, s_vp_w, s_vp_h, 0.0) != 1) return -1.0;
    glFinish();
    glReadPixels(0, 0, s_vp_w, s_vp_h, GL_RGBA, GL_UNSIGNED_BYTE, buf_b);

    for (i = 0; i < need; i += 4) {
        diff += fabs((double)buf_a[i]     - buf_b[i]);
        diff += fabs((double)buf_a[i + 1] - buf_b[i + 1]);
        diff += fabs((double)buf_a[i + 2] - buf_b[i + 2]);
    }
    return diff / (double)(need / 4);
}

static void sens_base(crt_params_t *p) {
    neutral_params(p);
    p->reconstruction = CRT_RECON_GAUSSIAN;
    p->reconstruction_radius = 1.0f;
    p->sample_phase_x = 0.5f;
    p->beam_sigma_min = p->beam_sigma_max = 0.35f;
}

static void sens_base_halation(crt_params_t *p);

static void sens_base_curved(crt_params_t *p) {
    sens_base(p);
    p->mask_layout = CRT_MASK_APERTURE_GRILLE;
    p->mask_pitch_pixels = 9.0f;
    p->mask_strength = 1.0f;

    p->face_geometry = CRT_FACE_SPHERICAL;
    p->warp_x = p->warp_y = 0.12f;
}

static void sens_base_halation(crt_params_t *p) {
    sens_base(p);
    p->halation_strength = 1.0f;
    p->halation_radius_lines = 8.0f;
    p->halation_threshold = 0.1f;
}

static void sens_base_spherical(crt_params_t *p) {
    sens_base(p);
    p->face_geometry = CRT_FACE_SPHERICAL;
    p->warp_x = p->warp_y = 0.15f;
}

static void test_parameter_sensitivity(void) {
    crt_params_t base, perturbed;
    size_t i;
    int silent = 0;

    printf("parameter sensitivity (%d public controls)\n", (int)(sizeof SENS / sizeof SENS[0]));

    for (i = 0; i < sizeof SENS / sizeof SENS[0]; i++) {
        double d, d2;
        char msg[160];

        if (strcmp(SENS[i].name, "mask_coord_mode") == 0) {
            sens_base_curved(&base);
            sens_base_curved(&perturbed);
        } else if (strcmp(SENS[i].name, "halation_threshold") == 0 ||
                   strcmp(SENS[i].name, "halation_tint") == 0) {
            sens_base_halation(&base);
            sens_base_halation(&perturbed);
        } else if (strcmp(SENS[i].name, "face_geometry") == 0) {
            sens_base_spherical(&base);
            sens_base_spherical(&perturbed);
        } else {
            sens_base(&base);
            sens_base(&perturbed);
        }
        SENS[i].apply(&perturbed);

        upload_bright_square(230, 40);
        d = frame_difference(&base, &perturbed);

        upload_flat(210, 90, 30);
        d2 = frame_difference(&base, &perturbed);
        if (d2 > d) d = d2;

        snprintf(msg, sizeof msg, "%-32s changes the image (%.3f/px)",
                 SENS[i].name, d);
        if (!(d > 0.05)) silent++;
        check(d > 0.05, msg);
    }

    if (silent == 0)
        printf("    every public control has a measurable effect\n");
}

static void test_defects(void) {
    static const struct {
        const char *name;
        void (*apply)(crt_params_t *p);
        int time_varying;
    } CASES[] = {
        { "jitter",  NULL, 1 },
        { "hum",     NULL, 1 },
        { "noise",   NULL, 1 },
        { "flicker", NULL, 1 },
    };
    crt_params_t p;
    measure_t a, b;
    size_t i;

    printf("optional defects\n");

    upload_bright_square(200, 50);

    for (i = 0; i < sizeof CASES / sizeof CASES[0]; i++) {
        crt_params_t off, on;
        double d_off, d_on, d_time;
        char msg[160];

        neutral_params(&off);
        neutral_params(&on);
        switch (i) {
            case 0: on.jitter_strength = 1.0f; break;
            case 1: on.hum_strength = 0.30f; break;
            case 2: on.noise_strength = 0.30f; break;
            case 3: on.flicker_strength = 0.30f; break;
        }

        s_clock = 0.0;  d_off = 0.0;
        CrtRenderer_SetParams(&off);
        render_and_measure(NULL, &a);
        s_clock = 3.7;
        render_and_measure(NULL, &b);
        d_off = fabs(luma_of(&a) - luma_of(&b));

        s_clock = 0.0;
        CrtRenderer_SetParams(&on);
        render_and_measure(NULL, &a);
        CrtRenderer_SetParams(&off);
        render_and_measure(NULL, &b);
        d_on = fabs(luma_of(&a) - luma_of(&b));

        s_clock = 0.0;
        CrtRenderer_SetParams(&on);
        render_and_measure(NULL, &a);
        s_clock = 3.7;
        render_and_measure(NULL, &b);
        d_time = fabs(luma_of(&a) - luma_of(&b));

        printf("    %-8s off->off %.4f   on vs off %.4f   time alone %.4f\n",
               CASES[i].name, d_off, d_on, d_time);

        snprintf(msg, sizeof msg, "%s at zero is exactly free", CASES[i].name);
        check(d_off < 0.001, msg);

        snprintf(msg, sizeof msg, "%s changes the image when on", CASES[i].name);
        check(d_on > 0.01, msg);

        snprintf(msg, sizeof msg,
                 "%s is driven by the CLOCK, not the frame count", CASES[i].name);
        check(d_time > 0.001, msg);
    }

    s_clock = 0.0;
    neutral_params(&p);
    CrtRenderer_SetParams(&p);

    {
        static const crt_profile_id_t ids[3] = {
            CRT_PROFILE_CLEAN, CRT_PROFILE_SGB_CONSUMER, CRT_PROFILE_SGB_RGB_PVM
        };
        int k;
        for (k = 0; k < 3; k++) {
            char msg[128];
            CrtRenderer_SetProfile(ids[k]);
            CrtRenderer_GetParams(&p);
            snprintf(msg, sizeof msg, "%s ships with no simulated faults",
                     CrtRenderer_ProfileName(ids[k]));
            check(p.jitter_strength == 0.0f && p.hum_strength == 0.0f &&
                  p.noise_strength == 0.0f && p.flicker_strength == 0.0f, msg);
        }
        CrtRenderer_SetProfile(CRT_PROFILE_OFF);
    }
}

static void test_path_equivalence(void) {
    crt_params_t p;
    measure_t direct, multi;
    double d;

    printf("direct vs multi-pass\n");

    upload_bright_square(210, 45);

    neutral_params(&p);
    p.mask_layout = CRT_MASK_SLOT;
    p.mask_pitch_pixels = 6.0f;
    p.mask_strength = 0.45f;
    p.face_geometry = CRT_FACE_SPHERICAL;
    p.warp_x = p.warp_y = 0.08f;
    p.corner_radius = 0.06f;
    p.vignette_strength = 0.10f;
    p.phosphor = CRT_PHOSPHOR_SMPTE_C;
    p.white_point_kelvin = 9300.0f;
    p.tube_gamma = 2.4f;
    p.headroom_policy = CRT_HEADROOM_PRESERVE_PEAKS;

    CrtRendererGL_TestForceFullPath(0);
    CrtRenderer_SetParams(&p);
    if (!render_and_measure("path_direct", &direct)) return;

    CrtRendererGL_TestForceFullPath(1);
    CrtRenderer_SetParams(&p);
    if (!render_and_measure("path_multi", &multi)) { CrtRendererGL_TestForceFullPath(0); return; }
    CrtRendererGL_TestForceFullPath(0);

    d = fabs(luma_of(&direct) - luma_of(&multi));
    printf("    direct %.3f   multi-pass %.3f   difference %.3f codes\n",
           luma_of(&direct), luma_of(&multi), d);
    check(d < 1.5, "the two paths agree to within a code value");
    check(fabs((direct.r - direct.b) - (multi.r - multi.b)) < 1.0,
          "and agree on colour balance, not just brightness");
}

static void test_limited_precision(void) {
    crt_params_t p;
    measure_t full, limited;

    printf("RGBA8 fallback\n");

    upload_bright_square(210, 45);

    neutral_params(&p);
    p.mask_layout = CRT_MASK_SLOT;
    p.mask_pitch_pixels = 6.0f;
    p.mask_strength = 0.45f;
    p.bloom_strength = 0.30f;
    p.bloom_radius_lines = 3.0f;
    p.bloom_threshold = 0.2f;
    p.headroom_policy = CRT_HEADROOM_PRESERVE_PEAKS;

    CrtRendererGL_TestForceLimitedPrecision(0);
    CrtRenderer_SetParams(&p);
    if (!render_and_measure(NULL, &full)) return;

    CrtRendererGL_TestForceLimitedPrecision(1);
    CrtRenderer_SetParams(&p);
    if (!render_and_measure("rgba8_fallback", &limited)) {
        CrtRendererGL_TestForceLimitedPrecision(0);
        return;
    }

    printf("    RGBA16F %.2f   RGBA8 %.2f   r-b %.2f vs %.2f\n",
           luma_of(&full), luma_of(&limited),
           full.r - full.b, limited.r - limited.b);

    check(luma_of(&limited) > 1.0, "the fallback still renders a picture");
    check(fabs(luma_of(&full) - luma_of(&limited)) < 12.0,
          "the fallback stays close to the float path");
    check(fabs((full.r - full.b) - (limited.r - limited.b)) < 3.0,
          "and introduces no colour cast");

    CrtRendererGL_TestForceLimitedPrecision(0);
}

static void test_resize_stability(void) {
    static const int HEIGHTS[] = {
        700, 716, 732, 748, 764, 780, 796, 812, 828, 844,
        860, 876, 892, 908, 924, 940, 956, 972, 988, 1004
    };
    crt_params_t p;
    double mod[sizeof HEIGHTS / sizeof HEIGHTS[0]];
    double worst_jump = 0.0, lo = 1e9, hi = -1e9;
    int worst_at = 0;
    size_t i;
    int keep_w = s_vp_w, keep_h = s_vp_h;

    printf("resize stability (mask contrast across a continuous resize)\n");

    upload_flat(160, 160, 160);

    CrtRenderer_SetCurve(CRT_CURVE_PROFILE);
    CrtRenderer_SetProfile(CRT_PROFILE_SGB_CONSUMER);
    CrtRenderer_GetParams(&p);

    for (i = 0; i < sizeof HEIGHTS / sizeof HEIGHTS[0]; i++) {
        SDL_SetWindowSize(s_window, HEIGHTS[i] * 4 / 3, HEIGHTS[i]);
        SDL_PumpEvents();
        SDL_GL_GetDrawableSize(s_window, &s_vp_w, &s_vp_h);
        CrtRenderer_SetParams(&p);
        mod[i] = measure_column_modulation(NULL);
        if (mod[i] < lo) lo = mod[i];
        if (mod[i] > hi) hi = mod[i];
        if (i > 0) {
            double jump = fabs(mod[i] - mod[i - 1]);
            if (jump > worst_jump) { worst_jump = jump; worst_at = (int)i; }
        }
    }

    printf("    contrast %.4f .. %.4f over %d..%d lines\n",
           lo, hi, HEIGHTS[0], HEIGHTS[(sizeof HEIGHTS / sizeof HEIGHTS[0]) - 1]);
    printf("    largest step-to-step jump %.4f (at %d -> %d lines)\n",
           worst_jump, HEIGHTS[worst_at - 1], HEIGHTS[worst_at]);

    check(worst_jump < 0.15, "the mask changes smoothly across a resize");

    check(lo > 0.001, "the mask never disappears entirely mid-range");
    check(hi < 1.0, "and never saturates into a hard pattern");

    s_vp_w = keep_w; s_vp_h = keep_h;
    SDL_SetWindowSize(s_window, keep_w, keep_h);
    SDL_PumpEvents();
    SDL_GL_GetDrawableSize(s_window, &s_vp_w, &s_vp_h);
    CrtRenderer_SetProfile(CRT_PROFILE_OFF);
}

static void test_cross_talk(void) {
    crt_params_t base, mod;
    double r0, r1, c0, c1, l0, l1;
    char msg[160];
    size_t i;

    printf("cross-talk (each control stays in its own dimension)\n");

    {
        static const struct { const char *name; void (*apply)(crt_params_t *); } H[] = {
            { "spot_sigma",    NULL }, { "recon_sharpness", NULL },
            { "recon_radius",  NULL },
        };
        upload_flat(140, 140, 140);

        neutral_params(&base);
        base.beam_sigma_min = base.beam_sigma_max = 0.22f;
        CrtRenderer_SetParams(&base);
        r0 = measure_row_modulation(NULL);

        for (i = 0; i < sizeof H / sizeof H[0]; i++) {
            mod = base;
            if (i == 0) { mod.spot_sigma_dark = mod.spot_sigma_bright = 0.8f; }
            if (i == 1) { mod.reconstruction = CRT_RECON_GAUSSIAN;
                          mod.reconstruction_sharpness = 0.95f; }
            if (i == 2) { mod.reconstruction = CRT_RECON_GAUSSIAN;
                          mod.reconstruction_radius = 3.0f; }
            CrtRenderer_SetParams(&mod);
            r1 = measure_row_modulation(NULL);
            snprintf(msg, sizeof msg,
                     "%s leaves scanline contrast alone (%.4f vs %.4f)",
                     H[i].name, r0, r1);
            check(fabs(r1 - r0) < 0.02, msg);
        }
        printf("    horizontal controls vs scanline contrast: %.4f baseline\n", r0);
    }

    {
        upload_vertical_grating(230, 20);

        neutral_params(&base);
        base.reconstruction = CRT_RECON_GAUSSIAN;
        base.reconstruction_radius = 1.0f;
        base.beam_sigma_min = base.beam_sigma_max = 0.20f;
        CrtRenderer_SetParams(&base);
        c0 = measure_column_modulation(NULL);

        mod = base; mod.beam_sigma_min = mod.beam_sigma_max = 0.60f;
        CrtRenderer_SetParams(&mod);
        c1 = measure_column_modulation(NULL);
        snprintf(msg, sizeof msg,
                 "beam width leaves horizontal detail alone (%.4f vs %.4f)", c0, c1);
        check(fabs(c1 - c0) < 0.02, msg);

        mod = base; mod.beam_shape_exponent = 4.0f;
        CrtRenderer_SetParams(&mod);
        c1 = measure_column_modulation(NULL);
        snprintf(msg, sizeof msg,
                 "beam shape leaves horizontal detail alone (%.4f vs %.4f)", c0, c1);
        check(fabs(c1 - c0) < 0.02, msg);

        mod = base; mod.scanline_phase = 0.0f;
        CrtRenderer_SetParams(&mod);
        c1 = measure_column_modulation(NULL);
        snprintf(msg, sizeof msg,
                 "scanline phase leaves horizontal detail alone (%.4f vs %.4f)", c0, c1);
        check(fabs(c1 - c0) < 0.02, msg);

        printf("    vertical controls vs horizontal detail: %.4f baseline\n", c0);
    }

    {
        measure_t m;
        upload_flat(180, 180, 180);

        neutral_params(&base);
        CrtRenderer_SetParams(&base);
        render_and_measure(NULL, &m); l0 = luma_of(&m);

        mod = base; mod.raster_pincushion = 0.10f;
        CrtRenderer_SetParams(&mod);
        render_and_measure(NULL, &m); l1 = luma_of(&m);
        snprintf(msg, sizeof msg, "pincushion costs no brightness (%.1f vs %.1f)", l0, l1);
        check(fabs(l1 - l0) < 2.0, msg);

        mod = base; mod.raster_rotation = 0.05f;
        CrtRenderer_SetParams(&mod);
        render_and_measure(NULL, &m); l1 = luma_of(&m);
        snprintf(msg, sizeof msg, "rotation costs no brightness (%.1f vs %.1f)", l0, l1);
        check(fabs(l1 - l0) < 2.0, msg);

        mod = base; mod.raster_keystone = 0.10f;
        CrtRenderer_SetParams(&mod);
        render_and_measure(NULL, &m); l1 = luma_of(&m);
        snprintf(msg, sizeof msg, "keystone costs no brightness (%.1f vs %.1f)", l0, l1);
        check(fabs(l1 - l0) < 2.0, msg);

        printf("    geometry vs centre brightness: %.1f baseline\n", l0);
    }

    {
        upload_flat(140, 140, 140);
        neutral_params(&base);
        base.beam_sigma_min = base.beam_sigma_max = 0.22f;
        CrtRenderer_SetParams(&base);
        r0 = measure_row_modulation(NULL);

        mod = base; mod.phosphor = CRT_PHOSPHOR_SMPTE_C; mod.white_point_kelvin = 9300.0f;
        CrtRenderer_SetParams(&mod);
        r1 = measure_row_modulation(NULL);
        snprintf(msg, sizeof msg,
                 "phosphor and white point leave scanlines alone (%.4f vs %.4f)", r0, r1);
        check(fabs(r1 - r0) < 0.02, msg);

        upload_vertical_grating(230, 20);
        mod = base; mod.reconstruction = CRT_RECON_GAUSSIAN;
        CrtRenderer_SetParams(&mod);
        c0 = measure_column_modulation(NULL);
        mod.phosphor = CRT_PHOSPHOR_SMPTE_C; mod.white_point_kelvin = 9300.0f;
        CrtRenderer_SetParams(&mod);
        c1 = measure_column_modulation(NULL);
        snprintf(msg, sizeof msg,
                 "phosphor and white point leave horizontal detail alone (%.4f vs %.4f)",
                 c0, c1);
        check(fabs(c1 - c0) < 0.02, msg);
    }

    {
        upload_flat(140, 140, 140);
        neutral_params(&base);
        base.beam_sigma_min = base.beam_sigma_max = 0.22f;
        base.spot_sigma_dark = base.spot_sigma_bright = 0.30f;
        CrtRenderer_SetParams(&base);
        r0 = measure_row_modulation(NULL);

        mod = base; mod.focus_edge = 1.0f;
        CrtRenderer_SetParams(&mod);
        r1 = measure_row_modulation(NULL);
        printf("    focus_edge vs scanline contrast: %.4f -> %.4f\n", r0, r1);
        check(fabs(r1 - r0) > 0.001,
              "focus_edge DOES touch both axes -- deliberate, not leakage");
    }

    upload_flat(128, 128, 128);
}

static void test_profiles(void) {
    static const crt_profile_id_t ids[3] = {
        CRT_PROFILE_CLEAN, CRT_PROFILE_SGB_CONSUMER, CRT_PROFILE_SGB_RGB_PVM
    };
    crt_params_t p;
    measure_t m;
    char buf[160];
    int i;

    printf("shipping profiles\n");

    for (i = 0; i < 3; i++) {
        measure_t hi, mid;

        CrtRenderer_SetCurve(CRT_CURVE_PROFILE);
        CrtRenderer_SetProfile(ids[i]);
        CrtRenderer_GetParams(&p);

        upload_flat(255, 255, 255);
        if (!render_and_measure(CrtRenderer_ProfileName(ids[i]), &m)) continue;

        upload_flat(230, 230, 230);
        if (!render_and_measure(NULL, &hi)) continue;

        upload_flat(128, 128, 128);
        if (!render_and_measure(NULL, &mid)) continue;

        printf("    %-8s white renders at %.0f/255 (mask peak %.3f)\n",
               CrtRenderer_ProfileName(ids[i]), m.r,
               (double)CrtRenderer_MaskPeak(p.mask_layout, p.mask_strength,
                                            CrtRenderer_MaskPitchForViewport(&p, s_vp_h)));

        snprintf(buf, sizeof buf, "%s: has headroom below white (%ld px at 230)",
                 CrtRenderer_ProfileName(ids[i]), hi.clipped);
        check(p.headroom_policy != CRT_HEADROOM_PRESERVE_PEAKS || hi.clipped == 0,
              buf);

        if (p.white_point_kelvin <= 0.0f ||
            fabsf(p.white_point_kelvin - 6504.0f) < 100.0f) {
            snprintf(buf, sizeof buf, "%s: mid gray stays neutral (r-b %.2f)",
                     CrtRenderer_ProfileName(ids[i]), mid.r - mid.b);
            check(fabs(mid.r - mid.b) < 2.0, buf);
        } else {
            snprintf(buf, sizeof buf,
                     "%s: %.0fK tube renders grey BLUER (r-b %.2f)",
                     CrtRenderer_ProfileName(ids[i]),
                     (double)p.white_point_kelvin, mid.r - mid.b);
            check(mid.r - mid.b < -2.0, buf);
        }

        snprintf(buf, sizeof buf, "%s: white is not over-dimmed (%.0f/255)",
                 CrtRenderer_ProfileName(ids[i]), m.r);
        check(m.r > 160.0, buf);
    }

    upload_flat(255, 255, 255);
    CrtRenderer_SetProfile(CRT_PROFILE_CLEAN);
    CrtRenderer_SetCurve(CRT_CURVE_TV);
    if (render_and_measure("clean_tv_curve", &m)) {

        checkf(luma_of(&m) > 160.0,
               "CLEAN + TV curve costs no extra brightness (luma %.1f, >160)",
               luma_of(&m), 160.0);
    }
    CrtRenderer_SetCurve(CRT_CURVE_PROFILE);
}

static void test_perf(void) {
    static const int sizes[][2] = {
        { 1280,  960 }, { 1920, 1080 }, { 2560, 1440 },
    };
    crt_params_t p;
    measure_t m;
    size_t i;

    printf("performance (GPU time per frame, swap excluded)\n");
    upload_flat(180, 180, 180);

    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        int want_w = sizes[i][0], want_h = sizes[i][1];
        int q;

        SDL_SetWindowSize(s_window, want_w, want_h);
        SDL_PumpEvents();
        SDL_GL_GetDrawableSize(s_window, &s_vp_w, &s_vp_h);

        for (q = 0; q < 2; q++) {
            Uint64 t0, t1;
            int frame;
            const int FRAMES = 60;

            CrtRenderer_SetProfile(CRT_PROFILE_SGB_CONSUMER);
            CrtRenderer_GetParams(&p);
            p.quality = q ? CRT_QUALITY_FULL : CRT_QUALITY_FAST;
            CrtRenderer_SetParams(&p);

            render_and_measure(NULL, &m);

            t0 = SDL_GetPerformanceCounter();
            for (frame = 0; frame < FRAMES; frame++) {
                crt_frame_desc_t d;
                fill_desc(&d);
                glViewport(0, 0, s_vp_w, s_vp_h);
                CrtRendererGL_Draw(s_src_tex, &d, 0, 0, s_vp_w, s_vp_h, 0.0);
            }
            glFinish();
            t1 = SDL_GetPerformanceCounter();

            printf("  %4dx%-4d %-4s  %6.3f ms/frame\n", s_vp_w, s_vp_h,
                   q ? "full" : "fast",
                   1000.0 * (double)(t1 - t0) /
                   (double)SDL_GetPerformanceFrequency() / FRAMES);
        }
    }
}

static void dump_bmp(const char *name, const unsigned char *rgba, int w, int h) {
    char path[600];
    FILE *f;
    int y, x;
    unsigned char hdr[54];
    int row = w * 3, pad = (4 - (row % 4)) % 4;
    int size = 54 + (row + pad) * h;

    snprintf(path, sizeof path, "%s/%s.bmp", s_dump_dir, name);
    f = fopen(path, "wb");
    if (!f) return;

    memset(hdr, 0, sizeof hdr);
    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2] = (unsigned char)size; hdr[3] = (unsigned char)(size >> 8);
    hdr[4] = (unsigned char)(size >> 16); hdr[5] = (unsigned char)(size >> 24);
    hdr[10] = 54; hdr[14] = 40;
    hdr[18] = (unsigned char)w; hdr[19] = (unsigned char)(w >> 8);
    hdr[20] = (unsigned char)(w >> 16); hdr[21] = (unsigned char)(w >> 24);
    hdr[22] = (unsigned char)h; hdr[23] = (unsigned char)(h >> 8);
    hdr[24] = (unsigned char)(h >> 16); hdr[25] = (unsigned char)(h >> 24);
    hdr[26] = 1; hdr[28] = 24;
    fwrite(hdr, 1, sizeof hdr, f);

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            const unsigned char *p = rgba + ((size_t)y * w + x) * 4;
            unsigned char bgr[3] = { p[2], p[1], p[0] };
            fwrite(bgr, 1, 3, f);
        }
        for (x = 0; x < pad; x++) fputc(0, f);
    }
    fclose(f);
}

static void glBindFramebuffer_compat(void) {
    if (gl_BindFramebuffer) gl_BindFramebuffer(GL_FRAMEBUFFER, 0);
}

int main(int argc, char **argv) {
    int i, want_perf = 0;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--perf")) want_perf = 1;
        else if (!strcmp(argv[i], "--dump") && i + 1 < argc) s_dump_dir = argv[++i];
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 2;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    s_window = SDL_CreateWindow("crt_harness", SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, 1280, 960,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!s_window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 2;
    }
    s_ctx = SDL_GL_CreateContext(s_window);
    if (!s_ctx) {
        printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return 2;
    }
    SDL_GL_GetDrawableSize(s_window, &s_vp_w, &s_vp_h);

    if (GlApi_Load() != 0) {
        printf("GlApi_Load failed -- cannot measure\n");
        return 2;
    }
    if (!DisplayGL_ShaderDir()) {
        printf("no shaders/crt directory found; run from the repo root\n");
        return 2;
    }

    glGenTextures(1, &s_src_tex);
    CrtRenderer_Init();
    if (CrtRendererGL_Init() != 0) {
        printf("CrtRendererGL_Init failed -- see the log above\n");
        return 2;
    }

    printf("crt_harness: %dx%d drawable, GL %s\n\n",
           s_vp_w, s_vp_h, (const char *)glGetString(GL_VERSION));

    test_scanlines();
    test_beam_shape();
    test_black_and_white();
    test_corner_radius_brightness();
    test_mask_energy();
    test_phosphor_render();
    test_headroom();
    test_output_transfer();
    test_horizontal_response();
    test_active_rect_sampling();
    test_parameter_sensitivity();
    test_defects();
    test_path_equivalence();
    test_limited_precision();
    test_resize_stability();
    test_cross_talk();
    test_bloom();
    test_profiles();
    if (want_perf) test_perf();

    printf("\n%s\n", s_fail ? "FAILED" : "all passed");

    CrtRendererGL_Shutdown();
    SDL_GL_DeleteContext(s_ctx);
    SDL_DestroyWindow(s_window);
    SDL_Quit();
    return s_fail ? 1 : 0;
}
