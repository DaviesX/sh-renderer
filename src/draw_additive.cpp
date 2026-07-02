#include "draw_additive.h"

#include <glog/logging.h>

#include <string>

#include "culling.h"
#include "glad.h"
#include "q3_layer_bind.h"
#include "shader.h"

namespace sh_renderer {

namespace {

const char* kAdditiveVertex = "glsl/radiance.vert";
const char* kAdditiveFragment = "glsl/additive.frag";

}  // namespace

ShaderProgram CreateAdditiveProgram() {
  auto program = ShaderProgram::CreateGraphics(
      kAdditiveVertex, kAdditiveFragment,
      {{"MAX_LAYERS", std::to_string(kMaxLayers)}});
  if (!program) {
    LOG(FATAL) << "Failed to create additive shader program.";
    return {};
  }
  return std::move(*program);
}

void DrawAdditive(const Scene& scene, const Camera& camera,
                  const ShaderProgram& program, const RenderTarget& hdr_target,
                  float time) {
  if (!program) return;
  program.Use();

  glBindFramebuffer(GL_FRAMEBUFFER, hdr_target.fbo);
  glViewport(0, 0, hdr_target.width, hdr_target.height);

  // Test against the opaque depth (shared with the pre-pass) so flames behind
  // walls are occluded, but do not write depth -- additive surfaces are
  // translucent and order-independent.
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);

  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_ONE, GL_ONE);

  // Additive sprites/grates are typically two-sided (cull none); draw both
  // faces. Per-material cull modes are Step 6.
  glDisable(GL_CULL_FACE);

  program.Uniform("u_view_proj", GetViewProjMatrix(camera));
  BindLayerCompositor(scene, program, time);

  Eigen::Vector4f planes[6];
  ExtractFrustumPlanes(GetViewProjMatrix(camera), planes);

  for (const auto& geo : scene.geometries) {
    if (geo.vao == 0) continue;
    if (geo.material_id < 0 ||
        static_cast<size_t>(geo.material_id) >= scene.materials.size()) {
      continue;
    }
    const Material& mat = scene.materials[geo.material_id];
    if (!mat.additive) continue;
    if (!IsAABBInFrustum(geo.bounding_box, planes)) continue;

    program.Uniform("u_model", geo.transform.matrix());
    glBindTextureUnit(0, mat.albedo.texture_id);
    program.Uniform("u_material_index", geo.material_id);
    BindMaterialLayers(mat, time);

    glBindVertexArray(geo.vao);
    if (geo.index_count > 0) {
      glDrawElements(GL_TRIANGLES, geo.index_count, GL_UNSIGNED_INT, nullptr);
    } else {
      glDrawArrays(GL_TRIANGLES, 0, geo.vertices.size());
    }
  }

  // Restore state.
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);
  glEnable(GL_CULL_FACE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

}  // namespace sh_renderer
