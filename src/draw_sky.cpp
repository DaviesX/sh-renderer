#include "draw_sky.h"

#include <glog/logging.h>

#include "camera.h"
#include "glad.h"

namespace sh_renderer {
namespace {

const char* kSkyVertex = "glsl/sky_analytic.vert";
const char* kSkyFragment = "glsl/sky_analytic.frag";

}  // namespace

ShaderProgram CreateSkyAnalyticProgram() {
  auto program = ShaderProgram::CreateGraphics(kSkyVertex, kSkyFragment);
  if (!program) {
    LOG(FATAL) << "Failed to create sky analytic shader program.";
    return {};
  }
  return std::move(*program);
}

void DrawSkyAnalytic(const Scene& scene, const Camera& camera,
                     const SunLight& sun_light, const RenderTarget& target,
                     const ShaderProgram& program) {
  if (!program) return;

  // Render to target
  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
  glViewport(0, 0, target.width, target.height);

  // Enable depth test, but only pass if depth == 1.0 (or LEQUAL since sky
  // is 1.0)
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);  // Don't write to depth buffer

  // Disable culling since we're drawing a fullscreen triangle
  glDisable(GL_CULL_FACE);

  program.Use();

  // Set uniforms
  program.Uniform("u_inv_view_proj",
                  GetViewProjMatrix(camera).inverse().eval());
  program.Uniform("u_camera_pos", camera.position);
  program.Uniform("u_sun_direction", sun_light.direction);
  program.Uniform("u_sky_color", kSkyColor);

  // Draw fullscreen triangle
  glDrawArrays(GL_TRIANGLES, 0, 3);

  // Restore states
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);
  glEnable(GL_CULL_FACE);
}

}  // namespace sh_renderer
