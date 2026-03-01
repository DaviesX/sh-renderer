#version 460 core

in vec3 v_view_normal;
in vec2 v_uv;

layout(binding = 0) uniform sampler2D u_albedo_texture;

layout(location = 0) out vec4 out_normal;

void main() {
  vec4 albedo = texture(u_albedo_texture, v_uv);
  if (albedo.a < 0.5) {
    discard;
  }

  vec3 n = normalize(v_view_normal);
  out_normal = vec4(n * 0.5 + 0.5, 1.0);
}
