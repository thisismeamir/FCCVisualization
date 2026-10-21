/**
 * @file Svg.h
 * @brief SVG writer for the SVG backend (B2): scenes and layouts as SVG text.
 *
 * Uses the painter's display list of Drawing.h (draw::Paint), so the SVG and
 * raster backends draw the same picture. Coordinates are written with a fixed
 * number of decimals and trailing zeros removed, so output is stable text
 * (golden tests compare it verbatim).
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Drawing.h"

#include <map>
#include <optional>
#include <set>
#include <string>

namespace fccvis::backend::svg {

/** @brief Output settings. */
struct SvgOptions {
  int width = 800;       ///< Picture width (SVG user units = pixels).
  int height = 600;      ///< Picture height.
  int precision = 2;     ///< Decimals of coordinates.
  bool titles = true;    ///< Draw pane titles (layout label or scene name).
};

/** @brief Number with at most @p precision decimals, trailing zeros removed, no "-0". */
[[nodiscard]] inline std::string Num(double v, int precision) { return draw::FormatNumber(v, precision); }

/** @brief Escapes &, <, >, " and ' for XML text and attributes. */
[[nodiscard]] inline std::string Escape(const std::string& s) { return draw::XmlEscape(s); }

/**
 * @brief One scene as an SVG group (`<g>` with a clip path) in @p rect.
 * @param id    Unique id prefix within the document (used for the clip path).
 * @param title Pane title; empty for none.
 */
[[nodiscard]] std::string SceneGroup(const draw::SceneDrawing& scene, const draw::Rect& rect, const std::string& id,
                                     const std::string& title, const SvgOptions& options);

/** @brief Standalone SVG document of one scene. */
[[nodiscard]] std::string SceneDocument(const draw::SceneDrawing& scene, const SvgOptions& options);

/**
 * @brief Standalone SVG document of a layout: panes tiled as groups, titled
 *        with their layout labels (tabs show their first tab).
 */
[[nodiscard]] std::string LayoutDocument(const std::map<std::string, draw::SceneDrawing>& scenes,
                                         const std::optional<scene::layout::LayoutNode>& layout,
                                         const std::set<std::string>& open, const SvgOptions& options);

}  // namespace fccvis::backend::svg
