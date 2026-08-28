/* SPDX-License-Identifier: MIT
 *
 * crt_renderer_tests.c, CPU-side invariant tests for the CRT renderer.
 *
 * Standalone on purpose: it links crt_renderer.c and nothing else, so it
 * builds under RED_ONLY where pokered_tests does not exist, and a failure here
 * can only mean the renderer maths is wrong.
 */

#include "../src/platform/crt_renderer.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_fail;

static void check(int cond, const char *what) {
    if (!cond) {
        printf("  FAIL  %s\n", what);
        g_fail++;
    }
}

static void check_eq_i(int got, int want, const char *what) {
    if (got != want) {
        printf("  FAIL  %s (got %d, want %d)\n", what, got, want);
        g_fail++;
    }
}

static crt_frame_desc_t sgb_desc(int scale) {
    crt_frame_desc_t f;
    memset(&f, 0, sizeof f);
    f.texture_width  = 256 * scale;
    f.texture_height = 224 * scale;
    f.raster_width   = 256;
    f.raster_height  = 224;
    f.texture_active_rect = (crt_rect_i_t){ 0, 0, 256 * scale, 224 * scale };
    f.raster_active_rect  = (crt_rect_i_t){ 0, 0, 256, 224 };
    f.pixel_aspect_num = 8;
    f.pixel_aspect_den = 7;
    f.scan_mode      = CRT_SCAN_PROGRESSIVE;
    f.field          = CRT_FIELD_NONE;
    f.input_transfer = CRT_TRANSFER_SRGB;
    f.input_gamma    = 2.2f;
    f.source_refresh_hz = 59.7275f;
    f.frame_number   = 1;
    return f;
}

static void test_validation(void) {
    crt_frame_desc_t f;
    const char *why;

    printf("validation\n");

    f = sgb_desc(1);
    check(CrtRenderer_ValidateFrame(&f, &why), "raw 256x224 accepted");
    f = sgb_desc(2);
    check(CrtRenderer_ValidateFrame(&f, &why), "decoded 512x448 accepted");

    f = sgb_desc(2);
    f.raster_width = 512; f.raster_height = 448;
    f.raster_active_rect = (crt_rect_i_t){ 0, 0, 512, 448 };
    check(CrtRenderer_ValidateFrame(&f, &why), "512x448 progressive raster accepted");

    f = sgb_desc(1);
    f.scan_mode = CRT_SCAN_INTERLACED;
    f.field = CRT_FIELD_EVEN;
    check(!CrtRenderer_ValidateFrame(&f, &why), "interlaced scan rejected as unsupported");

    f = sgb_desc(1); f.texture_width = 0;
    check(!CrtRenderer_ValidateFrame(&f, &why), "zero texture width rejected");
    f = sgb_desc(1); f.raster_height = -1;
    check(!CrtRenderer_ValidateFrame(&f, &why), "negative raster height rejected");
    f = sgb_desc(1); f.pixel_aspect_den = 0;
    check(!CrtRenderer_ValidateFrame(&f, &why), "zero aspect denominator rejected");

    f = sgb_desc(1); f.texture_active_rect.w = 257;
    check(!CrtRenderer_ValidateFrame(&f, &why), "texture rect overflow rejected");
    f = sgb_desc(1); f.raster_active_rect.y = 1;
    check(!CrtRenderer_ValidateFrame(&f, &why), "raster rect overflow rejected");

    f = sgb_desc(1); f.field = CRT_FIELD_ODD;
    check(!CrtRenderer_ValidateFrame(&f, &why), "progressive with field rejected");

    check(!CrtRenderer_ValidateFrame(NULL, &why), "null descriptor rejected");
}

