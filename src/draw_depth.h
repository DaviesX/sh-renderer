#pragma once

#include "camera.h"
#include "scene.h"
#include "shader.h"

namespace sh_renderer {

ShaderProgram CreateDepthProgram();

void DrawDepth(const Scene& scene, const Camera& camera);

}  // namespace sh_renderer