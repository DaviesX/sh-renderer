#version 460 core

in vec2 v_uv;
layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform sampler2D u_ssao;

void main() {
  float ambient_occlusion = texture(u_ssao, v_uv).r;
  out_color = vec4(vec3(ambient_occlusion), 1.0);
}
