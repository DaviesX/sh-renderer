#include "shader.h"

#include <glog/logging.h>

#include <fstream>
#include <iostream>
#include <sstream>

namespace sh_renderer {
namespace {

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    LOG(ERROR) << "Failed to open shader file: " << path;
    return "";
  }
  std::stringstream buffer;
  buffer << f.rdbuf();
  return buffer.str();
}

GLuint CompileShader(GLenum type, const std::string& source,
                     const std::string& name) {
  GLuint shader = glCreateShader(type);
  const char* src = source.c_str();
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char info_log[1024];
    glGetShaderInfoLog(shader, 1024, nullptr, info_log);
    LOG(ERROR) << "Failed to compile shader (" << name << "):\n" << info_log;
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

}  // namespace

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept : id_(other.id_) {
  other.id_ = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
  if (this != &other) {
    if (id_) glDeleteProgram(id_);
    id_ = other.id_;
    other.id_ = 0;
  }
  return *this;
}

ShaderProgram::~ShaderProgram() {
  if (id_) glDeleteProgram(id_);
}

std::optional<ShaderProgram> ShaderProgram::CreateCompute(
    const std::filesystem::path& compute_path) {
  std::string compute_src = ReadFile(compute_path);
  if (compute_src.empty()) return std::nullopt;

  GLuint compute_shader =
      CompileShader(GL_COMPUTE_SHADER, compute_src, compute_path.string());
  if (!compute_shader) return std::nullopt;

  GLuint program = glCreateProgram();
  glAttachShader(program, compute_shader);
  glLinkProgram(program);

  // Flag shader for deletion when program is deleted
  glDeleteShader(compute_shader);

  GLint success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    char info_log[1024];
    glGetProgramInfoLog(program, 1024, nullptr, info_log);
    LOG(ERROR) << "Failed to link compute program:\n" << info_log;
    glDeleteProgram(program);
    return std::nullopt;
  }

  return ShaderProgram(program);
}

std::optional<ShaderProgram> ShaderProgram::CreateGraphics(
    const std::filesystem::path& vertex_path,
    const std::filesystem::path& fragment_path) {
  std::string vertex_src = ReadFile(vertex_path);
  if (vertex_src.empty()) return std::nullopt;

  std::string fragment_src = ReadFile(fragment_path);
  if (fragment_src.empty()) return std::nullopt;

  return CreateFromSource(vertex_src, fragment_src);
}

std::optional<ShaderProgram> ShaderProgram::CreateFromSource(
    std::string_view vertex_source, std::string_view fragment_source) {
  GLuint vertex_shader = CompileShader(
      GL_VERTEX_SHADER, std::string(vertex_source), "Vertex Source");
  if (!vertex_shader) return std::nullopt;

  GLuint fragment_shader = CompileShader(
      GL_FRAGMENT_SHADER, std::string(fragment_source), "Fragment Source");
  if (!fragment_shader) {
    glDeleteShader(vertex_shader);
    return std::nullopt;
  }

  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  GLint success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    char info_log[1024];
    glGetProgramInfoLog(program, 1024, nullptr, info_log);
    LOG(ERROR) << "Failed to link graphics program:\n" << info_log;
    glDeleteProgram(program);
    return std::nullopt;
  }

  return ShaderProgram(program);
}

void ShaderProgram::Uniform(std::string_view name, int value) const {
  glUniform1i(glGetUniformLocation(id_, std::string(name).c_str()), value);
}

void ShaderProgram::Uniform(std::string_view name, float value) const {
  glUniform1f(glGetUniformLocation(id_, std::string(name).c_str()), value);
}

void ShaderProgram::Uniform(std::string_view name,
                            const Eigen::Vector3f& value) const {
  glUniform3fv(glGetUniformLocation(id_, std::string(name).c_str()), 1,
               value.data());
}

void ShaderProgram::Uniform(std::string_view name,
                            const Eigen::Matrix4f& value) const {
  glUniformMatrix4fv(glGetUniformLocation(id_, std::string(name).c_str()), 1,
                     GL_FALSE, value.data());
}

void ShaderProgram::Use() const { glUseProgram(id_); }

}  // namespace sh_renderer
