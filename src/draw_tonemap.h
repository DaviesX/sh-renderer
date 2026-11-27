#pragma once

#include <string>

#include "render_target.h"
#include "shader.h"

namespace sh_renderer {

ShaderProgram CreateTonemapProgram(std::string* error = nullptr);

// Tonemaps `hdr_target` into framebuffer 0. `display_gamma` g != 1 raises the
// sRGB encoding e of the tonemapped colour to 1/g (the framebuffer then stores
// e^(1/g)); at 1 the output is the plain tonemap.
void DrawTonemap(const RenderTarget& hdr_target, const ShaderProgram& program,
                 float display_gamma = 1.0f);

}  // namespace sh_renderer
