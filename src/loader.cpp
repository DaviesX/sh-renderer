#include "loader.h"

#include <glog/logging.h>

#include <Eigen/Geometry>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>

#include "colorspace.h"
#include "scene.h"
#include "tiny_gltf.h"

namespace sh_baker {
namespace {

const float kLightIntensityScale = 1 / 200.f;

// Helper to convert array to Eigen matrix/vector
Eigen::Affine3f NodeToTransform(const tinygltf::Node& node) {
  Eigen::Affine3f transform = Eigen::Affine3f::Identity();

  if (node.matrix.size() == 16) {
    Eigen::Matrix4f mat;
    for (int i = 0; i < 16; ++i) mat(i) = static_cast<float>(node.matrix[i]);
    transform = Eigen::Affine3f(mat);
  } else {
    if (node.translation.size() == 3) {
      transform.translate(
          Eigen::Vector3f(static_cast<float>(node.translation[0]),
                          static_cast<float>(node.translation[1]),
                          static_cast<float>(node.translation[2])));
    }
    if (node.rotation.size() == 4) {
      // glTF quat is x, y, z, w. Eigen Quat constructor is w, x, y, z
      Eigen::Quaternionf q(static_cast<float>(node.rotation[3]),
                           static_cast<float>(node.rotation[0]),
                           static_cast<float>(node.rotation[1]),
                           static_cast<float>(node.rotation[2]));
      transform.rotate(q);
    }
    if (node.scale.size() == 3) {
      transform.scale(Eigen::Vector3f(static_cast<float>(node.scale[0]),
                                      static_cast<float>(node.scale[1]),
                                      static_cast<float>(node.scale[2])));
    }
  }
  return transform;
}

bool ProcessPrimitive(const tinygltf::Model& model,
                      const tinygltf::Primitive& primitive,
                      const Eigen::Affine3f& transform,
                      std::vector<Geometry>* result) {
  // Precondition.
  if (primitive.material < 0) {
    LOG(ERROR) << "Primitive missing material.";
    return false;
  }
  if (primitive.attributes.find("POSITION") == primitive.attributes.end()) {
    LOG(ERROR) << "Primitive missing POSITION attribute.";
    return false;
  }
  if (primitive.attributes.find("NORMAL") == primitive.attributes.end()) {
    LOG(ERROR) << "Primitive missing NORMAL attribute. Baking requires Vertex "
                  "Normals.";
    return false;
  }
  if (primitive.attributes.find("TEXCOORD_0") == primitive.attributes.end()) {
    LOG(ERROR) << "Primitive missing TEXCOORD_0 attribute. Baking requires "
                  "UVs.";
    return false;
  }

  Geometry geo;
  geo.transform = transform;
  geo.material_id = primitive.material;

  // Get Position
  const tinygltf::Accessor& pos_accessor =
      model.accessors[primitive.attributes.at("POSITION")];
  const tinygltf::BufferView& pos_view =
      model.bufferViews[pos_accessor.bufferView];
  const tinygltf::Buffer& pos_buffer = model.buffers[pos_view.buffer];
  const float* pos_data = reinterpret_cast<const float*>(
      &pos_buffer.data[pos_view.byteOffset + pos_accessor.byteOffset]);
  size_t vertex_count = pos_accessor.count;
  int pos_stride = pos_accessor.ByteStride(pos_view)
                       ? (pos_accessor.ByteStride(pos_view) / sizeof(float))
                       : 3;

  // Get Normal
  const float* norm_data = nullptr;
  int norm_stride = 0;
  if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
    const tinygltf::Accessor& norm_accessor =
        model.accessors[primitive.attributes.at("NORMAL")];
    const tinygltf::BufferView& norm_view =
        model.bufferViews[norm_accessor.bufferView];
    const tinygltf::Buffer& norm_buffer = model.buffers[norm_view.buffer];
    norm_data = reinterpret_cast<const float*>(
        &norm_buffer.data[norm_view.byteOffset + norm_accessor.byteOffset]);
    norm_stride = norm_accessor.ByteStride(norm_view)
                      ? (norm_accessor.ByteStride(norm_view) / sizeof(float))
                      : 3;
  }

