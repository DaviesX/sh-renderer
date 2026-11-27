#include "file_reader.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "log.h"
#include "shader.h"

namespace sh_renderer {
namespace {

constexpr auto npos = std::string::npos;

std::filesystem::path MakeDir(const std::string& name) {
  auto dir =
      std::filesystem::temp_directory_path() / ("sh_file_reader_" + name);
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

void Write(const std::filesystem::path& p, const std::string& s) {
  std::filesystem::create_directories(p.parent_path());
  std::ofstream f(p, std::ios::binary);
  f << s;
}

// An in-memory reader that also records every path it was asked for.
struct MemoryFiles {
  std::map<std::string, std::string, std::less<>> files;
  std::shared_ptr<std::vector<std::string>> requests =
      std::make_shared<std::vector<std::string>>();

  FileReader Reader() const {
    return [files = files, requests = requests](
               std::string_view path) -> std::optional<std::string> {
      requests->emplace_back(path);
      auto it = files.find(path);
      if (it == files.end()) return std::nullopt;
      return it->second;
    };
  }
};

// Captures ERROR messages so a test can check what was reported.
class CapturedErrors {
 public:
  CapturedErrors() {
    LogSink sink;
    sink.message = [this](LogSeverity severity, const char*, int,
                          const std::string& message) {
      if (severity == LogSeverity::kError) errors_.push_back(message);
    };
    previous_ = SetLogSink(sink);
  }
  ~CapturedErrors() { SetLogSink(previous_); }

  const std::vector<std::string>& errors() const { return errors_; }

 private:
  LogSink previous_;
  std::vector<std::string> errors_;
};

TEST(FileReader, DiskReaderReadsUnderRoot) {
  auto dir = MakeDir("disk");
  Write(dir / "glsl" / "a.frag", "A\r\nbytes\n");
  FileReader reader = MakeDiskReader(dir);
  std::optional<std::string> contents = reader("glsl/a.frag");
  ASSERT_TRUE(contents.has_value());
  EXPECT_EQ(*contents, "A\r\nbytes\n");  // byte-for-byte
  std::filesystem::remove_all(dir);
}

TEST(FileReader, DiskReaderReturnsEmptyOnMissingFile) {
  auto dir = MakeDir("missing");
  FileReader reader = MakeDiskReader(dir);
  EXPECT_FALSE(reader("glsl/none.frag").has_value());
  std::filesystem::create_directories(dir / "glsl");
  EXPECT_FALSE(reader("glsl").has_value());  // a directory is not a file
  std::filesystem::remove_all(dir);
}

TEST(FileReader, ChainFallsBackOnlyWhenPrimaryMisses) {
  MemoryFiles primary;
  primary.files["a"] = "primary a";
  MemoryFiles fallback;
  fallback.files["a"] = "fallback a";
  fallback.files["b"] = "fallback b";
  FileReader chain = ChainReaders(primary.Reader(), fallback.Reader());

  EXPECT_EQ(chain("a"), "primary a");
  EXPECT_EQ(chain("b"), "fallback b");
  EXPECT_FALSE(chain("c").has_value());
  // The fallback is not asked for a file the primary served.
  EXPECT_EQ(*fallback.requests, (std::vector<std::string>{"b", "c"}));
}

TEST(FileReader, RecordServedPathsListsOnlyServedFiles) {
  MemoryFiles vfs;
  vfs.files["glsl/override.frag"] = "override";
  MemoryFiles builtin;
  builtin.files["glsl/override.frag"] = "builtin";
  builtin.files["glsl/other.frag"] = "builtin other";
  auto served = std::make_shared<std::vector<std::string>>();
  FileReader chain =
      ChainReaders(RecordServedPaths(vfs.Reader(), served), builtin.Reader());

  EXPECT_EQ(chain("glsl/override.frag"), "override");
  EXPECT_EQ(chain("glsl/other.frag"), "builtin other");
  EXPECT_EQ(*served, (std::vector<std::string>{"glsl/override.frag"}));
}

TEST(FileReader, DefaultReaderCanBeReplacedAndRestored) {
  MemoryFiles memory;
  memory.files["only/in/memory"] = "x";
  FileReader previous = SetDefaultFileReader(memory.Reader());
  EXPECT_EQ(DefaultFileReader()("only/in/memory"), "x");
  SetDefaultFileReader(previous);
  EXPECT_FALSE(DefaultFileReader()("only/in/memory").has_value());
}

TEST(FileReader, JoinReaderPathNormalises) {
  EXPECT_EQ(JoinReaderPath("", "a.glsl"), "a.glsl");
  EXPECT_EQ(JoinReaderPath("glsl", "a.glsl"), "glsl/a.glsl");
  EXPECT_EQ(JoinReaderPath("glsl/sub", "../a.glsl"), "glsl/a.glsl");
  EXPECT_EQ(JoinReaderPath("glsl", "./inc/b.glsl"), "glsl/inc/b.glsl");
  EXPECT_EQ(JoinReaderPath("", "../a.glsl"), "../a.glsl");
  EXPECT_EQ(JoinReaderPath("glsl", "/abs/a.glsl"), "/abs/a.glsl");
  EXPECT_EQ(JoinReaderPath("/tmp/x", "y.glsl"), "/tmp/x/y.glsl");
  EXPECT_EQ(ReaderPathDirectory("glsl/inc/b.glsl"), "glsl/inc");
  EXPECT_EQ(ReaderPathDirectory("b.glsl"), "");
}

TEST(FileReader, InMemoryReaderResolvesNestedAndRelativeIncludes) {
  MemoryFiles memory;
  memory.files["glsl/main.frag"] =
      "#version 460 core\n#include \"inc/b.glsl\"\nvoid main() {}\n";
  memory.files["glsl/inc/b.glsl"] =
      "B_BEFORE\n#include \"../common/c.glsl\"\n  #include "
      "\"d.glsl\"\nB_AFTER\n";
  memory.files["glsl/common/c.glsl"] = "C_CONTENT\n";
  memory.files["glsl/inc/d.glsl"] = "D_CONTENT\n";
  FileReader previous = SetDefaultFileReader([](std::string_view) {
    ADD_FAILURE() << "the default reader must not be used";
    return std::optional<std::string>();
  });

  std::optional<std::string> out =
      LoadShaderSource("glsl/main.frag", memory.Reader());
  SetDefaultFileReader(previous);

  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(*out,
            "#version 460 core\n"
            "B_BEFORE\nC_CONTENT\n\nD_CONTENT\n\nB_AFTER\n\n"
            "void main() {}\n");
  EXPECT_EQ(*memory.requests, (std::vector<std::string>{
                                  "glsl/main.frag", "glsl/inc/b.glsl",
                                  "glsl/common/c.glsl", "glsl/inc/d.glsl"}));
}

TEST(FileReader, ReaderOverloadResolvesRelativeToSourceDir) {
  MemoryFiles memory;
  memory.files["glsl/x.glsl"] = "X_CONTENT\n";
  std::string out = ResolveShaderIncludes("a\n#include \"x.glsl\"\nb\n", "glsl",
                                          memory.Reader());
  EXPECT_EQ(out, "a\nX_CONTENT\n\nb\n");
}

TEST(FileReader, MissingIncludeIsReportedAndDropped) {
  CapturedErrors errors;
  MemoryFiles memory;
  std::string out = ResolveShaderIncludes("a\n#include \"gone.glsl\"\nb\n",
                                          "glsl", memory.Reader());
  EXPECT_EQ(out, "a\n\nb\n");
  ASSERT_EQ(errors.errors().size(), 1u);
  EXPECT_NE(errors.errors()[0].find("glsl/gone.glsl"), npos);
}

TEST(FileReader, MissingSourceIsEmpty) {
  CapturedErrors errors;
  MemoryFiles memory;
  EXPECT_FALSE(LoadShaderSource("glsl/none.frag", memory.Reader()).has_value());
  ASSERT_EQ(errors.errors().size(), 1u);
  EXPECT_NE(errors.errors()[0].find("glsl/none.frag"), npos);
}

TEST(FileReader, IncludeDepthIsLimited) {
  CapturedErrors errors;
  MemoryFiles memory;
  memory.files["loop.glsl"] = "L\n#include \"loop.glsl\"\n";
  std::string out =
      ResolveShaderIncludes("#include \"loop.glsl\"\n", "", memory.Reader());

  // Depths 1..16 inline the file; depth 17 stops and keeps the directive.
  size_t inlined = 0;
  for (size_t pos = out.find("L\n"); pos != npos;
       pos = out.find("L\n", pos + 1))
    ++inlined;
  EXPECT_EQ(inlined, 17u);
  EXPECT_NE(out.find("#include \"loop.glsl\""), npos);
  EXPECT_EQ(*memory.requests, std::vector<std::string>(17, "loop.glsl"));
  ASSERT_EQ(errors.errors().size(), 1u);
  EXPECT_NE(errors.errors()[0].find("too deep"), npos);
}

}  // namespace
}  // namespace sh_renderer
