#include "embedded_glsl.h"

#include <algorithm>
#include <optional>
#include <string>

namespace sh_renderer {

FileReader MakeEmbeddedReader() {
  return [](std::string_view path) -> std::optional<std::string> {
    const std::span<const EmbeddedFile> files = EmbeddedGlslFiles();
    auto it =
        std::lower_bound(files.begin(), files.end(), path,
                         [](const EmbeddedFile& file, std::string_view p) {
                           return file.path < p;
                         });
    if (it == files.end() || it->path != path) return std::nullopt;
    return std::string(it->contents);
  };
}

}  // namespace sh_renderer
