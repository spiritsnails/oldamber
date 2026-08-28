/* SPDX-License-Identifier: MIT
 *
 * crt_renderer.c, see crt_renderer.h. Original OldAmber code, written from
 * docs/crt-renderer-implementation-spec.md.
 *
 * PHASE 0. This is the skeleton the spec's phase plan asks for: the frame
 * contract, descriptor validation, the derived viewport and persistence maths,
 * and the profile parameter blocks, all pure CPU code with no GL and no
 * presentation change. CrtRenderer_Draw and the pass graph arrive in Phase 1.
 *
 * Nothing here is reachable from the present path yet, on purpose: Phase 0's
 * exit condition is that presentation behaviour is unchanged.
 */

#include "crt_renderer.h"

#include <stdio.h>

#include <math.h>
#include <string.h>

typedef struct {
    float min, max;
} crt_range_t;

static const crt_range_t RANGE_BEAM_SIGMA        = { 0.10f, 0.65f };
static const crt_range_t RANGE_BEAM_LUMA_EXP     = { 0.25f, 4.00f };
static const crt_range_t RANGE_BEAM_SHAPE_EXP    = { 1.00f, 4.00f };

static const crt_range_t RANGE_MASK_PITCH        = { 2.00f, 12.00f };
static const crt_range_t RANGE_MASK_PITCH_DERIVED = { 2.00f, 24.00f };

static const crt_range_t RANGE_MASK_TRIADS       = { 0.00f, 1200.00f };

static const crt_range_t RANGE_TUBE_GAMMA        = { 2.00f, 2.80f };
static const crt_range_t RANGE_UNIT              = { 0.00f, 1.00f };

static const crt_range_t RANGE_SPOT_SIGMA        = { 0.00f, 1.00f };

static const crt_range_t RANGE_MASK_ROW_RATIO    = { 0.33f, 3.00f };

static const crt_range_t RANGE_GLOW_TAIL         = { 0.80f, 4.00f };

static const crt_range_t RANGE_WHITE_K           = { 5000.0f, 12000.0f };
static const crt_range_t RANGE_BLACK_LEVEL       = { 0.00f, 0.05f };

static const crt_range_t RANGE_PAPER_WHITE       = { 0.10f, 1.50f };
static const crt_range_t RANGE_BLOOM_RADIUS      = { 0.00f, 24.00f };
static const crt_range_t RANGE_HALATION_RADIUS   = { 0.00f, 64.00f };
static const crt_range_t RANGE_WARP              = { 0.00f, 0.15f };

static const crt_range_t RANGE_PINCUSHION        = { -0.10f, 0.10f };
static const crt_range_t RANGE_KEYSTONE          = { -0.10f, 0.10f };
static const crt_range_t RANGE_ROTATION          = { -0.05f, 0.05f };
static const crt_range_t RANGE_OVERSCAN          = { 0.00f, 0.15f };
static const crt_range_t RANGE_CONVERGENCE       = { -3.00f, 3.00f };

static const crt_range_t RANGE_CONV_RADIAL       = { 0.00f, 3.00f };

static const crt_range_t RANGE_FOCUS_EDGE        = { 0.00f, 1.00f };

static const crt_range_t RANGE_JITTER            = { 0.00f, 1.00f };
static const crt_range_t RANGE_HUM               = { 0.00f, 0.30f };

static const crt_range_t RANGE_HUM_HZ            = { 0.05f, 20.00f };
static const crt_range_t RANGE_NOISE             = { 0.00f, 0.30f };
static const crt_range_t RANGE_FLICKER           = { 0.00f, 0.30f };

static float clampf(float v, crt_range_t r) {

    if (!(v >= r.min)) return r.min;
    if (!(v <= r.max)) return r.max;
    return v;
}

