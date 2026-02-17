#version 460 core

in vec2 v_uv;
out vec4 out_color;

layout(binding = 0) uniform sampler2D u_depth;

uniform float u_z_near;
uniform float u_z_far;

float LinearizeDepth(float depth) {
  float z = depth * 2.0 - 1.0;  // Back to NDC
  return (2.0 * u_z_near * u_z_far) /
         (u_z_far + u_z_near - z * (u_z_far - u_z_near));
}

void main() {
  float depth = texture(u_depth, v_uv).r;
  float linear_depth = LinearizeDepth(depth);

  // Normalize for visualization: [near, far] -> [0, 1]
  // roughly: (linear_depth - near) / (far - near)
  float normalized_depth = (linear_depth - u_z_near) / (u_z_far - u_z_near);

  // Invert so close is white, far is black? Or close is black, far is white?
  // Let's do close = dark, far = light.
  out_color = vec4(vec3(normalized_depth), 1.0);
}
