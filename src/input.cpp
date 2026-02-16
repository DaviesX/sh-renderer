#include "input.h"

#include <glog/logging.h>

#include <optional>

namespace sh_renderer {

std::optional<InputEvent> PollInputEvent(Window window, InputState *state) {
  // TODO: Implement input polling.
  return std::nullopt;
}

}  // namespace sh_renderer
