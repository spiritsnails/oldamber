// SPDX-License-Identifier: MIT
// OldAmber CRT renderer -- final composite. Original OldAmber code.
//
// Combines emission with glow and halation, applies vignette and the rounded
// screen edge, encodes the output transfer, and dithers (spec 11.5).
//
// Everything up to this point has been in LINEAR light. This is the single
// place the image is encoded for the display, which is why GL_FRAMEBUFFER_SRGB
// must be off while it runs -- otherwise the encoding happens twice and every
// midtone lifts.
#version 150

uniform sampler2D u_emission;
uniform sampler2D u_bloom;
uniform sampler2D u_halation;
uniform sampler2D u_diffusion;

uniform vec2  u_viewport_origin;
uniform vec2  u_viewport_size;

uniform float u_bloom_strength;
uniform float u_diffusion_strength;
uniform float u_halation_strength;
uniform vec3  u_halation_tint;

uniform float u_vignette;

uniform int   u_output_transfer;   // crt_transfer_t: 0 sRGB, 1 power, 2 linear
uniform float u_output_gamma;
uniform float u_black_level;
uniform int   u_dither;

out vec4 frag_color;

// Ordered dither, +/- half a code value. An 8-bit target quantizes the smooth
// gradients bloom produces into visible bands, especially in the near-black a
// CRT spends most of its time in; this breaks the banding without adding
// perceptible noise.
float dither_offset(vec2 p)
{
    const mat4 bayer = mat4(
         0.0,  8.0,  2.0, 10.0,
        12.0,  4.0, 14.0,  6.0,
         3.0, 11.0,  1.0,  9.0,
        15.0,  7.0, 13.0,  5.0);
    int x = int(mod(p.x, 4.0));
    int y = int(mod(p.y, 4.0));
    return (bayer[y][x] / 16.0 - 0.5) / 255.0;
}

void main(void)
{
    vec2 frag_px = gl_FragCoord.xy - u_viewport_origin;
    vec2 uv = frag_px / u_viewport_size;

    vec4 em = texture(u_emission, uv);
    vec3 c = em.rgb;

    // Glow is accumulated separately so the glass boundary can be applied to it
    // ONCE, below. The tube pass has already applied that boundary to em.rgb;
    // multiplying the combined sum by it again would square it and darken the
    // picture -- which is exactly the bug that shipped a half-brightness image.
    vec3 glow = vec3(0.0);

    // Additive, because glow and halation are light ARRIVING at the eye from
    // elsewhere on the tube -- they add to what the phosphor at this point is
    // emitting rather than replacing it.
    if (u_bloom_strength > 0.0)
        glow += texture(u_bloom, uv).rgb * u_bloom_strength;

    // Glass diffusion: NOT thresholded upstream, so this carries the whole
    // picture softened rather than just its highlights. It is what separates a
    // slightly milky faceplate from a clear one, and a bright-pass cannot see
    // it at all.
    if (u_diffusion_strength > 0.0)
        glow += texture(u_diffusion, uv).rgb * u_diffusion_strength;

    // Halation picks up the glass's own tint on the way through, which is why
    // it is warmer than the bloom on a real set.
    if (u_halation_strength > 0.0)
        glow += texture(u_halation, uv).rgb * u_halation_strength * u_halation_tint;

    // Clip only the glow to the glass: it is generated from a blur that spreads
    // past the bezel, and light does not escape the tube. em.rgb is already
    // bounded, so it must not be multiplied again.
    c += glow * em.a;

    if (u_vignette > 0.0) {
        vec2 v = uv * (1.0 - uv) * 4.0;
        c *= mix(1.0, pow(clamp(v.x * v.y, 0.0, 1.0), 0.25), u_vignette);
    }

    c = max(c, 0.0);
    // Black lift in linear light, matching tube.frag's direct path exactly so
    // the two encode identically.
    c = u_black_level + (1.0 - u_black_level) * c;

    // Same three-way encode as tube.frag's to_display(). The two paths MUST
    // agree bit for bit here, or switching glow on and off would shift the
    // whole tone scale.
    vec3 outc;
    if (u_output_transfer == 0) {
        vec3 lo = c * 12.92;
        vec3 hi = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
        outc = mix(lo, hi, step(vec3(0.0031308), c));
    } else if (u_output_transfer == 2) {
        outc = c;
    } else {
        outc = pow(c, vec3(1.0 / u_output_gamma));
    }
    if (u_dither == 1) outc += dither_offset(gl_FragCoord.xy);

    frag_color = vec4(outc, 1.0);
}
