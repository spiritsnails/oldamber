
#include "display_gl.h"
#include "crt_renderer_gl.h"
#include "gl_api.h"
#include "display.h"
#include "../game/constants.h"

#include <SDL_opengl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef GL_MAJOR_VERSION
#define GL_MAJOR_VERSION 0x821B
#define GL_MINOR_VERSION 0x821C
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER   0x8B31
#define GL_COMPILE_STATUS  0x8B81
#define GL_LINK_STATUS     0x8B82
#define GL_ARRAY_BUFFER    0x8892
#define GL_STATIC_DRAW     0x88E4
#define GL_TEXTURE0        0x84C0
#define GL_TEXTURE1        0x84C1
#define GL_CLAMP_TO_EDGE   0x812F
#endif

typedef char GLchar_;
typedef ptrdiff_t GLsizeiptr_;

static GLuint (APIENTRY *p_glCreateShader)(GLenum);
static void   (APIENTRY *p_glShaderSource)(GLuint, GLsizei, const GLchar_ *const *, const GLint *);
static void   (APIENTRY *p_glCompileShader)(GLuint);
static void   (APIENTRY *p_glGetShaderiv)(GLuint, GLenum, GLint *);
static void   (APIENTRY *p_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei *, GLchar_ *);
static GLuint (APIENTRY *p_glCreateProgram)(void);
static void   (APIENTRY *p_glAttachShader)(GLuint, GLuint);
static void   (APIENTRY *p_glLinkProgram)(GLuint);
static void   (APIENTRY *p_glGetProgramiv)(GLuint, GLenum, GLint *);
static void   (APIENTRY *p_glGetProgramInfoLog)(GLuint, GLsizei, GLsizei *, GLchar_ *);
static void   (APIENTRY *p_glDeleteShader)(GLuint);
static void   (APIENTRY *p_glDeleteProgram)(GLuint);
static void   (APIENTRY *p_glUseProgram)(GLuint);
static GLint  (APIENTRY *p_glGetAttribLocation)(GLuint, const GLchar_ *);
static GLint  (APIENTRY *p_glGetUniformLocation)(GLuint, const GLchar_ *);
static void   (APIENTRY *p_glUniform1i)(GLint, GLint);
static void   (APIENTRY *p_glUniform2f)(GLint, GLfloat, GLfloat);
static void   (APIENTRY *p_glActiveTexture)(GLenum);

