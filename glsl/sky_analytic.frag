#version 460 core

layout(location = 0) out vec4 out_color;

in vec2 v_uv;

uniform mat4 u_inv_view_proj;
uniform vec3 u_camera_pos;
uniform vec3 u_sun_direction;

uniform vec3 u_sky_color;
uniform float u_sun_intensity;

vec3 PreethamSky(vec3 view_dir, vec3 sun_dir) {
  float cos_theta = max(view_dir.y, 0.0);
  float cos_gamma = dot(view_dir, sun_dir);

  // Rayleigh
  vec3 rayleigh = u_sky_color;
  // Gradient based on zenith
  rayleigh *= (1.0 + 2.0 * cos_theta);

  // Mie (Sun halo)
  float mie = pow(max(0.0, cos_gamma), 100.0) * 0.5;

  // Sun disk
  float in_sun_disk = smoothstep(0.9995, 0.99975, cos_gamma);

  vec3 sky_ambient = (rayleigh * 0.5 + vec3(mie)) * (u_sun_intensity * 0.02);
  vec3 sun = vec3(in_sun_disk) * u_sun_intensity;
  return sky_ambient + sun;
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
