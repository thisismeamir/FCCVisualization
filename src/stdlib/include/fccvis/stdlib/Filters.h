/**
 * @file Filters.h
 * @brief Standard filters: attributes, energy, selection helpers (and the
 *        Selector type shared by shape and detector filters).
 *
 * Standard filters return a Selector: one filter per built-in element kind
 * (Point, Line, Surface), so the same expression works for any layer. A
 * Selector converts implicitly to `filter::Filter<X>` for the built-in element
 * types, composes with `&`, `|`, `!`, and mixes with existing `Filter<X>`:
 *
 * @code
 * filter::Filter<data::PointElement> f = EnergyAbove(0.5) & !Attr<std::string>("subdetector") == "ECal";
 * @endcode
 *
 * Missing attributes: every comparison fails when the attribute is absent or
 * has an incompatible type (text vs number), including `!=`. Negation is plain
 * logic, so `!EnergyAbove(1)` passes elements that have no energy; combine
 * with Exists() when that matters.
 *
 * Numeric comparisons read `Metadata::Number` (bool, integer and floating
 * values convert to double). Text comparisons read `Metadata::Text`; flags
 * read `Metadata::Flag`.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Elements.h"
#include "Filter.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace fccvis::stdlib {

/** @brief True for the element types a Selector serves. */
template <typename X>
inline constexpr bool kIsBuiltinElement =
    std::is_same_v<X, data::PointElement> || std::is_same_v<X, data::LineElement> ||
    std::is_same_v<X, data::SurfaceElement>;

/**
 * @brief A filter for every built-in element kind at once.
 *
 * Build it from per-kind filters, or from a metadata predicate (FromMetadata).
 */
class Selector {
 public:
  /** @brief Selects everything. */
  Selector() = default;

  /**
   * @brief Selector from one filter per element kind.
   * @param p Filter for points.
   * @param l Filter for lines.
   * @param s Filter for surfaces.
   */
  Selector(filter::Filter<data::PointElement> p, filter::Filter<data::LineElement> l,
           filter::Filter<data::SurfaceElement> s)
      : m_point(std::move(p)), m_line(std::move(l)), m_surface(std::move(s)) {}

  /** @brief The filter for element type @p X. */
  template <typename X>
    requires kIsBuiltinElement<X>
  [[nodiscard]] filter::Filter<X> For() const {
    if constexpr (std::is_same_v<X, data::PointElement>) return m_point;
    else if constexpr (std::is_same_v<X, data::LineElement>) return m_line;
    else return m_surface;
  }

  /** @brief Implicit conversion to the filter of a built-in element type. */
  template <typename X>
    requires kIsBuiltinElement<X>
  operator filter::Filter<X>() const {  // NOLINT(google-explicit-constructor)
    return For<X>();
  }

  /** @brief Tests an element of a built-in kind. */
  template <typename X>
    requires kIsBuiltinElement<X>
  [[nodiscard]] bool DoesPass(const X& x, std::size_t index = filter::kNoIndex) const {
    return For<X>().DoesPass(x, index);
  }

  /** @brief Both selectors pass. */
  friend Selector operator&(const Selector& a, const Selector& b) {
    return {a.m_point & b.m_point, a.m_line & b.m_line, a.m_surface & b.m_surface};
  }
  /** @brief Either selector passes. */
  friend Selector operator|(const Selector& a, const Selector& b) {
    return {a.m_point | b.m_point, a.m_line | b.m_line, a.m_surface | b.m_surface};
  }
  /** @brief The selector does not pass. */
  friend Selector operator!(const Selector& a) { return {!a.m_point, !a.m_line, !a.m_surface}; }

 private:
  filter::Filter<data::PointElement> m_point;
  filter::Filter<data::LineElement> m_line;
  filter::Filter<data::SurfaceElement> m_surface;
};

/** @name Mixing Selector with Filter<X> */
///@{
/** @brief `Filter<X> & Selector`. */
template <typename X>
  requires kIsBuiltinElement<X>
filter::Filter<X> operator&(const filter::Filter<X>& a, const Selector& b) { return a & b.For<X>(); }
/** @brief `Selector & Filter<X>`. */
template <typename X>
  requires kIsBuiltinElement<X>
filter::Filter<X> operator&(const Selector& a, const filter::Filter<X>& b) { return a.For<X>() & b; }
/** @brief `Filter<X> | Selector`. */
template <typename X>
  requires kIsBuiltinElement<X>
filter::Filter<X> operator|(const filter::Filter<X>& a, const Selector& b) { return a | b.For<X>(); }
/** @brief `Selector | Filter<X>`. */
template <typename X>
  requires kIsBuiltinElement<X>
filter::Filter<X> operator|(const Selector& a, const filter::Filter<X>& b) { return a.For<X>() | b; }
///@}

/**
 * @brief Selector from a predicate on the element's metadata.
 * @param pred Called with the element's Metadata.
 */
[[nodiscard]] Selector FromMetadata(std::function<bool(const data::Metadata&)> pred);

/**
 * @brief Selector from a predicate on an element's position in its entry.
 * @param pred Called with the index; elements with an unknown index fail.
 */
[[nodiscard]] Selector FromIndex(std::function<bool(std::size_t)> pred);

/**
 * @brief Named attribute of value type @p T, compared with `==`, `!=`, `<`,
 *        `<=`, `>`, `>=` to build a Selector.
 *
 * @tparam T `double` or `std::int64_t` (numeric), `bool` (flag) or
 *           `std::string` (text).
 */
