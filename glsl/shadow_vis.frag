#version 460 core

in vec2 v_uv;
out vec4 out_color;

// Shadow map depth textures have comparison mode disabled for visualization,
// so we sample them as a regular sampler2D.
layout(binding = 0) uniform sampler2D u_shadow_map;

void main() {
  float depth = texture(u_shadow_map, v_uv).r;
  // Depth is in [0, 1] from the orthographic projection. Visualize directly.
  out_color = vec4(vec3(depth), 1.0);
}
