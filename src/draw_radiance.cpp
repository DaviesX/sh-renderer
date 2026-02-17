#include "draw_radiance.h"

#include <glog/logging.h>

#include "glad.h"
#include "shader.h"

namespace sh_renderer {

namespace {

const char* kRadianceVertex = "glsl/radiance.vert";
const char* kRadianceFragment = "glsl/radiance.frag";

}  // namespace

ShaderProgram CreateRadianceProgram() {
  auto program =
      ShaderProgram::CreateGraphics(kRadianceVertex, kRadianceFragment);
  if (!program) {
    LOG(FATAL) << "Failed to create radiance shader program.";
    return {};
  }
  return std::move(*program);
}

void DrawSceneRadiance(const Scene& scene, const Camera& camera,
                       const ShaderProgram& program,
                       const RenderTarget& hdr_target) {
  if (!program) return;

  // Bind FBO and resize if needed (though resize is handled in main loop
  // mostly)
  glBindFramebuffer(GL_FRAMEBUFFER, hdr_target.fbo);
  glViewport(0, 0, hdr_target.width, hdr_target.height);

  // Clear Color (Radiance) but KEEP Depth (from pre-pass)
  // We assume the caller has already populated the depth buffer.
  glClearColor(0.0f, 0.0f, 0.0f,
               0.0f);  // Clear HDR target to black/transparent
  glClear(GL_COLOR_BUFFER_BIT);

  // Z-Prepass State
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);  // Don't write depth, it's already there

  program.Use();
  program.Uniform("u_view_proj", GetViewProjMatrix(camera));
  program.Uniform("u_camera_pos", camera.position);

  // Sun Light (Hardcoded for now as per instructions usually, but we have a
  // scene light?) Finding the first directional light in the scene for now.
  const Light* sun = nullptr;
  for (const auto& light : scene.lights) {
    if (light.type == Light::Type::Directional) {
      sun = &light;
      break;
    }
  }

  if (sun) {
    program.Uniform("u_sun.direction", sun->direction);
    program.Uniform("u_sun.color", sun->color);
    program.Uniform("u_sun.intensity", sun->intensity);
  } else {
    // Default sun if none found
    program.Uniform("u_sun.direction",
                    Eigen::Vector3f(0.5f, -1.0f, 0.1f).normalized());
    program.Uniform("u_sun.color", Eigen::Vector3f(1.0f, 1.0f, 1.0f));
    program.Uniform("u_sun.intensity", 1.0f);
  }

  for (const auto& geo : scene.geometries) {
    if (geo.vao == 0) continue;

    program.Uniform("u_model", geo.transform.matrix());

    if (geo.material_id >= 0 &&
        static_cast<size_t>(geo.material_id) < scene.materials.size()) {
      const auto& mat = scene.materials[geo.material_id];
      glBindTextureUnit(0, mat.albedo.texture_id);
      glBindTextureUnit(1, mat.normal_texture.texture_id);
      glBindTextureUnit(2, mat.metallic_roughness_texture.texture_id);

      program.Uniform("u_emissive_factor", mat.emissive_factor);
      program.Uniform("u_emissive_strength", mat.emissive_strength);

      if (mat.emissive_texture) {
        program.Uniform("u_has_emissive_texture", 1);
        glBindTextureUnit(3, mat.emissive_texture->texture_id);
      } else {
        program.Uniform("u_has_emissive_texture", 0);
        glBindTextureUnit(3, 0);
      }
    } else {
      glBindTextureUnit(0, 0);
      glBindTextureUnit(1, 0);
      glBindTextureUnit(2, 0);
      glBindTextureUnit(3, 0);
      program.Uniform("u_has_emissive_texture", 0);
      program.Uniform("u_emissive_factor",
                      Eigen::Vector3f(Eigen::Vector3f::Zero()));
      program.Uniform("u_emissive_strength", 0.0f);
    }

    glBindVertexArray(geo.vao);
    if (geo.index_count > 0) {
      glDrawElements(GL_TRIANGLES, geo.index_count, GL_UNSIGNED_INT, nullptr);
    } else {
      glDrawArrays(GL_TRIANGLES, 0, geo.vertices.size());
    }
  }

  // Restore State
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);
}

}  // namespace sh_renderer