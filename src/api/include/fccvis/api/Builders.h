/**
 * @file Builders.h
 * @brief Fluent, value-based builders that produce Scripts of changes.
 *
 * Builders only construct changes from the existing catalogue; running them is
 * always Session::Resolve. Example:
 *
 * @code
 * using namespace fccvis::api;
 * Script s = Scene("xy").Create()
 *                .Direction({0, 0, -1})
 *                .Layer<data::Point>("hits", "calo-hits")
 *                    .Where(stdlib::EnergyAbove(0.5))
 *                    .Style(red)
 *                .Layer<data::Line>("tracks", "tracks");
 * Run(session, s);
 * @endcode
 *
 * `Layer<G>` takes the geometry kind (data::Point, data::Line, data::Surface);
 * the layer shows `data::Element<G>`.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "CameraChanges.h"
#include "SceneChanges.h"
#include "fccvis/api/Script.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fccvis::api {

template <typename G>
class LayerBuilder;

/** @brief Builds changes for one scene. Converts to Script. */
class SceneBuilder {
 public:
  /** @param name Scene name. */
  explicit SceneBuilder(std::string name) : m_name(std::move(name)) {}

  /** @brief Scene name. */
  [[nodiscard]] const std::string& Name() const { return m_name; }

  /** @brief Creates the scene with a camera of its own (named like the scene). */
  SceneBuilder& Create() {
    m_script.Add(changes::CreateScene{m_name});
    m_script.Add(changes::SetCamera{m_name, std::make_shared<scene::camera::Camera>(m_name)});
    return *this;
  }
  /** @brief Removes the scene. */
  SceneBuilder& Remove() {
    m_script.Add(changes::RemoveScene{m_name});
    return *this;
  }
  /** @brief Uses @p camera (possibly shared with other scenes). */
  SceneBuilder& Camera(scene::CameraPtr camera) {
    m_script.Add(changes::SetCamera{m_name, std::move(camera)});
    return *this;
  }
  /** @brief Sets the viewing direction. */
  SceneBuilder& Direction(std::array<double, 3> d) {
    m_script.Add(changes::ChangeCameraDirection{m_name, {d[0], d[1], d[2]}});
    return *this;
  }
  /** @brief Sets the camera position (pan). */
  SceneBuilder& Position(std::array<double, 3> p) {
    m_script.Add(changes::ChangeCameraPosition{m_name, {p[0], p[1], p[2]}});
    return *this;
  }
  /** @brief Sets the magnification. */
  SceneBuilder& Zoom(double magnification) {
    m_script.Add(changes::ChangeCameraMagnification{m_name, magnification});
    return *this;
  }
  /** @brief Resets the magnification. */
  SceneBuilder& ResetZoom() {
    m_script.Add(changes::ResetCameraMagnification{m_name});
    return *this;
  }
  /** @brief Camera mode (free, fixed, animated). */
  SceneBuilder& CameraMode(scene::camera::CameraMode mode) {
    scene::camera::CameraOptions o;
    o.mode = mode;
    m_script.Add(changes::SetCameraOptions{m_name, o});
    return *this;
  }
  /** @brief Updates the environment (defined fields only). */
  SceneBuilder& Environment(style::EnvironmentStyle environment) {
    m_script.Add(changes::SetEnvironment{m_name, std::move(environment)});
    return *this;
  }
  /** @brief Removes a layer. */
  SceneBuilder& RemoveLayer(const std::string& layer) {
    m_script.Add(changes::RemoveLayer{m_name, layer});
    return *this;
  }

  /**
   * @brief Adds (or re-targets) a layer showing a series of `data::Element<G>`.
   * @param layer  Layer name.
   * @param series Series name (unique across sources) or full reference.
   * @param entry  Entry to show.
   */
  template <typename G>
  LayerBuilder<G> Layer(std::string layer, data::SeriesRef series, std::size_t entry = 0) const&;
  /** @copydoc Layer */
  template <typename G>
  LayerBuilder<G> Layer(std::string layer, data::SeriesRef series, std::size_t entry = 0) &&;
  /** @brief Shorthand: series given by name only. */
  template <typename G>
  LayerBuilder<G> Layer(std::string layer, std::string series, std::size_t entry = 0) const& {
    return Layer<G>(std::move(layer), data::SeriesRef{std::move(series), {}}, entry);
  }
  /** @brief Shorthand: series given by name only (rvalue overload). */
  template <typename G>
  LayerBuilder<G> Layer(std::string layer, std::string series, std::size_t entry = 0) && {
    return std::move(*this).template Layer<G>(std::move(layer), data::SeriesRef{std::move(series), {}}, entry);
  }

  /** @brief Changes accumulated so far. */
  [[nodiscard]] const Script& Changes() const { return m_script; }
  /** @brief The script. */
  operator Script() const { return m_script; }  // NOLINT(google-explicit-constructor)

 private:
  template <typename G>
  friend class LayerBuilder;
  std::string m_name;
  Script m_script;
};

/** @brief Starts building changes for scene @p name. */
inline SceneBuilder Scene(std::string name) { return SceneBuilder(std::move(name)); }

/**
 * @brief Builds one layer: series, entry, filter, style rules. Chains back
 *        into the scene with Layer<G2>() and converts to Script.
 */
template <typename G>
class LayerBuilder {
 public:
  /** @brief Element type shown by the layer. */
  using X = data::Element<G>;

