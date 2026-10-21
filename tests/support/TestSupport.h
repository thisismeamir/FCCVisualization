// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
/**
 * @file TestSupport.h
 * @brief Helpers shared by the session, backend and adapter tests.
 */
#pragma once

#include "BackendRegistry.h"
#include "Data.h"
#include "Elements.h"
#include "Layout.h"
#include "RecordingBackend.h"
#include "Session.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace fccvis::test {

/** @brief In-memory series; counts loads; may grow (live source). */
template <typename X>
class VectorSeries final : public data::Series<X> {
 public:
  explicit VectorSeries(std::vector<std::vector<X>> entries,
                        std::shared_ptr<int> loads = std::make_shared<int>(0))
      : m_entries(std::make_shared<Store>()), m_loads(std::move(loads)) {
    m_entries->entries = std::move(entries);
  }

  std::size_t Size() const override {
    std::lock_guard<std::mutex> lock(m_entries->mutex);
    return m_entries->entries.size();
  }
  std::vector<X> Load(std::size_t entry) const override {
    std::lock_guard<std::mutex> lock(m_entries->mutex);
    ++*m_loads;
    return m_entries->entries.at(entry);
  }

  /** @brief Handle that appends entries after registration (live source). */
  struct Appender {
    std::shared_ptr<void> keep;
    std::function<void(std::vector<X>)> append;
  };
  Appender MakeAppender() const {
    auto store = m_entries;
    return {store, [store](std::vector<X> e) {
              std::lock_guard<std::mutex> lock(store->mutex);
              store->entries.push_back(std::move(e));
            }};
  }

 private:
  struct Store {
    std::mutex mutex;
    std::vector<std::vector<X>> entries;
  };
  std::shared_ptr<Store> m_entries;
  std::shared_ptr<int> m_loads;
};

/** @brief Silences the session log; returns the collected messages. */
inline std::shared_ptr<std::vector<std::pair<changes::Severity, std::string>>>
CaptureLog(session::Session& s) {
  auto log = std::make_shared<std::vector<std::pair<changes::Severity, std::string>>>();
  s.SetLogSink([log](changes::Severity sev, const std::string& m) { log->emplace_back(sev, m); });
  return log;
}

/** @brief Number of captured messages of a severity containing @p text. */
inline int CountLog(const std::vector<std::pair<changes::Severity, std::string>>& log,
                    changes::Severity sev, const std::string& text) {
  int n = 0;
  for (const auto& [s, m] : log)
    if (s == sev && m.find(text) != std::string::npos) ++n;
  return n;
}

/** @brief Registers a fresh recording backend type and attaches an instance. */
inline std::shared_ptr<backend::Recording> AttachRecording(session::Session& s,
                                                           const std::string& id) {
  static int n = 0;
  const std::string type = "test-recording-" + std::to_string(++n);
  auto rec = backend::RegisterRecordingBackend(type);
  s.InitBackend(type, id);
  return rec;
}

/** @brief Points with metadata "e" (energy-like) at x = 0, 1, 2, ... */
inline std::vector<data::PointElement> MakePoints(std::size_t n, double e0 = 0.5) {
  std::vector<data::PointElement> out(n);
  for (std::size_t i = 0; i < n; ++i) {
    out[i].geometry.position = {static_cast<double>(i), 0.0, 0.0};
    out[i].meta.Set("e", e0 + static_cast<double>(i));
  }
  return out;
}

/** @brief A unit square in the z=0 plane, tessellated as two triangles. */
class SquareShape final : public geometry::AbstractShape {
 public:
  std::shared_ptr<geometry::AbstractShape> Clone() const override {
    return std::make_shared<SquareShape>(*this);
  }
  double Criterion(const Vector& p) const override {
    const double d = std::max(std::abs(p[0]) - 0.5, std::abs(p[1]) - 0.5);
    return std::max(d, std::abs(p[2]));
  }
  geometry::Mesh Tessellate(double) const override {
    geometry::Mesh m;
    m.vertices = {{-0.5f, -0.5f, 0.f}, {0.5f, -0.5f, 0.f}, {0.5f, 0.5f, 0.f}, {-0.5f, 0.5f, 0.f}};
    m.triangles = {{0, 1, 2}, {0, 2, 3}};
    return m;
  }
};

/** @brief Surfaces: unit squares translated to x = 0, 2, 4, ... */
inline std::vector<data::SurfaceElement> MakeSurfaces(std::size_t n) {
  std::vector<data::SurfaceElement> out;
  for (std::size_t i = 0; i < n; ++i) {
    geometry::Transform t({2.0 * static_cast<double>(i), 0.0, 0.0},
                          {1, 0, 0, 0, 1, 0, 0, 0, 1});
    data::SurfaceElement e{data::Surface{geometry::Shape(std::make_shared<SquareShape>(), t)}, {}};
    e.meta.Set("volume", "box" + std::to_string(i));
    out.push_back(std::move(e));
  }
  return out;
}

/** @brief Lines: one polyline per entry element from (i,0,0) to (i,1,1). */
inline std::vector<data::LineElement> MakeLines(std::size_t n) {
  std::vector<data::LineElement> out(n);
  for (std::size_t i = 0; i < n; ++i) {
    const double x = static_cast<double>(i);
    out[i].geometry.vertices = {{x, 0, 0}, {x, 0.5, 0.5}, {x, 1, 1}};
    out[i].meta.Set("charge", static_cast<std::int64_t>(i % 2 ? 1 : -1));
  }
  return out;
}

/** @brief Layout helpers: pane, split, tabs, and a compact textual form. */
inline scene::layout::LayoutNode P(std::string n) {
  return scene::layout::LayoutNode{scene::layout::Pane{std::move(n)}};
}
inline scene::layout::LayoutNode S(std::vector<scene::layout::LayoutNode> kids) {
  scene::layout::Split sp;
  for (auto& k : kids) sp.children.push_back({std::move(k), std::nullopt, 1});
  return scene::layout::LayoutNode{std::move(sp)};
}
inline scene::layout::LayoutNode T(std::vector<scene::layout::LayoutNode> kids) {
  scene::layout::Tabs t;
  for (auto& k : kids) t.children.push_back({std::move(k), std::nullopt});
  return scene::layout::LayoutNode{std::move(t)};
}
inline std::string Describe(const scene::layout::LayoutNode& n) {
  return std::visit(
      [](const auto& v) -> std::string {
        using V = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<V, scene::layout::Pane>) {
          return v.sceneName;
        } else {
          std::string out = std::is_same_v<V, scene::layout::Split> ? "S(" : "T(";
          for (std::size_t i = 0; i < v.children.size(); ++i)
            out += (i ? "," : "") + Describe(v.children[i].node);
          return out + ")";
        }
      },
      n.value);
}
inline std::string Describe(const std::optional<scene::layout::LayoutNode>& n) {
  return n ? Describe(*n) : "-";
}

}  // namespace fccvis::test
