#pragma once

#include <filesystem>
#include <optional>

#include "scene.h"

namespace tinygltf {
class Model;
struct Material;
}  // namespace tinygltf

namespace sh_renderer {

// Loads a glTF file and returns a Scene object.
// Returns std::nullopt if loading fails.
std::optional<Scene> LoadScene(const std::filesystem::path& gltf_file);

// Parses the `SH_material_layers` extension on `gltf_mat` into `mat->layers`
// (loading each layer + animMap-frame texture from `model`), and sets
// `mat->base_layer` / `mat->cull_mode`. No-op when the extension is absent.
// Exposed for testing; called by the loader during material processing.
void ParseMaterialLayers(const tinygltf::Model& model,
                         const tinygltf::Material& gltf_mat,
                         const std::filesystem::path& base_path, Material* mat);

}  // namespace sh_renderer
