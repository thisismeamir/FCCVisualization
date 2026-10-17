/**
 * @file Expression.h
 * @brief Text forms used by the shell and configuration files: filter
 *        expressions, style assignments and layout expressions.
 *
 * Filter expressions (case-sensitive keywords):
 * @code
 * energy > 0.5 and subdetector == "ECal" or inside(box(100, 100, 100))
 * not exists(energy)          layer in [1, 2, 3]        volume ~ "Barrel.*"
 * between(energy, 1, 10)      first(5)                  index(0)
 * within(50, cylinder(0, 1000, 2000))                   inside(sphere(10), all)
 * @endcode
 * - Comparisons `== != < <= > >=` against a number, a "string" or true/false.
 *   A missing attribute fails every comparison (see stdlib/Filters.h).
 * - `attr in [v, ...]`, `attr ~ "regex"` (full match), `exists(attr)`.
 * - `and` binds tighter than `or`; `not` binds tightest; parentheses group.
 * - Functions: `all()`, `none()`, `first(n)`, `index(i)`, `between(attr, lo, hi)`,
 *   `inside(shape[, any|all|centroid])`, `outside(...)`, `within(d, shape[, ...])`.
 * - Shapes (millimetres): `box(dx, dy, dz)` (half-lengths), `sphere(r)`,
 *   `cylinder(rmin, rmax, halfz)`, `cone(rmin1, rmax1, rmin2, rmax2, halfz)`,
 *   `translate(shape, x, y, z)`.
 *
 * Style assignments: `key=value` items, for example
 * `color=#ff0000 size=4 marker=square opacity=0.5 width=2 dash=dashed`.
 *
 * Layout expressions: `xy`, `split(xy, rz)`, `vsplit(a, tabs(b, c))`, with
 * optional `as "label"` and `*weight` per child: `split(xy as "Front" *2, rz)`.
 *
 * Errors are reported with ParseError, whose column (1-based) points at the
 * offending character.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Layout.h"
#include "Style.h"
#include "fccvis/stdlib/Filters.h"

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fccvis::api {

/** @brief A text that could not be parsed. */
class ParseError : public std::runtime_error {
 public:
  /** @param message What is wrong. @param column 1-based column in the parsed text. */
  ParseError(const std::string& message, std::size_t column)
      : std::runtime_error(message + " (column " + std::to_string(column) + ")"), m_column(column), m_message(message) {}
  /** @brief 1-based column of the problem. */
  [[nodiscard]] std::size_t Column() const { return m_column; }
  /** @brief Message without the column. */
  [[nodiscard]] const std::string& Message() const { return m_message; }

 private:
  std::size_t m_column;
  std::string m_message;
};

/** @brief Parses a filter expression into a Selector (usable for any built-in element type). */
[[nodiscard]] stdlib::Selector ParseFilter(std::string_view text);

/** @brief Attribute names referenced by a filter expression (for completion and checks). */
[[nodiscard]] std::vector<std::string> FilterAttributes(std::string_view text);

/** @brief `key=value` items, split at blanks (quotes protect blanks); throws ParseError on malformed items. */
[[nodiscard]] std::map<std::string, std::string> ParseAssignments(std::string_view text);

/** @brief Parses a colour: `#rgb`, `#rrggbb`, `#rrggbbaa` or a name (red, blue, accent, ...). */
[[nodiscard]] style::Color ParseColor(std::string_view text);

/** @brief Point style from `key=value` items; unknown keys throw ParseError listing the allowed keys. */
[[nodiscard]] style::PointStyle ParsePointStyle(std::string_view text);
/** @brief Line style from `key=value` items (see ParsePointStyle). */
[[nodiscard]] style::LineStyle ParseLineStyle(std::string_view text);
/** @brief Surface style from `key=value` items (see ParsePointStyle). */
[[nodiscard]] style::SurfaceStyle ParseSurfaceStyle(std::string_view text);
/** @brief Environment style from `key=value` items (see ParsePointStyle). */
[[nodiscard]] style::EnvironmentStyle ParseEnvironment(std::string_view text);

/** @brief Keys accepted by the style parsers, by element kind ("point", "line", "surface", "environment"). */
[[nodiscard]] const std::vector<std::string>& StyleKeys(const std::string& kind);

/** @brief Parses a layout expression. */
[[nodiscard]] scene::layout::LayoutNode ParseLayout(std::string_view text);

/** @brief Layout expression of a node (inverse of ParseLayout). */
[[nodiscard]] std::string FormatLayout(const scene::layout::LayoutNode& node);

/** @brief Splits a command line into words; "double quotes" group, `\"` escapes; throws ParseError for an unterminated string. */
[[nodiscard]] std::vector<std::string> SplitWords(std::string_view line);

/** @brief Quotes a word for a command line if it needs it. */
[[nodiscard]] std::string QuoteWord(const std::string& word);

}  // namespace fccvis::api
