#version 460 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec2 in_lightmap_uv;
layout(location = 4) in vec4 in_tangent;

out vec3 v_world_pos;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_tangent;

uniform mat4 u_model;
uniform mat4 u_view_proj;

void main() {
  vec4 world_pos = u_model * vec4(in_position, 1.0);
  v_world_pos = world_pos.xyz;
  v_normal = mat3(u_model) * in_normal;
  v_uv = in_uv;
  v_tangent = in_tangent;

  gl_Position = u_view_proj * world_pos;
}