template <typename T>
class Attr {
  static_assert(std::is_same_v<T, double> || std::is_same_v<T, std::int64_t> ||
                    std::is_same_v<T, bool> || std::is_same_v<T, std::string>,
                "Attr<T>: T must be double, std::int64_t, bool or std::string");

 public:
  /** @param name Attribute key, as documented by the adapter. */
  explicit Attr(std::string name) : m_name(std::move(name)) {}

  /** @brief Attribute key. */
  [[nodiscard]] const std::string& Name() const { return m_name; }

  /** @brief Value of the attribute in @p m, or nullopt if absent or of another type. */
  [[nodiscard]] std::optional<T> Read(const data::Metadata& m) const {
    if constexpr (std::is_same_v<T, std::string>) return m.Text(m_name);
    else if constexpr (std::is_same_v<T, bool>) return m.Flag(m_name);
    else if constexpr (std::is_same_v<T, double>) return m.Number(m_name);
    else {
      auto v = m.Number(m_name);
      return v ? std::optional<T>(static_cast<T>(*v)) : std::nullopt;
    }
  }

  /** @brief Attribute equal to @p v (false when missing). */
  friend Selector operator==(const Attr& a, T v) { return a.Compare([v](const T& x) { return x == v; }); }
  /** @brief Attribute not equal to @p v (false when missing). */
  friend Selector operator!=(const Attr& a, T v) { return a.Compare([v](const T& x) { return x != v; }); }
  /** @brief Attribute less than @p v (false when missing). */
  friend Selector operator<(const Attr& a, T v) { return a.Compare([v](const T& x) { return x < v; }); }
  /** @brief Attribute at most @p v (false when missing). */
  friend Selector operator<=(const Attr& a, T v) { return a.Compare([v](const T& x) { return x <= v; }); }
  /** @brief Attribute greater than @p v (false when missing). */
  friend Selector operator>(const Attr& a, T v) { return a.Compare([v](const T& x) { return x > v; }); }
  /** @brief Attribute at least @p v (false when missing). */
  friend Selector operator>=(const Attr& a, T v) { return a.Compare([v](const T& x) { return x >= v; }); }

 private:
  template <typename F>
  Selector Compare(F f) const {
    if constexpr (std::is_same_v<T, std::int64_t>) {
      // integers compare exactly when stored as integers, numerically otherwise
      return FromMetadata([name = m_name, f](const data::Metadata& m) {
        const auto* v = m.Find(name);
        if (!v) return false;
        if (const auto* i = std::get_if<std::int64_t>(v)) return f(*i);
        auto n = m.Number(name);
        return n.has_value() && f(static_cast<std::int64_t>(*n)) &&
               static_cast<double>(static_cast<std::int64_t>(*n)) == *n;
      });
    } else {
      return FromMetadata([attr = *this, f](const data::Metadata& m) {
        auto v = attr.Read(m);
        return v.has_value() && f(*v);
      });
    }
  }
  std::string m_name;
};

/** @brief Text attributes accept string literals: `Attr<std::string>("volume") == "ECal"`. */
inline Selector operator==(const Attr<std::string>& a, const char* v) { return a == std::string(v); }
/** @copydoc operator==(const Attr<std::string>&, const char*) */
inline Selector operator!=(const Attr<std::string>& a, const char* v) { return a != std::string(v); }

/** @brief Numeric attribute in [lo, hi] (inclusive). */
[[nodiscard]] Selector Between(const std::string& name, double lo, double hi);
/** @brief Numeric attribute equal to one of @p values. */
[[nodiscard]] Selector OneOf(const std::string& name, std::vector<double> values);
/** @brief Text attribute equal to one of @p values. */
[[nodiscard]] Selector OneOf(const std::string& name, std::vector<std::string> values);
/** @brief Text attribute equal to one of @p values (literal list). */
[[nodiscard]] Selector OneOf(const std::string& name, std::initializer_list<const char*> values);
/** @brief The attribute is present (any type). */
[[nodiscard]] Selector Exists(const std::string& name);
/**
 * @brief Text attribute fully matches the ECMAScript regular expression @p regex.
 * @throws std::regex_error if @p regex is invalid.
 */
[[nodiscard]] Selector Matches(const std::string& name, const std::string& regex);

/** @brief Default energy attribute key. */
inline constexpr const char* kEnergy = "energy";
/** @brief `energy > x`. */
[[nodiscard]] Selector EnergyAbove(double x, const std::string& attribute = kEnergy);
/** @brief `energy < x`. */
[[nodiscard]] Selector EnergyBelow(double x, const std::string& attribute = kEnergy);
/** @brief `lo <= energy <= hi`. */
[[nodiscard]] Selector EnergyBetween(double lo, double hi, const std::string& attribute = kEnergy);

/** @name Selection helpers (positional: need the element's index in its entry) */
///@{
/** @brief Only the element at position @p i. */
[[nodiscard]] Selector Index(std::size_t i);
/** @brief The first @p n elements. */
[[nodiscard]] Selector FirstN(std::size_t n);
/** @brief Every element. */
[[nodiscard]] Selector All();
/** @brief No element. */
[[nodiscard]] Selector None();
/** @brief Every element of type @p X (identity filter). */
template <typename X>
[[nodiscard]] filter::Filter<X> All() { return filter::Filter<X>{}; }
/** @brief No element of type @p X. */
template <typename X>
[[nodiscard]] filter::Filter<X> None() { return !filter::Filter<X>{}; }
///@}

}  // namespace fccvis::stdlib
