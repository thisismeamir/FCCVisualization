/**
 * @file Session.h
 * @brief Central state of a visualization session.
 *
 * A Session owns format-independent data and visualization configuration.
 * Input-specific loading and conversion are intentionally outside this
 * class: it has no knowledge of ROOT, podio, DD4hep, EDM4hep, or any other
 * input representation.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Layout.h"
#include "Scene.h"
#include "Data.h"
#include "SessionOptions.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fccvis::session {

/**
 * @brief Represents a visualization session.
 *
 * Session connects the typed, read-only SessionData with the visualization
 * configuration (cameras, scenes, layout). Scenes refer to data by series
 * reference only; they never copy it.
 */
class Session {
public:
  /**
   * @brief Construct an empty visualization session.
   *
   * @param name Name identifying the session.
   */
  explicit Session(std::string name) : m_name(std::move(name)) {}

  ~Session() = default;

  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;

  Session(Session &&) noexcept = default;
  Session &operator=(Session &&) noexcept = default;

  /** @brief Get the session name. */
  const std::string &Name() const noexcept { return m_name; }

  /** @brief Access the session data. */
  data::SessionData &Data() noexcept { return m_data; }

  /** @brief Access the session data without modification. */
  const data::SessionData &Data() const noexcept { return m_data; }

  /** @brief Replace the session data. */
  void SetData(data::SessionData data) { m_data = std::move(data); }

  /** @brief Access the session visualization options. */
  SessionOptions &Options() noexcept { return m_options; }

  /** @brief Access the session visualization options without modification. */
  const SessionOptions &Options() const noexcept { return m_options; }

  /**
   * @brief Get the names of all registered scenes, sorted.
   */
  std::vector<std::string> SceneNames() const {
    std::vector<std::string> names;
    names.reserve(m_options.scenes.size());

    for (const auto &[name, _] : m_options.scenes)
      names.push_back(name);

    std::sort(names.begin(), names.end());
    return names;
  }

  /**
   * @brief Create a new, empty scene and register it in the session.
   *
   * @param name Unique name of the new scene.
   *
   * @return The new scene, or nullptr if a scene with this name already
   *         exists.
   */
  std::shared_ptr<fccvis::scene::Scene>
  CreateScene(const std::string &name) {
    if (HasScene(name))
      return nullptr;

    auto scene = std::make_shared<fccvis::scene::Scene>(name);
    m_options.scenes.emplace(name, scene);
    return scene;
  }

  /**
   * @brief Look up a scene by name.
   *
   * @return The scene, or nullptr if no scene has this name.
   */
  std::shared_ptr<fccvis::scene::Scene>
  FindScene(const std::string &name) const {
    auto it = m_options.scenes.find(name);
    return it == m_options.scenes.end() ? nullptr : it->second;
  }

  /** @brief Check whether a scene with the given name exists. */
  bool HasScene(const std::string &name) const {
    return m_options.scenes.find(name) != m_options.scenes.end();
  }

  /**
   * @brief Set the scene layout.
   *
   * @param layout Root of the new layout tree.
   */
  void SetLayout(fccvis::scene::layout::LayoutNode layout) {
    m_options.sceneLayout = std::move(layout);
  }

  /**
   * @brief Validate the current layout against the registered scenes.
   *
   * @return One message per violation; empty if the layout is valid or no
   *         layout is set.
   */
  std::vector<std::string> ValidateLayout() const {
    if (!m_options.sceneLayout)
      return {};

    return fccvis::scene::layout::Validate(
        *m_options.sceneLayout,
        [this](const std::string &name) { return HasScene(name); });
  }

  /**
   * @brief Validate every scene's layers against the session data.
   *
   * Checks that each layer points at an existing, unambiguous series and that
   * its entry is in range.
   *
   * @return One message per violation, prefixed with the scene name.
   */
  std::vector<std::string> ValidateScenes() const {
    std::vector<std::string> errors;
    for (const auto &name : SceneNames()) {
      const auto scene = FindScene(name);
      if (!scene)
        continue;
      for (auto &e : scene->Validate(m_data))
        errors.push_back("scene '" + name + "': " + e);
    }
    return errors;
  }

  /**
   * @brief Validate layout and scenes together.
   *
   * @return Layout violations followed by scene violations; empty if the
   *         session configuration is consistent with its data.
   */
  std::vector<std::string> Validate() const {
    auto errors = ValidateLayout();
    for (auto &e : ValidateScenes())
      errors.push_back(std::move(e));
    return errors;
  }

private:
  std::string m_name;
  data::SessionData m_data;
  SessionOptions m_options;
};

} // namespace fccvis::session
