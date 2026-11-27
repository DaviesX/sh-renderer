// The frame API through the engine facade, against a real GL 4.6 context
// (hidden GLFW window with an sRGB-capable default framebuffer).

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine_api.h"
#include "file_reader.h"

namespace {

constexpr int kSize = 64;

// The tonemap's Uncharted2 curve, in float as the shader computes it.
float Uncharted2(float x) {
  constexpr float A = 0.15f, B = 0.50f, C = 0.10f, D = 0.20f, E = 0.02f,
                  F = 0.30f;
  return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

// The exact sRGB encoding of the tonemapped linear value `c`, unrounded.
double TonemappedSrgb(float c) {
  const double t = std::clamp(
      static_cast<double>(Uncharted2(c) / Uncharted2(11.2f)), 0.0, 1.0);
  return t <= 0.0031308 ? 12.92 * t : 1.055 * std::pow(t, 1.0 / 2.4) - 0.055;
}

// The expected framebuffer byte for clear value `c` at display gamma `g`:
// round(255 * e^(1/g)), with e the unrounded sRGB encoding.
int ExpectedByte(float c, double g) {
  return static_cast<int>(
      std::lround(255.0 * std::pow(TonemappedSrgb(c), 1.0 / g)));
}

void* GetProcAddress(const char* name) {
  return reinterpret_cast<void*>(glfwGetProcAddress(name));
}

class FrameTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    if (!glfwInit()) return;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    window_ = glfwCreateWindow(kSize, kSize, "frame_test", nullptr, nullptr);
    glfwDefaultWindowHints();
  }

  static void TearDownTestSuite() {
    if (window_ != nullptr) glfwDestroyWindow(window_);
    window_ = nullptr;
    glfwTerminate();
  }

  void SetUp() override {
    ASSERT_NE(window_, nullptr) << "no hidden GL 4.6 context";
    glfwMakeContextCurrent(window_);
  }

  static ShHostServices Services() {
    ShHostServices services;
    services.get_proc_address = &GetProcAddress;
    return services;
  }

  // Renders one cleared frame and returns its pixels (bottom row first).
  static std::vector<uint8_t> RenderCleared(ShRenderer* renderer, float r,
                                            float g, float b, float gamma) {
    const float rgb[3] = {r, g, b};
    ShSetClearColor(renderer, rgb);
    ShSetDisplayGamma(renderer, gamma);
    std::vector<uint8_t> pixels(kSize * kSize * 3, 0);
    EXPECT_TRUE(ShBeginFrame(renderer, kSize, kSize));
    ShEndFrame(renderer, 0.0);
    ShReadPixels(kSize, kSize, pixels.data());
    return pixels;
  }

  // Counts the bytes of channel `channel` more than 1 from `expected`.
  static int CountOff(const std::vector<uint8_t>& pixels, int channel,
                      int expected, int* worst) {
    int off = 0;
    for (size_t i = channel; i < pixels.size(); i += 3) {
      const int d = std::abs(static_cast<int>(pixels[i]) - expected);
      *worst = std::max(*worst, d);
      if (d > 1) ++off;
    }
    return off;
  }

  static GLFWwindow* window_;
};

GLFWwindow* FrameTest::window_ = nullptr;

TEST_F(FrameTest, ClearedFrameIsTonemappedSrgb) {
  ShRenderer* renderer = ShCreate(Services());
  ASSERT_NE(renderer, nullptr) << ShLastError();
  EXPECT_TRUE(ShLastError().empty());

  const float clear[3] = {0.25f, 1.0f, 4.0f};
  std::vector<uint8_t> pixels =
      RenderCleared(renderer, clear[0], clear[1], clear[2], 1.0f);
  for (int channel = 0; channel < 3; ++channel) {
    const int expected = ExpectedByte(clear[channel], 1.0);
    int worst = 0;
    EXPECT_EQ(CountOff(pixels, channel, expected, &worst), 0)
        << "channel " << channel << " expected " << expected << " worst off "
        << worst;
  }
  ShDestroy(renderer);
}

