#pragma once

#include "camera.h"
#include "scene.h"

namespace sh_renderer {

void DrawDepth(const Scene& scene, const Camera& camera);

}  // namespace sh_renderer