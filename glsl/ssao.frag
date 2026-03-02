#version 460 core

in vec2 v_uv;

layout(location = 0) out float out_occlusion;

layout(binding = 0) uniform sampler2D u_depth;
layout(binding = 1) uniform sampler2D u_normal;
layout(binding = 2) uniform sampler2D u_noise;

uniform mat4 u_projection;
uniform mat4 u_inv_projection;

uniform vec3 u_samples[MAX_SAMPLES];
const int kKernelSize = MAX_SAMPLES;
const float kRadius = .5f;
const float kBias = 0.025;

uniform vec2 u_resolution;

void main() {
  float depth = texture(u_depth, v_uv).r;
  if (depth == 1.0) {
    // Skybox depth
    out_occlusion = 1.0;
    return;
  }

  vec4 normal_data = texture(u_normal, v_uv);
  if (normal_data.a < 0.5) {  // Skybox flag
    out_occlusion = 1.0;
    return;
  }

  // decode normal
  vec3 normal = normalize(normal_data.xyz * 2.0 - 1.0);

  // reconstruct view space position
  vec4 clip_pos = vec4(v_uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
  vec4 view_pos_h = u_inv_projection * clip_pos;
  vec3 view_pos = view_pos_h.xyz / view_pos_h.w;

  // Build TBN matrix
  vec2 noise_scale = u_resolution / 4.0;  // 4x4 noise texture
  vec3 random_vec = normalize(texture(u_noise, v_uv * noise_scale).xyz);

  vec3 tangent = normalize(random_vec - normal * dot(random_vec, normal));
  vec3 bitangent = cross(normal, tangent);
  mat3 tbn = mat3(tangent, bitangent, normal);

  float occlusion = 0.0;
  for (int i = 0; i < kKernelSize; ++i) {
    // get sample position
    vec3 sample_pos = tbn * u_samples[i];  // From tangent to view-space
    sample_pos = view_pos + sample_pos * kRadius;

    // project sample position (to sample texture)
    vec2 offset = vec2(
        u_projection[0][0] * sample_pos.x + u_projection[2][0] * sample_pos.z,
        u_projection[1][1] * sample_pos.y + u_projection[2][1] * sample_pos.z);
    offset /= -sample_pos.z;      // perspective divide
    offset = offset * 0.5 + 0.5;  // transform to range 0.0 - 1.0

    // get sample depth
    float sample_depth = texture(u_depth, offset).r;

    // Reconstruct Z for depth comparison
    float sample_ndc_z = sample_depth * 2.0 - 1.0;
    float actual_sample_z =
        -u_projection[3][2] / (sample_ndc_z + u_projection[2][2]);

    // range check & accumulate
    float depth_diff = abs(view_pos.z - actual_sample_z);
    float range_check = smoothstep(0.0, 1.0, 1.0 - (depth_diff / kRadius));
    occlusion +=
        (actual_sample_z >= sample_pos.z + kBias ? 1.0 : 0.0) * range_check;
  }

  occlusion = 1.0 - (occlusion / float(kKernelSize));
  out_occlusion = occlusion;
}
