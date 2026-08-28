// SPDX-License-Identifier: MIT
// OldAmber CRT renderer -- tube pass, fragment stage.
// Original OldAmber code, written from docs/crt-renderer-implementation-spec.md
// and public display physics. No third-party shader was copied, translated or
// restructured to produce it. See THIRD_PARTY.md.
//
// WHAT THIS PASS DOES (spec 11.2), in order:
//   output pixel -> overscan/warp -> raster coordinates
//                -> horizontal reconstruction around the sample position
//                -> luminance-dependent vertical beam over nearby raster lines
//                -> output-space phosphor mask with bounded compensation
//                -> output transfer encoding
//
// THE CENTRAL RULE (spec 5.1): the RASTER decides where scanlines are, never
// the texture. u_raster_size is 256x224 whether the upload is 256x224 or the
// NTSC decoder's 512x448. Everything vertical is computed in raster-line units
// and only converted to texture coordinates at the moment of sampling.
#version 150

uniform sampler2D u_source;

// Storage vs. raster, kept separate on purpose.
uniform vec2  u_texture_size;        // uploaded texels
uniform vec4  u_texture_active;      // xy = origin, zw = size, in texels
uniform vec2  u_raster_size;         // console sampling grid
uniform vec4  u_raster_active;       // xy = origin, zw = size, in raster units

// Where the tube is on the host display, in drawable pixels.
uniform vec2  u_viewport_origin;
uniform vec2  u_viewport_size;

// Input light.
uniform int   u_input_transfer;      // 0 sRGB, 1 power, 2 already linear
uniform float u_input_gamma;

// The tube's own transfer exponent, relative to the source's assumed 2.2.
// Exactly 1.0 is the neutral and skips the stage entirely. A real CRT is around
// 2.4, so this is about 1.09 -- a small exponent with a visible effect on
// midtones, which is where the extra contrast of a real set comes from.
uniform float u_tube_gamma_exp;

// Linear RGB matrix from the tube's phosphor primaries into sRGB. Identity when
// no phosphor set is selected. Rows sum to 1, so neutrals pass through
// untouched and only saturated colour moves.
uniform mat3  u_phosphor;

// Horizontal RECONSTRUCTION: recovering a continuous signal from the source's
// discrete samples. A resampling choice, not a physical one.
uniform int   u_recon_kind;          // 0 nearest, 1 linear, 2 gaussian, 3 lanczos2
uniform float u_recon_radius;
uniform float u_recon_sharpness;     // 0.5 neutral; higher narrows the kernel
uniform float u_recon_anti_ringing;  // 0 off; clamps overshoot from negative lobes
uniform float u_sample_phase_x;

// Horizontal ELECTRON SPOT: the beam's finite width as it sweeps. A property of
// the tube, applied to the reconstructed signal, and INDEPENDENT of how that
// signal was reconstructed -- a sharper resampling filter does not make the
// electron beam narrower. Widths are in source-pixel units.
//
// These were declared in the parameter block and ignored, which meant the only
// way to soften horizontally was to blunt the reconstruction filter, conflating
// a resampling decision with a physical one.
uniform float u_spot_sigma_dark;
uniform float u_spot_sigma_bright;
uniform float u_spot_luma_exponent;

// Vertical beam. Sigmas are fractions of ONE raster-line pitch.
uniform float u_beam_sigma_min;
uniform float u_beam_sigma_max;
uniform float u_beam_luma_exponent;
uniform float u_beam_shape_exponent;

// Sigma-independent part of the beam kernel's integral, from the CPU (it needs
// a gamma function). Each line is divided by sigma * this.
uniform float u_beam_norm;
uniform float u_scanline_phase;
// Attenuation on the scanline envelope. 1.0 is the physical beam; lower values
// blend toward a merged average. A taste control, never a correction.
uniform float u_scanline_strength;

// Phosphor mask, measured in drawable pixels (spec 5.2).
uniform int   u_mask_layout;         // 0 none, 1 aperture grille, 2 slot, 3 shadow
uniform int   u_mask_order;          // 0 RGB, 1 BGR
uniform float u_mask_pitch;
uniform float u_mask_strength;       // already scaled by resolution adaptation
uniform vec2  u_mask_phase;
// Aperture shape, 0 = pure sinusoid (soft, wide), 1 = narrowest stripe with the
// darkest gaps this construction allows. Energy-free, see aperture_weight.
uniform float u_mask_aperture;
// Phosphor row height as a multiple of the horizontal triad pitch. A slot
// mask's cells are not square and how far from square is a tube property.
uniform float u_mask_row_ratio;
// 0 = mask locked to the output pixel grid, 1 = mask rides the curved face.
uniform int   u_mask_coord_mode;
// Headroom scale (spec 23.4). 1.0 leaves peaks to clip; 1/mask_peak keeps the
// mask's energy and colour intact all the way to white at the cost of overall
// brightness. The two cannot both be had on a 0..1 target.
uniform float u_headroom_scale;

