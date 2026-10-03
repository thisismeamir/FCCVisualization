/**
 * @file Scene.h
 * @brief A scene: the unit of visualization.
 *
 * A scene holds no data. It is composed of:
 * - view:       camera and camera options,
 * - appearance: scene-wide environment,
 * - layers:     each layer points at a series in the session, an entry cursor,
 *               a selection filter and a style sheet.
 *
 * Layers are the answer to "what does this scene show": two layers of the same
 * element type (reconstructed and MC tracks) have independent filters and
 * styles. Scenes exist independently of layouts and may appear in several
 * panes, which then share the scene's view state, cursors included.
 *
 * Scenes are composed with @c CompleteBy and @c TakeRef (see
 * fccvis::merge::Mergeable); layers merge by name.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

#include "BaseSessionObject.h"
#include "Camera.h"
#include "Filter.h"
#include "Mergeable.h"
#include "Data.h"
#include "Style.h"

namespace fccvis::scene::meta {

/** @brief Shared handle to a filter; editing through it affects every holder. */
template <typename X>
using FilterHandle = std::shared_ptr<filter::Filter<X>>;


/**
 * @brief One displayed series of a scene.
 *
 * All fields are optional or neutral when unset, so layers merge like every
 * other partial record.
 *
 * @tparam X Element type of the series shown.
 */
template <typename X>
struct Layer : merge::Mergeable<Layer<X>> {
  /** @brief Series to show. A layer without a series draws nothing. */
  std::optional<data::SeriesRef> series;

  /** @brief Entry of the series to draw. Defaults to 0 when unset. */
  std::optional<std::size_t> entry;

  /** @brief Selection filter; null passes every element. */
  FilterHandle<X> filter;

  /** @brief Style rules, resolved per selected element. */
  style::StyleSheet<X> styles;

  /** @brief True if @p x passes the layer's filter. */
  [[nodiscard]] bool Passes(const X& x) const {
    return !filter || filter->DoesPass(x);
  }

  /** @brief Members taking part in merging. */
  static constexpr auto Members() {
    return std::tuple{&Layer::series, &Layer::entry, &Layer::filter,
                      &Layer::styles};
  }
};

/**
 * @brief A selected element together with its resolved style.
 *
 * This is what a backend receives for one layer.
 *
 * @tparam X Element type.
 */
template <typename X>
struct Styled {
  X element;                ///< The selected element.
  style::StyleOf<X> style;  ///< Cascaded style completed with the caller's defaults.
};

/**
 * @brief Named layers of a scene; each layer has its own element type.
 *
 * Deep-copied on copy. Names are unique within a set and iterate in name order.
 */
class LayerSet {
 public:
  LayerSet() = default;

  /** @brief Deep copy. */
  LayerSet(const LayerSet& o) {
    for (const auto& [k, v] : o.m_layers) m_layers.emplace(k, v->Clone());
  }
  /** @brief Deep copy assignment. */
  LayerSet& operator=(const LayerSet& o) {
    if (this != &o) {
      LayerSet tmp(o);
      m_layers.swap(tmp.m_layers);
    }
    return *this;
  }
  LayerSet(LayerSet&&) noexcept = default;
  LayerSet& operator=(LayerSet&&) noexcept = default;

  /**
   * @brief Layer @p name of element type @p X, created if missing.
   * @return The layer, or nullptr if a layer of this name exists with another type.
   */
  template <typename X>
  Layer<X>* Add(const std::string& name) {
    auto& slot = m_layers[name];
    if (!slot) slot = std::make_unique<Holder<X>>();
    if (slot->Type() != std::type_index(typeid(X))) return nullptr;
    return &static_cast<Holder<X>&>(*slot).layer;
  }

  /** @brief Layer @p name of type @p X, or nullptr if absent or of another type. */
  template <typename X>
  [[nodiscard]] Layer<X>* Find(const std::string& name) {
    auto it = m_layers.find(name);
    if (it == m_layers.end() || it->second->Type() != std::type_index(typeid(X)))
      return nullptr;
    return &static_cast<Holder<X>&>(*it->second).layer;
  }