static void clamp_params(crt_params_t *p) {
    p->beam_sigma_min       = clampf(p->beam_sigma_min, RANGE_BEAM_SIGMA);
    p->beam_sigma_max       = clampf(p->beam_sigma_max, RANGE_BEAM_SIGMA);
    p->beam_luma_exponent   = clampf(p->beam_luma_exponent, RANGE_BEAM_LUMA_EXP);
    p->beam_shape_exponent  = clampf(p->beam_shape_exponent, RANGE_BEAM_SHAPE_EXP);
    p->scanline_strength    = clampf(p->scanline_strength, RANGE_UNIT);

    p->mask_pitch_pixels    = clampf(p->mask_pitch_pixels, RANGE_MASK_PITCH);
    p->mask_triads_per_picture_height =
        clampf(p->mask_triads_per_picture_height, RANGE_MASK_TRIADS);

    if (p->mask_triads_per_picture_height > 0.0f &&
        p->mask_triads_per_picture_height < 40.0f)
        p->mask_triads_per_picture_height = 40.0f;
    p->mask_strength        = clampf(p->mask_strength, RANGE_UNIT);
    p->mask_aperture        = clampf(p->mask_aperture, RANGE_UNIT);
    p->mask_row_ratio       = clampf(p->mask_row_ratio, RANGE_MASK_ROW_RATIO);
    if (p->mask_coord_mode < CRT_MASK_COORD_OUTPUT_GRID ||
        p->mask_coord_mode > CRT_MASK_COORD_TUBE_SURFACE)
        p->mask_coord_mode = CRT_MASK_COORD_OUTPUT_GRID;
    p->reconstruction_sharpness    = clampf(p->reconstruction_sharpness, RANGE_UNIT);
    p->reconstruction_anti_ringing = clampf(p->reconstruction_anti_ringing, RANGE_UNIT);
    p->spot_sigma_dark      = clampf(p->spot_sigma_dark, RANGE_SPOT_SIGMA);
    p->spot_sigma_bright    = clampf(p->spot_sigma_bright, RANGE_SPOT_SIGMA);
    p->spot_luma_exponent   = clampf(p->spot_luma_exponent, RANGE_BEAM_LUMA_EXP);
    if (p->spot_sigma_bright < p->spot_sigma_dark)
        p->spot_sigma_bright = p->spot_sigma_dark;

    if (p->tube_gamma != 0.0f) p->tube_gamma = clampf(p->tube_gamma, RANGE_TUBE_GAMMA);
    if (p->phosphor < CRT_PHOSPHOR_NONE || p->phosphor > CRT_PHOSPHOR_EBU)
        p->phosphor = CRT_PHOSPHOR_NONE;

    if (p->white_point_kelvin != 0.0f)
        p->white_point_kelvin = clampf(p->white_point_kelvin, RANGE_WHITE_K);
    if (p->headroom_policy < CRT_HEADROOM_CLIP ||
        p->headroom_policy > CRT_HEADROOM_PRESERVE_PEAKS)
        p->headroom_policy = CRT_HEADROOM_CLIP;
    if (p->output_transfer < CRT_TRANSFER_SRGB ||
        p->output_transfer > CRT_TRANSFER_LINEAR)
        p->output_transfer = CRT_TRANSFER_SRGB;
    p->black_level  = clampf(p->black_level, RANGE_BLACK_LEVEL);
    p->paper_white  = clampf(p->paper_white, RANGE_PAPER_WHITE);

    p->bloom_strength       = clampf(p->bloom_strength, RANGE_UNIT);
    p->diffusion_strength   = clampf(p->diffusion_strength, RANGE_UNIT);
    p->diffusion_radius_pixels = clampf(p->diffusion_radius_pixels, RANGE_BLOOM_RADIUS);
    p->bloom_tail           = clampf(p->bloom_tail, RANGE_GLOW_TAIL);
    p->diffusion_tail       = clampf(p->diffusion_tail, RANGE_GLOW_TAIL);
    p->halation_tail        = clampf(p->halation_tail, RANGE_GLOW_TAIL);
    p->halation_threshold   = clampf(p->halation_threshold, RANGE_UNIT);
    p->halation_knee        = clampf(p->halation_knee, RANGE_UNIT);
    p->halation_tint_r      = clampf(p->halation_tint_r, RANGE_UNIT);
    p->halation_tint_g      = clampf(p->halation_tint_g, RANGE_UNIT);
    p->halation_tint_b      = clampf(p->halation_tint_b, RANGE_UNIT);
    p->bloom_radius_pixels  = clampf(p->bloom_radius_pixels, RANGE_BLOOM_RADIUS);
    p->halation_strength    = clampf(p->halation_strength, RANGE_UNIT);
    p->halation_radius_pixels = clampf(p->halation_radius_pixels, RANGE_HALATION_RADIUS);

    if (p->face_geometry < CRT_FACE_FLAT || p->face_geometry > CRT_FACE_CYLINDRICAL)
        p->face_geometry = CRT_FACE_FLAT;
    p->raster_pincushion    = clampf(p->raster_pincushion, RANGE_PINCUSHION);
    p->raster_keystone      = clampf(p->raster_keystone, RANGE_KEYSTONE);
    p->raster_rotation      = clampf(p->raster_rotation, RANGE_ROTATION);
    p->warp_x               = clampf(p->warp_x, RANGE_WARP);
    p->warp_y               = clampf(p->warp_y, RANGE_WARP);
    p->overscan_x           = clampf(p->overscan_x, RANGE_OVERSCAN);
    p->overscan_y           = clampf(p->overscan_y, RANGE_OVERSCAN);
    p->vignette_strength    = clampf(p->vignette_strength, RANGE_UNIT);

    p->convergence_r_x = clampf(p->convergence_r_x, RANGE_CONVERGENCE);
    p->convergence_r_y = clampf(p->convergence_r_y, RANGE_CONVERGENCE);
    p->convergence_g_x = clampf(p->convergence_g_x, RANGE_CONVERGENCE);
    p->convergence_g_y = clampf(p->convergence_g_y, RANGE_CONVERGENCE);
    p->convergence_b_x = clampf(p->convergence_b_x, RANGE_CONVERGENCE);
    p->convergence_b_y = clampf(p->convergence_b_y, RANGE_CONVERGENCE);
    p->convergence_radial = clampf(p->convergence_radial, RANGE_CONV_RADIAL);
    p->focus_edge         = clampf(p->focus_edge, RANGE_FOCUS_EDGE);
    p->jitter_strength    = clampf(p->jitter_strength, RANGE_JITTER);
    p->hum_strength       = clampf(p->hum_strength, RANGE_HUM);
    p->noise_strength     = clampf(p->noise_strength, RANGE_NOISE);
    p->flicker_strength   = clampf(p->flicker_strength, RANGE_FLICKER);

    if (p->hum_hz != 0.0f) p->hum_hz = clampf(p->hum_hz, RANGE_HUM_HZ);

    if (p->beam_sigma_max < p->beam_sigma_min)
        p->beam_sigma_max = p->beam_sigma_min;
}