// Output luminance. paper_white scales emitted light before glow so the glow
// scales with it; black_level lifts the floor at encode time, because a real
// tube's black is not zero and a crushed zero is further from it.
uniform float u_paper_white;
uniform float u_black_level;

// Geometry.
// Face shape: 0 flat, 1 spherical, 2 cylindrical (Trinitron-like).
uniform int   u_face_geometry;
// Curvature magnitude of the FACE, per axis. Meaningless when flat.
uniform vec2  u_warp;
// Raster defects -- faults of a unit, not properties of a tube. Zero is exact.
uniform float u_raster_pincushion;
uniform float u_raster_keystone;
uniform float u_raster_rotation;   // radians
uniform vec2  u_overscan;
uniform vec2  u_center;
uniform float u_vignette;

// The tube's own face: corner radius as a fraction of the short side, and how
// far the edge is feathered. Both live in THIS pass because the boundary can
// only be evaluated correctly in the curved frame -- see tube_edge below.
uniform float u_corner_radius;
uniform float u_corner_softness;

// Convergence, in drawable pixels per channel.
uniform vec2  u_conv_r;
uniform vec2  u_conv_g;
uniform vec2  u_conv_b;

// RADIAL convergence error, and the reason a constant offset alone is not a
// convincing model: a tube is converged at the CENTRE and drifts toward the
// edges, because the three beams travel different distances to reach a
// corner. A set misregistered uniformly across the whole screen would have
// been sent back. Red is pushed outward and blue inward, growing with the
// square of the distance from centre.
uniform float u_conv_radial;

// Centre-to-edge focus loss. The beam meets the glass obliquely away from
// centre, so its footprint grows -- every CRT is softer at the edges than in
// the middle, and it is one of the more recognisable things about them.
// Scales both the vertical beam and the horizontal spot, because the same
// widening affects both axes.
uniform float u_focus_edge;

// ---- Optional defects (audit P3) ---------------------------------------
//
// Faults of a PARTICULAR UNIT, never part of a generic tube. Every one is
// exactly zero by default and exactly free when zero -- the branch is skipped,
// not multiplied by nothing.
//
// THE TIMEBASE IS SECONDS, NOT FRAMES. That is the audit's explicit
// requirement and it is not pedantry: hum rolls at a rate set by the beat
// between the mains and the field rate, and flicker is a property of the power
// supply. Drive either from a frame counter and both change speed when the
// presentation cadence does, which is precisely backwards -- a 120Hz display
// would hum twice as fast.
uniform float u_time;              // seconds, monotonic
uniform float u_hum_strength;      // mains coupling: rolling horizontal bars
uniform float u_hum_hz;            // beat frequency between mains and field
uniform float u_noise_strength;    // electronic noise in the video chain
uniform float u_jitter_strength;   // line-to-line horizontal timing instability
uniform float u_flicker_strength;  // supply-driven whole-frame brightness

// Output light. u_output_transfer matches crt_transfer_t: 0 sRGB, 1 power, 2
// linear. Distinct from u_input_transfer (how the SOURCE was encoded) and from
// u_tube_gamma_exp (how the TUBE responds) -- three stages, three owners.
uniform int   u_output_transfer;
uniform float u_output_gamma;
uniform float u_brightness;

// 1 = this pass is drawing straight to the window, so it owns vignette and
// output encoding. 0 = it is filling the emission target and must write LINEAR
// light, leaving vignette and encoding to the final pass (spec 11.2 step 9).
//
// This exists so the degraded path is the SAME shader rather than a second one:
// if the float FBO or the blur programs fail, the renderer sets this to 1 and
// keeps drawing beam and mask without glow, which is exactly the fallback spec
// 15 rule 4 asks for.
uniform int   u_encode_output;

// Number of raster lines each side of centre that contribute to a pixel. 1
// gives the mandatory nearest-three-plus coverage; 2 is the five-line full
// quality mode (spec 11.2).
uniform int   u_beam_taps;

out vec4 frag_color;

const float PI = 3.14159265358979;

// Integer bit-mix hash. Deliberately not the fract(sin(dot(...))) folklore:
// that has visible structure on a regular grid, which on a scanline renderer
// shows up as a stationary pattern rather than as noise.
float hash01(vec3 p)
{
    uvec3 u = uvec3(ivec3(p * 1024.0 + 8192.0));
    uint h = u.x * 374761393u + u.y * 668265263u + u.z * 2246822519u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= h >> 16u;
    return float(h) * (1.0 / 4294967296.0);
}

// Defined with the beam, used by the horizontal spot, which comes first.
float luma(vec3 c);

// ---- transfer functions -------------------------------------------------

vec3 to_linear(vec3 c)
{
    if (u_input_transfer == 2) return c;
    if (u_input_transfer == 1) return pow(max(c, 0.0), vec3(u_input_gamma));
    // sRGB, piecewise -- the toe matters for near-black, which is most of a
    // Game Boy image's dark areas.
    vec3 lo = c / 12.92;
    vec3 hi = pow(max(c + 0.055, 0.0) / 1.055, vec3(2.4));
    return mix(lo, hi, step(vec3(0.04045), c));
}

