#pragma once

#include "scene.h"
#include "shader.h"

namespace sh_renderer {

// Layer sampler array base unit; must match `layout(binding = 16)` for
// u_layers[] in q3_composite.glsl.
constexpr int kLayerSamplerBase = 16;

// Binds the SH_material_layers descriptor SSBOs (bindings 3/4/5) and the
// animation clock so the q3_composite.glsl compositor can run in `program`.
// Call once per pass, after `program.Use()`.
void BindLayerCompositor(const Scene& scene, const ShaderProgram& program,
                         float time);

// Binds a layered material's stage textures to u_layers[0..], selecting the
// active animMap frame on the CPU. Unused slots get the material's modern
// albedo so the sampler array stays complete. Call per layered material.
void BindMaterialLayers(const Material& mat, float time);

}  // namespace sh_renderer
