#pragma once

#include <Eigen/Dense>

#include "camera.h"
#include "cascade.h"
#include "compute_light_tile.h"
#include "render_target.h"
#include "scene.h"
#include "shader.h"

namespace sh_renderer {

// Creates an unlit shader program.
ShaderProgram CreateUnlitProgram();

// Draws the scene with an unlit shader.
void DrawSceneUnlit(const Scene& scene, const Camera& camera,
                    const ShaderProgram& program);

// Creates a radiance shader program (forward shading).
ShaderProgram CreateRadianceProgram();

// Draws the scene with a radiance shader (forward shading).
void DrawSceneRadiance(const Scene& scene, const Camera& camera,
                       const std::vector<RenderTarget>& sun_shadow_maps,
                       const std::vector<Cascade>& sun_cascades,
                       const RenderTarget& spot_shadow_atlas,
                       const TileLightListList& tile_light_list,
                       const ShaderProgram& program,
                       const RenderTarget& hdr_target);

}  // namespace sh_renderer
