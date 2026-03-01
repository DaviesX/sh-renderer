#version 460 core

in vec3 v_view_normal;

layout(location = 0) out vec4 out_normal;

void main() {
  vec3 n = normalize(v_view_normal);
  out_normal = vec4(n * 0.5 + 0.5, 1.0);
}
