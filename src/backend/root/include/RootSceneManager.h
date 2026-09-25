/**
 * @file RootSceneManager.h
 * @brief Management of materialized ROOT/TEve visualization scenes.
 *
 * Defines @ref fccvis::backend::root::RootSceneManager, which manages
 * the lifetime and lookup of ROOT representations of abstract
 * FCCVisualization scenes.
 *
 * The manager maintains a clear separation between:
 *
 * - abstract scenes owned by the
 *   @ref fccvis::session::Session; and
 * - their live ROOT/TEve materializations represented by
 *   @ref fccvis::backend::root::RootScene.
 *
 * Opening and closing a scene therefore affects only its ROOT-side
 * representation. The abstract scene and session data remain
 * independent of the ROOT scene lifetime.
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

#include "RootScene.h"
#include "Scene.h"
#include "Session.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief ROOT rendering backend.
 *
 * Contains the objects responsible for materializing and managing
 * FCCVisualization scenes using ROOT/TEve.
 */
namespace fccvis::backend::root {

/**
 * @brief Manages the live ROOT/TEve representations of session scenes.
 *
 * RootSceneManager provides the main interface for opening, updating,
 * querying, and closing ROOT representations of abstract
 * @ref fccvis::scene::Scene objects.
 *
 * The manager holds a non-owning reference to an existing
 * @ref fccvis::session::Session. Abstract scenes are discovered from
 * that session and materialized into @ref RootScene objects when
 * requested.
 *
 * Multiple open scenes are tracked by their names. Opening an already
 * open scene returns the existing materialization rather than creating
 * a second ROOT representation.
 *
 * @see RootScene
 * @see fccvis::session::Session
 * @see fccvis::scene::Scene
 */
class RootSceneManager {
public:
    /**
     * @brief Construct a ROOT scene manager for a session.
     *
     * @param session Session containing the abstract scenes managed by
     *                this object.
     *
     * @note The session is referenced but not owned. It must remain
     *       valid for the lifetime of the RootSceneManager.
     */
    explicit RootSceneManager(fccvis::session::Session& session);

    /**
     * @brief Open and materialize a scene.
     *
     * Finds the abstract scene identified by @p sceneName and
     * materializes it as a live @ref RootScene.
     *
     * If the scene is already open, the existing RootScene is returned
     * instead of creating a new materialization.
     *
     * @param sceneName Name of the abstract scene to open.
     *
     * @return Pointer to the open ROOT representation, or @c nullptr
     *         if the requested scene cannot be materialized.
     */
    RootScene* Open(const std::string& sceneName);

    /**
     * @brief Close a materialized scene.
     *
     * Tears down the ROOT/TEve representation identified by
     * @p sceneName.
     *
     * Closing a scene does not modify the corresponding abstract
     * @ref fccvis::scene::Scene or the session's event data.
     *
     * @param sceneName Name of the scene to close.
     */
    void Close(const std::string& sceneName);

    /**
     * @brief Update an open ROOT scene.
     *
     * Re-synchronizes the ROOT representation with the corresponding
     * abstract scene.
     *
     * In particular, camera state is pushed through
     * @ref RootScene::SyncCamera. Shared scene configuration such as
     * filters remains associated with the same abstract objects.
     *
     * @param sceneName Name of the scene to update.
     *
     * @return @c true if the scene was found and updated; @c false
     *         otherwise.
     */
    bool Update(const std::string& sceneName);

    /**
     * @brief Get an open ROOT scene by name.
     *
     * @param sceneName Name of the scene.
     *
     * @return Pointer to the corresponding open RootScene, or
     *         @c nullptr if the scene is not currently open.
     *
     * @note The returned pointer is non-owning. Lifetime is managed by
     *       the RootSceneManager.
     */
    RootScene* Get(const std::string& sceneName) const;

    /**
     * @brief Check whether a scene is currently open.
     *
     * @param sceneName Name of the scene to query.
     *
     * @return @c true if the scene has a live ROOT representation;
     *         otherwise @c false.
     */
    bool IsOpen(const std::string& sceneName) const;

    /**
     * @brief Close all currently open scenes.
     *
     * Tears down every ROOT/TEve scene managed by this object while
     * leaving the abstract session scenes and event data unchanged.
     */
    void CloseAll();

    /**
     * @brief Get the abstract scenes currently open in ROOT.
     *
     * @return Shared pointers to the abstract scenes corresponding
     *         to the currently materialized ROOT scenes.
     */
    std::vector<std::shared_ptr<fccvis::scene::Scene>> OpenScenes();

    /**
     * @brief Get the names of currently open scenes.
     *
     * @return Shared pointers containing the names of all currently
     *         open scenes.
     */
    std::vector<std::shared_ptr<std::string>> SceneNames();

    /**
     * @brief Opens a given Scene with the category and index of an event data
     */
    RootScene *OpenWithData(const std::string &sceneName,
                        const std::string &category,
                        size_t entryIndex);
    // TODO: Funcationalities to introduce new scenes, cameras etc should be given here.

private:
    /**
     * @brief Session containing the abstract scene definitions.
     *
     * This is a non-owning reference. The session must outlive the
     * RootSceneManager.
     */
    fccvis::session::Session& m_session;

    /**
     * @brief Currently materialized ROOT scenes.
     *
     * Maps scene names to their live @ref RootScene representations.
     *
     * The manager owns the RootScene objects through
     * @c std::unique_ptr.
     */
    std::map<std::string, std::unique_ptr<RootScene>> m_openScenes;

    /**
     * @brief Find an abstract scene in the associated session.
     *
     * @param sceneName Name of the scene to find.
     *
     * @return Shared pointer to the matching abstract scene, or an
     *         empty shared pointer if no matching scene exists.
     */
    std::shared_ptr<fccvis::scene::Scene>
    FindAbstractScene(const std::string& sceneName) const;
};

} // namespace fccvis::backend::root
