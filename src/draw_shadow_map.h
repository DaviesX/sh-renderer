#pragma once

#include "camera.h"
#include "cascade.h"
#include "render_target.h"
#include "scene.h"
#include "shader.h"

namespace sh_renderer {

// Creates a shadow map shader program.
ShaderProgram CreateShadowMapProgram();

// Creates cascaded shadow map targets.
std::vector<RenderTarget> CreateCascadedShadowMapTargets();

// Draws the cascaded shadow maps in the sun light's perspective over the
// camera's view frustum.
void DrawCascadedShadowMap(const Scene& scene, const Camera& camera,
                           const ShaderProgram& program,
                           const std::vector<Cascade>& cascades,
                           const std::vector<RenderTarget>& shadow_map_targets);

// Creates a shadow map visualization shader program.
ShaderProgram CreateShadowMapVisualizationProgram();

// Draw the visualization of the cascaded shadow maps.
void DrawCascadedShadowMapVisualization(
    const std::vector<RenderTarget>& shadow_map_targets, Eigen::Vector2i offset,
    Eigen::Vector2i size, const ShaderProgram& program,
    const RenderTarget& out = {});

}  // namespace sh_renderer
