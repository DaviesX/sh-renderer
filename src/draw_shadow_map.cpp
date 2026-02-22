#include "draw_shadow_map.h"

#include <glog/logging.h>

#include "camera.h"
#include "cascade.h"
#include "glad.h"
#include "render_target.h"
#include "scene.h"
#include "shader.h"

namespace sh_renderer {

ShaderProgram CreateShadowMapOpaqueProgram() {
  auto program =
      ShaderProgram::CreateGraphics("glsl/shadow.vert", "glsl/shadow.frag");
  if (!program) {
    LOG(ERROR) << "Failed to create opaque shadow map program.";
    return {};
  }
  return std::move(*program);
}

ShaderProgram CreateShadowMapCutoutProgram() {
  auto program = ShaderProgram::CreateGraphics("glsl/depth_cutout.vert",
                                               "glsl/depth_cutout.frag");
  if (!program) {
    LOG(ERROR) << "Failed to create cutout shadow map program.";
    return {};
  }
  return std::move(*program);
}

std::vector<RenderTarget> CreateCascadedShadowMapTargets() {
  std::vector<RenderTarget> targets;
  targets.reserve(kNumShadowMapCascades);

  for (unsigned i = 0; i < kNumShadowMapCascades; ++i) {
    RenderTarget target;
    target.width = kCascadeShadowMapSize;
    target.height = kCascadeShadowMapSize;

    // Create Depth Texture
    glCreateTextures(GL_TEXTURE_2D, 1, &target.depth_buffer);
    glTextureStorage2D(target.depth_buffer, 1, GL_DEPTH_COMPONENT32F,
                       target.width, target.height);

    // Set parameters for shadow mapping (PCF comparison)
    glTextureParameteri(target.depth_buffer, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(target.depth_buffer, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(target.depth_buffer, GL_TEXTURE_WRAP_S,
                        GL_CLAMP_TO_BORDER);
    glTextureParameteri(target.depth_buffer, GL_TEXTURE_WRAP_T,
                        GL_CLAMP_TO_BORDER);
    glTextureParameteri(target.depth_buffer, GL_TEXTURE_COMPARE_MODE,
                        GL_COMPARE_REF_TO_TEXTURE);
    glTextureParameteri(target.depth_buffer, GL_TEXTURE_COMPARE_FUNC,
                        GL_LEQUAL);

    float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTextureParameterfv(target.depth_buffer, GL_TEXTURE_BORDER_COLOR,
                         border_color);

    // Create FBO
    glCreateFramebuffers(1, &target.fbo);
    glNamedFramebufferTexture(target.fbo, GL_DEPTH_ATTACHMENT,
                              target.depth_buffer, 0);
    // No color buffer
    glNamedFramebufferDrawBuffer(target.fbo, GL_NONE);
    glNamedFramebufferReadBuffer(target.fbo, GL_NONE);

    if (glCheckNamedFramebufferStatus(target.fbo, GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE) {
      LOG(ERROR) << "Shadow map FBO " << i << " is incomplete.";
    }

    targets.push_back(target);
  }

  return targets;
}

void DrawCascadedShadowMap(
    const Scene& scene, const Camera& camera,
    const ShaderProgram& opaque_program, const ShaderProgram& cutout_program,
    const std::vector<Cascade>& cascades,
    const std::vector<RenderTarget>& shadow_map_targets) {
  if (!opaque_program || !cutout_program) return;
  if (cascades.size() != shadow_map_targets.size()) {
    LOG_EVERY_N(ERROR, 100)
        << "Mismatch between cascades and shadow map targets size.";
    return;
  }

  // Enable Depth Test
  glEnable(GL_DEPTH_TEST);
  // glDepthFunc(GL_LEQUAL) is standard for depth passes if clearing to 1.0.
  glDepthFunc(GL_LEQUAL);
  // Disable Color mask
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  // Important: Face culling. Standard for shadows is often Front Face culling
  // to solve peter-panning, or standard Back Face with bias. Use Back Face
  // culling (default) with bias, or Front Face. Front Face culling is robust
  // for closed meshes.
  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);

  std::vector<const Geometry*> opaque_geos;
  std::vector<const Geometry*> cutout_geos;
  opaque_geos.reserve(scene.geometries.size());
  cutout_geos.reserve(scene.geometries.size());

  for (const auto& geo : scene.geometries) {
    if (geo.vao == 0) continue;
    if (geo.material_id >= 0 &&
        static_cast<size_t>(geo.material_id) < scene.materials.size()) {
      const auto& mat = scene.materials[geo.material_id];
      if (mat.alpha_cutout) {
        cutout_geos.push_back(&geo);
      } else {
        opaque_geos.push_back(&geo);
      }
    } else {
      opaque_geos.push_back(&geo);
    }
  }

  for (size_t i = 0; i < cascades.size(); ++i) {
    const auto& cascade = cascades[i];
    const auto& target = shadow_map_targets[i];

    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
    glViewport(0, 0, target.width, target.height);
    glClear(GL_DEPTH_BUFFER_BIT);

    opaque_program.Use();
    opaque_program.Uniform("u_view_projection_matrix",
                           cascade.view_projection_matrix);
    for (const Geometry* geo_ptr : opaque_geos) {
      const Geometry& geo = *geo_ptr;
      opaque_program.Uniform("u_model", geo.transform.matrix());
      glBindVertexArray(geo.vao);
      if (geo.index_count > 0) {
        glDrawElements(GL_TRIANGLES, geo.index_count, GL_UNSIGNED_INT, nullptr);
      } else {
        glDrawArrays(GL_TRIANGLES, 0, geo.vertices.size());
      }
    }

    cutout_program.Use();
    cutout_program.Uniform("u_view_proj", cascade.view_projection_matrix);
    for (const Geometry* geo_ptr : cutout_geos) {
      const Geometry& geo = *geo_ptr;
      cutout_program.Uniform("u_model", geo.transform.matrix());

      if (geo.material_id >= 0 &&
          static_cast<size_t>(geo.material_id) < scene.materials.size()) {
        const auto& mat = scene.materials[geo.material_id];
        glBindTextureUnit(0, mat.albedo.texture_id);
      } else {
        glBindTextureUnit(0, 0);
      }

      glBindVertexArray(geo.vao);

      if (geo.index_count > 0) {
        glDrawElements(GL_TRIANGLES, geo.index_count, GL_UNSIGNED_INT, nullptr);
      } else {
        glDrawArrays(GL_TRIANGLES, 0, geo.vertices.size());
      }
    }
  }

  // Restore state
  glCullFace(GL_BACK);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

namespace {

// Fullscreen quad geometry
const float kQuadVertices[] = {
    // positions        // uvs
    -1.0f, 1.0f, 0.0f, 0.0f,  1.0f, -1.0f, -1.0f, 0.0f,
    0.0f,  0.0f, 1.0f, -1.0f, 0.0f, 1.0f,  0.0f,

    -1.0f, 1.0f, 0.0f, 0.0f,  1.0f, 1.0f,  -1.0f, 0.0f,
    1.0f,  0.0f, 1.0f, 1.0f,  0.0f, 1.0f,  1.0f};

GLuint GetQuadVAO() {
  static GLuint vao = 0;
  if (vao == 0) {
    GLuint vbo;
    glCreateVertexArrays(1, &vao);
    glCreateBuffers(1, &vbo);
    glNamedBufferStorage(vbo, sizeof(kQuadVertices), kQuadVertices, 0);

    // Position
    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(vao, 0, 0);

    // UV
    glEnableVertexArrayAttrib(vao, 2);  // location 2 as per fullscreen.vert
    glVertexArrayAttribFormat(vao, 2, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    glVertexArrayAttribBinding(vao, 2, 0);

    glVertexArrayVertexBuffer(vao, 0, vbo, 0, 5 * sizeof(float));
  }
  return vao;
}

}  // namespace

ShaderProgram CreateShadowMapVisualizationProgram() {
  auto program = ShaderProgram::CreateGraphics("glsl/fullscreen.vert",
                                               "glsl/shadow_vis.frag");
  if (!program) {
    LOG(ERROR) << "Failed to create shadow map visualization program.";
    return {};
  }
  return std::move(*program);
}

void DrawCascadedShadowMapVisualization(
    const std::vector<RenderTarget>& shadow_map_targets, Eigen::Vector2i offset,
    Eigen::Vector2i size, const ShaderProgram& program,
    const RenderTarget& out) {
  if (!program) return;
  if (shadow_map_targets.empty()) return;

  glBindFramebuffer(GL_FRAMEBUFFER, out.fbo);
  glDisable(GL_DEPTH_TEST);

  program.Use();
  program.Uniform("u_shadow_map", 0);

  int x = offset.x();
  for (size_t i = 0; i < shadow_map_targets.size(); ++i) {
    const auto& target = shadow_map_targets[i];

    // Temporarily disable comparison mode so we can sample raw depth.
    glTextureParameteri(target.depth_buffer, GL_TEXTURE_COMPARE_MODE, GL_NONE);

    glViewport(x, offset.y(), size.x(), size.y());
    glBindTextureUnit(0, target.depth_buffer);

    glBindVertexArray(GetQuadVAO());
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Restore comparison mode for shadow mapping.
    glTextureParameteri(target.depth_buffer, GL_TEXTURE_COMPARE_MODE,
                        GL_COMPARE_REF_TO_TEXTURE);

    x += size.x();
  }
}

}  // namespace sh_renderer