static void profile_neutral(crt_params_t *p) {

    memset(p, 0, sizeof *p);
    p->quality        = CRT_QUALITY_AUTO;

    p->aspect_policy  = CRT_ASPECT_SOURCE_PAR;

    p->output_transfer = CRT_TRANSFER_SRGB;
    p->output_gamma   = 2.2f;
    p->black_level    = 0.0f;
    p->paper_white    = 1.0f;
    p->brightness_compensation = 1.0f;

    p->reconstruction        = CRT_RECON_GAUSSIAN;
    p->reconstruction_radius = 1.0f;
    p->reconstruction_sharpness = 0.5f;
    p->sample_phase_x        = 0.5f;

    p->spot_sigma_dark       = 0.0f;
    p->spot_sigma_bright     = 0.0f;
    p->spot_luma_exponent    = 1.0f;
    p->reconstruction_sharpness  = 0.5f;
    p->reconstruction_anti_ringing = 0.0f;

    p->beam_sigma_min      = 0.25f;
    p->beam_sigma_max      = 0.40f;
    p->beam_luma_exponent  = 1.0f;
    p->beam_shape_exponent = 2.0f;
    p->scanline_phase      = 0.5f;

    p->scanline_strength   = 1.0f;

    p->bloom_tail        = 2.0f;
    p->diffusion_tail    = 2.0f;
    p->halation_tail     = 2.0f;
    p->halation_tint_r   = 1.0f;
    p->halation_tint_g   = 1.0f;
    p->halation_tint_b   = 1.0f;

    p->face_geometry     = CRT_FACE_FLAT;

    p->mask_layout       = CRT_MASK_NONE;
    p->mask_order        = CRT_MASK_RGB;
    p->mask_pitch_pixels = 3.0f;
    p->mask_strength     = 0.0f;
    p->mask_aperture     = 0.0f;
    p->mask_row_ratio    = 0.75f;
    p->mask_coord_mode   = CRT_MASK_COORD_OUTPUT_GRID;
    p->headroom_policy   = CRT_HEADROOM_CLIP;
}

static void profile_clean(crt_params_t *p) {
    profile_neutral(p);
    p->profile = CRT_PROFILE_CLEAN;

    p->reconstruction     = CRT_RECON_LANCZOS2;
    p->reconstruction_sharpness = 0.55f;

    p->reconstruction_anti_ringing = 0.7f;
    p->spot_sigma_dark    = 0.18f;
    p->spot_sigma_bright  = 0.28f;
    p->spot_luma_exponent = 1.0f;
    p->beam_sigma_min     = 0.22f;
    p->beam_sigma_max     = 0.34f;
    p->beam_luma_exponent = 1.2f;
    p->mask_layout        = CRT_MASK_APERTURE_GRILLE;

    p->mask_triads_per_picture_height = 240.0f;
    p->mask_pitch_pixels  = 3.0f;
    p->mask_strength      = 0.20f;

    p->headroom_policy    = CRT_HEADROOM_PRESERVE_PEAKS;
}

