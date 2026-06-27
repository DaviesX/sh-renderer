#version 460 core

#ifdef CUTOUT
// The composite coverage (q3Composite(...).a) must match the radiance pass so
// the pre-pass mask agrees with the shaded result. q3_composite.glsl pulls in
// u_albedo_texture (binding 0), the descriptor SSBOs (3/4/5), u_layers
// (binding 16+), u_time, u_material_index and q3Composite().
layout(binding = 0) uniform sampler2D u_albedo_texture;
in vec2 v_uv;
#include "q3_composite.glsl"
#endif

void main() {
#ifdef CUTOUT
  if (q3Composite(u_material_index, v_uv, u_time).a < 0.5) {
    discard;
  }
#endif
}
