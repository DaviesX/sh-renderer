#include "file_reader.h"

#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace sh_renderer {
namespace {

FileReader& DefaultReaderStorage() {
  static FileReader reader = MakeDiskReader("");
  return reader;
}

}  // namespace

FileReader MakeDiskReader(std::filesystem::path root) {
  return [root = std::move(root)](
             std::string_view path) -> std::optional<std::string> {
    const std::filesystem::path full =
        root.empty() ? std::filesystem::path(path) : root / path;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(full, ec)) return std::nullopt;
    std::ifstream f(full, std::ios::binary);
    if (!f.is_open()) return std::nullopt;
    std::ostringstream buffer;
    buffer << f.rdbuf();
    return std::move(buffer).str();
  };
}

FileReader ChainReaders(FileReader primary, FileReader fallback) {
  return [primary = std::move(primary), fallback = std::move(fallback)](
             std::string_view path) -> std::optional<std::string> {
    if (primary) {
      if (std::optional<std::string> contents = primary(path)) return contents;
    }
    if (fallback) return fallback(path);
    return std::nullopt;
  };
}

FileReader RecordServedPaths(FileReader reader,
                             std::shared_ptr<std::vector<std::string>> served) {
  return [reader = std::move(reader), served = std::move(served)](
             std::string_view path) -> std::optional<std::string> {
    if (!reader) return std::nullopt;
    std::optional<std::string> contents = reader(path);
    if (contents && served) served->emplace_back(path);
    return contents;
  };
}

const FileReader& DefaultFileReader() { return DefaultReaderStorage(); }

FileReader SetDefaultFileReader(FileReader reader) {
  return std::exchange(DefaultReaderStorage(), std::move(reader));
}

std::string JoinReaderPath(std::string_view dir, std::string_view name) {
  const bool absolute = !name.empty() && name.front() == '/';
  std::string joined;
  if (absolute || dir.empty()) {
    joined = std::string(name);
  } else {
    joined = std::string(dir) + "/" + std::string(name);
  }
  const bool rooted = !joined.empty() && joined.front() == '/';

  // Split on '/', dropping empty and "." segments and folding "x/..".
  std::vector<std::string> segments;
  size_t start = 0;
  while (start <= joined.size()) {
    size_t end = joined.find('/', start);
    if (end == std::string::npos) end = joined.size();
    std::string segment = joined.substr(start, end - start);
    if (segment == "..") {
      if (!segments.empty() && segments.back() != "..") {
        segments.pop_back();
      } else if (!rooted) {
        segments.push_back(segment);
      }
    } else if (!segment.empty() && segment != ".") {
      segments.push_back(std::move(segment));
    }
    start = end + 1;
  }

  std::string out = rooted ? "/" : "";
  for (size_t i = 0; i < segments.size(); ++i) {
    if (i > 0) out += '/';
    out += segments[i];
  }
  return out;
}

std::string ReaderPathDirectory(std::string_view path) {
  const size_t slash = path.rfind('/');
  if (slash == std::string_view::npos) return "";
  if (slash == 0) return "/";
  return std::string(path.substr(0, slash));
}

}  // namespace sh_renderer