// HOST encoding -- the third and last transfer stage, and a different thing
// from both the source's encoding and the tube's own response.
//
// The sRGB branch is the real piecewise curve, not pow(1/2.2). They diverge
// most exactly where a CRT image spends most of its time: near black. At a
// linear 0.001 the power function gives 0.0295 and sRGB gives 0.0129 -- more
// than a factor of two, which is the difference between a convincing black and
// a washed-out one.
vec3 encode_srgb(vec3 c)
{
    vec3 lo = c * 12.92;
    vec3 hi = 1.055 * pow(max(c, 0.0), vec3(1.0 / 2.4)) - 0.055;
    return mix(lo, hi, step(vec3(0.0031308), c));
}

vec3 to_display(vec3 c)
{
    c = max(c, 0.0);
    // Black lift last, in linear light: a tube's black is not zero, and the
    // floor should not itself be gamma-shaped.
    c = u_black_level + (1.0 - u_black_level) * c;
    if (u_output_transfer == 0) return encode_srgb(c);
    if (u_output_transfer == 2) return c;                  // already linear
    return pow(c, vec3(1.0 / u_output_gamma));             // plain power
}

// ---- geometry -----------------------------------------------------------

// Normalized tube coordinate [0,1] -> curved coordinate. Zero warp is exactly
// identity (spec 5.6), so curvature can be switched off without residue.
// TWO UNRELATED THINGS, kept apart (audit P2.12).
//
//   FACE GEOMETRY is the shape of the glass. It is what the tube IS, it is
//   never zero on a curved set, and it differs by construction: a consumer
//   shadow-mask tube is a section of a sphere, a Trinitron-style one a section
//   of a CYLINDER -- curved left to right, straight top to bottom. That is a
//   visible difference and it is most of what distinguishes the two on sight.
//
//   RASTER DEFECTS are misadjustments of the deflection drawing onto that
//   glass. Pincushion, rotation and trapezoid are faults of a particular unit,
//   default to zero, and must never be baked into the face shape -- otherwise
//   "this tube is curved" and "this tube is badly adjusted" become one dial.
//
// Order is physical: the yoke draws a (possibly distorted) raster, and the
// glass then presents it. Defects first, face second.

vec2 apply_raster_defects(vec2 c)
{
    // Pincushion: the deflection is not perfectly linear, so the edges bow.
    // Distinct from face curvature, which bows the whole picture including a
    // perfectly drawn raster.
    if (u_raster_pincushion != 0.0) {
        float r2 = dot(c, c);
        c *= 1.0 + u_raster_pincushion * r2;
    }

    // Trapezoid: one end of the raster wider than the other. On a real set this
    // is a vertical-amplitude imbalance, so width varies with y.
    if (u_raster_keystone != 0.0)
        c.x *= 1.0 + u_raster_keystone * c.y;

    // Rotation: the yoke sits a degree or two off. Extremely common on tubes
    // that have been moved, and one of the few defects a viewer names.
    if (u_raster_rotation != 0.0) {
        float a = u_raster_rotation;
        float sa = sin(a), ca = cos(a);
        c = vec2(c.x * ca - c.y * sa, c.x * sa + c.y * ca);
    }
    return c;
}

vec2 apply_face_geometry(vec2 c)
{
    // 0 FLAT: a genuinely flat faceplate. Exactly identity, whatever warp says
    // -- the TYPE decides whether there is curvature at all, so a flat profile
    // cannot acquire a bulge by someone nudging a magnitude.
    if (u_face_geometry == 0) return c;

    vec2 c2 = c * c;

    // 2 CYLINDRICAL: curved about a VERTICAL axis. The horizontal scale varies
    // across the screen and the vertical does not, so horizontal lines stay
    // straight -- which is exactly what a Trinitron looks like and what a
    // spherical warp cannot produce.
    if (u_face_geometry == 2) {
        c.x *= 1.0 + u_warp.x * c2.x;
        return c;
    }

    // 1 SPHERICAL: a section of a sphere, bulging on both axes. Each axis is
    // displaced by the OTHER's distance from centre, which is what makes the
    // corners bow rather than just the edges.
    c.x *= 1.0 + u_warp.x * c2.y;
    c.y *= 1.0 + u_warp.y * c2.x;
    return c;
}

vec2 warp_uv(vec2 uv)
{
    vec2 c = uv * 2.0 - 1.0;                 // [-1,1]
    c = apply_raster_defects(c);
    c = apply_face_geometry(c);
    return c * 0.5 + 0.5;
}

