/* SPDX-License-Identifier: MIT
 *
 * crt_renderer_gl.c, see crt_renderer_gl.h. Original OldAmber code.
 *
 * The pass graph (spec 11). Two shapes, and which one runs is decided by what
 * the driver actually gave us rather than by a setting:
 *
 *   full   tube -> emission (linear, RGBA16F)
 *          emission -> bright-pass + blur H -> work A -> blur V -> bloom
 *          emission -> bright-pass + blur H -> work A -> blur V -> halation
 *          emission + bloom + halation -> vignette, corners, encode -> window
 *
 *   direct tube -> vignette, encode -> window
 *
 * The direct shape is not a stub or a simplification written twice: it is the
 * same tube shader with u_encode_output = 1, which is what makes spec 15's
 * degradation rules cheap to honour. Lose the float target, lose a blur
 * program, run at fast quality, or set both glow strengths to zero, and the
 * renderer drops to it without a second code path to keep in sync.
 */

#include "crt_renderer_gl.h"
#include "gl_api.h"
#include "display_gl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define GLOW_DIVISOR   2
#define BLUR_TAPS      12

#define CRT_ATTRIB_POSITION 0

static GLuint s_tube, s_blur, s_final;
static GLuint s_vao, s_vbo;
static int    s_ready;
static int    s_failed;

static GLuint s_fbo;
static GLuint s_emission, s_work_a, s_work_b, s_work_c, s_work_d;
static int    s_vp_w, s_vp_h;
static int    s_glow_w, s_glow_h;
static int    s_targets_ok;
static int    s_targets_failed;
static int    s_limited_precision;
static int    s_force_full_path;
static int    s_force_limited;

static struct {
    GLint source;
    GLint texture_size, texture_active, raster_size, raster_active;
    GLint viewport_origin, viewport_size;
    GLint input_transfer, input_gamma, tube_gamma_exp, phosphor;
    GLint recon_kind, recon_radius, recon_sharpness, recon_anti_ringing;
    GLint sample_phase_x;
    GLint spot_sigma_dark, spot_sigma_bright, spot_luma_exp;
    GLint beam_sigma_min, beam_sigma_max, beam_luma_exp, beam_shape_exp;
    GLint beam_taps, scanline_phase, scanline_strength, beam_norm;
    GLint mask_layout, mask_order, mask_pitch, mask_strength, mask_phase;
    GLint mask_aperture, mask_row_ratio, mask_coord_mode;
    GLint headroom_scale, paper_white, black_level;
    GLint warp, overscan, center, vignette, corner_radius, corner_softness;
    GLint face_geometry, raster_pincushion, raster_keystone, raster_rotation;
    GLint conv_r, conv_g, conv_b, conv_radial, focus_edge;
    GLint time, hum_strength, hum_hz, noise_strength;
    GLint jitter_strength, flicker_strength;
    GLint output_transfer, output_gamma, brightness, encode_output;
} u;

static struct {
    GLint source, texel, direction, sigma, taps, stride;
    GLint bright_pass, threshold, knee, tail;
} ub;

static struct {
    GLint emission, bloom, halation;
    GLint viewport_origin, viewport_size;
    GLint diffusion;
    GLint bloom_strength, diffusion_strength, halation_strength, halation_tint;
    GLint vignette;
    GLint output_transfer, output_gamma, black_level, dither;
} uf;

static char *read_all(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    char *buf;
    long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fclose(f); free(buf); return NULL;
    }
    fclose(f);
    buf[n] = '\0';
    if (len) *len = (size_t)n;
    return buf;
}

