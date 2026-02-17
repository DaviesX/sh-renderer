#include "camera.h"

#include <gtest/gtest.h>

#include <cmath>

namespace sh_renderer {
namespace {

constexpr float kEpsilon = 1e-5f;

Camera MakeDefaultCamera() {
  return Camera{
      .position = Eigen::Vector3f::Zero(),
      .orientation = Eigen::Quaternionf::Identity(),
  };
}

// --- TranslateCamera ---

TEST(CameraTest, TranslateIdentityOrientation) {
  Camera camera = MakeDefaultCamera();
  TranslateCamera(Eigen::Vector3f(1.0f, 2.0f, 3.0f), &camera);
  EXPECT_TRUE(
      camera.position.isApprox(Eigen::Vector3f(1.0f, 2.0f, 3.0f), kEpsilon));
}

TEST(CameraTest, TranslateWithRotation) {
  Camera camera = MakeDefaultCamera();
  // Rotate 90° around Y (yaw left). Camera's local +X becomes world -Z.
  PanCamera(M_PI / 2.0f, &camera);

  TranslateCamera(Eigen::Vector3f(1.0f, 0.0f, 0.0f), &camera);
  // Local +X should map to world -Z after 90° yaw.
  EXPECT_NEAR(camera.position.x(), 0.0f, kEpsilon);
  EXPECT_NEAR(camera.position.z(), -1.0f, kEpsilon);
}

TEST(CameraTest, TranslateAccumulates) {
  Camera camera = MakeDefaultCamera();
  TranslateCamera(Eigen::Vector3f(1.0f, 0.0f, 0.0f), &camera);
  TranslateCamera(Eigen::Vector3f(0.0f, 2.0f, 0.0f), &camera);
  EXPECT_TRUE(
      camera.position.isApprox(Eigen::Vector3f(1.0f, 2.0f, 0.0f), kEpsilon));
}

// --- PanCamera (Yaw) ---

TEST(CameraTest, PanZeroNoChange) {
  Camera camera = MakeDefaultCamera();
  PanCamera(0.0f, &camera);
  EXPECT_TRUE(
      camera.orientation.isApprox(Eigen::Quaternionf::Identity(), kEpsilon));
}

TEST(CameraTest, PanFullTurnReturnsToIdentity) {
  Camera camera = MakeDefaultCamera();
  PanCamera(static_cast<float>(2.0 * M_PI), &camera);
  // Full rotation should be approximately identity (or its negative).
  float dot = std::abs(camera.orientation.dot(Eigen::Quaternionf::Identity()));
  EXPECT_NEAR(dot, 1.0f, kEpsilon);
}

TEST(CameraTest, PanHalfTurnFlipsForward) {
  Camera camera = MakeDefaultCamera();
  PanCamera(static_cast<float>(M_PI), &camera);
  // Camera forward was (0,0,-1), after 180° yaw it should be (0,0,1).
  Eigen::Vector3f forward = camera.orientation * Eigen::Vector3f(0, 0, -1);
  EXPECT_NEAR(forward.x(), 0.0f, kEpsilon);
  EXPECT_NEAR(forward.z(), 1.0f, kEpsilon);
}

// --- TiltCamera (Pitch) ---

TEST(CameraTest, TiltZeroNoChange) {
  Camera camera = MakeDefaultCamera();
  TiltCamera(0.0f, &camera);
  EXPECT_TRUE(
      camera.orientation.isApprox(Eigen::Quaternionf::Identity(), kEpsilon));
}

TEST(CameraTest, TiltLookUp) {
  Camera camera = MakeDefaultCamera();
  // Positive pitch around local +X rotates forward (-Z) toward +Y.
  TiltCamera(static_cast<float>(M_PI / 2.0), &camera);
  Eigen::Vector3f forward = camera.orientation * Eigen::Vector3f(0, 0, -1);
  EXPECT_NEAR(forward.x(), 0.0f, kEpsilon);
  EXPECT_NEAR(forward.y(), 1.0f, kEpsilon);
  EXPECT_NEAR(forward.z(), 0.0f, kEpsilon);
}

// --- RollCamera ---

TEST(CameraTest, RollZeroNoChange) {
  Camera camera = MakeDefaultCamera();
  RollCamera(0.0f, &camera);
  EXPECT_TRUE(
      camera.orientation.isApprox(Eigen::Quaternionf::Identity(), kEpsilon));
}

TEST(CameraTest, RollPreservesForward) {
  Camera camera = MakeDefaultCamera();
  RollCamera(static_cast<float>(M_PI / 4.0), &camera);
  // Rolling should not change the forward direction.
  Eigen::Vector3f forward = camera.orientation * Eigen::Vector3f(0, 0, -1);
  EXPECT_NEAR(forward.x(), 0.0f, kEpsilon);
  EXPECT_NEAR(forward.y(), 0.0f, kEpsilon);
  EXPECT_NEAR(forward.z(), -1.0f, kEpsilon);
}

TEST(CameraTest, RollChangesUp) {
  Camera camera = MakeDefaultCamera();
  RollCamera(static_cast<float>(M_PI / 2.0), &camera);
  // After 90° roll, camera's local up should become local right.
  Eigen::Vector3f up = camera.orientation * Eigen::Vector3f(0, 1, 0);
  EXPECT_NEAR(up.x(), -1.0f, kEpsilon);
  EXPECT_NEAR(up.y(), 0.0f, kEpsilon);
  EXPECT_NEAR(up.z(), 0.0f, kEpsilon);
}

// --- LookAt ---

TEST(CameraTest, LookAtForward) {
  Camera camera = MakeDefaultCamera();
  LookAt(Eigen::Vector3f(0, 0, -5), &camera);
  // Looking along -Z, should be close to identity.
  Eigen::Vector3f forward = camera.orientation * Eigen::Vector3f(0, 0, -1);
  EXPECT_NEAR(forward.x(), 0.0f, kEpsilon);
  EXPECT_NEAR(forward.y(), 0.0f, kEpsilon);
  EXPECT_NEAR(forward.z(), -1.0f, kEpsilon);
}

TEST(CameraTest, LookAtRight) {
  Camera camera = MakeDefaultCamera();
  LookAt(Eigen::Vector3f(5, 0, 0), &camera);
  // Camera should now look along +X.
  Eigen::Vector3f forward = camera.orientation * Eigen::Vector3f(0, 0, -1);
  EXPECT_NEAR(forward.x(), 1.0f, kEpsilon);
  EXPECT_NEAR(forward.y(), 0.0f, kEpsilon);
  EXPECT_NEAR(forward.z(), 0.0f, kEpsilon);
}

TEST(CameraTest, LookAtStraightUp) {
  Camera camera = MakeDefaultCamera();
  LookAt(Eigen::Vector3f(0, 5, 0), &camera);
  // Degenerate case — should not crash, forward should point up.
  Eigen::Vector3f forward = camera.orientation * Eigen::Vector3f(0, 0, -1);
  EXPECT_NEAR(forward.y(), 1.0f, kEpsilon);
}

// --- GetViewMatrix ---

TEST(CameraTest, ViewMatrixIdentity) {
  Camera camera = MakeDefaultCamera();
  Eigen::Matrix4f view = GetViewMatrix(camera);
  EXPECT_TRUE(view.isApprox(Eigen::Matrix4f::Identity(), kEpsilon));
}

TEST(CameraTest, ViewMatrixTranslation) {
  Camera camera{
      .position = Eigen::Vector3f(3.0f, 0.0f, 0.0f),
      .orientation = Eigen::Quaternionf::Identity(),
  };
  Eigen::Matrix4f view = GetViewMatrix(camera);

  // With identity rotation, translation column should be (-3, 0, 0).
  EXPECT_NEAR(view(0, 3), -3.0f, kEpsilon);
  EXPECT_NEAR(view(1, 3), 0.0f, kEpsilon);
  EXPECT_NEAR(view(2, 3), 0.0f, kEpsilon);
}

TEST(CameraTest, ViewMatrixInversesModelMatrix) {
  Camera camera{
      .position = Eigen::Vector3f(1.0f, 2.0f, 3.0f),
      .orientation = Eigen::Quaternionf::Identity(),
  };
  PanCamera(0.5f, &camera);

  Eigen::Matrix4f view = GetViewMatrix(camera);

  // A point at the camera's position should map to origin in view space.
  Eigen::Vector4f cam_pos_h(camera.position.x(), camera.position.y(),
                            camera.position.z(), 1.0f);
  Eigen::Vector4f result = view * cam_pos_h;
  EXPECT_NEAR(result.x(), 0.0f, kEpsilon);
  EXPECT_NEAR(result.y(), 0.0f, kEpsilon);
  EXPECT_NEAR(result.z(), 0.0f, kEpsilon);
}

}  // namespace
}  // namespace sh_renderer
