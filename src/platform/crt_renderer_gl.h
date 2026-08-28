/* SPDX-License-Identifier: MIT
 *
 * crt_renderer_gl.h, the GL-private half of the CRT renderer.
 *
 * Spec section 10 allows this interface to contain GL types and restricts it to
 * display_gl.c. It is separate from crt_renderer.[ch] for a second reason too:
 * that file stays pure CPU maths so tests/crt_renderer_tests.c can link it
 * alone, with no GL, no SDL and no window.
 *
 * This module never creates a context and never swaps (spec 9).
 */

#pragma once

#include "crt_renderer.h"

void CrtRendererGL_TestForceFullPath(int on);
void CrtRendererGL_TestForceLimitedPrecision(int on);

int  CrtRendererGL_Init(void);
void CrtRendererGL_Shutdown(void);

int  CrtRendererGL_Draw(unsigned source_texture,
                        const crt_frame_desc_t *frame,
                        int viewport_x, int viewport_y,
                        int viewport_w, int viewport_h,
                        double now_seconds);
