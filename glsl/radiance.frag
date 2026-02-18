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

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float NdotH = max(dot(N, H), 0.0);
  float NdotH2 = NdotH * NdotH;

  float num = a2;
  float denom = (NdotH2 * (a2 - 1.0) + 1.0);
  denom = PI * denom * denom;

  return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;

  float num = NdotV;
  float denom = NdotV * (1.0 - k) + k;

  return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float ggx2 = GeometrySchlickGGX(NdotV, roughness);
  float ggx1 = GeometrySchlickGGX(NdotL, roughness);

  return ggx1 * ggx2;
}

// --- Shadow Calculation ---
float ShadowCalculation(vec3 worldPos, vec3 N, vec3 L) {
  vec4 viewPos = u_view * vec4(worldPos, 1.0);
  float viewDepth = abs(viewPos.z);

  int layer = -1;
  for (int i = 0; i < NUM_CASCADES; ++i) {
    if (viewDepth < u_sun_cascade_splits[i]) {
      layer = i;
      break;
    }
  }
  if (layer == -1) layer = NUM_CASCADES - 1;

  vec4 fragPosLightSpace =
      u_sun_cascade_view_projections[layer] * vec4(worldPos, 1.0);
  vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
  projCoords = projCoords * 0.5 + 0.5;

  if (projCoords.z > 1.0) return 1.0;

  float bias = max(0.005 * (1.0 - dot(N, L)), 0.001);

  // Hardware PCF
  float shadow = texture(u_sun_shadow_maps[layer],
                         vec3(projCoords.xy, projCoords.z - bias));

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
  vec3 tangentNormal = texture(u_normal_texture, v_uv).rgb;
  tangentNormal = tangentNormal * 2.0 - 1.0;

  vec3 N = normalize(v_normal);
  vec3 T = normalize(v_tangent.xyz - N * dot(v_tangent.xyz, N));
  vec3 B = cross(N, T) * (v_tangent.w > 0.0 ? 1.0 : -1.0);
  mat3 TBN = mat3(T, B, N);

  vec3 normal_world = normalize(TBN * tangentNormal);

  // 2. Lighting Vectors
  vec3 V = normalize(u_camera_pos - v_world_pos);
  vec3 L = normalize(-u_sun.direction);  // Direction from surface to light
  vec3 H = normalize(V + L);

  // 3. Shadow
  float shadow = ShadowCalculation(v_world_pos, normal_world, L);

  // 4. PBR Calculation
  vec3 F0 = vec3(0.04);
  F0 = mix(F0, albedo, metallic);

  vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
  float NDF = DistributionGGX(normal_world, H, roughness);
  float G = GeometrySmith(normal_world, V, L, roughness);

  vec3 numerator = NDF * G * F;
  float denominator =
      4.0 * max(dot(normal_world, V), 0.0) * max(dot(normal_world, L), 0.0) +
      0.0001;
  vec3 specular = numerator / denominator;

  vec3 kS = F;
  vec3 kD = vec3(1.0) - kS;
  kD *= (1.0 - metallic);

  float NdotL = max(dot(normal_world, L), 0.0);
  vec3 incoming_radiance = u_sun.color * u_sun.intensity * shadow;

  vec3 diffuse = albedo / PI;

  vec3 Lo = (kD * diffuse + specular) * incoming_radiance * NdotL;

  // 5. Emission
  vec3 emission = vec3(0.0);
  if (u_has_emissive_texture > 0) {
    emission = texture(u_emissive_texture, v_uv).rgb;
  }
  emission *= u_emissive_factor;
  emission *= u_emissive_strength;

  vec3 color = Lo + emission;

  out_color = vec4(color, 1.0);
}
