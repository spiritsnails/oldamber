/* SPDX-License-Identifier: MIT
 *
 * gl_api.h, checked OpenGL entry points shared by the presentation modules.
 *
 * Original OldAmber code. No third-party shader or renderer source was copied,
 * translated, or restructured to produce this file; it exists only to resolve
 * GL function pointers, which SDL_opengl.h on this toolchain declares only up
 * to GL 1.1 (see display_gl.c's own note on the same problem).
 *
 * Why a second loader exists. display_gl.c already loads the subset the SameBoy
 * filter path needs, as file-static pointers. The CRT renderer needs those plus
 * framebuffer-object entry points, and spec section 9 requires one shared,
 * checked surface rather than two private copies drifting apart.
 *
 * This module is that surface. display_gl.c keeps its existing statics for now
 *, Phase 0 must not change presentation behaviour, and migrating it here is
 * deliberate later work, not a Phase 0 refactor.
 *
 * A missing MANDATORY pointer disables the CRT renderer only. It must never
 * disable OpenGL as a whole: the SameBoy path has its own, already-working set.
 */

#pragma once

#include <SDL.h>
#include <SDL_opengl.h>

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER          0x8D40
#define GL_COLOR_ATTACHMENT0    0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_RGBA16F
#define GL_RGBA16F              0x881A
#endif
#ifndef GL_FRAMEBUFFER_SRGB
#define GL_FRAMEBUFFER_SRGB     0x8DB9
#endif
#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8         0x8C43
#endif
#ifndef GL_HALF_FLOAT
#define GL_HALF_FLOAT           0x140B
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE        0x812F
#endif

int GlApi_Load(void);

int GlApi_IsLoaded(void);

extern void (APIENTRY *gl_GenFramebuffers)(GLsizei, GLuint *);
extern void (APIENTRY *gl_BindFramebuffer)(GLenum, GLuint);
extern void (APIENTRY *gl_FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
extern GLenum (APIENTRY *gl_CheckFramebufferStatus)(GLenum);
extern void (APIENTRY *gl_DeleteFramebuffers)(GLsizei, const GLuint *);

extern void (APIENTRY *gl_Uniform1f)(GLint, GLfloat);
extern void (APIENTRY *gl_Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
extern void (APIENTRY *gl_Uniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
extern void (APIENTRY *gl_Uniform1fv)(GLint, GLsizei, const GLfloat *);
extern void (APIENTRY *gl_Uniform2f)(GLint, GLfloat, GLfloat);
extern void (APIENTRY *gl_Uniform1i)(GLint, GLint);

extern GLuint (APIENTRY *gl_CreateShader)(GLenum);
extern void   (APIENTRY *gl_ShaderSource)(GLuint, GLsizei, const char *const *, const GLint *);
extern void   (APIENTRY *gl_CompileShader)(GLuint);
extern void   (APIENTRY *gl_GetShaderiv)(GLuint, GLenum, GLint *);
extern void   (APIENTRY *gl_GetShaderInfoLog)(GLuint, GLsizei, GLsizei *, char *);
extern GLuint (APIENTRY *gl_CreateProgram)(void);
extern void   (APIENTRY *gl_AttachShader)(GLuint, GLuint);
extern void   (APIENTRY *gl_LinkProgram)(GLuint);
extern void   (APIENTRY *gl_GetProgramiv)(GLuint, GLenum, GLint *);
extern void   (APIENTRY *gl_GetProgramInfoLog)(GLuint, GLsizei, GLsizei *, char *);
extern void   (APIENTRY *gl_DeleteShader)(GLuint);
extern void   (APIENTRY *gl_DeleteProgram)(GLuint);
extern void   (APIENTRY *gl_UseProgram)(GLuint);
extern GLint  (APIENTRY *gl_GetAttribLocation)(GLuint, const char *);
extern GLint  (APIENTRY *gl_GetUniformLocation)(GLuint, const char *);
extern void   (APIENTRY *gl_ActiveTexture)(GLenum);
extern void   (APIENTRY *gl_GenVertexArrays)(GLsizei, GLuint *);
extern void   (APIENTRY *gl_BindVertexArray)(GLuint);
extern void   (APIENTRY *gl_DeleteVertexArrays)(GLsizei, const GLuint *);
extern void   (APIENTRY *gl_GenBuffers)(GLsizei, GLuint *);
extern void   (APIENTRY *gl_BindBuffer)(GLenum, GLuint);
extern void   (APIENTRY *gl_BufferData)(GLenum, ptrdiff_t, const void *, GLenum);
extern void   (APIENTRY *gl_DeleteBuffers)(GLsizei, const GLuint *);
extern void   (APIENTRY *gl_EnableVertexAttribArray)(GLuint);
extern void   (APIENTRY *gl_VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
extern void   (APIENTRY *gl_BindFragDataLocation)(GLuint, GLuint, const char *);

extern void   (APIENTRY *gl_BindAttribLocation)(GLuint, GLuint, const char *);
extern void   (APIENTRY *gl_UniformMatrix3fv)(GLint, GLsizei, GLboolean, const GLfloat *);
