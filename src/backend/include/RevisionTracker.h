/**
 * @file RevisionTracker.h
 * @brief Turns a SyncRequest into "what to redraw", the same way for every backend.
 *
 * ARCHITECTURE decision 9: no observers; a backend remembers the revisions it
 * last drew and diffs them with the current ones. This class implements that
 * bookkeeping once:
 *
 * - a scene opened since the last sync is drawn completely;
 * - otherwise only the parts and layers whose revisions moved are redrawn;
 * - a change in the data revision (series registered or replaced) redraws
 *   every layer, since any series reference may now resolve differently;
 * - when the request's origin is this backend, the change is already on its
 *   screen: revisions are recorded and nothing is redrawn;
 * - the layout is reapplied when its revision or the set of open scenes moved.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Backend.h"
#include "FootPrint.h"
#include "Revision.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace fccvis::backend {

/** @brief What one scene needs redrawn. */
struct SceneUpdate {
  bool opened = false;              ///< First sync since the scene was opened: draw everything.
  bool camera = false;              ///< Camera moved (or shared camera moved).
  bool cameraOptions = false;       ///< Camera mode/animation changed.
  bool environment = false;         ///< Background/axes/grid changed.
  bool layerSet = false;            ///< Layers were added or removed.
  std::set<std::string> layers;     ///< Layers whose content must be rebuilt (existing layers only).
  std::set<std::string> removed;    ///< Layers that disappeared and must be cleared.

  /** @brief True if nothing needs redrawing. */
  [[nodiscard]] bool Empty() const {
    return !opened && !camera && !cameraOptions && !environment && !layerSet &&
           layers.empty() && removed.empty();
  }
};

/** @brief Everything a backend must do for one SyncRequest. */
struct SyncPlan {
  std::map<std::string, SceneUpdate> scenes;  ///< Open scenes with something to redraw.
  std::set<std::string> closed;               ///< Scenes no longer open (drop their views).
  bool layout = false;                        ///< Layout or open-scene set changed.
  bool skippedOwnChange = false;              ///< Origin was this backend: recorded, not drawn.

  /** @brief True if nothing needs to be done. */
  [[nodiscard]] bool Empty() const { return scenes.empty() && closed.empty() && !layout; }
};

/** @brief Remembers drawn revisions and computes SyncPlans. Not thread-safe. */
class RevisionTracker {
 public:
  /**
   * @brief Computes what changed since the last call and records the new state.
   * @param request The session's sync request.
   * @param selfId  This backend's instance id (for the origin rule).
   */
  SyncPlan Update(const SyncRequest& request, const std::string& selfId) {
    SyncPlan plan;
    const auto& view = request.view;
    const bool own = !request.origin.empty() && request.origin == selfId;
    const bool dataMoved = view.DataRevision() != m_data;
    m_data = view.DataRevision();

    for (auto it = m_seen.begin(); it != m_seen.end();) {
      if (!request.open.count(it->first)) {
        plan.closed.insert(it->first);
        m_layers.erase(it->first);
        it = m_seen.erase(it);
      } else {
        ++it;
      }
    }

    for (const auto& name : request.open) {
      const auto now = view.Revision(name);
      const auto scene = view.FindScene(name);
      std::vector<std::string> names;
      if (scene) names = scene->layers.Names();
      auto seen = m_seen.find(name);
      SceneUpdate u;
      if (seen == m_seen.end()) {
        u.opened = u.camera = u.cameraOptions = u.environment = u.layerSet = true;
        u.layers.insert(names.begin(), names.end());
      } else {
        const changes::Footprint f = now.Since(seen->second, name);
        u.camera = changes::Has(f.parts, changes::Part::Camera);
        u.cameraOptions = changes::Has(f.parts, changes::Part::CameraOptions);
        u.environment = changes::Has(f.parts, changes::Part::Environment);
        u.layerSet = changes::Has(f.parts, changes::Part::LayerSet);
        for (const auto& l : f.layers)
          if (std::find(names.begin(), names.end(), l) != names.end()) u.layers.insert(l);
        if (dataMoved) u.layers.insert(names.begin(), names.end());
        for (const auto& old : m_layers[name])
          if (std::find(names.begin(), names.end(), old) == names.end()) u.removed.insert(old);
        for (const auto& l : names)
          if (!m_layers[name].count(l)) u.layers.insert(l);  // new layer
      }
      m_seen[name] = now;
      m_layers[name] = std::set<std::string>(names.begin(), names.end());
      if (!u.Empty()) plan.scenes.emplace(name, std::move(u));
    }

    if (view.LayoutRevision() != m_layout || request.open != m_open) plan.layout = true;
    m_layout = view.LayoutRevision();
    m_open = request.open;

    if (own) {
      // Already on screen: keep the recorded revisions, draw nothing except
      // structural work the origin cannot have done (closing scenes).
      plan.scenes.clear();
      plan.layout = false;
      plan.skippedOwnChange = true;
    }
    return plan;
  }

  /** @brief Forgets a scene (call from Backend::CloseScene). */
  void Forget(const std::string& scene) {
    m_seen.erase(scene);
    m_layers.erase(scene);
    m_open.erase(scene);
  }

  /** @brief Forgets everything (call from Backend::Stop). */
  void Reset() { *this = RevisionTracker{}; }

 private:
  std::map<std::string, changes::SceneRevision> m_seen;
  std::map<std::string, std::set<std::string>> m_layers;
  std::set<std::string> m_open;
  std::uint64_t m_layout = 0;
  std::uint64_t m_data = 0;
};

/** @brief Adds a SyncPlan to DrawStats (helper for backends that keep stats). */
inline void Count(DrawStats& stats, const SyncPlan& plan) {
  if (plan.Empty()) return;
  ++stats.syncs;
  if (plan.layout) ++stats.layouts;
  for (const auto& [scene, u] : plan.scenes) {
    if (u.camera) ++stats.cameras[scene];
    if (u.environment) ++stats.environments[scene];
    for (const auto& l : u.layers) ++stats.layers[scene + "/" + l];
  }
}

}  // namespace fccvis::backend
