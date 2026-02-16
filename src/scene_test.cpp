#include "scene.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <random>

namespace sh_baker {

TEST(SceneTest, TransformedVertices) {
  Geometry geo;
  geo.vertices = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};

  // Translate by (1, 2, 3)
  geo.transform = Eigen::Affine3f::Identity();
  geo.transform.translate(Eigen::Vector3f(1.0f, 2.0f, 3.0f));

  auto transformed = TransformedVertices(geo);
  ASSERT_EQ(transformed.size(), 2);

  // (1,0,0) + (1,2,3) = (2,2,3)
  EXPECT_TRUE(transformed[0].isApprox(Eigen::Vector3f(2.0f, 2.0f, 3.0f)));
  // (0,1,0) + (1,2,3) = (1,3,3)
  EXPECT_TRUE(transformed[1].isApprox(Eigen::Vector3f(1.0f, 3.0f, 3.0f)));
}

TEST(SceneTest, TransformedNormals) {
  Geometry geo;
  geo.normals = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};

  // Rotate 90 degrees around Z axis.
  // X axis (1,0,0) becomes Y axis (0,1,0)
  // Y axis (0,1,0) becomes -X axis (-1,0,0)
  geo.transform = Eigen::Affine3f::Identity();
  geo.transform.rotate(
      Eigen::AngleAxisf(M_PI / 2.0f, Eigen::Vector3f::UnitZ()));

  auto transformed = TransformedNormals(geo);
  ASSERT_EQ(transformed.size(), 2);

  EXPECT_TRUE(transformed[0].isApprox(Eigen::Vector3f(0.0f, 1.0f, 0.0f)));
  EXPECT_TRUE(transformed[1].isApprox(Eigen::Vector3f(-1.0f, 0.0f, 0.0f)));
}

TEST(SceneTest, TransformedTangents) {
  Geometry geo;
  // Tangent pointing X, with sign 1.0
  geo.tangents = {Eigen::Vector4f(1.0f, 0.0f, 0.0f, 1.0f)};

  // Rotate 90 degrees around Z axis.
  geo.transform = Eigen::Affine3f::Identity();
  geo.transform.rotate(
      Eigen::AngleAxisf(M_PI / 2.0f, Eigen::Vector3f::UnitZ()));

  auto transformed = TransformedTangents(geo);
  ASSERT_EQ(transformed.size(), 1);

  // Should rotate to Y (0,1,0), sign preserved
  EXPECT_TRUE(
      transformed[0].head<3>().isApprox(Eigen::Vector3f(0.0f, 1.0f, 0.0f)));
  EXPECT_EQ(transformed[0].w(), 1.0f);
}

}  // namespace sh_baker
