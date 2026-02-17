#include "draw_radiance.h"

#include <glog/logging.h>

#include "glad.h"
#include "shader.h"

namespace sh_renderer {

namespace {

// Simple Unlit Shader
const char* kUnlitVertex = "src/shaders/unlit.vert";
const char* kUnlitFragment = "src/shaders/unlit.frag";

}  // namespace

ShaderProgram CreateUnlitProgram() {
  auto program = ShaderProgram::CreateGraphics(kUnlitVertex, kUnlitFragment);
  if (!program) {
    LOG(FATAL) << "Failed to create unlit shader program.";
    return {};
  }
  return std::move(*program);
}

void DrawSceneRadiance(const Scene& scene, const Camera& camera,
                       const ShaderProgram& program) {
  // TODO: Phase 3.
}

// Actual implementation of DrawSceneUnlit
void DrawSceneUnlit(const Scene& scene, const Camera& camera,
                    const ShaderProgram& program) {
  if (!program) return;

  program.Use();
  program.Uniform("u_view_proj", GetViewProjMatrix(camera));

  for (const auto& geo : scene.geometries) {
    if (geo.vao == 0) continue;

    program.Uniform("u_model", geo.transform.matrix());

    // Bind Albedo
    if (geo.material_id >= 0 &&
        static_cast<size_t>(geo.material_id) < scene.materials.size()) {
      const auto& mat = scene.materials[geo.material_id];
      glBindTextureUnit(0, mat.albedo.texture_id);
    } else {
      glBindTextureUnit(0, 0);  // Invalid texture?
    }

    glBindVertexArray(geo.vao);
    if (geo.index_count > 0) {
      glDrawElements(GL_TRIANGLES, geo.index_count, GL_UNSIGNED_INT, nullptr);
    } else {
      glDrawArrays(GL_TRIANGLES, 0, geo.vertices.size());
    }
  }
}

}  // namespace sh_renderer