// Coverage of the tube's face at a CURVED coordinate: 1 inside the glass, 0
// outside, antialiased across the boundary.
//
// WHY THIS EXISTS, and why curvature looked absent without it. A CRT reads as
// curved because its EDGE curves. Warping only the sampling coordinate bends
// the picture's interior -- which is nearly invisible on a 256x224 image -- and
// then overscan crops away the one part that would have shown: the region the
// warp pushed off the raster. Corner rounding done later in flat viewport space
// gives a rounded RECTANGLE, which is a different shape from a tube.
//
// So the boundary is evaluated here, in the same curved frame the sampling uses,
// BEFORE overscan. The black surround then follows the curvature, which is the
// whole visual effect.
float tube_edge(vec2 curved)
{
    // Work in square units so a corner is round rather than elliptical.
    float aspect = u_viewport_size.x / max(u_viewport_size.y, 1.0);
    vec2  p = curved * 2.0 - 1.0;
    p.x *= aspect;

    vec2  half_size = vec2(aspect, 1.0);
    float r = clamp(u_corner_radius, 0.0, 0.5);   // fraction of the short side

    // Rounded-box signed distance: negative inside, zero on the glass edge,
    // positive outside.
    //
    // THE min(max(q.x,q.y), 0.0) TERM IS NOT OPTIONAL, however much it looks
    // like it only refines the interior. Without it, dist inside is
    // length(max(q,0)) - r, and when r == 0 that is EXACTLY 0.0 across the
    // whole picture rather than negative -- so smoothstep(-aa, aa, 0.0)
    // returns 0.5 and the entire image is multiplied by a half. It is invisible
    // on any profile with rounded corners (dist = -r inside, correctly
    // negative) and halves the brightness of every profile without them.
    vec2  q = abs(p) - (half_size - vec2(r));
    float dist = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;

    // Feathering derived from the actual screen-space gradient, so the edge
    // stays the same apparent softness at any window size instead of getting
    // harder as the window grows. Softness widens it beyond plain antialiasing.
    float aa = fwidth(dist) * (0.75 + u_corner_softness * 4.0) + 1e-5;
    return 1.0 - smoothstep(-aa, aa, dist);
}

// ---- sampling -----------------------------------------------------------

// Raster position -> texture coordinate. The ONLY place the texture's own size
// is allowed to matter: a 2x upload simply has a larger active rect, and the
// same raster position lands on the same picture content either way.
// An explicit transform from RASTER-ACTIVE coordinates into TEXTURE-ACTIVE
// coordinates, then into normalized texels.
//
// It used to divide by the FULL raster size and then scale by the texture's
// active rect, which is the same answer only when both rects start at zero and
// cover everything. That is the only case shipping today, so nothing was
// visibly wrong -- but the descriptor advertised cropped and offset rectangles
// as supported, and they would have sampled the wrong part of the texture.
//
// Written as a proportion between the two active rectangles, which is what the
// two fields actually mean: "this part of the storage holds that part of the
// raster".
vec2 raster_to_texcoord(vec2 raster_pos)
{
    vec2 within = (raster_pos - u_raster_active.xy) /
                  max(u_raster_active.zw, vec2(1.0));
    vec2 texel  = u_texture_active.xy + within * u_texture_active.zw;
    return texel / u_texture_size;
}

vec3 fetch_line(float raster_x, float line_center_y)
{
    return texture(u_source, raster_to_texcoord(vec2(raster_x, line_center_y))).rgb;
}

float recon_weight(float d)
{
    // Sharpness scales the kernel's width. 0.5 is the neutral, giving exactly
    // the kernel's nominal support; above it the filter narrows and rings more,
    // below it softens. Independent of the spot: this changes how the SIGNAL is
    // reconstructed, not how wide the beam is.
    // Scaling d UP evaluates the kernel further out for the same distance,
    // which narrows it -- so scale must RISE with sharpness. Written the other
    // way round first, which made the control run backwards; the grating
    // measurement caught it, an eyeball would not have.
    float scale = max(0.5 + u_recon_sharpness, 0.25);
    float x = abs(d) * scale;

    if (u_recon_kind == 0) return x < 0.5 ? 1.0 : 0.0;             // nearest
    if (u_recon_kind == 1) return max(0.0, 1.0 - x);               // linear
    if (u_recon_kind == 3) {                                       // lanczos2
        if (x < 1e-5) return 1.0;
        if (x >= 2.0) return 0.0;
        float pix = PI * x;
        return (sin(pix) / pix) * (sin(pix * 0.5) / (pix * 0.5));
    }
    // gaussian, radius interpreted as ~2 sigma so the tails are negligible
    float sigma = max(u_recon_radius, 1e-3) * 0.5;
    return exp(-0.5 * (x * x) / (sigma * sigma));
}

// The reconstruction kernel convolved with the electron spot.
//
// Physically these are two stages in sequence: reconstruct the signal, then
// blur it by the beam's finite width. Convolving the kernels and doing ONE
// weighted fetch is mathematically the same thing and costs a handful of
// register operations instead of taps_recon x taps_spot texture reads.
float combined_weight(float d, float spot_sigma)
{
    if (spot_sigma <= 1e-3) return recon_weight(d);

    float acc = 0.0, wsum = 0.0;
    float h = spot_sigma * 0.7;          // sub-tap spacing inside the spot
    for (int k = -2; k <= 2; k++) {
        float o = float(k) * h;
        float g = exp(-0.5 * (o * o) / (spot_sigma * spot_sigma));
        acc  += recon_weight(d - o) * g;
        wsum += g;
    }
    return wsum > 0.0 ? acc / wsum : 0.0;
}