  // Get UV0
  const float* uv0_data = nullptr;
  int uv0_stride = 0;
  if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
    const tinygltf::Accessor& uv_accessor =
        model.accessors[primitive.attributes.at("TEXCOORD_0")];
    const tinygltf::BufferView& uv_view =
        model.bufferViews[uv_accessor.bufferView];
    const tinygltf::Buffer& uv_buffer = model.buffers[uv_view.buffer];
    uv0_data = reinterpret_cast<const float*>(
        &uv_buffer.data[uv_view.byteOffset + uv_accessor.byteOffset]);
    uv0_stride = uv_accessor.ByteStride(uv_view)
                     ? (uv_accessor.ByteStride(uv_view) / sizeof(float))
                     : 2;
  }

  // Get UV1
  const float* uv1_data = nullptr;
  int uv1_stride = 0;
  if (primitive.attributes.find("TEXCOORD_1") != primitive.attributes.end()) {
    const tinygltf::Accessor& uv_accessor =
        model.accessors[primitive.attributes.at("TEXCOORD_1")];
    const tinygltf::BufferView& uv_view =
        model.bufferViews[uv_accessor.bufferView];
    const tinygltf::Buffer& uv_buffer = model.buffers[uv_view.buffer];
    uv1_data = reinterpret_cast<const float*>(
        &uv_buffer.data[uv_view.byteOffset + uv_accessor.byteOffset]);
    uv1_stride = uv_accessor.ByteStride(uv_view)
                     ? (uv_accessor.ByteStride(uv_view) / sizeof(float))
                     : 2;
  }

  // Get Tangent
  const float* tan_data = nullptr;
  int tan_stride = 0;
  if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
    const tinygltf::Accessor& tan_accessor =
        model.accessors[primitive.attributes.at("TANGENT")];
    const tinygltf::BufferView& tan_view =
        model.bufferViews[tan_accessor.bufferView];
    const tinygltf::Buffer& tan_buffer = model.buffers[tan_view.buffer];
    tan_data = reinterpret_cast<const float*>(
        &tan_buffer.data[tan_view.byteOffset + tan_accessor.byteOffset]);
    tan_stride = tan_accessor.ByteStride(tan_view)
                     ? (tan_accessor.ByteStride(tan_view) / sizeof(float))
                     : 4;
  }

  // Indices
  if (primitive.indices > -1) {
    const tinygltf::Accessor& index_accessor =
        model.accessors[primitive.indices];
    const tinygltf::BufferView& index_view =
        model.bufferViews[index_accessor.bufferView];
    const tinygltf::Buffer& index_buffer = model.buffers[index_view.buffer];
    const uint8_t* index_data_ptr =
        &index_buffer.data[index_view.byteOffset + index_accessor.byteOffset];
    int stride = index_accessor.ByteStride(index_view);

    geo.indices.reserve(index_accessor.count);
    for (size_t i = 0; i < index_accessor.count; ++i) {
      uint32_t val = 0;
      if (index_accessor.componentType ==
          TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        val = *(index_data_ptr + i * stride);
      } else if (index_accessor.componentType ==
                 TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        val = *reinterpret_cast<const uint16_t*>(index_data_ptr + i * stride);
      } else if (index_accessor.componentType ==
                 TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        val = *reinterpret_cast<const uint32_t*>(index_data_ptr + i * stride);
      }
      geo.indices.push_back(val);
    }
  }

  // Vertices
  geo.vertices.reserve(vertex_count);
  geo.normals.reserve(vertex_count);
  geo.texture_uvs.reserve(vertex_count);
  if (uv1_data) {
    geo.lightmap_uvs.reserve(vertex_count);
  }
  geo.tangents.reserve(vertex_count);

  for (size_t i = 0; i < vertex_count; ++i) {
    geo.vertices.emplace_back(pos_data[i * pos_stride + 0],
                              pos_data[i * pos_stride + 1],
                              pos_data[i * pos_stride + 2]);

    if (norm_data) {
      geo.normals.emplace_back(norm_data[i * norm_stride + 0],
                               norm_data[i * norm_stride + 1],
                               norm_data[i * norm_stride + 2]);
    } else {
      // Normals are critical for baking.
      // We could try to generate them from face normals?
      // User says: "return std::nullopt if ... normal attribute exists" ->
      // imply strict check at Scene level. But ProcessPrimitive returns void.
      // We need to handle this. For now, let's log error and rely on LoadScene
      // to fail if scene is empty/invalid? Actually, user wants strict check.
      // Better to check outside or flag error. Since function is void, let's
      // assume valid normals for now or fill dummy to avoid crash, but strict
      // check is requested. Let's stick to user request: "return std::nullopt
      // if the precondition that the normal attribute exists"
      // -> We need to propagage error up.
      // For now, let's keep the fallback (0,1,0) but LOG(ERROR) so it's
      // visible? Or: The Loop below generates tangents.
      geo.normals.emplace_back(0, 1, 0);
    }

    if (uv0_data) {
      geo.texture_uvs.emplace_back(uv0_data[i * uv0_stride + 0],
                                   uv0_data[i * uv0_stride + 1]);
    } else {
      geo.texture_uvs.emplace_back(0, 0);
    }

    if (uv1_data) {
      geo.lightmap_uvs.emplace_back(uv1_data[i * uv1_stride + 0],
                                    uv1_data[i * uv1_stride + 1]);
    }

    if (tan_data) {
      geo.tangents.emplace_back(
          tan_data[i * tan_stride + 0], tan_data[i * tan_stride + 1],
          tan_data[i * tan_stride + 2], tan_data[i * tan_stride + 3]);
    }
  }

  // Filter degenerate triangles
  if (!geo.indices.empty() && !geo.vertices.empty()) {
    std::vector<uint32_t> valid_indices;
    valid_indices.reserve(geo.indices.size());
    int degenerate_count = 0;

    for (size_t i = 0; i < geo.indices.size(); i += 3) {
      uint32_t i0 = geo.indices[i];
      uint32_t i1 = geo.indices[i + 1];
      uint32_t i2 = geo.indices[i + 2];

      bool degenerate = false;
      if (i0 == i1 || i0 == i2 || i1 == i2) {
        // Duplicate indices.
        degenerate = true;
      }

      if (degenerate) {
        degenerate_count++;
      } else {
        valid_indices.push_back(i0);
        valid_indices.push_back(i1);
        valid_indices.push_back(i2);
      }
    }

    if (degenerate_count > 0) {
      LOG(WARNING) << "Removed " << degenerate_count
                   << " degenerate triangles from primitive.";
      geo.indices = std::move(valid_indices);
    }
  }

  // Tangent Generation Logic
  if (geo.tangents.empty()) {
    LOG(ERROR) << "Primitive missing tangents.";
    return false;
  }

  result->push_back(std::move(geo));
  return true;
}

