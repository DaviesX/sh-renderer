#include "draw_depth.h"

#include <glog/logging.h>

#include "glad.h"

namespace sh_renderer {

namespace {

// Fullscreen quad geometry
const float kQuadVertices[] = {
    // positions        // uvs
    -1.0f, 1.0f, 0.0f, 0.0f,  1.0f, -1.0f, -1.0f, 0.0f,
    0.0f,  0.0f, 1.0f, -1.0f, 0.0f, 1.0f,  0.0f,

    -1.0f, 1.0f, 0.0f, 0.0f,  1.0f, 1.0f,  -1.0f, 0.0f,
    1.0f,  0.0f, 1.0f, 1.0f,  0.0f, 1.0f,  1.0f};

GLuint GetQuadVAO() {
  static GLuint vao = 0;
  if (vao == 0) {
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

}  // namespace

ShaderProgram CreateDepthOpaqueProgram() {
  auto program = ShaderProgram::CreateGraphics("glsl/depth_opaque.vert",
                                               "glsl/depth_opaque.frag");
  if (!program) {
    LOG(ERROR) << "Failed to create depth shader program.";
    return {};
  }
  return std::move(*program);
}

ShaderProgram CreateDepthCutoutProgram() {
  auto program = ShaderProgram::CreateGraphics("glsl/depth_cutout.vert",
                                               "glsl/depth_cutout.frag");
  if (!program) {
    LOG(ERROR) << "Failed to create depth shader program.";
    return {};
  }
  return std::move(*program);
}

ShaderProgram CreateDepthOpaqueWNormalProgram() {
  auto program = ShaderProgram::CreateGraphics(
      "glsl/depth_opaque_w_normal.vert", "glsl/depth_opaque_w_normal.frag");
  if (!program) {
    LOG(ERROR) << "Failed to create depth shader program.";
    return {};
  }
  return std::move(*program);
}

ShaderProgram CreateDepthCutoutWNormalProgram() {
  auto program = ShaderProgram::CreateGraphics(
      "glsl/depth_cutout_w_normal.vert", "glsl/depth_cutout_w_normal.frag");
  if (!program) {
    LOG(ERROR) << "Failed to create depth shader program.";
    return {};
  }
  return std::move(*program);
}

ShaderProgram CreateDepthVisualizerProgram() {
  auto program = ShaderProgram::CreateGraphics("glsl/fullscreen.vert",
                                               "glsl/depth_vis.frag");
  if (!program) {
    LOG(ERROR) << "Failed to create depth visualizer program.";
    return {};
  }
  return std::move(*program);
}

void DrawDepth(const Scene& scene, const Camera& camera,
               const ShaderProgram& opaque_program,
               const ShaderProgram& cutout_program,
               const RenderTarget& target) {
  if (!opaque_program || !cutout_program) return;

  // Bind FBO
  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
  glViewport(0, 0, target.width, target.height);

  // Clear Depth
  glClear(GL_DEPTH_BUFFER_BIT);

  // Disable Color Write
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  // Enable Depth Test
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  std::vector<const Geometry*> opaque_geos;
  std::vector<const Geometry*> cutout_geos;
  opaque_geos.reserve(scene.geometries.size());
  cutout_geos.reserve(scene.geometries.size());

  for (const auto& geo : scene.geometries) {
    if (geo.vao == 0) continue;
    if (geo.material_id >= 0 &&
        static_cast<size_t>(geo.material_id) < scene.materials.size()) {
      const auto& mat = scene.materials[geo.material_id];
      if (mat.alpha_cutout) {
        cutout_geos.push_back(&geo);
      } else {
        opaque_geos.push_back(&geo);
      }
    } else {
      opaque_geos.push_back(&geo);
    }
  }

  // Draw the opaque geometries.
  opaque_program.Use();
  opaque_program.Uniform("u_view_proj", GetViewProjMatrix(camera));

  for (const Geometry* geo_ptr : opaque_geos) {
    const Geometry& geo = *geo_ptr;
    opaque_program.Uniform("u_model", geo.transform.matrix());

    glBindVertexArray(geo.vao);
    if (geo.index_count > 0) {
      glDrawElements(GL_TRIANGLES, geo.index_count, GL_UNSIGNED_INT, nullptr);
    } else {
      glDrawArrays(GL_TRIANGLES, 0, geo.vertices.size());
    }
  }

  // Draw the cutout geometries.
  cutout_program.Use();
  cutout_program.Uniform("u_view_proj", GetViewProjMatrix(camera));

  for (const Geometry* geo_ptr : cutout_geos) {
    const Geometry& geo = *geo_ptr;
    cutout_program.Uniform("u_model", geo.transform.matrix());

    // For cutout transparency, we need to bind the albedo texture.
    if (geo.material_id >= 0 &&
        static_cast<size_t>(geo.material_id) < scene.materials.size()) {
      const auto& mat = scene.materials[geo.material_id];
      glBindTextureUnit(0, mat.albedo.texture_id);
    } else {
      glBindTextureUnit(0, 0);
    }

    glBindVertexArray(geo.vao);
    if (geo.index_count > 0) {
      glDrawElements(GL_TRIANGLES, geo.index_count, GL_UNSIGNED_INT, nullptr);
    } else {
      glDrawArrays(GL_TRIANGLES, 0, geo.vertices.size());
    }
  }

  // Restore State
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthFunc(GL_LEQUAL);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DrawDepthWNormal(const Scene& scene, const Camera& camera,
                      const ShaderProgram& opaque_program,
                      const ShaderProgram& cutout_program,
                      const RenderTarget& target) {
  if (!opaque_program || !cutout_program) return;

  // Bind FBO
  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
  glViewport(0, 0, target.width, target.height);

  // Clear Depth
  glClear(
      GL_DEPTH_BUFFER_BIT);  // Ensure we also clear normal buffer if needed?
                             // Yes, but glClear(GL_DEPTH_BUFFER_BIT) only
                             // clears depth.
  // We can clear normal buffer by clearing color attachment 0, or just let
  // depth testing overwrite them appropriately since we don't blend. However,
  // skybox pixels might not be written to. The normal buffer's alpha channel
  // serves as skybox flag. So we must clear attachment 0 to alpha=0.
  const float clear_normal[] = {0.0f, 0.0f, 0.0f, 0.0f};
  glClearNamedFramebufferfv(target.fbo, GL_COLOR, 0, clear_normal);

  // Enable Color Write on Attachment 0 (Normal texture)
  glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  // Enable Depth Test
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  std::vector<const Geometry*> opaque_geos;
  std::vector<const Geometry*> cutout_geos;
  opaque_geos.reserve(scene.geometries.size());
  cutout_geos.reserve(scene.geometries.size());

  for (const auto& geo : scene.geometries) {
    if (geo.vao == 0) continue;
    if (geo.material_id >= 0 &&
        static_cast<size_t>(geo.material_id) < scene.materials.size()) {
      const auto& mat = scene.materials[geo.material_id];
      if (mat.alpha_cutout) {
        cutout_geos.push_back(&geo);
      } else {
        opaque_geos.push_back(&geo);
      }
    } else {
      opaque_geos.push_back(&geo);
    }
  }

  Eigen::Matrix4f view = GetViewMatrix(camera);

  // Draw the opaque geometries.
  opaque_program.Use();
  opaque_program.Uniform("u_view_proj", GetViewProjMatrix(camera));
  opaque_program.Uniform("u_view", view);

  for (const Geometry* geo_ptr : opaque_geos) {
    const Geometry& geo = *geo_ptr;
    opaque_program.Uniform("u_model", geo.transform.matrix());

    glBindVertexArray(geo.vao);
    if (geo.index_count > 0) {
      glDrawElements(GL_TRIANGLES, geo.index_count, GL_UNSIGNED_INT, nullptr);
    } else {
      glDrawArrays(GL_TRIANGLES, 0, geo.vertices.size());
    }
  }

  // Draw the cutout geometries.
  cutout_program.Use();
  cutout_program.Uniform("u_view_proj", GetViewProjMatrix(camera));
  cutout_program.Uniform("u_view", view);

  for (const Geometry* geo_ptr : cutout_geos) {
    const Geometry& geo = *geo_ptr;
    cutout_program.Uniform("u_model", geo.transform.matrix());

    // For cutout transparency, we need to bind the albedo texture.
    if (geo.material_id >= 0 &&
        static_cast<size_t>(geo.material_id) < scene.materials.size()) {
      const auto& mat = scene.materials[geo.material_id];
      glBindTextureUnit(0, mat.albedo.texture_id);
    } else {
      glBindTextureUnit(0, 0);
    }

    glBindVertexArray(geo.vao);
    if (geo.index_count > 0) {
      glDrawElements(GL_TRIANGLES, geo.index_count, GL_UNSIGNED_INT, nullptr);
    } else {
      glDrawArrays(GL_TRIANGLES, 0, geo.vertices.size());
    }
  }

  // Restore State
  glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE,
               GL_TRUE);  // restore mask for buffer 0
  glDepthFunc(GL_LEQUAL);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DrawDepthVisualization(const RenderTarget& depth, const Camera& camera,
                            const ShaderProgram& program,
                            const RenderTarget& out) {
  if (!program) return;

  // Bind Output FBO (or default)
  glBindFramebuffer(GL_FRAMEBUFFER, out.fbo);

  // Set Viewport based on target
  if (out.fbo != 0) {
    glViewport(0, 0, out.width, out.height);
  } else {
    // If out is screen, assuming main window viewport is already set or we need
    // to query it? Safer to get current viewport if drawing to default
    // framebuffer to avoid messing it up? But usually we want to fill the
    // screen. Let's assume the caller sets viewport for screen, OR we query it.
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    // If viewport is 0,0,width,height, we use it.
    // But DrawDepth sets viewport.
    // So we MUST set viewport here.
    // If target is 0, we don't know the window size here easily without passing
    // it. Use glGetIntegerv(GL_LAYOUT_X...)? No. We can use the depth target
    // size as a guess or assume full window. Let's assume 'depth' target size
    // matches window size for now.
    glViewport(0, 0, depth.width, depth.height);
  }

  // Clear Color (and Depth if we need to, though we are drawing a full screen
  // quad with no depth test) Actually we shouldn't clear unless we mean to.
  // Just disable depth test.
  glDisable(GL_DEPTH_TEST);

  program.Use();

  // Bind Depth Texture
  glBindTextureUnit(0, depth.depth_buffer);
  program.Uniform("u_depth", 0);
  program.Uniform("u_z_near", camera.intrinsics.z_near);
  program.Uniform("u_z_far", camera.intrinsics.z_far);

  glBindVertexArray(GetQuadVAO());
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

}  // namespace sh_renderer