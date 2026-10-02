#pragma once
/**
 * @file Camera.h
 * @brief Backend-independent description of a Camera for a Scene
 *
 * Nothing here depends on a rendering backend.
 */

#include "BaseSessionObject.h"
#include <vector>
namespace fccvis::scene::camera {

/**
 * @brief Camera configuration for a visualization scene.
 *
 * Camera stores the geometric parameters required to describe a
 * scene's viewpoint.
 *
 * The object inherits its identifying name from
 * @ref fccvis::scene::BaseSessionObject and can therefore be
 * referenced as a named session object.
 *
 * @see fccvis::scene::Scene
 * @see fccvis::scene::BaseSessionObject
 */
class Camera : public fccvis::scene::BaseSessionObject {
public:
  /**
   * @brief Inherit constructors from BaseSessionObject.
   *
   * Camera construction is delegated to the base class. In
   * particular, the inherited constructor can be used to assign
   * the camera's session-level name.
   */
  using BaseSessionObject::BaseSessionObject;

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
};


}
