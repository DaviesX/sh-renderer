#pragma once

#include <filesystem>
#include <optional>

#include "scene.h"

namespace sh_renderer {

// Loads a glTF file and returns a Scene object.
// Returns std::nullopt if loading fails.
std::optional<Scene> LoadScene(const std::filesystem::path& gltf_file);

}  // namespace sh_renderer
