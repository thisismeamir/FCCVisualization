/**
 * @file Session.h
 * @brief Session state, input data, and visualization configuration.
 *
 * Defines the classes used to represent an FCCVisualization session:
 * - @ref fccvis::session::SessionOptions stores visualization configuration.
 * - @ref fccvis::session::SessionData stores loaded event data.
 * - @ref fccvis::session::Session coordinates the session name, data,
 *   options, and access to event information.
 *
 * The session owns the data and the registry of scenes. Scenes hold no data:
 * they keep the filters and style rules applied when session content becomes
 * drawables. The layout only references scenes by name.
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

#include "Camera.h"
#include "Detector.h"
#include "Layout.h"
#include "Scene.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <podio/Frame.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @brief Session management and session state.
 *
 * The @c fccvis::session namespace contains the data structures used to
 * represent an FCCVisualization session, including its visualization
 * configuration and loaded event data.
 */
namespace fccvis::session {

/**
 * @brief Visualization and runtime configuration for a session.
 *
 * SessionOptions describes the objects that configure how a session is
 * visualized: the available cameras, the scenes, and the layout that arranges
 * the scenes, together with the optional path to a detector geometry
 * description.
 *
 * Cameras and scenes are shared through @c std::shared_ptr because the same
 * object may be referenced by several parts of the visualization system
 * (several scenes may use one camera, and a scene may appear in several
 * layout panes).
 *
 * Filters are not registered here: they are typed
 * (@ref fccvis::Filter<X>) and are assigned to scenes through shared handles,
 * so one filter can be shared by assigning the same handle to several scenes.
 *
 * @note Copying SessionOptions deep-copies the layout tree but aliases the
 *       shared cameras and scenes.
 *
 * @see fccvis::session::Session
 * @see fccvis::scene::Scene
 * @see fccvis::scene::camera::Camera
 * @see fccvis::layout::LayoutNode
 */
class SessionOptions {
public:
  /**
   * @brief Cameras available to the session.
   *
   * A camera describes a viewpoint and related camera configuration that
   * scenes can use. Several scenes may share one camera to stay
   * synchronized.
   */
  std::vector<std::shared_ptr<fccvis::scene::camera::Camera>> cameras;

  /**
   * @brief Scenes available to the session, keyed by unique scene name.
   *
   * A scene is a unit of visualization: its camera, selection filters, and
   * style rules. Scenes exist independently of the layout and may be placed
   * in any number of panes.
   *
   * Prefer @ref Session::CreateScene over inserting directly, which enforces
   * name uniqueness.
   */
  std::unordered_map<std::string, std::shared_ptr<fccvis::scene::Scene>>

      scenes;
  /**
   * @brief Layout describing how scenes are presented together.
   *
   * A value-owned tree of @ref fccvis::layout::LayoutNode whose panes refer
   * to scenes by name. Empty if no layout has been set.
   *
   * @see Session::ValidateLayout
   */
  std::optional<fccvis::scene::layout::LayoutNode> sceneLayout;

  /**
   * @brief Optional path to the detector geometry file.
   *
   * If present, this path identifies the geometry resource associated
   * with the session.
   */
  std::optional<std::filesystem::path> geometryFilePath;
};

/**
 * @brief Event data loaded into a session.
 *
 * SessionData organizes podio event frames into named categories.
 *
 * A category contains an ordered sequence of @c podio::Frame objects.
 * The category name is used to distinguish different data sources or
 * logical groups of events within a session.
 *
 * @see fccvis::session::Session
 * @see podio::Frame
 */
class SessionData {
public:
  /**
   * @brief Event frames grouped by category.
   *
   * The map key identifies the category, while the associated vector
   * contains the podio frames belonging to that category.
   *
   * The position of a frame in the vector is its entry index within
   * that category.
   */
  std::unordered_map<std::string, std::vector<podio::Frame>> categories;
};

/**
 * @brief Represents an FCCVisualization session.
 *
 * A Session is the central object that connects session configuration
 * with loaded event data.
 *
 * A session consists of:
 * - a name identifying the session;
 * - an optional event-data file;
 * - an optional options/configuration file;
 * - @ref fccvis::session::SessionData containing loaded podio frames;
 * - @ref fccvis::session::SessionOptions containing visualization
 *   configuration (cameras, scenes, layout).
 *
 * The session provides the main programmatic interface for discovering
 * available data categories, entries, collections, and collection types, and
 * for creating and looking up scenes.
 *
 * @note Copy construction and copy assignment are disabled. Sessions
 *       are movable.
 *
 * @see fccvis::session::SessionData
 * @see fccvis::session::SessionOptions
 */
class Session {
public:
  /**
   * @brief Construct a visualization session.
   *
   * The constructor stores the session identity and optional input
   * paths. Data and options are initialized separately through
   * @ref initData and @ref initOptions.
   *
   * @param name Name identifying the session.
   * @param dataFilePath Optional path to the event-data file.
   * @param optionsFilePath Optional path to the session options file.
   */
  Session(std::string name, std::optional<std::filesystem::path> dataFilePath,
          std::optional<std::filesystem::path> optionsFilePath);

