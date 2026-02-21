#pragma once

#include <vector>

#include "camera.h"
#include "scene.h"

namespace sh_renderer {

const unsigned kNumShadowMapCascades = 3;
const unsigned kCascadeShadowMapSize = 1024;

struct Cascade {
  // View-space depth limit for this cascade.
  float split_depth;

  // Light-space orthographic bounds.
  float near;
  float far;
  float left;
  float right;
  float bottom;
  float top;

  // World to light's shadow map space.
  Eigen::Matrix4f view_projection_matrix;
};

// Computes the cascade bounds for the given camera and the sun light.
std::vector<Cascade> ComputeCascades(const Light& sun_light,
                                     const Camera& camera);

}  // namespace sh_renderer