static void test_viewport(void) {
    crt_frame_desc_t raw = sgb_desc(1), dec = sgb_desc(2);
    crt_rect_i_t a, b;

    printf("viewport\n");

    CrtRenderer_ComputeViewport(&raw, CRT_ASPECT_SOURCE_PAR, 1920, 1080, &a);
    CrtRenderer_ComputeViewport(&dec, CRT_ASPECT_SOURCE_PAR, 1920, 1080, &b);
    check(a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h,
          "256x224 and 512x448 give the same viewport");

    CrtRenderer_ComputeViewport(&raw, CRT_ASPECT_SOURCE_PAR, 1920, 1080, &a);
    check_eq_i(a.h, 1080, "source-PAR is height-limited at 1920x1080");
    check(a.w == (int)(1080.0 * (256.0 * 8.0) / (224.0 * 7.0) + 0.5),
          "source-PAR width follows the rational aspect");
    check_eq_i(a.x, (1920 - a.w) / 2, "source-PAR centred horizontally");

    CrtRenderer_ComputeViewport(&raw, CRT_ASPECT_4_3, 1920, 1080, &a);
    check_eq_i(a.h, 1080, "4:3 height-limited at 1920x1080");
    check_eq_i(a.w, 1440, "4:3 gives 1440x1080");

    CrtRenderer_ComputeViewport(&raw, CRT_ASPECT_RAW, 1920, 1080, &a);
    check_eq_i(a.h, 1080, "raw height-limited at 1920x1080");
    check_eq_i(a.w, (int)(1080.0 * 256.0 / 224.0 + 0.5), "raw uses 256:224");

    CrtRenderer_ComputeViewport(&raw, CRT_ASPECT_4_3, 640, 1080, &a);
    check_eq_i(a.w, 640, "narrow drawable is width-limited");
    check_eq_i(a.h, 480, "4:3 of 640 wide is 480 tall");
    check_eq_i(a.y, (1080 - 480) / 2, "centred vertically when pillarboxed");

    CrtRenderer_ComputeViewport(&raw, CRT_ASPECT_4_3, 0, 0, &a);
    check(a.w == 0 && a.h == 0, "zero drawable yields an empty viewport");
}

static void test_persistence(void) {
    float tau = 0.020f;
    float w60, w120, wvar;

    printf("persistence\n");

    w60  = CrtRenderer_HistoryWeight(tau, 1.0f / 60.0f);
    w120 = CrtRenderer_HistoryWeight(tau, 1.0f / 120.0f);
    check(fabs((double)(w120 * w120) - (double)w60) < 1e-6,
          "two 120Hz steps equal one 60Hz step");

    wvar = CrtRenderer_HistoryWeight(tau, 0.0f);
    check(wvar == 1.0f, "zero elapsed time keeps history intact");

    check(CrtRenderer_HistoryWeight(0.0f, 1.0f / 60.0f) == 0.0f,
          "zero tau disables persistence");
    check(CrtRenderer_HistoryWeight(-1.0f, 1.0f / 60.0f) == 0.0f,
          "negative tau disables persistence");
    check(w60 > 0.0f && w60 < 1.0f, "60Hz weight is a proper fraction");
}

static void test_params(void) {
    crt_params_t p;

    printf("parameters\n");

    CrtRenderer_Init();

    check(CrtRenderer_SetProfile(CRT_PROFILE_OFF), "OFF profile selectable");
    check(CrtRenderer_GetParams(&p), "params readable");

    check(p.mask_strength == 0.0f && p.bloom_strength == 0.0f &&
          p.halation_strength == 0.0f && p.warp_x == 0.0f && p.warp_y == 0.0f &&
          p.corner_radius == 0.0f && p.vignette_strength == 0.0f &&
          p.spot_sigma_dark == 0.0f && p.spot_sigma_bright == 0.0f &&
          p.phosphor == CRT_PHOSPHOR_NONE && p.tube_gamma == 0.0f &&
          p.black_level == 0.0f &&
          p.convergence_r_x == 0.0f && p.convergence_g_x == 0.0f &&
          p.convergence_b_x == 0.0f,
          "OFF profile is exactly neutral");

    check(p.scanline_strength == 1.0f, "OFF leaves the beam unattenuated");

    check(CrtRenderer_SetProfile(CRT_PROFILE_SGB_CONSUMER), "consumer selectable");
    CrtRenderer_GetParams(&p);
    check(p.mask_layout == CRT_MASK_SLOT, "consumer uses a slot mask");

    check(p.convergence_r_x == 0.0f && p.convergence_b_x == 0.0f &&
          p.convergence_r_y == 0.0f && p.convergence_b_y == 0.0f,
          "consumer profile ships with zero convergence error");

    check(CrtRenderer_SetProfile(CRT_PROFILE_SGB_RGB_PVM), "PVM selectable");
    CrtRenderer_GetParams(&p);
    check(p.mask_layout == CRT_MASK_APERTURE_GRILLE, "PVM uses an aperture grille");
    check(p.warp_x == 0.0f && p.warp_y == 0.0f, "PVM glass is flat");

    memset(&p, 0, sizeof p);
    p.mask_pitch_pixels = 1000.0f;
    p.beam_sigma_min    = -5.0f;
    p.mask_strength     = 4.0f;
    p.warp_x            = 99.0f;
    p.convergence_r_x   = -50.0f;
    p.bloom_strength    = (float)NAN;
    CrtRenderer_SetParams(&p);
    CrtRenderer_GetParams(&p);
    check(p.mask_pitch_pixels <= 12.0f, "mask pitch clamped");
    check(p.beam_sigma_min >= 0.10f, "beam sigma clamped");
    check(p.mask_strength <= 1.0f, "mask strength clamped");
    check(p.warp_x <= 0.15f, "warp clamped");
    check(p.convergence_r_x >= -3.0f, "convergence clamped");
    check(p.bloom_strength == p.bloom_strength, "NaN scrubbed from bloom");
    check(p.beam_sigma_max >= p.beam_sigma_min, "beam sigma pair ordered");

    CrtRenderer_SetProfile(CRT_PROFILE_OFF);
}

