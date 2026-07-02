#pragma once

#include "camera.h"
#include "render_target.h"
#include "scene.h"
#include "shader.h"

namespace sh_renderer {

// Creates the additive (order-independent) transparent pass shader program.
ShaderProgram CreateAdditiveProgram();

// Renders the scene's additive transparent surfaces (materials flagged
// `additive` -- every layer blends with dst factor GL_ONE: flames, glows,
// plasma) into the lit HDR target. Tests against the opaque depth buffer but does
// not write it, and blends GL_ONE/GL_ONE; because additive blending is
// commutative no depth sorting is needed. `time` drives the layer compositor
// (animMap/tcMod/rgbGen). Run after the opaque radiance and sky passes.
void DrawAdditive(const Scene& scene, const Camera& camera,
                  const ShaderProgram& program, const RenderTarget& hdr_target,
                  float time);

}  // namespace sh_renderer
