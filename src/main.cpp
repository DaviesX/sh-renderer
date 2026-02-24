#include <gflags/gflags.h>
#include <glog/logging.h>

#include <filesystem>
#include <string>

#include "compute_light_tile.h"
#include "draw_depth.h"
#include "draw_radiance.h"
#include "draw_shadow_map.h"
#include "draw_sky.h"
#include "draw_tonemap.h"
#include "input.h"
#include "interaction.h"
#include "loader.h"
#include "render_target.h"
#include "scene.h"
#include "window.h"

DEFINE_string(input, "", "Path to the glTF scene file to render.");
DEFINE_uint32(width, 1280, "Width of the window.");
DEFINE_uint32(height, 720, "Height of the window.");
DEFINE_uint32(msaa_samples, 0, "Number of MSAA samples.");
DEFINE_uint32(log_frame_time_interval, 100,
              "Log average frame time every N frames.");

namespace sh_renderer {

void Run(const std::filesystem::path& scene_path) {
  LOG(INFO) << "Loading scene: " << scene_path;

  auto window = CreateWindow(FLAGS_width, FLAGS_height, "SH Renderer",
                             FLAGS_msaa_samples);
  if (!window.has_value()) {
    LOG(ERROR) << "Failed to create window.";
    return;
  }

  Camera camera{
      .position = Eigen::Vector3f(0.0f, 1.0f, 3.0f),
      .orientation = Eigen::Quaternionf::Identity(),
  };

  std::optional<Scene> scene = LoadScene(scene_path);
  if (!scene) {
    LOG(ERROR) << "Failed to load scene: " << scene_path;
    return;
  }

  UploadSceneToGPU(*scene);
  UploadLightsToGPU(*scene);

  ShaderProgram cascaded_shadow_map_opaque_program =
      CreateShadowMapOpaqueProgram();
  ShaderProgram cascaded_shadow_map_cutout_program =
      CreateShadowMapCutoutProgram();
  ShaderProgram depth_opaque_program = CreateDepthOpaqueProgram();
  ShaderProgram depth_cutout_program = CreateDepthCutoutProgram();
  ShaderProgram depth_vis_program = CreateDepthVisualizerProgram();
  ShaderProgram shadow_vis_program = CreateShadowMapVisualizationProgram();
  ShaderProgram radiance_program = CreateRadianceProgram();
  ShaderProgram sky_program = CreateSkyAnalyticProgram();
  ShaderProgram tonemap_program = CreateTonemapProgram();
  ShaderProgram light_cull_program = CreateLightCullProgram();

  if (!cascaded_shadow_map_opaque_program ||
      !cascaded_shadow_map_cutout_program || !depth_opaque_program ||
      !depth_cutout_program || !depth_vis_program || !shadow_vis_program ||
      !radiance_program || !sky_program || !tonemap_program ||
      !light_cull_program) {
    LOG(ERROR) << "Failed to create shader programs.";
    return;
  }

  // Initial HDR target
  int initial_width, initial_height;
  glfwGetFramebufferSize(*window, &initial_width, &initial_height);
  RenderTarget hdr_target = CreateHDRTarget(initial_width, initial_height);
  std::vector<RenderTarget> sun_shadow_map_targets =
      CreateCascadedShadowMapTargets();
  LightTileResources tile_resources =
      CreateLightTileResources(initial_width, initial_height);

  InputState input_state;
  InteractionState interaction_state;
  bool should_close = false;

  uint32_t frame_count = 0;
  double last_time = glfwGetTime();

  while (!glfwWindowShouldClose(*window) && !should_close) {
    // Process all queued input events.
    std::vector<InputEvent> events = PollInputEvents(*window, &input_state);
    for (const auto& event : events) {
      HandleInputEvent(event, &interaction_state, &camera, &should_close);
    }

    // Get the current framebuffer size for the viewport.
    int fb_width, fb_height;
    glfwGetFramebufferSize(*window, &fb_width, &fb_height);

    // Resize HDR target if needed
    if (fb_width != hdr_target.width || fb_height != hdr_target.height) {
      glDeleteFramebuffers(1, &hdr_target.fbo);
      glDeleteTextures(1, &hdr_target.texture);
      glDeleteTextures(1, &hdr_target.depth_buffer);
      hdr_target = CreateHDRTarget(fb_width, fb_height);
    }

    glViewport(0, 0, fb_width, fb_height);

    // Enable depth testing.
    glEnable(GL_DEPTH_TEST);

    // 1. Depth Pre-pass
    // Bind HDR target but disable color writes
    glBindFramebuffer(GL_FRAMEBUFFER, hdr_target.fbo);
    glClear(GL_DEPTH_BUFFER_BIT);  // Clear depth only
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    std::vector<Cascade> sun_cascades;
    if (scene->sun_light) {
      sun_cascades = ComputeCascades(*(scene->sun_light), camera);
    }
    DrawCascadedShadowMap(*scene, camera, cascaded_shadow_map_opaque_program,
                          cascaded_shadow_map_cutout_program, sun_cascades,
                          sun_shadow_map_targets);

    DrawDepth(*scene, camera, depth_opaque_program, depth_cutout_program,
              hdr_target);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // 1.5. Compute Light Culling (Forward+)
    ComputeTileLightList(camera, hdr_target, *scene, light_cull_program,
                         tile_resources);

    // 2. Radiance Pass (Forward PBR)
    // DrawRadiance will handle clearing color, setting LEQUAL, etc.
    DrawSceneRadiance(*scene, camera, sun_shadow_map_targets, sun_cascades,
                      radiance_program, hdr_target, tile_resources);

    SunLight default_sun;
    default_sun.direction = Eigen::Vector3f(0.5f, -1.0f, 0.1f).normalized();
    default_sun.color = Eigen::Vector3f(1.0f, 1.0f, 1.0f);
    default_sun.intensity = 1.0f;
    SunLight active_sun = scene->sun_light.value_or(default_sun);

    DrawSkyAnalytic(*scene, camera, active_sun, hdr_target, sky_program);

    // 3. Tonemapping (to default framebuffer)
    DrawTonemap(hdr_target, tonemap_program);

    glfwSwapBuffers(*window);

    frame_count++;
    if (frame_count % FLAGS_log_frame_time_interval == 0) {
      double current_time = glfwGetTime();
      double average_time_ms =
          (current_time - last_time) * 1000.0 / FLAGS_log_frame_time_interval;
      LOG(INFO) << "Average frame time over last "
                << FLAGS_log_frame_time_interval
                << " frames: " << average_time_ms << " ms";
      last_time = current_time;
    }
  }

  DestroyWindow(*window);

  // Cleanup
  glDeleteFramebuffers(1, &hdr_target.fbo);
  glDeleteTextures(1, &hdr_target.texture);
  glDeleteTextures(1, &hdr_target.depth_buffer);
  for (const auto& target : sun_shadow_map_targets) {
    glDeleteFramebuffers(1, &target.fbo);
    glDeleteTextures(1, &target.depth_buffer);
  }
  DestroyLightTileResources(tile_resources);
}

}  // namespace sh_renderer

int main(int argc, char** argv) {
  gflags::SetUsageMessage("Simulating mixed lighting renderer.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  // Log to stderr also
  FLAGS_logtostderr = 1;

  if (FLAGS_input.empty()) {
    LOG(ERROR)
        << "No input file specified. Use --input to specify a glTF file.";
    return 1;
  }

  sh_renderer::Run(FLAGS_input);

  gflags::ShutDownCommandLineFlags();
  return 0;
}
