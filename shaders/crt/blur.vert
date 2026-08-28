// SPDX-License-Identifier: MIT
// OldAmber CRT renderer -- separable blur, vertex stage. Original OldAmber code.
#version 150

in vec4 a_position;

void main(void)
{
    gl_Position = a_position;
}