static void test_profile_ranges(void) {
    const crt_profile_id_t ids[3] = {
        CRT_PROFILE_CLEAN, CRT_PROFILE_SGB_CONSUMER, CRT_PROFILE_SGB_RGB_PVM
    };
    const char *names[3] = { "clean", "consumer", "pvm" };
    int i;

    printf("profile ranges\n");

    for (i = 0; i < 3; i++) {
        crt_params_t p, after;
        char msg[128];

        CrtRenderer_SetProfile(ids[i]);
        CrtRenderer_GetParams(&p);

        CrtRenderer_SetParams(&p);
        CrtRenderer_GetParams(&after);
        snprintf(msg, sizeof msg, "%s profile survives clamping unchanged", names[i]);
        check(memcmp(&p, &after, sizeof p) == 0, msg);

        snprintf(msg, sizeof msg, "%s bloom radius in range", names[i]);
        check(p.bloom_radius_pixels >= 0.0f && p.bloom_radius_pixels <= 24.0f, msg);
        snprintf(msg, sizeof msg, "%s halation radius in range", names[i]);
        check(p.halation_radius_pixels >= 0.0f && p.halation_radius_pixels <= 64.0f, msg);
        snprintf(msg, sizeof msg, "%s glow strengths in range", names[i]);
        check(p.bloom_strength >= 0.0f && p.bloom_strength <= 1.0f &&
              p.halation_strength >= 0.0f && p.halation_strength <= 1.0f, msg);
        snprintf(msg, sizeof msg, "%s mask pitch in range", names[i]);
        check(p.mask_pitch_pixels >= 2.0f && p.mask_pitch_pixels <= 12.0f, msg);
        snprintf(msg, sizeof msg, "%s declares a valid headroom policy", names[i]);
        check(p.headroom_policy == CRT_HEADROOM_CLIP ||
              p.headroom_policy == CRT_HEADROOM_PRESERVE_PEAKS, msg);
        snprintf(msg, sizeof msg, "%s overscan in range", names[i]);
        check(p.overscan_x >= 0.0f && p.overscan_x <= 0.15f &&
              p.overscan_y >= 0.0f && p.overscan_y <= 0.15f, msg);

        snprintf(msg, sizeof msg, "%s bloom radius and strength agree", names[i]);
        check((p.bloom_radius_pixels > 0.0f) == (p.bloom_strength > 0.0f), msg);
        snprintf(msg, sizeof msg, "%s halation radius and strength agree", names[i]);
        check((p.halation_radius_pixels > 0.0f) == (p.halation_strength > 0.0f), msg);
    }

    CrtRenderer_SetProfile(CRT_PROFILE_OFF);
}

