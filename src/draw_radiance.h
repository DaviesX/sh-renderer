#pragma once

#include <Eigen/Dense>

#include "scene.h"
#include "shader.h"

namespace sh_renderer {

// Creates a simple unlit shader program.
ShaderProgram CreateUnlitProgram();

// Draws the scene with a simple unlit shader.
void DrawSceneUnlit(const Scene& scene, const Eigen::Matrix4f& vp,
                    const ShaderProgram& program);

// TODO: Phase 3
ShaderProgram CreateRadianceProgram();

// TODO: Phase 3
void DrawSceneRadiance(const Scene& scene, const Eigen::Matrix4f& vp,
                       const ShaderProgram& program);

}  // namespace sh_renderer
