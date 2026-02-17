#pragma once

#include "camera.h"
#include "input.h"

namespace sh_renderer {

constexpr float kDefaultMoveSpeed = 2.0f;
constexpr float kMouseSensitivity = 3.0f;
constexpr float kScrollSensitivityMultiplier = 1.2f;

struct InteractionState {
  float move_speed = kDefaultMoveSpeed;
};

/*
 * @brief Handles input events.
 *
 * @param event The input event to handle.
 * @param state The interaction state (e.g. move speed).
 * @param camera The camera to handle input events for.
 * @param should_close A pointer to a boolean that will be set to true if the
 * window should be closed.
 */
void HandleInputEvent(const InputEvent &event, InteractionState *state,
                      Camera *camera, bool *should_close);

}  // namespace sh_renderer