static GLuint s_vao;
static void   (APIENTRY *p_glGenVertexArrays)(GLsizei, GLuint *);
static void   (APIENTRY *p_glBindVertexArray)(GLuint);
static void   (APIENTRY *p_glGenBuffers)(GLsizei, GLuint *);
static void   (APIENTRY *p_glBindBuffer)(GLenum, GLuint);
static void   (APIENTRY *p_glBufferData)(GLenum, GLsizeiptr_, const void *, GLenum);
static void   (APIENTRY *p_glEnableVertexAttribArray)(GLuint);
static void   (APIENTRY *p_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
static void   (APIENTRY *p_glBindFragDataLocation)(GLuint, GLuint, const GLchar_ *);

#define LOAD(sym) do {                                                    \
        *(void **)&p_##sym = SDL_GL_GetProcAddress(#sym);                 \
        if (!p_##sym) {                                                   \
            printf("[displaygl] missing GL entry point: %s\n", #sym);     \
            return -1;                                                    \
        }                                                                 \
    } while (0)

static int gl_load_entry_points(void) {
    LOAD(glCreateShader);      LOAD(glShaderSource);   LOAD(glCompileShader);
    LOAD(glGetShaderiv);       LOAD(glGetShaderInfoLog);
    LOAD(glCreateProgram);     LOAD(glAttachShader);   LOAD(glLinkProgram);
    LOAD(glGetProgramiv);      LOAD(glGetProgramInfoLog);
    LOAD(glDeleteShader);      LOAD(glDeleteProgram);  LOAD(glUseProgram);
    LOAD(glGetAttribLocation); LOAD(glGetUniformLocation);
    LOAD(glUniform1i);         LOAD(glUniform2f);      LOAD(glActiveTexture);
    LOAD(glGenVertexArrays);   LOAD(glBindVertexArray);
    LOAD(glGenBuffers);        LOAD(glBindBuffer);     LOAD(glBufferData);
    LOAD(glEnableVertexAttribArray); LOAD(glVertexAttribPointer);
    LOAD(glBindFragDataLocation);
    return 0;
}
#undef LOAD

static int          s_requested;
static int          s_active;
static SDL_GLContext s_ctx;
static SDL_Window  *s_window;

static GLuint s_program;
static GLuint s_tex, s_prev_tex;

static GLuint s_ov_tex, s_ov_program;
static GLint  s_ov_u_image, s_ov_u_res, s_ov_u_origin, s_ov_u_blend, s_ov_u_prev;
static int    s_ov_w, s_ov_h, s_ov_have;
static int    s_game_rect[4];
static GLint  s_u_image, s_u_prev, s_u_blend, s_u_res, s_u_origin;
static char   s_filter[64] = "NearestNeighbor";
static display_gl_blend_t s_blend = DISPLAY_GL_BLEND_DISABLED;
static int    s_blend_odd;
static int    s_source_blend_odd;
static int    s_source_frame_advanced = 1;
static display_gl_scale_t s_scale = DISPLAY_GL_SCALE_INTEGER;

static int s_par_num = 1, s_par_den = 1;
static display_gl_vsync_t s_vsync = DISPLAY_GL_VSYNC_OFF;
uint64_t g_dbg_swap_ticks;

void DisplayGL_SetSourceFrameAdvanced(int advanced) {
    s_source_frame_advanced = advanced != 0;
    if (s_source_frame_advanced) s_source_blend_odd = s_blend_odd;
}

static uint32_t s_up[SCREEN_WIDTH_PX * SCREEN_HEIGHT_PX];
static uint32_t s_up_prev[SCREEN_WIDTH_PX * SCREEN_HEIGHT_PX];

static int s_up_w = SCREEN_WIDTH_PX, s_up_h = SCREEN_HEIGHT_PX;
static uint32_t *s_up_big, *s_up_big_prev;

static void to_gl_rgba_n(uint32_t *dst, const uint32_t *src, size_t n) {

    int tr, tg, tb;
    int tint = Display_GetTint(&tr, &tg, &tb);
    for (size_t i = 0; i < n; i++) {
        uint32_t p = src[i];
        uint8_t r = (uint8_t)(p >> 24), g = (uint8_t)(p >> 16), b = (uint8_t)(p >> 8);
        if (tint) {
            int rr = r * tr >> 8, gg = g * tg >> 8, bb = b * tb >> 8;
            r = (uint8_t)(rr > 255 ? 255 : rr);
            g = (uint8_t)(gg > 255 ? 255 : gg);
            b = (uint8_t)(bb > 255 ? 255 : bb);
        }

        dst[i] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | 0xFF000000u;
    }
}

static void to_gl_rgba_keep_alpha(uint32_t *dst, const uint32_t *src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        uint32_t p = src[i];
        uint8_t r = (uint8_t)(p >> 24), g = (uint8_t)(p >> 16),
                b = (uint8_t)(p >> 8),  a = (uint8_t)p;
        dst[i] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16)
               | ((uint32_t)a << 24);
    }
}

static const char *k_vertex_shader =
    "#version 150\n"
    "in vec4 aPosition;\n"
    "void main(void) { gl_Position = aPosition; }\n";

static char s_shader_dir[512];

static int try_shader_dir(const char *dir) {
    char probe[600];
    FILE *f;
    snprintf(probe, sizeof probe, "%sMasterShader.fsh", dir);
    f = fopen(probe, "rb");
    if (!f) return 0;
    fclose(f);
    snprintf(s_shader_dir, sizeof s_shader_dir, "%s", dir);
    return 1;
}

