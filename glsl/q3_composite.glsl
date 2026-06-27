// Quake 3 shader-stage compositor for the SH_material_layers extension.
//
// Mirrors sh-baker/src/layer_composite.cpp so the real-time result matches the
// baked indirect light at t=0 (the shared sh-scene lib is cancelled; parity is
// by mirroring). Unlike the baker, which freezes everything at t=0, this animates
// tcMod scroll/rotate and rgbGen wave by `u_time` (both are identity / equal to
// the baker at t=0). turb/stretch stay frozen, matching the baker.
//
// Requires the includer to declare `u_albedo_texture` at binding 0 (the modern
// base albedo) and the macro MAX_LAYERS. std430 layouts mirror scene.h; each
// descriptor SSBO is `uint count; T items[];` with the array at offset 4.

struct GpuMaterial {
  int layer_offset;
  int layer_count;
  int base_layer;
  int modern_has_alpha;
};

struct GpuMaterialLayer {
  int blend_src;
  int blend_dst;
  int rgbgen_type;
  int rgbgen_wave;
  float rgbgen_base;
  float rgbgen_amplitude;
  float rgbgen_phase;
  float rgbgen_frequency;
  int tcmod_offset;
  int tcmod_count;
  int pad0;
  int pad1;
};

struct GpuTcMod {
  int type;
  int wave;
  float v[6];
};

layout(std430, binding = 3) readonly buffer MaterialRangeBuffer {
  uint material_count;
  GpuMaterial gpu_materials[];
};
layout(std430, binding = 4) readonly buffer MaterialLayerBuffer {
  uint layer_total;
  GpuMaterialLayer gpu_layers[];
};
layout(std430, binding = 5) readonly buffer MaterialTcModBuffer {
  uint tcmod_total;
  GpuTcMod gpu_tcmods[];
};

// One sampler per layer; the CPU binds each material's layers in order (animMap
// frame already selected) and the base layer's own Q3 texture (for coverage).
layout(binding = 16) uniform sampler2D u_layers[MAX_LAYERS];
uniform float u_time;
uniform int u_material_index;

// --- enum constants (match q3_layer.h / layer_composite.h) ---
#define Q3_BF_ZERO 0
#define Q3_BF_ONE 1
#define Q3_BF_SRC_COLOR 2
#define Q3_BF_ONE_MINUS_SRC_COLOR 3
#define Q3_BF_DST_COLOR 4
#define Q3_BF_ONE_MINUS_DST_COLOR 5
#define Q3_BF_SRC_ALPHA 6
#define Q3_BF_ONE_MINUS_SRC_ALPHA 7
#define Q3_BF_DST_ALPHA 8
#define Q3_BF_ONE_MINUS_DST_ALPHA 9

#define Q3_RGBGEN_WAVE 4

#define Q3_WAVE_SINE 0
#define Q3_WAVE_TRIANGLE 1
#define Q3_WAVE_SQUARE 2
#define Q3_WAVE_SAWTOOTH 3
#define Q3_WAVE_INVERSE_SAWTOOTH 4

#define Q3_TCMOD_SCALE 1
#define Q3_TCMOD_SCROLL 2
#define Q3_TCMOD_ROTATE 3
#define Q3_TCMOD_TRANSFORM 6

float q3WaveValue(int type, float x) {
  float f = fract(x);
  if (type == Q3_WAVE_SQUARE) return f < 0.5 ? 1.0 : -1.0;
  if (type == Q3_WAVE_TRIANGLE) return f < 0.5 ? (-1.0 + 4.0 * f) : (3.0 - 4.0 * f);
  if (type == Q3_WAVE_SAWTOOTH) return f;
  if (type == Q3_WAVE_INVERSE_SAWTOOTH) return 1.0 - f;
  return sin(f * 6.28318530718);  // Q3_WAVE_SINE
}

vec3 q3RgbGen(GpuMaterialLayer layer, float t) {
  if (layer.rgbgen_type == Q3_RGBGEN_WAVE) {
    float s = layer.rgbgen_base +
              layer.rgbgen_amplitude *
                  q3WaveValue(layer.rgbgen_wave,
                              layer.rgbgen_phase + layer.rgbgen_frequency * t);
    return vec3(clamp(s, 0.0, 1.0));
  }
  // identity / identityLighting -> 1; vertex / exactVertex -> 1 (deferred).
  return vec3(1.0);
}

