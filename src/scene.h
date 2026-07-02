#pragma once

#include <embree4/rtcore.h>

#include <Eigen/Dense>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "culling.h"
#include "q3_layer.h"
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

// --- Layer ---
// One Quake 3 shader stage from `SH_material_layers`. The base layer's colour is
// the modern `baseColorTexture` (the artist-modernised albedo) substituted at
// composite time; its own `texture` is kept for coverage (alpha).
struct Layer {
  Texture texture;
  // animMap frames (frame 0 == `texture`); empty for a static `map` stage.
  std::vector<Texture> anim_frames;
  float anim_freq = 0.f;  // frames per second

  BlendFactor blend_src = BlendFactor::kOne;
  BlendFactor blend_dst = BlendFactor::kZero;
  RgbGen rgbgen;
  std::vector<TcMod> tcmods;
  bool is_base = false;
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

  // Additive (order-independent) transparency: every layer blends with dst
  // factor GL_ONE (flames, glows, plasma). Such materials render only in the
  // additive pass (GL_ONE/GL_ONE), never the opaque/cutout/depth/shadow passes.
  bool additive = false;

  // Quake 3 shader stack from SH_material_layers (empty for plain PBR materials).
  std::vector<Layer> layers;
  // Index of the layer whose colour comes from the modern albedo; -1 for
  // additive materials, where every stage samples its own (animated) texture.
  int base_layer = 0;
  CullMode cull_mode = CullMode::kFront;
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

  // Culling
  AABB bounding_box;
};

// --- Light ---

struct PointLight {
  Eigen::Vector3f position = Eigen::Vector3f::Zero();
  Eigen::Vector3f color = Eigen::Vector3f::Ones();
  float intensity = 1.0f;
  float radius = 0.0f;  // Bounding sphere for culling.

  // GL Resources
  int shadow_map_layer = -1;
};

struct SpotLight {
  Eigen::Vector3f position = Eigen::Vector3f::Zero();
  Eigen::Vector3f direction = Eigen::Vector3f(0, 0, -1);
  Eigen::Vector3f color = Eigen::Vector3f::Ones();
  float intensity = 1.0f;
  float radius = 0.0f;  // Bounding sphere for culling.

  float cos_inner_cone = 1.0f;
  float cos_outer_cone = 0.70710678118654752440f;  // cos(pi/4)

