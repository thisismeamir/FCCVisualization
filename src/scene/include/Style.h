#pragma once

/**
 * @file Style.h
 * @brief Backend-independent styling: partial style records, typed style sheets.
 *
 * Styling follows a rule model: a @ref StyleRule pairs a Filter with a partial
 * style ("elements passing this test get this style"). Rules cascade, later
 * rules override earlier ones, and the result is completed by backend defaults:
 *
 *     resolved = sheet.Resolve(x).CompleteBy(defaults)
 *
 * Styles are typed per drawable type, like filters. Each drawable type maps to
 * one style record through @ref StyleTraits.
 */

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

#include "Filter.h"     // fccvis::Filter<X>
#include "Mergeable.h"  // fccvis::merge::Mergeable

namespace fccvis::data {
// PLACEHOLDERS: replace with the real drawable types / includes.
struct Hit;       ///< Tracker hit.
struct Vertex;    ///< Reconstructed or MC vertex.
struct Track;     ///< Reconstructed track.
struct Particle;  ///< MC particle.
struct Cluster;   ///< Calorimeter cluster.
struct CaloCell;  ///< Calorimeter cell / hit.
struct Jet;       ///< Reconstructed jet.
struct Volume;    ///< Geometry volume.
}  // namespace fccvis::data

namespace fccvis::style {

/**
 * @brief 8-bit RGBA color.
 */
struct Color {
  std::uint8_t r = 0;    ///< Red channel.
  std::uint8_t g = 0;    ///< Green channel.
  std::uint8_t b = 0;    ///< Blue channel.
  std::uint8_t a = 255;  ///< Alpha channel (255 is opaque).

  /**
   * @brief Builds a color from a 0xRRGGBB value.
   * @param rgb Packed color, for example 0x7017FF.
   */
  [[nodiscard]] static constexpr Color FromHex(std::uint32_t rgb) {
    return Color{static_cast<std::uint8_t>((rgb >> 16) & 0xFF),
                 static_cast<std::uint8_t>((rgb >> 8) & 0xFF),
                 static_cast<std::uint8_t>(rgb & 0xFF), 255};
  }
};

/** @brief Line dash pattern. */
enum class DashPattern { Solid, Dashed, Dotted, DashDot };

/** @brief Point marker shape. */
enum class MarkerShape { Circle, Square, Diamond, Triangle, Cross };

/**
 * @brief Properties shared by every drawable.
 *
 * All fields are optional; an undefined field means "not specified here".
 */
struct CommonStyle : merge::Mergeable<CommonStyle> {
  std::optional<Color> color;    ///< Primary color (fill color for surfaces).
  std::optional<float> opacity;  ///< Opacity in [0, 1], multiplied with the color alpha.
  std::optional<bool> visible;   ///< False hides the element without removing it from the selection.

  /** @brief Members taking part in merging. */
  static constexpr auto Members() {
    return std::tuple{&CommonStyle::color, &CommonStyle::opacity,
                      &CommonStyle::visible};
  }
};

/**
 * @brief Style of point-like drawables.
 */
struct PointStyle : merge::Mergeable<PointStyle> {
  CommonStyle common;                  ///< Shared properties.
  std::optional<float> size;           ///< Marker size in logical pixels.
  std::optional<MarkerShape> marker;   ///< Marker shape.

  /** @brief Members taking part in merging. */
  static constexpr auto Members() {
    return std::tuple{&PointStyle::common, &PointStyle::size,
                      &PointStyle::marker};
  }
};

/**
 * @brief Style of line-like drawables.
 */
struct LineStyle : merge::Mergeable<LineStyle> {
  CommonStyle common;                ///< Shared properties.
  std::optional<float> width;        ///< Line width in logical pixels.
  std::optional<DashPattern> dash;   ///< Dash pattern.