static void test_curve_override(void) {
    crt_params_t p;
    float declared_warp, declared_corner;

    printf("curve override\n");

    CrtRenderer_SetCurve(CRT_CURVE_PROFILE);
    CrtRenderer_SetProfile(CRT_PROFILE_SGB_CONSUMER);
    CrtRenderer_GetParams(&p);
    declared_warp = p.warp_x;
    declared_corner = p.corner_radius;
    check(declared_warp > 0.0f, "consumer profile declares curvature");

    CrtRenderer_SetCurve(CRT_CURVE_OFF);
    CrtRenderer_GetParams(&p);
    check(p.warp_x == 0.0f && p.warp_y == 0.0f && p.corner_radius == 0.0f,
          "OFF flattens a curved profile");

    CrtRenderer_SetCurve(CRT_CURVE_PROFILE);
    CrtRenderer_GetParams(&p);
    check(p.warp_x == declared_warp && p.corner_radius == declared_corner,
          "PROFILE restores the profile's own curvature after an override");

    CrtRenderer_SetProfile(CRT_PROFILE_CLEAN);
    CrtRenderer_GetParams(&p);
    check(p.warp_x == 0.0f, "clean profile is flat by default");

    CrtRenderer_SetCurve(CRT_CURVE_TV);
    CrtRenderer_GetParams(&p);
    check(p.warp_x > 0.0f && p.corner_radius > 0.0f,
          "TV curve applies to the clean profile");
    check(p.mask_layout == CRT_MASK_APERTURE_GRILLE,
          "curving a profile does not disturb its mask");
    check(p.corner_softness > 0.0f,
          "a corner radius always gets a feather to antialias against");

    CrtRenderer_SetProfile(CRT_PROFILE_SGB_RGB_PVM);
    CrtRenderer_GetParams(&p);
    check(p.warp_x > 0.0f, "curve override survives a profile change");

    CrtRenderer_SetCurve(CRT_CURVE_PROFILE);
    CrtRenderer_SetProfile(CRT_PROFILE_OFF);
}

static void test_mask_density(void) {
    crt_params_t p;
    float p960, p1440, p2160;
    int i;

    printf("mask triad density\n");

    CrtRenderer_SetProfile(CRT_PROFILE_SGB_CONSUMER);
    CrtRenderer_GetParams(&p);
    check(p.mask_triads_per_picture_height > 0.0f,
          "consumer profile declares a triad density");

    p960  = CrtRenderer_MaskPitchForViewport(&p, 960);
    p1440 = CrtRenderer_MaskPitchForViewport(&p, 1440);
    p2160 = CrtRenderer_MaskPitchForViewport(&p, 2160);

    check(p1440 > p960 && p2160 > p1440, "pitch grows with viewport height");
    check(fabsf(p1440 / p960 - 1.5f) < 0.01f,
          "pitch scales linearly with height (1440/960 == 1.5)");
    check(fabsf((960.0f / p960) - (1440.0f / p1440)) < 0.01f,
          "triads per picture height are identical at every resolution");

    check(fabsf(p960 - 960.0f / p.mask_triads_per_picture_height) < 0.001f,
          "pitch is exactly viewport_height / triad density");

    p.mask_triads_per_picture_height = 0.0f;
    p.mask_pitch_pixels = 5.0f;
    check(CrtRenderer_MaskPitchForViewport(&p, 960) == 5.0f &&
          CrtRenderer_MaskPitchForViewport(&p, 2160) == 5.0f,
          "no density falls back to the authored pitch");

    for (i = 0; i < 3; i++) {
        static const crt_profile_id_t ids[3] = {
            CRT_PROFILE_CLEAN, CRT_PROFILE_SGB_CONSUMER, CRT_PROFILE_SGB_RGB_PVM
        };
        char msg[160];
        float pitch;
        CrtRenderer_SetProfile(ids[i]);
        CrtRenderer_GetParams(&p);
        pitch = CrtRenderer_MaskPitchForViewport(&p, 960);
        snprintf(msg, sizeof msg,
                 "%s mask is representable at 960 lines (%.2f px/triad)",
                 CrtRenderer_ProfileName(ids[i]), pitch);
        check(pitch >= 3.0f, msg);
    }

    {
        float pitches[3];
        static const crt_profile_id_t ids[3] = {
            CRT_PROFILE_CLEAN, CRT_PROFILE_SGB_CONSUMER, CRT_PROFILE_SGB_RGB_PVM
        };
        for (i = 0; i < 3; i++) {
            CrtRenderer_SetProfile(ids[i]);
            CrtRenderer_GetParams(&p);
            pitches[i] = CrtRenderer_MaskPitchForViewport(&p, 960);
        }
        check(fabsf(pitches[0] - pitches[1]) > 0.5f &&
              fabsf(pitches[1] - pitches[2]) > 0.5f &&
              fabsf(pitches[0] - pitches[2]) > 0.5f,
              "the three profiles have visibly different triad densities");
    }

    CrtRenderer_SetProfile(CRT_PROFILE_OFF);
}

