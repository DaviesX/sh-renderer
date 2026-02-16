#include "camera.h"

#include <glog/logging.h>

#include <Eigen/Dense>

namespace sh_renderer {

// TODO: Implement camera functions.

void TranslateCamera(const Eigen::Vector3f& delta, Camera* camera) {}

void PanCamera(const float delta, Camera* camera) {}

void TiltCamera(const float delta, Camera* camera) {}

void RollCamera(const float delta, Camera* camera) {}

void LookAt(const Eigen::Vector3f& target, Camera* camera);

Eigen::Matrix4f GetViewMatrix(const Camera& camera) {}

}  // namespace sh_renderer
