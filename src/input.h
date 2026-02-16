#pragma once

#include <Eigen/Dense>
#include <deque>
#include <optional>
#include <variant>

#include "window.h"

namespace sh_renderer {

// Key press event (distinct type for variant discrimination).
struct KeyPressEvent {
  char key;
};

// Key release event (distinct type for variant discrimination).
struct KeyReleaseEvent {
  char key;
};

// Normalized delta position.
using MouseDragEvent = Eigen::Vector2f;

using MouseScrollEvent = float;

// Union of all input events.
using InputEvent = std::variant<KeyPressEvent, KeyReleaseEvent, MouseDragEvent,
                                MouseScrollEvent>;

struct InputState {
  // Whether GLFW callbacks have been registered.
  bool registered_callbacks = false;

  // Event queue populated by callbacks.
  std::deque<InputEvent> event_queue;

  // Last known cursor position for computing deltas.
  double last_cursor_x = 0.0;
  double last_cursor_y = 0.0;
  bool cursor_initialized = false;
};

namespace input_internal {

// GLFW callbacks. These retrieve InputState via glfwGetWindowUserPointer().
void KeyCallback(Window window, int key, int scancode, int action, int mods);
void CursorPosCallback(Window window, double xpos, double ypos);
void ScrollCallback(Window window, double xoffset, double yoffset);

// Testable core logic (no GLFW dependency).
void ProcessKeyEvent(int key, int action, InputState* state);
void ProcessCursorPos(double xpos, double ypos, int window_width,
                      int window_height, InputState* state);
void ProcessScroll(double yoffset, InputState* state);

}  // namespace input_internal

// Polls for the next input event. Returns std::nullopt if no event is
// available.
std::optional<InputEvent> PollInputEvent(Window window, InputState* state);

}  // namespace sh_renderer
