#pragma once

#include <Eigen/Dense>
#include <filesystem>

#include "scene.h"

namespace sh_renderer {

// Scene I/O used only by the glTF path (sh_renderer_gltf): the glTF loader's
// light conversion and the EXR lightmap reader. The core library never links
// these, so it carries no tinyexr dependency.

// Computes the bounding radius of a light from its flux via inverse-square law.
// radius = sqrt(flux / threshold), where flux = intensity * max(color).
float ComputeLightRadius(float intensity, const Eigen::Vector3f& color,
                         float threshold = 0.01f);

// Loads the SH lightmap EXRs into the scene.
void LoadLightmaps(Scene& scene, const std::filesystem::path& gltf_file);

}  // namespace sh_renderer
