#pragma once
/**
 * @file Revision.h
 * @brief Per-scene revision counters.
 *
 * A backend remembers the revisions it last drew and compares them with the
 * current ones to learn what changed, with no observers or callbacks.
 */

#include <cstdint>
#include <map>
#include <string>

#include "FootPrint.h"

namespace fccvis::changes {

/** @brief Revision counters of one scene. Larger means newer. */
struct SceneRevision {
  std::uint64_t camera = 0;
  std::uint64_t cameraOptions = 0;
  std::uint64_t environment = 0;
  std::uint64_t layerSet = 0;                   ///< Layers added or removed.
  std::map<std::string, std::uint64_t> layer;   ///< Content revision per layer.

  /**
   * @brief What changed relative to a revision a backend has seen.
   * @param seen  Revisions the backend last drew.
   * @param scene Scene name, copied into the result.
   */
  [[nodiscard]] Footprint Since(const SceneRevision& seen, std::string scene) const {
    Footprint f;
    f.scene = std::move(scene);
    if (camera != seen.camera) f.parts = f.parts | Part::Camera;
    if (cameraOptions != seen.cameraOptions) f.parts = f.parts | Part::CameraOptions;
    if (environment != seen.environment) f.parts = f.parts | Part::Environment;
    if (layerSet != seen.layerSet) f.parts = f.parts | Part::LayerSet;
    for (const auto& [name, rev] : layer) {
      auto it = seen.layer.find(name);
      if (it == seen.layer.end() || it->second != rev) f.layers.insert(name);
    }
    return f;
  }
};

}  // namespace fccvis::changes
