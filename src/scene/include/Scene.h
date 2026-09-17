/**
 * @file Scene.h
 * @brief Scene definition and visualization configuration.
 *
 * Defines the @ref fccvis::scene::Scene class, which represents a
 * reusable visualization scene and its associated camera, filters,
 * and rendering options.
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

#include "BaseSessionObject.h"
#include "MetaObjects.h"

#include <utility>

/**
 * @brief Scene and visualization objects.
 *
 * The @c fccvis::scene namespace contains the objects that define
 * reusable visualization configurations.
 */
namespace fccvis::scene {

/**
 * @brief A reusable visualization scene.
 *
 * A Scene represents a named visualization configuration within an
 * FCCVisualization session. It combines:
 *
 * - a scene name inherited from @ref BaseSessionObject;
 * - an optional camera configuration;
 * - a collection of visualization filters;
 * - scene-specific rendering and display options.
 *
 * A scene can therefore describe how a particular view of event data
 * should be presented without being tied to a specific event.
 *
 * @see BaseSessionObject
 * @see fccvis::scene::meta::Camera
 * @see fccvis::scene::meta::Filter
 * @see fccvis::scene::meta::SceneOptions
 */
class Scene : public BaseSessionObject {
public:
  /**
   * @brief Construct a scene.
   *
   * The supplied scene name is passed to @ref BaseSessionObject and
   * the options name is used to initialize the scene's
   * @ref fccvis::scene::meta::SceneOptions object.
   *
   * @param sceneName Name identifying the scene.
   * @param optionsName Name identifying the scene options.
   */
  Scene(std::string sceneName, std::string optionsName)
      : BaseSessionObject(std::move(sceneName)),
        options(std::move(optionsName)) {}

  /**
   * @brief Camera associated with the scene.
   *
   * The camera defines the viewpoint from which the scene is
   * displayed.
   *
   * A shared pointer is used because camera objects may be shared
   * between multiple parts of a session configuration.
   *
   * @see fccvis::scene::meta::Camera
   */
  std::shared_ptr<fccvis::scene::meta::Camera> camera;

  /**
   * @brief Filters applied to the scene.
   *
   * Filters determine which visualization content is included in
   * or excluded from the scene.
   *
   * Each filter is shared through a @c std::shared_ptr so that filter
   * definitions can be reused across scenes.
   *
   * @see fccvis::scene::meta::Filter
   */
  std::vector<std::shared_ptr<fccvis::scene::meta::Filter>> filters;

  /**
   * @brief Scene-specific visualization options.
   *
   * Contains the configuration identified by the options name
   * supplied when constructing the scene.
   *
   * @see fccvis::scene::meta::SceneOptions
   */
  fccvis::scene::meta::SceneOptions options;
};

} // namespace fccvis::scene
