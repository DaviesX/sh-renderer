#pragma once

#include "camera.h"
#include "render_target.h"
#include "scene.h"
#include "shader.h"

namespace sh_renderer {

const Eigen::Vector3f kSkyColor = Eigen::Vector3f(0.2, 0.5, 0.9);

/**
 * @brief Creates a skybox shader program.
 * @return The skybox shader program.
 */
ShaderProgram CreateSkyAnalyticProgram();

/**
 * @brief Draw the skybox to the render target.
 * @param sun_light The sun light.
 * @param target The render target to draw the skybox to.
 */
void DrawSkyAnalytic(const Scene& scene, const Camera& camera,
                     const SunLight& sun_light, const RenderTarget& target,
                     const ShaderProgram& program);

}  // namespace sh_renderer