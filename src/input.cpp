#include "input.h"

#include <glog/logging.h>

#include <deque>
#include <optional>

namespace sh_renderer {

namespace input_internal {

// Queue to buffer events from GLFW callbacks.
std::deque<InputEvent> event_queue;

void KeyCallback(GLFWwindow* /*window*/, int key, int /*scancode*/, int action,
                 int /*mods*/) {
  if (key < 0 || key > GLFW_KEY_LAST) return;

  // Map common keys to single-char codes.
  auto MapKey = [](int glfw_key) -> char {
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
  };

  char mapped = MapKey(key);
  if (mapped == '\0') return;

  if (action == GLFW_PRESS) {
    event_queue.push_back(InputEvent{KeyPressEvent{mapped}});
  } else if (action == GLFW_RELEASE) {
    event_queue.push_back(InputEvent{KeyReleaseEvent{mapped}});
  }
}

void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
  static double last_x = xpos;
  static double last_y = ypos;

  int width, height;
  glfwGetWindowSize(window, &width, &height);
  if (width == 0 || height == 0) return;

  float dx = static_cast<float>(xpos - last_x) / static_cast<float>(width);
  float dy = static_cast<float>(ypos - last_y) / static_cast<float>(height);

  last_x = xpos;
  last_y = ypos;

  if (dx != 0.0f || dy != 0.0f) {
    event_queue.push_back(MouseDragEvent{dx, dy});
  }
}

void ScrollCallback(GLFWwindow* /*window*/, double /*xoffset*/,
                    double yoffset) {
  event_queue.push_back(MouseScrollEvent{static_cast<float>(yoffset)});
}

}  // namespace input_internal

std::optional<InputEvent> PollInputEvent(Window window, InputState* state) {
  if (!state->registered_callbacks) {
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

  if (input_internal::event_queue.empty()) {
    return std::nullopt;
  }

  InputEvent event = input_internal::event_queue.front();
  input_internal::event_queue.pop_front();
  return event;
}

}  // namespace sh_renderer
