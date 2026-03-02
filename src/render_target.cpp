#include "render_target.h"

#include <glog/logging.h>

namespace sh_renderer {

RenderTarget CreateDepthTarget(int width, int height) {
  RenderTarget target;
  target.width = width;
  target.height = height;

  // Create FBO
  glCreateFramebuffers(1, &target.fbo);

  // Create Depth Texture
  glCreateTextures(GL_TEXTURE_2D, 1, &target.depth_buffer);
  glTextureStorage2D(target.depth_buffer, 1, GL_DEPTH_COMPONENT32F, width,
                     height);

  glTextureParameteri(target.depth_buffer, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTextureParameteri(target.depth_buffer, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTextureParameteri(target.depth_buffer, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTextureParameteri(target.depth_buffer, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Attach Depth Texture to FBO
  glNamedFramebufferTexture(target.fbo, GL_DEPTH_ATTACHMENT,
                            target.depth_buffer, 0);

  // No Color Buffer
  glNamedFramebufferDrawBuffer(target.fbo, GL_NONE);
  glNamedFramebufferReadBuffer(target.fbo, GL_NONE);

  if (glCheckNamedFramebufferStatus(target.fbo, GL_FRAMEBUFFER) !=
      GL_FRAMEBUFFER_COMPLETE) {
    LOG(ERROR) << "Depth Framebuffer is not complete!";
  }

  return target;
}

RenderTarget CreateColorTarget(int width, int height) {
  RenderTarget target;
  target.width = width;
  target.height = height;

  // Create FBO
  glCreateFramebuffers(1, &target.fbo);

  // Create Color Texture
  glCreateTextures(GL_TEXTURE_2D, 1, &target.texture);
  glTextureStorage2D(target.texture, 1, GL_RGB8, width, height);

  glTextureParameteri(target.texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTextureParameteri(target.texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTextureParameteri(target.texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTextureParameteri(target.texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Attach Color Texture to FBO
  glNamedFramebufferTexture(target.fbo, GL_COLOR_ATTACHMENT0, target.texture,
                            0);

  if (glCheckNamedFramebufferStatus(target.fbo, GL_FRAMEBUFFER) !=
      GL_FRAMEBUFFER_COMPLETE) {
    LOG(ERROR) << "Color Framebuffer is not complete!";
  }

  return target;
}

RenderTarget CreateHDRTarget(int width, int height, GLuint shared_depth) {
  RenderTarget target;
  target.width = width;
  target.height = height;

  // Create FBO
  glCreateFramebuffers(1, &target.fbo);

  // Create Color Texture (HDR)
  glCreateTextures(GL_TEXTURE_2D, 1, &target.texture);
  glTextureStorage2D(target.texture, 1, GL_RGBA16F, width, height);

  glTextureParameteri(target.texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTextureParameteri(target.texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTextureParameteri(target.texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTextureParameteri(target.texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (shared_depth > 0) {
    target.depth_buffer = shared_depth;
  } else {
    // Create Depth Texture
    glCreateTextures(GL_TEXTURE_2D, 1, &target.depth_buffer);
    glTextureStorage2D(target.depth_buffer, 1, GL_DEPTH_COMPONENT32F, width,
                       height);

    glTextureParameteri(target.depth_buffer, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(target.depth_buffer, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(target.depth_buffer, GL_TEXTURE_WRAP_S,
                        GL_CLAMP_TO_EDGE);
    glTextureParameteri(target.depth_buffer, GL_TEXTURE_WRAP_T,
                        GL_CLAMP_TO_EDGE);
  }

  // Attach Textures
  glNamedFramebufferTexture(target.fbo, GL_COLOR_ATTACHMENT0, target.texture,
                            0);
  glNamedFramebufferTexture(target.fbo, GL_DEPTH_ATTACHMENT,
                            target.depth_buffer, 0);

  if (glCheckNamedFramebufferStatus(target.fbo, GL_FRAMEBUFFER) !=
      GL_FRAMEBUFFER_COMPLETE) {
    LOG(ERROR) << "HDR Framebuffer is not complete!";
  }

  return target;
}

RenderTarget CreateDepthAndNormalTarget(int width, int height) {
  RenderTarget target;
  target.width = width;
  target.height = height;

  // Create FBO
  glCreateFramebuffers(1, &target.fbo);

  // Create Depth Texture
  glCreateTextures(GL_TEXTURE_2D, 1, &target.depth_buffer);
  glTextureStorage2D(target.depth_buffer, 1, GL_DEPTH_COMPONENT32F, width,
                     height);

  glTextureParameteri(target.depth_buffer, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTextureParameteri(target.depth_buffer, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTextureParameteri(target.depth_buffer, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTextureParameteri(target.depth_buffer, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Create Normal Texture (MRT)
  glCreateTextures(GL_TEXTURE_2D, 1, &target.normal_texture);
  glTextureStorage2D(target.normal_texture, 1, GL_RGB10_A2, width, height);

  glTextureParameteri(target.normal_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTextureParameteri(target.normal_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTextureParameteri(target.normal_texture, GL_TEXTURE_WRAP_S,
                      GL_CLAMP_TO_EDGE);
  glTextureParameteri(target.normal_texture, GL_TEXTURE_WRAP_T,
                      GL_CLAMP_TO_EDGE);

  // Attach Textures
  glNamedFramebufferTexture(target.fbo, GL_COLOR_ATTACHMENT0,
                            target.normal_texture, 0);
  glNamedFramebufferTexture(target.fbo, GL_DEPTH_ATTACHMENT,
                            target.depth_buffer, 0);

  if (glCheckNamedFramebufferStatus(target.fbo, GL_FRAMEBUFFER) !=
      GL_FRAMEBUFFER_COMPLETE) {
    LOG(ERROR) << "Depth and Normal Framebuffer is not complete!";
  }

  return target;
}

RenderTarget CreateSSAOTarget(int width, int height) {
  RenderTarget target;
  target.width = width;
  target.height = height;

  // Create FBO
  glCreateFramebuffers(1, &target.fbo);

  // Create Color Texture (GL_R8)
  glCreateTextures(GL_TEXTURE_2D, 1, &target.texture);
  glTextureStorage2D(target.texture, 1, GL_R8, width, height);

  glTextureParameteri(target.texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTextureParameteri(target.texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTextureParameteri(target.texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTextureParameteri(target.texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Attach Color Texture to FBO
  glNamedFramebufferTexture(target.fbo, GL_COLOR_ATTACHMENT0, target.texture,
                            0);

  if (glCheckNamedFramebufferStatus(target.fbo, GL_FRAMEBUFFER) !=
      GL_FRAMEBUFFER_COMPLETE) {
    LOG(ERROR) << "SSAO Framebuffer is not complete!";
  }

  return target;
}

}  // namespace sh_renderer