static GLuint compile(GLenum type, const char *src, const char *label) {
    GLuint sh = gl_CreateShader(type);
    GLint ok = 0;
    gl_ShaderSource(sh, 1, &src, NULL);
    gl_CompileShader(sh);
    gl_GetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        GLsizei got = 0;
        gl_GetShaderInfoLog(sh, (GLsizei)sizeof log, &got, log);
        printf("[crt] %s failed to compile:\n%.*s\n", label, (int)got, log);
        fflush(stdout);
        gl_DeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint build_program(const char *vert_name, const char *frag_name) {
    const char *dir = DisplayGL_ShaderDir();
    char path[512];
    char *vs_src = NULL, *fs_src = NULL;
    GLuint vs = 0, fs = 0, prog = 0;
    GLint ok = 0;

    if (!dir) {
        printf("[crt] no shaders/ directory found\n");
        fflush(stdout);
        return 0;
    }

    snprintf(path, sizeof path, "%scrt/%s", dir, vert_name);
    vs_src = read_all(path, NULL);
    if (!vs_src) { printf("[crt] cannot read %s\n", path); goto done; }

    snprintf(path, sizeof path, "%scrt/%s", dir, frag_name);
    fs_src = read_all(path, NULL);
    if (!fs_src) { printf("[crt] cannot read %s\n", path); goto done; }

    vs = compile(GL_VERTEX_SHADER, vs_src, vert_name);
    fs = compile(GL_FRAGMENT_SHADER, fs_src, frag_name);
    if (!vs || !fs) goto done;

    prog = gl_CreateProgram();
    gl_AttachShader(prog, vs);
    gl_AttachShader(prog, fs);
    gl_BindFragDataLocation(prog, 0, "frag_color");

    gl_BindAttribLocation(prog, CRT_ATTRIB_POSITION, "a_position");
    gl_LinkProgram(prog);
    gl_GetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        GLsizei got = 0;
        gl_GetProgramInfoLog(prog, (GLsizei)sizeof log, &got, log);
        printf("[crt] %s link failed:\n%.*s\n", frag_name, (int)got, log);
        gl_DeleteProgram(prog);
        prog = 0;
    }

done:
    fflush(stdout);
    if (vs) gl_DeleteShader(vs);
    if (fs) gl_DeleteShader(fs);
    free(vs_src);
    free(fs_src);
    return prog;
}

#define U(field, name) u.field = gl_GetUniformLocation(s_tube, name)

static void resolve_tube_uniforms(void) {
    U(source, "u_source");
    U(texture_size, "u_texture_size");
    U(texture_active, "u_texture_active");
    U(raster_size, "u_raster_size");
    U(raster_active, "u_raster_active");
    U(viewport_origin, "u_viewport_origin");
    U(viewport_size, "u_viewport_size");
    U(input_transfer, "u_input_transfer");
    U(input_gamma, "u_input_gamma");
    U(tube_gamma_exp, "u_tube_gamma_exp");
    U(phosphor, "u_phosphor");
    U(recon_kind, "u_recon_kind");
    U(recon_radius, "u_recon_radius");
    U(recon_sharpness, "u_recon_sharpness");
    U(recon_anti_ringing, "u_recon_anti_ringing");
    U(spot_sigma_dark, "u_spot_sigma_dark");
    U(spot_sigma_bright, "u_spot_sigma_bright");
    U(spot_luma_exp, "u_spot_luma_exponent");
    U(sample_phase_x, "u_sample_phase_x");
    U(beam_sigma_min, "u_beam_sigma_min");
    U(beam_sigma_max, "u_beam_sigma_max");
    U(beam_luma_exp, "u_beam_luma_exponent");
    U(beam_shape_exp, "u_beam_shape_exponent");
    U(beam_taps, "u_beam_taps");
    U(beam_norm, "u_beam_norm");
    U(scanline_phase, "u_scanline_phase");
    U(scanline_strength, "u_scanline_strength");
    U(mask_layout, "u_mask_layout");
    U(mask_order, "u_mask_order");
    U(mask_pitch, "u_mask_pitch");
    U(mask_strength, "u_mask_strength");
    U(mask_phase, "u_mask_phase");
    U(mask_aperture, "u_mask_aperture");
    U(mask_row_ratio, "u_mask_row_ratio");
    U(mask_coord_mode, "u_mask_coord_mode");
    U(headroom_scale, "u_headroom_scale");
    U(paper_white, "u_paper_white");
    U(black_level, "u_black_level");
    U(warp, "u_warp");
    U(face_geometry, "u_face_geometry");
    U(raster_pincushion, "u_raster_pincushion");
    U(raster_keystone, "u_raster_keystone");
    U(raster_rotation, "u_raster_rotation");
    U(overscan, "u_overscan");
    U(center, "u_center");
    U(vignette, "u_vignette");
    U(corner_radius, "u_corner_radius");
    U(corner_softness, "u_corner_softness");
    U(conv_r, "u_conv_r");
    U(conv_g, "u_conv_g");
    U(conv_b, "u_conv_b");
    U(conv_radial, "u_conv_radial");
    U(focus_edge, "u_focus_edge");
    U(time, "u_time");
    U(hum_strength, "u_hum_strength");
    U(hum_hz, "u_hum_hz");
    U(noise_strength, "u_noise_strength");
    U(jitter_strength, "u_jitter_strength");
    U(flicker_strength, "u_flicker_strength");
    U(output_transfer, "u_output_transfer");
    U(output_gamma, "u_output_gamma");
    U(brightness, "u_brightness");
    U(encode_output, "u_encode_output");
}

