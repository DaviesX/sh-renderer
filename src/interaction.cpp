#include "interaction.h"

#include <glog/logging.h>

#include "camera.h"
#include "input.h"

namespace sh_renderer {

void HandleInputEvent(const InputEvent &event, Camera *camera,
                      bool *should_close) {
  // TODO: FPS camera controls (WASD + QE + Mouse). Scrolling to change
  // velocity.
}

}  // namespace sh_renderer