  /** @brief Continues @p scene with a new layer. */
  LayerBuilder(SceneBuilder scene, std::string layer, data::SeriesRef series, std::size_t entry)
      : m_scene(std::move(scene)), m_layer(std::move(layer)) {
    m_own.Add(changes::AddLayer<X>{m_scene.Name(), m_layer, std::move(series), entry});
  }

  /** @brief Shows another entry. */
  LayerBuilder& Entry(std::size_t entry) {
    m_own.Add(changes::SetEntry<X>{m_scene.Name(), m_layer, entry});
    return *this;
  }
  /** @brief Sets the selection filter (a stdlib Selector converts implicitly). */
  LayerBuilder& Where(filter::Filter<X> f) {
    m_own.Add(changes::SetLayerFilter<X>{m_scene.Name(), m_layer, std::make_shared<filter::Filter<X>>(std::move(f))});
    return *this;
  }
  /** @brief Clears the selection filter. */
  LayerBuilder& All() {
    m_own.Add(changes::SetLayerFilter<X>{m_scene.Name(), m_layer, nullptr});
    return *this;
  }
  /** @brief Style for every element (a rule matching all). */
  LayerBuilder& Style(style::StyleOf<X> s) { return Rule({}, std::move(s)); }
  /** @brief Style for the elements @p when selects. */
  LayerBuilder& Rule(filter::Filter<X> when, style::StyleOf<X> s) {
    m_own.Add(changes::AddStyleRule<X>{m_scene.Name(), m_layer, std::move(when), std::move(s)});
    return *this;
  }
  /** @brief Appends a style sheet: its base as a match-all rule, then its rules. */
  LayerBuilder& Sheet(const style::StyleSheet<X>& sheet) {
    Style(sheet.base);
    for (const auto& r : sheet.rules) Rule(r.when, r.style);
    return *this;
  }
  /** @brief Appends rules (for example stdlib::ColorByAttribute). */
  LayerBuilder& Rules(const std::vector<style::StyleRule<X>>& rules) {
    for (const auto& r : rules) Rule(r.when, r.style);
    return *this;
  }

  /** @brief Finishes this layer and starts another one in the same scene. */
  template <typename G2>
  LayerBuilder<G2> Layer(std::string layer, data::SeriesRef series, std::size_t entry = 0) const {
    return LayerBuilder<G2>(Scene(), std::move(layer), std::move(series), entry);
  }
  /** @brief Shorthand: series given by name only. */
  template <typename G2>
  LayerBuilder<G2> Layer(std::string layer, std::string series, std::size_t entry = 0) const {
    return Layer<G2>(std::move(layer), data::SeriesRef{std::move(series), {}}, entry);
  }

  /** @brief The scene builder with this layer's changes appended. */
  [[nodiscard]] SceneBuilder Scene() const {
    SceneBuilder s = m_scene;
    Append(s);
    return s;
  }
  /** @brief All changes: the scene's so far, then this layer's. */
  [[nodiscard]] Script Changes() const {
    Script s = m_scene.Changes();
    s.Append(m_own);
    return s;
  }
  /** @brief The script. */
  operator Script() const { return Changes(); }  // NOLINT(google-explicit-constructor)

 private:
  void Append(SceneBuilder& s) const;
  SceneBuilder m_scene;
  std::string m_layer;
  Script m_own;
};

template <typename G>
void LayerBuilder<G>::Append(SceneBuilder& s) const {
  s.m_script.Append(m_own);
}

template <typename G>
LayerBuilder<G> SceneBuilder::Layer(std::string layer, data::SeriesRef series, std::size_t entry) const& {
  return LayerBuilder<G>(*this, std::move(layer), std::move(series), entry);
}

template <typename G>
LayerBuilder<G> SceneBuilder::Layer(std::string layer, data::SeriesRef series, std::size_t entry) && {
  return LayerBuilder<G>(std::move(*this), std::move(layer), std::move(series), entry);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

/** @brief A layout child with an optional label and weight. */
struct Child {
  scene::layout::LayoutNode node;      ///< The child.
  std::optional<std::string> label{};  ///< Display label.
  int weight = 1;                      ///< Share of a split (ignored in tabs).

  /** @brief Child from a node. */
  Child(scene::layout::LayoutNode n) : node(std::move(n)) {}  // NOLINT(google-explicit-constructor)
  /** @brief Child with label and weight. */
  Child(scene::layout::LayoutNode n, std::string l, int w = 1) : node(std::move(n)), label(std::move(l)), weight(w) {}
};

/** @brief Pane showing a scene. */
inline scene::layout::LayoutNode Pane(std::string scene) {
  return scene::layout::LayoutNode{scene::layout::Pane{std::move(scene)}};
}

/** @brief Split of children side by side (or stacked when @p orientation is Vertical), by weight. */
inline scene::layout::LayoutNode Split(std::vector<Child> children,
                                       scene::layout::Orientation orientation = scene::layout::Orientation::Horizontal) {
  scene::layout::Split s;
  s.orientation = orientation;
  for (auto& c : children) s.children.push_back({std::move(c.node), std::move(c.label), c.weight});
  return scene::layout::LayoutNode{std::move(s)};
}

/** @brief Tabs over children. */
inline scene::layout::LayoutNode Tabs(std::vector<Child> children) {
  scene::layout::Tabs t;
  for (auto& c : children) t.children.push_back({std::move(c.node), std::move(c.label)});
  return scene::layout::LayoutNode{std::move(t)};
}

/** @brief Script setting the session layout. */
inline Script Layout(scene::layout::LayoutNode node) {
  Script s;
  s.Add(changes::SetLayout{std::move(node)});
  return s;
}

}  // namespace fccvis::api
