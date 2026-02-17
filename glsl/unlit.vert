#version 460 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) uniform mat4 u_view_proj;
layout(location = 1) uniform mat4 u_model;

out vec2 v_uv;
out vec3 v_normal;

void main() {
  v_uv = in_uv;
  v_normal = mat3(u_model) * in_normal;  // Simplified normal transform
  gl_Position = u_view_proj * u_model * vec4(in_position, 1.0);
}
