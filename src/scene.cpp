#include "scene.h"

#include <glog/logging.h>
#include <tinyexr.h>

#include <algorithm>
#include <cmath>

#include "camera.h"
#include "glad.h"

namespace sh_renderer {

namespace {

GLuint CreateTexture2D(const Texture& texture, bool srgb = true) {
  if (texture.width == 0 || texture.height == 0) return 0;

  GLuint tex;
  glCreateTextures(GL_TEXTURE_2D, 1, &tex);

  // Determine number of levels
  GLsizei levels = 1;
  // Generate mipmaps
  levels = 1 + static_cast<GLsizei>(std::floor(
                   std::log2(std::max(texture.width, texture.height))));

  GLenum internal_format = GL_RGBA8;
  if (texture.channels == 3) {
    internal_format = srgb ? GL_SRGB8 : GL_RGB8;
  } else if (texture.channels == 4) {
    internal_format = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
  } else if (texture.channels == 1) {
    internal_format = GL_R8;
  }

  glTextureStorage2D(tex, levels, internal_format, texture.width,
                     texture.height);

  GLenum format = GL_RGBA;
  if (texture.channels == 3)
    format = GL_RGB;
  else if (texture.channels == 1)
    format = GL_RED;

  if (!texture.pixel_data.empty()) {
    glTextureSubImage2D(tex, 0, 0, 0, texture.width, texture.height, format,
                        GL_UNSIGNED_BYTE, texture.pixel_data.data());
    glGenerateTextureMipmap(tex);
  }

  // Set parameters
  glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  if (GLAD_GL_ARB_texture_filter_anisotropic) {
    GLfloat max_anisotropy = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotropy);
    glTextureParameterf(tex, GL_TEXTURE_MAX_ANISOTROPY, max_anisotropy);
  }

  return tex;
}

GLuint CreateTexture2D(const Texture32F& texture) {
  if (texture.width == 0 || texture.height == 0) return 0;

  GLuint tex;
  glCreateTextures(GL_TEXTURE_2D, 1, &tex);

  // Determine number of levels
  GLsizei levels = 1;
  // Generate mipmaps
  levels = 1 + static_cast<GLsizei>(std::floor(
                   std::log2(std::max(texture.width, texture.height))));

  GLenum internal_format = GL_RGBA16F;
  if (texture.channels == 3) {
    internal_format = GL_RGB16F;
  } else if (texture.channels == 1) {
    internal_format = GL_R16F;
  }

  glTextureStorage2D(tex, levels, internal_format, texture.width,
                     texture.height);

  GLenum format = GL_RGBA;
  if (texture.channels == 3)
    format = GL_RGB;
  else if (texture.channels == 1)
    format = GL_RED;

  if (!texture.pixel_data.empty()) {
    glTextureSubImage2D(tex, 0, 0, 0, texture.width, texture.height, format,
                        GL_FLOAT, texture.pixel_data.data());
    glGenerateTextureMipmap(tex);
  }

  // Set parameters
  glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  if (GLAD_GL_ARB_texture_filter_anisotropic) {
    GLfloat max_anisotropy = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotropy);
    glTextureParameterf(tex, GL_TEXTURE_MAX_ANISOTROPY, max_anisotropy);
  }

  return tex;
}

