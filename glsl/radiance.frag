#version 460 core

layout(location = 0) out vec4 out_color;

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in vec2 v_lightmap_uv;
in vec4 v_tangent;

// Camera
uniform vec3 u_camera_pos;
uniform mat4 u_view;  // Needed for cascade selection

// Sun Light
struct Light {
  vec3 direction;
  vec3 color;
  float intensity;
};
uniform Light u_sun;

// Shadows
#define NUM_CASCADES 3
layout(binding = 5) uniform sampler2DShadow u_sun_shadow_maps[NUM_CASCADES];
uniform float u_sun_cascade_splits[NUM_CASCADES];
uniform mat4 u_sun_cascade_view_projections[NUM_CASCADES];

// Material
layout(binding = 0) uniform sampler2D u_albedo_texture;
layout(binding = 1) uniform sampler2D u_normal_texture;
layout(binding = 2) uniform sampler2D
    u_metallic_roughness_texture;  // B = Metal, G = Rough
layout(binding = 3) uniform sampler2D u_emissive_texture;

// Lightmaps
layout(binding = 8) uniform sampler2D u_PackedTex0;
layout(binding = 9) uniform sampler2D u_PackedTex1;
layout(binding = 10) uniform sampler2D u_PackedTex2;

uniform vec3 u_emissive_factor;
uniform float u_emissive_strength;
uniform int u_has_emissive_texture;
uniform vec3 u_sky_color;

// Forward+ tile info.
uniform ivec2 u_tile_count;
uniform ivec2 u_screen_size;

// --- SSBOs for Forward+ ---
struct GpuPointLight {
  vec3 position;
  float radius;
  vec3 color;
  float intensity;
};

struct GpuSpotLight {
  vec3 position;
  float radius;
  vec3 direction;
  float intensity;
  vec3 color;
  float cos_inner_cone;
  float cos_outer_cone;
  float pad0[3];
};

layout(std430, binding = 0) readonly buffer PointLightBuffer {
  uint point_light_count;
  uint plpad[3];
  GpuPointLight gpu_point_lights[];
};

layout(std430, binding = 1) readonly buffer SpotLightBuffer {
  uint spot_light_count;
  uint slpad[3];
  GpuSpotLight gpu_spot_lights[];
};

layout(std430, binding = 2) readonly buffer TileLightIndexBuffer {
  uint tile_data[];  // Header: [offset, p_count, s_count] per tile, then light
                     // indices.
};

const float PI = 3.14159265359;

// --- PBR Functions ---

struct ShadingAngles {
  float n_dot_l;
  float n_dot_v;
  float n_dot_h;
  float l_dot_h;
  float v_dot_h;
};

struct LightmapTexel {
  vec3 sh_coeffs[9];
  float visibility;
};

ShadingAngles ComputeShadingAngles(vec3 N, vec3 V, vec3 L, vec3 H) {
  ShadingAngles angles;
  angles.n_dot_l = max(dot(N, L), 0.0);
  angles.n_dot_v = max(dot(N, V), 0.0);
  angles.n_dot_h = max(dot(N, H), 0.0);
  angles.l_dot_h = max(dot(L, H), 0.0);
  angles.v_dot_h = max(dot(V, H), 0.0);
  return angles;
}

