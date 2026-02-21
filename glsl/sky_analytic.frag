#version 460 core

layout(location = 0) out vec4 out_color;

in vec2 v_uv;

uniform mat4 u_inv_view_proj;
uniform vec3 u_camera_pos;
uniform vec3 u_sun_direction;

uniform vec3 u_sky_color;

vec3 PreethamSky(vec3 viewDir, vec3 sunDir) {
  float cosTheta = max(viewDir.y, 0.0);
  float cosGamma = dot(viewDir, sunDir);

  // Rayleigh
  vec3 rayleigh = u_sky_color;
  // Gradient based on zenith
  rayleigh *= (1.0 + 2.0 * cosTheta);

  // Mie (Sun halo)
  float mie = pow(max(0.0, cosGamma), 100.0) * 0.5;

  // Sun disk
  float sunUtils = step(0.9995, cosGamma);  // Small disk

  vec3 skyColor = rayleigh * 0.5 + vec3(mie) + vec3(sunUtils) * 20.0;
  return skyColor;
}

void main() {
  // Map uv to clip space coordinates (-1 to 1) for x and y
  vec4 clip_space = vec4(v_uv * 2.0 - 1.0, 1.0, 1.0);

  // Unproject to world space
  vec4 world_space = u_inv_view_proj * clip_space;
  vec3 world_pos = world_space.xyz / world_space.w;

  // View direction
  vec3 view_dir = normalize(world_pos - u_camera_pos);

  // Direction pointing towards the light source
  vec3 light_dir = normalize(-u_sun_direction);

  vec3 color = PreethamSky(view_dir, light_dir);
  out_color = vec4(color, 1.0);
}