TEST_F(FrameTest, ZeroSizeSkipsTheFrame) {
  ShRenderer* renderer = ShCreate(Services());
  ASSERT_NE(renderer, nullptr) << ShLastError();
  EXPECT_FALSE(ShBeginFrame(renderer, 0, 0));
  EXPECT_FALSE(ShBeginFrame(renderer, kSize, 0));
  EXPECT_FALSE(ShBeginFrame(renderer, 0, kSize));
  ShEndFrame(renderer, 0.0);  // Does nothing after a skipped BeginFrame.

  // A later frame with a real size renders.
  std::vector<uint8_t> pixels = RenderCleared(renderer, 1.0f, 1.0f, 1.0f, 1.0f);
  int worst = 0;
  EXPECT_EQ(CountOff(pixels, 0, ExpectedByte(1.0f, 1.0), &worst), 0);
  ShDestroy(renderer);
}

TEST_F(FrameTest, CreateFailsNamingAMissingShader) {
  ShHostServices services = Services();
  sh_renderer::FileReader disk = sh_renderer::MakeDiskReader("");
  services.reader =
      [disk](std::string_view path) -> std::optional<std::string> {
    if (path == "glsl/tonemap.frag") return std::nullopt;
    return disk(path);
  };
  services.log = [](ShLogSeverity, const char*, int, const std::string&) {};
  ShRenderer* renderer = ShCreate(services);
  EXPECT_EQ(renderer, nullptr);
  EXPECT_NE(ShLastError().find("tonemap.frag"), std::string::npos)
      << ShLastError();
  // The reason names the missing path itself, not only the program's files.
  EXPECT_NE(ShLastError().find("\"glsl/tonemap.frag\""), std::string::npos)
      << ShLastError();
  ShDestroy(renderer);

  // The previous reader is back: a normal create succeeds.
  renderer = ShCreate(Services());
  ASSERT_NE(renderer, nullptr) << ShLastError();
  EXPECT_TRUE(ShLastError().empty());
  ShDestroy(renderer);
}

TEST_F(FrameTest, CreateFailsWithTheInfoLogOfABrokenShader) {
  ShHostServices services = Services();
  sh_renderer::FileReader disk = sh_renderer::MakeDiskReader("");
  services.reader =
      [disk](std::string_view path) -> std::optional<std::string> {
    if (path == "glsl/tonemap.frag") {
      return std::string(
          "#version 460 core\nout vec4 color;\nvoid main() { color = ; }\n");
    }
    return disk(path);
  };
  std::string logged;
  services.log = [&logged](ShLogSeverity severity, const char*, int,
                           const std::string& message) {
    if (severity == ShLogSeverity::kError) logged += message + "\n";
  };
  ShRenderer* renderer = ShCreate(services);
  EXPECT_EQ(renderer, nullptr);
  const std::string& error = ShLastError();
  EXPECT_NE(error.find("tonemap"), std::string::npos) << error;
  EXPECT_NE(error.find("glsl/tonemap.frag"), std::string::npos) << error;
  // The driver's info log reports the syntax error (NVIDIA and Mesa both say
  // "syntax error"), and the same text was logged.
  EXPECT_NE(error.find("syntax error"), std::string::npos) << error;
  EXPECT_NE(logged.find("syntax error"), std::string::npos) << logged;
  // Only the broken program failed.
  EXPECT_EQ(error.find("radiance"), std::string::npos) << error;
  ShDestroy(renderer);
}

TEST_F(FrameTest, EmbeddedReaderMatchesTheBuild) {
  ShFileReader embedded = ShEmbeddedReader();
  ASSERT_TRUE(embedded);
  EXPECT_EQ(embedded("not/a/glsl/file.txt"), std::nullopt);
#ifdef SH_RENDERER_EMBED_GLSL
  EXPECT_TRUE(ShHasEmbeddedGlsl());
  sh_renderer::FileReader disk = sh_renderer::MakeDiskReader("");
  const std::optional<std::string> built_in = embedded("glsl/tonemap.frag");
  ASSERT_TRUE(built_in.has_value());
  EXPECT_EQ(built_in, disk("glsl/tonemap.frag"));

  // The renderer builds from the embedded GLSL alone.
  ShHostServices services = Services();
  services.reader = embedded;
  ShRenderer* renderer = ShCreate(services);
  ASSERT_NE(renderer, nullptr) << ShLastError();
  ShDestroy(renderer);
#else
  EXPECT_FALSE(ShHasEmbeddedGlsl());
  EXPECT_EQ(embedded("glsl/tonemap.frag"), std::nullopt);
#endif
}

