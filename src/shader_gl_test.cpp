// Shader program creation against a real GL 4.6 context (hidden GLFW window).

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "file_reader.h"
#include "glad.h"
#include "log.h"
#include "shader.h"

#ifdef SH_RENDERER_EMBED_GLSL
#include "embedded_glsl.h"
#endif

namespace sh_renderer {
namespace {

constexpr auto npos = std::string::npos;

FileReader MemoryReader(std::map<std::string, std::string, std::less<>> files) {
  return [files = std::move(files)](
             std::string_view path) -> std::optional<std::string> {
    auto it = files.find(path);
    if (it == files.end()) return std::nullopt;
    return it->second;
  };
}

// A hidden window with a current GL 4.6 core context for every test.
class ShaderGlTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    if (!glfwInit()) return;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window_ = glfwCreateWindow(64, 64, "shader_gl_test", nullptr, nullptr);
    glfwDefaultWindowHints();
    if (window_ == nullptr) return;
    glfwMakeContextCurrent(window_);
    if (!gladLoaderLoadGL()) {
      glfwDestroyWindow(window_);
      window_ = nullptr;
    }
  }

  static void TearDownTestSuite() {
    if (window_ != nullptr) glfwDestroyWindow(window_);
    window_ = nullptr;
    glfwTerminate();
  }

  void SetUp() override {
    ASSERT_NE(window_, nullptr) << "no hidden GL 4.6 context";
    glfwMakeContextCurrent(window_);
    LogSink sink;
    sink.message = [this](LogSeverity severity, const char*, int,
                          const std::string& message) {
      if (severity == LogSeverity::kError) errors_.push_back(message);
    };
    previous_sink_ = SetLogSink(sink);
  }

  void TearDown() override { SetLogSink(previous_sink_); }

  static GLFWwindow* window_;
  std::vector<std::string> errors_;
  LogSink previous_sink_;
};

GLFWwindow* ShaderGlTest::window_ = nullptr;

constexpr std::string_view kVertex =
    "#version 460 core\n"
    "#include \"common.glsl\"\n"
    "void main() { gl_Position = vec4(kValue); }\n";
constexpr std::string_view kFragment =
    "#version 460 core\n"
    "#include \"common.glsl\"\n"
    "out vec4 color;\n"
    "void main() { color = vec4(kValue); }\n";
constexpr std::string_view kBrokenFragment =
    "#version 460 core\n"
    "#include \"common.glsl\"\n"
    "out vec4 color;\n"
    "void main() { color = vec4(kValue) + undeclared_symbol; }\n";

TEST_F(ShaderGlTest, ProgramBuildsThroughInMemoryReader) {
  FileReader reader =
      MemoryReader({{"mem/a.vert", std::string(kVertex)},
                    {"mem/a.frag", std::string(kFragment)},
                    {"mem/common.glsl", "const float kValue = 0.5;\n"}});
  std::optional<ShaderProgram> program =
      ShaderProgram::CreateGraphics("mem/a.vert", "mem/a.frag", {}, &reader);
  ASSERT_TRUE(program.has_value());
  EXPECT_TRUE(*program);
  EXPECT_TRUE(errors_.empty());
}

TEST_F(ShaderGlTest, BrokenProgramReturnsEmptyWithInfoLog) {
  FileReader reader =
      MemoryReader({{"mem/a.vert", std::string(kVertex)},
                    {"mem/broken.frag", std::string(kBrokenFragment)},
                    {"mem/common.glsl", "const float kValue = 0.5;\n"}});

  // Logged as an ERROR when the caller asks for no error string.
  std::optional<ShaderProgram> program = ShaderProgram::CreateGraphics(
      "mem/a.vert", "mem/broken.frag", {}, &reader);
  EXPECT_FALSE(program.has_value());
  ASSERT_EQ(errors_.size(), 1u);
  EXPECT_NE(errors_[0].find("mem/broken.frag"), npos) << errors_[0];
  EXPECT_NE(errors_[0].find("undeclared_symbol"), npos) << errors_[0];

  // Returned instead of logged when asked for.
  std::string error;
  program = ShaderProgram::CreateGraphics("mem/a.vert", "mem/broken.frag", {},
                                          &reader, &error);
  EXPECT_FALSE(program.has_value());
  EXPECT_EQ(errors_.size(), 1u);
  EXPECT_NE(error.find("mem/broken.frag"), npos) << error;
  EXPECT_NE(error.find("undeclared_symbol"), npos) << error;
}

TEST_F(ShaderGlTest, MissingFileNamesThePath) {
  FileReader reader = MemoryReader({{"mem/a.vert", std::string(kVertex)}});
  std::string error;
  std::optional<ShaderProgram> program = ShaderProgram::CreateGraphics(
      "mem/a.vert", "mem/missing.frag", {}, &reader, &error);
  EXPECT_FALSE(program.has_value());
  EXPECT_NE(error.find("mem/missing.frag"), npos) << error;
}

TEST_F(ShaderGlTest, BrokenComputeProgramReturnsEmptyWithInfoLog) {
  FileReader reader =
      MemoryReader({{"mem/broken.comp",
                     "#version 460 core\nlayout(local_size_x = 1) in;\n"
                     "void main() { undeclared_compute_symbol = 1; }\n"}});
  std::string error;
  std::optional<ShaderProgram> program =
      ShaderProgram::CreateCompute("mem/broken.comp", {}, &reader, &error);
  EXPECT_FALSE(program.has_value());
  EXPECT_NE(error.find("mem/broken.comp"), npos) << error;
  EXPECT_NE(error.find("undeclared_compute_symbol"), npos) << error;
}

TEST_F(ShaderGlTest, DefaultReaderIsUsedWhenNoneIsGiven) {
  FileReader previous = SetDefaultFileReader(
      MemoryReader({{"mem/a.vert", std::string(kVertex)},
                    {"mem/a.frag", std::string(kFragment)},
                    {"mem/common.glsl", "const float kValue = 1.0;\n"}}));
  std::optional<ShaderProgram> program =
      ShaderProgram::CreateGraphics("mem/a.vert", "mem/a.frag");
  SetDefaultFileReader(previous);
  EXPECT_TRUE(program.has_value());
}

#ifdef SH_RENDERER_EMBED_GLSL
TEST_F(ShaderGlTest, BuiltInProgramsCompileWithoutGlslDirectory) {
  const auto previous_cwd = std::filesystem::current_path();
  const auto temp = std::filesystem::temp_directory_path() / "sh_shader_gl_cwd";
  std::filesystem::remove_all(temp);
  std::filesystem::create_directories(temp);
  std::filesystem::current_path(temp);

  FileReader reader = MakeEmbeddedReader();
  std::optional<ShaderProgram> tonemap = ShaderProgram::CreateGraphics(
      "glsl/tonemap.vert", "glsl/tonemap.frag", {}, &reader);
  std::optional<ShaderProgram> light_cull =
      ShaderProgram::CreateCompute("glsl/light_cull.comp", {}, &reader);

  std::filesystem::current_path(previous_cwd);
  std::filesystem::remove_all(temp);
  EXPECT_TRUE(tonemap.has_value());
  EXPECT_TRUE(light_cull.has_value());
  EXPECT_TRUE(errors_.empty());
}
#endif

}  // namespace
}  // namespace sh_renderer
