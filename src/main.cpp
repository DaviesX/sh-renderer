#include <gflags/gflags.h>
#include <glog/logging.h>

#include <filesystem>
#include <string>

DEFINE_string(input, "", "Path to the glTF scene file to render.");

namespace sh_renderer {

void RunRenderer(const std::filesystem::path& scene_path) {
  LOG(INFO) << "Starting renderer with scene: " << scene_path;
  // TODO(renderer): Initialize window and load scene.
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

  sh_renderer::RunRenderer(FLAGS_input);

  gflags::ShutDownCommandLineFlags();
  return 0;
}
