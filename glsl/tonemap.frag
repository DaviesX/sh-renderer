#version 460 core

layout(location = 0) out vec4 out_color;

in vec2 v_uv;

layout(binding = 0) uniform sampler2D u_hdr_texture;

// Display gamma g. For g != 1 the tonemapped colour is encoded to sRGB
// exactly (e), raised to 1/g and decoded back to linear, so the sRGB
// framebuffer stores e^(1/g). At g = 1 the branch is skipped.
uniform float u_display_gamma = 1.0;

vec3 LinearToSrgb(vec3 c) {
  return mix(12.92 * c, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055,
             step(vec3(0.0031308), c));
}

vec3 SrgbToLinear(vec3 e) {
  return mix(e / 12.92, pow((e + 0.055) / 1.055, vec3(2.4)),
             step(vec3(0.04045), e));
}

// --- Tonemapping Helper ---
const float A = 0.15;
const float B = 0.50;
const float C = 0.10;
const float D = 0.20;
const float E = 0.02;
const float F = 0.30;
const float W = 11.2;

const float exposureBias = 1.0f;

vec3 Uncharted2Tonemap(vec3 x) {
  return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

void main() {
  vec3 color = texture(u_hdr_texture, v_uv).rgb;

  const float exposure = 1.0f;
  color *= exposure;

  // Tone mapping
  vec3 curr = Uncharted2Tonemap(exposureBias * color);
  vec3 whiteScale = 1.0f / Uncharted2Tonemap(vec3(W));
  color = curr * whiteScale;

  // Gamma correction is handled by GL_FRAMEBUFFER_SRGB
  // color = pow(color, vec3(1.0 / 2.2));

  if (u_display_gamma != 1.0) {
    vec3 e = LinearToSrgb(clamp(color, 0.0, 1.0));
    color = SrgbToLinear(pow(e, vec3(1.0 / u_display_gamma)));
  }

  out_color = vec4(color, 1.0);
}
