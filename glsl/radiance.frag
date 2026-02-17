#version 460 core

layout(location = 0) out vec4 out_color;

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_tangent;

// Camera
uniform vec3 u_camera_pos;

// Sun Light
struct Light {
  vec3 direction;
  vec3 color;
  float intensity;
};
uniform Light u_sun;

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

// ---------------------

void main() {
  // 1. Material Properties
  vec4 albedo_sample = texture(u_albedo_texture, v_uv);
  vec3 albedo = pow(albedo_sample.rgb, vec3(2.2));  // sRGB to Linear
  float alpha = albedo_sample.a;

  if (alpha < 0.5) discard;

  vec4 mr_sample = texture(u_metallic_roughness_texture, v_uv);
  float metallic = mr_sample.b;
  float roughness = mr_sample.g;

  // Normal Mapping
  vec3 tangentNormal = texture(u_normal_texture, v_uv).xyz * 2.0 - 1.0;

  vec3 Q1 = dFdx(v_world_pos);
  vec3 Q2 = dFdy(v_world_pos);
  vec2 st1 = dFdx(v_uv);
  vec2 st2 = dFdy(v_uv);

  vec3 N = normalize(v_normal);
  vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
  vec3 B = -normalize(cross(N, T));
  mat3 TBN = mat3(T, B, N);

  vec3 normal_world = normalize(TBN * tangentNormal);

  // 2. Lighting Vectors
  vec3 V = normalize(u_camera_pos - v_world_pos);
  vec3 L = normalize(-u_sun.direction);  // Direction from surface to light
  vec3 H = normalize(V + L);

  // 3. PBR Calculation
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
  vec3 radiance = u_sun.color * u_sun.intensity;

  vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

  // 4. Emission
  vec3 emission = vec3(0.0);
  if (u_has_emissive_texture > 0) {
    emission = texture(u_emissive_texture, v_uv).rgb;
    emission = pow(emission, vec3(2.2));  // sRGB to Linear
  }
  emission += u_emissive_factor;
  emission *= u_emissive_strength;

  vec3 color = Lo + emission;

  // No tonemapping here, output HDR
  out_color = vec4(color, 1.0);
}