void UploadGeometry(Geometry& geo) {
  if (geo.vertices.empty()) return;

  // Create VAO
  glCreateVertexArrays(1, &geo.vao);

  // Create VBO
  // Interleave data? Or separate buffers?
  // Let's use a single struct for vertex data usually, but here we have SOA in
  // struct Geometry. Let's start with separate buffers or interleave them
  // manually. Actually, standard is to use separate bindings or one large
  // buffer. Let's pack them into a single buffer for cache locality (AOS).
  // Format: Position (3f), Normal (3f), UV (2f), LightmapUV (2f), Tangent (4f)
  // stride = 12 + 12 + 8 + 8 + 16 = 56 bytes.

  struct Vertex {
    float pos[3];
    float norm[3];
    float uv[2];
    float luv[2];
    float tan[4];
  };

  std::vector<Vertex> vertices;
  vertices.reserve(geo.vertices.size());
  bool has_lightmap = !geo.lightmap_uvs.empty();

  for (size_t i = 0; i < geo.vertices.size(); ++i) {
    Vertex v;
    v.pos[0] = geo.vertices[i].x();
    v.pos[1] = geo.vertices[i].y();
    v.pos[2] = geo.vertices[i].z();

    if (i < geo.normals.size()) {
      v.norm[0] = geo.normals[i].x();
      v.norm[1] = geo.normals[i].y();
      v.norm[2] = geo.normals[i].z();
    } else {
      v.norm[0] = 0;
      v.norm[1] = 1;
      v.norm[2] = 0;
    }

    if (i < geo.texture_uvs.size()) {
      v.uv[0] = geo.texture_uvs[i].x();
      v.uv[1] = geo.texture_uvs[i].y();
    } else {
      v.uv[0] = 0;
      v.uv[1] = 0;
    }

    if (has_lightmap && i < geo.lightmap_uvs.size()) {
      v.luv[0] = geo.lightmap_uvs[i].x();
      v.luv[1] = geo.lightmap_uvs[i].y();
    } else {
      v.luv[0] = 0;
      v.luv[1] = 0;
    }

    if (i < geo.tangents.size()) {
      v.tan[0] = geo.tangents[i].x();
      v.tan[1] = geo.tangents[i].y();
      v.tan[2] = geo.tangents[i].z();
      v.tan[3] = geo.tangents[i].w();
    } else {
      v.tan[0] = 1;
      v.tan[1] = 0;
      v.tan[2] = 0;
      v.tan[3] = 1;
    }
    vertices.push_back(v);
  }

  glCreateBuffers(1, &geo.vbo);
  glNamedBufferStorage(geo.vbo, vertices.size() * sizeof(Vertex),
                       vertices.data(), 0);

  // EBO
  if (!geo.indices.empty()) {
    glCreateBuffers(1, &geo.ebo);
    glNamedBufferStorage(geo.ebo, geo.indices.size() * sizeof(uint32_t),
                         geo.indices.data(), 0);
    glVertexArrayElementBuffer(geo.vao, geo.ebo);
    geo.index_count = static_cast<uint32_t>(geo.indices.size());
  } else {
    geo.index_count = 0;
  }

  // Bindings
  const GLuint binding_index = 0;
  glVertexArrayVertexBuffer(geo.vao, binding_index, geo.vbo, 0, sizeof(Vertex));

  // Attributes
  // 0: Pos
  glEnableVertexArrayAttrib(geo.vao, 0);
  glVertexArrayAttribFormat(geo.vao, 0, 3, GL_FLOAT, GL_FALSE,
                            offsetof(Vertex, pos));
  glVertexArrayAttribBinding(geo.vao, 0, binding_index);

  // 1: Normal
  glEnableVertexArrayAttrib(geo.vao, 1);
  glVertexArrayAttribFormat(geo.vao, 1, 3, GL_FLOAT, GL_FALSE,
                            offsetof(Vertex, norm));
  glVertexArrayAttribBinding(geo.vao, 1, binding_index);

  // 2: UV
  glEnableVertexArrayAttrib(geo.vao, 2);
  glVertexArrayAttribFormat(geo.vao, 2, 2, GL_FLOAT, GL_FALSE,
                            offsetof(Vertex, uv));
  glVertexArrayAttribBinding(geo.vao, 2, binding_index);

  // 3: Lightmap UV
  glEnableVertexArrayAttrib(geo.vao, 3);
  glVertexArrayAttribFormat(geo.vao, 3, 2, GL_FLOAT, GL_FALSE,
                            offsetof(Vertex, luv));
  glVertexArrayAttribBinding(geo.vao, 3, binding_index);

  // 4: Tangent
  glEnableVertexArrayAttrib(geo.vao, 4);
  glVertexArrayAttribFormat(geo.vao, 4, 4, GL_FLOAT, GL_FALSE,
                            offsetof(Vertex, tan));
  glVertexArrayAttribBinding(geo.vao, 4, binding_index);
}

}  // namespace

