#version 460 core

#ifdef CUTOUT
layout(binding = 0) uniform sampler2D u_albedo_texture;
in vec2 v_uv;
#endif

in vec3 v_view_normal;

layout(location = 0) out vec4 out_normal;

void main() {
#ifdef CUTOUT
  vec4 albedo = texture(u_albedo_texture, v_uv);
  if (albedo.a < 0.5) {
    discard;
  }
#endif

  vec3 n = normalize(v_view_normal);
  out_normal = vec4(n * 0.5 + 0.5, 1.0);
}
