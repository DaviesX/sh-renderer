#pragma once

#include <Eigen/Dense>

namespace sh_renderer {

// Axis-Aligned Bounding Box.
struct AABB {
  Eigen::Vector3f min = Eigen::Vector3f(std::numeric_limits<float>::infinity(),
                                        std::numeric_limits<float>::infinity(),
                                        std::numeric_limits<float>::infinity());
  Eigen::Vector3f max =
      Eigen::Vector3f(-std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity());
};

// Extracts the 6 frustum planes from a view-projection matrix.
// The planes are in the form (A, B, C, D) where Ax + By + Cz + D = 0.
// Planes are normalized.
// Order: Left, Right, Bottom, Top, Near, Far.
void ExtractFrustumPlanes(const Eigen::Matrix4f& vp, Eigen::Vector4f planes[6]);

// Checks if an AABB is inside or intersecting a frustum.
// True means the AABB should be rendered.
bool IsAABBInFrustum(const AABB& aabb, const Eigen::Vector4f planes[6]);

}  // namespace sh_renderer