vec2 q3ApplyTcMods(GpuMaterialLayer layer, vec2 uv, float t) {
  for (int i = 0; i < layer.tcmod_count; ++i) {
    GpuTcMod m = gpu_tcmods[layer.tcmod_offset + i];
    if (m.type == Q3_TCMOD_SCALE) {
      uv = vec2(uv.x * m.v[0], uv.y * m.v[1]);
    } else if (m.type == Q3_TCMOD_SCROLL) {
      uv += vec2(m.v[0], m.v[1]) * t;
    } else if (m.type == Q3_TCMOD_ROTATE) {
      float a = radians(m.v[0] * t);
      float c = cos(a), s = sin(a);
      vec2 p = uv - vec2(0.5);
      uv = vec2(c * p.x - s * p.y, s * p.x + c * p.y) + vec2(0.5);
    } else if (m.type == Q3_TCMOD_TRANSFORM) {
      uv = vec2(m.v[0] * uv.x + m.v[1] * uv.y + m.v[2],
                m.v[3] * uv.x + m.v[4] * uv.y + m.v[5]);
    }
    // TURB / STRETCH / NOOP: identity (matches the baker's t=0 freeze).
  }
  return uv;
}

vec3 q3BlendWeight(int factor, vec3 src_rgb, float src_a, vec3 dst_rgb,
                   float dst_a) {
  if (factor == Q3_BF_ZERO) return vec3(0.0);
  if (factor == Q3_BF_ONE) return vec3(1.0);
  if (factor == Q3_BF_SRC_COLOR) return src_rgb;
  if (factor == Q3_BF_ONE_MINUS_SRC_COLOR) return vec3(1.0) - src_rgb;
  if (factor == Q3_BF_DST_COLOR) return dst_rgb;
  if (factor == Q3_BF_ONE_MINUS_DST_COLOR) return vec3(1.0) - dst_rgb;
  if (factor == Q3_BF_SRC_ALPHA) return vec3(src_a);
  if (factor == Q3_BF_ONE_MINUS_SRC_ALPHA) return vec3(1.0 - src_a);
  if (factor == Q3_BF_DST_ALPHA) return vec3(dst_a);
  if (factor == Q3_BF_ONE_MINUS_DST_ALPHA) return vec3(1.0 - dst_a);
  return vec3(1.0);
}

// Composites material `material_index`'s layer stack at TEXCOORD_0 `uv0` and
// time `t`, returning sRGB-space colour + binary-ish coverage in alpha. Plain
// PBR materials (no layers) just return the modern albedo.
vec4 q3Composite(int material_index, vec2 uv0, float t) {
  if (material_index < 0 || material_index >= int(material_count)) {
    return texture(u_albedo_texture, uv0);
  }
  GpuMaterial mat = gpu_materials[material_index];
  if (mat.layer_count == 0) {
    return texture(u_albedo_texture, uv0);
  }

  int n = min(mat.layer_count, MAX_LAYERS);
  vec3 acc = vec3(0.0);
  float acc_alpha = 1.0;
  float coverage = 1.0;
  for (int j = 0; j < n; ++j) {
    GpuMaterialLayer layer = gpu_layers[mat.layer_offset + j];
    vec2 luv = q3ApplyTcMods(layer, uv0, t);

    // Base layer colour is the modern albedo; other layers use their sampler.
    vec4 tex = (j == mat.base_layer) ? texture(u_albedo_texture, luv)
                                     : texture(u_layers[j], luv);
    vec3 cl = tex.rgb * q3RgbGen(layer, t);
    vec3 sf = q3BlendWeight(layer.blend_src, cl, tex.a, acc, acc_alpha);
    vec3 df = q3BlendWeight(layer.blend_dst, cl, tex.a, acc, acc_alpha);
    acc = sf * cl + df * acc;

    if (j == mat.base_layer) {
      float q3_alpha = texture(u_layers[j], luv).a;  // base layer's Q3 alpha
      coverage = (mat.modern_has_alpha != 0) ? tex.a : q3_alpha;
    }
  }
  return vec4(clamp(acc, 0.0, 1.0), coverage);
}
