#version 460 core

#ifdef CUTOUT
layout(binding = 0) uniform sampler2D u_albedo_texture;
in vec2 v_uv;
#endif

void main() {
#ifdef CUTOUT
  float alpha = texture(u_albedo_texture, v_uv).a;
  if (alpha < 0.5) {
    discard;
  }
#endif
}
