#include "shader.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace sh_renderer {
namespace {

constexpr auto npos = std::string::npos;

std::filesystem::path MakeDir(const std::string& name) {
  auto dir = std::filesystem::temp_directory_path() / ("sh_shader_inc_" + name);
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

void Write(const std::filesystem::path& p, const std::string& s) {
  std::ofstream f(p);
  f << s;
}

TEST(ShaderInclude, InlinesIncludedFile) {
  auto dir = MakeDir("basic");
  Write(dir / "snippet.glsl", "float helper() { return 1.0; }\n");
  std::string src =
      "#version 460 core\n#include \"snippet.glsl\"\nvoid main() {}\n";

  std::string out = ResolveShaderIncludes(src, dir);
  EXPECT_NE(out.find("float helper()"), npos);
  EXPECT_EQ(out.find("#include"), npos);        // directive consumed
  EXPECT_NE(out.find("#version 460 core"), npos);  // version preserved at top
  EXPECT_NE(out.find("void main()"), npos);
  std::filesystem::remove_all(dir);
}

TEST(ShaderInclude, ResolvesNestedIncludes) {
  auto dir = MakeDir("nested");
  Write(dir / "c.glsl", "C_CONTENT\n");
  Write(dir / "b.glsl", "B_BEFORE\n#include \"c.glsl\"\nB_AFTER\n");
  std::string src = "#include \"b.glsl\"\n";

  std::string out = ResolveShaderIncludes(src, dir);
  EXPECT_NE(out.find("B_BEFORE"), npos);
  EXPECT_NE(out.find("C_CONTENT"), npos);
  EXPECT_NE(out.find("B_AFTER"), npos);
  EXPECT_EQ(out.find("#include"), npos);
  std::filesystem::remove_all(dir);
}

TEST(ShaderInclude, HandlesIndentationAndPassesOtherLines) {
  auto dir = MakeDir("indent");
  Write(dir / "x.glsl", "X_CONTENT\n");
  std::string src = "line_a\n   #include \"x.glsl\"\nline_b\n";

  std::string out = ResolveShaderIncludes(src, dir);
  EXPECT_NE(out.find("X_CONTENT"), npos);
  EXPECT_NE(out.find("line_a"), npos);
  EXPECT_NE(out.find("line_b"), npos);
  EXPECT_EQ(out.find("#include"), npos);
  std::filesystem::remove_all(dir);
}

TEST(ShaderInclude, NoIncludeLeavesContentIntact) {
  std::string src = "vec3 a = vec3(1.0);\nvec3 b = a;\n";
  std::string out = ResolveShaderIncludes(src, ".");
  EXPECT_NE(out.find("vec3 a = vec3(1.0);"), npos);
  EXPECT_NE(out.find("vec3 b = a;"), npos);
  EXPECT_EQ(out.find("#include"), npos);
}

}  // namespace
}  // namespace sh_renderer
