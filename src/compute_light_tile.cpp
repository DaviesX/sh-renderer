#include "compute_light_tile.h"

namespace sh_renderer {
namespace {

const uint32_t kMaxLightsPerTile = 256;
const uint32_t kTileSize = 16;

}  // namespace

SSBO CreateLightListSSBO() {
  // TODO: Implement this function.
}

SSBO CreateLightTileListSSBO(uint32_t width, uint32_t height) {
  // TODO: Implement this function.
}

void UpdateLightListSSBO(const std::vector<Light> &lights, const SSBO &ssbo) {
  // TODO: Implement this function.
}

void ComputeTileLightList(const Camera &camera, const RenderTarget &depth_map,
                          const SSBO &light_list_ssbo,
                          const SSBO &light_tile_list_ssbo) {
  // TODO: Implement this function.
}

}  // namespace sh_renderer