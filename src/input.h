#pragma once

#include <Eigen/Dense>
#include <optional>
#include <variant>

#include "camera.h"
#include "window.h"

namespace sh_renderer {

// Key code.
using KeyPressEvent = char;
using KeyReleaseEvent = char;

// Normalized delta position.
using MouseDragEvent = Eigen::Vector2f;

using MouseScrollEvent = float;

// Union of all input events.
using InputEvent = std::variant<KeyPressEvent, KeyReleaseEvent, MouseDragEvent,
                                MouseScrollEvent>;

struct InputState {
  // Internal states for testing purposes.
  bool registered_callbacks = false;

  // TODO: ...
};

namespace input_internal {

// TODO: Callbacks go here.

}  // namespace input_internal

// Polls for the next input event. Returns std::nullopt if no event is
// available.
std::optional<InputEvent> PollInputEvent(Window window, InputState *state);

}  // namespace sh_renderer
