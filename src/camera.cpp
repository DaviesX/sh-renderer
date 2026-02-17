#include "camera.h"

#include <Eigen/Geometry>
#include <cmath>

namespace sh_renderer {

void TranslateCamera(const Eigen::Vector3f& delta, Camera* camera) {
  // Translate in local space: apply orientation to delta.
  camera->position += camera->orientation * delta;
}

void PanCamera(const float delta, Camera* camera) {
  // Pan around +Y axis (World Up).
  // Global rotation: q_new = rot_y * q_old
  Eigen::Quaternionf q =
      Eigen::AngleAxisf(delta, Eigen::Vector3f::UnitY()) * camera->orientation;
  camera->orientation = q.normalized();
}

void TiltCamera(const float delta, Camera* camera) {
  // Tilt around local +X axis (Right).
  // Local rotation: q_new = q_old * rot_x
  Eigen::Quaternionf q =
      camera->orientation * Eigen::AngleAxisf(delta, Eigen::Vector3f::UnitX());
  camera->orientation = q.normalized();
}

void RollCamera(const float delta, Camera* camera) {
  // Roll around local +Z axis (Backward) to match tests which expect +Y -> -X
  // for positive rotation. Alternatively, this is CW around -Z.
  Eigen::Quaternionf q =
      camera->orientation * Eigen::AngleAxisf(delta, Eigen::Vector3f::UnitZ());
  camera->orientation = q.normalized();
}

void LookAt(const Eigen::Vector3f& target, Camera* camera) {
  Eigen::Matrix3f rotation_matrix = Eigen::Matrix3f::Identity();
  Eigen::Vector3f forward = (target - camera->position).normalized();

  if (forward.isZero()) {
    forward = -Eigen::Vector3f::UnitZ();  // Default forward
  }

  Eigen::Vector3f world_up = Eigen::Vector3f::UnitY();

  // Handle singularity if forward is parallel to world_up
  if (std::abs(forward.dot(world_up)) > 0.999f) {
    // If looking up or down, standard right calculation fails.
    // Choose arbitrary "up" to calculate right.
    // If looking +Y, Forward=(0,1,0). Right? Let's say +X.
    // If looking -Y, Forward=(0,-1,0). Right? +X.
    // Let's use UnitZ as temporary up to compute Right.
    // But standard LookAt usually preserves Right as +X when Looking Y?
    // Let's force Right = +X.
    Eigen::Vector3f right = Eigen::Vector3f::UnitX();
    Eigen::Vector3f up = right.cross(forward).normalized();
    rotation_matrix.col(0) = right;
    rotation_matrix.col(1) = up;
    rotation_matrix.col(2) = -forward;
  } else {
    Eigen::Vector3f right = forward.cross(world_up).normalized();
    Eigen::Vector3f up = right.cross(forward).normalized();

    rotation_matrix.col(0) = right;
    rotation_matrix.col(1) = up;
    rotation_matrix.col(2) = -forward;
  }

  camera->orientation = Eigen::Quaternionf(rotation_matrix);
}

Eigen::Matrix4f GetViewMatrix(const Camera& camera) {
  Eigen::Matrix4f view = Eigen::Matrix4f::Identity();
  view.block<3, 3>(0, 0) = camera.orientation.toRotationMatrix().transpose();
  view.block<3, 1>(0, 3) =
      -(camera.orientation.toRotationMatrix().transpose() * camera.position);
  return view;
}

Eigen::Matrix4f GetProjectionMatrix(float fov_y_radians, float aspect_ratio,
                                    float z_near, float z_far) {
  Eigen::Matrix4f projection = Eigen::Matrix4f::Zero();
  float tan_half_fov = std::tan(fov_y_radians / 2.0f);

  projection(0, 0) = 1.0f / (aspect_ratio * tan_half_fov);
  projection(1, 1) = 1.0f / tan_half_fov;
  projection(2, 2) = -(z_far + z_near) / (z_far - z_near);
  projection(2, 3) = -(2.0f * z_far * z_near) / (z_far - z_near);
  projection(3, 2) = -1.0f;

  return projection;
}

}  // namespace sh_renderer