static void profile_sgb_consumer(crt_params_t *p) {
    profile_neutral(p);
    p->profile = CRT_PROFILE_SGB_CONSUMER;

    p->reconstruction     = CRT_RECON_GAUSSIAN;
    p->reconstruction_radius = 1.25f;
    p->reconstruction_sharpness = 0.45f;
    p->reconstruction_anti_ringing = 0.0f;

    p->spot_sigma_dark    = 0.34f;
    p->spot_sigma_bright  = 0.52f;
    p->spot_luma_exponent = 1.0f;
    p->beam_sigma_min     = 0.30f;
    p->beam_sigma_max     = 0.48f;
    p->beam_luma_exponent = 1.0f;
    p->mask_layout        = CRT_MASK_SLOT;

    p->mask_aperture      = 0.55f;
    p->mask_row_ratio     = 1.20f;

    p->mask_coord_mode    = CRT_MASK_COORD_TUBE_SURFACE;

    p->phosphor           = CRT_PHOSPHOR_SMPTE_C;

    p->white_point_kelvin = 9300.0f;

    p->focus_edge         = 0.18f;
    p->tube_gamma         = 2.40f;

    p->mask_triads_per_picture_height = 160.0f;
    p->mask_pitch_pixels  = 6.0f;
    p->mask_strength      = 0.45f;

    p->headroom_policy    = CRT_HEADROOM_PRESERVE_PEAKS;

    p->bloom_threshold    = 0.30f;
    p->bloom_knee         = 0.15f;
    p->bloom_strength     = 0.24f;
    p->halation_strength  = 0.13f;

    p->bloom_radius_lines     = 2.0f;
    p->halation_radius_lines  = 7.0f;

    p->diffusion_strength     = 0.10f;
    p->diffusion_radius_lines = 3.0f;

    p->halation_threshold     = 0.55f;
    p->halation_knee          = 0.20f;
    p->halation_tail          = 1.4f;
    p->halation_tint_r        = 1.00f;
    p->halation_tint_g        = 0.72f;
    p->halation_tint_b        = 0.55f;
    p->bloom_radius_pixels    = 6.0f;
    p->halation_radius_pixels = 18.0f;
    p->overscan_x         = 0.02f;
    p->overscan_y         = 0.02f;

    p->face_geometry      = CRT_FACE_SPHERICAL;
    p->warp_x             = 0.08f;
    p->warp_y             = 0.08f;
    p->vignette_strength  = 0.10f;
    p->corner_radius      = 0.06f;
    p->corner_softness    = 0.20f;
}

static void profile_sgb_pvm(crt_params_t *p) {
    profile_neutral(p);
    p->profile = CRT_PROFILE_SGB_RGB_PVM;

    p->reconstruction     = CRT_RECON_LANCZOS2;
    p->reconstruction_radius = 1.0f;
    p->reconstruction_sharpness = 0.60f;
    p->reconstruction_anti_ringing = 0.6f;
    p->spot_sigma_dark    = 0.12f;
    p->spot_sigma_bright  = 0.20f;
    p->spot_luma_exponent = 1.0f;
    p->beam_sigma_min     = 0.18f;
    p->beam_sigma_max     = 0.30f;
    p->beam_luma_exponent = 1.4f;
    p->mask_layout        = CRT_MASK_APERTURE_GRILLE;

    p->phosphor           = CRT_PHOSPHOR_SMPTE_C;

    p->white_point_kelvin = 6504.0f;
    p->tube_gamma         = 2.40f;

    p->mask_triads_per_picture_height = 320.0f;
    p->mask_pitch_pixels  = 4.0f;
    p->mask_strength      = 0.35f;
    p->headroom_policy    = CRT_HEADROOM_PRESERVE_PEAKS;

    p->bloom_threshold    = 0.45f;
    p->bloom_knee         = 0.10f;

    p->bloom_strength     = 0.28f;
    p->bloom_radius_lines = 1.5f;
    p->bloom_radius_pixels = 3.0f;
}

float CrtRenderer_MaskPitchForViewport(const crt_params_t *p, int viewport_h) {
    float pitch;
    if (!p) return 3.0f;
    if (p->mask_triads_per_picture_height <= 0.0f || viewport_h <= 0)
        return p->mask_pitch_pixels;

    pitch = (float)viewport_h / p->mask_triads_per_picture_height;
    return clampf(pitch, RANGE_MASK_PITCH_DERIVED);
}

typedef struct { double x, y; } chroma_t;

