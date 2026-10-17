#pragma once
/**
 * @file Changes.h
 * @brief Base of the change catalogue and the result of resolving changes.
 *
 * A change is a small struct that knows its name, arguments, footprint and how
 * to apply itself. Changes are executed only through Session::Resolve, which
 * adds logging, revisions and backend notification. Each change validates
 * before it mutates, so it is all-or-nothing.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */

#include <memory>
#include <set>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "Data.h"
#include "FootPrint.h"
#include "Layout.h"
#include "Scene.h"

namespace fccvis::changes {

/** @brief Log severity. */
enum class Severity { Info, Warning, Error };

/** @brief Thrown by a change that cannot be applied. */
class ChangeError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/** @brief Registry of scenes, as held by the session. */
using SceneMap = std::unordered_map<std::string, std::shared_ptr<scene::Scene>>;

/** @brief What a change may modify. A view onto the session's state. */
struct Workspace {
  data::SessionData& data;                             ///< Series store.
  SceneMap& scenes;                                    ///< Scene registry.
  std::optional<scene::layout::LayoutNode>& layout;    ///< Session layout.
  /**
   * @brief Backend id that submitted the change; empty for user, API and
   *        script changes. Lets a change refuse interaction-driven edits, for
   *        example moving a camera whose scene is CameraMode::Fixed.
   */
  std::string origin{};
};

/** @brief Outcome of resolving one change or a batch. */
struct Result {
  bool ok = true;                       ///< False if a change failed.
  std::size_t applied = 0;              ///< Number of changes applied.
  std::vector<Footprint> touched;       ///< Footprints of the applied changes.
  std::vector<std::string> messages;    ///< Errors and warnings.

  /** @brief True if @ref ok. */
  explicit operator bool() const noexcept { return ok; }
};

/** @brief Base of every change. */
class Change {
 public:
  virtual ~Change() = default;

  /** @brief Short stable name, for example "AddLayer". */
  [[nodiscard]] virtual std::string Name() const = 0;

  /** @brief Name with arguments, for logs and scripts. */
  [[nodiscard]] virtual std::string Describe() const = 0;

  /** @brief Targeted scene; empty for session-wide changes. */
  [[nodiscard]] virtual std::string SceneName() const { return {}; }

  /** @brief What the change touches once applied. */
  [[nodiscard]] virtual Footprint Touches() const = 0;

  /**
   * @brief Applies the change.
   * @throws ChangeError if it cannot be applied; nothing is modified then.
   */
  virtual void Apply(Workspace& ws) const = 0;

  /**
   * @brief The change that undoes this one, computed from the state before
   *        it is applied (undo/redo, see Session::Undo).
   *
   * The default captures the parts of the session this change declares in
   * Touches() (a whole scene for Part::Scenes, the named layers, camera,
   * camera options, environment, layout) and returns a Restore that puts them
   * back. A change whose effect its footprint does not describe must override
   * this and return nullptr ("no inverse"), which clears the undo history.
   */
  [[nodiscard]] virtual std::shared_ptr<const Change> Inverse(const Workspace& before) const;
};

/**
 * @brief Puts back parts of the session captured before another change: the
 *        generic inverse returned by Change::Inverse.
 *
 * Captured according to a footprint: for Part::Scenes the whole scene (or its
 * absence); otherwise the named layers (Part::LayerSet or listed layers),
 * the camera (handle and state), camera options and environment of the
 * scene; and the layout for Part::Layout. The camera state is restored into
 * the camera object, so scenes that share it stay shared.
 */
class Restore final : public Change {
 public:
  /** @brief Captures what @p touched names from @p ws (state before the change). */
  static std::shared_ptr<const Restore> Capture(const Footprint& touched, const Workspace& ws) {
    auto r = std::shared_ptr<Restore>(new Restore());
    r->m_footprint = touched;
    const auto it = touched.scene.empty() ? ws.scenes.end() : ws.scenes.find(touched.scene);
    const scene::Scene* s = it == ws.scenes.end() ? nullptr : it->second.get();
    if (Has(touched.parts, Part::Scenes) && !touched.scene.empty()) {
      r->m_wholeScene = true;
      if (s) r->m_scene = *s;
      r->m_footprint.parts = touched.parts | Part::Camera | Part::CameraOptions | Part::Environment | Part::LayerSet;
      r->m_footprint.allLayers = true;
    } else if (s) {
      if (Has(touched.parts, Part::LayerSet) || !touched.layers.empty() || touched.allLayers) {
        r->m_layers = s->layers;
        r->m_layerNames = touched.layers;
        if (touched.allLayers)
          for (const auto& n : s->layers.Names()) r->m_layerNames.insert(n);
      }
      if (Has(touched.parts, Part::Camera)) r->m_cameraTouched = true;
      if (Has(touched.parts, Part::CameraOptions)) r->m_cameraOptions = s->cameraOptions;
      if (Has(touched.parts, Part::Environment)) r->m_environment = s->environment;
    }
    if (s && s->camera && (r->m_wholeScene || r->m_cameraTouched)) {
      r->m_camera = s->camera;
      r->m_cameraState = *s->camera;
    }
    if (Has(touched.parts, Part::Layout)) r->m_layout = ws.layout;
    return r;
  }

  [[nodiscard]] std::string Name() const override { return "Restore"; }
  [[nodiscard]] std::string Describe() const override {
    return "Restore(" + (m_footprint.scene.empty() ? std::string("layout") : m_footprint.scene) + ")";
  }
  [[nodiscard]] std::string SceneName() const override { return m_footprint.scene; }
  [[nodiscard]] Footprint Touches() const override { return m_footprint; }

  void Apply(Workspace& ws) const override {
    const std::string& name = m_footprint.scene;
    auto it = name.empty() ? ws.scenes.end() : ws.scenes.find(name);
    if (!m_wholeScene && !name.empty() && (it == ws.scenes.end() || !it->second))
      throw ChangeError("unknown scene '" + name + "'");
    // validated; mutate
    if (m_wholeScene) {
      if (!m_scene) {
        if (it != ws.scenes.end()) ws.scenes.erase(it);
      } else if (it != ws.scenes.end() && it->second) {
        *it->second = *m_scene;
      } else {
        ws.scenes[name] = std::make_shared<scene::Scene>(*m_scene);
      }
    } else if (!name.empty()) {
      scene::Scene& s = *it->second;
      for (const auto& layer : m_layerNames) s.layers.CopyLayer(m_layers, layer);
      if (m_cameraTouched) s.camera = m_camera;
      if (m_cameraOptions) s.cameraOptions = *m_cameraOptions;
      if (m_environment) s.environment = *m_environment;
    }
    if (m_camera && m_cameraState) *m_camera = *m_cameraState;
    if (m_layout) ws.layout = *m_layout;
  }

 private:
  Restore() = default;
  Footprint m_footprint;
  bool m_wholeScene = false;
  std::optional<scene::Scene> m_scene;                 // whole scene, or nullopt: it did not exist
  scene::meta::LayerSet m_layers;                      // layers before (only m_layerNames are restored)
  std::set<std::string> m_layerNames;
  bool m_cameraTouched = false;
  scene::CameraPtr m_camera;                           // camera handle before
  std::optional<scene::camera::Camera> m_cameraState;  // its state before
  std::optional<scene::camera::CameraOptions> m_cameraOptions;
  std::optional<style::EnvironmentStyle> m_environment;
  std::optional<std::optional<scene::layout::LayoutNode>> m_layout;
};

inline std::shared_ptr<const Change> Change::Inverse(const Workspace& before) const {
  return Restore::Capture(Touches(), before);
}

}  // namespace fccvis::changes
