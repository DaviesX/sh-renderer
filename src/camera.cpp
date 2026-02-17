#include "camera.h"

#include <glog/logging.h>

#include <Eigen/Dense>

namespace sh_renderer {

void TranslateCamera(const Eigen::Vector3f& delta, Camera* camera) {
  // delta is in camera-local space: x=right, y=up, z=backward.
  // Transform to world space using the camera's orientation.
  camera->position += camera->orientation * delta;
}

void PanCamera(const float delta, Camera* camera) {
  // Rotate around the world Y-up axis (yaw).
  Eigen::Quaternionf yaw(Eigen::AngleAxisf(delta, Eigen::Vector3f::UnitY()));
  camera->orientation = yaw * camera->orientation;
  camera->orientation.normalize();
}

void TiltCamera(const float delta, Camera* camera) {
  // Rotate around the camera's local X-right axis (pitch).
  Eigen::Quaternionf pitch(Eigen::AngleAxisf(delta, Eigen::Vector3f::UnitX()));
  camera->orientation = camera->orientation * pitch;
  camera->orientation.normalize();
}

void RollCamera(const float delta, Camera* camera) {
  // Rotate around the camera's local Z-forward axis (roll).
  Eigen::Quaternionf roll(Eigen::AngleAxisf(delta, Eigen::Vector3f::UnitZ()));
  camera->orientation = camera->orientation * roll;
  camera->orientation.normalize();
}

void LookAt(const Eigen::Vector3f& target, Camera* camera) {
  Eigen::Vector3f forward = (target - camera->position).normalized();
  Eigen::Vector3f world_up = Eigen::Vector3f::UnitY();

  // Avoid degenerate case when looking straight up or down.
  if (std::abs(forward.dot(world_up)) > 0.999f) {
    world_up = Eigen::Vector3f::UnitZ();
  }

  Eigen::Vector3f right = forward.cross(world_up).normalized();
  Eigen::Vector3f up = right.cross(forward).normalized();

  // Build rotation matrix. OpenGL convention: camera looks down -Z.
  Eigen::Matrix3f rotation;
  rotation.col(0) = right;
  rotation.col(1) = up;
  rotation.col(2) = -forward;

  camera->orientation = Eigen::Quaternionf(rotation).normalized();
}

Eigen::Matrix4f GetViewMatrix(const Camera& camera) {
  // View matrix = inverse of camera transform = R^T * T(-pos)
  Eigen::Matrix3f rotation = camera.orientation.normalized().toRotationMatrix();
  Eigen::Vector3f translated = rotation.transpose() * (-camera.position);

  Eigen::Matrix4f view = Eigen::Matrix4f::Identity();
  view.block<3, 3>(0, 0) = rotation.transpose();
  view.block<3, 1>(0, 3) = translated;
  return view;
}

}  // namespace sh_renderer