  // GL Resources
  int shadow_map_layer = -1;
  int has_shadow = 0;
  Eigen::Vector2f shadow_uv_offset = Eigen::Vector2f::Zero();
  Eigen::Vector2f shadow_uv_scale = Eigen::Vector2f::Ones();
  Eigen::Matrix4f shadow_view_proj = Eigen::Matrix4f::Identity();
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

// --- Shadow Atlas context ---
struct ShadowAtlasContext {
  uint32_t atlas_texture = 0;
  uint32_t atlas_fbo = 0;
  uint32_t resolution = 2048;
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
  // SH_material_layers descriptors (see GpuMaterial/GpuMaterialLayer/GpuTcMod).
  SSBO material_range_ssbo;   // one GpuMaterial per scene material
  SSBO material_layer_ssbo;   // flat GpuMaterialLayer array
  SSBO material_tcmod_ssbo;   // flat GpuTcMod array
  ShadowAtlasContext shadow_atlas;
};

// GPU-side SH_material_layers descriptors (std430, tightly packed, all 4-byte
// scalars padded to 16-byte multiples). Layer textures are NOT referenced here:
// the draw binds each material's layers to a capped sampler array in order and
// the CPU selects the active animMap frame. The base layer's colour comes from
// the modern baseColorTexture instead of its sampler (shader checks base_layer).
// Max layer samplers bound per draw (the shader's `u_layers` array size). Q3
// materials rarely exceed this; extra stages are dropped.
constexpr int kMaxLayers = 8;

struct GpuMaterial {
  int32_t layer_offset;      // first layer in the flat layer array
  int32_t layer_count;       // 0 for plain PBR materials
  int32_t base_layer;        // index within [0, layer_count)
  int32_t modern_has_alpha;  // 1 if the base coverage comes from the modern albedo
};
static_assert(sizeof(GpuMaterial) == 16);

struct GpuMaterialLayer {
  int32_t blend_src;  // BlendFactor
  int32_t blend_dst;  // BlendFactor
  int32_t rgbgen_type;
  int32_t rgbgen_wave;
  float rgbgen_base;
  float rgbgen_amplitude;
  float rgbgen_phase;
  float rgbgen_frequency;
  int32_t tcmod_offset;  // first tcMod in the flat tcMod array
  int32_t tcmod_count;
  int32_t _pad0;
  int32_t _pad1;
};
static_assert(sizeof(GpuMaterialLayer) == 48);

struct GpuTcMod {
  int32_t type;  // TcModType
  int32_t wave;  // WaveType (TURB/STRETCH)
  float v[6];    // SCALE [s,t]; TRANSFORM [m00..m12]; TURB/STRETCH [base,amp,phase,freq]
};
static_assert(sizeof(GpuTcMod) == 32);

// Packs the materials' layer stacks into flat GpuMaterial/GpuMaterialLayer/
// GpuTcMod arrays (pure CPU; no GL). Every material yields one GpuMaterial (with
// layer_count 0 for plain PBR). Exposed for testing.
void BuildLayerBuffers(const std::vector<Material>& materials,
                       std::vector<GpuMaterial>* out_materials,
                       std::vector<GpuMaterialLayer>* out_layers,
                       std::vector<GpuTcMod>* out_tcmods);

// GPU-side light structs (std430 layout).
// These are tightly packed for SSBO upload.
struct GpuPointLight {
  float position[3];
  float radius;
  float color[3];
  float intensity;
};
static_assert(sizeof(GpuPointLight) == 32);

struct GpuSpotLight {
  float position[3];
  float radius;
  float direction[3];
  float intensity;
  float color[3];
  float cos_inner_cone;
  float cos_outer_cone;
  int has_shadow;
  float shadow_uv_offset[2];
  float shadow_uv_scale[2];
  float _pad[2];
  float shadow_view_proj[16];
};
static_assert(sizeof(GpuSpotLight) == 144);

// Computes the bounding radius of a light from its flux via inverse-square law.
// radius = sqrt(flux / threshold), where flux = intensity * max(color).
float ComputeLightRadius(float intensity, const Eigen::Vector3f& color,
                         float threshold = 0.01f);

// Uploads the scene geometry and textures to the GPU.
// Populates the GL resource handles in the scene structs.
// Uses Direct State Access (DSA) for all GL operations.
void UploadSceneToGPU(Scene& scene);

// Uploads the point and spot light lists to the GPU SSBOs.
void UploadLightsToGPU(Scene& scene);

// Frustum cull spot lights against main camera, rank by form factor,
// and allocate into the shadow atlas.
void AllocateShadowMapForLights(Scene& scene, const class Camera& camera);

// Computes the world-space bounding box for each geometry in the scene.
void ComputeSceneBoundingBoxes(Scene& scene);

// Optimizes the scene by sorting geometries by material_id to minimize state
// changes.
void OptimizeScene(Scene& scene);

// Partitions each loose geometry in the scene into independent connected
// components that are further away than 0.1 meters. Replaces the original
// geometry with the partitioned geometries in the scene.
void PartitionLooseGeometries(Scene& scene);

// Logs statistics about the scene, such as the total number of geometries,
// materials, lights, and vertices.
void LogScene(const Scene& scene);

// Transforms the geometry by the transform matrix.
std::vector<Eigen::Vector3f> TransformedVertices(const Geometry& geometry);
std::vector<Eigen::Vector3f> TransformedNormals(const Geometry& geometry);
std::vector<Eigen::Vector4f> TransformedTangents(const Geometry& geometry);

// Returns the surface area of the geometry.
float SurfaceArea(const Geometry& geometry);

// Loads the SH lightmap EXRs into the scene.
void LoadLightmaps(Scene& scene, const std::filesystem::path& gltf_file);

}  // namespace sh_renderer
