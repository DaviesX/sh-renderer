#include <gflags/gflags.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "frame.h"
#include "input.h"
#include "interaction.h"
#include "loader.h"
#include "log.h"
#include "scene.h"
#include "window.h"

DEFINE_string(input, "", "Path to the glTF scene file to render.");
DEFINE_uint32(width, 1280, "Width of the window.");
DEFINE_uint32(height, 720, "Height of the window.");
DEFINE_uint32(msaa_samples, 0, "Number of MSAA samples.");
DEFINE_uint32(log_frame_time_interval, 100,
              "Log average frame time every N frames.");
DEFINE_uint32(readback_frame, 0,
              "Read back the default framebuffer of this presented frame "
              "(1-based) to --readback_path, then exit. 0 disables it.");
DEFINE_string(readback_path, "",
              "Binary PPM file written by --readback_frame.");
DEFINE_double(display_gamma, 1.0,
              "Display gamma g applied in the tonemap: the framebuffer stores "
              "e^(1/g), with e the sRGB encoding of the tonemapped colour.");

namespace sh_renderer {

// Reads the default framebuffer's back buffer and writes it as a binary PPM,
// top row first.
bool WriteFramebufferPPM(int width, int height, const std::string& path) {
  std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 3);
  ReadDefaultFramebufferRGB(width, height, pixels.data());

  std::ofstream out(path, std::ios::binary);
  if (!out) {
    LOG(ERROR) << "Failed to open readback file: " << path;
    return false;
  }
  out << "P6\n" << width << " " << height << "\n255\n";
  const size_t row_bytes = static_cast<size_t>(width) * 3;
  for (int y = height - 1; y >= 0; --y) {
    out.write(reinterpret_cast<const char*>(pixels.data() + y * row_bytes),
              static_cast<std::streamsize>(row_bytes));
  }
  return static_cast<bool>(out);
}

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
  PartitionLooseGeometries(*scene);
  OptimizeScene(*scene);
  ComputeSceneBoundingBoxes(*scene);
  LogScene(*scene);
  UploadSceneToGPU(*scene);

  std::string error;
  std::unique_ptr<Renderer> renderer = CreateRenderer(&error);
  if (!renderer) {
    LOG(ERROR) << error;
    return;
  }
  SetDisplayGamma(*renderer, static_cast<float>(FLAGS_display_gamma));

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
    if (!BeginFrame(*renderer, fb_width, fb_height)) {
      // Minimised: nothing to draw until the window comes back.
      glfwWaitEvents();
      continue;
    }

    RenderScene(*renderer, &*scene, camera, glfwGetTime());
    EndFrame(*renderer);

    if (FLAGS_readback_frame != 0 && frame_count + 1 == FLAGS_readback_frame) {
      if (WriteFramebufferPPM(fb_width, fb_height, FLAGS_readback_path)) {
        LOG(INFO) << "Wrote frame " << FLAGS_readback_frame << " to "
                  << FLAGS_readback_path;
      }
      should_close = true;
    }

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

  DestroyRenderer(std::move(renderer));
  DestroyWindow(*window);
}

}  // namespace sh_renderer

int main(int argc, char** argv) {
  gflags::SetUsageMessage("Simulating mixed lighting renderer.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_input.empty()) {
    LOG(ERROR)
        << "No input file specified. Use --input to specify a glTF file.";
    return 1;
  }

  sh_renderer::Run(FLAGS_input);

  gflags::ShutDownCommandLineFlags();
  return 0;
}