void UploadSceneToGPU(Scene& scene) {
  // Upload Materials (Textures)
  // Upload Materials (Textures)
  for (auto& mat : scene.materials) {
    // Albedo (sRGB)
    if (mat.albedo.texture_id == 0 && !mat.albedo.pixel_data.empty()) {
      mat.albedo.texture_id = CreateTexture2D(mat.albedo, true);
    }
    // Normal (Linear)
    if (mat.normal_texture.texture_id == 0 &&
        !mat.normal_texture.pixel_data.empty()) {
      mat.normal_texture.texture_id =
          CreateTexture2D(mat.normal_texture, false);
    }
    // Metallic-Roughness (Linear)
    if (mat.metallic_roughness_texture.texture_id == 0 &&
        !mat.metallic_roughness_texture.pixel_data.empty()) {
      mat.metallic_roughness_texture.texture_id =
          CreateTexture2D(mat.metallic_roughness_texture, false);
    }
    // Emissive (sRGB)
    if (mat.emissive_texture && mat.emissive_texture->texture_id == 0 &&
        !mat.emissive_texture->pixel_data.empty()) {
      mat.emissive_texture->texture_id =
          CreateTexture2D(*mat.emissive_texture, true);
    }
  }

  // Upload Lightmaps
  for (int i = 0; i < 3; ++i) {
    if (scene.lightmaps_packed[i].texture_id == 0 &&
        !scene.lightmaps_packed[i].pixel_data.empty()) {
      scene.lightmaps_packed[i].texture_id =
          CreateTexture2D(scene.lightmaps_packed[i]);
    }
  }

  // Upload Geometry
  for (auto& geo : scene.geometries) {
    UploadGeometry(geo);
  }
}

float ComputeLightRadius(float intensity, const Eigen::Vector3f& color,
                         float threshold) {
  float flux = intensity * color.maxCoeff();
  if (flux <= 0.0f || threshold <= 0.0f) return 0.0f;
  return std::sqrt(flux / threshold);
}

void UploadLightsToGPU(Scene& scene) {
  // Point lights.
  {
    uint32_t count = static_cast<uint32_t>(scene.point_lights.size());

    // Header: uint32_t count, padded to 16 bytes for std430 vec4 alignment.
    size_t header_size = 16;
    size_t data_size = header_size + count * sizeof(GpuPointLight);

    std::vector<uint8_t> buffer(data_size, 0);
    std::memcpy(buffer.data(), &count, sizeof(uint32_t));

    auto* gpu_lights =
        reinterpret_cast<GpuPointLight*>(buffer.data() + header_size);
    for (uint32_t i = 0; i < count; ++i) {
      const auto& l = scene.point_lights[i];
      gpu_lights[i].position[0] = l.position.x();
      gpu_lights[i].position[1] = l.position.y();
      gpu_lights[i].position[2] = l.position.z();
      gpu_lights[i].radius = l.radius;
      gpu_lights[i].color[0] = l.color.x();
      gpu_lights[i].color[1] = l.color.y();
      gpu_lights[i].color[2] = l.color.z();
      gpu_lights[i].intensity = l.intensity;
    }

    if (scene.point_light_list_ssbo.id != 0 &&
        scene.point_light_list_ssbo.size >= data_size) {
      UpdateSSBO(scene.point_light_list_ssbo, buffer.data(), data_size);
    } else {
      if (scene.point_light_list_ssbo.id != 0) {
        DestroySSBO(scene.point_light_list_ssbo);
      }
      scene.point_light_list_ssbo = CreateSSBO(buffer.data(), data_size);
    }
  }

  // Spot lights.
  {
    uint32_t count = static_cast<uint32_t>(scene.spot_lights.size());

    size_t header_size = 16;
    size_t data_size = header_size + count * sizeof(GpuSpotLight);

    std::vector<uint8_t> buffer(data_size, 0);
    std::memcpy(buffer.data(), &count, sizeof(uint32_t));

    auto* gpu_lights =
        reinterpret_cast<GpuSpotLight*>(buffer.data() + header_size);
    for (uint32_t i = 0; i < count; ++i) {
      const auto& l = scene.spot_lights[i];
      gpu_lights[i].position[0] = l.position.x();
      gpu_lights[i].position[1] = l.position.y();
      gpu_lights[i].position[2] = l.position.z();
      gpu_lights[i].radius = l.radius;
      gpu_lights[i].direction[0] = l.direction.x();
      gpu_lights[i].direction[1] = l.direction.y();
      gpu_lights[i].direction[2] = l.direction.z();
      gpu_lights[i].intensity = l.intensity;
      gpu_lights[i].color[0] = l.color.x();
      gpu_lights[i].color[1] = l.color.y();
      gpu_lights[i].color[2] = l.color.z();
      gpu_lights[i].cos_inner_cone = l.cos_inner_cone;
      gpu_lights[i].cos_outer_cone = l.cos_outer_cone;
      gpu_lights[i].has_shadow = l.has_shadow;
      gpu_lights[i].shadow_uv_offset[0] = l.shadow_uv_offset.x();
      gpu_lights[i].shadow_uv_offset[1] = l.shadow_uv_offset.y();
      gpu_lights[i].shadow_uv_scale[0] = l.shadow_uv_scale.x();
      gpu_lights[i].shadow_uv_scale[1] = l.shadow_uv_scale.y();
      gpu_lights[i]._pad[0] = 0.0f;
      gpu_lights[i]._pad[1] = 0.0f;

      const float* mat_data = l.shadow_view_proj.data();
      for (int m = 0; m < 16; ++m) {
        gpu_lights[i].shadow_view_proj[m] = mat_data[m];
      }
    }

    if (scene.spot_light_list_ssbo.id != 0 &&
        scene.spot_light_list_ssbo.size >= data_size) {
      UpdateSSBO(scene.spot_light_list_ssbo, buffer.data(), data_size);
    } else {
      if (scene.spot_light_list_ssbo.id != 0) {
        DestroySSBO(scene.spot_light_list_ssbo);
      }
      scene.spot_light_list_ssbo = CreateSSBO(buffer.data(), data_size);
    }
  }
}

