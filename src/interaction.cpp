#include "interaction.h"

#include <glog/logging.h>

#include <algorithm>

#include "camera.h"
#include "input.h"

namespace sh_renderer {

void HandleInputEvent(const InputEvent& event, InteractionState* state,
                      Camera* camera, bool* should_close) {
  if (auto* key_press = std::get_if<KeyPressEvent>(&event)) {
    Eigen::Vector3f delta = Eigen::Vector3f::Zero();
    float speed = state->move_speed * (1.0f / 60.0f);

    switch (key_press->key) {
      case 'W':
        delta.z() = -speed;  // Forward (-Z in camera space).
        break;
      case 'S':
        delta.z() = speed;  // Backward (+Z in camera space).
        break;
      case 'A':
        delta.x() = -speed;  // Left.
        break;
      case 'D':
        delta.x() = speed;  // Right.
        break;
      case 'Q':
        delta.y() = -speed;  // Down.
        break;
      case 'E':
        delta.y() = speed;  // Up.
        break;
      case '\x1B':  // Escape.
        *should_close = true;
        return;
      default:
        break;
    }

    if (!delta.isZero()) {
      TranslateCamera(delta, camera);
    }
  } else if (auto* drag = std::get_if<MouseDragEvent>(&event)) {
    float yaw = -(*drag)(0) * kMouseSensitivity;
    float pitch = -(*drag)(1) * kMouseSensitivity;

    PanCamera(yaw, camera);
    TiltCamera(pitch, camera);
  } else if (auto* scroll = std::get_if<MouseScrollEvent>(&event)) {
    if (*scroll > 0.0f) {
      state->move_speed *= kScrollSensitivityMultiplier;
    } else if (*scroll < 0.0f) {
      state->move_speed /= kScrollSensitivityMultiplier;
    }
    state->move_speed = std::clamp(state->move_speed, 0.1f, 100.0f);
    LOG(INFO) << "Move speed: " << state->move_speed;
  }
}

}  // namespace sh_renderer