static void test_phosphor(void) {
    static const crt_phosphor_t sets[3] = {
        CRT_PHOSPHOR_NONE, CRT_PHOSPHOR_SMPTE_C, CRT_PHOSPHOR_EBU
    };
    static const char *names[3] = { "none", "SMPTE-C", "EBU" };
    crt_params_t p;
    int i, r, c;

    printf("phosphor colorimetry\n");

    for (i = 0; i < 3; i++) {
        const float *m = CrtRenderer_PhosphorMatrix(sets[i], 6504.0f);
        char msg[128];
        int rows_ok = 1;
        for (r = 0; r < 3; r++) {
            float sum = 0.0f;
            for (c = 0; c < 3; c++) sum += m[r * 3 + c];
            if (fabsf(sum - 1.0f) > 1e-5f) rows_ok = 0;
        }
        snprintf(msg, sizeof msg,
                 "%s at D65: white maps to white (rows sum to 1)", names[i]);
        check(rows_ok, msg);
    }

    {
        const float *m = CrtRenderer_PhosphorMatrix(CRT_PHOSPHOR_NONE, 6504.0f);
        check(m[0] == 1.0f && m[1] == 0.0f && m[2] == 0.0f &&
              m[3] == 0.0f && m[4] == 1.0f && m[5] == 0.0f &&
              m[6] == 0.0f && m[7] == 0.0f && m[8] == 1.0f,
              "NONE is exactly the identity");
    }

    {
        const float *m = CrtRenderer_PhosphorMatrix(CRT_PHOSPHOR_SMPTE_C, 6504.0f);
        check(fabsf(m[0] - 1.0f) > 0.01f,
              "SMPTE-C measurably differs from sRGB");
    }

    {
        crt_frame_desc_t f = sgb_desc(1);
        memset(&p, 0, sizeof p);
        check(CrtRenderer_TubeGammaExponent(&p, &f) == 1.0f,
              "no tube gamma is exactly 1.0");
        p.tube_gamma = 2.2f;
        check(CrtRenderer_TubeGammaExponent(&p, &f) == 1.0f,
              "2.2 tube gamma against a 2.2 source is exactly 1.0");
        p.tube_gamma = 2.4f;
        check(CrtRenderer_TubeGammaExponent(&p, &f) > 1.0f,
              "2.4 tube gamma darkens midtones");
    }

    memset(&p, 0, sizeof p);
    p.profile = CRT_PROFILE_CLEAN;
    p.tube_gamma = 9.0f;
    p.phosphor = (crt_phosphor_t)99;
    CrtRenderer_SetParams(&p);
    CrtRenderer_GetParams(&p);
    check(p.tube_gamma <= 2.8f, "tube gamma clamped");
    check(p.phosphor == CRT_PHOSPHOR_NONE, "invalid phosphor falls back to none");

    CrtRenderer_SetProfile(CRT_PROFILE_SGB_CONSUMER);
    CrtRenderer_GetParams(&p);
    check(p.phosphor == CRT_PHOSPHOR_SMPTE_C, "consumer set uses SMPTE-C");
    check(p.tube_gamma > 2.2f, "consumer set has a real tube's gamma");

    CrtRenderer_SetProfile(CRT_PROFILE_OFF);
    CrtRenderer_GetParams(&p);
    {
        crt_frame_desc_t f = sgb_desc(1);
        check(p.phosphor == CRT_PHOSPHOR_NONE &&
              CrtRenderer_TubeGammaExponent(&p, &f) == 1.0f,
              "OFF stays exactly neutral in colour too");
    }
}

