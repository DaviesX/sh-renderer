#version 460 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

out vec3 v_view_normal;
out vec2 v_uv;

uniform mat4 u_view_proj;
uniform mat4 u_model;
uniform mat4 u_view;

void main() {
  mat3 normal_matrix = transpose(inverse(mat3(u_view * u_model)));
  v_view_normal = normalize(normal_matrix * in_normal);

  gl_Position = u_view_proj * u_model * vec4(in_position, 1.0);
  v_uv = in_uv;
}
