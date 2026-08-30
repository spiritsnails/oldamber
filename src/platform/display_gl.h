#pragma once

#include <stdint.h>
#include <SDL.h>
#include "crt_renderer.h"

typedef enum {
    DISPLAY_GL_BLEND_DISABLED      = 0,
    DISPLAY_GL_BLEND_SIMPLE        = 1,
    DISPLAY_GL_BLEND_ACCURATE      = 2,
    DISPLAY_GL_BLEND_ACCURATE_EVEN = 2,
    DISPLAY_GL_BLEND_ACCURATE_ODD  = 3,
} display_gl_blend_t;

typedef enum {
    DISPLAY_GL_SCALE_INTEGER = 0,
    DISPLAY_GL_SCALE_ASPECT,
    DISPLAY_GL_SCALE_STRETCH,
} display_gl_scale_t;

void DisplayGL_SetScaling(display_gl_scale_t mode);

void DisplayGL_SetPixelAspect(int num, int den);
display_gl_scale_t DisplayGL_Scaling(void);

uint32_t DisplayGL_WantedWindowFlags(void);

void DisplayGL_SetRequested(int on);
int  DisplayGL_IsRequested(void);

int  DisplayGL_Init(SDL_Window *window);
void DisplayGL_Shutdown(void);
int  DisplayGL_IsActive(void);

int  DisplayGL_SetFilter(const char *name);
const char *DisplayGL_Filter(void);

typedef enum {
    DISPLAY_GL_VSYNC_OFF      = 0,
    DISPLAY_GL_VSYNC_ON       = 1,
    DISPLAY_GL_VSYNC_ADAPTIVE = 2,
} display_gl_vsync_t;

void DisplayGL_SwapOnly(void);

const char *DisplayGL_ShaderDir(void);

int DisplayGL_PresentCRT(const uint32_t *pixels, const crt_frame_desc_t *frame);

void DisplayGL_SetVSync(display_gl_vsync_t mode);
display_gl_vsync_t DisplayGL_VSync(void);

void DisplayGL_SetBlending(display_gl_blend_t mode);
display_gl_blend_t DisplayGL_Blending(void);

void DisplayGL_SetSourceFrameAdvanced(int advanced);

void DisplayGL_Present(const uint32_t *fb, const uint32_t *prev);

void DisplayGL_PresentSized(const uint32_t *fb, const uint32_t *prev,
                            int w, int h);

void DisplayGL_SetGameRect(int x, int y, int w, int h);

void DisplayGL_SetOverlay(const uint32_t *px, int w, int h);
