#pragma once
/**
 * @file CsvAdapter.h
 * @brief A3: delimiter-separated text with a header -> point or line series.
 *
 * Registered as "csv". Opening scans the file once (streaming): it indexes
 * which rows belong to which entry, infers column types and reports malformed
 * rows with their line numbers. Loading entry n later re-reads only the rows
 * of entry n (by file offset).
 *
 * Mapping:
 * - `kind=point` (default): one point per row at (x, y, z).
 * - `kind=line`: rows with the same `group` value form one polyline, vertices
 *   ordered by the `order` column (file order if not given); metadata from the
 *   group's first row.
 * - `entry=<column>`: rows with the same value form one entry (integer values
 *   in ascending order, other values in order of first appearance); without it
 *   the file is a single entry.
 * - Metadata: every other column (or the `columns` list), typed by inference
 *   over the whole file: int64, else double, else bool (true/false), else
 *   string. Empty cells give no key.
 * - Units: a header `energy [GeV]` names the column `energy` and adds metadata
 *   `unit:energy = GeV`. Coordinates stated in a length unit (um, mm, cm, m)
 *   are converted to millimetres and the conversion is stated in the series
 *   description; any other coordinate unit skips the file.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */

#include "Adapter.h"

namespace fccvis::adapters {

/** @brief CSV adapter (A3). */
class CsvAdapter final : public Adapter {
 public:
  [[nodiscard]] std::string Name() const override { return "csv"; }
  [[nodiscard]] Capabilities GetCapabilities() const override;
  LoadReport Open(data::SessionData& data, const std::string& path, const Options& options) override;
};

}  // namespace fccvis::adapters
