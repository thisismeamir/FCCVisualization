/**
 * @file Histogram.h
 * @brief Histograms of metadata attributes (B3): specs, filling, CSV and SVG.
 *
 * A histogram reduces the elements a layer selects (the layer's filter, the
 * same as in every display backend) to the distribution of one metadata
 * attribute. Numeric attributes (integers, numbers, flags) get fixed-width
 * bins; text attributes get one count per value.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "Elements.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fccvis::backend::hist {

/** @brief What to histogram. */
struct Spec {
  std::string scene;      ///< Scene name; empty for every open scene.
  std::string layer;      ///< Layer name.
  std::string attribute;  ///< Metadata key.
  int bins = 20;          ///< Number of bins (numeric attributes).
  std::optional<std::pair<double, double>> range;  ///< [lo, hi); data range when unset.
};

/**
 * @brief Parses `[scene/]layer:attribute[:bins[:lo:hi]]` items separated by `;`.
 * @throws std::invalid_argument with the offending item.
 */
[[nodiscard]] std::vector<Spec> ParseSpecs(const std::string& text);

/** @brief A filled histogram. */
struct Histogram {
  std::string scene, layer, attribute;  ///< What it shows.
  std::string unit;                     ///< From metadata `unit:<attribute>`, if any.
  bool categorical = false;             ///< Text attribute: categories instead of bins.
  double lo = 0, hi = 1;                ///< Bin range (numeric).
  std::vector<std::uint64_t> counts;    ///< Bin counts (numeric).
  std::map<std::string, std::uint64_t> categories;  ///< Counts per value (text).
  std::uint64_t underflow = 0;          ///< Values below lo.
  std::uint64_t overflow = 0;           ///< Values at or above hi.
  std::uint64_t missing = 0;            ///< Selected elements without the attribute.
  std::uint64_t filled = 0;             ///< Values counted in bins or categories.
};

/**
 * @brief Fills a histogram from the metadata of the selected elements.
 *
 * The attribute is categorical if any value is text. Without a range the
 * data range is used (hi widened to include the maximum); an empty or single
 * value range becomes [v - 0.5, v + 0.5).
 */
[[nodiscard]] Histogram Fill(const Spec& spec, const std::string& scene,
                             const std::vector<const data::Metadata*>& selected);

/** @brief CSV: `layer,attribute,kind,bin,low,high,count`, one row per bin/category plus underflow, overflow, missing. */
[[nodiscard]] std::string ToCsv(const std::vector<Histogram>& histograms);

/** @brief SVG with one bar chart per histogram, tiled in a grid of @p width x @p height. */
[[nodiscard]] std::string ToSvg(const std::vector<Histogram>& histograms, int width, int height);

}  // namespace fccvis::backend::hist
