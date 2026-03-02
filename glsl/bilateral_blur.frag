#version 460 core

in vec2 v_uv;
layout(location = 0) out float out_color;

layout(binding = 0) uniform sampler2D u_ssao_input;
layout(binding = 1) uniform sampler2D u_depth;
layout(binding = 2) uniform sampler2D u_normal;

uniform float u_z_near;
uniform float u_z_far;

float LinearizeDepth(float depth) {
  float z = depth * 2.0 - 1.0;
  return (2.0 * u_z_near * u_z_far) /
         (u_z_far + u_z_near - z * (u_z_far - u_z_near));
}

void main() {
  vec2 texel_size = 1.0 / vec2(textureSize(u_ssao_input, 0));

  float center_depth_nl = texture(u_depth, v_uv).r;
  float center_depth = LinearizeDepth(center_depth_nl);
  vec3 center_normal = texture(u_normal, v_uv).xyz * 2.0 - 1.0;

  float result = 0.0;
  float total_weight = 0.0;

  const int blur_radius = 4;

  for (int i = -blur_radius; i <= blur_radius; ++i) {
    vec2 offset = vec2(0.0);
#ifdef HORIZONTAL
    offset = vec2(float(i) * texel_size.x, 0.0);
#else
    offset = vec2(0.0, float(i) * texel_size.y);
#endif

    vec2 sample_uv = v_uv + offset;
    float sample_depth_nl = texture(u_depth, sample_uv).r;
    float sample_depth = LinearizeDepth(sample_depth_nl);
    vec3 sample_normal = texture(u_normal, sample_uv).xyz * 2.0 - 1.0;
    float sample_ssao = texture(u_ssao_input, sample_uv).r;

    // Calculate weights
    float spatial_weight = exp(-(float(i * i)) / (2.0 * 4.0));
    float depth_diff = abs(center_depth - sample_depth);
    float depth_weight = exp(-(depth_diff * depth_diff) / 0.1);
    float normal_diff = max(0.0, 1.0 - dot(center_normal, sample_normal));
    float normal_weight = exp(-(normal_diff * normal_diff) / 0.05);

    float weight = spatial_weight * depth_weight * normal_weight;

    result += sample_ssao * weight;
    total_weight += weight;
  }

  out_color = result / max(total_weight, 0.0001);
}