  /** @brief Const overload of @ref Find. */
  template <typename X>
  [[nodiscard]] const Layer<X>* Find(const std::string& name) const {
    auto it = m_layers.find(name);
    if (it == m_layers.end() || it->second->Type() != std::type_index(typeid(X)))
      return nullptr;
    return &static_cast<const Holder<X>&>(*it->second).layer;
  }

  /**
   * @brief Visits every layer of element type @p X in name order.
   *
   * Backends call this once per element type they support.
   *
   * @param f Callable `void(const std::string& name, const Layer<X>&)`.
   */
  template <typename X, typename F>
  void ForEach(F&& f) const {
    for (const auto& [name, slot] : m_layers)
      if (slot->Type() == std::type_index(typeid(X)))
        f(name, static_cast<const Holder<X>&>(*slot).layer);
  }

  /** @brief Names of all layers, in order. */
  [[nodiscard]] std::vector<std::string> Names() const {
    std::vector<std::string> out;
    out.reserve(m_layers.size());
    for (const auto& [name, slot] : m_layers) out.push_back(name);
    return out;
  }

  /**
   * @brief Checks that every layer points at an existing, unambiguous series
   *        and that its entry is in range.
   * @param data   Session data to check against.
   * @param errors Receives one message per violation.
   */
  void Validate(const data::SessionData& data,
                std::vector<std::string>& errors) const {
    for (const auto& [name, slot] : m_layers) slot->Validate(data, name, errors);
  }

  /** @brief `TakeRef`: layers in @p b replace or merge into those of @p a by name. */
  friend void TakeRefInto(LayerSet& a, const LayerSet& b) {
    for (const auto& [k, v] : b.m_layers) {
      auto it = a.m_layers.find(k);
      if (it == a.m_layers.end() || it->second->Type() != v->Type())
        a.m_layers[k] = v->Clone();
      else
        it->second->TakeRefFrom(*v);
    }
  }

  /** @brief `CompleteBy`: layers missing in @p a are added; same-named layers are completed. */
  friend void CompleteInto(LayerSet& a, const LayerSet& b) {
    for (const auto& [k, v] : b.m_layers) {
      auto it = a.m_layers.find(k);
      if (it == a.m_layers.end())
        a.m_layers[k] = v->Clone();
      else if (it->second->Type() == v->Type())
        it->second->CompleteFrom(*v);
    }
  }

 private:
  struct Slot {
    virtual ~Slot() = default;
    virtual std::type_index Type() const = 0;
    virtual std::unique_ptr<Slot> Clone() const = 0;
    virtual void TakeRefFrom(const Slot& other) = 0;
    virtual void CompleteFrom(const Slot& other) = 0;
    virtual void Validate(const data::SessionData& data,
                          const std::string& name,
                          std::vector<std::string>& errors) const = 0;
  };

  template <typename X>
  struct Holder final : Slot {
    Layer<X> layer;

    std::type_index Type() const override { return std::type_index(typeid(X)); }
    std::unique_ptr<Slot> Clone() const override {
      return std::make_unique<Holder>(*this);
    }
    void TakeRefFrom(const Slot& o) override {
      layer = layer.TakeRef(static_cast<const Holder&>(o).layer);
    }
    void CompleteFrom(const Slot& o) override {
      layer = layer.CompleteBy(static_cast<const Holder&>(o).layer);
    }
    void Validate(const data::SessionData& data, const std::string& name,
                  std::vector<std::string>& errors) const override {
      if (!layer.series) {
        errors.push_back("layer '" + name + "' has no series");
        return;
      }
      const auto& ref = *layer.series;
      switch (data.Status<X>(ref)) {
        case data::Lookup::Missing:
          errors.push_back("layer '" + name + "': unknown series '" + ref.name + "'");
          break;
        case data::Lookup::Ambiguous:
          errors.push_back("layer '" + name + "': series '" + ref.name +
                           "' exists in several sources; set the source");
          break;
        case data::Lookup::Found:
          if (layer.entry.value_or(0) >= data.Size<X>(ref))
            errors.push_back("layer '" + name + "': entry out of range");
          break;
      }
    }
  };

  std::map<std::string, std::unique_ptr<Slot>> m_layers;
};

}  // namespace fccvis::scene::meta

