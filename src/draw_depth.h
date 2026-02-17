#pragma once

#include "camera.h"
#include "render_target.h"
#include "scene.h"
#include "shader.h"

namespace sh_renderer {

// Creates the depth pre-pass shader program.
ShaderProgram CreateDepthProgram();

// Creates the depth visualization shader program.
ShaderProgram CreateDepthVisualizerProgram();

// Draws the scene to the depth buffer.
void DrawDepth(const Scene& scene, const Camera& camera,
               const ShaderProgram& program, const RenderTarget& target);

// Draws the depth buffer to the output target (or screen) for visualization.
void DrawDepthVisualization(const RenderTarget& depth, const Camera& camera,
                            const ShaderProgram& program,
                            const RenderTarget& out = {});

}  // namespace sh_renderer