#undef U
#define U(field, name) ub.field = gl_GetUniformLocation(s_blur, name)

static void resolve_blur_uniforms(void) {
    U(source, "u_source");
    U(texel, "u_texel");
    U(direction, "u_direction");
    U(sigma, "u_sigma");
    U(taps, "u_taps");
    U(stride, "u_stride");
    U(bright_pass, "u_bright_pass");
    U(threshold, "u_threshold");
    U(knee, "u_knee");
    U(tail, "u_tail");
}

#undef U
#define U(field, name) uf.field = gl_GetUniformLocation(s_final, name)

static void resolve_final_uniforms(void) {
    U(emission, "u_emission");
    U(bloom, "u_bloom");
    U(halation, "u_halation");
    U(diffusion, "u_diffusion");
    U(viewport_origin, "u_viewport_origin");
    U(viewport_size, "u_viewport_size");
    U(bloom_strength, "u_bloom_strength");
    U(halation_strength, "u_halation_strength");
    U(diffusion_strength, "u_diffusion_strength");
    U(halation_tint, "u_halation_tint");
    U(vignette, "u_vignette");
    U(output_transfer, "u_output_transfer");
    U(output_gamma, "u_output_gamma");
    U(black_level, "u_black_level");
    U(dither, "u_dither");
}

#undef U