namespace fccvis::scene {

/** @brief Shared camera handle. */
using CameraPtr = std::shared_ptr<fccvis::scene::camera::Camera>;

/**
 * @class Scene
 * @brief Unit of visualization: view, appearance and layers.
 *
 * Merging keeps the receiver's name; shared handles (camera, filters) are
 * aliased, not cloned.
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

  /** @brief Camera defining the viewpoint; may be shared between scenes. */
  CameraPtr camera;

  /** @brief Scene-specific camera behaviour (fixed, animated, ...). */
  camera::CameraOptions cameraOptions;

  /** @brief Scene-wide appearance: background, axes, grid. */
  style::EnvironmentStyle environment;

  /** @brief Layers: which series are shown, selected and styled how. */
  meta::LayerSet layers;

  /**
   * @brief Sets the scene camera.
   * @param c Camera to associate; may be shared with other scenes.
   */
  void SetCamera(CameraPtr c) { camera = std::move(c); }

  /**
   * @brief Adds a layer showing a series, or re-targets an existing one.
   *
   * @tparam X Element type of the series.
   * @param name   Layer name, unique within the scene.
   * @param series Series to show.
   * @param entry  Entry to draw.
   * @return The layer, or nullptr if @p name is taken by a layer of another type.
   */
  template <typename X>
  meta::Layer<X>* AddLayer(const std::string& name, data::SeriesRef series,
                           std::size_t entry = 0) {
    auto* layer = layers.Add<X>(name);
    if (layer) {
      layer->series = std::move(series);
      layer->entry = entry;
    }
    return layer;
  }

  /**
   * @brief Moves a layer's cursor.
   * @return False if the layer does not exist or has another type.
   */
  template <typename X>
  bool SetEntry(const std::string& name, std::size_t entry) {
    auto* layer = layers.Find<X>(name);
    if (!layer) return false;
    layer->entry = entry;
    return true;
  }

  /**
   * @brief Selects the elements of a layer.
   *
   * Non-mutating: session data is untouched and the elements come from the
   * session's entry cache. Passes everything if the layer has no filter.
   *
   * @tparam X Element type.
   * @param data      Session data to read from.
   * @param layerName Name of the layer.
   * @return The selected elements; empty if the layer or entry is unavailable.
   */
  template <typename X>
  [[nodiscard]] std::vector<X> Select(const data::SessionData& data,
                                      const std::string& layerName) const {
    const auto* layer = layers.Find<X>(layerName);
    if (!layer || !layer->series) return {};
    auto entry = data.Entry<X>(*layer->series, layer->entry.value_or(0));
    if (!entry) return {};
    return layer->filter ? layer->filter->Apply(*entry) : *entry;
  }

  /**
   * @brief Selects the elements of a layer and resolves their styles.
   *
   * This is the call a backend makes per layer. Styles are cascaded from the
   * layer's rules and completed with @p defaults, which the backend supplies.
   *
   * @tparam X Element type.
   * @param data      Session data to read from.
   * @param layerName Name of the layer.
   * @param defaults  Lowest-priority style, typically the backend defaults.
   * @return Selected elements with their resolved styles.
   */
  template <typename X>
  [[nodiscard]] std::vector<meta::Styled<X>>
  Prepare(const data::SessionData& data, const std::string& layerName,
          const style::StyleOf<X>& defaults = {}) const {
    std::vector<meta::Styled<X>> out;
    const auto* layer = layers.Find<X>(layerName);
    if (!layer || !layer->series) return out;
    auto entry = data.Entry<X>(*layer->series, layer->entry.value_or(0));
    if (!entry) return out;
    for (const X& x : *entry) {
      if (!layer->Passes(x)) continue;
      out.push_back({x, layer->styles.Resolve(x).CompleteBy(defaults)});
    }
    return out;
  }

  /**
   * @brief Checks the scene's layers against the session data.
   * @param data Session data to check against.
   * @return One message per violation; empty if the scene is valid.
   */
  [[nodiscard]] std::vector<std::string>
  Validate(const data::SessionData& data) const {
    std::vector<std::string> errors;
    layers.Validate(data, errors);
    return errors;
  }

  /**
   * @brief Members taking part in merging.
   *
   * The name is deliberately absent: merging never changes a scene's identity.
   */
  static constexpr auto Members() {
    return std::tuple{&Scene::camera, &Scene::cameraOptions,
                      &Scene::environment, &Scene::layers};
  }
};

}  // namespace fccvis::scene