// Helper to decode URI (e.g. %20 -> space)
std::string UrlDecode(const std::string& str) {
  std::string ret;
  ret.reserve(str.length());
  for (size_t i = 0; i < str.length(); ++i) {
    if (str[i] == '%' && i + 2 < str.length()) {
      int value = 0;
      bool valid = true;
      for (int j = 1; j <= 2; ++j) {
        char c = str[i + j];
        if (c >= '0' && c <= '9') {
          value = (value << 4) | (c - '0');
        } else if (c >= 'a' && c <= 'f') {
          value = (value << 4) | (c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
          value = (value << 4) | (c - 'A' + 10);
        } else {
          valid = false;
          break;
        }
      }

      if (valid) {
        ret += static_cast<char>(value);
        i += 2;
        continue;
      }
    }
    ret += str[i];
  }
  return ret;
}

// Helper to load a texture
void LoadTexture(const tinygltf::Model& model, int tex_idx,
                 const std::filesystem::path& base_path, Texture* out_tex,
                 bool srgb = true) {
  if (tex_idx >= 0 && tex_idx < model.textures.size()) {
    int img_idx = model.textures[tex_idx].source;
    if (img_idx >= 0 && img_idx < model.images.size()) {
      const auto& img = model.images[img_idx];
      out_tex->width = img.width;
      out_tex->height = img.height;
      out_tex->channels = img.component;
      out_tex->pixel_data = img.image;  // Copy data
      if (!img.uri.empty()) {
        std::filesystem::path uri_path(UrlDecode(img.uri));
        if (uri_path.is_absolute()) {
          out_tex->file_path = uri_path;
        } else {
          out_tex->file_path = std::filesystem::absolute(base_path / uri_path);
        }
      }
      return;
    }
  }
}

void ProcessMaterials(const tinygltf::Model& model,
                      const std::filesystem::path& base_path,
                      std::vector<Material>* result) {
  if (model.materials.empty()) {
    Material default_mat;
    default_mat.name = "default";
    // Default 1x1 white albedo
    default_mat.albedo.width = 1;
    default_mat.albedo.height = 1;
    default_mat.albedo.channels = 4;
    default_mat.albedo.pixel_data = {255, 255, 255, 255};

    // Default 1x1 normal (0.5, 0.5, 1.0) -> (128, 128, 255)
    default_mat.normal_texture.width = 1;
    default_mat.normal_texture.height = 1;
    default_mat.normal_texture.channels = 3;
    default_mat.normal_texture.pixel_data = {128, 128, 255};

    // Default 1x1 metallic-roughness (roughness=1, metallic=1) -> G=255, B=255
    default_mat.metallic_roughness_texture.width = 1;
    default_mat.metallic_roughness_texture.height = 1;
    default_mat.metallic_roughness_texture.channels = 3;
    default_mat.metallic_roughness_texture.pixel_data = {0, 255,
                                                         255};  // R unused

    result->push_back(default_mat);
    return;
  }

  for (const auto& gltf_mat : model.materials) {
    Material mat;
    mat.name = gltf_mat.name;

    // Emissive Strength
    auto emissive_strength_it =
        gltf_mat.extensions.find("KHR_materials_emissive_strength");
    if (emissive_strength_it != gltf_mat.extensions.end()) {
      const auto& emissive_strength = emissive_strength_it->second;
      if (emissive_strength.Has("emissiveStrength")) {
        mat.emissive_strength = float(
            emissive_strength.Get("emissiveStrength").GetNumberAsDouble());
      }
    }

    // Emissive Factor
    mat.emissive_factor =
        Eigen::Vector3f(gltf_mat.emissiveFactor[0], gltf_mat.emissiveFactor[1],
                        gltf_mat.emissiveFactor[2]);

    // Emissive Texture
    int emissive_idx = gltf_mat.emissiveTexture.index;
    Texture emissive_texture;
    LoadTexture(model, emissive_idx, base_path, &emissive_texture, true);
    if (!emissive_texture.pixel_data.empty()) {
      mat.emissive_texture = emissive_texture;
    }

    // Texture (Base Color)
    int albedo_idx = gltf_mat.pbrMetallicRoughness.baseColorTexture.index;
    LoadTexture(model, albedo_idx, base_path, &mat.albedo, true);

    if (mat.albedo.pixel_data.empty()) {
      // Create 1x1 texture from baseColorFactor (Linear -> sRGB)
      const auto& color = gltf_mat.pbrMetallicRoughness.baseColorFactor;
      mat.albedo.width = 1;
      mat.albedo.height = 1;
      mat.albedo.channels = 4;
      mat.albedo.pixel_data.resize(4);

      // baseColorFactor is RGBA (4 items)
      if (color.size() == 4) {
        mat.albedo.pixel_data[0] = LinearToSRGB(static_cast<float>(color[0]));
        mat.albedo.pixel_data[1] = LinearToSRGB(static_cast<float>(color[1]));
        mat.albedo.pixel_data[2] = LinearToSRGB(static_cast<float>(color[2]));
        mat.albedo.pixel_data[3] = static_cast<unsigned char>(
            std::rint(std::clamp(color[3], 0.0, 1.0) * 255.0));
      } else {
        // Fallback white
        mat.albedo.pixel_data = {255, 255, 255, 255};
      }
    }

    // Normal Texture
    int norm_idx = gltf_mat.normalTexture.index;
    LoadTexture(model, norm_idx, base_path, &mat.normal_texture, false);

    if (mat.normal_texture.pixel_data.empty()) {
      // Default 1x1 normal (0.5, 0.5, 1.0) -> (128, 128, 255)
      mat.normal_texture.width = 1;
      mat.normal_texture.height = 1;
      mat.normal_texture.channels = 3;
      mat.normal_texture.pixel_data = {128, 128, 255};
    }

    // Metallic-Roughness Texture
    int mr_idx = gltf_mat.pbrMetallicRoughness.metallicRoughnessTexture.index;
    LoadTexture(model, mr_idx, base_path, &mat.metallic_roughness_texture,
                false);

    if (mat.metallic_roughness_texture.pixel_data.empty()) {
      // Default 1x1 using factors
      // Metallic is stored in B, Roughness in G
      mat.metallic_roughness_texture.width = 1;
      mat.metallic_roughness_texture.height = 1;
      mat.metallic_roughness_texture.channels = 3;
      // R is unused, G=Roughness, B=Metallic
      auto roughness =
          static_cast<float>(gltf_mat.pbrMetallicRoughness.roughnessFactor);
      auto metallic =
          static_cast<float>(gltf_mat.pbrMetallicRoughness.metallicFactor);
      mat.metallic_roughness_texture.pixel_data = {
          0, static_cast<uint8_t>(roughness * 255),
          static_cast<uint8_t>(metallic * 255)};
    }

    result->push_back(std::move(mat));
  }
}

void ProcessPunctualLight(const tinygltf::Model& model,
                          const tinygltf::Value& light_obj,
                          const Eigen::Affine3f& transform,
                          std::vector<Light>* result) {
  // Parse KHR_lights_punctual object
  if (!light_obj.IsObject()) return;

  Light l;

  // Position/Direction from transform
  l.position = transform.translation();
  l.direction = transform.rotation() * Eigen::Vector3f(0, 0, -1);

  float len_sq = l.direction.squaredNorm();
  if (len_sq > 1e-8f) {
    if (std::isinf(len_sq) || std::isnan(len_sq)) {
      LOG(WARNING) << "Light direction is NaN/Inf. Fallback to -Z.";
      l.direction = Eigen::Vector3f(0, 0, -1);
    } else {
      l.direction.normalize();
    }
  } else {
    LOG(WARNING) << "Light direction is zero. Fallback to -Z.";
    l.direction = Eigen::Vector3f(0, 0, -1);
  }

  std::string type_str;
  if (light_obj.Has("type")) {
    type_str = light_obj.Get("type").Get<std::string>();
  }

  if (type_str == "point")
    l.type = Light::Type::Point;
  else if (type_str == "directional")
    l.type = Light::Type::Directional;
  else if (type_str == "spot")
    l.type = Light::Type::Spot;

  if (light_obj.Has("color")) {
    const auto& color_arr = light_obj.Get("color");
    if (color_arr.IsArray() && color_arr.ArrayLen() == 3) {
      l.color = Eigen::Vector3f(color_arr.Get(0).Get<double>(),
                                color_arr.Get(1).Get<double>(),
                                color_arr.Get(2).Get<double>());
    }
  }

  if (light_obj.Has("intensity")) {
    l.intensity = static_cast<float>(light_obj.Get("intensity").Get<double>()) *
                  kLightIntensityScale;
  }

  if (l.type == Light::Type::Spot && light_obj.Has("spot")) {
    const auto& spot = light_obj.Get("spot");
    if (spot.Has("innerConeAngle")) {
      float inner_cone_angle =
          static_cast<float>(spot.Get("innerConeAngle").Get<double>());
      l.cos_inner_cone = std::cos(inner_cone_angle);
    }
    if (spot.Has("outerConeAngle")) {
      float outer_cone_angle =
          static_cast<float>(spot.Get("outerConeAngle").Get<double>());
      l.cos_outer_cone = std::cos(outer_cone_angle);
    }
  }

  result->push_back(std::move(l));
}

void ProcessAreaLights(const std::vector<Material>& materials,
                       const std::vector<Geometry>& geometries,
                       std::vector<Light>* result) {
  // Group geometries by material id for faster lookup.
  std::unordered_map<int, std::vector<const Geometry*>> geos_by_mat;
  for (size_t i = 0; i < geometries.size(); i++) {
    const Geometry& geometry = geometries[i];
    geos_by_mat[geometry.material_id].push_back(&geometry);
  }

  // Searches for emissive materials and creates area lights for the geometries
  // that use them.
  for (int mat_idx = 0; mat_idx < materials.size(); mat_idx++) {
    const Material& mat = materials[mat_idx];
    // Emission
    if (mat.emissive_strength == 0.0f) {
      continue;
    }

    const auto geos_it = geos_by_mat.find(mat_idx);
    if (geos_it == geos_by_mat.end()) {
      // No geometries use this material.
      continue;
    }
    const auto& geos = geos_it->second;
    for (const auto& geo : geos) {
      Light area_light;
      area_light.type = Light::Type::Area;
      area_light.intensity = mat.emissive_strength;
      area_light.color = mat.emissive_factor;
      area_light.material = &materials[mat_idx];
      area_light.geometry = geo;
      area_light.area = SurfaceArea(*geo);

      result->emplace_back(std::move(area_light));
    }
  }
}

bool TraverseNodes(const tinygltf::Model& model, int node_index,
                   const Eigen::Affine3f& parent_transform, Scene* scene) {
  const tinygltf::Node& node = model.nodes[node_index];

  Eigen::Affine3f global_transform = parent_transform * NodeToTransform(node);

  // Mesh
  if (node.mesh >= 0) {
    const tinygltf::Mesh& mesh = model.meshes[node.mesh];
    for (const auto& primitive : mesh.primitives) {
      if (!ProcessPrimitive(model, primitive, global_transform,
                            &scene->geometries)) {
        return false;
      }
    }
  }

  // Light (KHR_lights_punctual)
  // The extension is defined on the node as "extensions": {
  // "KHR_lights_punctual": { "light": 0 }
  // }
  auto lights_punctual_it = node.extensions.find("KHR_lights_punctual");
  if (lights_punctual_it != node.extensions.end()) {
    const auto& ext = lights_punctual_it->second;
    if (ext.Has("light")) {
      int light_idx = ext.Get("light").Get<int>();
      if (model.extensions.find("KHR_lights_punctual") !=
          model.extensions.end()) {
        const auto& model_ext = model.extensions.at("KHR_lights_punctual");
        if (model_ext.Has("lights")) {
          const auto& lights_arr = model_ext.Get("lights");
          if (lights_arr.IsArray() && light_idx < lights_arr.ArrayLen()) {
            ProcessPunctualLight(model, lights_arr.Get(light_idx),
                                 global_transform, &scene->lights);
          }
        }
      }
    }
  }

  for (int child : node.children) {
    if (!TraverseNodes(model, child, global_transform, scene)) return false;
  }
  return true;
}

const Light* BrightestDirectionalLight(const std::vector<Light>& lights) {
  float max_intensity = -1.0f;
  int max_idx = -1;

  for (int i = 0; i < lights.size(); ++i) {
    if (lights[i].type == Light::Type::Directional) {
      if (lights[i].intensity > max_intensity) {
        max_intensity = lights[i].intensity;
        max_idx = i;
      }
    }
  }

  if (max_idx < 0) {
    return nullptr;
  }

  return &lights[max_idx];
}

}  // namespace

