#version 460 core

in vec2 v_uv;
in vec3 v_normal;

out vec4 out_color;

layout(binding = 0) uniform sampler2D u_albedo;

void main() {
  vec4 color = texture(u_albedo, v_uv);
  // Simple N dot L with "headlamp"
  vec3 N = normalize(v_normal);
  vec3 L = normalize(vec3(0.5, 1.0, 1.0));  // Fixed light for unlit viz
  float ndotl = max(dot(N, L), 0.2);
  out_color = vec4(color.rgb * ndotl, color.a);
}
