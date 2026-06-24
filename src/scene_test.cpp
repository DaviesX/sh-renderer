#include "scene.h"

#include <gtest/gtest.h>

namespace sh_renderer {

TEST(SceneTest, TransformedVertices) {
  Geometry geo;
  geo.vertices = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};

  // Translate by (1, 2, 3)
  geo.transform = Eigen::Affine3f::Identity();
  geo.transform.translate(Eigen::Vector3f(1.0f, 2.0f, 3.0f));

  auto transformed = TransformedVertices(geo);
  ASSERT_EQ(transformed.size(), 2);

  // (1,0,0) + (1,2,3) = (2,2,3)
  EXPECT_TRUE(transformed[0].isApprox(Eigen::Vector3f(2.0f, 2.0f, 3.0f)));
  // (0,1,0) + (1,2,3) = (1,3,3)
  EXPECT_TRUE(transformed[1].isApprox(Eigen::Vector3f(1.0f, 3.0f, 3.0f)));
}

TEST(SceneTest, TransformedNormals) {
  Geometry geo;
  geo.normals = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};

  // Rotate 90 degrees around Z axis.
  // X axis (1,0,0) becomes Y axis (0,1,0)
  // Y axis (0,1,0) becomes -X axis (-1,0,0)
  geo.transform = Eigen::Affine3f::Identity();
  geo.transform.rotate(
      Eigen::AngleAxisf(M_PI / 2.0f, Eigen::Vector3f::UnitZ()));

  auto transformed = TransformedNormals(geo);
  ASSERT_EQ(transformed.size(), 2);

  EXPECT_TRUE(transformed[0].isApprox(Eigen::Vector3f(0.0f, 1.0f, 0.0f)));
  EXPECT_TRUE(transformed[1].isApprox(Eigen::Vector3f(-1.0f, 0.0f, 0.0f)));
}

TEST(SceneTest, TransformedTangents) {
  Geometry geo;
  // Tangent pointing X, with sign 1.0
  geo.tangents = {Eigen::Vector4f(1.0f, 0.0f, 0.0f, 1.0f)};

  // Rotate 90 degrees around Z axis.
  geo.transform = Eigen::Affine3f::Identity();
  geo.transform.rotate(
      Eigen::AngleAxisf(M_PI / 2.0f, Eigen::Vector3f::UnitZ()));

  auto transformed = TransformedTangents(geo);
  ASSERT_EQ(transformed.size(), 1);

  // Should rotate to Y (0,1,0), sign preserved
  EXPECT_TRUE(
      transformed[0].head<3>().isApprox(Eigen::Vector3f(0.0f, 1.0f, 0.0f)));
  EXPECT_EQ(transformed[0].w(), 1.0f);
}

TEST(SceneTest, PartitionLooseGeometries_SplitsDisconnected) {
  Scene scene;
  Geometry geo;
  // Two triangles far apart
  geo.vertices = {{0.0f, 0.0f, 0.0f},  {1.0f, 0.0f, 0.0f},
                  {0.0f, 1.0f, 0.0f},  {10.0f, 0.0f, 0.0f},
                  {11.0f, 0.0f, 0.0f}, {10.0f, 1.0f, 0.0f}};
  geo.indices = {0, 1, 2, 3, 4, 5};
  scene.geometries.push_back(std::move(geo));

  PartitionLooseGeometries(scene);

  // Should split into two geometries
  EXPECT_EQ(scene.geometries.size(), 2);

  if (scene.geometries.size() == 2) {
    EXPECT_EQ(scene.geometries[0].vertices.size(), 3);
    EXPECT_EQ(scene.geometries[1].vertices.size(), 3);
    EXPECT_EQ(scene.geometries[0].indices.size(), 3);
    EXPECT_EQ(scene.geometries[1].indices.size(), 3);
  }
}

TEST(SceneTest, PartitionLooseGeometries_MergesClose) {
  Scene scene;
  Geometry geo;
  // Two triangles close together (centers within 0.1m)
  geo.vertices = {{0.0f, 0.0f, 0.0f},  {1.0f, 0.0f, 0.0f},
                  {0.0f, 1.0f, 0.0f},  {0.05f, 0.0f, 0.0f},
                  {1.05f, 0.0f, 0.0f}, {0.05f, 1.0f, 0.0f}};
  geo.indices = {0, 1, 2, 3, 4, 5};
  scene.geometries.push_back(std::move(geo));

  PartitionLooseGeometries(scene);

  // Should remain one geometry
  EXPECT_EQ(scene.geometries.size(), 1);
  if (scene.geometries.size() == 1) {
    EXPECT_EQ(scene.geometries[0].vertices.size(), 6);
    EXPECT_EQ(scene.geometries[0].indices.size(), 6);
  }
}