// Beam width grows with drive, horizontally as well as vertically -- a bright
// horizontal feature measures wider than a dim one on real hardware. Kept on
// its own exponent rather than sharing the vertical beam's, because the two
// axes are limited by different things (spot optics versus video bandwidth).
float spot_sigma_for(float L)
{
    if (u_spot_sigma_bright <= 0.0 && u_spot_sigma_dark <= 0.0) return 0.0;
    return mix(u_spot_sigma_dark, u_spot_sigma_bright,
               pow(clamp(L, 0.0, 1.0), max(u_spot_luma_exponent, 1e-3)));
}

// Reconstruct one raster line horizontally at raster_x, then apply the spot.
vec3 sample_line(float raster_x, float line_center_y, float focus_scale)
{
    // Where inside a source pixel its sample sits. 0.5 is the centre, which is
    // the ordinary assumption; the parameter exists because the horizontal
    // sampling grid has a phase just as the vertical one does, and this is the
    // horizontal counterpart of u_scanline_phase. It was declared and never
    // read until the sensitivity sweep noticed nothing responded to it.
    float phase  = clamp(u_sample_phase_x, 0.0, 1.0);
    float center = raster_x - phase;
    float base   = floor(center);

    // The spot's width depends on how bright this part of the line is, and that
    // is not known until something has been sampled. The nearest sample is a
    // good enough estimate: the spot is a blur, so a small error in its width
    // moves the result far less than the blur itself does.
    float spot = 0.0;
    if (u_spot_sigma_dark > 0.0 || u_spot_sigma_bright > 0.0) {
        vec3 near = to_linear(fetch_line(floor(raster_x) + phase, line_center_y));
        spot = spot_sigma_for(luma(near)) * focus_scale;
    }

    // Nearest with no spot is a plain fetch; with a spot it still needs the
    // weighted loop, because the spot is what makes it non-trivial.
    if (u_recon_kind == 0 && spot <= 1e-3)
        return fetch_line(floor(center) + phase, line_center_y);

    int taps = (u_recon_kind == 3) ? 2 : 1;      // lanczos2 needs +/-2
    if (spot > 1e-3) taps += 2;                  // the spot widens the support

    vec3  acc = vec3(0.0);
    vec3  lo  = vec3(1e9), hi = vec3(-1e9);
    float wsum = 0.0;
    for (int i = -4; i <= 5; i++) {
        if (i < -taps || i > taps + 1) continue;
        float xi = base + float(i);
        float w  = combined_weight(center - xi, spot);
        if (w == 0.0) continue;
        vec3 s = fetch_line(xi + phase, line_center_y);
        acc  += s * w;
        wsum += w;
        lo = min(lo, s);
        hi = max(hi, s);
    }
    vec3 outv = wsum > 0.0 ? acc / wsum : vec3(0.0);

    // Anti-ringing: kernels with negative lobes (lanczos) overshoot at hard
    // edges, which on a 4-colour source reads as a bright halo that was never
    // in the signal. Clamping to the contributing samples' range removes the
    // overshoot; the strength dial exists because some ringing is part of what
    // an analog channel actually does. 0 is an exact bypass.
    if (u_recon_anti_ringing > 0.0)
        outv = mix(outv, clamp(outv, lo, hi), u_recon_anti_ringing);

    return outv;
}

// ---- beam ---------------------------------------------------------------

// Generalized Gaussian. shape 2 is an ordinary Gaussian; higher is flatter with
// steeper shoulders, which is how a well-focused beam behaves as it brightens.
float beam_weight(float d, float sigma)
{
    float s = max(sigma, 1e-4);
    return exp(-0.5 * pow(abs(d) / s, u_beam_shape_exponent));
}