static const char *shader_dir(void) {
    char cand[600];
    char *base;
    if (s_shader_dir[0]) return s_shader_dir;

    base = SDL_GetBasePath();
    if (base) {
        snprintf(cand, sizeof cand, "%sshaders/", base);
        if (try_shader_dir(cand)) { SDL_free(base); return s_shader_dir; }
        snprintf(cand, sizeof cand, "%s../shaders/", base);
        if (try_shader_dir(cand)) { SDL_free(base); return s_shader_dir; }
        SDL_free(base);
    }
    if (try_shader_dir("shaders/"))    return s_shader_dir;
    if (try_shader_dir("../shaders/")) return s_shader_dir;

    printf("[displaygl] no shaders/ directory found (looked next to the exe, "
           "one level up, and in the working directory)\n");
    return NULL;
}

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    long n;
    char *buf;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    buf[n] = '\0';
    if (out_len) *out_len = (size_t)n;
    return buf;
}

static GLuint compile(const char *src, GLenum type) {
    GLuint sh = p_glCreateShader(type);
    GLint status = 0;
    p_glShaderSource(sh, 1, &src, NULL);
    p_glCompileShader(sh);
    p_glGetShaderiv(sh, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[1024] = {0};
        p_glGetShaderInfoLog(sh, sizeof log, NULL, log);
        printf("[displaygl] shader compile failed: %s\n", log);
        p_glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint build_program(const char *filter_name) {
    char path[512];
    size_t master_len = 0;
    const char *dir = shader_dir();
    char *master, *filter, *token, *combined;
    GLuint vs, fs, prog = 0;
    GLint status = 0;
    size_t head;

    if (!dir) return 0;
    snprintf(path, sizeof path, "%sMasterShader.fsh", dir);
    master = read_file(path, &master_len);
    if (!master) {
        printf("[displaygl] cannot read %s\n", path);
        return 0;
    }
    token = strstr(master, "{filter}");
    if (!token) {
        printf("[displaygl] MasterShader.fsh has no {filter} token\n");
        free(master);
        return 0;
    }
    snprintf(path, sizeof path, "%s%s.fsh", dir, filter_name);
    filter = read_file(path, NULL);
    if (!filter) {
        printf("[displaygl] filter not found: %s\n", path);
        free(master);
        return 0;
    }
    head = (size_t)(token - master);
    combined = (char *)malloc(master_len + strlen(filter) + 1);
    if (!combined) { free(master); free(filter); return 0; }
    memcpy(combined, master, head);
    combined[head] = '\0';
    strcat(combined, filter);
    strcat(combined, token + strlen("{filter}"));
    free(filter);
    free(master);

    vs = compile(k_vertex_shader, GL_VERTEX_SHADER);
    fs = compile(combined, GL_FRAGMENT_SHADER);
    free(combined);
    if (!vs || !fs) {
        if (vs) p_glDeleteShader(vs);
        if (fs) p_glDeleteShader(fs);
        return 0;
    }
    prog = p_glCreateProgram();
    p_glAttachShader(prog, vs);
    p_glAttachShader(prog, fs);
    p_glLinkProgram(prog);
    p_glGetProgramiv(prog, GL_LINK_STATUS, &status);
    p_glDeleteShader(vs);
    p_glDeleteShader(fs);
    if (!status) {
        char log[1024] = {0};
        p_glGetProgramInfoLog(prog, sizeof log, NULL, log);
        printf("[displaygl] program link failed: %s\n", log);
        p_glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static void bind_uniforms(void) {
    s_u_image  = p_glGetUniformLocation(s_program, "image");
    s_u_prev   = p_glGetUniformLocation(s_program, "previous_image");
    s_u_blend  = p_glGetUniformLocation(s_program, "frame_blending_mode");
    s_u_res    = p_glGetUniformLocation(s_program, "output_resolution");
    s_u_origin = p_glGetUniformLocation(s_program, "origin");
}

static void make_texture(GLuint *t) {
    glGenTextures(1, t);
    glBindTexture(GL_TEXTURE_2D, *t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void DisplayGL_SetRequested(int on) { s_requested = on ? 1 : 0; }
int  DisplayGL_IsRequested(void)    { return s_requested; }
int  DisplayGL_IsActive(void)       { return s_active; }
const char *DisplayGL_Filter(void)  { return s_filter; }

static void apply_vsync(void) {
    if (!s_active) return;
    if (s_vsync == DISPLAY_GL_VSYNC_ADAPTIVE) {
        if (SDL_GL_SetSwapInterval(-1) == 0) return;
        SDL_GL_SetSwapInterval(1);
        return;
    }
    SDL_GL_SetSwapInterval(s_vsync == DISPLAY_GL_VSYNC_ON ? 1 : 0);
}

static int gl_upload_source(const uint32_t *px, int w, int h) {
    uint32_t *dst;

    if (w <= 0 || h <= 0) return 0;
    if (w != s_up_w || h != s_up_h) {
        uint32_t *nb  = malloc((size_t)w * h * 4);
        uint32_t *nbp = malloc((size_t)w * h * 4);
        if (!nb || !nbp) { free(nb); free(nbp); return 0; }
        free(s_up_big);      s_up_big      = nb;
        free(s_up_big_prev); s_up_big_prev = nbp;
        s_up_w = w; s_up_h = h;
        p_glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);
    }
    dst = (w == SCREEN_WIDTH_PX && h == SCREEN_HEIGHT_PX) ? s_up : s_up_big;
    if (!dst) return 0;
    to_gl_rgba_n(dst, px, (size_t)w * h);
    p_glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, dst);
    return 1;
}

static void draw_overlay_quad(int w, int h);

int DisplayGL_PresentCRT(const uint32_t *pixels, const crt_frame_desc_t *frame) {
    int w = 0, h = 0;
    crt_rect_i_t vp;
    crt_params_t params;
    int drew;

    if (!s_active || !pixels || !frame) return 0;
    if (!CrtRenderer_GetParams(&params)) return 0;
    if (params.profile == CRT_PROFILE_OFF) return 0;
    if (!CrtRenderer_ValidateFrame(frame, NULL)) return 0;

    SDL_GL_GetDrawableSize(s_window, &w, &h);

    if (w <= 0 || h <= 0) return 0;

    CrtRenderer_ComputeViewport(frame, params.aspect_policy, w, h, &vp);

    if (s_game_rect[2] > 0 && s_game_rect[3] > 0) {
        crt_frame_desc_t sub = *frame;
        crt_rect_i_t svp;
        CrtRenderer_ComputeViewport(&sub, params.aspect_policy,
                                    s_game_rect[2], s_game_rect[3], &svp);
        vp.x = s_game_rect[0] + svp.x;
        vp.y = s_game_rect[1] + svp.y;
        vp.w = svp.w;
        vp.h = svp.h;
    }
    if (vp.w <= 0 || vp.h <= 0) return 0;

    if (!gl_upload_source(pixels, frame->texture_width, frame->texture_height))
        return 0;

    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    drew = CrtRendererGL_Draw(s_tex, frame, vp.x, vp.y, vp.w, vp.h,
                              (double)SDL_GetTicks() / 1000.0);
    if (drew != 1) {

        gl_BindVertexArray(0);
        gl_UseProgram(0);
        glViewport(0, 0, w, h);
        return 0;
    }

    draw_overlay_quad(w, h);

    DisplayGL_SwapOnly();
    return 1;
}

const char *DisplayGL_ShaderDir(void) { return shader_dir(); }

void DisplayGL_SwapOnly(void) {
    if (!s_active) return;
    {
        uint64_t sw0 = SDL_GetPerformanceCounter();
        SDL_GL_SwapWindow(s_window);
        g_dbg_swap_ticks += SDL_GetPerformanceCounter() - sw0;
    }
}

void DisplayGL_SetVSync(display_gl_vsync_t mode) { s_vsync = mode; apply_vsync(); }
display_gl_vsync_t DisplayGL_VSync(void) { return s_vsync; }

void DisplayGL_SetScaling(display_gl_scale_t mode) { s_scale = mode; }

void DisplayGL_SetPixelAspect(int num, int den) {
    if (num > 0 && den > 0) { s_par_num = num; s_par_den = den; }
    else                    { s_par_num = 1;   s_par_den = 1;   }
}
display_gl_scale_t DisplayGL_Scaling(void) { return s_scale; }

void DisplayGL_SetBlending(display_gl_blend_t mode) { s_blend = mode; }
display_gl_blend_t DisplayGL_Blending(void) { return s_blend; }

uint32_t DisplayGL_WantedWindowFlags(void) {
    return s_requested ? SDL_WINDOW_OPENGL : 0u;
}

int DisplayGL_Init(SDL_Window *window) {
    GLint major = 0, minor = 0;
    GLuint vbo = 0;
    GLint pos_attr;
    static const GLfloat quad[16] = {
        -1.f, -1.f, 0, 1,
        -1.f, +1.f, 0, 1,
        +1.f, -1.f, 0, 1,
        +1.f, +1.f, 0, 1,
    };

    if (!s_requested || !window) return -1;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    s_ctx = SDL_GL_CreateContext(window);
    if (!s_ctx) {
        printf("[displaygl] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return -1;
    }
    s_window = window;

    SDL_GL_SetSwapInterval(0);

    if (gl_load_entry_points() != 0) goto fail;

    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major * 0x100 + minor < 0x302) {
        printf("[displaygl] GL %d.%d is below the 3.2 the shaders need\n",
               (int)major, (int)minor);
        goto fail;
    }

    s_program = build_program(s_filter);
    if (!s_program) goto fail;
    bind_uniforms();
    p_glUseProgram(s_program);

    make_texture(&s_tex);
    make_texture(&s_prev_tex);

    p_glGenVertexArrays(1, &s_vao);
    p_glBindVertexArray(s_vao);
    p_glGenBuffers(1, &vbo);
    p_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    p_glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    pos_attr = p_glGetAttribLocation(s_program, "aPosition");
    if (pos_attr >= 0) {
        p_glEnableVertexAttribArray((GLuint)pos_attr);
        p_glVertexAttribPointer((GLuint)pos_attr, 4, GL_FLOAT, GL_FALSE, 0, 0);
    }

    s_active = 1;
    apply_vsync();
    printf("[displaygl] active: GL %d.%d, filter '%s'\n",
           (int)major, (int)minor, s_filter);
    fflush(stdout);
    return 0;

fail:
    SDL_GL_DeleteContext(s_ctx);
    s_ctx = NULL;
    s_window = NULL;
    return -1;
}

void DisplayGL_Shutdown(void) {
    if (!s_active) return;

    CrtRendererGL_Shutdown();

    if (s_program) p_glDeleteProgram(s_program);
    if (s_ctx) SDL_GL_DeleteContext(s_ctx);

    s_program = 0;
    s_vao = 0;
    s_tex = s_prev_tex = 0;
    s_ov_tex = s_ov_program = 0;

    s_up_w = SCREEN_WIDTH_PX;
    s_up_h = SCREEN_HEIGHT_PX;
    s_ov_w = s_ov_h = s_ov_have = 0;
    s_game_rect[0] = s_game_rect[1] = s_game_rect[2] = s_game_rect[3] = 0;
    s_ctx = NULL;
    s_window = NULL;
    s_active = 0;
}

int DisplayGL_SetFilter(const char *name) {
    GLuint prog;
    if (!name || !*name) return 0;
    if (!s_active) {
        snprintf(s_filter, sizeof s_filter, "%s", name);
        return 1;
    }
    prog = build_program(name);
    if (!prog) {

        printf("[displaygl] keeping filter '%s'\n", s_filter);
        return 0;
    }
    p_glDeleteProgram(s_program);
    s_program = prog;
    bind_uniforms();
    p_glUseProgram(s_program);
    snprintf(s_filter, sizeof s_filter, "%s", name);
    return 1;
}

static void gl_present_impl(const uint32_t *fb, const uint32_t *prev,
                           int img_w, int img_h);

void DisplayGL_SetGameRect(int x, int y, int w, int h) {
    s_game_rect[0] = x; s_game_rect[1] = y;
    s_game_rect[2] = w; s_game_rect[3] = h;
}

void DisplayGL_SetOverlay(const uint32_t *px, int w, int h) {
    if (!px || w <= 0 || h <= 0) { s_ov_have = 0; return; }
    if (!s_ov_tex) make_texture(&s_ov_tex);
    if (!s_ov_tex) { s_ov_have = 0; return; }
    p_glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_ov_tex);
    if (w != s_ov_w || h != s_ov_h) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        s_ov_w = w; s_ov_h = h;
    }
    {

        static uint32_t *conv; static size_t conv_n;
        size_t n = (size_t)w * h;
        if (conv_n < n) { free(conv); conv = malloc(n * 4); conv_n = conv ? n : 0; }
        if (!conv) { s_ov_have = 0; return; }
        to_gl_rgba_keep_alpha(conv, px, n);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                        GL_RGBA, GL_UNSIGNED_BYTE, conv);
    }
    s_ov_have = 1;
}

static void draw_overlay_quad(int w, int h) {
    if (!s_ov_have || !s_ov_tex) return;
    if (!s_ov_program) {
        s_ov_program = build_program("NearestNeighbor");
        if (!s_ov_program) { s_ov_have = 0; return; }
        s_ov_u_image  = p_glGetUniformLocation(s_ov_program, "image");
        s_ov_u_prev   = p_glGetUniformLocation(s_ov_program, "previous_image");
        s_ov_u_blend  = p_glGetUniformLocation(s_ov_program, "frame_blending_mode");
        s_ov_u_res    = p_glGetUniformLocation(s_ov_program, "output_resolution");
        s_ov_u_origin = p_glGetUniformLocation(s_ov_program, "origin");
    }

    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    p_glUseProgram(s_ov_program);
    p_glUniform2f(s_ov_u_origin, 0.0f, 0.0f);
    p_glUniform2f(s_ov_u_res, (GLfloat)w, (GLfloat)h);
    p_glUniform1i(s_ov_u_blend, 0);
    p_glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_ov_tex);
    p_glUniform1i(s_ov_u_image, 0);

    p_glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_ov_tex);
    p_glUniform1i(s_ov_u_prev, 1);
    if (p_glBindVertexArray) p_glBindVertexArray(s_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisable(GL_BLEND);
}

