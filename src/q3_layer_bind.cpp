#include "q3_layer_bind.h"

#include <algorithm>
#include <cmath>

#include "glad.h"
#include "ssbo.h"

namespace sh_renderer {

void BindLayerCompositor(const Scene& scene, const ShaderProgram& program,
                         float time) {
  BindSSBO(scene.material_range_ssbo, 3);
  BindSSBO(scene.material_layer_ssbo, 4);
  BindSSBO(scene.material_tcmod_ssbo, 5);
  program.Uniform("u_time", time);
}

void BindMaterialLayers(const Material& mat, float time) {
  int n = std::min(static_cast<int>(mat.layers.size()), kMaxLayers);
  for (int j = 0; j < n; ++j) {
    const Layer& layer = mat.layers[j];
    uint32_t tex_id = layer.texture.texture_id;
    if (!layer.anim_frames.empty()) {
      int count = static_cast<int>(layer.anim_frames.size());
      int frame = static_cast<int>(std::floor(time * layer.anim_freq));
      frame = ((frame % count) + count) % count;  // wrap, tolerate negatives
      tex_id = layer.anim_frames[frame].texture_id;
    }
    glBindTextureUnit(kLayerSamplerBase + j, tex_id);
  }
  for (int j = n; j < kMaxLayers; ++j) {
    glBindTextureUnit(kLayerSamplerBase + j, mat.albedo.texture_id);
  }
}

}  // namespace sh_renderer