float luma(vec3 c)
{
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

// ---- mask ---------------------------------------------------------------
//
// ENERGY NEUTRALITY IS BUILT IN, NOT CORRECTED AFTERWARDS (spec 11.3).
//
// Every factor below is constructed so that its mean over one full period is
// exactly known, which is what lets the mask darken nothing on average at any
// strength. The alternative -- measuring the pattern's mean and dividing by it
// -- needs a compensation term that then has to be clamped, and a clamped
// compensation is precisely how a mask ends up either dimming the picture or
// clipping highlights to buy the brightness back. Neither happens here: the
// CPU-side compensation stays at 1.0 because there is nothing to compensate.
//
// The identity doing the work: three cosines 120 degrees apart sum to zero, so
//     w_c = (1/3) * (1 + beta * cos(2*pi*(x/pitch - c/3)))
// has mean 1/3 in every channel AND sums to exactly 1 across RGB at every
// single pixel. So the mask cannot shift brightness, only chromaticity -- even
// mid-triad, even at a fractional pitch.

const float TAU = 6.28318530718;

// A periodic dip with mean exactly 1: the raw dip 0.5*(1-cos) has mean 0.5, so
// dividing by (1 - depth*0.5) restores unit mean analytically. Used for the
// horizontal slot gaps and the shadow mask's vertical dot structure.
float unit_mean_dip(float t, float depth)
{
    float w = 0.5 * (1.0 - cos(TAU * fract(t)));    // 0..1, mean 0.5
    return (1.0 - depth * w) / max(1.0 - depth * 0.5, 1e-3);
}

// One channel of the aperture profile. Mean exactly 1/3 over a period, and the
// three channels sum to exactly 1 at every point, for ANY b1/b2 -- which is
// what lets the shape be tuned without touching energy.
float aperture_weight(float ph, float offset, float b1, float b2)
{
    float t = TAU * (ph - offset);
    return (1.0 + b1 * cos(t) + b2 * cos(2.0 * t)) / 3.0;
}

// Evaluated in VIEWPORT-LOCAL DRAWABLE PIXELS so pitch and phase are unaffected
// by the source upload changing between 256x224 and 512x448 (spec 5.2, 11.3).
//
// `flat_px` is that output-grid coordinate; `tube_px` is the same point on the
// CURVED face. u_mask_coord_mode picks between them:
//
//   0  output grid   -- the mask is locked to host pixels. Stable, never
//                       crawls, and geometrically a lie: a real mask sits on
//                       the glass, so it should bend when the glass does.
//   1  tube surface  -- the mask rides the curvature with the raster. Faithful,
//                       and it costs some stability because the projected pitch
//                       varies across the face.
//
// Identical when warp is zero, so this is not a behaviour change for a flat
// profile -- which is why both stay available rather than one replacing the
// other (audit P2.8).
vec3 phosphor_mask(vec2 flat_px, vec2 tube_px)
{
    if (u_mask_layout == 0 || u_mask_strength <= 0.0) return vec3(1.0);

    vec2 p = (u_mask_coord_mode == 1 ? tube_px : flat_px) + u_mask_phase;
    float pitch = max(u_mask_pitch, 1.0);

    // Stripe contrast falls off as the triad approaches the pixel grid. Without
    // this the pattern beats against the pixels and crawls when the window is
    // resized; with it the mask fades out smoothly, which is what spec 11.3
    // requires instead of an unstable one-pixel pattern.
    float beta = clamp((pitch - 2.0) / 2.0, 0.0, 1.0);

    // Slot and shadow masks stagger alternate phosphor rows. Row height is its
    // own parameter rather than a hardcoded fraction of the horizontal pitch:
    // a slot mask's cells are not square, and how far from square is a property
    // of the tube.
    float row_h = max(pitch * max(u_mask_row_ratio, 0.1), 1.0);
    float x = p.x;
    if (u_mask_layout == 2 || u_mask_layout == 3) {
        float row = floor(p.y / row_h);
        x += mod(row, 2.0) * pitch * 0.5;
    }

    // Per-channel stripe weights: mean 1/3 each, summing to 1 everywhere.
    //
    // APERTURE SHAPE VIA THE SECOND HARMONIC. A pure sinusoid is a soft, wide
    // stripe; real phosphor is narrower with darker gaps between. Sharpening it
    // normally means giving up the analytic energy guarantee -- but harmonics
    // whose order is NOT a multiple of three still sum to zero across the triad
    // and still have zero mean, so the 2nd harmonic reshapes the aperture for
    // free. Verified: per-pixel sum stays exactly 1.0 and every channel mean
    // stays exactly 1/3 at any sharpness. The 3rd harmonic is unusable for
    // precisely the reason the 2nd works -- it is in phase across all three.
    float b2 = beta * clamp(u_mask_aperture, 0.0, 1.0) * 0.5;
    float ph = x / pitch;
    vec3 m = vec3(
        aperture_weight(ph, 0.0 / 3.0, beta, b2),
        aperture_weight(ph, 1.0 / 3.0, beta, b2),
        aperture_weight(ph, 2.0 / 3.0, beta, b2));
    if (u_mask_order == 1) m = m.bgr;

    // Layout 2, slot mask: the stripes are broken into slots by a dark band at
    // each row boundary. Shallow, because a consumer slot mask's bridges are
    // thin relative to the phosphor.
    if (u_mask_layout == 2)
        m *= unit_mean_dip(p.y / row_h + 0.5, 0.35 * beta);

    // Layout 3, shadow mask: round dots rather than slots, so the vertical
    // structure is as strong as the horizontal and sits at half-triad offsets.
    if (u_mask_layout == 3)
        m *= unit_mean_dip(p.y / row_h + 0.5, 0.7 * beta);

    // Strength 0 is exactly white: no colour and no brightness change (5.6).
    return mix(vec3(1.0), m * 3.0, u_mask_strength);
}

// ---- main ---------------------------------------------------------------

void main(void)
{
    vec2 frag_px = gl_FragCoord.xy - u_viewport_origin;

    // Normalized position on the tube face.
    vec2 uv = frag_px / u_viewport_size;
    uv.y = 1.0 - uv.y;                       // raster row 0 at the top
    uv += u_center;
    uv = warp_uv(uv);

    // The glass boundary, in the curved frame and BEFORE overscan -- so the
    // curvature is actually visible as a curved edge.
    float edge = tube_edge(uv);

    // The point on the curved FACE, in tube pixels, captured before overscan
    // rescales uv for sampling. This is where a mask physically sits, so it is
    // what the tube-surface mask coordinate mode uses.
    vec2 tube_px = uv * u_viewport_size;

    // Overscan: the tube shows slightly less than the full raster. Applied only
    // to the SAMPLING position, never to the boundary above.
    uv = (uv - 0.5) * (1.0 - 2.0 * u_overscan) + 0.5;

    // Outside the tube, or off the raster entirely: exact black, never a
    // stretched edge texel. Alpha carries coverage on the emission path so the
    // final pass can clip glow to the glass.
    if (edge <= 0.0 ||
        uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        frag_color = vec4(0.0, 0.0, 0.0, u_encode_output == 1 ? 1.0 : 0.0);
        return;
    }

    // Into raster coordinates. u_raster_active lets a future path show only
    // part of the raster without changing any of the maths above.
    vec2 raster_pos = u_raster_active.xy + uv * u_raster_active.zw;

    // Convergence is an OUTPUT-space offset, so it is converted into raster
    // units here rather than being applied to uv.
    vec2 px_to_raster = u_raster_active.zw / u_viewport_size;

    // Vertical: which raster lines can reach this pixel. Line k is centred at
    // k + scanline_phase.
    float line_f  = raster_pos.y - u_scanline_phase;
    float line_i  = floor(line_f);

    // One output pixel spans this many raster lines; when that exceeds ~0.5 the
    // beam must widen or it aliases into moire. The CPU side also reduces mask
    // strength in this situation (spec 15).
    float lines_per_pixel = u_raster_active.w / u_viewport_size.y;

    // Position-dependent optics, evaluated ONCE per pixel rather than per
    // contributing line -- they depend on where the pixel is, not on which
    // line is being gathered.
    vec2 from_centre = (uv - 0.5) * 2.0;
    float r2 = dot(from_centre, from_centre);
    float focus_scale = 1.0 + u_focus_edge * r2;
    vec2 cr = u_conv_r, cg = u_conv_g, cb = u_conv_b;
    if (u_conv_radial != 0.0 && r2 > 1e-6) {
        vec2 dir = normalize(from_centre);
        cr += dir * (u_conv_radial * r2);
        cb -= dir * (u_conv_radial * r2);
    }

    vec3 acc = vec3(0.0);
    vec3 merged = vec3(0.0); // same sum, normalized the merged way
                             // (flat is a reserved GLSL qualifier)
    float wsum = 0.0;

    for (int k = -2; k <= 3; k++) {
        if (k < -u_beam_taps || k > u_beam_taps + 1) continue;
        float ly = line_i + float(k);
        // Bounds are the ACTIVE rect's own range, not 0..height. ly is a
        // full-raster line index, so comparing it against the active HEIGHT
        // only worked while the active rect began at zero.
        if (ly < u_raster_active.y ||
            ly > u_raster_active.y + u_raster_active.w - 1.0) continue;

        float center_y = ly + u_scanline_phase;
        float d = raster_pos.y - center_y;              // in line pitches

        // JITTER: line-to-line horizontal timing instability. Displaces where
        // the LINE starts, so it must be applied per contributing line rather
        // than per pixel -- a whole-picture wobble is a different fault and
        // looks nothing like this. New value every line and every ~60th of a
        // second, from the clock rather than a frame index.
        float line_x = raster_pos.x;
        if (u_jitter_strength > 0.0) {
            float tick = floor(u_time * 60.0);
            line_x += (hash01(vec3(ly, tick, 0.0)) - 0.5) * u_jitter_strength;
        }

        // Per-channel horizontal convergence offset.
        // Split into three samples only where the channels are ACTUALLY apart.
        //
        // Exact equality was fine while convergence was a constant, but radial
        // error makes the three differ at every pixel by an amount that goes to
        // zero at the centre -- so an equality test takes the 3x sampling path
        // across the whole screen to resolve differences far below a pixel.
        // Measured, that alone cost 5.9 -> 15.2 ms at 1440p. A quarter of a
        // pixel is well under what any reconstruction can express, so the cheap
        // path is taken wherever the separation is invisible anyway.
        vec3 s;
        float conv_spread = max(max(length(cr - cg), length(cg - cb)),
                                length(cr - cb));
        if (conv_spread < 0.25) {
            s = sample_line(line_x, center_y, focus_scale);
        } else {
            s.r = sample_line(line_x + cr.x * px_to_raster.x,
                              center_y + cr.y * px_to_raster.y, focus_scale).r;
            s.g = sample_line(line_x + cg.x * px_to_raster.x,
                              center_y + cg.y * px_to_raster.y, focus_scale).g;
            s.b = sample_line(line_x + cb.x * px_to_raster.x,
                              center_y + cb.y * px_to_raster.y, focus_scale).b;
        }
        s = to_linear(s);

        // The gun's transfer curve. Applied PER SAMPLE, before the beam
        // spreads the light, because it is how signal becomes light in the
        // first place -- doing it after the beam would be applying a display
        // characteristic to an image that has already been through the optics.
        if (u_tube_gamma_exp != 1.0)
            s = pow(max(s, 0.0), vec3(u_tube_gamma_exp));

        // Brighter lines bloom wider -- the reason a bright scanline looks fat
        // and a dim one looks thin on real hardware.
        float L = clamp(luma(s), 0.0, 1.0);
        float sigma = mix(u_beam_sigma_min, u_beam_sigma_max,
                          pow(L, max(u_beam_luma_exponent, 1e-3)));

        // Widen the beam when the output cannot resolve individual lines, so
        // the picture degrades to a soft average instead of aliasing.
        sigma = max(sigma, lines_per_pixel * 0.5) * focus_scale;

        // Each line's contribution is normalized by ITS OWN beam integral
        // (sigma * a shape-dependent constant), which conserves that line's
        // energy however wide the beam is.
        //
        // NOT by the summed weights. That looks equivalent and is not: the sum
        // at an output position depends on where the position falls BETWEEN
        // lines, so dividing by it forces every position to the same intensity
        // and the scanline envelope vanishes -- a flat field comes back
        // perfectly flat no matter how tightly the beam is focused. Dividing by
        // the integral leaves the sum free to vary with position, and that
        // variation IS the scanline.
        float w = beam_weight(d, sigma);
        acc  += s * w / (sigma * u_beam_norm);
        merged += s * w;
        wsum += w;
    }

    // scanline_strength is an ATTENUATION on the envelope, not the envelope
    // itself. 1.0 is the physical result computed above; lower values blend
    // toward the fully-merged average, which is what dividing by the summed
    // weights gives. Deliberately arranged so the DEFAULT is the physics --
    // this is a taste control for players who dislike visible scanlines, and
    // must never become the place a beam bug hides.
    vec3 emission = acc;
    if (u_scanline_strength < 1.0 && wsum > 0.0)
        emission = mix(merged / wsum, acc, max(u_scanline_strength, 0.0));

    // HUM: mains coupling into the video, as horizontal bars that ROLL. The
    // roll rate is the beat between the mains and the field rate, which is why
    // it is a frequency in Hz and why the phase comes from the clock -- drive
    // it per frame and the bars would move at a speed set by the display.
    if (u_hum_strength > 0.0) {
        float bar = sin((uv.y * 3.0 - u_time * u_hum_hz) * TAU);
        emission *= 1.0 + u_hum_strength * bar;
    }

    // FLICKER: the supply sagging, so the WHOLE frame breathes together. A
    // single value per instant, not per pixel -- per-pixel would be noise.
    if (u_flicker_strength > 0.0) {
        float f = hash01(vec3(floor(u_time * 60.0), 7.0, 13.0)) - 0.5;
        emission *= 1.0 + u_flicker_strength * f;
    }

    // NOISE: electronic noise in the video chain, so it ADDS rather than
    // multiplies -- a noisy chain lifts black, it does not modulate what is
    // already there. Signed, so it does not brighten on average.
    if (u_noise_strength > 0.0) {
        float n = hash01(vec3(frag_px, floor(u_time * 60.0))) - 0.5;
        emission += u_noise_strength * n;
    }

    emission *= phosphor_mask(frag_px, tube_px) * u_headroom_scale * u_brightness
                * u_paper_white;

    // Into the display's colour space. AFTER the mask, because the mask
    // modulates which phosphor is emitting -- it belongs in the tube's own
    // space, not the monitor's. The matrix is linear and its rows sum to 1, so
    // it preserves both neutrals and the mask's energy neutrality.
    emission = max(u_phosphor * emission, 0.0);

    emission *= edge;

    if (u_encode_output == 0) {
        // Feeding the emission target: linear light in RGB, glass coverage in
        // alpha. Vignette is the final pass's job so it is applied AFTER glow
        // rather than being smeared outward by it; the coverage travels in
        // alpha so glow can be clipped to the tube instead of haloing past it.
        frag_color = vec4(max(emission, 0.0), edge);
        return;
    }

    if (u_vignette > 0.0) {
        vec2 v = uv * (1.0 - uv) * 4.0;      // 1 at centre, 0 at the edges
        emission *= mix(1.0, pow(clamp(v.x * v.y, 0.0, 1.0), 0.25), u_vignette);
    }

    // This pass encodes for the display itself, so GL_FRAMEBUFFER_SRGB must be
    // off (spec 11.5); the two must never both be active.
    frag_color = vec4(to_display(emission), 1.0);
}