static void test_sgb_aspect(void) {
    crt_frame_desc_t f;
    crt_rect_i_t vp;
    double got, want = 64.0 / 49.0;

    printf("SGB aspect\n");

    memset(&f, 0, sizeof f);
    f.texture_width = 256; f.texture_height = 224;
    f.raster_width  = 256; f.raster_height  = 224;
    f.texture_active_rect = (crt_rect_i_t){ 0, 0, 256, 224 };
    f.raster_active_rect  = (crt_rect_i_t){ 0, 0, 256, 224 };
    f.pixel_aspect_num = 8;
    f.pixel_aspect_den = 7;
    f.input_transfer = CRT_TRANSFER_SRGB;
    f.source_refresh_hz = 59.7275f;

    CrtRenderer_ComputeViewport(&f, CRT_ASPECT_SOURCE_PAR, 1920, 1080, &vp);
    got = (double)vp.w / (double)vp.h;
    check(fabs(got - want) < 0.01,
          "source-PAR gives 64:49, the real SNES display aspect");

    check(got > 256.0 / 224.0,
          "the picture is stretched horizontally, as real hardware does");

    f.texture_width = 512; f.texture_height = 448;
    f.texture_active_rect = (crt_rect_i_t){ 0, 0, 512, 448 };
    {
        crt_rect_i_t vp2;
        CrtRenderer_ComputeViewport(&f, CRT_ASPECT_SOURCE_PAR, 1920, 1080, &vp2);
        check(vp2.w == vp.w && vp2.h == vp.h,
              "a 2x decoded upload gives an identical viewport");
    }

    CrtRenderer_ComputeViewport(&f, CRT_ASPECT_4_3, 1920, 1080, &vp);
    check(fabs((double)vp.w / (double)vp.h - 4.0 / 3.0) < 0.01,
          "forced 4:3 policy still available");
}

static void test_borderless_frame(void) {
    crt_frame_desc_t f;
    crt_rect_i_t vp;
    const char *why = NULL;
    double got;

    printf("borderless frame\n");

    memset(&f, 0, sizeof f);
    f.texture_width = 160; f.texture_height = 144;
    f.raster_width  = 160; f.raster_height  = 144;
    f.texture_active_rect = (crt_rect_i_t){ 0, 0, 160, 144 };
    f.raster_active_rect  = (crt_rect_i_t){ 0, 0, 160, 144 };
    f.pixel_aspect_num = 1;
    f.pixel_aspect_den = 1;
    f.scan_mode = CRT_SCAN_PROGRESSIVE;
    f.field = CRT_FIELD_NONE;
    f.input_transfer = CRT_TRANSFER_SRGB;
    f.input_gamma = 2.2f;
    f.source_refresh_hz = 59.7275f;

    check(CrtRenderer_ValidateFrame(&f, &why),
          "a borderless 160x144 frame is a valid descriptor");
    if (why) printf("      rejected: %s\n", why);

    CrtRenderer_ComputeViewport(&f, CRT_ASPECT_SOURCE_PAR, 1920, 1080, &vp);
    got = (double)vp.w / (double)vp.h;
    check(fabs(got - 160.0 / 144.0) < 0.01,
          "borderless source-PAR is the Game Boy's own 10:9");
    check(vp.w > 0 && vp.h > 0 && vp.w <= 1920 && vp.h <= 1080,
          "borderless viewport fits the drawable");

    {
        static const crt_profile_id_t ids[3] = {
            CRT_PROFILE_CLEAN, CRT_PROFILE_SGB_CONSUMER, CRT_PROFILE_SGB_RGB_PVM
        };
        int i;
        for (i = 0; i < 3; i++) {
            char msg[128];
            crt_params_t p;
            CrtRenderer_SetProfile(ids[i]);
            CrtRenderer_GetParams(&p);
            snprintf(msg, sizeof msg, "%s is selectable without a border",
                     CrtRenderer_ProfileName(ids[i]));
            check(p.profile == ids[i] && CrtRenderer_ValidateFrame(&f, NULL), msg);
        }
        CrtRenderer_SetProfile(CRT_PROFILE_OFF);
    }
}