static GLuint make_target(int w, int h, GLenum internal, GLenum type) {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internal, w, h, 0, GL_RGBA, type, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

static void destroy_targets(void) {
    GLuint texs[5];
    texs[0] = s_emission; texs[1] = s_work_a;
    texs[2] = s_work_b;   texs[3] = s_work_c;  texs[4] = s_work_d;
    if (texs[0] || texs[1] || texs[2] || texs[3] || texs[4])
        glDeleteTextures(5, texs);
    s_emission = s_work_a = s_work_b = s_work_c = s_work_d = 0;
    if (s_fbo) { gl_DeleteFramebuffers(1, &s_fbo); s_fbo = 0; }
    s_vp_w = s_vp_h = s_glow_w = s_glow_h = 0;
    s_targets_ok = 0;
}

void CrtRendererGL_TestForceFullPath(int on) { s_force_full_path = on ? 1 : 0; }

void CrtRendererGL_TestForceLimitedPrecision(int on) {
    if (s_force_limited == (on ? 1 : 0)) return;
    s_force_limited = on ? 1 : 0;

    destroy_targets();
    s_targets_failed = 0;
    s_limited_precision = 0;
}

static int ensure_targets(int vw, int vh) {
    GLenum internal = GL_RGBA16F, type = GL_HALF_FLOAT;
    int attempt;

    if (s_force_limited) { internal = GL_RGBA8; type = GL_UNSIGNED_BYTE; }

    if (s_targets_ok && vw == s_vp_w && vh == s_vp_h) return 1;
    if (s_targets_failed) return 0;
    destroy_targets();
    if (vw <= 0 || vh <= 0) return 0;

    s_glow_w = vw / GLOW_DIVISOR; if (s_glow_w < 1) s_glow_w = 1;
    s_glow_h = vh / GLOW_DIVISOR; if (s_glow_h < 1) s_glow_h = 1;

    gl_GenFramebuffers(1, &s_fbo);
    if (!s_fbo) { s_targets_failed = 1; return 0; }

    for (attempt = 0; attempt < 2; attempt++) {
        GLenum status;

        s_emission = make_target(vw, vh, internal, type);
        s_work_a   = make_target(s_glow_w, s_glow_h, internal, type);
        s_work_b   = make_target(s_glow_w, s_glow_h, internal, type);
        s_work_c   = make_target(s_glow_w, s_glow_h, internal, type);
        s_work_d   = make_target(s_glow_w, s_glow_h, internal, type);

        gl_BindFramebuffer(GL_FRAMEBUFFER, s_fbo);
        gl_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_2D, s_emission, 0);
        status = gl_CheckFramebufferStatus(GL_FRAMEBUFFER);
        gl_BindFramebuffer(GL_FRAMEBUFFER, 0);

        if (status == GL_FRAMEBUFFER_COMPLETE) {
            s_vp_w = vw; s_vp_h = vh;
            s_targets_ok = 1;
            if (attempt == 1 && !s_limited_precision) {
                s_limited_precision = 1;
                printf("[crt] RGBA16F unavailable; using bounded RGBA8 targets\n");
                fflush(stdout);
            }
            return 1;
        }

        {
            GLuint texs[5];
            texs[0] = s_emission; texs[1] = s_work_a;
            texs[2] = s_work_b;   texs[3] = s_work_c;  texs[4] = s_work_d;
            glDeleteTextures(5, texs);
            s_emission = s_work_a = s_work_b = s_work_c = s_work_d = 0;
        }
        internal = GL_RGBA8;
        type = GL_UNSIGNED_BYTE;
    }

    gl_DeleteFramebuffers(1, &s_fbo);
    s_fbo = 0;
    s_targets_failed = 1;
    printf("[crt] no usable render target; glow disabled\n");
    fflush(stdout);
    return 0;
}

int CrtRendererGL_Init(void) {
    static const GLfloat quad[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 0.0f, 1.0f,
    };
    GLint pos;

    if (s_ready)  return 0;
    if (s_failed) return -1;

    if (GlApi_Load() != 0) { s_failed = 1; CrtRenderer_SetAvailable(0); return -1; }

    s_tube = build_program("tube.vert", "tube.frag");
    if (!s_tube) { s_failed = 1; CrtRenderer_SetAvailable(0); return -1; }
    resolve_tube_uniforms();

    s_blur  = build_program("blur.vert", "blur.frag");
    s_final = build_program("blur.vert", "final.frag");
    if (s_blur)  resolve_blur_uniforms();
    if (s_final) resolve_final_uniforms();
    if (!s_blur || !s_final) {
        printf("[crt] glow passes unavailable; tube only\n");
        fflush(stdout);
    }

    gl_GenVertexArrays(1, &s_vao);
    gl_BindVertexArray(s_vao);
    gl_GenBuffers(1, &s_vbo);
    gl_BindBuffer(GL_ARRAY_BUFFER, s_vbo);
    gl_BufferData(GL_ARRAY_BUFFER, (ptrdiff_t)sizeof quad, quad, GL_STATIC_DRAW);
    pos = gl_GetAttribLocation(s_tube, "a_position");
    if (pos != CRT_ATTRIB_POSITION) pos = CRT_ATTRIB_POSITION;
    gl_EnableVertexAttribArray((GLuint)pos);
    gl_VertexAttribPointer((GLuint)pos, 4, GL_FLOAT, GL_FALSE, 0, 0);
    gl_BindVertexArray(0);

    s_ready = 1;
    CrtRenderer_SetAvailable(1);
    printf("[crt] renderer ready (tube%s)\n",
           (s_blur && s_final) ? " + glow" : " only");
    fflush(stdout);
    return 0;
}

