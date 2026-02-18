#pragma once

#include "render_target.h"
#include "shader.h"

namespace sh_renderer {

ShaderProgram CreateTonemapProgram();

void DrawTonemap(const RenderTarget& hdr_target, const ShaderProgram& program);

}  // namespace sh_renderer