static void test_transfer_stages(void) {
    crt_params_t p;
    crt_frame_desc_t f;

    printf("transfer stages\n");

    f = sgb_desc(1);

    f.input_transfer = CRT_TRANSFER_SRGB;
    check(CrtRenderer_SourceGamma(&f) == 2.2f, "sRGB source reports 2.2");

    f.input_transfer = CRT_TRANSFER_GAMMA;
    f.input_gamma = 2.0f;
    check(CrtRenderer_SourceGamma(&f) == 2.0f, "gamma source reports its own gamma");
    f.input_gamma = 2.4f;
    check(CrtRenderer_SourceGamma(&f) == 2.4f, "a different gamma reports differently");

    f.input_transfer = CRT_TRANSFER_LINEAR;
    check(CrtRenderer_SourceGamma(&f) == 0.0f,
          "a linear source has no encoding gamma at all");

    memset(&p, 0, sizeof p);
    p.tube_gamma = 2.4f;

    f.input_transfer = CRT_TRANSFER_GAMMA;
    f.input_gamma = 2.0f;
    check(fabsf(CrtRenderer_TubeGammaExponent(&p, &f) - 2.4f / 2.0f) < 1e-6f,
          "a 2.4 tube against a 2.0 source is 1.20");

    f.input_gamma = 2.4f;
    check(CrtRenderer_TubeGammaExponent(&p, &f) == 1.0f,
          "a 2.4 tube against a 2.4 source is EXACTLY neutral");

    f.input_transfer = CRT_TRANSFER_SRGB;
    check(fabsf(CrtRenderer_TubeGammaExponent(&p, &f) - 2.4f / 2.2f) < 1e-6f,
          "a 2.4 tube against sRGB is 2.4/2.2");

    f.input_transfer = CRT_TRANSFER_LINEAR;
    check(CrtRenderer_TubeGammaExponent(&p, &f) == 1.0f,
          "a linear source leaves the tube stage exactly neutral");

    p.tube_gamma = 0.0f;
    f.input_transfer = CRT_TRANSFER_GAMMA;
    f.input_gamma = 2.0f;
    check(CrtRenderer_TubeGammaExponent(&p, &f) == 1.0f,
          "no tube gamma is neutral whatever the source");

    CrtRenderer_SetProfile(CRT_PROFILE_SGB_CONSUMER);
    CrtRenderer_GetParams(&p);
    check(p.output_transfer == CRT_TRANSFER_SRGB,
          "profiles encode output as sRGB by default");

    memset(&p, 0, sizeof p);
    p.profile = CRT_PROFILE_CLEAN;
    p.output_transfer = (crt_transfer_t)77;
    CrtRenderer_SetParams(&p);
    CrtRenderer_GetParams(&p);
    check(p.output_transfer == CRT_TRANSFER_SRGB, "invalid output transfer rejected");

    CrtRenderer_SetProfile(CRT_PROFILE_OFF);
}

static void test_active_rects(void) {
    crt_frame_desc_t f;
    crt_rect_i_t vp, full;
    const char *why = NULL;

    printf("active rectangles\n");

    f = sgb_desc(1);
    f.texture_width = 512; f.texture_height = 512;
    f.texture_active_rect = (crt_rect_i_t){ 16, 32, 256, 224 };
    check(CrtRenderer_ValidateFrame(&f, &why), "padded storage with an offset rect");
    if (why) printf("      rejected: %s\n", why);

    f = sgb_desc(1);
    f.raster_active_rect = (crt_rect_i_t){ 8, 16, 240, 192 };
    f.texture_active_rect = (crt_rect_i_t){ 8, 16, 240, 192 };
    check(CrtRenderer_ValidateFrame(&f, &why), "cropped raster at an offset origin");

    CrtRenderer_ComputeViewport(&f, CRT_ASPECT_RAW, 1920, 1080, &vp);
    {
        double got = (double)vp.w / (double)vp.h;
        check(fabs(got - 240.0 / 192.0) < 0.01,
              "a cropped viewport follows the active picture's aspect");
    }

    f = sgb_desc(1);
    CrtRenderer_ComputeViewport(&f, CRT_ASPECT_SOURCE_PAR, 1920, 1080, &full);
    {
        crt_frame_desc_t g = sgb_desc(1);
        crt_rect_i_t vg;
        g.raster_active_rect = (crt_rect_i_t){ 0, 0, 256, 224 };
        CrtRenderer_ComputeViewport(&g, CRT_ASPECT_SOURCE_PAR, 1920, 1080, &vg);
        check(vg.w == full.w && vg.h == full.h,
              "a full-raster active rect is identical to the old behaviour");
    }

    f = sgb_desc(1);
    f.raster_active_rect = (crt_rect_i_t){ 200, 0, 100, 224 };
    check(!CrtRenderer_ValidateFrame(&f, &why),
          "an active rect overflowing the raster is still rejected");

    f = sgb_desc(1);
    f.texture_active_rect = (crt_rect_i_t){ -1, 0, 256, 224 };
    check(!CrtRenderer_ValidateFrame(&f, &why),
          "a negative active origin is still rejected");
}

