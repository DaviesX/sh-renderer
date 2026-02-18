#pragma once

#include "camera.h"
#include "cascade.h"
#include "render_target.h"
#include "scene.h"
#include "shader.h"

namespace sh_renderer {

const unsigned kNumShadowMapCascades = 3;

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

}  // namespace sh_renderer