  /**
   * @brief Destroy the session.
   *
   * Releases the session's data and visualization configuration.
   */
  ~Session();

  /**
   * @brief Construct a Session by copying another session.
   *
   * Copying sessions is intentionally disabled because a Session owns
   * mutable session state and loaded event data.
   */
  Session(const Session &) = delete;

  /**
   * @brief Copy-assign a Session.
   *
   * Copy assignment is intentionally disabled.
   *
   * @return This operation is deleted and cannot be called.
   */
  Session &operator=(const Session &) = delete;

  /**
   * @brief Construct a Session by moving another session.
   *
   * @param other Session whose state is transferred.
   *
   * @note The moved-from session remains valid but its previous state
   *       should not be relied upon.
   */
  Session(Session &&) noexcept = default;

  /**
   * @brief Move-assign a Session.
   *
   * @param other Session whose state is transferred.
   *
   * @return Reference to this session.
   */
  Session &operator=(Session &&) noexcept = default;

  /**
   * @brief Initialize event data.
   *
   * Loads and prepares the event data associated with the session's
   * configured data-file path.
   *
   * @note The exact loading behavior is determined by the corresponding
   *       implementation.
   */
  void initData();

  /**
   * @brief Initialize session options.
   *
   * Loads and prepares the visualization configuration associated with
   * the session's configured options-file path. The loader populates the
   * cameras, the scene registry, and the layout.
   *
   * @note The exact configuration format and loading behavior are
   *       determined by the corresponding implementation.
   */
  void initOptions();

  /**
   * @brief Get the session options-file path.
   *
   * @return The configured options-file path, or an empty
   *         @c std::optional if no options file was configured.
   */
  std::optional<std::filesystem::path> GetOptionsFile() const;

  /**
   * @brief Get the session visualization options.
   *
   * @return A copy of the session's @ref SessionOptions. The layout is
   *         deep-copied; cameras and scenes are shared with the session.
   *
   * @see Options
   */
  SessionOptions GetOptions() const;

  /**
   * @brief Get the session name.
   *
   * @return Constant reference to the session name.
   */
  const std::string &Name() const { return m_name; }

  /**
   * @brief Get the session event data.
   *
   * @return Constant reference to the session's @ref SessionData.
   *
   * @note The returned reference remains valid for the lifetime of the
   *       session unless the session's internal state is moved from.
   */
  const SessionData &Data() const { return m_data; }

  /**
   * @brief Get the session visualization options.
   *
   * @return Constant reference to the session's @ref SessionOptions.
   *
   * @note Unlike @ref GetOptions, this method does not create a copy.
   */
  const SessionOptions &Options() const { return m_options; }

  /**
   * @brief Get the names of all available data categories.
   *
   * @return Vector containing the names of the categories currently
   *         available in the session.
   *
   * @see SessionData::categories
   */
  std::vector<std::string> Categories() const;

  /**
   * @brief Get the number of entries in a data category.
   *
   * @param category Name of the category.
   *
   * @return Number of podio frames stored in the specified category.
   */
  size_t EntryCount(const std::string &category) const;

  /**
   * @brief Get the collection names for a category entry.
   *
   * @param category Name of the data category.
   * @param entryIndex Zero-based index of the entry within the category.
   *
   * @return Names of the collections contained in the requested frame.
   */
  std::vector<std::string> CollectionNames(const std::string &category,
                                           size_t entryIndex) const;

