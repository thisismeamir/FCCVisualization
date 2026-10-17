/**
 * @file Styles.h
 * @brief Palettes, attribute-driven style rules and style presets.
 *
 * Everything here produces ordinary style data (style::StyleRule lists,
 * style::StyleSheet values, style::EnvironmentStyle). Backends only ever see
 * the resolved, possibly partial, style of each element, completed with their
 * own defaults: `sheet.Resolve(x).CompleteBy(defaults)`.
 *
 * The default figure theme is monochrome with the accent `#7017FF`
 * (CONVENTIONS "Style").
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Elements.h"
#include "Filter.h"
#include "Style.h"
#include "fccvis/stdlib/Filters.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace fccvis::stdlib {

/** @brief Accent colour of the default theme. */
inline constexpr std::uint32_t kAccent = 0x7017FF;

/** @brief Ordered colour stops; continuous (interpolated) or indexed. */
class Palette {
 public:
  /**
   * @param stops Colours, at least one.
   * @throws std::invalid_argument if @p stops is empty.
   */
  explicit Palette(std::vector<style::Color> stops);

  /** @brief Colour at @p t in [0, 1] (clamped), linear between stops. */
  [[nodiscard]] style::Color At(double t) const;
  /** @brief Stop @p i, cycling through the stops. */
  [[nodiscard]] style::Color Index(std::size_t i) const;
  /** @brief Number of stops. */
  [[nodiscard]] std::size_t Size() const { return m_stops.size(); }
  /** @brief The stops. */
  [[nodiscard]] const std::vector<style::Color>& Colors() const { return m_stops; }

 private:
  std::vector<style::Color> m_stops;
};

/** @brief Sequential palette from a light tint of @p accent to @p accent (the default palette). */
[[nodiscard]] Palette Monochrome(style::Color accent = style::Color::FromHex(kAccent));
/** @brief @p n distinguishable colours (a fixed 14-colour set, cycling beyond). */
[[nodiscard]] Palette Categorical(std::size_t n);
/** @brief Two-stop sequential palette. */
[[nodiscard]] Palette Sequential(style::Color lo, style::Color hi);
/** @brief Three-stop diverging palette (default blue, light gray, red). */
[[nodiscard]] Palette Diverging(style::Color lo = style::Color::FromHex(0x0F62FE),
                                style::Color mid = style::Color::FromHex(0xF4F4F4),
                                style::Color hi = style::Color::FromHex(0xDA1E28));

/** @brief Closed value range [lo, hi] used to bin an attribute. */
struct Range {
  double lo = 0;  ///< Lower edge.
  double hi = 1;  ///< Upper edge (> lo).
};

/**
 * @brief Selector for bin @p i of @p bins over @p range of a numeric attribute.
 *
 * Bins are half-open [a, b); the first bin also takes values below the range
 * and the last bin values at or above its upper edge, so every element that
 * has the attribute falls in exactly one bin. Elements without it match none.
 */
[[nodiscard]] Selector Bin(const std::string& attribute, Range range, std::size_t i, std::size_t bins);

/** @brief Sets the primary colour of any style record. */
template <typename S>
void SetColor(S& s, style::Color c) { s.common.color = c; }

/** @brief Sets the "size" of a style record: point size, line width or surface edge width. */
template <typename S>
void SetSize(S& s, float size) {
  if constexpr (std::is_same_v<S, style::PointStyle>) s.size = size;
  else if constexpr (std::is_same_v<S, style::LineStyle>) s.width = size;
  else s.edgeWidth = size;
}

/**
 * @brief One colour rule per bin of a numeric attribute.
 *
 * Bin i gets `palette.At((i + 0.5) / bins)`. Append the rules to a sheet with
 * AddRules or one AddStyleRule change per rule.
 *
 * @throws std::invalid_argument if bins == 0 or range.hi <= range.lo.
 */
