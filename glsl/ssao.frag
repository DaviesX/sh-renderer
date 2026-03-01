#version 460 core

in vec2 v_uv;

layout(location = 0) out float out_occlusion;

layout(binding = 0) uniform sampler2D u_depth;
layout(binding = 1) uniform sampler2D u_normal;
layout(binding = 2) uniform sampler2D u_noise;

uniform mat4 u_projection;
uniform mat4 u_inv_projection;

#define MAX_SAMPLES 64
uniform vec3 u_samples[MAX_SAMPLES];
uniform int u_kernel_size = 64;
uniform float u_radius = 0.5;
uniform float u_bias = 0.025;

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
  for (int i = 0; i < u_kernel_size; ++i) {
    // get sample position
    vec3 sample_pos = tbn * u_samples[i];  // From tangent to view-space
    sample_pos = view_pos + sample_pos * u_radius;

    // project sample position (to sample texture)
    vec4 offset = vec4(sample_pos, 1.0);
    offset = u_projection * offset;       // from view to clip-space
    offset.xyz /= offset.w;               // perspective divide
    offset.xyz = offset.xyz * 0.5 + 0.5;  // transform to range 0.0 - 1.0

    // get sample depth
    float sample_depth = texture(u_depth, offset.xy).r;

    // Reconstruct Z for depth comparison
    vec4 sample_clip_pos =
        vec4(offset.xy * 2.0 - 1.0, sample_depth * 2.0 - 1.0, 1.0);
    vec4 sample_view_h = u_inv_projection * sample_clip_pos;
    float actual_sample_z = (sample_view_h.xyz / sample_view_h.w).z;

    // range check & accumulate
    float range_check =
        smoothstep(0.0, 1.0, u_radius / abs(view_pos.z - actual_sample_z));
    occlusion +=
        (actual_sample_z >= sample_pos.z + u_bias ? 1.0 : 0.0) * range_check;
  }

  occlusion = 1.0 - (occlusion / float(u_kernel_size));
  out_occlusion = occlusion;
}
