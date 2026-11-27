#pragma once

// The GLSL sources built into the library. Available only when the library is
// built with the CMake option SH_RENDERER_EMBED_GLSL=ON, which also defines
// the SH_RENDERER_EMBED_GLSL macro for users of the library.

#include <span>
#include <string_view>

#include "file_reader.h"

namespace sh_renderer {

// One embedded file: `path` is its reader path ("glsl/<name>"), `contents`
// its bytes.
struct EmbeddedFile {
  std::string_view path;
  std::string_view contents;
};

// Every file matching glsl/*.{vert,frag,comp,glsl}, sorted by path. Generated
// at build time by cmake/embed_glsl.cmake.
std::span<const EmbeddedFile> EmbeddedGlslFiles();

// A reader over EmbeddedGlslFiles(): "glsl/<name>" returns the built-in copy,
// any other path std::nullopt. Paths are matched exactly, so callers pass
// normalised reader paths (as the shader loader does).
FileReader MakeEmbeddedReader();

}  // namespace sh_renderer
