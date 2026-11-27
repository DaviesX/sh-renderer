#include "shader.h"

#include <algorithm>
#include <initializer_list>
#include <sstream>
#include <utility>

#include "log.h"

namespace sh_renderer {
namespace {

constexpr int kMaxIncludeDepth = 16;
constexpr std::string_view kIncludeDirective = "#include";

// Reports a failure to `*error` when the caller asked for it, and logs it
// otherwise.
void ReportError(std::string message, std::string* error) {
  if (error != nullptr) {
    *error = std::move(message);
  } else {
    LOG(ERROR) << message;
  }
}

std::string ShaderInfoLog(GLuint shader) {
  GLint length = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
  std::string log(static_cast<size_t>(std::max(length, 1)), '\0');
  GLsizei written = 0;
  glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), &written,
                     log.data());
  log.resize(static_cast<size_t>(std::max(written, 0)));
  return log;
}

std::string ProgramInfoLog(GLuint program) {
  GLint length = 0;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
  std::string log(static_cast<size_t>(std::max(length, 1)), '\0');
  GLsizei written = 0;
  glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), &written,
                      log.data());
  log.resize(static_cast<size_t>(std::max(written, 0)));
  return log;
}

// Compiles one stage. Returns 0 and sets `*error` on failure.
GLuint CompileShader(GLenum type, const std::string& source,
                     const std::string& name,
                     const std::map<std::string, std::string>& macros,
                     std::string* error) {
  std::string final_source = source;
  if (!macros.empty()) {
    std::ostringstream macro_stream;
    for (const auto& [key, val] : macros) {
      macro_stream << "#define " << key << " " << val << "\n";
    }
    std::string macro_str = macro_stream.str();

    size_t version_pos = final_source.find("#version");
    if (version_pos != std::string::npos) {
      size_t end_of_line = final_source.find('\n', version_pos);
      if (end_of_line != std::string::npos) {
        final_source.insert(end_of_line + 1, macro_str);
      } else {
        final_source += "\n" + macro_str;
      }
    } else {
      final_source = macro_str + final_source;
    }
  }

  GLuint shader = glCreateShader(type);
  const char* src = final_source.c_str();
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    *error =
        "Failed to compile shader (" + name + "):\n" + ShaderInfoLog(shader);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

// Links the given compiled stages into a program and flags the stages for
// deletion. Returns 0 and sets `*error` on failure.
GLuint LinkProgram(std::initializer_list<GLuint> shaders,
                   const std::string& name, std::string* error) {
  GLuint program = glCreateProgram();
  for (GLuint shader : shaders) glAttachShader(program, shader);
  glLinkProgram(program);
  for (GLuint shader : shaders) glDeleteShader(shader);

  GLint success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    *error =
        "Failed to link program (" + name + "):\n" + ProgramInfoLog(program);
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

// Compiles and links a vertex + fragment program from resolved sources.
// Returns 0 and sets `*error` on failure.
GLuint BuildGraphics(const std::string& vertex_source,
                     const std::string& vertex_name,
                     const std::string& fragment_source,
                     const std::string& fragment_name,
                     const std::map<std::string, std::string>& macros,
                     std::string* error) {
  GLuint vertex_shader = CompileShader(GL_VERTEX_SHADER, vertex_source,
                                       vertex_name, macros, error);
  if (!vertex_shader) return 0;

  GLuint fragment_shader = CompileShader(GL_FRAGMENT_SHADER, fragment_source,
                                         fragment_name, macros, error);
  if (!fragment_shader) {
    glDeleteShader(vertex_shader);
    return 0;
  }

  return LinkProgram({vertex_shader, fragment_shader},
                     vertex_name + ", " + fragment_name, error);
}

std::optional<std::string> ReadThrough(const FileReader& reader,
                                       std::string_view path) {
  if (!reader) return std::nullopt;
  return reader(path);
}

std::string ExpandIncludes(std::string_view source, std::string_view dir,
                           const FileReader& reader, int depth) {
  if (depth > kMaxIncludeDepth) {
    LOG(ERROR) << "Shader #include nesting too deep (under \"" << dir << "\")";
    return std::string(source);
  }

  // Scan line by line, inlining the content of each #include directive.
  std::istringstream in{std::string(source)};
  std::ostringstream out;
  std::string line;
  while (std::getline(in, line)) {
    size_t line_start = line.find_first_not_of(" \t");
    if (line_start == std::string::npos ||
        line.compare(line_start, kIncludeDirective.size(), kIncludeDirective) !=
            0) {
      // This line is not an #include directive, so write it as is.
      out << line << "\n";
      continue;
    }

    // An #include directive: parse the quoted path and inline that file.
    size_t file_name_start =
        line.find('"', line_start + kIncludeDirective.size());
    size_t file_name_end = (file_name_start == std::string::npos)
                               ? std::string::npos
                               : line.find('"', file_name_start + 1);
    if (file_name_start == std::string::npos ||
        file_name_end == std::string::npos ||
        file_name_end <= file_name_start) {
      LOG(ERROR) << "Malformed shader #include: " << line;
      out << line << "\n";
      continue;
    }

    std::string include_file_path =
        line.substr(file_name_start + 1, file_name_end - file_name_start - 1);
    std::string inc_path = JoinReaderPath(dir, include_file_path);
    std::optional<std::string> inc_src = ReadThrough(reader, inc_path);
    if (!inc_src) {
      LOG(ERROR) << "Failed to open shader file: \"" << inc_path << "\"";
    }
    out << ExpandIncludes(inc_src.value_or(""), ReaderPathDirectory(inc_path),
                          reader, depth + 1)
        << "\n";
  }

  return out.str();
}

// Reads and resolves a shader file, reporting a missing or empty file.
std::optional<std::string> LoadSource(const std::string& path,
                                      const FileReader& reader,
                                      std::string* error) {
  std::optional<std::string> src = ReadThrough(reader, path);
  if (!src || src->empty()) {
    *error = "Failed to open shader file: \"" + path + "\"";
    return std::nullopt;
  }
  return ExpandIncludes(*src, ReaderPathDirectory(path), reader, /*depth=*/0);
}

const FileReader& ReaderOrDefault(const FileReader* reader) {
  return reader != nullptr ? *reader : DefaultFileReader();
}

}  // namespace

std::string ResolveShaderIncludes(std::string_view source,
                                  const std::filesystem::path& base_dir) {
  return ExpandIncludes(source, "", MakeDiskReader(base_dir), /*depth=*/0);
}

std::string ResolveShaderIncludes(std::string_view source,
                                  std::string_view source_dir,
                                  const FileReader& reader) {
  return ExpandIncludes(source, source_dir, reader, /*depth=*/0);
}

std::optional<std::string> LoadShaderSource(std::string_view path,
                                            const FileReader& reader) {
  std::string error;
  std::optional<std::string> src =
      LoadSource(std::string(path), reader, &error);
  if (!src) LOG(ERROR) << error;
  return src;
}

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
    const std::filesystem::path& compute_path,
    const std::map<std::string, std::string>& macros, const FileReader* reader,
    std::string* error) {
  const FileReader& files = ReaderOrDefault(reader);
  const std::string compute_name = compute_path.generic_string();
  std::string message;

  std::optional<std::string> compute_src =
      LoadSource(compute_name, files, &message);
  if (!compute_src) {
    ReportError(std::move(message), error);
    return std::nullopt;
  }

  GLuint compute_shader = CompileShader(GL_COMPUTE_SHADER, *compute_src,
                                        compute_name, macros, &message);
  if (!compute_shader) {
    ReportError(std::move(message), error);
    return std::nullopt;
  }

  GLuint program = LinkProgram({compute_shader}, compute_name, &message);
  if (!program) {
    ReportError(std::move(message), error);
    return std::nullopt;
  }
  return ShaderProgram(program);
}