TEST(SceneTest, PartitionLooseGeometries_HandlesNoIndices) {
  Scene scene;
  Geometry geo;
  // Triangles far apart, no indices
  geo.vertices = {{0.0f, 0.0f, 0.0f},  {1.0f, 0.0f, 0.0f},
                  {0.0f, 1.0f, 0.0f},  {10.0f, 0.0f, 0.0f},
                  {11.0f, 0.0f, 0.0f}, {10.0f, 1.0f, 0.0f}};
  scene.geometries.push_back(std::move(geo));

  PartitionLooseGeometries(scene);

  EXPECT_EQ(scene.geometries.size(), 2);
}

TEST(SceneTest, BuildLayerBuffersPacksStack) {
  std::vector<Material> materials(2);

  // Material 0: plain PBR (no layers).
  materials[0].name = "plain";

  // Material 1: two layers; layer 0 has two tcMods, layer 1 a wave rgbGen.
  materials[1].name = "layered";
  materials[1].base_layer = 1;
  Layer l0;
  l0.blend_src = BlendFactor::kOne;
  l0.blend_dst = BlendFactor::kZero;
  l0.tcmods.push_back(TcMod{TcModType::kScale, {4.0f, 2.0f}, WaveType::kSine});
  l0.tcmods.push_back(TcMod{TcModType::kScroll, {0.5f, 0.0f}, WaveType::kSine});
  Layer l1;
  l1.blend_src = BlendFactor::kSrcAlpha;
  l1.blend_dst = BlendFactor::kOneMinusSrcAlpha;
  l1.is_base = true;
  l1.rgbgen.type = RgbGenType::kWave;
  l1.rgbgen.wave = WaveType::kSawtooth;
  l1.rgbgen.phase = 0.25f;
  l1.rgbgen.frequency = 2.0f;
  materials[1].layers = {l0, l1};

  std::vector<GpuMaterial> gpu_materials;
  std::vector<GpuMaterialLayer> gpu_layers;
  std::vector<GpuTcMod> gpu_tcmods;
  BuildLayerBuffers(materials, &gpu_materials, &gpu_layers, &gpu_tcmods);

  // One GpuMaterial per material; offsets are flat across materials.
  ASSERT_EQ(gpu_materials.size(), 2u);
  EXPECT_EQ(gpu_materials[0].layer_count, 0);
  EXPECT_EQ(gpu_materials[1].layer_count, 2);
  EXPECT_EQ(gpu_materials[1].layer_offset, 0);  // material 0 contributed none
  EXPECT_EQ(gpu_materials[1].base_layer, 1);

  ASSERT_EQ(gpu_layers.size(), 2u);
  EXPECT_EQ(gpu_layers[0].blend_src, static_cast<int>(BlendFactor::kOne));
  EXPECT_EQ(gpu_layers[0].tcmod_offset, 0);
  EXPECT_EQ(gpu_layers[0].tcmod_count, 2);
  EXPECT_EQ(gpu_layers[1].tcmod_offset, 2);  // after layer 0's two tcMods
  EXPECT_EQ(gpu_layers[1].tcmod_count, 0);
  EXPECT_EQ(gpu_layers[1].rgbgen_type, static_cast<int>(RgbGenType::kWave));
  EXPECT_EQ(gpu_layers[1].rgbgen_wave, static_cast<int>(WaveType::kSawtooth));
  EXPECT_FLOAT_EQ(gpu_layers[1].rgbgen_phase, 0.25f);

  ASSERT_EQ(gpu_tcmods.size(), 2u);
  EXPECT_EQ(gpu_tcmods[0].type, static_cast<int>(TcModType::kScale));
  EXPECT_FLOAT_EQ(gpu_tcmods[0].v[0], 4.0f);
  EXPECT_FLOAT_EQ(gpu_tcmods[0].v[1], 2.0f);
  EXPECT_EQ(gpu_tcmods[1].type, static_cast<int>(TcModType::kScroll));
  EXPECT_FLOAT_EQ(gpu_tcmods[1].v[0], 0.5f);
}

}  // namespace sh_renderer
