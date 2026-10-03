/**
 * @file Elements.h
 * @brief Element model: geometry kinds plus metadata.
 *
 * An @ref Element is a geometry (Point, Line, Surface, ...) together with the
 * metadata the source provides for it (energy, charge, ids, ...). Domain
 * meaning (hit, track, cluster) is carried by the series name and
 * description, not by the type, so backends only need to draw the geometry
 * kinds. Adding a kind (for example a plotting kind later) means adding a
 * geometry struct and one StyleTraits specialization.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include "Geometry.h"

namespace fccvis::data {

/** @brief Point geometry. */
struct Point {
  geometry::Transform::Vector position;  ///< Location of the point.

};

/**
 * @brief Polyline geometry.
 *
 * Ordered vertices. How a curve in the source becomes vertices (stored states,
 * sampled helix, ...) is decided by the adapter.
 */
struct Line {
  std::vector<geometry::Transform::Vector> vertices;  ///< Ordered vertices, at least two for a visible line.
};

/**
 * @brief Triangle-mesh surface geometry (placeholder shape).
 *
 * Replace or extend when volumes and shapes need a richer representation.
 */
struct Surface {
  geometry::Shape shape;
};

/**
 * @brief Named values attached to an element.
 *
 * Holds exactly what the source provides; an absent key means "the source does
 * not provide this". Keys are chosen by the adapter and documented in its
 * capabilities.
 */
class Metadata {
 public:
  /** @brief Supported value types. */
  using Value = std::variant<bool, std::int64_t, double, std::string>;

  /** @brief Sets or replaces a value. */
  void Set(std::string key, Value value) {
    m_values.insert_or_assign(std::move(key), std::move(value));
  }

  /** @brief True if the key is present. */
  [[nodiscard]] bool Has(std::string_view key) const {
    return m_values.find(key) != m_values.end();
  }

  /** @brief Value of a key, or nullptr if absent. */
  [[nodiscard]] const Value* Find(std::string_view key) const {
    auto it = m_values.find(key);
    return it == m_values.end() ? nullptr : &it->second;
  }

  /**
   * @brief Numeric value of a key (bool, integer and floating values convert).
   * @return The value, or nullopt if the key is absent or holds text.
   */
  [[nodiscard]] std::optional<double> Number(std::string_view key) const {
    const Value* v = Find(key);
    if (!v) return std::nullopt;
    return std::visit(
        [](const auto& x) -> std::optional<double> {
          if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::string>)
            return std::nullopt;
          else
            return static_cast<double>(x);
        },
        *v);
  }

  /** @brief Text value of a key, or nullopt if absent or not text. */
  [[nodiscard]] std::optional<std::string> Text(std::string_view key) const {
    const Value* v = Find(key);
    if (!v) return std::nullopt;
    if (const auto* s = std::get_if<std::string>(v)) return *s;
    return std::nullopt;
  }

  /** @brief Flag value of a key, or nullopt if absent or not a bool. */
  [[nodiscard]] std::optional<bool> Flag(std::string_view key) const {
    const Value* v = Find(key);
    if (!v) return std::nullopt;
    if (const auto* b = std::get_if<bool>(v)) return *b;
    return std::nullopt;
  }

  /** @brief All values, ordered by key. */
  [[nodiscard]] const std::map<std::string, Value, std::less<>>& All() const {
    return m_values;
  }

 private:
  std::map<std::string, Value, std::less<>> m_values;
};

/**
 * @brief A drawable element: geometry plus metadata.
 *
 * Series, filters and style sheets are typed on this, for example
 * `Filter<Element<Line>>`.
 *
 * @tparam Geometry Geometry kind (Point, Line, Surface, ...).
 */
template <typename Geometry>
struct Element {
  Geometry geometry;  ///< The drawable geometry.
  Metadata meta;      ///< Attributes provided by the source.
};

using PointElement = Element<Point>;      ///< Point element.
using LineElement = Element<Line>;        ///< Polyline element.
using SurfaceElement = Element<Surface>;  ///< Surface element.

}  // namespace fccvis::data
