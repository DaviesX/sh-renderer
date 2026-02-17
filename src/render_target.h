#pragma once

#include "glad.h"

namespace sh_renderer {

struct RenderTarget {
  GLuint fbo = 0;
  GLuint texture = 0;       // Color attachment (if applicable)
  GLuint depth_buffer = 0;  // Depth attachment (texture or rbo)
  int width = 0;
  int height = 0;
};

// Creates a RenderTarget with a depth attachment only (texture).
// Uses GL_DEPTH_COMPONENT32F for high precision.
RenderTarget CreateDepthTarget(int width, int height);

// Creates a RenderTarget with a single color attachment (texture).
// No depth buffer. Used for visualization.
RenderTarget CreateColorTarget(int width, int height);

}  // namespace sh_renderer