/* SPDX-License-Identifier: MIT
 *
 * crt_renderer.h, OldAmber's own CRT display renderer.
 *
 * PROVENANCE. Original OldAmber code, written from
 * docs/crt-renderer-implementation-spec.md, public display physics, and the
 * permissive references section 18 of that spec approves. No GPL shader was
 * copied, translated, mechanically ported, or closely restructured to produce
 * it, and no GPL parameter table or preset constant is reproduced. See
 * THIRD_PARTY.md for the design-provenance entry.
 *
 * WHAT THIS IS. The display stage: beam, phosphor mask, glass and geometry. It
 * consumes an ALREADY COMPOSED signal image and knows nothing about how that
 * image was produced. Composite video is an upstream signal property handled by
 * ntsc_filter.c; this module must never branch on whether composite is enabled
 * (spec 4, 5.4). A profile may only select defaults for explicitly declared CRT
 * parameters.
 *
 * The invariant that matters most (spec 5.1): texture size is not raster size.
 * The 2x NTSC decode uploads 512x448 storage carrying a 256x224 raster of 224
 * lines. Nothing here may derive scanline count, line phase, pixel aspect or
 * beam spacing from texture height. That is why the caller passes an explicit
 * descriptor instead of just a texture.
 *
 * No OpenGL type appears in this interface, by design (spec 6).
 */

#pragma once

#include <stdint.h>

typedef enum {
    CRT_SCAN_PROGRESSIVE = 0,
    CRT_SCAN_INTERLACED,
} crt_scan_mode_t;

typedef enum {
    CRT_FIELD_NONE = 0,
    CRT_FIELD_EVEN,
    CRT_FIELD_ODD,
} crt_field_t;

typedef enum {
    CRT_TRANSFER_SRGB = 0,
    CRT_TRANSFER_GAMMA,
    CRT_TRANSFER_LINEAR,
} crt_transfer_t;

typedef struct {
    int x, y, w, h;
} crt_rect_i_t;

typedef struct {

    int texture_width;
    int texture_height;

    int raster_width;
    int raster_height;

    crt_rect_i_t texture_active_rect;

    crt_rect_i_t raster_active_rect;

    int pixel_aspect_num;
    int pixel_aspect_den;

    crt_scan_mode_t scan_mode;
    crt_field_t field;

    crt_transfer_t input_transfer;
    float input_gamma;
    float source_refresh_hz;
    uint64_t frame_number;
} crt_frame_desc_t;

typedef enum {
    CRT_PROFILE_OFF = 0,
    CRT_PROFILE_CLEAN,
    CRT_PROFILE_SGB_CONSUMER,
    CRT_PROFILE_SGB_RGB_PVM,
} crt_profile_id_t;

typedef enum {
    CRT_QUALITY_AUTO = 0,
    CRT_QUALITY_FAST,
    CRT_QUALITY_FULL,
} crt_quality_t;

typedef enum {
    CRT_MASK_NONE = 0,
    CRT_MASK_APERTURE_GRILLE,
    CRT_MASK_SLOT,
    CRT_MASK_SHADOW,
} crt_mask_layout_t;

typedef enum {
    CRT_MASK_RGB = 0,
    CRT_MASK_BGR,
} crt_mask_order_t;

typedef enum {
    CRT_MASK_COORD_OUTPUT_GRID = 0,
    CRT_MASK_COORD_TUBE_SURFACE,
} crt_mask_coord_t;

typedef enum {
    CRT_CURVE_PROFILE = 0,
    CRT_CURVE_OFF,
    CRT_CURVE_SUBTLE,
    CRT_CURVE_TV,
} crt_curve_t;

typedef enum {
    CRT_FACE_FLAT = 0,
    CRT_FACE_SPHERICAL,
    CRT_FACE_CYLINDRICAL,
} crt_face_geometry_t;

typedef enum {
    CRT_ASPECT_RAW = 0,
    CRT_ASPECT_SOURCE_PAR,
    CRT_ASPECT_4_3,
} crt_aspect_policy_t;

typedef enum {

    CRT_HEADROOM_CLIP = 0,

    CRT_HEADROOM_PRESERVE_PEAKS,
} crt_headroom_t;

typedef enum {
    CRT_PHOSPHOR_NONE = 0,
    CRT_PHOSPHOR_SMPTE_C,
    CRT_PHOSPHOR_EBU,
} crt_phosphor_t;

typedef enum {
    CRT_RECON_NEAREST = 0,
    CRT_RECON_LINEAR,
    CRT_RECON_GAUSSIAN,
    CRT_RECON_LANCZOS2,
} crt_reconstruction_t;

