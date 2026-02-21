#version 460 core

layout(location = 0) out vec4 out_color;

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
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

uniform vec3 u_emissive_factor;
uniform float u_emissive_strength;
uniform int u_has_emissive_texture;

const float PI = 3.14159265359;

// --- PBR Functions ---

struct ShadingAngles {
  float n_dot_l;
  float n_dot_v;
  float n_dot_h;
  float l_dot_h;
  float v_dot_h;
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

vec3 FresnelSchlick(ShadingAngles angles, vec3 F0) {
  return F0 + (1.0 - F0) * pow(max(1.0 - angles.v_dot_h, 0.0), 5.0);
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

vec3 ComputeBRDF(ShadingAngles angles, vec3 albedo, float metallic,
                 float roughness, float occlusion) {
  vec3 F0 = vec3(0.04);
  F0 = mix(F0, albedo, metallic);

  vec3 F = FresnelSchlick(angles, F0);
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

  int layer = -1;
  for (int i = 0; i < NUM_CASCADES; ++i) {
    if (viewDepth < u_sun_cascade_splits[i]) {
      layer = i;
      break;
    }
  }
  if (layer == -1) layer = NUM_CASCADES - 1;

  const float normal_bias_scale = 0.05;
  vec3 biased_world_pos =
      world_pos + normal * (1.0 - angles.n_dot_l) * normal_bias_scale;
  vec4 frag_pos_light_space =
      u_sun_cascade_view_projections[layer] * vec4(biased_world_pos, 1.0);
  vec3 proj_coords = (frag_pos_light_space.xyz) / frag_pos_light_space.w;
  proj_coords = proj_coords * 0.5 + 0.5;

  float texel_size = 1.0 / float(textureSize(u_sun_shadow_maps[layer], 0).x);
  vec3 compare_coord = vec3(proj_coords.xy, proj_coords.z);
  float shadow =
      PCFPoissonDisk9(u_sun_shadow_maps[layer], compare_coord, texel_size, 2.0);

  return shadow;
}

// ---------------------

void main() {
  // 1. Material Properties
  vec4 albedo_sample = texture(u_albedo_texture, v_uv);
  vec3 albedo = albedo_sample.rgb;
  float alpha = albedo_sample.a;

  if (alpha < 0.5) discard;

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

  ShadingAngles angles =
      ComputeShadingAngles(normal_world, view_dir, light_dir, half_dir);

  // Direct incoming radiance
  float shadow = ComputeShadow(v_world_pos, normal_world, angles);
  vec3 direct_radiance = u_sun.color * u_sun.intensity * shadow;

  // TODO: Add indirect lighting

  // PBR Calculation
  vec3 brdf = ComputeBRDF(angles, albedo, metallic, roughness, occlusion);
  vec3 l_direct = brdf * angles.n_dot_l * direct_radiance;

  // Emission
  vec3 l_emission = u_emissive_strength * u_emissive_factor;
  if (u_has_emissive_texture > 0) {
    l_emission *= texture(u_emissive_texture, v_uv).rgb;
  }

  vec3 color = l_emission + l_direct;
  out_color = vec4(color, 1.0);
}