  /** @brief Members taking part in merging. */
  static constexpr auto Members() {
    return std::tuple{&LineStyle::common, &LineStyle::width, &LineStyle::dash};
  }
};

/**
 * @brief Style of surface- or volume-like drawables.
 *
 * @ref CommonStyle::color is the fill color.
 */
struct SurfaceStyle : merge::Mergeable<SurfaceStyle> {
  CommonStyle common;                 ///< Shared properties (fill color, opacity, visibility).
  std::optional<Color> edgeColor;     ///< Outline color.
  std::optional<float> edgeWidth;     ///< Outline width in logical pixels.
  std::optional<bool> wireframe;      ///< Draw outline only, no fill.

  /** @brief Members taking part in merging. */
  static constexpr auto Members() {
    return std::tuple{&SurfaceStyle::common, &SurfaceStyle::edgeColor,
                      &SurfaceStyle::edgeWidth, &SurfaceStyle::wireframe};
  }
};

/**
 * @brief Scene-wide appearance that is not tied to any drawable type.
 */
struct EnvironmentStyle : merge::Mergeable<EnvironmentStyle> {
  std::optional<Color> background;  ///< Background color.
  std::optional<bool> axes;         ///< Show coordinate axes.
  std::optional<bool> grid;         ///< Show reference grid.

  /** @brief Members taking part in merging. */
  static constexpr auto Members() {
    return std::tuple{&EnvironmentStyle::background, &EnvironmentStyle::axes,
                      &EnvironmentStyle::grid};
  }
};

/**
 * @brief Maps a drawable type to its style record.
 *
 * Specialize once per drawable type. Several drawable types may share a style
 * record; style sheets remain separate per drawable type.
 */
template <typename X>
struct StyleTraits;

/** @brief Style record of drawable type @p X. */
template <typename X>
using StyleOf = typename StyleTraits<X>::type;

template <> struct StyleTraits<data::Hit>      { using type = PointStyle; };
template <> struct StyleTraits<data::Vertex>   { using type = PointStyle; };
template <> struct StyleTraits<data::Cluster>  { using type = PointStyle; };
template <> struct StyleTraits<data::Track>    { using type = LineStyle; };
template <> struct StyleTraits<data::Particle> { using type = LineStyle; };
template <> struct StyleTraits<data::CaloCell> { using type = SurfaceStyle; };
template <> struct StyleTraits<data::Jet>      { using type = SurfaceStyle; };
template <> struct StyleTraits<data::Volume>   { using type = SurfaceStyle; };

/**
 * @brief "Elements passing @ref when get @ref style."
 *
 * @tparam X Drawable type.
 */
template <typename X>
struct StyleRule {
  filter::Filter<X> when;      ///< Which elements the rule applies to.
  StyleOf<X> style;    ///< Partial style applied to matching elements.
};

/**
 * @brief Ordered style rules for one drawable type.
 *
 * Resolution is a cascade: start from @ref base, then apply every matching
 * rule in order, later rules overriding earlier ones for the fields they
 * define. The result may be partial; complete it with backend defaults.
 *
 * Merging two sheets: @c TakeRef appends the other sheet's rules (they win the
 * cascade) and @c CompleteBy prepends them (this sheet's rules win).
 *
 * @tparam X Drawable type.
 */
template <typename X>
class StyleSheet {
 public:
  StyleOf<X> base;                  ///< Style applying to every element.
  std::vector<StyleRule<X>> rules;  ///< Rules in cascade order.

  /**
   * @brief Appends a rule.
   * @param when  Filter selecting the elements.
   * @param style Partial style applied to them.
   */
  void Add(filter::Filter<X> when, StyleOf<X> style) {
    rules.push_back({std::move(when), std::move(style)});
  }

  /**
   * @brief Resolves the style of one element.
   * @param x Element to style.
   * @return The cascaded, possibly partial style.
   */
  [[nodiscard]] StyleOf<X> Resolve(const X& x) const {
    StyleOf<X> s = base;
    for (const auto& r : rules)
      if (r.when.DoesPass(x)) s = s.TakeRef(r.style);
    return s;
  }

  /** @brief Merge policy for `TakeRef`: other's base and rules override. */
  friend void TakeRefInto(StyleSheet& a, const StyleSheet& b) {
    a.base = a.base.TakeRef(b.base);
    a.rules.insert(a.rules.end(), b.rules.begin(), b.rules.end());
  }

