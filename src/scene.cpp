#include "scene.h"

#include <glog/logging.h>

#include <Eigen/Core>

namespace sh_baker {

std::vector<Eigen::Vector3f> TransformedVertices(const Geometry& geometry) {
  std::vector<Eigen::Vector3f> vertices;
  vertices.reserve(geometry.vertices.size());
  for (const auto& v : geometry.vertices) {
    vertices.push_back(geometry.transform * v);
  }
  return vertices;
}

std::vector<Eigen::Vector3f> TransformedNormals(const Geometry& geometry) {
  std::vector<Eigen::Vector3f> normals;
  normals.reserve(geometry.normals.size());
  for (const auto& n : geometry.normals) {
    normals.push_back((geometry.transform.rotation() * n).normalized());
  }
  return normals;
}

std::vector<Eigen::Vector4f> TransformedTangents(const Geometry& geometry) {
  std::vector<Eigen::Vector4f> tangents;
  tangents.reserve(geometry.tangents.size());
  for (const auto& t : geometry.tangents) {
    Eigen::Vector3f tangent_vector = t.head<3>();
    float tangent_sign = t.w();
    Eigen::Vector3f transformed_tangent_vector =
        geometry.transform.rotation() * tangent_vector;

    Eigen::Vector4f transformed_tangent;
    transformed_tangent.head<3>() = transformed_tangent_vector.normalized();
    transformed_tangent.w() = tangent_sign;
    tangents.push_back(transformed_tangent);
  }
  return tangents;
}

float SurfaceArea(const Geometry& geometry) {
  const auto& vertices = TransformedVertices(geometry);
  float total_area = 0.0f;
  for (size_t i = 0; i < geometry.indices.size(); i += 3) {
    Eigen::Vector3f v0 = vertices[geometry.indices[i]];
    Eigen::Vector3f v1 = vertices[geometry.indices[i + 1]];
    Eigen::Vector3f v2 = vertices[geometry.indices[i + 2]];
    float tri_area = 0.5f * (v1 - v0).cross(v2 - v0).norm();
    total_area += tri_area;
  }
  return total_area;
}

}  // namespace sh_baker
