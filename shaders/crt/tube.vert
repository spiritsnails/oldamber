// SPDX-License-Identifier: MIT
// OldAmber CRT renderer -- tube pass, vertex stage.
// Original OldAmber code; see THIRD_PARTY.md's design-provenance entry.
//
// Deliberately trivial: every coordinate decision this renderer makes belongs
// in the fragment stage, where gl_FragCoord gives exact drawable pixels. The
// mask must be phase-stable in DRAWABLE pixels (spec 5.2), and interpolating a
// varying across the quad would tie it to the quad instead.
#version 150

in vec4 a_position;

void main(void)
{
    gl_Position = a_position;
}
