#version 460 core

out vec2 v_uv;

void main() {
  float x = -1.0 + float((gl_VertexID & 1) << 2);
  float y = -1.0 + float((gl_VertexID & 2) << 1);
  v_uv.x = (x + 1.0) * 0.5;
  v_uv.y = (y + 1.0) * 0.5;

  // Set z to 1.0 so the depth is 1.0 after perspective division, placing it at
  // the far plane. Ensure glDepthFunc is GL_LEQUAL when drawing the skybox.
  gl_Position = vec4(x, y, 1.0, 1.0);
}
