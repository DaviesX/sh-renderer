#include <gflags/gflags.h>
#include <glog/logging.h>

#include <filesystem>
#include <string>

DEFINE_string(input, "", "Path to the glTF scene file to render.");
DEFINE_uint32(width, 1280, "Width of the window.");
DEFINE_uint32(height, 720, "Height of the window.");

void Run(const std::filesystem::path& scene_path) {
  // TODO: Load scene, initialize window and start the main loop.
}

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

  Run(FLAGS_input);

  gflags::ShutDownCommandLineFlags();
  return 0;
}
