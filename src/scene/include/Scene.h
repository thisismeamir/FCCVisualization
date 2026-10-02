#pragma once

/**
 * @file Scene.h
 * @brief A scene: the unit of visualization.
 *
 * A scene holds no data. Content lives in the session; the scene keeps only
 * the predicates and style rules that are applied when content becomes
 * drawables, plus its view state. Scenes exist independently of layouts and
 * may be placed in several panes, which then share the scene's view state.
 *
 * A scene is composed from four independent groups, each merged field by field:
 * - view:       camera and camera options,
 * - selection:  filters (what is drawn),
 * - appearance: style sheets and environment (how it is drawn).
 */

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#include "BaseSessionObject.h"  // fccvis::session::BaseSessionObject
#include "Camera.h"             // fccvis::scene::meta::Camera
#include "Filter.h"             // fccvis::Filter<X>
#include "Mergeable.h"
#include "Style.h"

namespace fccvis::scene::meta {

/** @brief Shared handle to a filter; editing through it affects every holder. */
template <typename X>
using FilterHandle = std::shared_ptr<filter::Filter<X>>;

/**
 * @brief How the camera behaves in a scene.
 */
enum class CameraMode {
  Free,      ///< The user may move the camera freely.
  Fixed,     ///< The camera is locked.
  Animated   ///< The camera follows a scripted motion.
};

/**
 * @brief Scene-specific camera behaviour.
 *
 * Kept separate from @ref Camera, which is shared between scenes, so that two
 * scenes can share a viewpoint but behave differently.
 */
struct CameraOptions : merge::Mergeable<CameraOptions> {
  std::optional<CameraMode> mode;          ///< Camera behaviour.
  std::optional<float> animationSeconds;   ///< Duration of one animation cycle (Animated only).

  /** @brief Members taking part in merging. */
  static constexpr auto Members() {
    return std::tuple{&CameraOptions::mode, &CameraOptions::animationSeconds};
  }
};

/**
 * @brief Selection filters of a scene, one slot per element type.
 *
 * Slots hold shared handles, so filters can be reused across scenes. An unset
 * slot means "pass everything".
 */
class FilterSet {
 public:
  /**
   * @brief Assigns the filter for element type @p X.
   * @tparam X Element type.
   * @param f Shared filter handle; nullptr clears the slot.
   */
  template <typename X>
  void Set(FilterHandle<X> f) {
    slots_[std::type_index(typeid(X))] = std::move(f);
  }

  /**
   * @brief Shared handle of the filter for @p X, or nullptr if unset.
   * @tparam X Element type.
   */
  template <typename X>
  [[nodiscard]] FilterHandle<X> Get() const {
    auto it = slots_.find(std::type_index(typeid(X)));
    return it == slots_.end() ? nullptr
                              : std::static_pointer_cast<filter::Filter<X>>(it->second);
  }

  /**
   * @brief Filter to apply for @p X: the assigned one, or the identity filter.
   * @tparam X Element type.
   */
  template <typename X>
  [[nodiscard]] filter::Filter<X> Effective() const {
    auto f = Get<X>();
    return f ? *f : filter::Filter<X>{};
  }

  /** @brief `TakeRef`: slots set in @p b replace those in @p a. */
  friend void TakeRefInto(FilterSet& a, const FilterSet& b) {
    for (const auto& [k, v] : b.slots_)
      if (v) a.slots_[k] = v;
  }

  /** @brief `CompleteBy`: only slots unset in @p a are filled from @p b. */
  friend void CompleteInto(FilterSet& a, const FilterSet& b) {
    for (const auto& [k, v] : b.slots_) {
      auto& s = a.slots_[k];
      if (!s) s = v;
    }
  }

 private:
  std::unordered_map<std::type_index, std::shared_ptr<void>> slots_;
};

}  // namespace fccvis::scene::meta

namespace fccvis::scene {

/**
 * @class Scene
 * @brief Unit of visualization: view, selection and appearance.
 *
 * Scenes are composed with @c CompleteBy and @c TakeRef (see
 * @ref fccvis::merge::Mergeable). The result keeps the receiver's name;
 * shared handles (camera, filters) are aliased, not cloned.
 *
 * @note Scene must be copy-constructible for merging, which copies the receiver.
 */
class Scene : public BaseSessionObject, public merge::Mergeable<Scene> {
 public:
  /**
   * @brief Constructs an empty scene.
   * @param sceneName Unique name identifying the scene.
   */
  explicit Scene(std::string sceneName)
      : BaseSessionObject(std::move(sceneName)) {}

  /**
   * @brief Camera defining the viewpoint.
   *
   * Shared, because several scenes may use one camera and stay synchronized.
   *
   * @see fccvis::scene::meta::Camera
   */
  std::shared_ptr<camera::Camera> camera;

  /** @brief Scene-specific camera behaviour (fixed, animated, ...). */
  meta::CameraOptions cameraOptions;

  /**
   * @brief Selection filters, one per element type.
   *
   * Decide which elements are drawn at all.
   */
  meta::FilterSet filters;

  /**
   * @brief Style sheets, one per drawable type.
   *
   * Decide how the selected elements look.
   */
  style::StyleSet styles;

  /** @brief Scene-wide appearance: background, axes, grid. */
  style::EnvironmentStyle environment;

  /**
   * @brief Sets the scene camera.
   * @param c Camera to associate; may be shared with other scenes.
   */
  void SetCamera(std::shared_ptr<camera::Camera> c) { camera = std::move(c); }

  /**
   * @brief Assigns the selection filter for element type @p X.
   * @tparam X Element type.
   * @param f Shared filter handle.
   */
  template <typename X>
  void SetFilter(meta::FilterHandle<X> f) {
    filters.Set<X>(std::move(f));
  }

  /**
   * @brief Style sheet for drawable type @p X, created if missing.
   * @tparam X Drawable type.
   */
  template <typename X>
  style::StyleSheet<X>& StyleFor() {
    return styles.For<X>();
  }

  /**
   * @brief Applies the scene's selection filter to a sequence.
   *
   * Non-mutating; session data is untouched. Passes everything if no filter
   * is assigned for @p X.
   *
   * @tparam X Element type.
   * @param in Input sequence.
   * @return The selected elements, in input order.
   */
  template <typename X>
  [[nodiscard]] std::vector<X> Select(std::span<const X> in) const {
    return filters.Effective<X>().Apply(in);
  }

  /**
   * @brief Resolves the final style of one element.
   *
   * Cascades the matching style rules, then completes the result with
   * @p defaults.
   *
   * @tparam X Drawable type.
   * @param x        Element to style.
   * @param defaults Lowest-priority style, typically the backend defaults.
   */
  template <typename X>
  [[nodiscard]] style::StyleOf<X> ResolveStyle(
      const X& x, const style::StyleOf<X>& defaults = {}) const {
    return styles.Resolve<X>(x, defaults);
  }

  /**
   * @brief Members taking part in merging.
   *
   * The name is deliberately absent: merging never changes a scene's identity.
   */
  static constexpr auto Members() {
    return std::tuple{&Scene::camera, &Scene::cameraOptions, &Scene::filters,
                      &Scene::styles, &Scene::environment};
  }
};

}  // namespace fccvis::scene
