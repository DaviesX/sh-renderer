#include "frame.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <utility>

#include "cascade.h"
#include "draw_depth.h"
#include "draw_radiance.h"
#include "draw_shadow_map.h"
#include "draw_sky.h"
#include "draw_tonemap.h"
#include "glad.h"
#include "log.h"

namespace sh_renderer {

namespace {

void DeleteColorTarget(RenderTarget* target) {
  glDeleteFramebuffers(1, &target->fbo);
  glDeleteTextures(1, &target->texture);
  *target = RenderTarget{};
}

void DeleteSizedTargets(Renderer& renderer) {
  glDeleteFramebuffers(1, &renderer.depth_normal_target.fbo);
  glDeleteTextures(1, &renderer.depth_normal_target.normal_texture);
  glDeleteTextures(1, &renderer.depth_normal_target.depth_buffer);
  renderer.depth_normal_target = RenderTarget{};

  // The HDR target shares the depth texture deleted above.
  DeleteColorTarget(&renderer.hdr_target);
  DeleteColorTarget(&renderer.ssao_target);
  DeleteColorTarget(&renderer.ssao_blur_temp);
  DeleteColorTarget(&renderer.ssao_blur_target);
}

void CreateSizedTargets(Renderer& renderer, int width, int height) {
  renderer.depth_normal_target = CreateDepthAndNormalTarget(width, height);
  renderer.hdr_target =
      CreateHDRTarget(width, height, renderer.depth_normal_target.depth_buffer);
  renderer.ssao_target = CreateSSAOTarget(width, height);
  renderer.ssao_blur_temp = CreateSSAOTarget(width, height);
  renderer.ssao_blur_target = CreateSSAOTarget(width, height);
  renderer.width = width;
  renderer.height = height;
}

void DeleteSpotShadowAtlas(Renderer& renderer) {
  glDeleteFramebuffers(1, &renderer.spot_shadow_atlas.fbo);
  glDeleteTextures(1, &renderer.spot_shadow_atlas.depth_buffer);
  renderer.spot_shadow_atlas = RenderTarget{};
  renderer.spot_shadow_atlas_resolution = 0;
}

struct ProgramSlot {
  const char* name;
  const char* files;
  ShaderProgram (*create)(std::string* error);
  ShaderProgram Renderer::* program;
};

ShaderProgram CreateSSAOBlurHorizontalProgram(std::string* error) {
  return CreateSSAOBlurProgram(true, error);
}

ShaderProgram CreateSSAOBlurVerticalProgram(std::string* error) {
  return CreateSSAOBlurProgram(false, error);
}

}  // namespace

std::unique_ptr<Renderer> CreateRenderer(std::string* error) {
  auto renderer = std::make_unique<Renderer>();

  // The same order as the viewer built them before the frame API.
  const ProgramSlot slots[] = {
      {"shadow map (opaque)", "glsl/depth.vert, glsl/depth.frag",
       &CreateShadowMapOpaqueProgram, &Renderer::shadow_map_opaque_program},
      {"shadow map (cutout)", "glsl/depth.vert, glsl/depth.frag",
       &CreateShadowMapCutoutProgram, &Renderer::shadow_map_cutout_program},
      {"depth pre-pass (opaque)",
       "glsl/depth_w_normal.vert, glsl/depth_w_normal.frag",
       &CreateDepthOpaqueWNormalProgram, &Renderer::depth_opaque_program},
      {"depth pre-pass (cutout)",
       "glsl/depth_w_normal.vert, glsl/depth_w_normal.frag",
       &CreateDepthCutoutWNormalProgram, &Renderer::depth_cutout_program},
      {"radiance", "glsl/radiance.vert, glsl/radiance.frag",
       &CreateRadianceProgram, &Renderer::radiance_program},
      {"sky", "glsl/sky_analytic.vert, glsl/sky_analytic.frag",
       &CreateSkyAnalyticProgram, &Renderer::sky_program},
      {"tonemap", "glsl/tonemap.vert, glsl/tonemap.frag", &CreateTonemapProgram,
       &Renderer::tonemap_program},
      {"light culling", "glsl/light_cull.comp", &CreateLightCullProgram,
       &Renderer::light_cull_program},
      {"SSAO", "glsl/fullscreen.vert, glsl/ssao.frag", &CreateSSAOProgram,
       &Renderer::ssao_program},
      {"SSAO blur (horizontal)",
       "glsl/fullscreen.vert, glsl/bilateral_blur.frag",
       &CreateSSAOBlurHorizontalProgram,
       &Renderer::ssao_blur_horizontal_program},
      {"SSAO blur (vertical)", "glsl/fullscreen.vert, glsl/bilateral_blur.frag",
       &CreateSSAOBlurVerticalProgram, &Renderer::ssao_blur_vertical_program},
  };

  // Each failure contributes its program, its files and the reason (a
  // missing file, or the driver's info log).
  std::string failed;
  for (const ProgramSlot& slot : slots) {
    std::string reason;
    (*renderer).*slot.program = slot.create(&reason);
    if (!((*renderer).*slot.program)) {
      if (!failed.empty()) failed += "\n";
      failed += std::string(slot.name) + " (" + slot.files + "): " + reason;
    }
  }
  if (!failed.empty()) {
    if (error != nullptr) {
      *error = "Failed to create shader programs:\n" + failed;
    }
    return nullptr;
  }

  glCreateVertexArrays(1, &renderer->empty_vao);
  renderer->sun_shadow_map_targets = CreateCascadedShadowMapTargets();
  renderer->ssao_context = CreateSSAOContext();

  // Every host gets the sRGB encode of the default framebuffer, once the
  // renderer exists: a failed create leaves the host's GL state alone.
  glEnable(GL_FRAMEBUFFER_SRGB);
  return renderer;
}

bool BeginFrame(Renderer& renderer, int width, int height) {
  renderer.in_frame = false;
  if (width <= 0 || height <= 0) return false;

  if (renderer.width == 0) {
    CreateSizedTargets(renderer, width, height);
    renderer.tile_light_list = CreateTileLightList(width, height);
  } else if (width != renderer.width || height != renderer.height) {
    // The tile light list follows the HDR target's size by itself
    // (ComputeTileLightList).
    DeleteSizedTargets(renderer);
    CreateSizedTargets(renderer, width, height);
  }

  glViewport(0, 0, width, height);
  glEnable(GL_DEPTH_TEST);
  renderer.in_frame = true;
  return true;
}

void RenderScene(Renderer& renderer, Scene* scene, const Camera& camera,
                 double time_seconds) {
  if (!renderer.in_frame) return;

  const float clear[4] = {renderer.clear_color.x(), renderer.clear_color.y(),
                          renderer.clear_color.z(), 1.0f};
  // The clear honours the colour mask, which the depth pre-pass of an
  // earlier frame (or the host) may have left off.
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glClearNamedFramebufferfv(renderer.hdr_target.fbo, GL_COLOR, 0, clear);

  if (scene == nullptr) return;

  // Scene-sized resources.
  if (renderer.spot_shadow_atlas_resolution != scene->shadow_atlas.resolution) {
    if (renderer.spot_shadow_atlas_resolution != 0) {
      DeleteSpotShadowAtlas(renderer);
    }
    renderer.spot_shadow_atlas =
        CreateShadowAtlasTarget(scene->shadow_atlas.resolution);
    renderer.spot_shadow_atlas_resolution = scene->shadow_atlas.resolution;
  }

  // 0. Update dynamic light data and render shadow atlas
  AllocateShadowMapForLights(*scene, camera);
  UploadLightsToGPU(*scene);

  DrawShadowAtlas(*scene, renderer.shadow_map_opaque_program,
                  renderer.shadow_map_cutout_program,
                  renderer.spot_shadow_atlas);

  // 1. Depth Pre-pass
  // Bind Depth+Normal target
  glBindFramebuffer(GL_FRAMEBUFFER, renderer.depth_normal_target.fbo);
  glClear(GL_DEPTH_BUFFER_BIT);  // Clear depth only
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  std::vector<Cascade> sun_cascades;
  if (scene->sun_light) {
    sun_cascades = ComputeCascades(*(scene->sun_light), camera);
  }
  DrawCascadedShadowMap(*scene, camera, renderer.shadow_map_opaque_program,
                        renderer.shadow_map_cutout_program, sun_cascades,
                        renderer.sun_shadow_map_targets);

  DrawDepthWNormal(*scene, camera, renderer.depth_opaque_program,
                   renderer.depth_cutout_program, renderer.depth_normal_target);

  // 1.2 SSAO Pass
  DrawSSAO(renderer.depth_normal_target, camera, renderer.ssao_program,
           renderer.ssao_context, renderer.ssao_target);
  DrawSSAOBlur(
      renderer.ssao_target, camera, renderer.ssao_blur_horizontal_program,
      renderer.ssao_blur_vertical_program, renderer.depth_normal_target,
      renderer.ssao_blur_temp, renderer.ssao_blur_target);

  // 1.5. Compute Light Culling (Forward+)
  ComputeTileLightList(camera, renderer.hdr_target, *scene,
                       renderer.light_cull_program, &renderer.tile_light_list);

  // 2. Radiance Pass (Forward PBR)
  // DrawRadiance will handle clearing color, setting LEQUAL, etc.
  DrawSceneRadiance(*scene, camera, renderer.sun_shadow_map_targets,
                    sun_cascades, renderer.spot_shadow_atlas,
                    renderer.tile_light_list, renderer.ssao_blur_target,
                    renderer.radiance_program, renderer.hdr_target,
                    static_cast<float>(time_seconds));

  SunLight default_sun;
  default_sun.direction = Eigen::Vector3f(0.5f, -1.0f, 0.1f).normalized();
  default_sun.color = Eigen::Vector3f(1.0f, 1.0f, 1.0f);
  default_sun.intensity = 1.0f;
  SunLight active_sun = scene->sun_light.value_or(default_sun);

  DrawSkyAnalytic(*scene, camera, active_sun, renderer.hdr_target,
                  renderer.sky_program);
}

void EndFrame(Renderer& renderer) {
  if (!renderer.in_frame) return;
  // 3. Tonemapping (to default framebuffer)
  glBindVertexArray(renderer.empty_vao);
  DrawTonemap(renderer.hdr_target, renderer.tonemap_program,
              SanitizeDisplayGamma(renderer.display_gamma));

  // Hand the host back the cheap state (see EndFrame in frame.h).
  glUseProgram(0);
  glBindVertexArray(0);
  glBindTextureUnit(0, 0);
  glDisable(GL_DEPTH_TEST);
  renderer.in_frame = false;
}

float SanitizeDisplayGamma(float gamma) {
  if (!std::isfinite(gamma) || gamma <= 0.0f) {
    static std::once_flag warned;
    std::call_once(warned, [gamma] {
      LOG(WARNING) << "Display gamma " << gamma
                   << " is not a positive finite number; using 1.";
    });
    return 1.0f;
  }
  return std::clamp(gamma, kMinDisplayGamma, kMaxDisplayGamma);
}

void SetDisplayGamma(Renderer& renderer, float gamma) {
  renderer.display_gamma = SanitizeDisplayGamma(gamma);
}

void ReadDefaultFramebufferRGB(int width, int height, uint8_t* rgb) {
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb);
}

void DestroyRenderer(std::unique_ptr<Renderer> renderer) {
  if (!renderer) return;
  if (renderer->width != 0) {
    DeleteSizedTargets(*renderer);
    DestroyTileLightList(&renderer->tile_light_list);
  }
  DestroySSAOContext(&renderer->ssao_context);
  glDeleteVertexArrays(1, &renderer->empty_vao);
  for (const auto& target : renderer->sun_shadow_map_targets) {
    glDeleteFramebuffers(1, &target.fbo);
    glDeleteTextures(1, &target.depth_buffer);
  }
  if (renderer->spot_shadow_atlas_resolution != 0) {
    DeleteSpotShadowAtlas(*renderer);
  }
  // The programs are deleted by their destructors.
  renderer.reset();
}

}  // namespace sh_renderer
