#include "engine_api.h"

#include <memory>
#include <sstream>
#include <utility>

#include "camera.h"
#ifdef SH_RENDERER_EMBED_GLSL
#include "embedded_glsl.h"
#endif
#include "file_reader.h"
#include "frame.h"
#include "glad.h"
#include "log.h"

struct ShRenderer {
  std::unique_ptr<sh_renderer::Renderer> renderer;
  sh_renderer::Camera camera{
      .position = Eigen::Vector3f::Zero(),
      .orientation = Eigen::Quaternionf::Identity(),
  };
  bool installed_sink = false;
  sh_renderer::LogSink previous_sink;
  bool installed_reader = false;
  sh_renderer::FileReader previous_reader;
  bool installed_debug_callback = false;
};

namespace {

using sh_renderer::LogSeverity;

std::string& LastError() {
  static std::string error;
  return error;
}

// glad's loader takes a GLADloadfunc; the host gives void* (*)(const char*).
void* (*g_get_proc_address)(const char*) = nullptr;

GLADapiproc LoadGlProc(const char* name) {
  return reinterpret_cast<GLADapiproc>(g_get_proc_address(name));
}

void GLAPIENTRY DebugMessageCallback(GLenum source, GLenum type, GLuint id,
                                     GLenum severity, GLsizei /*length*/,
                                     const GLchar* message,
                                     const void* /*user_param*/) {
  (void)source;
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

ShLogSeverity ToShSeverity(LogSeverity severity) {
  switch (severity) {
    case LogSeverity::kInfo:
      return ShLogSeverity::kInfo;
    case LogSeverity::kWarning:
      return ShLogSeverity::kWarning;
    default:
      return ShLogSeverity::kError;
  }
}

// Restores what ShCreate installed, in reverse order.
void RestoreHost(ShRenderer& sh) {
  if (sh.installed_debug_callback) {
    glDebugMessageCallback(nullptr, nullptr);
    glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDisable(GL_DEBUG_OUTPUT);
    sh.installed_debug_callback = false;
  }
  if (sh.installed_reader) {
    sh_renderer::SetDefaultFileReader(std::move(sh.previous_reader));
    sh.installed_reader = false;
  }
  if (sh.installed_sink) {
    sh_renderer::SetLogSink(std::move(sh.previous_sink));
    sh.installed_sink = false;
  }
}

}  // namespace

bool ShHasEmbeddedGlsl() {
#ifdef SH_RENDERER_EMBED_GLSL
  return true;
#else
  return false;
#endif
}

ShFileReader ShEmbeddedReader() {
#ifdef SH_RENDERER_EMBED_GLSL
  return sh_renderer::MakeEmbeddedReader();
#else
  return [](std::string_view) -> std::optional<std::string> {
    return std::nullopt;
  };
#endif
}

ShRenderer* ShCreate(const ShHostServices& services) {
  LastError().clear();
  auto sh = std::make_unique<ShRenderer>();

  if (services.log || services.fatal) {
    sh_renderer::LogSink sink;
    if (services.log) {
      sink.message = [log = services.log](LogSeverity severity,
                                          const char* file, int line,
                                          const std::string& message) {
        log(ToShSeverity(severity), file, line, message);
      };
    }
    sink.fatal = services.fatal;
    sh->previous_sink = sh_renderer::SetLogSink(std::move(sink));
    sh->installed_sink = true;
  }
  if (services.reader) {
    sh->previous_reader = sh_renderer::SetDefaultFileReader(services.reader);
    sh->installed_reader = true;
  }

  auto fail = [&sh](std::string error) -> ShRenderer* {
    LastError() = std::move(error);
    RestoreHost(*sh);
    return nullptr;
  };

  if (services.get_proc_address == nullptr) {
    return fail("ShCreate: no GL address resolver given.");
  }
  g_get_proc_address = services.get_proc_address;
  const int version = gladLoadGL(&LoadGlProc);
  g_get_proc_address = nullptr;
  if (version == 0) {
    return fail("Failed to load OpenGL entry points (no current context?).");
  }
  const int major = GLAD_VERSION_MAJOR(version);
  const int minor = GLAD_VERSION_MINOR(version);
  if (major < 4 || (major == 4 && minor < 6)) {
    std::ostringstream os;
    os << "OpenGL 4.6 core is required; the context is " << major << "."
       << minor << ".";
    return fail(os.str());
  }

  if (services.debug_gl) {
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(DebugMessageCallback, nullptr);
    sh->installed_debug_callback = true;
  }

  std::string error;
  sh->renderer = sh_renderer::CreateRenderer(&error);
  if (!sh->renderer) return fail(error);
  return sh.release();
}

const std::string& ShLastError() { return LastError(); }

bool ShBeginFrame(ShRenderer* sh, int width, int height) {
  if (sh == nullptr) return false;
  return sh_renderer::BeginFrame(*sh->renderer, width, height);
}

void ShEndFrame(ShRenderer* sh, double time_seconds) {
  if (sh == nullptr) return;
  sh_renderer::RenderScene(*sh->renderer, /*scene=*/nullptr, sh->camera,
                           time_seconds);
  sh_renderer::EndFrame(*sh->renderer);
}

void ShSetClearColor(ShRenderer* sh, const float rgb[3]) {
  if (sh == nullptr) return;
  sh->renderer->clear_color = Eigen::Vector3f(rgb[0], rgb[1], rgb[2]);
}

void ShSetDisplayGamma(ShRenderer* sh, float gamma) {
  if (sh == nullptr) return;
  sh_renderer::SetDisplayGamma(*sh->renderer, gamma);
}

void ShReadPixels(int width, int height, uint8_t* rgb) {
  sh_renderer::ReadDefaultFramebufferRGB(width, height, rgb);
}

void ShDestroy(ShRenderer* sh) {
  if (sh == nullptr) return;
  sh_renderer::DestroyRenderer(std::move(sh->renderer));
  RestoreHost(*sh);
  delete sh;
}