void AllocateShadowMapForLights(Scene& scene, const Camera& camera) {
  Eigen::Matrix4f view_proj = GetViewProjMatrix(camera);

  // Extract 6 frustum planes from view_proj matrix.
  // Planes are in form (A, B, C, D) where Ax + By + Cz + D = 0.
  // Each row i corresponds to mat[i].
  Eigen::Vector4f planes[6];
  // Left
  planes[0] = view_proj.row(3) + view_proj.row(0);
  // Right
  planes[1] = view_proj.row(3) - view_proj.row(0);
  // Bottom
  planes[2] = view_proj.row(3) + view_proj.row(1);
  // Top
  planes[3] = view_proj.row(3) - view_proj.row(1);
  // Near
  planes[4] = view_proj.row(3) + view_proj.row(2);
  // Far
  planes[5] = view_proj.row(3) - view_proj.row(2);

  for (int i = 0; i < 6; ++i) {
    planes[i].normalize();
  }

  // Struct for sorting.
  struct LightRank {
    int index;
    float importance;
  };
  std::vector<LightRank> ranked_lights;

  for (size_t i = 0; i < scene.spot_lights.size(); ++i) {
    auto& light = scene.spot_lights[i];
    light.has_shadow = 0;  // Reset.

    // Frustum culling (sphere vs planes)
    bool in_frustum = true;
    for (int p = 0; p < 6; ++p) {
      float dist = planes[p].x() * light.position.x() +
                   planes[p].y() * light.position.y() +
                   planes[p].z() * light.position.z() + planes[p].w();
      if (dist < -light.radius) {
        in_frustum = false;
        break;
      }
    }

    if (!in_frustum) continue;

    // Rank by flux / distance^2. If distance is very small, cap it.
    float dist = (light.position - camera.position).norm();
    dist = std::max(dist, 0.1f);
    float flux = light.intensity * light.color.maxCoeff();
    float importance = flux / (dist * dist);

    ranked_lights.push_back({static_cast<int>(i), importance});
  }

  // Sort by highest importance.
  std::sort(ranked_lights.begin(), ranked_lights.end(),
            [](const LightRank& a, const LightRank& b) {
              return a.importance > b.importance;
            });

  // Log top ranking.
  for (size_t rank = 0; rank < ranked_lights.size() && rank < 10; ++rank) {
    int idx = ranked_lights[rank].index;
    auto& light = scene.spot_lights[idx];

    // Atlas Size: 2048
    float size = 0.0f;
    Eigen::Vector2f offset(0.0f, 0.0f);

    if (rank < 2) {
      size = 1024.0f;
      offset.x() = rank * 1024.0f;
      offset.y() = 0.0f;
    } else if (rank < 6) {
      size = 512.0f;
      int r2 = rank - 2;
      offset.x() = (r2 % 2) * 512.0f;
      offset.y() = 1024.0f + std::floor(r2 / 2.0f) * 512.0f;
    } else if (rank < 22) {
      size = 256.0f;
      int r16 = rank - 6;
      offset.x() = 1024.0f + (r16 % 4) * 256.0f;
      offset.y() = 1024.0f + std::floor(r16 / 4.0f) * 256.0f;
    }

    if (size > 0.0f) {
      light.has_shadow = 1;
      light.shadow_uv_offset = offset / 2048.0f;
      light.shadow_uv_scale = Eigen::Vector2f(size, size) / 2048.0f;
    }
  }
}

