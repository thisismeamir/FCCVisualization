/**
 * @file MetaObjects.h
 * @brief Metadata objects used to configure FCCVisualization scenes.
 *
 * Defines the metadata and configuration objects used by the scene
 * system, including cameras, filters, scene options, and hierarchical
 * scene layouts.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 *
 * @copyright Copyright 2026 FCC Project at CERN
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "BaseSessionObject.h"
#include "Markers.h"

/**
 * @brief Metadata and configuration objects for visualization scenes.
 *
 * The @c fccvis::scene::meta namespace contains the lightweight
 * configuration objects used to describe how scenes are constructed
 * and displayed.
 *
 * These objects are primarily metadata rather than rendering objects.
 * They can be composed into higher-level objects such as
 * @ref fccvis::scene::Scene and @ref fccvis::session::SessionOptions.
 */
namespace fccvis::scene::meta {

/**
 * @brief Describes the structural role of a node in a scene layout.
 *
 * A layout is represented as a tree of @ref LayoutNode objects.
 * Each node has one of three structural roles:
 *
 * - @c Pack: contains child nodes arranged according to an orientation.
 * - @c Tab: contains child nodes presented as alternative tab views.
 * - @c Leaf: represents a concrete scene slot.
 *
 * @see LayoutNode
 */
enum class LayoutKind {
  /**
   * @brief A container that arranges its children spatially.
   *
   * The arrangement direction is specified by
   * @ref PackOrientation.
   */
  Pack,

  /**
   * @brief A container that presents its children as tabs.
   */
  Tab,

  /**
   * @brief A leaf node representing a concrete scene.
   */
  Leaf
};

/**
 * @brief Specifies the orientation of a packed layout.
 *
 * This value is meaningful for @ref LayoutKind::Pack nodes and
 * determines the direction in which their children are arranged.
 */
enum class PackOrientation {
  /**
   * @brief Arrange children horizontally.
   */
  Horizontal,

  /**
   * @brief Arrange children vertically.
   */
  Vertical
};

/**
 * @brief Node in the hierarchical layout of visualization scenes.
 *
 * A LayoutNode forms one element of a tree describing how multiple
 * scenes are presented together.
 *
 * The meaning of the node's other members depends on its
 * @ref kind:
 *
 * - For @c LayoutKind::Pack, @ref orientation and @ref children are
 *   used. Child weights determine the relative allocation of space.
 * - For @c LayoutKind::Tab, @ref children is used and child weights
 *   are ignored.
 * - For @c LayoutKind::Leaf, @ref sceneName identifies the scene
 *   materialized by the node.
 *
 * This structure allows a session to represent nested arrangements,
 * such as horizontal and vertical groups containing tabbed scenes.
 *
 * @see LayoutKind
 * @see PackOrientation
 * @see fccvis::scene::Scene
 * @see fccvis::session::SessionOptions
 */
struct LayoutNode {
  /**
   * @brief Structural kind of this layout node.
   *
   * Determines which of the other members are applicable.
   */
  LayoutKind kind;

  /**
   * @brief Orientation of children in a packed node.
   *
   * This member is used when @ref kind is @c LayoutKind::Pack.
   *
   * The default orientation is horizontal.
   */
  PackOrientation orientation = PackOrientation::Horizontal;

  /**
   * @brief Name of the scene represented by a leaf node.
   *
   * This member is used when @ref kind is @c LayoutKind::Leaf.
   *
   * The name identifies the @ref fccvis::scene::Scene that is
   * materialized in this layout slot.
   */
  std::string sceneName;

  /**
   * @brief Child nodes of this layout node.
   *
   * Each child is represented by a shared pointer to another
   * @ref LayoutNode together with an integer weight.
   *
   * For @c LayoutKind::Pack nodes, the weight determines the child's
   * relative share of the available layout space.
   *
   * For @c LayoutKind::Tab nodes, the weight is ignored.
   *
   * Leaf nodes normally have no children.
   */
  std::vector<
      std::pair<std::shared_ptr<LayoutNode>, int /* weight, Pack only */>>
      children;
};

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
/**
 * @brief Filter configuration for a visualization scene.
 *
 * Filter is a named session object representing a visualization
 * filtering configuration.
 *
 * The class currently inherits its construction and identity
 * directly from @ref fccvis::scene::BaseSessionObject and does not
 * define additional filter parameters.
 *
 * @see fccvis::scene::Scene
 * @see fccvis::scene::BaseSessionObject
 */
class Filter : public BaseSessionObject {
public:
  /**
   * @brief Inherit constructors from BaseSessionObject.
   *
   * Allows a Filter to be constructed using the same naming
   * interface as other session objects.
   */
  using BaseSessionObject::BaseSessionObject;
};

/**
 * @brief Styling properties applicable to an event collection or visual grouping.
 */
struct CollectionStyle {
  /** @brief Primary color of the collection primitives. */
  Color color = Color::Auto;

  /** @brief Marker or point size (applicable to hits and points). */
  unsigned int markerSize = 1;

  /** @brief Line or trajectory stroke width (applicable to tracks). */
  unsigned int lineWidth = 1;

  /** @brief Line style index (e.g., solid, dashed, dotted). */
  unsigned int lineStyle = 1;

  /** @brief Transparency level (0.0 = fully opaque, 1.0 = fully transparent). */
  double transparency = 0.0;

  /** @brief Visibility flag to toggle rendering of this collection. */
  bool visible = true;
};

/**
 * @brief Visual options and primitive definitions associated with a scene.
 *
 * SceneOptions holds both per-collection style configurations and standalone
 * visual primitives (Markers, Lines) defined for the scene.
 */
class SceneOptions : public BaseSessionObject {
public:
  using BaseSessionObject::BaseSessionObject;

  /**
   * @brief Collection-specific styling map.
   * Key: Collection name (e.g., "ECalBarrelHits", "CentralTracks").
   * Value: Associated CollectionStyle parameters.
   */
  std::unordered_map<std::string, CollectionStyle> collectionStyles;

  /**
   * @brief Standalone point markers belonging directly to the scene.
   */
  std::vector<fccvis::scene::meta::Marker> markers;

  /**
   * @brief Standalone line segments belonging directly to the scene.
   */
  std::vector<Line> lines;

  /**
   * @brief Retrieves the style for a collection, returning defaults if non-existent.
   */
  CollectionStyle GetStyleForCollection(const std::string& collectionName) const {
    auto it = collectionStyles.find(collectionName);
    if (it != collectionStyles.end()) {
      return it->second;
    }
    return CollectionStyle{}; // Default configuration
  }
};

} // namespace fccvis::scene::meta
