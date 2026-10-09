#pragma once
/**
 * @file CameraChanges.h
 * @brief Changes of a scene's camera.
 *
 * ChangeCamera is the generalized change (any subset of position, direction
 * and magnification); the others are its localized forms and share its
 * validation. Backends report manual camera moves by submitting ChangeCamera
 * through their Port.
 *
 * The camera object may be shared by several scenes; every scene holding it
 * is marked as changed (see Session::Commit).
 */

#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "SceneChanges.h"

namespace fccvis::changes {

namespace detail {

inline scene::camera::Camera &RequireCamera(scene::Scene &s, const std::string &sceneName) {
  if (!s.camera) throw ChangeError("scene '" + sceneName + "' has no camera");
  return *s.camera;
}

// A vector must keep the dimension of the one it replaces and be finite.
inline void CheckVector(const std::vector<double> &v, std::size_t dim,
                        const char *what, bool nonZero) {
  if (v.size() != dim)
    throw ChangeError(std::string(what) + " must have " + std::to_string(dim) + " components");
  bool any = false;
  for (double x : v) {
    if (!std::isfinite(x)) throw ChangeError(std::string(what) + " must be finite");
    any = any || x != 0.0;
  }
  if (nonZero && !any) throw ChangeError(std::string(what) + " must not be zero");
}

inline void CheckScale(double scale) {
  if (!std::isfinite(scale) || scale <= 0.0)
    throw ChangeError("magnification must be positive and finite");
}

inline std::string Fmt(const std::vector<double> &v) {
  std::string s = "(";
  for (std::size_t i = 0; i < v.size(); ++i) s += (i ? ", " : "") + std::to_string(v[i]);
  return s + ")";
}

}  // namespace detail

/**
 * @brief Changes any subset of position, direction and magnification.
 *
 * All given values are validated before anything is modified.
 */
class ChangeCamera final : public SceneChange {
 public:
  ChangeCamera(std::string scene, std::optional<std::vector<double>> position,
               std::optional<std::vector<double>> direction,
               std::optional<double> magnification = std::nullopt)
      : SceneChange(std::move(scene)), m_position(std::move(position)),
        m_direction(std::move(direction)), m_magnification(magnification) {}

  [[nodiscard]] std::string Name() const override { return "ChangeCamera"; }
  [[nodiscard]] std::string Describe() const override {
    std::string s = "ChangeCamera(" + SceneName();
    if (m_position) s += ", position=" + detail::Fmt(*m_position);
    if (m_direction) s += ", direction=" + detail::Fmt(*m_direction);
    if (m_magnification) s += ", magnification=" + std::to_string(*m_magnification);
    return s + ")";
  }
  [[nodiscard]] Footprint Touches() const override {
    return Footprint::Of(SceneName(), Part::Camera);
  }

 protected:
  void ApplyToScene(scene::Scene &s, Workspace &) const override {
    auto &cam = detail::RequireCamera(s, SceneName());
    if (m_position) detail::CheckVector(*m_position, cam.positionVector.size(), "position", false);
    if (m_direction) detail::CheckVector(*m_direction, cam.directionVector.size(), "direction", true);
    if (m_magnification) detail::CheckScale(*m_magnification);

    if (m_position) cam.ChangeCameraPosition(*m_position);
    if (m_direction) cam.ChangeCameraDirection(*m_direction);
    if (m_magnification) cam.Magnification(*m_magnification);
  }

 private:
  std::optional<std::vector<double>> m_position;
  std::optional<std::vector<double>> m_direction;
  std::optional<double> m_magnification;
};

/** @brief Changes the camera position. */
class ChangeCameraPosition final : public SceneChange {
 public:
  ChangeCameraPosition(std::string scene, std::vector<double> position)
      : SceneChange(std::move(scene)), m_position(std::move(position)) {}
  [[nodiscard]] std::string Name() const override { return "ChangeCameraPosition"; }
  [[nodiscard]] std::string Describe() const override {
    return "ChangeCameraPosition(" + SceneName() + ", " + detail::Fmt(m_position) + ")";
  }
  [[nodiscard]] Footprint Touches() const override { return Footprint::Of(SceneName(), Part::Camera); }
 protected:
  void ApplyToScene(scene::Scene &s, Workspace &) const override {
    auto &cam = detail::RequireCamera(s, SceneName());
    detail::CheckVector(m_position, cam.positionVector.size(), "position", false);
    cam.ChangeCameraPosition(m_position);
  }
 private:
  std::vector<double> m_position;
};

/** @brief Changes the camera viewing direction. */
class ChangeCameraDirection final : public SceneChange {
 public:
  ChangeCameraDirection(std::string scene, std::vector<double> direction)
      : SceneChange(std::move(scene)), m_direction(std::move(direction)) {}
  [[nodiscard]] std::string Name() const override { return "ChangeCameraDirection"; }
  [[nodiscard]] std::string Describe() const override {
    return "ChangeCameraDirection(" + SceneName() + ", " + detail::Fmt(m_direction) + ")";
  }
  [[nodiscard]] Footprint Touches() const override { return Footprint::Of(SceneName(), Part::Camera); }
 protected:
  void ApplyToScene(scene::Scene &s, Workspace &) const override {
    auto &cam = detail::RequireCamera(s, SceneName());
    detail::CheckVector(m_direction, cam.directionVector.size(), "direction", true);
    cam.ChangeCameraDirection(m_direction);
  }
 private:
  std::vector<double> m_direction;
};

/** @brief Sets the magnification factor. */
class ChangeCameraMagnification final : public SceneChange {
 public:
  ChangeCameraMagnification(std::string scene, double scale)
      : SceneChange(std::move(scene)), m_scale(scale) {}
  [[nodiscard]] std::string Name() const override { return "ChangeCameraMagnification"; }
  [[nodiscard]] std::string Describe() const override {
    return "ChangeCameraMagnification(" + SceneName() + ", " + std::to_string(m_scale) + ")";
  }
  [[nodiscard]] Footprint Touches() const override { return Footprint::Of(SceneName(), Part::Camera); }
 protected:
  void ApplyToScene(scene::Scene &s, Workspace &) const override {
    auto &cam = detail::RequireCamera(s, SceneName());
    detail::CheckScale(m_scale);
    cam.Magnification(m_scale);
  }
 private:
  double m_scale;
};

/** @brief Resets the magnification factor to its default. */
class ResetCameraMagnification final : public SceneChange {
 public:
  explicit ResetCameraMagnification(std::string scene) : SceneChange(std::move(scene)) {}
  [[nodiscard]] std::string Name() const override { return "ResetCameraMagnification"; }
  [[nodiscard]] std::string Describe() const override {
    return "ResetCameraMagnification(" + SceneName() + ")";
  }
  [[nodiscard]] Footprint Touches() const override { return Footprint::Of(SceneName(), Part::Camera); }
 protected:
  void ApplyToScene(scene::Scene &s, Workspace &) const override {
    detail::RequireCamera(s, SceneName()).ResetMagnification();
  }
};

}  // namespace fccvis::changes