// ... Existing functions ...
std::vector<Eigen::Vector3f> TransformedVertices(const Geometry& geometry) {
  std::vector<Eigen::Vector3f> out;
  out.reserve(geometry.vertices.size());
  for (const auto& v : geometry.vertices) {
    out.push_back(geometry.transform * v);
  }
  return out;
}

std::vector<Eigen::Vector3f> TransformedNormals(const Geometry& geometry) {
  std::vector<Eigen::Vector3f> out;
  out.reserve(geometry.normals.size());
  Eigen::Matrix3f normal_mat =
      geometry.transform.linear().inverse().transpose();
  for (const auto& n : geometry.normals) {
    out.push_back((normal_mat * n).normalized());
  }
  return out;
}

std::vector<Eigen::Vector4f> TransformedTangents(const Geometry& geometry) {
  return geometry.tangents;  // Placeholder, assuming rigid transform handling
                             // elsewhere or not needed on CPU
}

float SurfaceArea(const Geometry& geometry) {
  // Basic implementation for area lights
  // Only works if indexed triangles
  float area = 0.0f;
  for (size_t i = 0; i < geometry.indices.size(); i += 3) {
    const auto& v0 = geometry.vertices[geometry.indices[i]];
    const auto& v1 = geometry.vertices[geometry.indices[i + 1]];
    const auto& v2 = geometry.vertices[geometry.indices[i + 2]];

    // Transform? The function signature implies local space, but usually we
    // want world area. Let's assume input geometry is local, so we apply
    // transform.
    auto p0 = geometry.transform * v0;
    auto p1 = geometry.transform * v1;
    auto p2 = geometry.transform * v2;

    area += 0.5f * (p1 - p0).cross(p2 - p0).norm();
  }
  return area;
}

void LoadLightmaps(Scene& scene, const std::filesystem::path& gltf_file) {
  std::filesystem::path base_path = gltf_file.parent_path();
  std::string file_names[3] = {"lightmap_packed_0.exr", "lightmap_packed_1.exr",
                               "lightmap_packed_2.exr"};

  for (int i = 0; i < 3; ++i) {
    std::string path = (base_path / file_names[i]).string();
    float* out_rgba = nullptr;
    int width;
    int height;
    const char* err = nullptr;

    int ret = LoadEXR(&out_rgba, &width, &height, path.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
      if (err) {
        LOG(ERROR) << "LoadEXR error: " << err << " for file: " << path;
        FreeEXRErrorMessage(err);
      } else {
        LOG(ERROR) << "Failed to load EXR: " << path;
      }

      // Fallback
      scene.lightmaps_packed[i].width = 1;
      scene.lightmaps_packed[i].height = 1;
      scene.lightmaps_packed[i].channels = 4;
      scene.lightmaps_packed[i].pixel_data.resize(4);
      if (i == 0) {
        scene.lightmaps_packed[i].pixel_data = {0.0f, 0.0f, 0.0f, 1.0f};
      } else {
        scene.lightmaps_packed[i].pixel_data = {0.0f, 0.0f, 0.0f, 0.0f};
      }
    } else {
      scene.lightmaps_packed[i].width = width;
      scene.lightmaps_packed[i].height = height;
      scene.lightmaps_packed[i].channels = 4;
      scene.lightmaps_packed[i].pixel_data.assign(
          out_rgba, out_rgba + width * height * 4);
      free(out_rgba);
    }
  }
}

}  // namespace sh_renderer
