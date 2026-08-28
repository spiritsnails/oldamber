/* SPDX-License-Identifier: MIT
 *
 * gl_api.c, see gl_api.h. Original OldAmber code.
 */

#include "gl_api.h"
#include <stdio.h>

void (APIENTRY *gl_GenFramebuffers)(GLsizei, GLuint *);
void (APIENTRY *gl_BindFramebuffer)(GLenum, GLuint);
void (APIENTRY *gl_FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
GLenum (APIENTRY *gl_CheckFramebufferStatus)(GLenum);
void (APIENTRY *gl_DeleteFramebuffers)(GLsizei, const GLuint *);

void (APIENTRY *gl_Uniform1f)(GLint, GLfloat);
void (APIENTRY *gl_Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
void (APIENTRY *gl_Uniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
void (APIENTRY *gl_Uniform1fv)(GLint, GLsizei, const GLfloat *);
void (APIENTRY *gl_Uniform2f)(GLint, GLfloat, GLfloat);
void (APIENTRY *gl_Uniform1i)(GLint, GLint);

GLuint (APIENTRY *gl_CreateShader)(GLenum);
void   (APIENTRY *gl_ShaderSource)(GLuint, GLsizei, const char *const *, const GLint *);
void   (APIENTRY *gl_CompileShader)(GLuint);
void   (APIENTRY *gl_GetShaderiv)(GLuint, GLenum, GLint *);
void   (APIENTRY *gl_GetShaderInfoLog)(GLuint, GLsizei, GLsizei *, char *);
GLuint (APIENTRY *gl_CreateProgram)(void);
void   (APIENTRY *gl_AttachShader)(GLuint, GLuint);
void   (APIENTRY *gl_LinkProgram)(GLuint);
void   (APIENTRY *gl_GetProgramiv)(GLuint, GLenum, GLint *);
void   (APIENTRY *gl_GetProgramInfoLog)(GLuint, GLsizei, GLsizei *, char *);
void   (APIENTRY *gl_DeleteShader)(GLuint);
void   (APIENTRY *gl_DeleteProgram)(GLuint);
void   (APIENTRY *gl_UseProgram)(GLuint);
GLint  (APIENTRY *gl_GetAttribLocation)(GLuint, const char *);
GLint  (APIENTRY *gl_GetUniformLocation)(GLuint, const char *);
void   (APIENTRY *gl_ActiveTexture)(GLenum);
void   (APIENTRY *gl_GenVertexArrays)(GLsizei, GLuint *);
void   (APIENTRY *gl_BindVertexArray)(GLuint);
void   (APIENTRY *gl_DeleteVertexArrays)(GLsizei, const GLuint *);
void   (APIENTRY *gl_GenBuffers)(GLsizei, GLuint *);
void   (APIENTRY *gl_BindBuffer)(GLenum, GLuint);
void   (APIENTRY *gl_BufferData)(GLenum, ptrdiff_t, const void *, GLenum);
void   (APIENTRY *gl_DeleteBuffers)(GLsizei, const GLuint *);
void   (APIENTRY *gl_EnableVertexAttribArray)(GLuint);
void   (APIENTRY *gl_VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
void   (APIENTRY *gl_BindFragDataLocation)(GLuint, GLuint, const char *);
void   (APIENTRY *gl_BindAttribLocation)(GLuint, GLuint, const char *);
void   (APIENTRY *gl_UniformMatrix3fv)(GLint, GLsizei, GLboolean, const GLfloat *);

static int s_loaded;

#define LOAD(ptr, name)                                                       \
    do {                                                                      \
        *(void **)&(ptr) = SDL_GL_GetProcAddress(name);                       \
        if (!(ptr)) {                                                         \
            printf("[glapi] missing GL entry point: %s\n", name);             \
            fflush(stdout);                                                   \
            return -1;                                                        \
        }                                                                     \
    } while (0)

int GlApi_Load(void) {
    if (s_loaded) return 0;
    if (!SDL_GL_GetCurrentContext()) {
        printf("[glapi] no current GL context\n");
        fflush(stdout);
        return -1;
    }

    LOAD(gl_GenFramebuffers,        "glGenFramebuffers");
    LOAD(gl_BindFramebuffer,        "glBindFramebuffer");
    LOAD(gl_FramebufferTexture2D,   "glFramebufferTexture2D");
    LOAD(gl_CheckFramebufferStatus, "glCheckFramebufferStatus");
    LOAD(gl_DeleteFramebuffers,     "glDeleteFramebuffers");

    LOAD(gl_Uniform1f,  "glUniform1f");
    LOAD(gl_Uniform3f,  "glUniform3f");
    LOAD(gl_Uniform4f,  "glUniform4f");
    LOAD(gl_Uniform1fv, "glUniform1fv");
    LOAD(gl_Uniform2f,  "glUniform2f");
    LOAD(gl_Uniform1i,  "glUniform1i");

    LOAD(gl_CreateShader,       "glCreateShader");
    LOAD(gl_ShaderSource,       "glShaderSource");
    LOAD(gl_CompileShader,      "glCompileShader");
    LOAD(gl_GetShaderiv,        "glGetShaderiv");
    LOAD(gl_GetShaderInfoLog,   "glGetShaderInfoLog");
    LOAD(gl_CreateProgram,      "glCreateProgram");
    LOAD(gl_AttachShader,       "glAttachShader");
    LOAD(gl_LinkProgram,        "glLinkProgram");
    LOAD(gl_GetProgramiv,       "glGetProgramiv");
    LOAD(gl_GetProgramInfoLog,  "glGetProgramInfoLog");
    LOAD(gl_DeleteShader,       "glDeleteShader");
    LOAD(gl_DeleteProgram,      "glDeleteProgram");
    LOAD(gl_UseProgram,         "glUseProgram");
    LOAD(gl_GetAttribLocation,  "glGetAttribLocation");
    LOAD(gl_GetUniformLocation, "glGetUniformLocation");
    LOAD(gl_ActiveTexture,      "glActiveTexture");
    LOAD(gl_GenVertexArrays,    "glGenVertexArrays");
    LOAD(gl_BindVertexArray,    "glBindVertexArray");
    LOAD(gl_DeleteVertexArrays, "glDeleteVertexArrays");
    LOAD(gl_GenBuffers,         "glGenBuffers");
    LOAD(gl_BindBuffer,         "glBindBuffer");
    LOAD(gl_BufferData,         "glBufferData");
    LOAD(gl_DeleteBuffers,      "glDeleteBuffers");
    LOAD(gl_EnableVertexAttribArray, "glEnableVertexAttribArray");
    LOAD(gl_VertexAttribPointer,     "glVertexAttribPointer");
    LOAD(gl_BindFragDataLocation,    "glBindFragDataLocation");
    LOAD(gl_BindAttribLocation,      "glBindAttribLocation");
    LOAD(gl_UniformMatrix3fv,        "glUniformMatrix3fv");

    s_loaded = 1;
    return 0;
}

#undef LOAD

int GlApi_IsLoaded(void) { return s_loaded; }
