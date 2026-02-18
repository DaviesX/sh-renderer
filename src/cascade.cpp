#include "cascade.h"

#include <Eigen/Geometry>
#include <algorithm>
#include <cmath>
#include <limits>

namespace sh_renderer {
namespace {

// Compute cascade splits using a mix of logarithmic and uniform distribution.
std::vector<float> CalculateCascadeSplits(float near, float far, int count,
                                          float lambda) {
  std::vector<float> splits(count + 1);
  splits[0] = near;
  for (int i = 1; i < count; ++i) {
    float p = static_cast<float>(i) / static_cast<float>(count);
    float log = near * std::pow(far / near, p);
    float uniform = near + (far - near) * p;
    splits[i] = lambda * log + (1.0f - lambda) * uniform;
  }
  splits[count] = far;
  return splits;
}

Eigen::Matrix4f LookAt(const Eigen::Vector3f& eye,
                       const Eigen::Vector3f& center,
                       const Eigen::Vector3f& up) {
  Eigen::Vector3f f = (center - eye).normalized();
  Eigen::Vector3f u = up.normalized();
  Eigen::Vector3f s = f.cross(u).normalized();
  u = s.cross(f);

  Eigen::Matrix4f res;
  res << s.x(), s.y(), s.z(), -s.dot(eye), u.x(), u.y(), u.z(), -u.dot(eye),
      -f.x(), -f.y(), -f.z(), f.dot(eye), 0, 0, 0, 1;
  return res;
}

Eigen::Matrix4f Ortho(float left, float right, float bottom, float top,
                      float zNear, float zFar) {
  Eigen::Matrix4f res = Eigen::Matrix4f::Identity();
  res(0, 0) = 2.0f / (right - left);
  res(1, 1) = 2.0f / (top - bottom);
  res(2, 2) = -2.0f / (zFar - zNear);
  res(0, 3) = -(right + left) / (right - left);
  res(1, 3) = -(top + bottom) / (top - bottom);
  res(2, 3) = -(zFar + zNear) / (zFar - zNear);
  return res;
}

}  // namespace

std::vector<Cascade> ComputeCascades(const Light& sun_light,
                                     const Camera& camera,
                                     unsigned num_cascades) {
  std::vector<Cascade> cascades;
  cascades.resize(num_cascades);

  // TODO: Make these configurable or pass them in.
  // Using a lambda of 0.5 for mix of log and uniform splits creates good
  // distribution for most scenes.
  const float lambda = 0.5f;

  std::vector<float> cascade_splits = CalculateCascadeSplits(
      camera.intrinsics.z_near, camera.intrinsics.z_far, num_cascades, lambda);

  // Calculate view-projection-inverse for the main camera to get frustum
  // corners. We need to rebuild projection matrices for each slice. However, a
  // simpler way is to compute points in view space and transform to world.

  Eigen::Vector3f forward = camera.orientation * Eigen::Vector3f(0, 0, -1);
  Eigen::Vector3f up_vec = camera.orientation * Eigen::Vector3f(0, 1, 0);
  Eigen::Matrix4f camera_view =
      LookAt(camera.position, camera.position + forward, up_vec);

  // Sun basis
  Eigen::Vector3f light_dir = sun_light.direction.normalized();
  // Ensure up vector is not parallel to light_dir
  Eigen::Vector3f light_up = Eigen::Vector3f(0, 1, 0);
  if (std::abs(light_dir.dot(light_up)) > 0.99f) {
    light_up = Eigen::Vector3f(0, 0, 1);
  }
  // We construct a view matrix centered at origin for the sun initially.
  // The position will be adjusted to center on the frustum.
  Eigen::Matrix4f light_view =
      LookAt(Eigen::Vector3f::Zero(), light_dir, light_up);

  for (unsigned i = 0; i < num_cascades; ++i) {
    float prev_split_dist = cascade_splits[i];
    float split_dist = cascade_splits[i + 1];

    // Compute frustum corners for this split in View Space.
    // 8 points: near plane (TL, TR, BL, BR), far plane (TL, TR, BL, BR).
    // Using simple trig if FOV is known, or re-constructing projection.
    // Let's use re-constructed projection.
    float aspect = camera.intrinsics.aspect_ratio;
    float tan_half_fov = std::tan(camera.intrinsics.fov_y_radians * 0.5f);

    float near_height = 2.0f * tan_half_fov * prev_split_dist;
    float near_width = near_height * aspect;
    float far_height = 2.0f * tan_half_fov * split_dist;
    float far_width = far_height * aspect;

    std::vector<Eigen::Vector3f> corners(8);
    // Near plane (z = -prev_split_dist)
    corners[0] =
        Eigen::Vector3f(-near_width / 2, near_height / 2, -prev_split_dist);
    corners[1] =
        Eigen::Vector3f(near_width / 2, near_height / 2, -prev_split_dist);
    corners[2] =
        Eigen::Vector3f(-near_width / 2, -near_height / 2, -prev_split_dist);
    corners[3] =
        Eigen::Vector3f(near_width / 2, -near_height / 2, -prev_split_dist);
    // Far plane (z = -split_dist)
    corners[4] = Eigen::Vector3f(-far_width / 2, far_height / 2, -split_dist);
    corners[5] = Eigen::Vector3f(far_width / 2, far_height / 2, -split_dist);
    corners[6] = Eigen::Vector3f(-far_width / 2, -far_height / 2, -split_dist);
    corners[7] = Eigen::Vector3f(far_width / 2, -far_height / 2, -split_dist);

    // Transform to Light Space.
    // First View -> World, then World -> Light.
    Eigen::Matrix4f cam_inv = camera_view.inverse();

    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();
    float min_z = std::numeric_limits<float>::max();
    float max_z = std::numeric_limits<float>::lowest();

    for (auto& corner : corners) {
      Eigen::Vector4f world_pos =
          cam_inv * Eigen::Vector4f(corner.x(), corner.y(), corner.z(), 1.0f);
      Eigen::Vector4f light_pos = light_view * world_pos;

      min_x = std::min(min_x, light_pos.x());
      max_x = std::max(max_x, light_pos.x());
      min_y = std::min(min_y, light_pos.y());
      max_y = std::max(max_y, light_pos.y());
      min_z = std::min(min_z, light_pos.z());
      max_z = std::max(max_z, light_pos.z());
    }

    // Stabilize shadow map (Texel snapping)
    // We assume a standard shadow map resolution per cascade.
    // Since we don't have the exact resolution passed in here easily,
    // we'll assume a safe default (e.g. 1024/2048) or dynamic based on cascade
    // index if known. Let's assume 1024 for now or pass it. Better: just snap
    // to a reasonably small unit to avoid sub-pixel jitter.
    float world_units_per_texel = (max_x - min_x) / 1024.0f;  // Approximate
    min_x = std::floor(min_x / world_units_per_texel) * world_units_per_texel;
    max_x = std::floor(max_x / world_units_per_texel) * world_units_per_texel;
    min_y = std::floor(min_y / world_units_per_texel) * world_units_per_texel;
    max_y = std::floor(max_y / world_units_per_texel) * world_units_per_texel;

    // Expand Z range to include potential occluders (e.g. objects between sun
    // and frustum). The "near" in light space corresponds to objects closer to
    // the light source. Since light looks down -Z, larger Z is further away. In
    // Light View Space, if we look down -Z, then positive Z is behind us (which
    // is confusing with typical ortho). Let's check LookAt convention: -Z is
    // forward. So objects closer to light have larger Z (less negative) than
    // objects further away? LookAt: Z axis is -(Target - Eye). So Z points
    // *towards* the eye. Light is at "0" looking at "Direction". If direction
    // is (0, -1, 0), then Z axis is (0, 1, 0). Points "below" the light have
    // negative Z locally? Let's stick to standard Ortho: near/far are distances
    // from eye. Ortho(l, r, b, t, n, f) -> maps [-n, -f] to [-1, 1]. So we need
    // to encompass [min_z, max_z] which are actual Z coordinates in light
    // space. If min_z = -100, max_z = -10, then we need near = 10, far = 100.
    // To include occluders, we need to decrease min_z (make it more negative /
    // larger distance). Wait, if Z points back towards light, then "closer to
    // light" is higher Z. Let's just use raw coords and Ortho projection matrix
    // manually.

    // We want to map [min_x, max_x] -> [-1, 1]
    // [min_y, max_y] -> [-1, 1]
    // [min_z - padding, max_z] -> [-1, 1] (or [0, 1] dep on depth range).
    // OpenGL clip Z is [-1, 1].
    // Let's widen the Z range significantly towards the light.
    float z_mult = 10.0f;
    if (min_z < 0)
      min_z *= z_mult;
    else
      min_z /= z_mult;
    if (max_z < 0)
      max_z /= z_mult;
    else
      max_z *= z_mult;

    // Let's just add a large buffer.
    min_z -= 200.0f;
    max_z += 50.0f;

    Eigen::Matrix4f ortho = Ortho(min_x, max_x, min_y, max_y, -max_z, -min_z);

    cascades[i].split_depth = split_dist;
    cascades[i].near = -max_z;  // Ortho near
    cascades[i].far = -min_z;   // Ortho far
    cascades[i].left = min_x;
    cascades[i].right = max_x;
    cascades[i].bottom = min_y;
    cascades[i].top = max_y;
    cascades[i].view_projection_matrix = ortho * light_view;
  }

  return cascades;
}

}  // namespace sh_renderer
