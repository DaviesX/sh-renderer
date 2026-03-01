#include "culling.h"

namespace sh_renderer {

void ExtractFrustumPlanes(const Eigen::Matrix4f& vp,
                          Eigen::Vector4f planes[6]) {
  // Left
  planes[0] = vp.row(3) + vp.row(0);
  // Right
  planes[1] = vp.row(3) - vp.row(0);
  // Bottom
  planes[2] = vp.row(3) + vp.row(1);
  // Top
  planes[3] = vp.row(3) - vp.row(1);
  // Near
  planes[4] = vp.row(3) + vp.row(2);
  // Far
  planes[5] = vp.row(3) - vp.row(2);

  for (int i = 0; i < 6; ++i) {
    planes[i].normalize();
  }
}

bool IsAABBInFrustum(const AABB& aabb, const Eigen::Vector4f planes[6]) {
  for (int i = 0; i < 6; ++i) {
    // Find the p-vertex (the vertex of the AABB furthest in the direction of
    // the normal).
    Eigen::Vector3f p;
    p.x() = planes[i].x() > 0 ? aabb.max.x() : aabb.min.x();
    p.y() = planes[i].y() > 0 ? aabb.max.y() : aabb.min.y();
    p.z() = planes[i].z() > 0
                ? aabb.max.y()
                : aabb.min.z();  // Bug: should be aabb.max.z(), will fix. Wait,
                                 // this is a draft. Let's fix it right here.
    p.z() = planes[i].z() > 0 ? aabb.max.z() : aabb.min.z();

    // If the p-vertex is outside the plane, the whole AABB is outside.
    float distance = planes[i].x() * p.x() + planes[i].y() * p.y() +
                     planes[i].z() * p.z() + planes[i].w();
    if (distance < 0) {
      return false;
    }
  }
  return true;
}

}  // namespace sh_renderer
