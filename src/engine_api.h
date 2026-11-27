#pragma once

// The engine-facing facade of the renderer core. It uses only standard types
// and an opaque handle, so a host translation unit (the ioquake3 module's
// glue) includes this header alone: no Eigen, no glad, no other core header,
// and none of the core's compiler flags.
//
// The host owns the window, the GL context and the swap:
//
//   ShRenderer* r = ShCreate(services);  // context current; null on failure
//   ...
//   if (ShBeginFrame(r, w, h)) {          // false for a 0x0 drawable
//     ShEndFrame(r, time_seconds);        // tonemapped frame in framebuffer 0
//     ShReadPixels(w, h, rgb);            // optional, before the swap
//   }
//   swap
//   ...
//   ShDestroy(r);

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

struct ShRenderer;

enum class ShLogSeverity { kInfo, kWarning, kError };

// Reads a file by forward-slash path ("glsl/tonemap.frag"), or returns
// std::nullopt when there is none.
using ShFileReader =
    std::function<std::optional<std::string>(std::string_view path)>;

// True when the library was built with its GLSL embedded (the CMake option
// SH_RENDERER_EMBED_GLSL).
bool ShHasEmbeddedGlsl();

// A reader over the library's built-in GLSL: "glsl/<name>" returns the
// embedded copy, any other path std::nullopt. Without embedded GLSL
// (ShHasEmbeddedGlsl() false) it returns std::nullopt for every path. A host
// can wrap it, for example to let files on disk override it.
ShFileReader ShEmbeddedReader();

struct ShHostServices {
  // Installed as the core's process-wide file reader for every program and
  // #include. Empty: the core's current default reader (a disk reader at the
  // working directory unless replaced).
  ShFileReader reader;

  // Receives the core's INFO, WARNING and ERROR messages. Empty: stderr.
  std::function<void(ShLogSeverity severity, const char* file, int line,
                     const std::string& message)>
      log;
  // Receives a failed check or fatal message. It should not return; if it
  // does, the process aborts. Empty: stderr, then abort.
  std::function<void(const char* file, int line, const std::string& message)>
      fatal;

  // Resolves a GL entry point by name (SDL_GL_GetProcAddress in the module).
  // Required.
  void* (*get_proc_address)(const char* name) = nullptr;

  // Installs a GL debug message callback that logs through `log`.
  bool debug_gl = false;
};

// Loads GL through `services.get_proc_address` for the current context,
// checks that it is at least OpenGL 4.6, installs the log sink and reader,
// and builds the renderer, then enables GL_FRAMEBUFFER_SRGB. Returns null on
// failure; ShLastError() then says why: a version below 4.6, or one line per
// program that failed to build with its files and the reason (the missing
// file, or the driver's info log). On failure the previous log sink and
// reader are restored and GL_FRAMEBUFFER_SRGB is left as it was.
ShRenderer* ShCreate(const ShHostServices& services);

// Why the last ShCreate failed; empty after a successful one.
const std::string& ShLastError();

// Starts a frame of `width` x `height` pixels. Returns false, and the frame is
// skipped, when either is 0 (a minimised window); ShEndFrame then does
// nothing.
bool ShBeginFrame(ShRenderer* renderer, int width, int height);

// Renders the frame started by ShBeginFrame (today: the HDR target cleared to
// the clear colour) and tonemaps it into framebuffer 0, without swapping.
// `time_seconds` is the host's clock.
//
// GL state left for the host: framebuffer 0 bound, the frame's viewport,
// GL_FRAMEBUFFER_SRGB enabled, no program, no vertex array, no texture on
// unit 0 and GL_DEPTH_TEST disabled. Blend, cull, depth function and mask,
// colour mask and other bindings are as the renderer's last pass left them,
// so a host that draws afterwards (2D, console) sets those itself.
void ShEndFrame(ShRenderer* renderer, double time_seconds = 0.0);

// Linear RGB the HDR target is cleared to each frame. Default black.
void ShSetClearColor(ShRenderer* renderer, const float rgb[3]);

// Display gamma g applied in the tonemap: with e the sRGB encoding of the
// tonemapped colour, the framebuffer stores e^(1/g). Default 1 (unchanged).
// Like stock r_gamma, g is clamped to [0.5, 3.0]; a non-finite or
// non-positive g is taken as 1, with a warning logged once.
void ShSetDisplayGamma(ShRenderer* renderer, float gamma);

// Reads framebuffer 0 (the back buffer, so call it after ShEndFrame and
// before the swap) as tightly packed RGB bytes, bottom row first, into `rgb`
// (width * height * 3 bytes).
void ShReadPixels(int width, int height, uint8_t* rgb);

// Deletes the renderer's GL objects (the context must be current) and
// restores the log sink and reader that ShCreate replaced. Null is allowed.
void ShDestroy(ShRenderer* renderer);
