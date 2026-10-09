#pragma once
/**
 * @file Camera.h
 * @brief Backend-independent description of a Camera for a Scene
 *
 * Nothing here depends on a rendering backend.
 */

#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Mergeable.h"

namespace fccvis::scene::camera {
/** @brief How the camera behaves in a scene. */
enum class CameraMode {
  Free,      ///< The user may move the camera freely.
  Fixed,     ///< The camera is locked.
  Animated   ///< The camera follows a scripted motion.
};

/**
 * @brief Scene-specific camera behaviour.
 *
 * Kept apart from the camera itself, which may be shared between scenes.
 */
struct CameraOptions : merge::Mergeable<CameraOptions> {
  std::optional<CameraMode> mode;          ///< Camera behaviour.
  std::optional<float> animationSeconds;   ///< Duration of one animation cycle.

  /** @brief Members taking part in merging. */
  static constexpr auto Members() {
    return std::tuple{&CameraOptions::mode, &CameraOptions::animationSeconds};
  }
};


/**
 * @brief Camera configuration for a visualization scene.
 *
 * Camera stores the geometric parameters required to describe a
 * scene's viewpoint.
 *
 * @see fccvis::scene::Scene
 */
class Camera {
public:
  Camera() = default;
  explicit Camera(std::string cameraName)
    : name(std::move(cameraName)) {}


  /**
   * @brief Position of the camera.
   *
   * Stores the camera position as a sequence of numerical
   * coordinates.
   *
   * The interpretation of the vector components is determined by
   * the visualization backend.
   */
  std::vector<double> positionVector{0.0, 0.0, 0.0};

  /**
   * @brief Viewing direction of the camera.
   *
   * Stores the direction in which the camera is oriented as a
   * sequence of numerical components.
   */
  std::vector<double> directionVector{0.0, 0.0, 1.0};

  /**
   * @brief a scaling factor to change the default magnitude of the direction vector.
  */
  double magnification{1.0};

  /**
   * @brief Changes both the position and direction vectors.
   */
  void ChangeCameraSpec(std::vector<double> position, std::vector<double> direction) {
    positionVector = std::move(position);
    directionVector = std::move(direction);
  }

  /**
   * @brief Changes the position vector.
   */
  void ChangeCameraPosition(std::vector<double> position) {
    positionVector = std::move(position);
  }

  /**
   * @brief Changes the direction vector.
   */
  void ChangeCameraDirection(std::vector<double> direction) {
    directionVector = std::move(direction);
  }

  /**
   * @brief Sets the magnification scale factor.
   */
  void Magnification(double scale) {
    magnification = scale;
  }

  /**
   * @brief Resets the magnification scale factor to default.
   */
  void ResetMagnification() {
    magnification = 1.0;
  }

  std::string name;
};


}