static chroma_t white_from_kelvin(double T) {
    chroma_t w;
    if (!(T > 0.0)) T = 6504.0;
    if (T < 4000.0)  T = 4000.0;
    if (T > 25000.0) T = 25000.0;
    if (T <= 7000.0)
        w.x = -4.6070e9 / (T*T*T) + 2.9678e6 / (T*T) + 0.09911e3 / T + 0.244063;
    else
        w.x = -2.0064e9 / (T*T*T) + 1.9018e6 / (T*T) + 0.24748e3 / T + 0.237040;
    w.y = -3.000 * w.x * w.x + 2.870 * w.x - 0.275;
    return w;
}

static void mat3_mul(const double *a, const double *b, double *out) {
    int i, j, k;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            double s = 0.0;
            for (k = 0; k < 3; k++) s += a[i*3+k] * b[k*3+j];
            out[i*3+j] = s;
        }
}

static int mat3_inverse(const double *m, double *out) {
    double det =
        m[0]*(m[4]*m[8] - m[5]*m[7]) -
        m[1]*(m[3]*m[8] - m[5]*m[6]) +
        m[2]*(m[3]*m[7] - m[4]*m[6]);
    if (det == 0.0 || det != det) return 0;
    out[0] =  (m[4]*m[8] - m[5]*m[7]) / det;
    out[1] = -(m[1]*m[8] - m[2]*m[7]) / det;
    out[2] =  (m[1]*m[5] - m[2]*m[4]) / det;
    out[3] = -(m[3]*m[8] - m[5]*m[6]) / det;
    out[4] =  (m[0]*m[8] - m[2]*m[6]) / det;
    out[5] = -(m[0]*m[5] - m[2]*m[3]) / det;
    out[6] =  (m[3]*m[7] - m[4]*m[6]) / det;
    out[7] = -(m[0]*m[7] - m[1]*m[6]) / det;
    out[8] =  (m[0]*m[4] - m[1]*m[3]) / det;
    return 1;
}

static int rgb_to_xyz_matrix(const chroma_t *prim, chroma_t white, double *out) {
    double M[9], Minv[9], W[3], S[3];
    int i;

    for (i = 0; i < 3; i++) {
        M[0*3+i] = prim[i].x / prim[i].y;
        M[1*3+i] = 1.0;
        M[2*3+i] = (1.0 - prim[i].x - prim[i].y) / prim[i].y;
    }
    W[0] = white.x / white.y;
    W[1] = 1.0;
    W[2] = (1.0 - white.x - white.y) / white.y;

    if (!mat3_inverse(M, Minv)) return 0;
    for (i = 0; i < 3; i++)
        S[i] = Minv[i*3+0]*W[0] + Minv[i*3+1]*W[1] + Minv[i*3+2]*W[2];
    for (i = 0; i < 3; i++) {
        out[0*3+i] = M[0*3+i] * S[i];
        out[1*3+i] = M[1*3+i] * S[i];
        out[2*3+i] = M[2*3+i] * S[i];
    }
    return 1;
}

static const chroma_t PRIM_SRGB[3]    = { {0.640,0.330}, {0.300,0.600}, {0.150,0.060} };
static const chroma_t PRIM_SMPTE_C[3] = { {0.630,0.340}, {0.310,0.595}, {0.155,0.070} };
static const chroma_t PRIM_EBU[3]     = { {0.640,0.330}, {0.290,0.600}, {0.150,0.060} };

static const float PHOSPHOR_IDENTITY[9] = {
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f,
};

const float *CrtRenderer_PhosphorMatrix(crt_phosphor_t phosphor,
                                        float white_kelvin) {

    static float cached[9];
    static int   cached_valid;
    static crt_phosphor_t cached_ph;
    static float cached_k;

    const chroma_t *prim;
    double src[9], dst[9], dstinv[9], m[9];
    chroma_t d65, white;
    int i;

    if (phosphor == CRT_PHOSPHOR_NONE) return PHOSPHOR_IDENTITY;

    if (cached_valid && cached_ph == phosphor && cached_k == white_kelvin)
        return cached;

    switch (phosphor) {
        case CRT_PHOSPHOR_EBU:     prim = PRIM_EBU;     break;
        case CRT_PHOSPHOR_SMPTE_C:
        default:                   prim = PRIM_SMPTE_C; break;
    }

    d65   = white_from_kelvin(6504.0);
    white = white_from_kelvin(white_kelvin > 0.0f ? (double)white_kelvin : 6504.0);

    if (!rgb_to_xyz_matrix(prim, white, src)) return PHOSPHOR_IDENTITY;

    if (!rgb_to_xyz_matrix(PRIM_SRGB, d65, dst)) return PHOSPHOR_IDENTITY;
    if (!mat3_inverse(dst, dstinv)) return PHOSPHOR_IDENTITY;

    mat3_mul(dstinv, src, m);
    for (i = 0; i < 9; i++) cached[i] = (float)m[i];

    cached_valid = 1;
    cached_ph = phosphor;
    cached_k = white_kelvin;
    return cached;
}

