#include "embedded_glsl.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <set>
#include <string>

#include "file_reader.h"
#include "shader.h"

namespace sh_renderer {
namespace {

// The tests run from the source root, where glsl/ is.
const std::filesystem::path kSourceRoot = std::filesystem::current_path();

bool IsEmbeddedExtension(const std::filesystem::path& p) {
  const std::string ext = p.extension().string();
  return ext == ".vert" || ext == ".frag" || ext == ".comp" || ext == ".glsl";
}

std::set<std::string> GlslFilesOnDisk() {
  std::set<std::string> paths;
  for (const auto& entry :
       std::filesystem::directory_iterator(kSourceRoot / "glsl")) {
    if (entry.is_regular_file() && IsEmbeddedExtension(entry.path())) {
      paths.insert("glsl/" + entry.path().filename().string());
    }
  }
  return paths;
}

// Restores the working directory when it goes out of scope.
class ScopedWorkingDirectory {
 public:
  explicit ScopedWorkingDirectory(const std::filesystem::path& dir)
      : previous_(std::filesystem::current_path()) {
    std::filesystem::current_path(dir);
  }
  ~ScopedWorkingDirectory() { std::filesystem::current_path(previous_); }

 private:
  std::filesystem::path previous_;
};

TEST(EmbeddedGlsl, TableHoldsEveryGlslFileByteForByte) {
  const std::set<std::string> on_disk = GlslFilesOnDisk();
  ASSERT_FALSE(on_disk.empty());

  std::set<std::string> embedded;
  std::string_view previous;
  FileReader disk = MakeDiskReader(kSourceRoot);
  for (const EmbeddedFile& file : EmbeddedGlslFiles()) {
    EXPECT_LT(previous, file.path) << "table not sorted";
    previous = file.path;
    embedded.insert(std::string(file.path));
    std::optional<std::string> contents = disk(file.path);
    ASSERT_TRUE(contents.has_value()) << file.path;
    EXPECT_EQ(*contents, file.contents) << file.path;
  }
  EXPECT_EQ(embedded, on_disk);
  EXPECT_FALSE(MakeEmbeddedReader()("glsl/q3_composite.glsl.import"));
  EXPECT_FALSE(MakeEmbeddedReader()("glsl/none.frag"));
}

TEST(EmbeddedGlsl, AssembledSourcesMatchDiskWithoutGlslDirectory) {
  FileReader disk = MakeDiskReader(kSourceRoot);
  FileReader embedded = MakeEmbeddedReader();
  const std::set<std::string> on_disk = GlslFilesOnDisk();

  const auto temp = std::filesystem::temp_directory_path() / "sh_embedded_cwd";
  std::filesystem::remove_all(temp);
  std::filesystem::create_directories(temp);
  {
    ScopedWorkingDirectory cwd(temp);
    ASSERT_FALSE(std::filesystem::exists("glsl"));
    int programs = 0;
    for (const std::string& path : on_disk) {
      if (path.ends_with(".glsl")) continue;  // include-only files
      std::optional<std::string> from_disk = LoadShaderSource(path, disk);
      std::optional<std::string> from_table = LoadShaderSource(path, embedded);
      ASSERT_TRUE(from_disk.has_value()) << path;
      ASSERT_TRUE(from_table.has_value()) << path;
      EXPECT_EQ(*from_table, *from_disk) << path;
      ++programs;
    }
    EXPECT_GT(programs, 0);
  }
  std::filesystem::remove_all(temp);
}

}  // namespace
}  // namespace sh_renderer