static void test_white_point(void) {
    const float *d65, *k93;
    crt_params_t p;
    float sum_d65 = 0.0f, sum_k93 = 0.0f;
    int i;

    printf("white point\n");

    d65 = CrtRenderer_PhosphorMatrix(CRT_PHOSPHOR_SMPTE_C, 6504.0f);

    {
        float a[9], b[9];
        for (i = 0; i < 9; i++) a[i] = d65[i];
        k93 = CrtRenderer_PhosphorMatrix(CRT_PHOSPHOR_SMPTE_C, 9300.0f);
        for (i = 0; i < 9; i++) b[i] = k93[i];

        for (i = 0; i < 3; i++) {
            float r = a[i*3] + a[i*3+1] + a[i*3+2];
            check(fabsf(r - 1.0f) < 1e-4f, "D65 rows sum to 1");
        }

        for (i = 0; i < 3; i++) {
            sum_d65 += a[i*3] + a[i*3+1] + a[i*3+2];
            sum_k93 += b[i*3] + b[i*3+1] + b[i*3+2];
        }
        check(fabsf(sum_k93 - 3.0f) > 0.01f,
              "9300K does NOT preserve the row sums, by design");

        {
            float red_row  = b[0] + b[1] + b[2];
            float blue_row = b[6] + b[7] + b[8];
            float red_d65  = a[0] + a[1] + a[2];
            float blue_d65 = a[6] + a[7] + a[8];
            check(blue_row / red_row > blue_d65 / red_d65,
                  "9300K is BLUER than D65, not warmer");
        }
    }

    CrtRenderer_SetProfile(CRT_PROFILE_SGB_CONSUMER);
    CrtRenderer_GetParams(&p);
    check(p.white_point_kelvin > 8000.0f,
          "the consumer set runs at a consumer factory white");

    CrtRenderer_SetProfile(CRT_PROFILE_SGB_RGB_PVM);
    CrtRenderer_GetParams(&p);
    check(fabsf(p.white_point_kelvin - 6504.0f) < 1.0f,
          "the broadcast monitor is lined up on D65");

    memset(&p, 0, sizeof p);
    p.profile = CRT_PROFILE_CLEAN;
    p.white_point_kelvin = 30000.0f;
    CrtRenderer_SetParams(&p);
    CrtRenderer_GetParams(&p);
    check(p.white_point_kelvin <= 12000.0f, "white point clamped");

    CrtRenderer_SetProfile(CRT_PROFILE_OFF);
}

static void test_effective_tvl(void) {
    crt_params_t p;
    float tvl;
    int i;
    static const crt_profile_id_t ids[3] = {
        CRT_PROFILE_CLEAN, CRT_PROFILE_SGB_CONSUMER, CRT_PROFILE_SGB_RGB_PVM
    };

    printf("effective horizontal TVL\n");

    for (i = 0; i < 3; i++) {
        CrtRenderer_SetProfile(ids[i]);
        CrtRenderer_GetParams(&p);
        tvl = CrtRenderer_EffectiveTvl(&p, 256, 224, 8, 7);
        printf("    %-8s spot %.2f -> %3.0f TVL   %s\n",
               CrtRenderer_ProfileName(ids[i]), p.spot_sigma_bright, tvl,
               tvl >= 196.0f ? "(source-limited)" : "(spot-limited)");
        check(tvl > 0.0f, "every profile reports a resolving power");
    }

    CrtRenderer_SetProfile(CRT_PROFILE_SGB_CONSUMER);
    CrtRenderer_GetParams(&p);
    check(CrtRenderer_EffectiveTvl(&p, 256, 224, 8, 7) < 196.0f,
          "the consumer set is limited by its own bandwidth, not the source");

    CrtRenderer_SetProfile(CRT_PROFILE_SGB_RGB_PVM);
    CrtRenderer_GetParams(&p);
    check(CrtRenderer_EffectiveTvl(&p, 256, 224, 8, 7) > 196.0f,
          "the broadcast monitor out-resolves a 256-wide raster");

    memset(&p, 0, sizeof p);
    p.profile = CRT_PROFILE_CLEAN;
    check(CrtRenderer_EffectiveTvl(&p, 256, 224, 8, 7) == 0.0f,
          "no spot reports source-limited");

    CrtRenderer_SetProfile(CRT_PROFILE_OFF);
}

int main(void) {
    printf("crt_renderer tests\n\n");
    test_validation();
    test_viewport();
    test_persistence();
    test_params();
    test_profile_ranges();
    test_curve_override();
    test_mask_density();
    test_phosphor();
    test_sgb_aspect();
    test_borderless_frame();
    test_transfer_stages();
    test_white_point();
    test_effective_tvl();
    test_active_rects();
    printf("\n%s\n", g_fail ? "FAILED" : "all passed");
    return g_fail ? 1 : 0;
}