float CrtRenderer_GlowRadiusForViewport(float radius_lines, float fallback_pixels,
                                        int viewport_h, int raster_h) {
    float scale;
    if (radius_lines <= 0.0f || viewport_h <= 0 || raster_h <= 0)
        return fallback_pixels;
    scale = (float)viewport_h / (float)raster_h;

    return clampf(radius_lines * scale, (crt_range_t){ 0.0f, 96.0f });
}

static double gamma_fn(double z) {
    static const double c[9] = {
         0.99999999999980993,   676.5203681218851,  -1259.1392167224028,
       771.32342877765313,     -176.61502916214059,    12.507343278686905,
        -0.13857109526572012,     9.9843695780195716e-6,
         1.5056327351493116e-7
    };
    double x, t;
    int i;

    z -= 1.0;
    x = c[0];
    for (i = 1; i < 9; i++) x += c[i] / (z + (double)i);
    t = z + 7.0 + 0.5;
    return sqrt(2.0 * 3.14159265358979323846) *
           pow(t, z + 0.5) * exp(-t) * x;
}

float CrtRenderer_BeamNormalization(float shape_exponent) {

    float p = shape_exponent;
    if (!(p >= 1.0f)) p = 2.0f;
    if (p > 4.0f) p = 4.0f;
    return 2.0f * powf(2.0f, 1.0f / p) * (float)gamma_fn(1.0 + 1.0 / (double)p);
}

float CrtRenderer_MaskPeak(crt_mask_layout_t layout, float strength, float pitch) {
    float beta, dip_depth, dip_max, m3;

    if (layout == CRT_MASK_NONE || strength <= 0.0f) return 1.0f;

    beta = (pitch - 2.0f) / 2.0f;
    if (beta < 0.0f) beta = 0.0f;
    if (beta > 1.0f) beta = 1.0f;

    m3 = 1.0f + beta;

    dip_depth = 0.0f;
    if (layout == CRT_MASK_SLOT)   dip_depth = 0.35f * beta;
    if (layout == CRT_MASK_SHADOW) dip_depth = 0.70f * beta;
    dip_max = 1.0f / (1.0f - dip_depth * 0.5f);
    m3 *= dip_max;

    return 1.0f + strength * (m3 - 1.0f);
}

float CrtRenderer_BeamPeak(float sigma, float shape_exponent) {

    float c = CrtRenderer_BeamNormalization(shape_exponent);
    float p = shape_exponent;
    float sum = 0.0f;
    int k;

    if (!(sigma > 0.0f)) return 1.0f;
    if (!(p >= 1.0f)) p = 2.0f;

    for (k = -4; k <= 4; k++) {
        float d = (float)k / sigma;
        if (d < 0.0f) d = -d;
        sum += expf(-0.5f * powf(d, p));
    }
    return sum / (sigma * c);
}

float CrtRenderer_PhosphorPeak(crt_phosphor_t phosphor, float white_kelvin) {
    const float *m = CrtRenderer_PhosphorMatrix(phosphor, white_kelvin);
    float peak = 1.0f;
    int i;
    for (i = 0; i < 3; i++) {
        float row = m[i*3] + m[i*3+1] + m[i*3+2];
        if (row > peak) peak = row;
    }
    return peak;
}

float CrtRenderer_EffectiveTvl(const crt_params_t *p,
                               int raster_w, int raster_h,
                               int pixel_aspect_num, int pixel_aspect_den) {
    double display_aspect, h_span_px, sigma, f;

    if (!p || raster_w <= 0 || raster_h <= 0) return 0.0f;
    if (pixel_aspect_num <= 0 || pixel_aspect_den <= 0) return 0.0f;

    sigma = (double)(p->spot_sigma_bright > p->spot_sigma_dark
                     ? p->spot_sigma_bright : p->spot_sigma_dark);
    if (sigma <= 0.0) return 0.0f;

    display_aspect = ((double)raster_w * pixel_aspect_num) /
                     ((double)raster_h * pixel_aspect_den);

    h_span_px = (double)raster_w / display_aspect;

    f = sqrt(0.6931471805599453 / (2.0 * 3.14159265358979 * 3.14159265358979)) / sigma;

    return (float)(2.0 * f * h_span_px);
}

float CrtRenderer_HeadroomScale(const crt_params_t *p, float pitch) {
    float peak;
    if (!p || p->headroom_policy != CRT_HEADROOM_PRESERVE_PEAKS) return 1.0f;

    peak = CrtRenderer_MaskPeak(p->mask_layout, p->mask_strength, pitch) *
           CrtRenderer_BeamPeak(p->beam_sigma_max, p->beam_shape_exponent) *
           CrtRenderer_PhosphorPeak(p->phosphor, p->white_point_kelvin);

    if (peak <= 1.0f) return 1.0f;
    return 1.0f / peak;
}

