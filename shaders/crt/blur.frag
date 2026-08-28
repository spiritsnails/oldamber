// SPDX-License-Identifier: MIT
// OldAmber CRT renderer -- separable Gaussian blur. Original OldAmber code.
//
// Used for BOTH glow and halation (spec 11.4). They share this pass and its
// downsampled targets, but keep independent radii and strengths -- the spec is
// explicit that they must not collapse into one "glow" number, because they are
// different physics: glow is light scattering in the phosphor and the electron
// optics, halation is light bouncing inside the glass, which travels much
// further and tints toward red on a real tube.
//
// Two passes, horizontal then vertical. A true 2D Gaussian is separable, so
// this costs 2N samples instead of N*N for the same result.
#version 150

uniform sampler2D u_source;
uniform vec2  u_texel;        // 1 / source size, in that source's pixels
uniform vec2  u_direction;    // (1,0) horizontal, (0,1) vertical
uniform float u_sigma;        // in SOURCE pixels of this pass
uniform int   u_taps;         // samples each side of centre
uniform float u_stride;       // spacing between samples, in source pixels

// Tail shape: 2.0 is an ordinary Gaussian, below 2 gives a longer tail (light
// carrying further with a weaker core), above 2 a shorter one.
//
// The three spreading mechanisms genuinely differ here, which is why it is a
// parameter and not a constant. Scatter inside the phosphor falls off quickly;
// light bouncing between the faceplate's surfaces reaches much further for its
// brightness, so halation with a Gaussian tail either looks too tight near the
// source or too hazy far from it, and cannot be made to look right by changing
// the radius alone.
uniform float u_tail;

// Bright-pass, applied only on the first (horizontal) pass so the threshold is
// not applied twice.
uniform int   u_bright_pass;
uniform float u_threshold;
uniform float u_knee;

out vec4 frag_color;

float luma(vec3 c)
{
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

// Soft-knee threshold: a hard cutoff makes bloom pop on and off as a highlight
// crosses it, which reads as flicker on moving content.
vec3 bright_pass(vec3 c)
{
    float l = luma(c);
    float knee = max(u_knee, 1e-4);
    float t = clamp((l - u_threshold + knee) / (2.0 * knee), 0.0, 1.0);
    float soft = t * t * (l - u_threshold + knee) * 0.5;
    float contrib = max(soft, l - u_threshold);
    return c * (l > 1e-5 ? max(contrib, 0.0) / l : 0.0);
}

void main(void)
{
    vec2 uv = gl_FragCoord.xy * u_texel;
    vec2 step_uv = u_direction * u_texel;
    float sigma = max(u_sigma, 1e-3);

    vec3 acc = vec3(0.0);
    float wsum = 0.0;

    // Samples are spaced u_stride source pixels apart rather than one per pixel,
    // so a wide halation radius costs the same as a narrow bloom. The gaps are
    // filled by the target's bilinear filtering, which is exact enough here
    // because the signal being blurred is already low-frequency -- and it is the
    // only way a 64-pixel radius fits in a bounded loop.
    for (int i = -16; i <= 16; i++) {
        if (i < -u_taps || i > u_taps) continue;
        float d = float(i) * u_stride;
        float w = exp(-0.5 * pow(abs(d) / sigma, max(u_tail, 0.5)));
        vec3 s = texture(u_source, uv + step_uv * d).rgb;
        if (u_bright_pass == 1) s = bright_pass(s);
        acc  += s * w;
        wsum += w;
    }

    frag_color = vec4(wsum > 0.0 ? acc / wsum : vec3(0.0), 1.0);
}
