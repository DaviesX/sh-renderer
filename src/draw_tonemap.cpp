#include "draw_tonemap.h"

#include <glog/logging.h>

#include "glad.h"

namespace sh_renderer {

namespace {

const char* kTonemapVertex = "glsl/tonemap.vert";
const char* kTonemapFragment = "glsl/tonemap.frag";

}  // namespace

ShaderProgram CreateTonemapProgram() {
  auto program =
      ShaderProgram::CreateGraphics(kTonemapVertex, kTonemapFragment);
  if (!program) {
    LOG(FATAL) << "Failed to create tonemap shader program.";
    return {};
  }
  return std::move(*program);
}

void DrawTonemap(const RenderTarget& hdr_target, const ShaderProgram& program) {
  if (!program) return;

  // Back to default framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  program.Use();

  glBindTextureUnit(0, hdr_target.texture);

  // Draw full screen triangle
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

}  // namespace sh_renderer
