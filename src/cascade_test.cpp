#include "cascade.h"

#include <gtest/gtest.h>

namespace sh_renderer {
namespace {

constexpr float kEpsilon = 1e-4f;

SunLight MakeSunLight(const Eigen::Vector3f& direction) {
  SunLight light;
  light.direction = direction;
  light.color = Eigen::Vector3f::Ones();
  light.intensity = 1.0f;
  return light;
}

Camera MakeDefaultCamera() {
  Camera camera;
  camera.position = Eigen::Vector3f::Zero();
  camera.orientation = Eigen::Quaternionf::Identity();
  camera.intrinsics.fov_y_radians = static_cast<float>(M_PI / 4.0);
  camera.intrinsics.aspect_ratio = 16.0f / 9.0f;
  camera.intrinsics.z_near = 0.1f;
  camera.intrinsics.z_far = 100.0f;
  return camera;
}

// --- Basic Output Shape ---

TEST(CascadeTest, ReturnsRequestedNumberOfCascades) {
  SunLight sun = MakeSunLight(Eigen::Vector3f(0, -1, 0));
  Camera camera = MakeDefaultCamera();

  auto cascades = ComputeCascades(sun, camera);
  EXPECT_EQ(cascades.size(), kNumShadowMapCascades);
}

// --- Split Depths ---

TEST(CascadeTest, LastCascadeSplitEqualsZFar) {
  SunLight sun = MakeSunLight(Eigen::Vector3f(0, -1, 0));
  Camera camera = MakeDefaultCamera();

  auto cascades = ComputeCascades(sun, camera);
  EXPECT_NEAR(cascades.back().split_depth, camera.intrinsics.z_far, kEpsilon);
}

TEST(CascadeTest, SplitDepthsAreMonotonicallyIncreasing) {
  SunLight sun = MakeSunLight(Eigen::Vector3f(0, -1, 0));
  Camera camera = MakeDefaultCamera();

  auto cascades = ComputeCascades(sun, camera);
  float prev = camera.intrinsics.z_near;
  for (const auto& c : cascades) {
    EXPECT_GT(c.split_depth, prev);
    prev = c.split_depth;
  }
}

// --- Orthographic Bounds ---

TEST(CascadeTest, OrthoBoundsAreValid) {
  SunLight sun = MakeSunLight(Eigen::Vector3f(0, -1, 0));
  Camera camera = MakeDefaultCamera();

  auto cascades = ComputeCascades(sun, camera);
  for (size_t i = 0; i < cascades.size(); ++i) {
    const auto& c = cascades[i];
    EXPECT_LT(c.left, c.right) << "cascade " << i;
    EXPECT_LT(c.bottom, c.top) << "cascade " << i;
    EXPECT_LT(c.near, c.far) << "cascade " << i;
  }
}

TEST(CascadeTest, LaterCascadesAreLargerOrEqual) {
  SunLight sun = MakeSunLight(Eigen::Vector3f(0, -1, 0));
  Camera camera = MakeDefaultCamera();

  auto cascades = ComputeCascades(sun, camera);
  for (size_t i = 1; i < cascades.size(); ++i) {
    float prev_width = cascades[i - 1].right - cascades[i - 1].left;
    float cur_width = cascades[i].right - cascades[i].left;
    EXPECT_GE(cur_width, prev_width - kEpsilon) << "cascade " << i;
  }
}

// --- View-Projection Matrix ---

TEST(CascadeTest, ViewProjectionMatrixIsFinite) {
  SunLight sun = MakeSunLight(Eigen::Vector3f(0, -1, 0));
  Camera camera = MakeDefaultCamera();

  auto cascades = ComputeCascades(sun, camera);
  for (size_t i = 0; i < cascades.size(); ++i) {
    EXPECT_TRUE(cascades[i].view_projection_matrix.allFinite())
        << "cascade " << i;
  }
}

TEST(CascadeTest, CameraOriginMapsInsideNDC) {
  SunLight sun = MakeSunLight(Eigen::Vector3f(0, -1, 0));
  Camera camera = MakeDefaultCamera();
  camera.position = Eigen::Vector3f::Zero();

  auto cascades = ComputeCascades(sun, camera);
  // The origin should project into the first cascade's NDC cube since the
  // camera is at the origin and the first cascade covers nearby geometry.
  Eigen::Vector4f origin_h(0, 0, 0, 1);
  Eigen::Vector4f clip = cascades[0].view_projection_matrix * origin_h;
  // In orthographic projection w is always 1.
  EXPECT_NEAR(clip.w(), 1.0f, kEpsilon);
  // Allow a small margin beyond [-1, 1] because texel snapping can shift the
  // ortho bounds by up to one texel.
  constexpr float kNdcMargin = 0.05f;
  EXPECT_GE(clip.x(), -1.0f - kNdcMargin);
  EXPECT_LE(clip.x(), 1.0f + kNdcMargin);
  EXPECT_GE(clip.y(), -1.0f - kNdcMargin);
  EXPECT_LE(clip.y(), 1.0f + kNdcMargin);
}

// --- Different Light Directions ---

TEST(CascadeTest, DiagonalLightProducesValidCascades) {
  SunLight sun = MakeSunLight(Eigen::Vector3f(1, -1, -1).normalized());
  Camera camera = MakeDefaultCamera();

  auto cascades = ComputeCascades(sun, camera);
  ASSERT_EQ(cascades.size(), 3u);
  for (size_t i = 0; i < cascades.size(); ++i) {
    EXPECT_TRUE(cascades[i].view_projection_matrix.allFinite())
        << "cascade " << i;
    EXPECT_LT(cascades[i].left, cascades[i].right) << "cascade " << i;
    EXPECT_LT(cascades[i].bottom, cascades[i].top) << "cascade " << i;
  }
}

TEST(CascadeTest, HorizontalLightProducesValidCascades) {
  // Light coming from +X (sunrise/sunset scenario).
  SunLight sun = MakeSunLight(Eigen::Vector3f(-1, 0, 0));
  Camera camera = MakeDefaultCamera();

  auto cascades = ComputeCascades(sun, camera);
  ASSERT_EQ(cascades.size(), 3u);
  for (size_t i = 0; i < cascades.size(); ++i) {
    EXPECT_TRUE(cascades[i].view_projection_matrix.allFinite())
        << "cascade " << i;
    EXPECT_LT(cascades[i].near, cascades[i].far) << "cascade " << i;
  }
}

// --- Camera Pose Variations ---

TEST(CascadeTest, TranslatedCameraShiftsCascadeBounds) {
  SunLight sun = MakeSunLight(Eigen::Vector3f(0, -1, 0));
  Camera cam_a = MakeDefaultCamera();
  cam_a.position = Eigen::Vector3f::Zero();

  Camera cam_b = MakeDefaultCamera();
  cam_b.position = Eigen::Vector3f(50, 0, 50);

  auto cascades_a = ComputeCascades(sun, cam_a);
  auto cascades_b = ComputeCascades(sun, cam_b);

  // The view-projection matrices must differ because the frustums are in
  // different world-space regions.
  EXPECT_FALSE(cascades_a[0].view_projection_matrix.isApprox(
      cascades_b[0].view_projection_matrix, kEpsilon));
}

TEST(CascadeTest, RotatedCameraChangesCascadeBounds) {
  SunLight sun = MakeSunLight(Eigen::Vector3f(0, -1, 0));
  Camera cam_a = MakeDefaultCamera();

  Camera cam_b = MakeDefaultCamera();
  // Yaw 90 degrees.
  cam_b.orientation = Eigen::Quaternionf(Eigen::AngleAxisf(
      static_cast<float>(M_PI / 2.0), Eigen::Vector3f::UnitY()));

  auto cascades_a = ComputeCascades(sun, cam_a);
  auto cascades_b = ComputeCascades(sun, cam_b);

  EXPECT_FALSE(cascades_a[0].view_projection_matrix.isApprox(
      cascades_b[0].view_projection_matrix, kEpsilon));
}

// --- Near-Degenerate Light Direction ---

TEST(CascadeTest, NearlyVerticalLightUsesAlternateUp) {
  // Light direction almost exactly along +Y triggers alternate up vector path.
  SunLight sun = MakeSunLight(Eigen::Vector3f(0, -1, 0.001f).normalized());
  Camera camera = MakeDefaultCamera();

  auto cascades = ComputeCascades(sun, camera);
  for (size_t i = 0; i < cascades.size(); ++i) {
    EXPECT_TRUE(cascades[i].view_projection_matrix.allFinite())
        << "cascade " << i;
  }
}

// --- Z Range Padding ---

TEST(CascadeTest, ZRangeIncludesPadding) {
  SunLight sun = MakeSunLight(Eigen::Vector3f(0, -1, 0));
  Camera camera = MakeDefaultCamera();

  auto cascades = ComputeCascades(sun, camera);
  // The Z range (far - near) should be larger than the raw frustum extent
  // because the implementation adds padding for occluders (20.0 units).
  float z_range = cascades[0].far - cascades[0].near;
  EXPECT_GT(z_range, 20.0f);
}

}  // namespace
}  // namespace sh_renderer