std::optional<ShaderProgram> ShaderProgram::CreateGraphics(
    const std::filesystem::path& vertex_path,
    const std::filesystem::path& fragment_path,
    const std::map<std::string, std::string>& macros, const FileReader* reader,
    std::string* error) {
  const FileReader& files = ReaderOrDefault(reader);
  const std::string vertex_name = vertex_path.generic_string();
  const std::string fragment_name = fragment_path.generic_string();
  std::string message;

  std::optional<std::string> vertex_src =
      LoadSource(vertex_name, files, &message);
  if (!vertex_src) {
    ReportError(std::move(message), error);
    return std::nullopt;
  }
  std::optional<std::string> fragment_src =
      LoadSource(fragment_name, files, &message);
  if (!fragment_src) {
    ReportError(std::move(message), error);
    return std::nullopt;
  }

  GLuint program = BuildGraphics(*vertex_src, vertex_name, *fragment_src,
                                 fragment_name, macros, &message);
  if (!program) {
    ReportError(std::move(message), error);
    return std::nullopt;
  }
  return ShaderProgram(program);
}

std::optional<ShaderProgram> ShaderProgram::CreateFromSource(
    std::string_view vertex_source, std::string_view fragment_source,
    const std::map<std::string, std::string>& macros, std::string* error) {
  std::string message;
  GLuint program = BuildGraphics(std::string(vertex_source), "Vertex Source",
                                 std::string(fragment_source),
                                 "Fragment Source", macros, &message);
  if (!program) {
    ReportError(std::move(message), error);
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
                            const Eigen::Vector2i& value) const {
  glUniform2iv(glGetUniformLocation(id_, std::string(name).c_str()), 1,
               value.data());
}

void ShaderProgram::Uniform(std::string_view name,
                            const Eigen::Vector2f& value) const {
  glUniform2fv(glGetUniformLocation(id_, std::string(name).c_str()), 1,
               value.data());
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
