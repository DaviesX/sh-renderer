#include "draw_tonemap.h"

#include <string>

#include "glad.h"
#include "log.h"

namespace sh_renderer {

namespace {

const char* kTonemapVertex = "glsl/tonemap.vert";
const char* kTonemapFragment = "glsl/tonemap.frag";

}  // namespace

ShaderProgram CreateTonemapProgram() {
  std::string error;
  auto program = ShaderProgram::CreateGraphics(kTonemapVertex, kTonemapFragment,
                                               {}, /*reader=*/nullptr, &error);
  if (!program) {
    LOG(ERROR) << "Failed to create tonemap shader program (" << kTonemapVertex
               << ", " << kTonemapFragment << "): " << error;
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
