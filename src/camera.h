#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace sh_renderer {

struct Camera {
  Eigen::Vector3f position;
  Eigen::Quaternionf orientation;
};

/*
 * @brief Translates the camera by the given delta.
 *
 * @param delta The delta to translate the camera by.
 * @param camera The camera to translate.
 */
void TranslateCamera(const Eigen::Vector3f& delta, Camera* camera);

/*
 * @brief Pans the camera by the given delta (+Y-up world).
 *
 * @param delta The delta to pan the camera by.
 * @param camera The camera to pan.
 */
void PanCamera(const float delta, Camera* camera);

/*
 * @brief Tilts the camera by the given delta (+X-right world).
 *
 * @param delta The delta to tilt the camera by.
 * @param camera The camera to tilt.
 */
void TiltCamera(const float delta, Camera* camera);

/*
 * @brief Rolls the camera by the given delta (+Z-right world).
 *
 * @param delta The delta to roll the camera by.
 * @param camera The camera to roll.
 */
void RollCamera(const float delta, Camera* camera);

/*
 * @brief Sets the camera's orientation to look at the given target.
 *
 * @param target The target to look at.
 * @param camera The camera to look at.
 */
void LookAt(const Eigen::Vector3f& target, Camera* camera);

/*
 * @brief Gets the view matrix for the given camera.
 *
 * @param camera The camera to get the view matrix for.
 * @return The view matrix.
 */
Eigen::Matrix4f GetViewMatrix(const Camera& camera);

/*
 * @brief Computes the perspective projection matrix.
 *
 * @param fov_y_radians The vertical field of view in radians.
 * @param aspect_ratio The aspect ratio (width / height).
 * @param z_near The distance to the near clipping plane.
 * @param z_far The distance to the far clipping plane.
 * @return The 4x4 projection matrix.
 */
Eigen::Matrix4f GetProjectionMatrix(float fov_y_radians, float aspect_ratio,
                                    float z_near, float z_far);

}  // namespace sh_renderer