template <typename X>
[[nodiscard]] std::vector<style::StyleRule<X>> ColorByAttribute(const std::string& attribute,
                                                               const Palette& palette, Range range,
                                                               std::size_t bins = 16);

/**
 * @brief One size rule per bin of a numeric attribute, linear from @p minSize to @p maxSize.
 * @throws std::invalid_argument if bins == 0 or range.hi <= range.lo.
 */
template <typename X>
[[nodiscard]] std::vector<style::StyleRule<X>> SizeByAttribute(const std::string& attribute, float minSize,
                                                              float maxSize, Range range,
                                                              std::size_t bins = 8);

/** @brief Appends rules to a style sheet in order (later rules win the cascade). */
template <typename X>
void AddRules(style::StyleSheet<X>& sheet, const std::vector<style::StyleRule<X>>& rules) {
  for (const auto& r : rules) sheet.rules.push_back(r);
}

/** @brief Rule giving the elements selected by @p when the accent colour. */
template <typename X>
[[nodiscard]] style::StyleRule<X> Highlight(filter::Filter<X> when,
                                            style::Color accent = style::Color::FromHex(kAccent)) {
  style::StyleOf<X> s;
  SetColor(s, accent);
  s.common.opacity = 1.0f;
  return {std::move(when), s};
}

/** @name Presets */
///@{
/** @brief Detector geometry: thin dark edges, light low-opacity fill. */
[[nodiscard]] style::StyleSheet<data::SurfaceElement> DetectorOutline();
/** @brief Hits coloured (monochrome) and sized by energy over @p range. */
[[nodiscard]] style::StyleSheet<data::PointElement> HitsByEnergy(Range range,
                                                                 const std::string& attribute = kEnergy);
/** @brief Tracks: negative charge blue, positive accent, neutral gray dashed. */
[[nodiscard]] style::StyleSheet<data::LineElement> TracksByCharge(const std::string& attribute = "charge");
/** @brief Everything faded gray (context layers). Apply to every layer of a scene to ghost it. */
template <typename X>
[[nodiscard]] style::StyleSheet<X> Ghosted() {
  style::StyleSheet<X> s;
  SetColor(s.base, style::Color::FromHex(0xA8A8A8));
  s.base.common.opacity = 0.15f;
  return s;
}
/** @brief Publication environment: white background, axes on, no grid. */
[[nodiscard]] style::EnvironmentStyle Publication();
/** @brief Default figure environment of the theme: white background, light grid, no axes. */
[[nodiscard]] style::EnvironmentStyle DefaultEnvironment();
///@}

// ---- template definitions -----------------------------------------------------

namespace detail {
void CheckBinning(Range range, std::size_t bins);
}  // namespace detail

template <typename X>
std::vector<style::StyleRule<X>> ColorByAttribute(const std::string& attribute, const Palette& palette,
                                                  Range range, std::size_t bins) {
  detail::CheckBinning(range, bins);
  std::vector<style::StyleRule<X>> rules;
  for (std::size_t i = 0; i < bins; ++i) {
    style::StyleOf<X> s;
    SetColor(s, palette.At((static_cast<double>(i) + 0.5) / static_cast<double>(bins)));
    rules.push_back({Bin(attribute, range, i, bins), s});
  }
  return rules;
}

template <typename X>
std::vector<style::StyleRule<X>> SizeByAttribute(const std::string& attribute, float minSize, float maxSize,
                                                 Range range, std::size_t bins) {
  detail::CheckBinning(range, bins);
  std::vector<style::StyleRule<X>> rules;
  for (std::size_t i = 0; i < bins; ++i) {
    style::StyleOf<X> s;
    const float t = bins == 1 ? 1.0f : static_cast<float>(i) / static_cast<float>(bins - 1);
    SetSize(s, minSize + (maxSize - minSize) * t);
    rules.push_back({Bin(attribute, range, i, bins), s});
  }
  return rules;
}

}  // namespace fccvis::stdlib