std::optional<Scene> LoadScene(const std::filesystem::path& gltf_file) {
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret = false;
  if (gltf_file.extension() == ".glb") {
    ret = loader.LoadBinaryFromFile(&model, &err, &warn, gltf_file.string());
  } else {
    ret = loader.LoadASCIIFromFile(&model, &err, &warn, gltf_file.string());
  }

  if (!warn.empty()) {
    DLOG(WARNING) << "TinyGLTF warning: " << warn;
  }
  if (!err.empty()) {
    LOG(ERROR) << "TinyGLTF error: " << err;
  }
  if (!ret) {
    LOG(INFO) << "LoadScene: tinygltf failed";
    return std::nullopt;
  }

  Scene scene;

  // Process Materials
  ProcessMaterials(model, gltf_file.parent_path(), &scene.materials);

  // Traverse Nodes to find Meshes/Punctual Lights
  const tinygltf::Scene& gltf_scene =
      model.scenes[model.defaultScene > -1 ? model.defaultScene : 0];
  for (int node_index : gltf_scene.nodes) {
    if (!TraverseNodes(model, node_index, Eigen::Affine3f::Identity(),
                       &scene)) {
      LOG(ERROR) << "Failed to process scene graph.";
      return std::nullopt;
    }
  }

  // Process Area Lights (from emissive materials)
  ProcessAreaLights(scene.materials, scene.geometries, &scene.lights);

  return scene;
}

}  // namespace sh_baker
