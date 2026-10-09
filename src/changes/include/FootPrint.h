#pragma once
/**
 * @file Footprint.h
 * @brief Which parts of the session a change touches.
 */

#include <cstdint>
#include <set>
#include <string>
#include <utility>

namespace fccvis::changes {

/** @brief Parts of a scene or session that revisions are tracked for. */
enum class Part : std::uint32_t {
  None          = 0,
  Camera        = 1u << 0,  ///< The camera handle or its state.
  CameraOptions = 1u << 1,  ///< Camera mode and animation settings.
  Environment   = 1u << 2,  ///< Background, axes, grid.
  LayerSet      = 1u << 3,  ///< Layers added or removed.
  Layout        = 1u << 4,  ///< The session layout.
  Scenes        = 1u << 5,  ///< The scene registry (scene created or removed).
  Data          = 1u << 6   ///< Registered series.
};

/** @brief Union of two part sets. */
constexpr Part operator|(Part a, Part b) {
  return static_cast<Part>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

/** @brief True if @p set contains any of @p p. */
constexpr bool Has(Part set, Part p) {
  return (static_cast<std::uint32_t>(set) & static_cast<std::uint32_t>(p)) != 0;
}

/**
 * @brief Describes what a change touched.
 *
 * Drives revision bumps, so backends redraw only what differs.
 */
struct Footprint {
  std::string scene;             ///< Affected scene; empty for session-wide parts.
  Part parts = Part::None;       ///< Touched parts.
  std::set<std::string> layers;  ///< Layers whose content changed.
  bool allLayers = false;        ///< Treat every layer of the scene as changed.

  /** @brief Builds a footprint. */
  static Footprint Of(std::string scene, Part parts,
                      std::set<std::string> layers = {}) {
    Footprint f;
    f.scene = std::move(scene);
    f.parts = parts;
    f.layers = std::move(layers);
    return f;
  }
};

}  // namespace fccvis::changes
