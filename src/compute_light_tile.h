#pragma once

#include <cstdint>

#include "camera.h"
#include "render_target.h"
#include "scene.h"
#include "shader.h"
#include "ssbo.h"

namespace sh_renderer {

// Resources for tile-based light culling.
struct TileLightListList {
  SSBO tile_light_index_ssbo;  // Per-tile light index list.
  uint32_t debug_heatmap_texture = 0;
  uint32_t tile_count_x = 0;
  uint32_t tile_count_y = 0;
  uint32_t screen_width = 0;
  uint32_t screen_height = 0;
};

// Creates the light cull compute shader program.
ShaderProgram CreateLightCullProgram();

// Creates the tile resources sized for the given screen dimensions.
TileLightListList CreateTileLightList(uint32_t width, uint32_t height);

// Destroys the tile resources.
void DestroyTileLightList(TileLightListList* tile_light_list);

// Resizes tile resources if screen dimensions changed.
// Returns true if resources were resized.
bool ResizeLightTileList(uint32_t width, uint32_t height,
                         TileLightListList* tile_light_list);

// Dispatches the compute shader to build per-tile light lists.
void ComputeTileLightList(const Camera& camera, const RenderTarget& hdr_target,
                          const Scene& scene, const ShaderProgram& cull_program,
                          TileLightListList* tile_light_list);

// Binds the tile light SSBOs for consumption by the forward pass.
void BindTileLightList(const Scene& scene,
                       const TileLightListList& tile_light_list);

}  // namespace sh_renderer