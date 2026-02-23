#include "scene.h"

#include <glog/logging.h>
#include <tinyexr.h>

#include <algorithm>
#include <cmath>

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