vec3 FresnelSchlick(float v_dot_h, vec3 F0) {
  return F0 + (1.0 - F0) * pow(max(1.0 - v_dot_h, 0.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cos_theta, vec3 F0, float roughness) {
  return F0 + (max(vec3(1.0 - roughness), F0) - F0) *
                  pow(max(1.0 - cos_theta, 0.0), 5.0);
}

float DistributionGGX(ShadingAngles angles, float roughness) {
  float r = max(roughness, 0.05);
  float a = r * r;
  float a2 = a * a;
  float n_dot_h_sqr = angles.n_dot_h * angles.n_dot_h;

  float num = a2;
  float denom = (n_dot_h_sqr * (a2 - 1.0) + 1.0);
  denom = PI * denom * denom;

  return num / denom;
}

float GeometrySchlickGGX(float cos_theta, float roughness) {
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;

  float num = cos_theta;
  float denom = cos_theta * (1.0 - k) + k;

  return num / denom;
}

float GeometrySmith(ShadingAngles angles, float roughness) {
  float ggx2 = GeometrySchlickGGX(angles.n_dot_v, roughness);
  float ggx1 = GeometrySchlickGGX(angles.n_dot_l, roughness);
  return ggx1 * ggx2;
}

float DistributionDisney(ShadingAngles angles, float roughness) {
  float fd90 = 0.5 + 2.0 * angles.l_dot_h * angles.l_dot_h * roughness;

  // Schlick Fresnel approximation for both light and view angles
  float light_scatter =
      1.0 + (fd90 - 1.0) * pow(max(1.0 - angles.n_dot_l, 0.0), 5.0);
  float view_scatter =
      1.0 + (fd90 - 1.0) * pow(max(1.0 - angles.n_dot_v, 0.0), 5.0);

  return light_scatter * view_scatter;
}

vec3 ComputeDirectBRDF(ShadingAngles angles, vec3 f0, vec3 albedo,
                       float metallic, float roughness, float occlusion) {
  vec3 F = FresnelSchlick(angles.v_dot_h, f0);
  float NDF = DistributionGGX(angles, roughness);
  float G = GeometrySmith(angles, roughness);

  vec3 numerator = NDF * G * F;
  float denominator = 4.0 * angles.n_dot_v * angles.n_dot_l + 0.001;
  vec3 specular = numerator / denominator;

  vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
  float disney = DistributionDisney(angles, roughness);
  vec3 diffuse = kD * albedo / PI * disney;

  // TODO: Fix the ORM textures so that they support occlusion.
  // return occlusion * (diffuse + specular);
  return (diffuse + specular);
}

vec3 ComputeIndirectBRDF(ShadingAngles angles, vec3 irradiance,
                         vec3 reflection_radiance, vec3 f0, vec3 albedo,
                         float metallic, float roughness, float occlusion) {
  vec3 fresnel = FresnelSchlickRoughness(angles.n_dot_v, f0, roughness);

  vec3 specular = fresnel * reflection_radiance;

  vec3 kD = (vec3(1.0) - fresnel) * (1.0 - metallic);
  vec3 diffuse = kD * albedo / PI * irradiance;

  // TODO: Fix the ORM textures so that they support occlusion.
  // return occlusion * (diffuse + specular);
  return (diffuse + specular);
}

// --- Helper: Interleaved Gradient Noise ---
float InterleavedGradientNoise(vec2 position_screen) {
  vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
  return fract(magic.z * fract(dot(position_screen, magic.xy)));
}

// --- Helper: 9-Tap Poisson Disk PCF ---
float PCFPoissonDisk9(sampler2DShadow shadow_map, vec3 shadow_coord,
                      float texel_size, float penumbra) {
  const vec2 poisson_disk[9] =
      vec2[](vec2(-0.7674601, 0.5097495), vec2(-0.0652758, 0.9238806),
             vec2(0.6559792, 0.7077699), vec2(-0.9333916, -0.2797686),
             vec2(-0.3013854, -0.4005081), vec2(0.4485547, -0.1982736),
             vec2(0.9008985, 0.1802927), vec2(-0.5298812, -0.8258384),
             vec2(0.3533838, -0.8351508));

  float random_angle =
      InterleavedGradientNoise(gl_FragCoord.xy) * 6.28318530718;
  float s = sin(random_angle);
  float c = cos(random_angle);
  mat2 rotationMat = mat2(c, -s, s, c);

  float shadow = 0.0;
  for (int i = 0; i < 9; ++i) {
    vec2 offset = rotationMat * poisson_disk[i] * texel_size * penumbra;
    // Hardware PCF uses vec3(uv.x, uv.y, compare_depth) for sampler2DShadow
    shadow +=
        texture(shadow_map, vec3(shadow_coord.xy + offset, shadow_coord.z));
  }
  return shadow / 9.0;
}

// --- Shadow Calculation ---
float ComputeShadow(vec3 world_pos, vec3 normal, ShadingAngles angles) {
  vec4 viewPos = u_view * vec4(world_pos, 1.0);
  float viewDepth = abs(viewPos.z);

  int layer = 0;
  for (int i = 0; i < NUM_CASCADES - 1; ++i) {
    layer += int(viewDepth >= u_sun_cascade_splits[i]);
  }

  const float normal_bias_scale = 0.02;
  vec3 biased_world_pos =
      world_pos + normal * (1.0 - angles.n_dot_l) * normal_bias_scale;
  vec4 frag_pos_light_space =
      u_sun_cascade_view_projections[layer] * vec4(biased_world_pos, 1.0);
  vec3 proj_coords = (frag_pos_light_space.xyz) / frag_pos_light_space.w;
  proj_coords = proj_coords * 0.5 + 0.5;

  float texel_size = 1.0 / float(textureSize(u_sun_shadow_maps[layer], 0).x);
  vec3 compare_coord = vec3(proj_coords.xy, proj_coords.z);
  float shadow = PCFPoissonDisk9(u_sun_shadow_maps[layer], compare_coord,
                                 texel_size, 1.5 / float(layer + 1));

  return shadow;
}

// --- SH Irradiance ---
vec3 EvalSHIrradiance(vec3 normal, vec3 sh_coeffs[9]) {
  float x = normal.x;
  float y = normal.y;
  float z = normal.z;

  // Constants pre-multiplied by A_l convolution factors
  // Band 0: 0.282095 * 3.141593 = 0.886227
  float c1 = 0.886227;

  // Band 1: 0.488603 * 2.094395 = 1.023326
  float c2 = 1.023326;

  // Band 2: 1.092548 * 0.785398 = 0.858086
  // Band 2: 0.315392 * 0.785398 = 0.247708
  // Band 2: 0.546274 * 0.785398 = 0.429043
  float c3 = 0.858086;
  float c4 = 0.247708;
  float c5 = 0.429043;

  float b0 = c1;
  float b1 = c2 * y;
  float b2 = c2 * z;
  float b3 = c2 * x;
  float b4 = c3 * x * y;
  float b5 = c3 * y * z;
  float b6 = c4 * (3.0 * z * z - 1.0);
  float b7 = c3 * x * z;
  float b8 = c5 * (x * x - y * y);

  vec3 result = sh_coeffs[0] * b0 + sh_coeffs[1] * b1 + sh_coeffs[2] * b2 +
                sh_coeffs[3] * b3 + sh_coeffs[4] * b4 + sh_coeffs[5] * b5 +
                sh_coeffs[6] * b6 + sh_coeffs[7] * b7 + sh_coeffs[8] * b8;

  return max(result, 0.0);
}

// --- SH Radiance ---
vec3 EvalSHRadiance(vec3 normal, vec3 sh_coeffs[9]) {
  float x = normal.x;
  float y = normal.y;
  float z = normal.z;

  float c1 = 0.282095;
  float c2 = 0.488603;
  float c3 = 1.092548;
  float c4 = 0.315392;
  float c5 = 0.546274;

  float b0 = c1;
  float b1 = c2 * y;
  float b2 = c2 * z;
  float b3 = c2 * x;
  float b4 = c3 * x * y;
  float b5 = c3 * y * z;
  float b6 = c4 * (3.0 * z * z - 1.0);
  float b7 = c3 * x * z;
  float b8 = c5 * (x * x - y * y);

  vec3 result = sh_coeffs[0] * b0 + sh_coeffs[1] * b1 + sh_coeffs[2] * b2 +
                sh_coeffs[3] * b3 + sh_coeffs[4] * b4 + sh_coeffs[5] * b5 +
                sh_coeffs[6] * b6 + sh_coeffs[7] * b7 + sh_coeffs[8] * b8;

  return max(result, 0.0);
}

LightmapTexel GetLightmapTexel() {
  vec4 p0 = texture(u_PackedTex0, v_lightmap_uv);
  vec4 p1 = texture(u_PackedTex1, v_lightmap_uv);
  vec4 p2 = texture(u_PackedTex2, v_lightmap_uv);

  LightmapTexel texel;
  texel.sh_coeffs[0] = p0.rgb;
  texel.visibility = p0.a;

  // Chroma reconstruction for higher bands
  float L0_lum = dot(texel.sh_coeffs[0], vec3(0.2126, 0.7152, 0.0722));
  vec3 chroma = vec3(1.0);
  if (L0_lum > 1e-6) {
    chroma = texel.sh_coeffs[0] / L0_lum;
  }

  // File 1: L1m1, L10, L11, L2m2
  texel.sh_coeffs[1] = vec3(p1.r) * chroma;
  texel.sh_coeffs[2] = vec3(p1.g) * chroma;
  texel.sh_coeffs[3] = vec3(p1.b) * chroma;
  texel.sh_coeffs[4] = vec3(p1.a) * chroma;

  // File 2: L2m1, L20, L21, L22
  texel.sh_coeffs[5] = vec3(p2.r) * chroma;
  texel.sh_coeffs[6] = vec3(p2.g) * chroma;
  texel.sh_coeffs[7] = vec3(p2.b) * chroma;
  texel.sh_coeffs[8] = vec3(p2.a) * chroma;

  return texel;
}

// ---------------------

void main() {
  // 1. Material Properties
  vec4 albedo_sample = texture(u_albedo_texture, v_uv);
  vec3 albedo = albedo_sample.rgb;
  float alpha = albedo_sample.a;

  if (alpha < 0.5) {
    // Cutout transparency.
    discard;
  }

  vec4 mr_sample = texture(u_metallic_roughness_texture, v_uv);
  float metallic = mr_sample.b;
  float roughness = mr_sample.g;
  float occlusion = mr_sample.r;

  // Normal Mapping
  vec3 tangent_space_normal = texture(u_normal_texture, v_uv).rgb;
  tangent_space_normal = tangent_space_normal * 2.0 - 1.0;

  vec3 interpolated_normal = normalize(v_normal);
  vec3 tangent =
      normalize(v_tangent.xyz -
                interpolated_normal * dot(v_tangent.xyz, interpolated_normal));
  vec3 bitangent =
      cross(interpolated_normal, tangent) * (v_tangent.w > 0.0 ? 1.0 : -1.0);
  mat3 tangent_to_world_space = mat3(tangent, bitangent, interpolated_normal);

  vec3 normal_world = normalize(tangent_to_world_space * tangent_space_normal);

  // Lighting Vectors
  vec3 view_dir = normalize(u_camera_pos - v_world_pos);
  vec3 light_dir =
      normalize(-u_sun.direction);  // Direction from surface to light
  vec3 half_dir = normalize(view_dir + light_dir);
  vec3 reflection_dir = reflect(-view_dir, normal_world);

  ShadingAngles angles =
      ComputeShadingAngles(normal_world, view_dir, light_dir, half_dir);
  vec3 f0 = mix(vec3(0.04), albedo, metallic);

  // Indirect lighting (SH)
  LightmapTexel texel = GetLightmapTexel();
  vec3 sh_irradiance = EvalSHIrradiance(normal_world, texel.sh_coeffs);
  vec3 sky_emission = u_sky_color * u_sun.intensity / 10.f;
  vec3 indirect_irradiance = sh_irradiance + sky_emission * texel.visibility;
  vec3 indirect_reflection_radiance =
      EvalSHRadiance(reflection_dir, texel.sh_coeffs);

  vec3 l_indirect = ComputeIndirectBRDF(angles, indirect_irradiance,
                                        indirect_reflection_radiance, f0,
                                        albedo, metallic, roughness, occlusion);

  // Emission
  vec3 l_emission = u_emissive_strength * u_emissive_factor;
  if (u_has_emissive_texture > 0) {
    l_emission *= texture(u_emissive_texture, v_uv).rgb;
  }

  // Direct lighting (forward+).
  vec3 l_direct = vec3(0.0);

  // Direct sun.
  float sun_visibility = ComputeShadow(v_world_pos, normal_world, angles);
  vec3 sun_incoming = u_sun.color * u_sun.intensity * sun_visibility;
  vec3 direct_sun_brdf =
      ComputeDirectBRDF(angles, f0, albedo, metallic, roughness, occlusion);
  l_direct += direct_sun_brdf * angles.n_dot_l * sun_incoming;

  // Point and spot lights.
  ivec2 tile_id = ivec2(gl_FragCoord.xy) / 16;
  // Clamp tile_id to valid range.
  tile_id = clamp(tile_id, ivec2(0), u_tile_count - 1);
  uint tile_flat = uint(tile_id.y * u_tile_count.x + tile_id.x);

  uint tile_offset = tile_data[tile_flat * 3 + 0];
  uint p_count = tile_data[tile_flat * 3 + 1];
  uint s_count = tile_data[tile_flat * 3 + 2];

  // Point lights.
  for (uint i = 0; i < p_count; ++i) {
    uint light_idx = tile_data[tile_offset + i];
    GpuPointLight pl = gpu_point_lights[light_idx];
    vec3 to_light = pl.position - v_world_pos;
    float dist = length(to_light);

    if (dist < 0.005) {
      continue;
    }
    vec3 L = to_light / dist;
    vec3 H = normalize(view_dir + L);
    ShadingAngles local_angles =
        ComputeShadingAngles(normal_world, view_dir, L, H);

    vec3 incoming = pl.color * pl.intensity / (dist * dist);

    vec3 brdf = ComputeDirectBRDF(local_angles, f0, albedo, metallic, roughness,
                                  occlusion);
    l_direct += brdf * local_angles.n_dot_l * incoming;
  }

  // Spot lights.
  for (uint i = 0; i < s_count; ++i) {
    uint light_idx = tile_data[tile_offset + p_count + i];
    GpuSpotLight sl = gpu_spot_lights[light_idx];
    vec3 to_light = sl.position - v_world_pos;
    float dist = length(to_light);

    if (dist < 0.005) {
      continue;
    }
    vec3 L = to_light / dist;
    float cos_angle = dot(-L, sl.direction);

    // Angular falloff.
    float spot_effect =
        smoothstep(sl.cos_outer_cone, sl.cos_inner_cone, cos_angle);
    if (spot_effect > 0.0) {
      vec3 H = normalize(view_dir + L);
      ShadingAngles local_angles =
          ComputeShadingAngles(normal_world, view_dir, L, H);

      vec3 incoming = sl.color * sl.intensity * spot_effect / (dist * dist);

      vec3 brdf = ComputeDirectBRDF(local_angles, f0, albedo, metallic,
                                    roughness, occlusion);
      l_direct += brdf * local_angles.n_dot_l * incoming;
    }
  }

  vec3 color = l_emission + l_direct + l_indirect;

  out_color = vec4(color, 1.0);
}