void CrtRendererGL_Shutdown(void) {
    destroy_targets();
    if (s_vbo) { gl_DeleteBuffers(1, &s_vbo); s_vbo = 0; }
    if (s_vao) { gl_DeleteVertexArrays(1, &s_vao); s_vao = 0; }
    if (s_tube)  { gl_DeleteProgram(s_tube);  s_tube = 0; }
    if (s_blur)  { gl_DeleteProgram(s_blur);  s_blur = 0; }
    if (s_final) { gl_DeleteProgram(s_final); s_final = 0; }
    s_ready = 0;
    s_failed = 0;
    s_targets_failed = 0;
    s_limited_precision = 0;
    CrtRenderer_SetAvailable(0);
}

static float adapt_mask_strength(const crt_params_t *p,
                                 float rows_per_line, float pitch) {
    float s = p->mask_strength;
    if (s <= 0.0f || p->mask_layout == CRT_MASK_NONE) return 0.0f;

    if (rows_per_line < 2.0f)
        s *= (rows_per_line <= 1.0f) ? 0.0f : (rows_per_line - 1.0f);

    if (pitch < 3.0f)
        s *= (pitch <= 2.0f) ? 0.0f : (pitch - 2.0f);

    return s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
}

static void draw_quad(void) {
    gl_BindVertexArray(s_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl_BindVertexArray(0);
}

static void bind_target(GLuint tex, int w, int h) {
    gl_BindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    gl_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D, tex, 0);
    glViewport(0, 0, w, h);
}

static void blur_pass(GLuint src, GLuint dst, int w, int h,
                      float sigma, float dir_x, float dir_y,
                      int bright, float threshold, float knee, float tail) {
    float stride;

    stride = (sigma * 3.0f) / (float)BLUR_TAPS;
    if (stride < 1.0f) stride = 1.0f;

    bind_target(dst, w, h);
    gl_UseProgram(s_blur);
    gl_ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src);
    gl_Uniform1i(ub.source, 0);
    gl_Uniform2f(ub.texel, 1.0f / (float)w, 1.0f / (float)h);
    gl_Uniform2f(ub.direction, dir_x, dir_y);
    gl_Uniform1f(ub.sigma, sigma);
    gl_Uniform1i(ub.taps, BLUR_TAPS);
    gl_Uniform1f(ub.stride, stride);
    gl_Uniform1i(ub.bright_pass, bright);
    gl_Uniform1f(ub.threshold, threshold);
    gl_Uniform1f(ub.knee, knee);
    gl_Uniform1f(ub.tail, tail);
    draw_quad();
}

static void glow_chain(GLuint dst, float radius_px, int thresholded,
                       float threshold, float knee, float tail) {

    float sigma = radius_px / (float)GLOW_DIVISOR;
    if (sigma < 0.5f) sigma = 0.5f;

    blur_pass(s_emission, s_work_a, s_glow_w, s_glow_h,
              sigma, 1.0f, 0.0f, thresholded, threshold, knee, tail);
    blur_pass(s_work_a, dst, s_glow_w, s_glow_h,
              sigma, 0.0f, 1.0f, 0, threshold, knee, tail);
}

