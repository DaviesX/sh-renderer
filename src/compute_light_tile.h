#pragma once

#include <vector>

#include "camera.h"
#include "render_target.h"
#include "scene.h"
#include "ssbo.h"

namespace sh_renderer {

const uint32_t kMaxLights = 1024;
const uint32_t kMaxLightsPerTile = 256;
const uint32_t kTileSize = 16;

// TODO: Design the data structure for light list and light tile list.

SSBO CreateLightListSSBO();

SSBO CreateLightTileListSSBO(uint32_t width, uint32_t height);

void UpdateLightListSSBO(const std::vector<Light> &lights, const SSBO &ssbo);

void ComputeTileLightList(const Camera &camera, const RenderTarget &depth_map,
                          const SSBO &light_list_ssbo,
                          const SSBO &light_tile_list_ssbo);

}  // namespace sh_renderer