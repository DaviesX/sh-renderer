#ifndef SH_RENDERER_SRC_Q3_LAYER_H_
#define SH_RENDERER_SRC_Q3_LAYER_H_

#include <vector>

// CPU-side description of one Quake 3 shader stage from the `SH_material_layers`
// glTF extension. The enums/structs mirror `sh-baker/src/layer_composite.h` so
// the renderer's GLSL compositor can reproduce the baker's result at t=0 (the
// shared `sh-scene` library is cancelled; parity is by mirroring). `Layer` itself
// lives in scene.h because it holds a `Texture`.
namespace sh_renderer {

// GL blend factors (subset Quake 3 uses), matching the exporter's emitted names.
enum class BlendFactor {
  kZero,
  kOne,
  kSrcColor,
  kOneMinusSrcColor,
  kDstColor,
  kOneMinusDstColor,
  kSrcAlpha,
  kOneMinusSrcAlpha,
  kDstAlpha,
  kOneMinusDstAlpha,
};

enum class RgbGenType {
  kIdentity,
  kIdentityLighting,
  kVertex,
  kExactVertex,
  kWave,
};

enum class WaveType {
  kSine,
  kTriangle,
  kSquare,
  kSawtooth,
  kInverseSawtooth,
};

struct RgbGen {
  RgbGenType type = RgbGenType::kIdentity;
  WaveType wave = WaveType::kSine;
  float base = 0.0f;
  float amplitude = 0.0f;
  float phase = 0.0f;
  float frequency = 0.0f;
};

enum class TcModType {
  kNoOp,
  kScale,
  kScroll,
  kRotate,
  kTurb,
  kStretch,
  kTransform,
};

struct TcMod {
  TcModType type = TcModType::kNoOp;
  // Numeric params, by type: SCALE [s,t]; SCROLL [s_rate,t_rate]; ROTATE [deg/s];
  // TRANSFORM [m00,m01,m02,m10,m11,m12]; TURB/STRETCH [base,amp,phase,freq].
  // Unlike the baker (which freezes time-varying types), the renderer animates
  // them by t.
  std::vector<float> values;
  WaveType wave = WaveType::kSine;  // TURB / STRETCH only
};

// Per-material face culling, from the extension's `cullMode`.
enum class CullMode {
  kFront,  // default; cull back faces (GL_BACK)
  kBack,   // cull front faces
  kNone,   // two-sided (grates / fences / foliage)
};

}  // namespace sh_renderer

#endif  // SH_RENDERER_SRC_Q3_LAYER_H_
