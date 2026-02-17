#include <gflags/gflags.h>
#include <glog/logging.h>

#include <filesystem>
#include <string>

#include "draw_depth.h"
#include "draw_radiance.h"
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

  ShaderProgram unlit_program = CreateUnlitProgram();
  ShaderProgram depth_program = CreateDepthProgram();
  ShaderProgram depth_vis_program = CreateDepthVisualizerProgram();

  if (!unlit_program || !depth_program || !depth_vis_program) {
    LOG(ERROR) << "Failed to create shader programs.";
    return;
  }

  // Initial depth target
  int initial_width, initial_height;
  glfwGetFramebufferSize(*window, &initial_width, &initial_height);
  RenderTarget depth_target = CreateDepthTarget(initial_width, initial_height);

  InputState input_state;
  InteractionState interaction_state;
  bool should_close = false;

  while (!glfwWindowShouldClose(*window) && !should_close) {
    // Process all queued input events.
    std::vector<InputEvent> events = PollInputEvents(*window, &input_state);
    for (const auto& event : events) {
      HandleInputEvent(event, &interaction_state, &camera, &should_close);
    }

    // Get the current framebuffer size for the viewport.
    int fb_width, fb_height;
    glfwGetFramebufferSize(*window, &fb_width, &fb_height);

    // Resize depth target if needed
    if (fb_width != depth_target.width || fb_height != depth_target.height) {
      glDeleteFramebuffers(1, &depth_target.fbo);
      glDeleteTextures(1, &depth_target.depth_buffer);
      depth_target = CreateDepthTarget(fb_width, fb_height);
    }

    glViewport(0, 0, fb_width, fb_height);

    // Enable depth testing.
    glEnable(GL_DEPTH_TEST);

    // Clear to a dark teal color.
    glClearColor(0.05f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = static_cast<float>(fb_width) / static_cast<float>(fb_height);
    camera.intrinsics.aspect_ratio = aspect;

    // 1. Depth Pre-pass
    DrawDepth(*scene, camera, depth_program, depth_target);

    // 2. Visualization (Overwrite screen)
    DrawDepthVisualization(depth_target, camera, depth_vis_program);

    // DrawSceneUnlit(*scene, camera, unlit_program);

    glfwSwapBuffers(*window);
  }

  DestroyWindow(*window);

  // Cleanup
  glDeleteFramebuffers(1, &depth_target.fbo);
  glDeleteTextures(1, &depth_target.depth_buffer);
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
