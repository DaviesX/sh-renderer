#include "window.h"

#include <glog/logging.h>

#include <optional>
#include <string_view>

namespace sh_renderer {

std::optional<Window> CreateWindow(unsigned width, unsigned height,
                                   std::string_view title) {}

void DestroyWindow(Window window) {}

}  // namespace sh_renderer