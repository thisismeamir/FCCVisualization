/**
 * @file Markers.h
 * @brief Basic graphical primitives and color configuration.
 *
 * Defines the lightweight scene primitives used by FCCVisualization
 * to represent points and line segments, together with their color
 * and rendering properties.
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

#include <array>
#include <string>
#include <utility>

/**
 * @brief Metadata and graphical primitives for visualization scenes.
 */
namespace fccvis::scene::meta {

/**
 * @brief Color specification for visualization primitives.
 *
 * Color provides a backend-independent enumeration of the colors
 * available to scene primitives such as @ref Marker and @ref Line.
 *
 * In addition to explicit colors, the enumeration provides automatic
 * color modes. The interpretation of automatic modes is performed by
 * the visualization backend.
 */
enum class Color {
  /** @brief White. */
  White,

  /** @brief Black. */
  Black,

  /** @brief Red. */
  Red,

  /** @brief Green. */
  Green,

  /** @brief Blue. */
  Blue,

  /** @brief Magenta. */
  Magenta,

  /** @brief Cyan. */
  Cyan,

  /** @brief Violet. */
  Violet,

  /** @brief Pink. */
  Pink,

  /** @brief Orange. */
  Orange,

  /** @brief Yellow. */
  Yellow,

  /** @brief Spring green. */
  Spring,

  /** @brief Teal. */
  Teal,

  /** @brief Azure. */
  Azure,

  /** @brief Gray. */
  Gray,

  /** @brief Dark red. */
  DarkRed,

  /** @brief Dark green. */
  DarkGreen,

  /** @brief Dark blue. */
  DarkBlue,

  /** @brief Dark magenta. */
  DarkMagenta,

  /** @brief Dark cyan. */
  DarkCyan,

  /** @brief Dark violet. */
  DarkViolet,

  /** @brief Dark pink. */
  DarkPink,

  /** @brief Dark orange. */
  DarkOrange,

  /** @brief Dark yellow. */
  DarkYellow,

  /** @brief Light red. */
  LightRed,

  /** @brief Light green. */
  LightGreen,

  /** @brief Light blue. */
  LightBlue,

  /** @brief Light magenta. */
  LightMagenta,

  /** @brief Light cyan. */
  LightCyan,

  /** @brief Light violet. */
  LightViolet,

  /** @brief Light pink. */
  LightPink,

  /** @brief Light orange. */
  LightOrange,

  /** @brief Light yellow. */
  LightYellow,

  /**
   * @brief Automatically select a color.
   *
   * The backend determines the color according to the visualization
   * context.
   */
  Auto,

  /**
   * @brief Automatically select a color based on object identity.
   *
   * The backend associates colors with object identifiers.
   */
  AutoId,

  /**
   * @brief Automatically select a color based on iteration.
   *
   * The backend associates colors with the current iteration or
   * sequence position.
   */
  AutoIter,

  /**
   * @brief Automatically select a color based on energy.
   *
   * The backend associates colors with the relevant energy value.
   */
  AutoEnergy
};

/**
 * @brief A point-like graphical primitive.
 *
 * Marker represents a single position in three-dimensional space
 * together with its display color and size.
 *
 * Markers can be used for annotations, reference points, detector
 * features, or other point-like visualization elements.
 */
class Marker {
public:
  /**
   * @brief Construct a marker with a name.
   *
   * @param name Name identifying the marker.
   */
  Marker(std::string name) : name(std::move(name)) {}

  /**
   * @brief Name identifying the marker.
   */
  std::string name;

  /**
   * @brief Three-dimensional position of the marker.
   *
   * The array contains the three spatial coordinates in the
   * coordinate system used by the visualization.
   */
  std::array<double, 3> position{};

  /**
   * @brief Display color of the marker.
   *
   * Defaults to @ref Color::White.
   */
  Color color = Color::White;

  /**
   * @brief Display size of the marker.
   *
   * Defaults to one backend-defined size unit.
   */
  unsigned int size = 1;
};

/**
 * @brief A line-segment graphical primitive.
 *
 * Line represents a straight segment between two three-dimensional
 * positions.
 *
 * A line has independently configurable color, width, and rendering
 * style.
 */
class Line {
public:
  /**
   * @brief Construct a line with a name.
   *
   * @param name Name identifying the line.
   */
  Line(std::string name) : name(std::move(name)) {}

  /**
   * @brief Name identifying the line.
   */
  std::string name;

  /**
   * @brief Three-dimensional starting position of the line.
   */
  std::array<double, 3> start{};

  /**
   * @brief Three-dimensional ending position of the line.
   */
  std::array<double, 3> end{};

  /**
   * @brief Display color of the line.
   *
   * Defaults to @ref Color::White.
   */
  Color color = Color::White;

  /**
   * @brief Line width.
   *
   * Defaults to one backend-defined width unit.
   */
  unsigned int width = 1;

  /**
   * @brief Line rendering style.
   *
   * The numerical value is interpreted by the visualization backend.
   * The default value is one.
   */
  unsigned int style = 1;
};

} // namespace fccvis::scene::meta
