#pragma once

#include <Eigen/Dense>

#include "scene.h"

namespace sh_renderer {

void DrawRadiance(const Scene& scene, const Eigen::Matrix4f& vp,
                  const Eigen::Vector3f& cam_pos);

}  // namespace sh_renderer