float CrtRenderer_SourceGamma(const crt_frame_desc_t *f) {
    if (!f) return 2.2f;
    switch (f->input_transfer) {
        case CRT_TRANSFER_LINEAR:

            return 0.0f;
        case CRT_TRANSFER_GAMMA:
            return f->input_gamma > 0.0f ? f->input_gamma : 2.2f;
        case CRT_TRANSFER_SRGB:
        default:

            return 2.2f;
    }
}

float CrtRenderer_TubeGammaExponent(const crt_params_t *p,
                                    const crt_frame_desc_t *f) {
    float src;
    if (!p || p->tube_gamma <= 0.0f) return 1.0f;

    src = CrtRenderer_SourceGamma(f);

    if (src <= 0.0f) return 1.0f;

    if (p->tube_gamma == src) return 1.0f;
    return p->tube_gamma / src;
}

static crt_curve_t s_curve = CRT_CURVE_PROFILE;

static void apply_curve_override(crt_params_t *p) {
    switch (s_curve) {
        case CRT_CURVE_PROFILE:
            return;
        case CRT_CURVE_OFF:

            p->face_geometry = CRT_FACE_FLAT;
            p->warp_x = p->warp_y = 0.0f;
            p->corner_radius = 0.0f;
            return;
        case CRT_CURVE_SUBTLE:

            if (p->face_geometry == CRT_FACE_FLAT)
                p->face_geometry = CRT_FACE_SPHERICAL;
            p->warp_x = p->warp_y = 0.04f;
            p->corner_radius = 0.03f;
            break;
        case CRT_CURVE_TV:
            if (p->face_geometry == CRT_FACE_FLAT)
                p->face_geometry = CRT_FACE_SPHERICAL;
            p->warp_x = p->warp_y = 0.08f;
            p->corner_radius = 0.06f;
            break;
    }

    if (p->corner_softness <= 0.0f) p->corner_softness = 0.20f;
}

const char *CrtRenderer_ProfileName(crt_profile_id_t id) {
    switch (id) {
        case CRT_PROFILE_OFF:          return "OFF";
        case CRT_PROFILE_CLEAN:        return "CLEAN";
        case CRT_PROFILE_SGB_CONSUMER: return "SGB TV";
        case CRT_PROFILE_SGB_RGB_PVM:  return "SGB PVM";
    }
    return "?";
}

static void load_profile(crt_params_t *p, crt_profile_id_t id) {
    switch (id) {
        case CRT_PROFILE_CLEAN:        profile_clean(p);        break;
        case CRT_PROFILE_SGB_CONSUMER: profile_sgb_consumer(p); break;
        case CRT_PROFILE_SGB_RGB_PVM:  profile_sgb_pvm(p);      break;
        case CRT_PROFILE_OFF:
        default:
            profile_neutral(p);
            p->profile = CRT_PROFILE_OFF;
            break;
    }

    if (p->profile != CRT_PROFILE_OFF)
        apply_curve_override(p);
    clamp_params(p);

    printf("[crt] profile '%s': mask=%d pitch=%.1f strength=%.2f "
           "triads=%.0f beam=%.2f-%.2f bloom=%.2f/%.1fpx halation=%.2f/%.1fpx "
           "warp=%.3f overscan=%.3f corner=%.3f vignette=%.2f\n",
           CrtRenderer_ProfileName(p->profile),
           (int)p->mask_layout, p->mask_pitch_pixels, p->mask_strength,
           p->mask_triads_per_picture_height,
           p->beam_sigma_min, p->beam_sigma_max,
           p->bloom_strength, p->bloom_radius_pixels,
           p->halation_strength, p->halation_radius_pixels,
           p->warp_x, p->overscan_x, p->corner_radius, p->vignette_strength);
    fflush(stdout);
}

static crt_params_t s_params;
static int          s_initialized;

static int          s_available;

int CrtRenderer_Init(void) {
    if (!s_initialized) {
        load_profile(&s_params, CRT_PROFILE_OFF);
        s_initialized = 1;
    }

    return 0;
}

void CrtRenderer_Shutdown(void) {
    s_available = 0;
    s_initialized = 0;
}

int CrtRenderer_SetProfile(crt_profile_id_t profile) {
    if (!s_initialized) CrtRenderer_Init();
    load_profile(&s_params, profile);
    return s_params.profile == profile;
}

crt_profile_id_t CrtRenderer_Profile(void) {
    return s_initialized ? s_params.profile : CRT_PROFILE_OFF;
}