TEST_F(FrameTest, InvalidDisplayGammaIsOneAndLargeGammaIsClamped) {
  ShHostServices services = Services();
  services.log = [](ShLogSeverity, const char*, int, const std::string&) {};
  ShRenderer* renderer = ShCreate(services);
  ASSERT_NE(renderer, nullptr) << ShLastError();
  const float c = 0.05f;
  const struct {
    float gamma;
    double effective;
  } cases[] = {
      {0.0f, 1.0},          {-1.0f, 1.0},
      {std::nanf(""), 1.0}, {std::numeric_limits<float>::infinity(), 1.0},
      {10.0f, 3.0},         {0.1f, 0.5},
  };
  for (const auto& test : cases) {
    std::vector<uint8_t> pixels = RenderCleared(renderer, c, c, c, test.gamma);
    const int expected = ExpectedByte(c, test.effective);
    for (int channel = 0; channel < 3; ++channel) {
      int worst = 0;
      EXPECT_EQ(CountOff(pixels, channel, expected, &worst), 0)
          << "gamma " << test.gamma << " channel " << channel << " expected "
          << expected << " got " << static_cast<int>(pixels[channel]);
    }
  }
  // The clamped results differ from the plain tonemap, so the clamp (not a
  // reset to 1) produced them.
  EXPECT_NE(ExpectedByte(c, 3.0), ExpectedByte(c, 1.0));
  EXPECT_NE(ExpectedByte(c, 0.5), ExpectedByte(c, 1.0));
  ShDestroy(renderer);
}

TEST_F(FrameTest, EndFrameRestoresCheapState) {
  ShRenderer* renderer = ShCreate(Services());
  ASSERT_NE(renderer, nullptr) << ShLastError();
  RenderCleared(renderer, 1.0f, 1.0f, 1.0f, 1.0f);
  // The facade hides glad, so query through the resolver.
  using GetIntegerv = void (*)(unsigned, int*);
  using IsEnabled = unsigned char (*)(unsigned);
  auto get_integerv =
      reinterpret_cast<GetIntegerv>(GetProcAddress("glGetIntegerv"));
  auto is_enabled = reinterpret_cast<IsEnabled>(GetProcAddress("glIsEnabled"));
  ASSERT_NE(get_integerv, nullptr);
  ASSERT_NE(is_enabled, nullptr);
  constexpr unsigned kCurrentProgram = 0x8B8D;
  constexpr unsigned kVertexArrayBinding = 0x85B5;
  constexpr unsigned kActiveTexture = 0x84E0;
  constexpr unsigned kTexture0 = 0x84C0;
  constexpr unsigned kTextureBinding2D = 0x8069;
  constexpr unsigned kDepthTest = 0x0B71;
  constexpr unsigned kFramebufferSrgb = 0x8DB9;
  int value = -1;
  get_integerv(kCurrentProgram, &value);
  EXPECT_EQ(value, 0);
  get_integerv(kVertexArrayBinding, &value);
  EXPECT_EQ(value, 0);
  get_integerv(kActiveTexture, &value);
  EXPECT_EQ(value, static_cast<int>(kTexture0));
  get_integerv(kTextureBinding2D, &value);
  EXPECT_EQ(value, 0);
  EXPECT_EQ(is_enabled(kDepthTest), 0);
  EXPECT_NE(is_enabled(kFramebufferSrgb), 0);
  ShDestroy(renderer);
}

TEST_F(FrameTest, CreateFailsWithoutResolver) {
  ShHostServices services;
  EXPECT_EQ(ShCreate(services), nullptr);
  EXPECT_FALSE(ShLastError().empty());
}

TEST_F(FrameTest, DisplayGammaRaisesTheSrgbEncoding) {
  ShRenderer* renderer = ShCreate(Services());
  ASSERT_NE(renderer, nullptr) << ShLastError();
  const float clears[] = {0.0f, 0.002f, 0.05f, 0.25f, 1.0f, 4.0f, 20.0f};
  for (double gamma : {1.5, 2.2}) {
    for (float c : clears) {
      std::vector<uint8_t> pixels =
          RenderCleared(renderer, c, c, c, static_cast<float>(gamma));
      const int expected = ExpectedByte(c, gamma);
      for (int channel = 0; channel < 3; ++channel) {
        int worst = 0;
        EXPECT_EQ(CountOff(pixels, channel, expected, &worst), 0)
            << "gamma " << gamma << " clear " << c << " channel " << channel
            << " expected " << expected << " got "
            << static_cast<int>(pixels[channel]);
      }
    }
  }
  ShDestroy(renderer);
}

}  // namespace
