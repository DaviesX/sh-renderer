#pragma once

#include <vector>

#include "camera.h"
#include "render_target.h"
#include "shader.h"

namespace sh_renderer {

ShaderProgram CreateSSAOProgram();
ShaderProgram CreateSSAOBlurProgram(bool horizontal);
ShaderProgram CreateSSAOVisualizerProgram();

struct SSAOContext {
  GLuint noise_texture = 0;
  std::vector<Eigen::Vector3f> kernel;
};

SSAOContext CreateSSAOContext();
void DestroySSAOContext(SSAOContext* ctx);

void DrawSSAO(const RenderTarget& depth_normal_target, const Camera& camera,
              const ShaderProgram& ssao_program, const SSAOContext& context,
              const RenderTarget& ssao_out);

void DrawSSAOBlur(const RenderTarget& ssao_in, const Camera& camera,
                  const ShaderProgram& blur_horizontal_program,
                  const ShaderProgram& blur_vertical_program,
                  const RenderTarget& depth_normal_target,
                  const RenderTarget& blur_temp, const RenderTarget& blur_out);

void DrawSSAOVisualization(const RenderTarget& ssao,
                           const ShaderProgram& program,
                           const RenderTarget& out = {});

}  // namespace sh_renderer
