#pragma once

#include <Eigen/Core>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "file_reader.h"
#include "glad.h"

namespace sh_renderer {

// Wraps an OpenGL shader program.
class ShaderProgram {
 public:
  // Creates an empty shader program.
  ShaderProgram() = default;

  // Move-only.
  ShaderProgram(ShaderProgram&& other) noexcept;
  ShaderProgram& operator=(ShaderProgram&& other) noexcept;
  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;

  ~ShaderProgram();

  // Loads and compiles a compute shader from a file read through `reader`
  // (the process-wide DefaultFileReader() when null); #includes resolve
  // through the same reader. Returns std::nullopt on failure. On failure the
  // error (file names and the driver's info log) goes to `*error` when given,
  // and is logged as an ERROR otherwise.
  static std::optional<ShaderProgram> CreateCompute(
      const std::filesystem::path& compute_path,
      const std::map<std::string, std::string>& macros = {},
      const FileReader* reader = nullptr, std::string* error = nullptr);

  // Loads and compiles a graphics pipeline (vertex + fragment) from files read
  // through `reader`, as CreateCompute does. Returns std::nullopt on failure.
  static std::optional<ShaderProgram> CreateGraphics(
      const std::filesystem::path& vertex_path,
      const std::filesystem::path& fragment_path,
      const std::map<std::string, std::string>& macros = {},
      const FileReader* reader = nullptr, std::string* error = nullptr);

  // Loads and compiles a graphics pipeline (vertex + fragment) from source
  // strings. Returns std::nullopt on failure, reporting as CreateCompute does.
  static std::optional<ShaderProgram> CreateFromSource(
      std::string_view vertex_source, std::string_view fragment_source,
      const std::map<std::string, std::string>& macros = {},
      std::string* error = nullptr);

  // Checks if the program is valid.
  explicit operator bool() const { return id_ != 0; }

  // Sets a uniform value by name.
  void Uniform(std::string_view name, int value) const;
  void Uniform(std::string_view name, float value) const;
  void Uniform(std::string_view name, const Eigen::Vector2i& value) const;
  void Uniform(std::string_view name, const Eigen::Vector2f& value) const;
  void Uniform(std::string_view name, const Eigen::Vector3f& value) const;
  void Uniform(std::string_view name, const Eigen::Matrix4f& value) const;

  // Use this program for subsequent rendering commands.
  void Use() const;

  GLuint id() const { return id_; }

 private:
  explicit ShaderProgram(GLuint id) : id_(id) {}

  GLuint id_ = 0;
};

// Resolves `#include "file"` directives in GLSL source by inlining the file's
// contents (paths are resolved relative to `base_dir`, then to the included
// file's own directory for nested includes). GLSL has no native include, so
// this is plain textual substitution, applied before compilation. Reads
// through a disk reader rooted at `base_dir`. Exposed for testing.
std::string ResolveShaderIncludes(std::string_view source,
                                  const std::filesystem::path& base_dir);

// As above, through `reader`: `source_dir` is the reader path of the
// directory holding `source` ("" for the reader's root), and each include
// resolves relative to the including file's directory.
std::string ResolveShaderIncludes(std::string_view source,
                                  std::string_view source_dir,
                                  const FileReader& reader);

// Reads the shader at reader path `path` through `reader` and resolves its
// #includes. Returns std::nullopt when the file is missing or empty.
std::optional<std::string> LoadShaderSource(std::string_view path,
                                            const FileReader& reader);

}  // namespace sh_renderer
