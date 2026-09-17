/**
 * @file RootScene.h
 * @brief ROOT/TEve materialization of an FCCVisualization scene.
 *
 * Defines @ref fccvis::backend::root::RootScene, which connects an
 * abstract FCCVisualization scene to its ROOT/TEve representation.
 *
 * An abstract @ref fccvis::scene::Scene describes the visualization
 * independently of the rendering backend. RootScene materializes that
 * description into the corresponding ROOT TEve objects, including a
 * @c TEveScene and @c TEveViewer.
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

#include "Scene.h"

#include <TEveManager.h>
#include <TEveScene.h>
#include <TEveViewer.h>

#include <memory>

/**
 * @brief ROOT rendering backend.
 *
 * Contains the objects responsible for materializing and managing
 * FCCVisualization scenes using the ROOT TEve framework.
 */
namespace fccvis::backend::root {

/**
 * @brief ROOT/TEve representation of an abstract visualization scene.
 *
 * RootScene bridges the backend-independent
 * @ref fccvis::scene::Scene representation and the corresponding
 * ROOT/TEve graphical objects.
 *
 * The class maintains a shared reference to the abstract Scene while
 * owning or managing its associated TEve-side representation.
 *
 * The abstract scene remains independent of ROOT and can therefore
 * continue to exist separately from its materialized ROOT scene.
 *
 * @see fccvis::scene::Scene
 * @see fccvis::backend::root::RootSceneManager
 * @see TEveScene
 * @see TEveViewer
 */
class RootScene {
public:
    /**
     * @brief Construct a ROOT representation of a scene.
     *
     * Materializes the supplied abstract scene into the ROOT/TEve
     * representation.
     *
     * @param scene Abstract scene to materialize.
     *
     * @note The abstract scene is retained through a
     *       @c std::shared_ptr and therefore remains alive as long as
     *       the RootScene retains its reference.
     */
    explicit RootScene(std::shared_ptr<fccvis::scene::Scene> scene);

    /**
     * @brief Destroy the ROOT representation of the scene.
     *
     * Releases the ROOT/TEve resources associated with this scene.
     */
    ~RootScene();

    /**
     * @brief Copy construction is disabled.
     *
     * A RootScene represents live ROOT/TEve state and is therefore
     * intentionally non-copyable.
     */
    RootScene(const RootScene&) = delete;

    /**
     * @brief Copy assignment is disabled.
     *
     * A RootScene represents live ROOT/TEve state and is therefore
     * intentionally non-copyable.
     */
    RootScene& operator=(const RootScene&) = delete;

    /**
     * @brief Get the ROOT TEve viewer associated with the scene.
     *
     * @return Pointer to the associated @c TEveViewer.
     *
     * @note The returned pointer is non-owning and is managed by the
     *       ROOT/TEve infrastructure.
     */
    TEveViewer* Viewer() const { return m_pViewer; }

    /**
     * @brief Get the ROOT TEve scene.
     *
     * @return Pointer to the associated @c TEveScene.
     *
     * @note The returned pointer is non-owning and is managed by the
     *       ROOT/TEve infrastructure.
     */
    TEveScene* EveScene() const { return m_pEveScene; }

    /**
     * @brief Synchronize the ROOT camera with the abstract scene.
     *
     * Pushes the camera position and viewing direction stored in
     * @ref fccvis::scene::Scene::camera into the ROOT/TEve viewer.
     *
     * This method synchronizes camera state from the abstract
     * FCCVisualization representation to the ROOT rendering backend.
     */
    void SyncCamera();

    /**
     * @brief Get the underlying abstract scene.
     *
     * @return Shared pointer to the abstract
     *         @ref fccvis::scene::Scene represented by this object.
     */
    std::shared_ptr<fccvis::scene::Scene> GetSceneAbstract();

private:
    /**
     * @brief Abstract FCCVisualization scene represented by this object.
     *
     * The shared pointer keeps the backend-independent scene alive
     * while its ROOT representation exists.
     */
    std::shared_ptr<fccvis::scene::Scene> m_scene;

    /**
     * @brief ROOT TEve viewer associated with the scene.
     *
     * This pointer refers to the viewer used to display the
     * materialized scene.
     */
    TEveViewer* m_pViewer;

    /**
     * @brief ROOT TEve scene containing the scene's graphical objects.
     *
     * This pointer refers to the TEve scene materialized from the
     * abstract FCCVisualization scene.
     */
    TEveScene* m_pEveScene;
};

} // namespace fccvis::backend::root
