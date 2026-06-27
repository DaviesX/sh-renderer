#pragma once

#include "camera.h"
#include "render_target.h"
#include "scene.h"
#include "shader.h"

namespace sh_renderer {

// Creates the depth pre-pass shader program for opaque materials.
ShaderProgram CreateDepthOpaqueProgram();

// Creates the depth pre-pass shader program for cutout materials.
ShaderProgram CreateDepthCutoutProgram();

// Creates the depth pre-pass shader program for opaque materials with
// view-space normal output.
ShaderProgram CreateDepthOpaqueWNormalProgram();

// Creates the depth pre-pass shader program for cutout materials with
// view-space normal output.
ShaderProgram CreateDepthCutoutWNormalProgram();

// Creates the depth visualization shader program.
ShaderProgram CreateDepthVisualizerProgram();

// Draws the scene to the depth buffer. `time` drives the layer compositor on
// the cutout path so its coverage matches the radiance pass.
void DrawDepth(const Scene& scene, const Camera& camera,
               const ShaderProgram& opaque_program,
               const ShaderProgram& cutout_program, const RenderTarget& target,
               float time);

// Draws the scene to the depth buffer and normal buffer. `time` drives the
// layer compositor on the cutout path so its coverage matches the radiance pass.
void DrawDepthWNormal(const Scene& scene, const Camera& camera,
                      const ShaderProgram& opaque_program,
                      const ShaderProgram& cutout_program,
                      const RenderTarget& target, float time);

// Draws the depth buffer to the output target (or screen) for visualization.
void DrawDepthVisualization(const RenderTarget& depth, const Camera& camera,
                            const ShaderProgram& program,
                            const RenderTarget& out = {});

}  // namespace sh_renderer