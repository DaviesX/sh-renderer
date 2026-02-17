#include "draw_radiance.h"

#include <glog/logging.h>

#include "glad.h"
#include "shader.h"

namespace sh_renderer {

namespace {

// Simple Unlit Shader
const char* kUnlitVertex = R"(
#version 460 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) uniform mat4 u_view_proj;
layout(location = 1) uniform mat4 u_model;

out vec2 v_uv;
out vec3 v_normal;

void main() {
    v_uv = in_uv;
    v_normal = mat3(u_model) * in_normal; // Simplified normal transform
    gl_Position = u_view_proj * u_model * vec4(in_position, 1.0);
}
)";

const char* kUnlitFragment = R"(
#version 460 core

in vec2 v_uv;
in vec3 v_normal;

out vec4 out_color;

layout(binding = 0) uniform sampler2D u_albedo;

void main() {
    vec4 color = texture(u_albedo, v_uv);
    // Simple N dot L with "headlamp"
    vec3 N = normalize(v_normal);
    vec3 L = normalize(vec3(0.5, 1.0, 1.0)); // Fixed light for unlit viz
    float ndotl = max(dot(N, L), 0.2);
    out_color = vec4(color.rgb * ndotl, color.a);
}
)";

}  // namespace

ShaderProgram CreateUnlitProgram() {
  auto program = ShaderProgram::CreateFromSource(kUnlitVertex, kUnlitFragment);
  if (!program) {
    LOG(FATAL) << "Failed to create unlit shader program.";
    return {};
  }
  return std::move(*program);
}

void DrawSceneRadiance(const Scene& scene, const Eigen::Matrix4f& vp,
                       const ShaderProgram& program) {
  // TODO: Phase 3.
}

// Actual implementation of DrawSceneUnlit
void DrawSceneUnlit(const Scene& scene, const Eigen::Matrix4f& vp,
                    const ShaderProgram& program) {
  if (!program) return;

  program.Use();
  program.Uniform("u_view_proj", vp);

  for (const auto& geo : scene.geometries) {
    if (geo.vao == 0) continue;

    program.Uniform("u_model", geo.transform.matrix());

    // Bind Albedo
    if (geo.material_id >= 0 &&
        static_cast<size_t>(geo.material_id) < scene.materials.size()) {
      const auto& mat = scene.materials[geo.material_id];
      glBindTextureUnit(0, mat.albedo.texture_id);
    } else {
      glBindTextureUnit(0, 0);  // Invalid texture?
    }

    glBindVertexArray(geo.vao);
    if (geo.index_count > 0) {
      glDrawElements(GL_TRIANGLES, geo.index_count, GL_UNSIGNED_INT, nullptr);
    } else {
      glDrawArrays(GL_TRIANGLES, 0, geo.vertices.size());
    }
  }
}

}  // namespace sh_renderer