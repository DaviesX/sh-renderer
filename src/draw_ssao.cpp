#include "draw_ssao.h"

#include <glog/logging.h>

#include <random>

#include "glad.h"

namespace sh_renderer {

namespace {

GLuint GetFullscreenQuadVAO() {
  static GLuint vao = 0;
  if (vao == 0) {
    const float kQuadVertices[] = {
        // positions        // uvs
        -1.0f, 1.0f, 0.0f, 0.0f,  1.0f, -1.0f, -1.0f, 0.0f,
        0.0f,  0.0f, 1.0f, -1.0f, 0.0f, 1.0f,  0.0f,

        -1.0f, 1.0f, 0.0f, 0.0f,  1.0f, 1.0f,  -1.0f, 0.0f,
        1.0f,  0.0f, 1.0f, 1.0f,  0.0f, 1.0f,  1.0f};

    GLuint vbo;
    glCreateVertexArrays(1, &vao);
    glCreateBuffers(1, &vbo);
    glNamedBufferStorage(vbo, sizeof(kQuadVertices), kQuadVertices, 0);

    // Position
    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(vao, 0, 0);

    // UV
    glEnableVertexArrayAttrib(vao, 2);  // location 2 as per fullscreen.vert
    glVertexArrayAttribFormat(vao, 2, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    glVertexArrayAttribBinding(vao, 2, 0);

    glVertexArrayVertexBuffer(vao, 0, vbo, 0, 5 * sizeof(float));
  }
  return vao;
}

float Lerp(float a, float b, float f) { return a + f * (b - a); }

}  // namespace

ShaderProgram CreateSSAOProgram() {
  auto program =
      ShaderProgram::CreateGraphics("glsl/fullscreen.vert", "glsl/ssao.frag");
  if (!program) {
    LOG(ERROR) << "Failed to create SSAO shader program.";
    return {};
  }
  return std::move(*program);
}

ShaderProgram CreateSSAOBlurProgram() {
  auto program =
      ShaderProgram::CreateGraphics("glsl/fullscreen.vert", "glsl/blur.frag");
  if (!program) {
    LOG(ERROR) << "Failed to create SSAO blur shader program.";
    return {};
  }
  return std::move(*program);
}

SSAOContext CreateSSAOContext() {
  SSAOContext ctx;
  std::uniform_real_distribution<float> random_floats(
      0.0, 1.0);  // random floats between 0.0 - 1.0
  std::default_random_engine generator;

  for (unsigned int i = 0; i < 64; ++i) {
    Eigen::Vector3f sample(random_floats(generator) * 2.0 - 1.0,
                           random_floats(generator) * 2.0 - 1.0,
                           random_floats(generator));
    sample.normalize();
    sample *= random_floats(generator);
    float scale = float(i) / 64.0;
    // Scale samples s.t. they're more aligned to center of kernel
    scale = Lerp(0.1f, 1.0f, scale * scale);
    sample *= scale;
    ctx.kernel.push_back(sample);
  }

  std::vector<Eigen::Vector3f> ssao_noise;
  for (unsigned int i = 0; i < 16; i++) {
    Eigen::Vector3f noise(random_floats(generator) * 2.0 - 1.0,
                          random_floats(generator) * 2.0 - 1.0, 0.0f);
    ssao_noise.push_back(noise);
  }

  glCreateTextures(GL_TEXTURE_2D, 1, &ctx.noise_texture);
  glTextureStorage2D(ctx.noise_texture, 1, GL_RGB32F, 4, 4);
  glTextureSubImage2D(ctx.noise_texture, 0, 0, 0, 4, 4, GL_RGB, GL_FLOAT,
                      ssao_noise.data());
  glTextureParameteri(ctx.noise_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTextureParameteri(ctx.noise_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTextureParameteri(ctx.noise_texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTextureParameteri(ctx.noise_texture, GL_TEXTURE_WRAP_T, GL_REPEAT);

  return ctx;
}

void DestroySSAOContext(SSAOContext* ctx) {
  if (ctx->noise_texture != 0) {
    glDeleteTextures(1, &ctx->noise_texture);
    ctx->noise_texture = 0;
  }
}

void DrawSSAO(const RenderTarget& depth_normal_target, const Camera& camera,
              const ShaderProgram& ssao_program, const SSAOContext& context,
              const RenderTarget& ssao_out) {
  glBindFramebuffer(GL_FRAMEBUFFER, ssao_out.fbo);
  glViewport(0, 0, ssao_out.width, ssao_out.height);
  glClear(GL_COLOR_BUFFER_BIT);

  // Disable depth testing since we draw a fullscreen quad.
  glDisable(GL_DEPTH_TEST);

  ssao_program.Use();

  // Bind depth map to binding 0
  glBindTextureUnit(0, depth_normal_target.depth_buffer);

  // Bind normal map to binding 1
  glBindTextureUnit(1, depth_normal_target.normal_texture);

  // Bind noise texture to binding 2
  glBindTextureUnit(2, context.noise_texture);

  // Upload Uniforms
  ssao_program.Uniform("u_projection", GetProjectionMatrix(camera));
  ssao_program.Uniform("u_inv_projection",
                       GetProjectionMatrix(camera).inverse().eval());
  glUniform2f(glGetUniformLocation(ssao_program.id(), "u_resolution"),
              ssao_out.width, ssao_out.height);

  for (unsigned int i = 0; i < 64; ++i) {
    ssao_program.Uniform("u_samples[" + std::to_string(i) + "]",
                         context.kernel[i]);
  }

  // Draw fullscreen quad
  glBindVertexArray(GetFullscreenQuadVAO());
  glDrawArrays(GL_TRIANGLES, 0, 6);

  // Clean up
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DrawSSAOBlur(const RenderTarget& ssao_in,
                  const ShaderProgram& blur_program,
                  const RenderTarget& blur_out) {
  glBindFramebuffer(GL_FRAMEBUFFER, blur_out.fbo);
  glViewport(0, 0, blur_out.width, blur_out.height);
  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);

  blur_program.Use();

  // Bind SSAO texture to binding 0
  glBindTextureUnit(0, ssao_in.texture);

  glBindVertexArray(GetFullscreenQuadVAO());
  glDrawArrays(GL_TRIANGLES, 0, 6);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

}  // namespace sh_renderer
