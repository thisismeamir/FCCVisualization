/**
 * @file DetectorFilters.h
 * @brief Detector sub-part filters: sugar over attribute filters.
 *
 * No detector knowledge is built in: these filters read metadata that
 * adapters set. The attribute names are the documented defaults below (the
 * detector adapter A1 sets them on geometry; event adapters may set them on
 * hits); every filter takes the attribute name as an optional last argument.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "fccvis/stdlib/Filters.h"

#include <cstdint>
#include <string>

namespace fccvis::stdlib {

/** @brief Default metadata keys for detector structure, set by adapters. */
namespace keys {
inline constexpr const char* kSubdetector = "subdetector";  ///< Sub-detector name (text).
inline constexpr const char* kLayer = "layer";              ///< Layer number (integer).
inline constexpr const char* kVolume = "volume";            ///< Volume or path name (text).
inline constexpr const char* kMaterial = "material";        ///< Material name (text).
inline constexpr const char* kSystemId = "system_id";       ///< DD4hep system id (integer).
}  // namespace keys

/** @brief Elements of sub-detector @p name (exact text match). */
[[nodiscard]] Selector Subdetector(const std::string& name, const std::string& attribute = keys::kSubdetector);

/** @brief Elements in layer @p n. */
[[nodiscard]] Selector Layer(std::int64_t n, const std::string& attribute = keys::kLayer);

/** @brief Elements in layers @p first .. @p last (inclusive). */
[[nodiscard]] Selector LayerRange(std::int64_t first, std::int64_t last,
                                  const std::string& attribute = keys::kLayer);

/**
 * @brief Elements whose volume name fully matches the regular expression @p regex.
 * @throws std::regex_error if @p regex is invalid.
 */
[[nodiscard]] Selector Volume(const std::string& regex, const std::string& attribute = keys::kVolume);

/** @brief Elements made of material @p name (exact text match). */
[[nodiscard]] Selector Material(const std::string& name, const std::string& attribute = keys::kMaterial);

}  // namespace fccvis::stdlib
