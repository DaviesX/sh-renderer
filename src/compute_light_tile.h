#pragma once

#include <vector>

#include "camera.h"
#include "render_target.h"
#include "scene.h"
#include "ssbo.h"

namespace sh_renderer {

// TODO: Designt the data structure for light tile list.

SSBO CreateLightTileListSSBO(uint32_t width, uint32_t height);

void ComputeTileLightList(const Camera &camera, const RenderTarget &depth_map,
                          const SSBO &light_list_ssbo,
                          const SSBO &light_tile_list_ssbo);

}  // namespace sh_renderer