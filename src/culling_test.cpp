#include "culling.h"

#include <gtest/gtest.h>

namespace sh_renderer {
namespace {

TEST(CullingTest, IsAABBInFrustum_Inside) {
  // Simple orthographic projection frustum
  Eigen::Matrix4f proj = Eigen::Matrix4f::Identity();
  // Near = -1, Far = 1, Left = -1, Right = 1, Bottom = -1, Top = 1

  Eigen::Vector4f planes[6];
  ExtractFrustumPlanes(proj, planes);

  AABB aabb;
  aabb.min = Eigen::Vector3f(-0.5f, -0.5f, -0.5f);
  aabb.max = Eigen::Vector3f(0.5f, 0.5f, 0.5f);

  EXPECT_TRUE(IsAABBInFrustum(aabb, planes));
}

TEST(CullingTest, IsAABBInFrustum_Outside) {
  Eigen::Matrix4f proj = Eigen::Matrix4f::Identity();

  Eigen::Vector4f planes[6];
  ExtractFrustumPlanes(proj, planes);

  AABB aabb;
  aabb.min = Eigen::Vector3f(2.0f, 2.0f, 2.0f);
  aabb.max = Eigen::Vector3f(3.0f, 3.0f, 3.0f);

  EXPECT_FALSE(IsAABBInFrustum(aabb, planes));
}

TEST(CullingTest, IsAABBInFrustum_Intersecting) {
  Eigen::Matrix4f proj = Eigen::Matrix4f::Identity();

  Eigen::Vector4f planes[6];
  ExtractFrustumPlanes(proj, planes);

  AABB aabb;
  // Partly inside, partly outside x > 1
  aabb.min = Eigen::Vector3f(0.5f, -0.5f, -0.5f);
  aabb.max = Eigen::Vector3f(1.5f, 0.5f, 0.5f);

  EXPECT_TRUE(IsAABBInFrustum(aabb, planes));
}

}  // namespace
}  // namespace sh_renderer
