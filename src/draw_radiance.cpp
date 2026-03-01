#include "draw_radiance.h"

#include <glog/logging.h>

#include "cascade.h"
#include "compute_light_tile.h"
#include "culling.h"
#include "draw_sky.h"
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
                       const std::vector<RenderTarget>& sun_shadow_maps,
                       const std::vector<Cascade>& sun_cascades,
                       const RenderTarget& spot_shadow_atlas,
                       const TileLightListList& tile_light_list,
                       const RenderTarget& ssao_target,
                       const ShaderProgram& program,
                       const RenderTarget& hdr_target) {
  if (!program) return;
  program.Use();

  // Bind Sun Shadow Maps and set uniforms
  if (!sun_cascades.empty() && !sun_shadow_maps.empty()) {
    for (size_t i = 0; i < sun_cascades.size(); ++i) {
      if (i >= 3) break;  // Hardcoded limit in shader

      // Bind texture unit 5 + i
      glBindTextureUnit(5 + i, sun_shadow_maps[i].depth_buffer);

      // Set uniforms
      std::string base = "u_sun_cascade_splits[" + std::to_string(i) + "]";
      program.Uniform(base, sun_cascades[i].split_depth);

      base = "u_sun_cascade_view_projections[" + std::to_string(i) + "]";
      program.Uniform(base, sun_cascades[i].view_projection_matrix);
    }
  }

  // Bind FBO
  glBindFramebuffer(GL_FRAMEBUFFER, hdr_target.fbo);
  glViewport(0, 0, hdr_target.width, hdr_target.height);

  // Clear Color (Radiance) but KEEP Depth (from pre-pass)
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Z-Prepass State
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);

  // Set u_view matrix for shadow selection
  program.Uniform("u_view", GetViewMatrix(camera));

  program.Use();
  program.Uniform("u_view_proj", GetViewProjMatrix(camera));
  program.Uniform("u_camera_pos", camera.position);

  // Sun Light (Hardcoded for now as per instructions usually, but we have a
  // scene light?) Finding the first directional light in the scene for now.
  const SunLight* sun = scene.sun_light ? &(*scene.sun_light) : nullptr;

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

  program.Uniform("u_sky_color", kSkyColor);

  // Forward+ tile info.
  BindTileLightList(scene, tile_light_list);
  GLint tile_count_loc = glGetUniformLocation(program.id(), "u_tile_count");
  glUniform2i(tile_count_loc, tile_light_list.tile_count_x,
              tile_light_list.tile_count_y);
  GLint screen_size_loc = glGetUniformLocation(program.id(), "u_screen_size");
  glUniform2i(screen_size_loc, hdr_target.width, hdr_target.height);

  // Bind Lightmap Textures
  if (scene.lightmaps_packed[0].texture_id != 0) {
    glBindTextureUnit(8, scene.lightmaps_packed[0].texture_id);
    glBindTextureUnit(9, scene.lightmaps_packed[1].texture_id);
    glBindTextureUnit(10, scene.lightmaps_packed[2].texture_id);
  } else {
    glBindTextureUnit(8, 0);
    glBindTextureUnit(9, 0);
    glBindTextureUnit(10, 0);
  }

  // Bind Shadow Atlas
  if (spot_shadow_atlas.depth_buffer != 0) {
    glBindTextureUnit(11, spot_shadow_atlas.depth_buffer);
  } else {
    glBindTextureUnit(11, 0);
  }

  // Bind SSAO texture
  if (ssao_target.texture != 0) {
    glBindTextureUnit(12, ssao_target.texture);
  } else {
    glBindTextureUnit(12, 0);
  }

  Eigen::Vector4f planes[6];
  ExtractFrustumPlanes(GetViewProjMatrix(camera), planes);

  int current_material_id = -2;

  for (const auto& geo : scene.geometries) {
    if (geo.vao == 0) continue;
    if (!IsAABBInFrustum(geo.bounding_box, planes)) continue;

    program.Uniform("u_model", geo.transform.matrix());

    if (geo.material_id != current_material_id) {
      current_material_id = geo.material_id;

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