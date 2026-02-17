#include "input.h"

#include <gtest/gtest.h>

namespace sh_renderer {
namespace {

TEST(InputTest, ProcessKeyPressW) {
  InputState state;
  input_internal::ProcessKeyEvent(GLFW_KEY_W, GLFW_PRESS, &state);

  ASSERT_EQ(state.event_queue.size(), 1);
  auto* key = std::get_if<KeyPressEvent>(&state.event_queue.front());
  ASSERT_NE(key, nullptr);
  EXPECT_EQ(key->key, 'W');
}

TEST(InputTest, ProcessKeyReleaseA) {
  InputState state;
  input_internal::ProcessKeyEvent(GLFW_KEY_A, GLFW_RELEASE, &state);

  ASSERT_EQ(state.event_queue.size(), 1);
  auto* key = std::get_if<KeyReleaseEvent>(&state.event_queue.front());
  ASSERT_NE(key, nullptr);
  EXPECT_EQ(key->key, 'A');
}

TEST(InputTest, ProcessKeyEscape) {
  InputState state;
  input_internal::ProcessKeyEvent(GLFW_KEY_ESCAPE, GLFW_PRESS, &state);

  ASSERT_EQ(state.event_queue.size(), 1);
  auto* key = std::get_if<KeyPressEvent>(&state.event_queue.front());
  ASSERT_NE(key, nullptr);
  EXPECT_EQ(key->key, '\x1B');
}

TEST(InputTest, ProcessKeyRepeatIgnored) {
  InputState state;
  input_internal::ProcessKeyEvent(GLFW_KEY_W, GLFW_REPEAT, &state);
  EXPECT_TRUE(state.event_queue.empty());
}

TEST(InputTest, ProcessKeyUnmappedIgnored) {
  InputState state;
  input_internal::ProcessKeyEvent(GLFW_KEY_F1, GLFW_PRESS, &state);
  EXPECT_TRUE(state.event_queue.empty());
}

TEST(InputTest, ProcessKeyInvalidIgnored) {
  InputState state;
  input_internal::ProcessKeyEvent(-1, GLFW_PRESS, &state);
  EXPECT_TRUE(state.event_queue.empty());
}

TEST(InputTest, ProcessCursorPosFirstCallInitializes) {
  InputState state;
  input_internal::ProcessCursorPos(100.0, 200.0, 800, 600, &state);

  // First call only initializes — no event emitted.
  EXPECT_TRUE(state.event_queue.empty());
  EXPECT_TRUE(state.cursor_initialized);
  EXPECT_DOUBLE_EQ(state.last_cursor_x, 100.0);
  EXPECT_DOUBLE_EQ(state.last_cursor_y, 200.0);
}

TEST(InputTest, ProcessCursorPosProducesDelta) {
  InputState state;
  input_internal::ProcessCursorPos(100.0, 200.0, 800, 600, &state);
  input_internal::ProcessCursorPos(180.0, 260.0, 800, 600, &state);

  ASSERT_EQ(state.event_queue.size(), 1);
  auto* drag = std::get_if<MouseDragEvent>(&state.event_queue.front());
  ASSERT_NE(drag, nullptr);
  EXPECT_FLOAT_EQ((*drag)(0), 80.0f / 800.0f);
  EXPECT_FLOAT_EQ((*drag)(1), 60.0f / 600.0f);
}

TEST(InputTest, ProcessCursorPosZeroSizeWindowIgnored) {
  InputState state;
  state.cursor_initialized = true;
  state.last_cursor_x = 0.0;
  state.last_cursor_y = 0.0;
  input_internal::ProcessCursorPos(10.0, 10.0, 0, 0, &state);

  EXPECT_TRUE(state.event_queue.empty());
}

TEST(InputTest, ProcessCursorPosNoDeltaNoEvent) {
  InputState state;
  input_internal::ProcessCursorPos(100.0, 200.0, 800, 600, &state);
  input_internal::ProcessCursorPos(100.0, 200.0, 800, 600, &state);

  EXPECT_TRUE(state.event_queue.empty());
}

TEST(InputTest, ProcessScrollPositive) {
  InputState state;
  input_internal::ProcessScroll(1.0, &state);

  ASSERT_EQ(state.event_queue.size(), 1);
  auto* scroll = std::get_if<MouseScrollEvent>(&state.event_queue.front());
  ASSERT_NE(scroll, nullptr);
  EXPECT_FLOAT_EQ(*scroll, 1.0f);
}

TEST(InputTest, ProcessScrollNegative) {
  InputState state;
  input_internal::ProcessScroll(-2.5, &state);

  ASSERT_EQ(state.event_queue.size(), 1);
  auto* scroll = std::get_if<MouseScrollEvent>(&state.event_queue.front());
  ASSERT_NE(scroll, nullptr);
  EXPECT_FLOAT_EQ(*scroll, -2.5f);
}

TEST(InputTest, MultipleEventsQueueInOrder) {
  InputState state;
  input_internal::ProcessKeyEvent(GLFW_KEY_W, GLFW_PRESS, &state);
  input_internal::ProcessScroll(1.0, &state);
  input_internal::ProcessKeyEvent(GLFW_KEY_W, GLFW_RELEASE, &state);

  ASSERT_EQ(state.event_queue.size(), 3);

  EXPECT_NE(std::get_if<KeyPressEvent>(&state.event_queue[0]), nullptr);
  EXPECT_NE(std::get_if<MouseScrollEvent>(&state.event_queue[1]), nullptr);
  EXPECT_NE(std::get_if<KeyReleaseEvent>(&state.event_queue[2]), nullptr);
}

TEST(InputTest, PressedKeysTracking) {
  InputState state;
  input_internal::ProcessKeyEvent(GLFW_KEY_W, GLFW_PRESS, &state);
  EXPECT_TRUE(state.pressed_keys.count('W'));

  input_internal::ProcessKeyEvent(GLFW_KEY_A, GLFW_PRESS, &state);
  EXPECT_TRUE(state.pressed_keys.count('W'));
  EXPECT_TRUE(state.pressed_keys.count('A'));

  input_internal::ProcessKeyEvent(GLFW_KEY_W, GLFW_RELEASE, &state);
  EXPECT_FALSE(state.pressed_keys.count('W'));
  EXPECT_TRUE(state.pressed_keys.count('A'));

  input_internal::ProcessKeyEvent(GLFW_KEY_A, GLFW_RELEASE, &state);
  EXPECT_FALSE(state.pressed_keys.count('A'));
}

}  // namespace
}  // namespace sh_renderer
