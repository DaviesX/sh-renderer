#pragma once

#include "glad.h"

namespace sh_renderer {

struct RenderTarget {
  GLuint fbo = 0;
  GLuint texture = 0;         // Color attachment (if applicable)
  GLuint normal_texture = 0;  // Normal attachment (GL_RGB10_A2)
  GLuint depth_buffer = 0;    // Depth attachment (texture or rbo)
  int width = 0;
  int height = 0;
};

// Creates a RenderTarget with a depth attachment only (texture).
// Uses GL_DEPTH_COMPONENT32F for high precision.
RenderTarget CreateDepthTarget(int width, int height);

// Creates a RenderTarget with a single color attachment (texture).
// No depth buffer. Used for visualization.
RenderTarget CreateColorTarget(int width, int height);

// Creates a RenderTarget with a GL_RGB10_A2 normal attachment and a depth
// attachment. Used for the depth pre-pass.
RenderTarget CreateDepthAndNormalTarget(int width, int height);

// Creates a RenderTarget with a float color attachment and a depth attachment.
// Used for HDR rendering. If shared_depth is > 0, attaches the existing deep
// texture.
RenderTarget CreateHDRTarget(int width, int height, GLuint shared_depth = 0);

// Creates a RenderTarget with a single channel color attachment (GL_R8).
// Used for SSAO.
RenderTarget CreateSSAOTarget(int width, int height);

}  // namespace sh_renderer