int CrtRendererGL_Draw(unsigned source_texture,
                       const crt_frame_desc_t *frame,
                       int vx, int vy, int vw, int vh,
                       double now_seconds) {
    crt_params_t p;
    const char *why = NULL;
    float rows_per_line, mask_strength, comp, mask_pitch;
    float bloom_s, halation_s, diffusion_s;
    int want_glow, full_path;

    if (s_failed) return -1;
    if (!s_ready && CrtRendererGL_Init() != 0) return -1;
    if (!frame || vw <= 0 || vh <= 0) return 0;
    if (!CrtRenderer_ValidateFrame(frame, &why)) {
        static int logged;
        if (!logged) {
            logged = 1;
            printf("[crt] invalid frame descriptor: %s\n", why ? why : "?");
            fflush(stdout);
        }
        return 0;
    }
    if (!CrtRenderer_GetParams(&p)) return 0;
    if (p.profile == CRT_PROFILE_OFF) return 0;

    rows_per_line = (float)vh / (float)frame->raster_active_rect.h;

    mask_pitch    = CrtRenderer_MaskPitchForViewport(&p, vh);
    mask_strength = adapt_mask_strength(&p, rows_per_line, mask_pitch);

    comp = CrtRenderer_HeadroomScale(&p, mask_pitch);

    bloom_s    = p.bloom_strength;
    halation_s = p.halation_strength;
    diffusion_s = p.diffusion_strength;

    if (p.bloom_radius_lines <= 0.0f && p.bloom_radius_pixels <= 0.0f)
        bloom_s = 0.0f;
    if (p.diffusion_radius_lines <= 0.0f && p.diffusion_radius_pixels <= 0.0f)
        diffusion_s = 0.0f;
    if (p.halation_radius_lines <= 0.0f && p.halation_radius_pixels <= 0.0f)
        halation_s = 0.0f;

    if (s_limited_precision) {
        if (bloom_s > 0.5f)     bloom_s = 0.5f;
        if (diffusion_s > 0.5f) diffusion_s = 0.5f;
        if (halation_s > 0.5f)  halation_s = 0.5f;
    }

    want_glow = (bloom_s > 0.0f || diffusion_s > 0.0f || halation_s > 0.0f) ||
                s_force_full_path;
    full_path = want_glow && s_blur && s_final &&
                p.quality != CRT_QUALITY_FAST &&
                ensure_targets(vw, vh);

    if (full_path) {
        bind_target(s_emission, vw, vh);
    } else {
        gl_BindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(vx, vy, vw, vh);
    }

    gl_UseProgram(s_tube);
    gl_ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)source_texture);
    gl_Uniform1i(u.source, 0);

    gl_Uniform2f(u.texture_size, (GLfloat)frame->texture_width,
                                 (GLfloat)frame->texture_height);
    gl_Uniform4f(u.texture_active,
                 (GLfloat)frame->texture_active_rect.x,
                 (GLfloat)frame->texture_active_rect.y,
                 (GLfloat)frame->texture_active_rect.w,
                 (GLfloat)frame->texture_active_rect.h);
    gl_Uniform2f(u.raster_size, (GLfloat)frame->raster_width,
                                (GLfloat)frame->raster_height);
    gl_Uniform4f(u.raster_active,
                 (GLfloat)frame->raster_active_rect.x,
                 (GLfloat)frame->raster_active_rect.y,
                 (GLfloat)frame->raster_active_rect.w,
                 (GLfloat)frame->raster_active_rect.h);

    if (full_path) {
        gl_Uniform2f(u.viewport_origin, 0.0f, 0.0f);
    } else {
        gl_Uniform2f(u.viewport_origin, (GLfloat)vx, (GLfloat)vy);
    }
    gl_Uniform2f(u.viewport_size, (GLfloat)vw, (GLfloat)vh);

    gl_Uniform1i(u.input_transfer, (GLint)frame->input_transfer);

    gl_Uniform1f(u.input_gamma,
                 frame->input_gamma > 0.0f ? frame->input_gamma : 2.2f);

    gl_Uniform1f(u.tube_gamma_exp, CrtRenderer_TubeGammaExponent(&p, frame));

    gl_UniformMatrix3fv(u.phosphor, 1, GL_TRUE,
                        CrtRenderer_PhosphorMatrix(p.phosphor,
                                                   p.white_point_kelvin));

    gl_Uniform1i(u.recon_kind,     (GLint)p.reconstruction);
    gl_Uniform1f(u.recon_radius,   p.reconstruction_radius);
    gl_Uniform1f(u.recon_sharpness, p.reconstruction_sharpness);
    gl_Uniform1f(u.recon_anti_ringing, p.reconstruction_anti_ringing);
    gl_Uniform1f(u.spot_sigma_dark, p.spot_sigma_dark);
    gl_Uniform1f(u.spot_sigma_bright, p.spot_sigma_bright);
    gl_Uniform1f(u.spot_luma_exp, p.spot_luma_exponent);
    gl_Uniform1f(u.sample_phase_x, p.sample_phase_x);

    gl_Uniform1f(u.beam_sigma_min, p.beam_sigma_min);
    gl_Uniform1f(u.beam_sigma_max, p.beam_sigma_max);
    gl_Uniform1f(u.beam_luma_exp,  p.beam_luma_exponent);
    gl_Uniform1f(u.beam_shape_exp, p.beam_shape_exponent);
    gl_Uniform1i(u.beam_taps,      p.quality == CRT_QUALITY_FAST ? 1 : 2);
    gl_Uniform1f(u.beam_norm,
                 CrtRenderer_BeamNormalization(p.beam_shape_exponent));
    gl_Uniform1f(u.scanline_phase, p.scanline_phase);
    gl_Uniform1f(u.scanline_strength, p.scanline_strength);

    gl_Uniform1i(u.mask_layout,   (GLint)p.mask_layout);
    gl_Uniform1i(u.mask_order,    (GLint)p.mask_order);
    gl_Uniform1f(u.mask_pitch,    mask_pitch);
    gl_Uniform1f(u.mask_strength, mask_strength);
    gl_Uniform2f(u.mask_phase,    p.mask_phase_x, p.mask_phase_y);
    gl_Uniform1f(u.mask_aperture, p.mask_aperture);
    gl_Uniform1f(u.mask_row_ratio, p.mask_row_ratio);
    gl_Uniform1i(u.mask_coord_mode, (GLint)p.mask_coord_mode);
    gl_Uniform1f(u.headroom_scale, comp);
    gl_Uniform1f(u.paper_white, p.paper_white > 0.0f ? p.paper_white : 1.0f);
    gl_Uniform1f(u.black_level, p.black_level);

    gl_Uniform2f(u.warp,     p.warp_x, p.warp_y);
    gl_Uniform1i(u.face_geometry, (GLint)p.face_geometry);
    gl_Uniform1f(u.raster_pincushion, p.raster_pincushion);
    gl_Uniform1f(u.raster_keystone, p.raster_keystone);
    gl_Uniform1f(u.raster_rotation, p.raster_rotation);
    gl_Uniform2f(u.overscan, p.overscan_x, p.overscan_y);
    gl_Uniform2f(u.center,   p.center_x, p.center_y);

    gl_Uniform1f(u.vignette, full_path ? 0.0f : p.vignette_strength);
    gl_Uniform1f(u.corner_radius, p.corner_radius);
    gl_Uniform1f(u.corner_softness, p.corner_softness);

    gl_Uniform2f(u.conv_r, p.convergence_r_x, p.convergence_r_y);
    gl_Uniform2f(u.conv_g, p.convergence_g_x, p.convergence_g_y);
    gl_Uniform2f(u.conv_b, p.convergence_b_x, p.convergence_b_y);
    gl_Uniform1f(u.conv_radial, p.convergence_radial);
    gl_Uniform1f(u.focus_edge, p.focus_edge);
    gl_Uniform1f(u.time, (GLfloat)now_seconds);
    gl_Uniform1f(u.hum_strength, p.hum_strength);
    gl_Uniform1f(u.hum_hz, p.hum_hz > 0.0f ? p.hum_hz : 0.9f);
    gl_Uniform1f(u.noise_strength, p.noise_strength);
    gl_Uniform1f(u.jitter_strength, p.jitter_strength);
    gl_Uniform1f(u.flicker_strength, p.flicker_strength);

    gl_Uniform1i(u.output_transfer, (GLint)p.output_transfer);
    gl_Uniform1f(u.output_gamma, p.output_gamma > 0.0f ? p.output_gamma : 2.2f);
    gl_Uniform1f(u.brightness,
                 p.brightness_compensation > 0.0f ? p.brightness_compensation : 1.0f);
    gl_Uniform1i(u.encode_output, full_path ? 0 : 1);

    glDisable(GL_FRAMEBUFFER_SRGB);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);

    draw_quad();

    if (!full_path) return 1;

    {

        float bloom_thr = p.bloom_threshold * comp;
        float hal_thr   = p.halation_threshold * comp;

        if (bloom_s > 0.0f)
            glow_chain(s_work_b,
                       CrtRenderer_GlowRadiusForViewport(p.bloom_radius_lines,
                                                         p.bloom_radius_pixels,
                                                         vh, frame->raster_active_rect.h),
                       1, bloom_thr, p.bloom_knee, p.bloom_tail);

        if (diffusion_s > 0.0f)
            glow_chain(s_work_d,
                       CrtRenderer_GlowRadiusForViewport(p.diffusion_radius_lines,
                                                         p.diffusion_radius_pixels,
                                                         vh, frame->raster_active_rect.h),
                       0, 0.0f, 0.0f, p.diffusion_tail);

        if (halation_s > 0.0f)
            glow_chain(s_work_c,
                       CrtRenderer_GlowRadiusForViewport(p.halation_radius_lines,
                                                         p.halation_radius_pixels,
                                                         vh, frame->raster_active_rect.h),
                       1, hal_thr, p.halation_knee, p.halation_tail);
    }

    gl_BindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(vx, vy, vw, vh);

    gl_UseProgram(s_final);
    gl_ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_emission);
    gl_Uniform1i(uf.emission, 0);

    gl_ActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloom_s > 0.0f ? s_work_b : s_emission);
    gl_Uniform1i(uf.bloom, 1);
    gl_ActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, halation_s > 0.0f ? s_work_c : s_emission);
    gl_Uniform1i(uf.halation, 2);
    gl_ActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, diffusion_s > 0.0f ? s_work_d : s_emission);
    gl_Uniform1i(uf.diffusion, 3);

    gl_Uniform2f(uf.viewport_origin, (GLfloat)vx, (GLfloat)vy);
    gl_Uniform2f(uf.viewport_size,   (GLfloat)vw, (GLfloat)vh);

    gl_Uniform1f(uf.bloom_strength,     bloom_s);
    gl_Uniform1f(uf.diffusion_strength, diffusion_s);
    gl_Uniform1f(uf.halation_strength,  halation_s);
    gl_Uniform3f(uf.halation_tint,
                 p.halation_tint_r, p.halation_tint_g, p.halation_tint_b);

    gl_Uniform1f(uf.vignette,     p.vignette_strength);
    gl_Uniform1i(uf.output_transfer, (GLint)p.output_transfer);
    gl_Uniform1f(uf.output_gamma, p.output_gamma > 0.0f ? p.output_gamma : 2.2f);
    gl_Uniform1f(uf.black_level, p.black_level);
    gl_Uniform1i(uf.dither, 1);

    draw_quad();

    gl_ActiveTexture(GL_TEXTURE0);
    return 1;
}
