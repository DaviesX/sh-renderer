#include "input.h"

#include <glog/logging.h>

namespace sh_renderer {

namespace input_internal {

namespace {

char MapKey(int glfw_key) {
  switch (glfw_key) {
    case GLFW_KEY_W:
      return 'W';
    case GLFW_KEY_A:
      return 'A';
    case GLFW_KEY_S:
      return 'S';
    case GLFW_KEY_D:
      return 'D';
    case GLFW_KEY_Q:
      return 'Q';
    case GLFW_KEY_E:
      return 'E';
    case GLFW_KEY_ESCAPE:
      return '\x1B';
    case GLFW_KEY_SPACE:
      return ' ';
    case GLFW_KEY_LEFT_SHIFT:
      return '\x01';
    default:
      return '\0';
  }
}

}  // namespace

void ProcessKeyEvent(int key, int action, InputState* state) {
  if (key < 0 || key > GLFW_KEY_LAST) return;

  char mapped = MapKey(key);
  if (mapped == '\0') return;

  if (action == GLFW_PRESS) {
    state->pressed_keys.insert(mapped);
    state->event_queue.push_back(InputEvent{KeyPressEvent{mapped}});
  } else if (action == GLFW_RELEASE) {
    state->pressed_keys.erase(mapped);
    state->event_queue.push_back(InputEvent{KeyReleaseEvent{mapped}});
  }
}

void ProcessCursorPos(double xpos, double ypos, int window_width,
                      int window_height, InputState* state) {
  if (!state->cursor_initialized) {
    state->last_cursor_x = xpos;
    state->last_cursor_y = ypos;
    state->cursor_initialized = true;
    return;
  }

  if (window_width == 0 || window_height == 0) return;

  float dx = static_cast<float>(xpos - state->last_cursor_x) /
             static_cast<float>(window_width);
  float dy = static_cast<float>(ypos - state->last_cursor_y) /
             static_cast<float>(window_height);

  state->last_cursor_x = xpos;
  state->last_cursor_y = ypos;

  if (dx != 0.0f || dy != 0.0f) {
    state->event_queue.push_back(MouseDragEvent{dx, dy});
  }
}

void ProcessScroll(double yoffset, InputState* state) {
  state->event_queue.push_back(MouseScrollEvent{static_cast<float>(yoffset)});
}

void KeyCallback(Window window, int key, int /*scancode*/, int action,
                 int /*mods*/) {
  auto* state = static_cast<InputState*>(glfwGetWindowUserPointer(window));
  if (!state) return;
  ProcessKeyEvent(key, action, state);
}

void CursorPosCallback(Window window, double xpos, double ypos) {
  auto* state = static_cast<InputState*>(glfwGetWindowUserPointer(window));
  if (!state) return;

  int width, height;
  glfwGetWindowSize(window, &width, &height);
  ProcessCursorPos(xpos, ypos, width, height, state);
}

void ScrollCallback(Window window, double /*xoffset*/, double yoffset) {
  auto* state = static_cast<InputState*>(glfwGetWindowUserPointer(window));
  if (!state) return;
  ProcessScroll(yoffset, state);
}

}  // namespace input_internal

std::vector<InputEvent> PollInputEvents(Window window, InputState* state) {
  if (!state->registered_callbacks) {
    glfwSetWindowUserPointer(window, state);
    glfwSetKeyCallback(window, input_internal::KeyCallback);
    glfwSetCursorPosCallback(window, input_internal::CursorPosCallback);
    glfwSetScrollCallback(window, input_internal::ScrollCallback);

    // Capture the cursor for FPS-style input.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) {
      glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    state->registered_callbacks = true;
  }

  glfwPollEvents();

  std::vector<InputEvent> events;
  std::unordered_set<char> just_pressed;

  // 1. Collect all events from callbacks (buffered in the queue).
  while (!state->event_queue.empty()) {
    const auto& event = state->event_queue.front();
    events.push_back(event);

    // Track keys that were physically pressed this frame to avoid duplicates.
    if (auto* press = std::get_if<KeyPressEvent>(&event)) {
      just_pressed.insert(press->key);
    }

    state->event_queue.pop_front();
  }

  // 2. Generate events for held keys, skipping any that were just pressed.
  for (char key : state->pressed_keys) {
    if (just_pressed.find(key) == just_pressed.end()) {
      events.push_back(InputEvent{KeyPressEvent{key}});
    }
  }

  return events;
}

}  // namespace sh_renderer
