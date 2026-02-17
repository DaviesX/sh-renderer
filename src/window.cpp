#include "window.h"

#include <glog/logging.h>

#include <optional>
#include <string>
#include <string_view>

namespace sh_renderer {

namespace {

void GLAPIENTRY DebugMessageCallback(GLenum source, GLenum type, GLuint id,
                                     GLenum severity, GLsizei /*length*/,
                                     const GLchar* message,
                                     const void* /*user_param*/) {
  // Ignore non-significant notifications.
  if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

  const char* severity_str = "UNKNOWN";
  switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
      severity_str = "HIGH";
      break;
    case GL_DEBUG_SEVERITY_MEDIUM:
      severity_str = "MEDIUM";
      break;
    case GL_DEBUG_SEVERITY_LOW:
      severity_str = "LOW";
      break;
  }

  const char* type_str = "OTHER";
  switch (type) {
    case GL_DEBUG_TYPE_ERROR:
      type_str = "ERROR";
      break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
      type_str = "DEPRECATED";
      break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
      type_str = "UNDEFINED_BEHAVIOR";
      break;
    case GL_DEBUG_TYPE_PERFORMANCE:
      type_str = "PERFORMANCE";
      break;
  }

  if (type == GL_DEBUG_TYPE_ERROR) {
    LOG(ERROR) << "GL " << type_str << " [" << severity_str << "] id=" << id
               << ": " << message;
  } else {
    LOG(WARNING) << "GL " << type_str << " [" << severity_str << "] id=" << id
                 << ": " << message;
  }
}

}  // namespace

std::optional<Window> CreateWindow(unsigned width, unsigned height,
                                   std::string_view title,
                                   unsigned msaa_samples) {
  glfwSetErrorCallback([](int error, const char* description) {
    LOG(ERROR) << "GLFW Error " << error << ": " << description;
  });

  if (!glfwInit()) {
    LOG(ERROR) << "Failed to initialize GLFW.";
    return std::nullopt;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

  if (msaa_samples > 0) {
    glfwWindowHint(GLFW_SAMPLES, static_cast<int>(msaa_samples));
  }

  // GLFW requires a null-terminated string.
  std::string title_str(title);
  GLFWwindow* window =
      glfwCreateWindow(static_cast<int>(width), static_cast<int>(height),
                       title_str.c_str(), nullptr, nullptr);
  if (!window) {
    LOG(ERROR) << "Failed to create GLFW window.";
    glfwTerminate();
    return std::nullopt;
  }

  glfwMakeContextCurrent(window);

  // Initialize GLAD for OpenGL function loading.
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    LOG(ERROR) << "Failed to initialize GLAD.";
    glfwDestroyWindow(window);
    glfwTerminate();
    return std::nullopt;
  }

  LOG(INFO) << "OpenGL " << glGetString(GL_VERSION)
            << " | Renderer: " << glGetString(GL_RENDERER);

  // Enable debug output.
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  // glDebugMessageCallback(DebugMessageCallback, nullptr);

  if (msaa_samples > 0) {
    glEnable(GL_MULTISAMPLE);
  }

  // Disable VSync by default for uncapped framerate profiling.
  glfwSwapInterval(1);

  return window;
}

void DestroyWindow(Window window) {
  if (window) {
    glfwDestroyWindow(window);
  }
}

}  // namespace sh_renderer