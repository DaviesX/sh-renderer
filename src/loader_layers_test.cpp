#include <gtest/gtest.h>
#include <tiny_gltf.h>

#include <string>
#include <vector>

#include "loader.h"
#include "scene.h"

namespace sh_renderer {
namespace {

tinygltf::Value Str(const char* s) { return tinygltf::Value(std::string(s)); }

// Adds a 1x1 image + texture to the model and returns the texture index.
int AddTexture(tinygltf::Model* m, int component, uint8_t alpha = 255) {
  tinygltf::Image img;
  img.width = 1;
  img.height = 1;
  img.component = component;
  img.image = component == 4 ? std::vector<uint8_t>{200, 100, 50, alpha}
                             : std::vector<uint8_t>{200, 100, 50};
  m->images.push_back(img);
  tinygltf::Texture tex;
  tex.source = static_cast<int>(m->images.size() - 1);
  m->textures.push_back(tex);
  return static_cast<int>(m->textures.size() - 1);
}

tinygltf::Value::Object MakeLayer(int tex, const char* src, const char* dst) {
  tinygltf::Value::Object lo;
  tinygltf::Value::Object texo;
  texo["index"] = tinygltf::Value(tex);
  lo["texture"] = tinygltf::Value(texo);
  lo["blendSrc"] = Str(src);
  lo["blendDst"] = Str(dst);
  tinygltf::Value::Object rgb;
  rgb["type"] = Str("IDENTITY");
  lo["rgbGen"] = tinygltf::Value(rgb);
  return lo;
}

TEST(LoaderLayers, ParsesStack) {
  tinygltf::Model model;
  int t_base = AddTexture(&model, 4, 128);  // layer 0: has alpha
  int t_l1 = AddTexture(&model, 3);         // layer 1 (base)
  int t_frame = AddTexture(&model, 3);      // animMap frame 1

  // Layer 0: SCALE + SCROLL tcMods and an animMap.
  tinygltf::Value::Object l0 = MakeLayer(t_base, "ONE", "ZERO");
  tinygltf::Value::Object scale;
  scale["type"] = Str("SCALE");
  scale["value"] = tinygltf::Value(
      tinygltf::Value::Array{tinygltf::Value(4.0), tinygltf::Value(2.0)});
  tinygltf::Value::Object scroll;
  scroll["type"] = Str("SCROLL");
  scroll["value"] = tinygltf::Value(
      tinygltf::Value::Array{tinygltf::Value(0.5), tinygltf::Value(0.0)});
  l0["tcMod"] = tinygltf::Value(tinygltf::Value::Array{tinygltf::Value(scale),
                                                       tinygltf::Value(scroll)});
  l0["animFreq"] = tinygltf::Value(10.0);
  l0["animFrames"] = tinygltf::Value(
      tinygltf::Value::Array{tinygltf::Value(t_base), tinygltf::Value(t_frame)});

  // Layer 1: a WAVE rgbGen.
  tinygltf::Value::Object l1 =
      MakeLayer(t_l1, "SRC_ALPHA", "ONE_MINUS_SRC_ALPHA");
  tinygltf::Value::Object wave;
  wave["type"] = Str("WAVE");
  wave["func"] = Str("SAWTOOTH");
  wave["base"] = tinygltf::Value(0.0);
  wave["amplitude"] = tinygltf::Value(1.0);
  wave["phase"] = tinygltf::Value(0.25);
  wave["frequency"] = tinygltf::Value(2.0);
  l1["rgbGen"] = tinygltf::Value(wave);

  tinygltf::Value::Object ext;
  ext["surfaceBlend"] = Str("OPAQUE");
  ext["cullMode"] = Str("NONE");
  ext["baseLayer"] = tinygltf::Value(1);
  ext["layers"] = tinygltf::Value(
      tinygltf::Value::Array{tinygltf::Value(l0), tinygltf::Value(l1)});

  tinygltf::Material gmat;
  gmat.extensions["SH_material_layers"] = tinygltf::Value(ext);

  Material mat;
  ParseMaterialLayers(model, gmat, ".", &mat);

  ASSERT_EQ(mat.layers.size(), 2u);
  EXPECT_EQ(mat.base_layer, 1);
  EXPECT_EQ(mat.cull_mode, CullMode::kNone);
  EXPECT_FALSE(mat.layers[0].is_base);
  EXPECT_TRUE(mat.layers[1].is_base);

  const Layer& a = mat.layers[0];
  EXPECT_EQ(a.blend_src, BlendFactor::kOne);
  EXPECT_EQ(a.blend_dst, BlendFactor::kZero);
  EXPECT_EQ(a.texture.channels, 4u);  // base alpha preserved (has a transparent texel)
  ASSERT_EQ(a.tcmods.size(), 2u);
  EXPECT_EQ(a.tcmods[0].type, TcModType::kScale);
  ASSERT_EQ(a.tcmods[0].values.size(), 2u);
  EXPECT_FLOAT_EQ(a.tcmods[0].values[0], 4.0f);
  EXPECT_FLOAT_EQ(a.tcmods[0].values[1], 2.0f);
  EXPECT_EQ(a.tcmods[1].type, TcModType::kScroll);
  EXPECT_FLOAT_EQ(a.tcmods[1].values[0], 0.5f);
  EXPECT_FLOAT_EQ(a.anim_freq, 10.0f);
  ASSERT_EQ(a.anim_frames.size(), 2u);

  const Layer& b = mat.layers[1];
  EXPECT_EQ(b.blend_src, BlendFactor::kSrcAlpha);
  EXPECT_EQ(b.blend_dst, BlendFactor::kOneMinusSrcAlpha);
  EXPECT_EQ(b.rgbgen.type, RgbGenType::kWave);
  EXPECT_EQ(b.rgbgen.wave, WaveType::kSawtooth);
  EXPECT_FLOAT_EQ(b.rgbgen.phase, 0.25f);
  EXPECT_FLOAT_EQ(b.rgbgen.frequency, 2.0f);
}

TEST(LoaderLayers, NoExtensionLeavesLayersEmpty) {
  tinygltf::Model model;
  tinygltf::Material gmat;
  Material mat;
  ParseMaterialLayers(model, gmat, ".", &mat);
  EXPECT_TRUE(mat.layers.empty());
}

TEST(LoaderLayers, NormalizesShortTcModValues) {
  tinygltf::Model model;
  int t0 = AddTexture(&model, 3);
  tinygltf::Value::Object lo = MakeLayer(t0, "ONE", "ZERO");
  tinygltf::Value::Object scale;
  scale["type"] = Str("SCALE");
  scale["value"] =
      tinygltf::Value(tinygltf::Value::Array{tinygltf::Value(4.0)});  // only 1
  lo["tcMod"] = tinygltf::Value(tinygltf::Value::Array{tinygltf::Value(scale)});
  tinygltf::Value::Object ext;
  ext["layers"] = tinygltf::Value(tinygltf::Value::Array{tinygltf::Value(lo)});
  tinygltf::Material gmat;
  gmat.extensions["SH_material_layers"] = tinygltf::Value(ext);

  Material mat;
  ParseMaterialLayers(model, gmat, ".", &mat);
  ASSERT_EQ(mat.layers.size(), 1u);
  ASSERT_EQ(mat.layers[0].tcmods.size(), 1u);
  ASSERT_EQ(mat.layers[0].tcmods[0].values.size(), 2u);  // padded to [s, t]
  EXPECT_FLOAT_EQ(mat.layers[0].tcmods[0].values[0], 4.0f);
  EXPECT_FLOAT_EQ(mat.layers[0].tcmods[0].values[1], 1.0f);  // identity scale
}

TEST(LoaderLayers, OutOfRangeBaseLayerClampsToZero) {
  tinygltf::Model model;
  int t0 = AddTexture(&model, 3);
  tinygltf::Value::Object ext;
  ext["baseLayer"] = tinygltf::Value(9);  // out of range
  ext["layers"] =
      tinygltf::Value(tinygltf::Value::Array{tinygltf::Value(
          MakeLayer(t0, "ONE", "ZERO"))});
  tinygltf::Material gmat;
  gmat.extensions["SH_material_layers"] = tinygltf::Value(ext);

  Material mat;
  ParseMaterialLayers(model, gmat, ".", &mat);
  ASSERT_EQ(mat.layers.size(), 1u);
  EXPECT_EQ(mat.base_layer, 0);
  EXPECT_TRUE(mat.layers[0].is_base);
}

}  // namespace
}  // namespace sh_renderer