void CrtRenderer_SetCurve(crt_curve_t curve) {
    if (!s_initialized) CrtRenderer_Init();
    if (curve < CRT_CURVE_PROFILE || curve > CRT_CURVE_TV) return;
    s_curve = curve;

    load_profile(&s_params, s_params.profile);
}

crt_curve_t CrtRenderer_Curve(void) { return s_curve; }

void CrtRenderer_SetAvailable(int available) { s_available = available ? 1 : 0; }

int CrtRenderer_IsAvailable(void) { return s_available; }

int CrtRenderer_IsActive(void) {
    return s_available && s_initialized && s_params.profile != CRT_PROFILE_OFF;
}

void CrtRenderer_SetQuality(crt_quality_t quality) {
    if (!s_initialized) CrtRenderer_Init();
    s_params.quality = quality;
}

crt_quality_t CrtRenderer_Quality(void) {
    return s_initialized ? s_params.quality : CRT_QUALITY_AUTO;
}

int CrtRenderer_GetParams(crt_params_t *out) {
    if (!out) return 0;
    if (!s_initialized) CrtRenderer_Init();
    *out = s_params;
    return 1;
}

int CrtRenderer_SetParams(const crt_params_t *params) {
    if (!params) return 0;
    if (!s_initialized) CrtRenderer_Init();
    s_params = *params;
    clamp_params(&s_params);
    return 1;
}

static int rect_within(crt_rect_i_t r, int w, int h) {
    if (r.w <= 0 || r.h <= 0) return 0;
    if (r.x < 0 || r.y < 0)   return 0;
    return r.x + r.w <= w && r.y + r.h <= h;
}

int CrtRenderer_ValidateFrame(const crt_frame_desc_t *f, const char **why) {
    const char *err = NULL;

    if (!f) {
        err = "null descriptor";
    } else if (f->texture_width <= 0 || f->texture_height <= 0) {
        err = "non-positive texture dimensions";
    } else if (f->raster_width <= 0 || f->raster_height <= 0) {
        err = "non-positive raster dimensions";
    } else if (f->pixel_aspect_num <= 0 || f->pixel_aspect_den <= 0) {
        err = "non-positive pixel aspect";
    } else if (!rect_within(f->texture_active_rect, f->texture_width,
                            f->texture_height)) {
        err = "texture_active_rect outside texture";
    } else if (!rect_within(f->raster_active_rect, f->raster_width,
                            f->raster_height)) {
        err = "raster_active_rect outside raster";
    } else if (f->scan_mode == CRT_SCAN_INTERLACED) {

        err = "interlaced scan is not supported yet";
    } else if (f->scan_mode == CRT_SCAN_PROGRESSIVE &&
               f->field != CRT_FIELD_NONE) {
        err = "progressive with a field";
    }

    if (why) *why = err;
    return err == NULL;
}

void CrtRenderer_ComputeViewport(const crt_frame_desc_t *f,
                                 crt_aspect_policy_t policy,
                                 int drawable_w, int drawable_h,
                                 crt_rect_i_t *out) {
    double want;
    int w, h;
    int aw, ah;

    if (!out) return;
    out->x = out->y = out->w = out->h = 0;
    if (!f || drawable_w <= 0 || drawable_h <= 0) return;

    aw = f->raster_active_rect.w > 0 ? f->raster_active_rect.w : f->raster_width;
    ah = f->raster_active_rect.h > 0 ? f->raster_active_rect.h : f->raster_height;

    switch (policy) {
        case CRT_ASPECT_4_3:
            want = 4.0 / 3.0;
            break;
        case CRT_ASPECT_SOURCE_PAR:
            want = ((double)aw * (double)f->pixel_aspect_num) /
                   ((double)ah * (double)f->pixel_aspect_den);
            break;
        case CRT_ASPECT_RAW:
        default:
            want = (double)aw / (double)ah;
            break;
    }
    if (!(want > 0.0)) want = (double)aw / (double)ah;

    w = drawable_w;
    h = (int)((double)drawable_w / want + 0.5);
    if (h > drawable_h) {
        h = drawable_h;
        w = (int)((double)drawable_h * want + 0.5);
    }
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    out->w = w;
    out->h = h;
    out->x = (drawable_w - w) / 2;
    out->y = (drawable_h - h) / 2;
}

float CrtRenderer_HistoryWeight(float tau_seconds, float dt_seconds) {
    if (!(tau_seconds > 0.0f)) return 0.0f;
    if (!(dt_seconds > 0.0f))  return 1.0f;
    return (float)exp(-(double)dt_seconds / (double)tau_seconds);
}
