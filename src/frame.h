#pragma once

// The inverted frame API: the host owns the window, the GL context, the swap
// and input, and drives the renderer one frame at a time.
//
//   std::unique_ptr<Renderer> r = CreateRenderer(&error);  // context current
//   while (...) {
//     if (!BeginFrame(*r, width, height)) continue;  // 0x0: skip the frame
//     RenderScene(*r, scene, camera, time_seconds);   // scene may be null
//     EndFrame(*r);  // tonemapped frame in framebuffer 0, not swapped
//     swap buffers
//   }
//   DestroyRenderer(std::move(r));

#include <Eigen/Core>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "camera.h"
#include "compute_light_tile.h"
#include "draw_ssao.h"
#include "render_target.h"
#include "scene.h"
#include "shader.h"

namespace sh_renderer {

// The display gamma range, as stock ioquake3 clamps r_gamma.
inline constexpr float kMinDisplayGamma = 0.5f;
inline constexpr float kMaxDisplayGamma = 3.0f;

struct Renderer {
  // Programs, built by CreateRenderer through the default file reader.
  ShaderProgram shadow_map_opaque_program;
  ShaderProgram shadow_map_cutout_program;
  ShaderProgram depth_opaque_program;
  ShaderProgram depth_cutout_program;
  ShaderProgram radiance_program;
  ShaderProgram sky_program;
  ShaderProgram tonemap_program;
  ShaderProgram light_cull_program;
  ShaderProgram ssao_program;
  ShaderProgram ssao_blur_horizontal_program;
  ShaderProgram ssao_blur_vertical_program;

  // Size-independent resources. `empty_vao` is bound for the tonemap's
  // attribute-less full-screen triangle (a core context draws nothing with no
  // vertex array bound, which happens when no scene pass ran).
  GLuint empty_vao = 0;
  SSAOContext ssao_context;
  std::vector<RenderTarget> sun_shadow_map_targets;

  // Size-dependent targets, made by the first BeginFrame and remade when the
  // size changes. `width`/`height` are 0 until then.
  int width = 0;
  int height = 0;
  RenderTarget depth_normal_target;
  RenderTarget hdr_target;
  RenderTarget ssao_target;
  RenderTarget ssao_blur_temp;
  RenderTarget ssao_blur_target;
  TileLightListList tile_light_list;

  // Scene-sized resources, made by RenderScene from the first scene it gets
  // and remade when the scene asks for another resolution. 0 = none yet.
  RenderTarget spot_shadow_atlas;
  uint32_t spot_shadow_atlas_resolution = 0;

  // Linear colour the HDR target is cleared to at the start of RenderScene.
  // With a scene, the radiance pass clears again to black, as before.
  Eigen::Vector3f clear_color = Eigen::Vector3f::Zero();
  // Display gamma g applied in the tonemap (decision 8 of the C1 design):
  // with e the sRGB encoding of the tonemapped colour, the framebuffer gets
  // e^(1/g). At 1 the tonemap output is unchanged. Set it with
  // SetDisplayGamma; EndFrame sanitises a value written here directly the
  // same way.
  float display_gamma = 1.0f;

  // True between a BeginFrame that returned true and the next EndFrame.
  bool in_frame = false;
};

// Builds the programs and size-independent resources, then enables
// GL_FRAMEBUFFER_SRGB. Needs a current GL 4.6 core context with glad loaded.
// Returns null when a program fails to build, leaving GL_FRAMEBUFFER_SRGB as
// it was; `*error` (when given) then has one line per failed program with its
// name, its files and the reason (the missing file or the driver's info log),
// which has also been logged as an ERROR.
std::unique_ptr<Renderer> CreateRenderer(std::string* error = nullptr);

// Starts a frame of `width` x `height` pixels: makes or resizes the targets and
// sets the viewport. Returns false, doing nothing, when either is 0 (for
// example a minimised window); the host then skips RenderScene and EndFrame.
bool BeginFrame(Renderer& renderer, int width, int height);

// Clears the HDR target to `renderer.clear_color` and, when `scene` is given,
// runs the shadow, depth, SSAO, light culling, radiance and sky passes.
// `time_seconds` is the host's clock (layer animation).
void RenderScene(Renderer& renderer, Scene* scene, const Camera& camera,
                 double time_seconds);

// Tonemaps the HDR target into framebuffer 0. Does not swap.
//
// GL state left for the host afterwards: framebuffer 0 bound for drawing, the
// viewport of the frame, GL_FRAMEBUFFER_SRGB enabled (from CreateRenderer),
// no program, no vertex array, no texture on unit 0, and GL_DEPTH_TEST
// disabled. Other state the passes touch (blend, cull, depth function and
// mask, colour mask, other texture units and image/buffer bindings) is left
// as the last pass set it; BeginFrame and RenderScene set what they need.
void EndFrame(Renderer& renderer);

// Returns `gamma` clamped to [kMinDisplayGamma, kMaxDisplayGamma]; a
// non-finite or non-positive value gives 1, with a warning logged once per
// process.
float SanitizeDisplayGamma(float gamma);

// Sets `renderer.display_gamma` to SanitizeDisplayGamma(gamma).
void SetDisplayGamma(Renderer& renderer, float gamma);

// Reads framebuffer 0's colour buffer (the back buffer of a double-buffered
// window) as tightly packed RGB bytes, bottom row first, into `rgb`, which
// holds width * height * 3 bytes.
void ReadDefaultFramebufferRGB(int width, int height, uint8_t* rgb);

// Deletes every GL object the renderer owns. The context must be current.
void DestroyRenderer(std::unique_ptr<Renderer> renderer);

}  // namespace sh_renderer
