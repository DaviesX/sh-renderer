#pragma once

// Every file the renderer core reads (GLSL sources and their #includes) goes
// through a FileReader the embedding program supplies, so the core never
// assumes a file system layout. Paths are forward-slash strings relative to
// the reader's root, e.g. "glsl/radiance.frag".

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sh_renderer {

// Returns the contents of `path`, or std::nullopt when the reader has no such
// file. A reader logs nothing for a missing file; the caller decides.
using FileReader =
    std::function<std::optional<std::string>(std::string_view path)>;

// Reads files from disk under `root`: `path` is appended to `root` (an
// absolute `path` is used as is). An empty `root` resolves paths against the
// working directory at the time of each read.
FileReader MakeDiskReader(std::filesystem::path root);

// Asks `primary` first and `fallback` when `primary` has no such file.
FileReader ChainReaders(FileReader primary, FileReader fallback);

// Wraps `reader` so that every path it serves (returns contents for) is
// appended to `*served`, in order. Lets a caller tell which reader of a chain
// supplied a program's files.
FileReader RecordServedPaths(FileReader reader,
                             std::shared_ptr<std::vector<std::string>> served);

// The process-wide reader used when a caller passes none. Initially a disk
// reader at the working directory (MakeDiskReader("")).
const FileReader& DefaultFileReader();

// Replaces the process-wide reader and returns the previous one. Not
// thread-safe: call it at setup, before any reads.
FileReader SetDefaultFileReader(FileReader reader);

// Joins `dir` and the relative `name` with '/', then removes "." segments and
// folds "segment/.." pairs lexically. An absolute `name` (leading '/') is
// returned normalised and unjoined. Leading ".." segments are kept.
std::string JoinReaderPath(std::string_view dir, std::string_view name);

// The directory part of a reader path: everything before the last '/', or ""
// when there is none.
std::string ReaderPathDirectory(std::string_view path);

}  // namespace sh_renderer
