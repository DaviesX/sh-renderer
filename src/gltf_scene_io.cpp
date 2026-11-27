#include "gltf_scene_io.h"

#include <tinyexr.h>

#include <cmath>
#include <cstdlib>
#include <string>

#include "log.h"

namespace sh_renderer {

float ComputeLightRadius(float intensity, const Eigen::Vector3f& color,
                         float threshold) {
  float flux = intensity * color.maxCoeff();
  if (flux <= 0.0f || threshold <= 0.0f) return 0.0f;
  return std::sqrt(flux / threshold);
}

void LoadLightmaps(Scene& scene, const std::filesystem::path& gltf_file) {
  std::filesystem::path base_path = gltf_file.parent_path();
  std::string file_names[3] = {"lightmap_packed_0.exr", "lightmap_packed_1.exr",
                               "lightmap_packed_2.exr"};

  for (int i = 0; i < 3; ++i) {
    std::string path = (base_path / file_names[i]).string();
    float* out_rgba = nullptr;
    int width;
    int height;
    const char* err = nullptr;

    int ret = LoadEXR(&out_rgba, &width, &height, path.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
      if (err) {
        LOG(ERROR) << "LoadEXR error: " << err << " for file: " << path;
        FreeEXRErrorMessage(err);
      } else {
        LOG(ERROR) << "Failed to load EXR: " << path;
      }

      // Fallback
      scene.lightmaps_packed[i].width = 1;
      scene.lightmaps_packed[i].height = 1;
      scene.lightmaps_packed[i].channels = 4;
      scene.lightmaps_packed[i].pixel_data.resize(4);
      if (i == 0) {
        scene.lightmaps_packed[i].pixel_data = {0.0f, 0.0f, 0.0f, 1.0f};
      } else {
        scene.lightmaps_packed[i].pixel_data = {0.0f, 0.0f, 0.0f, 0.0f};
      }
    } else {
      scene.lightmaps_packed[i].width = width;
      scene.lightmaps_packed[i].height = height;
      scene.lightmaps_packed[i].channels = 4;
      scene.lightmaps_packed[i].pixel_data.assign(
          out_rgba, out_rgba + width * height * 4);
      free(out_rgba);
    }
  }
}

}  // namespace sh_renderer
