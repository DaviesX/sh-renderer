#pragma once

#include <vector>

#include "camera.h"
#include "render_target.h"
#include "shader.h"

namespace sh_renderer {

ShaderProgram CreateSSAOProgram();
ShaderProgram CreateSSAOBlurProgram();

struct SSAOContext {
  GLuint noise_texture = 0;
  std::vector<Eigen::Vector3f> kernel;
};

SSAOContext CreateSSAOContext();
void DestroySSAOContext(SSAOContext* ctx);

void DrawSSAO(const RenderTarget& depth_normal_target, const Camera& camera,
              const ShaderProgram& ssao_program, const SSAOContext& context,
              const RenderTarget& ssao_out);

void DrawSSAOBlur(const RenderTarget& ssao_in,
                  const ShaderProgram& blur_program,
                  const RenderTarget& blur_out);

}  // namespace sh_renderer
