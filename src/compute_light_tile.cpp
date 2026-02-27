#include "compute_light_tile.h"

#include <glog/logging.h>

#include "glad.h"

namespace sh_renderer {
namespace {

const uint32_t kMaxLightsPerTile = 256;
const uint32_t kTileSize = 16;
const char* kLightCullCompute = "glsl/light_cull.comp";

}  // namespace

ShaderProgram CreateLightCullProgram() {
  auto program = ShaderProgram::CreateCompute(kLightCullCompute);
  if (!program) {
    LOG(FATAL) << "Failed to create light cull compute shader program.";
    return {};
  }
  return std::move(*program);
}

TileLightListList CreateTileLightList(uint32_t width, uint32_t height) {
  CHECK_GT(width, 0);
  CHECK_GT(height, 0);

  TileLightListList result;
  result.screen_width = width;
  result.screen_height = height;
  result.tile_count_x = (width + kTileSize - 1) / kTileSize;
  result.tile_count_y = (height + kTileSize - 1) / kTileSize;

  uint32_t total_tiles = result.tile_count_x * result.tile_count_y;

  // SSBO layout:
  // [header: total_tiles * 2 uints (offset, count per tile)]
  // [data:   total_tiles * MAX_LIGHTS_PER_TILE uints (light indices)]
  size_t header_size = total_tiles * 2 * sizeof(uint32_t);
  size_t data_size = total_tiles * kMaxLightsPerTile * sizeof(uint32_t);
  size_t total_size = header_size + data_size;

  result.tile_light_index_ssbo = CreateSSBO(nullptr, total_size);

  // Create debug heatmap texture (RGBA8).
  glCreateTextures(GL_TEXTURE_2D, 1, &result.debug_heatmap_texture);
  glTextureStorage2D(result.debug_heatmap_texture, 1, GL_RGBA8, width, height);

  LOG(INFO) << "Created light tile resources: " << result.tile_count_x << "x"
            << result.tile_count_y << " tiles (" << total_tiles
            << " total), SSBO size: " << total_size << " bytes.";

  return result;
}

void DestroyTileLightList(TileLightListList* tile_light_list) {
  if (tile_light_list->tile_light_index_ssbo.id != 0) {
    DestroySSBO(tile_light_list->tile_light_index_ssbo);
    tile_light_list->tile_light_index_ssbo = {};
  }
  if (tile_light_list->debug_heatmap_texture != 0) {
    glDeleteTextures(1, &tile_light_list->debug_heatmap_texture);
    tile_light_list->debug_heatmap_texture = 0;
  }
}

bool ResizeTileLightList(uint32_t width, uint32_t height,
                         TileLightListList* tile_light_list) {
  if (tile_light_list->screen_width == width &&
      tile_light_list->screen_height == height) {
    return false;
  }
  DestroyTileLightList(tile_light_list);
  *tile_light_list = CreateTileLightList(width, height);
  return true;
}

void ComputeTileLightList(const Camera& camera, const RenderTarget& hdr_target,
                          const Scene& scene, const ShaderProgram& cull_program,
                          TileLightListList* tile_light_list) {
  if (!cull_program) return;

  // Resize if necessary.
  ResizeTileLightList(hdr_target.width, hdr_target.height, tile_light_list);

  cull_program.Use();

  // Bind SSBOs.
  BindSSBO(scene.point_light_list_ssbo, 0);
  BindSSBO(scene.spot_light_list_ssbo, 1);
  BindSSBO(tile_light_list->tile_light_index_ssbo, 2);

  // Bind depth texture.
  glBindTextureUnit(15, hdr_target.depth_buffer);

  // Bind debug heatmap as image.
  glBindImageTexture(14, tile_light_list->debug_heatmap_texture, 0, GL_FALSE, 0,
                     GL_WRITE_ONLY, GL_RGBA8);

  // Set uniforms.
  Eigen::Matrix4f projection = GetProjectionMatrix(camera);
  Eigen::Matrix4f view = GetViewMatrix(camera);
  Eigen::Matrix4f inv_projection = projection.inverse();

  cull_program.Uniform("u_projection", projection);
  cull_program.Uniform("u_inv_projection", inv_projection);
  cull_program.Uniform("u_view", view);

  // u_screen_size is ivec2 — use glUniform2i directly.
  GLint loc = glGetUniformLocation(cull_program.id(), "u_screen_size");
  glUniform2i(loc, hdr_target.width, hdr_target.height);

  // Dispatch.
  glDispatchCompute(tile_light_list->tile_count_x,
                    tile_light_list->tile_count_y, 1);

  // Barrier to ensure compute writes are visible to fragment shader.
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                  GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void BindTileLightList(const Scene& scene,
                       const TileLightListList& tile_light_list) {
  BindSSBO(scene.point_light_list_ssbo, 0);
  BindSSBO(scene.spot_light_list_ssbo, 1);
  BindSSBO(tile_light_list.tile_light_index_ssbo, 2);
}

}  // namespace sh_renderer