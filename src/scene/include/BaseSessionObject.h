/**
  * @file BaseSessionObject.h
  * @brief Base class for named objects belonging to a visualization session.
  *
  * Provides a common identity mechanism for objects that are part of
  * an FCCVisualization scene or session. Every derived object has a
  * human-readable name that can be used to identify and reference it
  * within the visualization configuration.
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

#include <string>
#include <utility>

/**
 * @brief Visualization scene objects.
 *
 * Contains the base and concrete objects used to describe the
 * structure and configuration of FCCVisualization scenes.
 */
namespace fccvis::scene {

/**
 * @brief Base class for named objects in an FCCVisualization session.
 *
 * BaseSessionObject provides a common identity mechanism for objects
 * that participate in a scene or session configuration.
 *
 * Derived classes inherit a public @ref name member and can therefore
 * be identified consistently throughout the visualization system.
 *
 * Examples of objects derived from this class include:
 * - @ref fccvis::scene::Scene
 * - @ref fccvis::scene::meta::Camera
 * - @ref fccvis::scene::meta::Filter
 * - @ref fccvis::scene::meta::SceneOptions
 *
 * @note The class has a virtual destructor so that derived objects can
 *       be destroyed safely through a pointer or reference to the base
 *       class.
 */
class BaseSessionObject {
public:
    /**
     * @brief Construct a named session object.
     *
     * @param objectName Name used to identify the object within the
     *                   visualization session.
     */
    explicit BaseSessionObject(std::string objectName)
        : name(std::move(objectName)) {}

    /**
     * @brief Destroy the session object.
     *
     * The virtual destructor allows derived session objects to be
     * destroyed correctly through a @c BaseSessionObject pointer.
     */
    virtual ~BaseSessionObject() = default;

    /**
     * @brief Name identifying the object.
     *
     * The name provides the object's identity within the session
     * configuration and can be used by other session objects to
     * refer to it.
     */
    std::string name;
};

} // namespace fccvis::scene