typedef struct {
    crt_profile_id_t profile;
    crt_quality_t quality;
    crt_aspect_policy_t aspect_policy;

    crt_transfer_t output_transfer;
    float output_gamma;

    float tube_gamma;

    crt_phosphor_t phosphor;

    float white_point_kelvin;
    float black_level;
    float paper_white;

    float brightness_compensation;

    crt_reconstruction_t reconstruction;
    float reconstruction_radius;
    float reconstruction_sharpness;
    float reconstruction_anti_ringing;
    float sample_phase_x;

    float spot_sigma_dark;
    float spot_sigma_bright;

    float spot_luma_exponent;

    float beam_sigma_min;
    float beam_sigma_max;
    float beam_luma_exponent;
    float beam_shape_exponent;
    float scanline_phase;
    float scanline_strength;

    crt_mask_layout_t mask_layout;
    crt_mask_order_t mask_order;

    float mask_triads_per_picture_height;

    float mask_pitch_pixels;
    float mask_strength;

    float mask_phase_x;
    float mask_phase_y;

    float mask_aperture;

    float mask_row_ratio;

    crt_mask_coord_t mask_coord_mode;

    crt_headroom_t headroom_policy;

    float bloom_threshold;
    float bloom_knee;
    float bloom_radius_pixels;
    float bloom_strength;
    float bloom_tail;

    float diffusion_radius_pixels;
    float diffusion_strength;
    float diffusion_tail;

    float halation_threshold;
    float halation_knee;
    float halation_radius_pixels;
    float halation_strength;
    float halation_tail;

    float halation_tint_r;
    float halation_tint_g;
    float halation_tint_b;

    float bloom_radius_lines;
    float diffusion_radius_lines;
    float halation_radius_lines;

    crt_face_geometry_t face_geometry;

    float warp_x;
    float warp_y;

    float raster_pincushion;
    float raster_keystone;
    float raster_rotation;
    float overscan_x;
    float overscan_y;
    float center_x;
    float center_y;
    float corner_radius;
    float corner_softness;
    float vignette_strength;

    float convergence_r_x, convergence_r_y;
    float convergence_g_x, convergence_g_y;
    float convergence_b_x, convergence_b_y;

    float convergence_radial;

    float focus_edge;

    float jitter_strength;
    float hum_strength;
    float hum_hz;
    float noise_strength;
    float flicker_strength;

} crt_params_t;

int  CrtRenderer_Init(void);
void CrtRenderer_Shutdown(void);

int  CrtRenderer_SetProfile(crt_profile_id_t profile);
crt_profile_id_t CrtRenderer_Profile(void);

void CrtRenderer_SetAvailable(int available);

int  CrtRenderer_IsAvailable(void);

const char *CrtRenderer_ProfileName(crt_profile_id_t id);

float CrtRenderer_MaskPitchForViewport(const crt_params_t *p, int viewport_h);

const float *CrtRenderer_PhosphorMatrix(crt_phosphor_t phosphor,
                                        float white_kelvin);

float CrtRenderer_GlowRadiusForViewport(float radius_lines, float fallback_pixels,
                                        int viewport_h, int raster_h);

float CrtRenderer_SourceGamma(const crt_frame_desc_t *f);

float CrtRenderer_TubeGammaExponent(const crt_params_t *p,
                                    const crt_frame_desc_t *f);

float CrtRenderer_BeamNormalization(float shape_exponent);

float CrtRenderer_MaskPeak(crt_mask_layout_t layout, float strength, float pitch);

float CrtRenderer_BeamPeak(float sigma, float shape_exponent);

float CrtRenderer_PhosphorPeak(crt_phosphor_t phosphor, float white_kelvin);

float CrtRenderer_EffectiveTvl(const crt_params_t *p,
                               int raster_w, int raster_h,
                               int pixel_aspect_num, int pixel_aspect_den);

float CrtRenderer_HeadroomScale(const crt_params_t *p, float pitch);
int  CrtRenderer_IsActive(void);

void CrtRenderer_SetQuality(crt_quality_t quality);
crt_quality_t CrtRenderer_Quality(void);

void CrtRenderer_SetCurve(crt_curve_t curve);
crt_curve_t CrtRenderer_Curve(void);

int  CrtRenderer_GetParams(crt_params_t *out);
int  CrtRenderer_SetParams(const crt_params_t *params);

int CrtRenderer_ValidateFrame(const crt_frame_desc_t *frame, const char **why);

void CrtRenderer_ComputeViewport(const crt_frame_desc_t *frame,
                                 crt_aspect_policy_t policy,
                                 int drawable_w, int drawable_h,
                                 crt_rect_i_t *out);

float CrtRenderer_HistoryWeight(float tau_seconds, float dt_seconds);