void DisplayGL_PresentSized(const uint32_t *fb, const uint32_t *prev,
                            int w, int h) {
    if (w != s_up_w || h != s_up_h) {
        uint32_t *nb  = malloc((size_t)w * h * 4);
        uint32_t *nbp = malloc((size_t)w * h * 4);

        if (!nb || !nbp) { free(nb); free(nbp); return; }
        free(s_up_big);      s_up_big      = nb;
        free(s_up_big_prev); s_up_big_prev = nbp;
        s_up_w = w; s_up_h = h;

        p_glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);
        p_glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, s_prev_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);
        p_glActiveTexture(GL_TEXTURE0);
    }
    gl_present_impl(fb, prev, w, h);
}

void DisplayGL_Present(const uint32_t *fb, const uint32_t *prev) {

    DisplayGL_PresentSized(fb, prev, Display_FrameWidth(), SCREEN_HEIGHT_PX);
}

static void gl_present_impl(const uint32_t *fb, const uint32_t *prev,
                            int img_w, int img_h) {
    int w = 0, h = 0;
    int blend;

    if (!s_active || !fb) return;

    SDL_GL_GetDrawableSize(s_window, &w, &h);

    if (gl_BindFramebuffer) gl_BindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    {

        double disp_w = (double)img_w;
        if (s_scale != DISPLAY_GL_SCALE_INTEGER && s_par_num != s_par_den)
            disp_w = (double)img_w * (double)s_par_num / (double)s_par_den;

        double xf = (double)w / disp_w;
        double yf = (double)h / (double)img_h;
        int nw, nh, ox, oy;

        int clip_x, clip_y, clip_w, clip_h;
        if (s_scale == DISPLAY_GL_SCALE_INTEGER) {
            xf = (double)(unsigned)xf;
            yf = (double)(unsigned)yf;
            if (xf < 1) xf = 1;
            if (yf < 1) yf = 1;
        }
        if (s_scale != DISPLAY_GL_SCALE_STRETCH) {
            if (xf > yf) xf = yf; else yf = xf;
        }
        nw = (int)(xf * disp_w);
        nh = (int)(yf * img_h);
        ox = (w - nw) / 2;
        oy = (h - nh) / 2;
        clip_x = ox; clip_y = oy; clip_w = nw; clip_h = nh;

        if (s_game_rect[2] > 0 && s_game_rect[3] > 0) {

            double sw = (double)s_game_rect[2];
            double sh = (double)s_game_rect[3];
            double sxf = sw / disp_w;
            double syf = sh / (double)img_h;
            if (sxf > syf) sxf = syf; else syf = sxf;
            nw = (int)(sxf * disp_w);
            nh = (int)(syf * img_h);
            if (nw < 1) nw = 1;
            if (nh < 1) nh = 1;
            ox = s_game_rect[0] + (s_game_rect[2] - nw) / 2;
            oy = s_game_rect[1] + (s_game_rect[3] - nh) / 2;
            clip_x = s_game_rect[0]; clip_y = s_game_rect[1];
            clip_w = s_game_rect[2]; clip_h = s_game_rect[3];
        }

        p_glUseProgram(s_program);
        p_glUniform2f(s_u_origin, (GLfloat)ox, (GLfloat)oy);
        p_glUniform2f(s_u_res, (GLfloat)nw, (GLfloat)nh);

        glEnable(GL_SCISSOR_TEST);
        glScissor(clip_x, clip_y, clip_w, clip_h);
    }

    {
        uint32_t *dst = (img_w == SCREEN_WIDTH_PX && img_h == SCREEN_HEIGHT_PX)
                        ? s_up : s_up_big;
        if (!dst) return;
        to_gl_rgba_n(dst, fb, (size_t)img_w * img_h);
        p_glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img_w, img_h,
                        GL_RGBA, GL_UNSIGNED_BYTE, dst);
    }
    p_glUniform1i(s_u_image, 0);

    blend = (int)s_blend;
    if (s_blend == DISPLAY_GL_BLEND_ACCURATE) {
        blend = s_source_blend_odd ? (int)DISPLAY_GL_BLEND_ACCURATE_ODD
                                   : (int)DISPLAY_GL_BLEND_ACCURATE_EVEN;
    }
    if (!prev) blend = (int)DISPLAY_GL_BLEND_DISABLED;
    p_glUniform1i(s_u_blend, blend);

    if (prev) {
        uint32_t *dstp = (img_w == SCREEN_WIDTH_PX && img_h == SCREEN_HEIGHT_PX)
                         ? s_up_prev : s_up_big_prev;
        if (dstp) {
            to_gl_rgba_n(dstp, prev, (size_t)img_w * img_h);
            p_glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, s_prev_tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img_w, img_h,
                            GL_RGBA, GL_UNSIGNED_BYTE, dstp);
        }
        p_glUniform1i(s_u_prev, 1);
    }

    p_glBindFragDataLocation(s_program, 0, "frag_color");

    if (p_glBindVertexArray) p_glBindVertexArray(s_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisable(GL_SCISSOR_TEST);

    draw_overlay_quad(w, h);

    {
        uint64_t sw0 = SDL_GetPerformanceCounter();
        SDL_GL_SwapWindow(s_window);
        g_dbg_swap_ticks += SDL_GetPerformanceCounter() - sw0;
    }
    if (s_source_frame_advanced) s_blend_odd = !s_blend_odd;
}