  /**
   * @brief Get the type of a collection in a category entry.
   *
   * @param category Name of the data category.
   * @param entryIndex Zero-based index of the entry within the category.
   * @param collName Name of the collection.
   *
   * @return Type name of the requested collection.
   */
  std::string CollectionType(const std::string &category, size_t entryIndex,
                             const std::string &collName) const;

  /**
   * @brief Get a podio frame from the session.
   *
   * @param category Name of the data category.
   * @param entryIndex Zero-based index of the entry within the category.
   *
   * @return Constant reference to the requested @c podio::Frame.
   *
   * @see SessionData::categories
   */
  const podio::Frame &GetFrame(const std::string &category,
                               size_t entryIndex) const;

  /**
   * @brief Get the detector node object.
   *
   * @return Shared pointer to the root detector node, or nullptr if no
   *         geometry has been loaded.
   */
  const std::shared_ptr<fccvis::geometry::detector::DetectorNode> &
  Detector() const {
    return m_detector;
  }

  /**
   * @brief Load the detector geometry from a DD4hep compact XML description.
   *
   * On success the root detector node is available through @ref Detector;
   * on failure @ref Detector stays nullptr.
   *
   * @param mainXMLPath Path to the main DD4hep compact XML file.
   */
  void InitializeDetectorGeometry(const std::string &mainXMLPath);

  /**
   * @brief Create a new, empty scene and register it in the session.
   *
   * Scene names are unique within a session. The returned scene is shared
   * with the session's registry and may be configured (camera, filters,
   * style rules) through its own interface.
   *
   * @param name Unique name of the new scene.
   *
   * @return The new scene, or nullptr if a scene with this name already
   *         exists.
   */
  std::shared_ptr<fccvis::scene::Scene> CreateScene(const std::string &name) {
    auto scene = std::make_shared<fccvis::scene::Scene>(name);
    const bool inserted = m_options.scenes.emplace(name, scene).second;
    return inserted ? scene : nullptr;
  }

  /**
   * @brief Look up a scene by name.
   *
   * @param name Name of the scene.
   *
   * @return The scene, or nullptr if no scene has this name.
   */
  std::shared_ptr<fccvis::scene::Scene>
  FindScene(const std::string &name) const {
    auto it = m_options.scenes.find(name);
    return it == m_options.scenes.end() ? nullptr : it->second;
  }

  /**
   * @brief Check whether a scene with the given name exists.
   *
   * @param name Name of the scene.
   *
   * @return True if the scene is registered.
   */
  bool HasScene(const std::string &name) const {
    return m_options.scenes.find(name) != m_options.scenes.end();
  }

  /**
   * @brief Set the scene layout.
   *
   * The layout is stored as given and not validated here; call
   * @ref ValidateLayout to check it against the registered scenes.
   *
   * @param layout Root of the new layout tree.
   */
  void SetLayout(fccvis::scene::layout::LayoutNode layout) {
    m_options.sceneLayout = std::move(layout);
  }

  /**
   * @brief Validate the current layout against the registered scenes.
   *
   * Checks that every pane refers to an existing scene, that containers are
   * not empty, and that split weights are positive. The same scene appearing
   * in several panes is valid.
   *
   * @return One message per violation; empty if the layout is valid or no
   *         layout is set.
   *
   * @see fccvis::layout::Validate
   */
  std::vector<std::string> ValidateLayout() const {
    if (!m_options.sceneLayout) return {};
    return fccvis::scene::layout::Validate(
        *m_options.sceneLayout,
        [this](const std::string &name) { return HasScene(name); });
  }

private:
  /**
   * @brief Name identifying the session.
   */
  std::string m_name;

  /**
   * @brief Optional path to the session's event-data file.
   */
  std::optional<std::filesystem::path> m_dataFilePath;

  /**
   * @brief Optional path to the session's options/configuration file.
   */
  std::optional<std::filesystem::path> m_optionsFilePath;

  /**
   * @brief Event data loaded by the session.
   *
   * Contains the podio frames organized into named categories. Scenes never
   * copy this data; they select from it when drawables are materialized.
   *
   * @see SessionData
   */
  SessionData m_data;

  /**
   * @brief Visualization configuration associated with the session.
   *
   * @see SessionOptions
   */
  SessionOptions m_options;

  /**
   * @brief Root of the loaded detector geometry, or nullptr.
   */
  std::shared_ptr<fccvis::geometry::detector::DetectorNode> m_detector;
};

} // namespace fccvis::session