  /** @brief Merge policy for `CompleteBy`: this sheet's base and rules keep priority. */
  friend void CompleteInto(StyleSheet& a, const StyleSheet& b) {
    a.base = a.base.CompleteBy(b.base);
    a.rules.insert(a.rules.begin(), b.rules.begin(), b.rules.end());
  }
};

/**
 * @brief One style sheet per drawable type, keyed by type.
 *
 * Deep-copied on copy. A drawable type without a sheet resolves to an empty
 * style.
 */
class StyleSet {
 public:
  StyleSet() = default;

  /** @brief Deep copy: every sheet is cloned. */
  StyleSet(const StyleSet& o) {
    for (const auto& [k, v] : o.sheets_) sheets_.emplace(k, v->Clone());
  }
  /** @brief Deep copy assignment. */
  StyleSet& operator=(const StyleSet& o) {
    if (this != &o) {
      StyleSet tmp(o);
      sheets_.swap(tmp.sheets_);
    }
    return *this;
  }
  StyleSet(StyleSet&&) noexcept = default;
  StyleSet& operator=(StyleSet&&) noexcept = default;

  /**
   * @brief Sheet for drawable type @p X, created if missing.
   * @tparam X Drawable type.
   */
  template <typename X>
  StyleSheet<X>& For() {
    auto& slot = sheets_[std::type_index(typeid(X))];
    if (!slot) slot = std::make_unique<Holder<X>>();
    return static_cast<Holder<X>&>(*slot).sheet;
  }

  /**
   * @brief Sheet for drawable type @p X, or nullptr if none exists.
   * @tparam X Drawable type.
   */
  template <typename X>
  [[nodiscard]] const StyleSheet<X>* Find() const {
    auto it = sheets_.find(std::type_index(typeid(X)));
    return it == sheets_.end()
               ? nullptr
               : &static_cast<const Holder<X>&>(*it->second).sheet;
  }

  /**
   * @brief Resolves the style of an element and completes it with defaults.
   * @tparam X Drawable type.
   * @param x        Element to style.
   * @param defaults Lowest-priority style, typically the backend defaults.
   */
  template <typename X>
  [[nodiscard]] StyleOf<X> Resolve(const X& x,
                                   const StyleOf<X>& defaults = {}) const {
    const auto* s = Find<X>();
    return (s ? s->Resolve(x) : StyleOf<X>{}).CompleteBy(defaults);
  }

  /** @brief Per-type `TakeRef`: sheets present in @p b override. */
  friend void TakeRefInto(StyleSet& a, const StyleSet& b) {
    for (const auto& [k, v] : b.sheets_) {
      auto it = a.sheets_.find(k);
      if (it == a.sheets_.end()) a.sheets_.emplace(k, v->Clone());
      else it->second->TakeRefFrom(*v);
    }
  }

  /** @brief Per-type `CompleteBy`: this set keeps priority. */
  friend void CompleteInto(StyleSet& a, const StyleSet& b) {
    for (const auto& [k, v] : b.sheets_) {
      auto it = a.sheets_.find(k);
      if (it == a.sheets_.end()) a.sheets_.emplace(k, v->Clone());
      else it->second->CompleteFrom(*v);
    }
  }

 private:
  struct SheetBase {
    virtual ~SheetBase() = default;
    virtual std::unique_ptr<SheetBase> Clone() const = 0;
    virtual void TakeRefFrom(const SheetBase& other) = 0;
    virtual void CompleteFrom(const SheetBase& other) = 0;
  };

  template <typename X>
  struct Holder final : SheetBase {
    StyleSheet<X> sheet;
    std::unique_ptr<SheetBase> Clone() const override {
      return std::make_unique<Holder>(*this);
    }
    void TakeRefFrom(const SheetBase& o) override {
      TakeRefInto(sheet, static_cast<const Holder&>(o).sheet);
    }
    void CompleteFrom(const SheetBase& o) override {
      CompleteInto(sheet, static_cast<const Holder&>(o).sheet);
    }
  };

  std::map<std::type_index, std::unique_ptr<SheetBase>> sheets_;
};

}  // namespace fccvis::style
