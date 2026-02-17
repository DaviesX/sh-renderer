#pragma once

#include <Eigen/Core>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

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

  // Loads and compiles a compute shader from a file.
  // Returns std::nullopt on failure.
  static std::optional<ShaderProgram> CreateCompute(
      const std::filesystem::path& compute_path);

  // Loads and compiles a graphics pipeline (vertex + fragment) from files.
  // Returns std::nullopt on failure.
  static std::optional<ShaderProgram> CreateGraphics(
      const std::filesystem::path& vertex_path,
      const std::filesystem::path& fragment_path);

  // Loads and compiles a graphics pipeline (vertex + fragment) from source
  // strings. Returns std::nullopt on failure.
  static std::optional<ShaderProgram> CreateFromSource(
      std::string_view vertex_source, std::string_view fragment_source);

  // Checks if the program is valid.
  explicit operator bool() const { return id_ != 0; }

  // Sets a uniform value by name.
  void Uniform(std::string_view name, int value) const;
  void Uniform(std::string_view name, float value) const;
  void Uniform(std::string_view name, const Eigen::Vector3f& value) const;
  void Uniform(std::string_view name, const Eigen::Matrix4f& value) const;

  // Use this program for subsequent rendering commands.
  void Use() const;

  GLuint id() const { return id_; }

 private:
  explicit ShaderProgram(GLuint id) : id_(id) {}

  GLuint id_ = 0;
};

}  // namespace sh_renderer
