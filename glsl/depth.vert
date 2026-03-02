#version 460 core

layout(location = 0) in vec3 in_position;
#ifdef CUTOUT
layout(location = 2) in vec2 in_uv;
out vec2 v_uv;
#endif

layout(location = 0) uniform mat4 u_view_proj;
layout(location = 1) uniform mat4 u_model;

void main() {
  gl_Position = u_view_proj * u_model * vec4(in_position, 1.0);
#ifdef CUTOUT
  v_uv = in_uv;
#endif
}
