#pragma once
/**
 * @file SceneChanges.h
 * @brief Built-in changes. New changes are new structs; nothing else changes.
 *
 * Camera changes (directions, magnification, ...) depend on the Camera API and
 * are added next.
 */

#include <string>
#include <utility>
#include "Camera.h"
#include "Changes.h"

namespace fccvis::changes {

/** @brief Base of changes targeting one existing scene. */
class SceneChange : public Change {
 public:
  explicit SceneChange(std::string scene) : m_scene(std::move(scene)) {}

  [[nodiscard]] std::string SceneName() const final { return m_scene; }

  void Apply(Workspace& ws) const final {
    auto it = ws.scenes.find(m_scene);
    if (it == ws.scenes.end() || !it->second)
      throw ChangeError("unknown scene '" + m_scene + "'");
    ApplyToScene(*it->second, ws);
  }

 protected:
  /** @brief Applies the change to the resolved scene. */
  virtual void ApplyToScene(scene::Scene& scene, Workspace& ws) const = 0;

 private:
  std::string m_scene;
};

/** @brief Creates an empty scene. */
class CreateScene final : public Change {
 public:
  explicit CreateScene(std::string name) : m_name(std::move(name)) {}
  [[nodiscard]] std::string Name() const override { return "CreateScene"; }
  [[nodiscard]] std::string Describe() const override { return "CreateScene(" + m_name + ")"; }
  [[nodiscard]] Footprint Touches() const override { return Footprint::Of(m_name, Part::Scenes); }
  void Apply(Workspace& ws) const override {
    if (m_name.empty()) throw ChangeError("scene name is empty");
    if (ws.scenes.count(m_name)) throw ChangeError("scene '" + m_name + "' already exists");
    ws.scenes.emplace(m_name, std::make_shared<scene::Scene>(m_name));
  }
 private:
  std::string m_name;
};

/** @brief Replaces the session layout. */
class SetLayout final : public Change {
 public:
  explicit SetLayout(scene::layout::LayoutNode layout) : m_layout(std::move(layout)) {}
  [[nodiscard]] std::string Name() const override { return "SetLayout"; }
  [[nodiscard]] std::string Describe() const override { return "SetLayout(...)"; }
  [[nodiscard]] Footprint Touches() const override { return Footprint::Of({}, Part::Layout); }
  void Apply(Workspace& ws) const override { ws.layout = m_layout; }
 private:
  scene::layout::LayoutNode m_layout;
};

/** @brief Adds a layer showing a series, or re-targets an existing one. */
template <typename X>
class AddLayer final : public SceneChange {
 public:
  AddLayer(std::string scene, std::string layer, data::SeriesRef series, std::size_t entry = 0)
      : SceneChange(std::move(scene)), m_layer(std::move(layer)),
        m_series(std::move(series)), m_entry(entry) {}
  [[nodiscard]] std::string Name() const override { return "AddLayer"; }
  [[nodiscard]] std::string Describe() const override {
    return "AddLayer(" + SceneName() + ", " + m_layer + ", series=" + m_series.name + ")";
  }
  [[nodiscard]] Footprint Touches() const override {
    return Footprint::Of(SceneName(), Part::LayerSet, {m_layer});
  }
 protected:
  void ApplyToScene(scene::Scene& s, Workspace&) const override {
    if (!s.AddLayer<X>(m_layer, m_series, m_entry))
      throw ChangeError("layer '" + m_layer + "' exists with another element type");
  }
 private:
  std::string m_layer;
  data::SeriesRef m_series;
  std::size_t m_entry;
};

/** @brief Removes a layer. */
class RemoveLayer final : public SceneChange {
 public:
  RemoveLayer(std::string scene, std::string layer)
      : SceneChange(std::move(scene)), m_layer(std::move(layer)) {}
  [[nodiscard]] std::string Name() const override { return "RemoveLayer"; }
  [[nodiscard]] std::string Describe() const override {
    return "RemoveLayer(" + SceneName() + ", " + m_layer + ")";
  }
  [[nodiscard]] Footprint Touches() const override {
    return Footprint::Of(SceneName(), Part::LayerSet, {m_layer});
  }
 protected:
  void ApplyToScene(scene::Scene& s, Workspace&) const override {
    if (!s.layers.Remove(m_layer)) throw ChangeError("unknown layer '" + m_layer + "'");
  }
 private:
  std::string m_layer;
};

/** @brief Moves a layer's entry cursor. */
template <typename X>
class SetEntry final : public SceneChange {
 public:
  SetEntry(std::string scene, std::string layer, std::size_t entry)
      : SceneChange(std::move(scene)), m_layer(std::move(layer)), m_entry(entry) {}
  [[nodiscard]] std::string Name() const override { return "SetEntry"; }
  [[nodiscard]] std::string Describe() const override {
    return "SetEntry(" + SceneName() + ", " + m_layer + ", " + std::to_string(m_entry) + ")";
  }
  [[nodiscard]] Footprint Touches() const override {
    return Footprint::Of(SceneName(), Part::None, {m_layer});
  }
 protected:
  void ApplyToScene(scene::Scene& s, Workspace&) const override {
    if (!s.SetEntry<X>(m_layer, m_entry))
      throw ChangeError("unknown layer '" + m_layer + "' of this element type");
  }
 private:
  std::string m_layer;
  std::size_t m_entry;
};

/** @brief Assigns a layer's selection filter (shared handle; null clears it). */
template <typename X>
class SetLayerFilter final : public SceneChange {
 public:
  SetLayerFilter(std::string scene, std::string layer, scene::meta::FilterHandle<X> filter)
      : SceneChange(std::move(scene)), m_layer(std::move(layer)), m_filter(std::move(filter)) {}
  [[nodiscard]] std::string Name() const override { return "SetLayerFilter"; }
  [[nodiscard]] std::string Describe() const override {
    return "SetLayerFilter(" + SceneName() + ", " + m_layer + (m_filter ? ")" : ", none)");
  }
  [[nodiscard]] Footprint Touches() const override {
    return Footprint::Of(SceneName(), Part::None, {m_layer});
  }
 protected:
  void ApplyToScene(scene::Scene& s, Workspace&) const override {
    auto* layer = s.layers.Find<X>(m_layer);
    if (!layer) throw ChangeError("unknown layer '" + m_layer + "' of this element type");
    layer->filter = m_filter;
  }
 private:
  std::string m_layer;
  scene::meta::FilterHandle<X> m_filter;
};

/** @brief Appends a style rule to a layer's cascade. */
template <typename X>
class AddStyleRule final : public SceneChange {
 public:
  AddStyleRule(std::string scene, std::string layer, filter::Filter<X> when, style::StyleOf<X> style)
      : SceneChange(std::move(scene)), m_layer(std::move(layer)),
        m_when(std::move(when)), m_style(std::move(style)) {}
  [[nodiscard]] std::string Name() const override { return "AddStyleRule"; }
  [[nodiscard]] std::string Describe() const override {
    return "AddStyleRule(" + SceneName() + ", " + m_layer + ")";
  }
  [[nodiscard]] Footprint Touches() const override {
    return Footprint::Of(SceneName(), Part::None, {m_layer});
  }
 protected:
  void ApplyToScene(scene::Scene& s, Workspace&) const override {
    auto* layer = s.layers.Find<X>(m_layer);
    if (!layer) throw ChangeError("unknown layer '" + m_layer + "' of this element type");
    layer->styles.Add(m_when, m_style);
  }
 private:
  std::string m_layer;
  filter::Filter<X> m_when;
  style::StyleOf<X> m_style;
};

/** @brief Sets the camera handle of a scene. */
class SetCamera final : public SceneChange {
 public:
  SetCamera(std::string scene, scene::CameraPtr camera)
      : SceneChange(std::move(scene)), m_camera(std::move(camera)) {}
  [[nodiscard]] std::string Name() const override { return "SetCamera"; }
  [[nodiscard]] std::string Describe() const override { return "SetCamera(" + SceneName() + ")"; }
  [[nodiscard]] Footprint Touches() const override { return Footprint::Of(SceneName(), Part::Camera); }
 protected:
  void ApplyToScene(scene::Scene& s, Workspace&) const override { s.SetCamera(m_camera); }
 private:
  scene::CameraPtr m_camera;
};

/** @brief Updates camera options; only the defined fields are applied. */
class SetCameraOptions final : public SceneChange {
 public:
  SetCameraOptions(std::string scene, scene::camera::CameraOptions options)
      : SceneChange(std::move(scene)), m_options(std::move(options)) {}
  [[nodiscard]] std::string Name() const override { return "SetCameraOptions"; }
  [[nodiscard]] std::string Describe() const override { return "SetCameraOptions(" + SceneName() + ")"; }
  [[nodiscard]] Footprint Touches() const override { return Footprint::Of(SceneName(), Part::CameraOptions); }
 protected:
  void ApplyToScene(scene::Scene& s, Workspace&) const override {
    s.cameraOptions = s.cameraOptions.TakeRef(m_options);
  }
 private:
  scene::camera::CameraOptions m_options;
};

/** @brief Updates the environment style; only the defined fields are applied. */
class SetEnvironment final : public SceneChange {
 public:
  SetEnvironment(std::string scene, style::EnvironmentStyle environment)
      : SceneChange(std::move(scene)), m_environment(std::move(environment)) {}
  [[nodiscard]] std::string Name() const override { return "SetEnvironment"; }
  [[nodiscard]] std::string Describe() const override { return "SetEnvironment(" + SceneName() + ")"; }
  [[nodiscard]] Footprint Touches() const override { return Footprint::Of(SceneName(), Part::Environment); }
 protected:
  void ApplyToScene(scene::Scene& s, Workspace&) const override {
    s.environment = s.environment.TakeRef(m_environment);
  }
 private:
  style::EnvironmentStyle m_environment;
};

}  // namespace fccvis::changes
//
