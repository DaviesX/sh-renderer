#pragma once

#include <embree4/rtcore.h>

#include <Eigen/Dense>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "ssbo.h"

namespace sh_renderer {

// --- Texture ---
struct Texture {
  // If set, the texture is loaded from a file. This denotes the provenance of
  // the texture.
  std::optional<std::filesystem::path> file_path;

  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t channels = 0;
  std::vector<uint8_t> pixel_data;

  // GL Resource
  uint32_t texture_id = 0;
};

// --- Texture32F ---
struct Texture32F {
  // If set, the texture is loaded from a file. This denotes the provenance of
  // the texture.
  std::optional<std::filesystem::path> file_path;

  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t channels = 0;
  std::vector<float> pixel_data;

  // GL Resource. The underlying texture may be 16-bit.
  uint32_t texture_id = 0;
};

// --- Texture32I ---
struct Texture32I {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t channels = 0;
  std::vector<int32_t> pixel_data;

  // GL Resource
  uint32_t texture_id = 0;
};

// --- Material ---
struct Material {
  std::string name;

  // Albedo / Transparency
  Texture albedo;
  Texture normal_texture;
  Texture metallic_roughness_texture;  // Metallic in B, Roughness in G

  // Emission (for Area Lights).
  Eigen::Vector3f emissive_factor = Eigen::Vector3f::Zero();
  float emissive_strength = 0.f;
  std::optional<Texture> emissive_texture;

  // Alpha Cutout.
  bool alpha_cutout = false;
};

// --- Geometry ---
struct Geometry {
  std::vector<Eigen::Vector3f> vertices;
  std::vector<Eigen::Vector3f> normals;
  std::vector<Eigen::Vector2f> texture_uvs;
  std::vector<Eigen::Vector2f> lightmap_uvs;
  std::vector<Eigen::Vector4f> tangents;  // xyz + w (sign)

  std::vector<uint32_t> indices;

  int material_id = -1;  // Index into Scene::materials
  Eigen::Affine3f transform = Eigen::Affine3f::Identity();

  // GL Resources
  uint32_t vao = 0;
  uint32_t vbo = 0;
  uint32_t ebo = 0;
  uint32_t index_count = 0;
};

// --- Light ---

struct PointLight {
  Eigen::Vector3f position = Eigen::Vector3f::Zero();
  Eigen::Vector3f color = Eigen::Vector3f::Ones();
  float intensity = 1.0f;

  // GL Resources
  int shadow_map_layer = -1;
};

struct SpotLight {
  Eigen::Vector3f position = Eigen::Vector3f::Zero();
  Eigen::Vector3f direction = Eigen::Vector3f(0, 0, -1);
  Eigen::Vector3f color = Eigen::Vector3f::Ones();
  float intensity = 1.0f;

  float cos_inner_cone = 1.0f;
  float cos_outer_cone = 0.70710678118654752440f;  // cos(pi/4)

  // GL Resources
  int shadow_map_layer = -1;
};

struct SunLight {
  Eigen::Vector3f direction = Eigen::Vector3f(0, -1, 0);
  Eigen::Vector3f color = Eigen::Vector3f::Ones();
  float intensity = 1.0f;

  // GL Resources
  int shadow_map_layer = -1;
};

struct AreaLight {
  Eigen::Vector3f position = Eigen::Vector3f::Zero();
  Eigen::Vector3f direction = Eigen::Vector3f(0, 0, -1);
  Eigen::Vector3f color = Eigen::Vector3f::Ones();
  float intensity = 1.0f;
  float area = 0.0f;
  const Material* material = nullptr;
  const Geometry* geometry = nullptr;
};

// --- Scene ---
struct Scene {
  std::vector<Geometry> geometries;
  std::vector<Material> materials;

  std::vector<PointLight> point_lights;
  std::vector<SpotLight> spot_lights;
  std::vector<AreaLight> area_lights;
  std::optional<SunLight> sun_light;

  // Baked Indirect SH Lightmaps
  std::array<Texture32F, 3> lightmaps_packed;

  // GL Resources
  SSBO point_light_list_ssbo;
  SSBO spot_light_list_ssbo;
};

// Uploads the scene geometry and textures to the GPU.
// Populates the GL resource handles in the scene structs.
// Uses Direct State Access (DSA) for all GL operations.
//
// TODO: Add function to upload the point and spot light lists to the GPU SSBO.
void UploadSceneToGPU(Scene& scene);

// Transforms the geometry by the transform matrix.
std::vector<Eigen::Vector3f> TransformedVertices(const Geometry& geometry);
std::vector<Eigen::Vector3f> TransformedNormals(const Geometry& geometry);
std::vector<Eigen::Vector4f> TransformedTangents(const Geometry& geometry);

// Returns the surface area of the geometry.
float SurfaceArea(const Geometry& geometry);

// Loads the SH lightmap EXRs into the scene.
void LoadLightmaps(Scene& scene, const std::filesystem::path& gltf_file);

}  // namespace sh_renderer
