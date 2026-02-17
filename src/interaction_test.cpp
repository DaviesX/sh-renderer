#include "interaction.h"

#include <gtest/gtest.h>

namespace sh_renderer {
namespace {

Camera MakeDefaultCamera() {
  return Camera{
      .position = Eigen::Vector3f::Zero(),
      .orientation = Eigen::Quaternionf::Identity(),
  };
}

constexpr float kEpsilon = 1e-5f;

// --- Key press movement ---

TEST(InteractionTest, WKeyMovesForward) {
  Camera camera = MakeDefaultCamera();
  InteractionState state;
  bool should_close = false;

  HandleInputEvent(InputEvent{KeyPressEvent{'W'}}, &state, &camera,
                   &should_close);

  // Forward is -Z in camera space.
  EXPECT_LT(camera.position.z(), 0.0f);
  EXPECT_NEAR(camera.position.x(), 0.0f, kEpsilon);
  EXPECT_NEAR(camera.position.y(), 0.0f, kEpsilon);
  EXPECT_FALSE(should_close);
}

TEST(InteractionTest, SKeyMovesBackward) {
  Camera camera = MakeDefaultCamera();
  InteractionState state;
  bool should_close = false;

  HandleInputEvent(InputEvent{KeyPressEvent{'S'}}, &state, &camera,
                   &should_close);

  EXPECT_GT(camera.position.z(), 0.0f);
}

TEST(InteractionTest, AKeyMovesLeft) {
  Camera camera = MakeDefaultCamera();
  InteractionState state;
  bool should_close = false;

  HandleInputEvent(InputEvent{KeyPressEvent{'A'}}, &state, &camera,
                   &should_close);

  EXPECT_LT(camera.position.x(), 0.0f);
}

TEST(InteractionTest, DKeyMovesRight) {
  Camera camera = MakeDefaultCamera();
  InteractionState state;
  bool should_close = false;

  HandleInputEvent(InputEvent{KeyPressEvent{'D'}}, &state, &camera,
                   &should_close);

  EXPECT_GT(camera.position.x(), 0.0f);
}

TEST(InteractionTest, QKeyMovesDown) {
  Camera camera = MakeDefaultCamera();
  InteractionState state;
  bool should_close = false;

  HandleInputEvent(InputEvent{KeyPressEvent{'Q'}}, &state, &camera,
                   &should_close);

  EXPECT_LT(camera.position.y(), 0.0f);
}

TEST(InteractionTest, EKeyMovesUp) {
  Camera camera = MakeDefaultCamera();
  InteractionState state;
  bool should_close = false;

  HandleInputEvent(InputEvent{KeyPressEvent{'E'}}, &state, &camera,
                   &should_close);

  EXPECT_GT(camera.position.y(), 0.0f);
}

TEST(InteractionTest, EscapeSetsClose) {
  Camera camera = MakeDefaultCamera();
  InteractionState state;
  bool should_close = false;

  HandleInputEvent(InputEvent{KeyPressEvent{'\x1B'}}, &state, &camera,
                   &should_close);

  EXPECT_TRUE(should_close);
  // Camera should not have moved.
  EXPECT_TRUE(camera.position.isApprox(Eigen::Vector3f::Zero(), kEpsilon));
}

TEST(InteractionTest, UnmappedKeyNoMovement) {
  Camera camera = MakeDefaultCamera();
  InteractionState state;
  bool should_close = false;

  HandleInputEvent(InputEvent{KeyPressEvent{' '}}, &state, &camera,
                   &should_close);

  EXPECT_TRUE(camera.position.isApprox(Eigen::Vector3f::Zero(), kEpsilon));
  EXPECT_FALSE(should_close);
}

// --- Mouse drag ---

TEST(InteractionTest, MouseDragChangesOrientation) {
  Camera camera = MakeDefaultCamera();
  InteractionState state;
  bool should_close = false;

  Eigen::Quaternionf before = camera.orientation;
  HandleInputEvent(InputEvent{MouseDragEvent{0.1f, 0.05f}}, &state, &camera,
                   &should_close);

  // Orientation should have changed.
  EXPECT_FALSE(camera.orientation.isApprox(before, kEpsilon));
}

TEST(InteractionTest, MouseDragZeroNoChange) {
  Camera camera = MakeDefaultCamera();
  InteractionState state;
  bool should_close = false;

  HandleInputEvent(InputEvent{MouseDragEvent{0.0f, 0.0f}}, &state, &camera,
                   &should_close);

  // Pan(0) and Tilt(0) should not change orientation.
  float dot = std::abs(camera.orientation.dot(Eigen::Quaternionf::Identity()));
  EXPECT_NEAR(dot, 1.0f, kEpsilon);
}

// --- Scroll speed ---

TEST(InteractionTest, ScrollUpIncreasesSpeed) {
  Camera camera = MakeDefaultCamera();
  InteractionState state;
  bool should_close = false;
  float initial_speed = state.move_speed;

  HandleInputEvent(InputEvent{MouseScrollEvent{1.0f}}, &state, &camera,
                   &should_close);

  EXPECT_GT(state.move_speed, initial_speed);
}

TEST(InteractionTest, ScrollDownDecreasesSpeed) {
  Camera camera = MakeDefaultCamera();
  InteractionState state;
  bool should_close = false;
  float initial_speed = state.move_speed;

  HandleInputEvent(InputEvent{MouseScrollEvent{-1.0f}}, &state, &camera,
                   &should_close);

  EXPECT_LT(state.move_speed, initial_speed);
}

TEST(InteractionTest, ScrollSpeedClampedMin) {
  Camera camera = MakeDefaultCamera();
  InteractionState state;
  state.move_speed = 0.15f;
  bool should_close = false;

  // Scroll down many times to hit the floor.
  for (int i = 0; i < 50; ++i) {
    HandleInputEvent(InputEvent{MouseScrollEvent{-1.0f}}, &state, &camera,
                     &should_close);
  }

  EXPECT_GE(state.move_speed, 0.1f);
}

TEST(InteractionTest, ScrollSpeedClampedMax) {
  Camera camera = MakeDefaultCamera();
  InteractionState state;
  state.move_speed = 90.0f;
  bool should_close = false;

  for (int i = 0; i < 50; ++i) {
    HandleInputEvent(InputEvent{MouseScrollEvent{1.0f}}, &state, &camera,
                     &should_close);
  }

  EXPECT_LE(state.move_speed, 100.0f);
}

// --- Movement speed affects translation ---

TEST(InteractionTest, HigherSpeedMovesFarther) {
  Camera camera1 = MakeDefaultCamera();
  Camera camera2 = MakeDefaultCamera();
  InteractionState state1;
  InteractionState state2;
  state2.move_speed = state1.move_speed * 4.0f;
  bool close = false;

  HandleInputEvent(InputEvent{KeyPressEvent{'W'}}, &state1, &camera1, &close);
  HandleInputEvent(InputEvent{KeyPressEvent{'W'}}, &state2, &camera2, &close);

  EXPECT_GT(std::abs(camera2.position.z()), std::abs(camera1.position.z()));
}

// --- Key release is a no-op ---

TEST(InteractionTest, KeyReleaseNoEffect) {
  Camera camera = MakeDefaultCamera();
  InteractionState state;
  bool should_close = false;

  HandleInputEvent(InputEvent{KeyReleaseEvent{'W'}}, &state, &camera,
                   &should_close);

  EXPECT_TRUE(camera.position.isApprox(Eigen::Vector3f::Zero(), kEpsilon));
  EXPECT_FALSE(should_close);
}

}  // namespace
}  // namespace sh_renderer
