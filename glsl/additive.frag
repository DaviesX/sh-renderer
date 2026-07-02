#version 460 core

// Additive (order-independent) transparent pass for SH_material_layers surfaces
// whose every stage blends with dst factor GL_ONE (flames, glows, plasma). The
// composited colour is added to the already-lit HDR buffer via
// glBlendFunc(GL_ONE, GL_ONE), so black texels contribute nothing ("adding
// zero"), the surface neither writes depth nor needs an alpha test, and -- being
// additive -- the result is independent of draw order. q3Composite()'s coverage
// (alpha) is irrelevant here and ignored.
//
// These materials are loaded with base_layer = -1, so q3Composite samples every
// stage's own (animMap-selected) texture rather than the static modern albedo.

layout(location = 0) out vec4 out_color;

in vec2 v_uv;

// Declared for q3_composite.glsl; unused for additive materials (base_layer<0).
layout(binding = 0) uniform sampler2D u_albedo_texture;

#include "q3_composite.glsl"

void main() {
  vec4 c = q3Composite(u_material_index, v_uv, u_time);
  out_color = vec4(c.rgb, 1.0);
}
