#pragma once

#include "input.h"

namespace sh_renderer {

/*
 * @brief Handles input events.
 *
 * @param event The input event to handle.
 * @param camera The camera to handle input events for.
 * @param should_close A pointer to a boolean that will be set to true if the
 * window should be closed.
 */
void HandleInputEvent(const InputEvent &event, Camera *camera,
                      bool *should_close);

}  // namespace sh_renderer
