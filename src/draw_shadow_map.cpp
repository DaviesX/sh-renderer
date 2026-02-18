#include "draw_shadow_map.h"

#include <glog/logging.h>

#include "camera.h"
#include "cascade.h"
#include "glad.h"
#include "render_target.h"
#include "scene.h"
#include "shader.h"

namespace sh_renderer {
namespace {

const unsigned kCascadeShadowMapSizes[kNumShadowMapCascades] = {
    1024, 512, 256};  // Powers of two for best performance.
const float kCascadeSplitProportions[kNumShadowMapCascades] = {
    0.1f, 0.3f, 0.6f};  // Ratios for cascade splits.

}  // namespace

ShaderProgram CreateShadowMapProgram() {
  auto program =
      ShaderProgram::CreateGraphics("glsl/shadow.vert", "glsl/shadow.frag");
  if (!program) {
    LOG(ERROR) << "Failed to create shadow map program.";
    return {};
  }
  return std::move(*program);
}

std::vector<RenderTarget> CreateCascadedShadowMapTargets() {
  std::vector<RenderTarget> targets;
  targets.reserve(kNumShadowMapCascades);

  for (unsigned i = 0; i < kNumShadowMapCascades; ++i) {
    RenderTarget target;
    target.width = kCascadeShadowMapSizes[i];
    target.height = kCascadeShadowMapSizes[i];

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
    const Scene& scene, const Camera& camera, const ShaderProgram& program,
    const std::vector<Cascade>& cascades,
    const std::vector<RenderTarget>& shadow_map_targets) {
  if (!program) return;
  if (cascades.size() != shadow_map_targets.size()) {
    LOG_EVERY_N(ERROR, 100)
        << "Mismatch between cascades and shadow map targets size.";
    return;
  }

  program.Use();

  // We need to render the scene into each cascade shadow map.
  // Optimization: We could cull objects per cascade, but for now draw all.

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

  for (size_t i = 0; i < cascades.size(); ++i) {
    const auto& cascade = cascades[i];
    const auto& target = shadow_map_targets[i];

    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
    glViewport(0, 0, target.width, target.height);
    glClear(GL_DEPTH_BUFFER_BIT);

    program.Uniform("u_view_projection_matrix", cascade.view_projection_matrix);

    for (const auto& geo : scene.geometries) {
      if (geo.vao == 0) continue;

      program.Uniform("u_model", geo.transform.matrix());
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

}  // namespace sh_renderer