/**
 * @file HistogramBackend.h
 * @brief B3: histogram maker backend (no external dependency).
 *
 * Registered as "histogram". Not a drawing of elements: for every open scene
 * it reduces layers to distributions of metadata attributes, over the
 * elements each layer selects (its filter, so selections are identical to the
 * display backends) in the layer's current entry.
 *
 * Which histograms: option `histograms` = `[scene/]layer:attribute[:bins[:lo:hi]]`
 * items separated by `;` (default bins 20, data range). Without the option,
 * every metadata attribute of every layer (keys `unit:*` excluded).
 *
 * Outputs after each changing sync (Session::Flush waits):
 * `<output_dir>/<scene>.hist.csv` and `<output_dir>/<scene>.hist.svg`
 * (option `formats`: `csv,svg` by default).
 *
 * ROOT `TH1` output is not implemented (see STATUS.md).
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "BackendRegistry.h"
#include "FileBackend.h"
#include "Histogram.h"

#include <set>
#include <string>
#include <vector>

namespace fccvis::backend {

/** @brief Histogram maker backend (B3). */
class HistogramBackend final : public FileBackend {
 public:
  [[nodiscard]] Capabilities GetCapabilities() const override;

  /** @brief Histograms of one scene of the session view (also used by tests). */
  [[nodiscard]] static std::vector<hist::Histogram> Make(const SessionView& view, const std::string& scene,
                                                         const std::vector<hist::Spec>& specs);

 protected:
  [[nodiscard]] std::string Kind() const override { return "histogram"; }
  void Configure(const Options& options) override;
  [[nodiscard]] std::set<ElementKind> DrawnKinds() const override { return {}; }
  std::shared_ptr<const void> Collect(const SyncPlan& plan, const SyncRequest& request) override;
  void Render(const Job& job) override;

 private:
  std::vector<hist::Spec> m_specs;
  bool m_csv = true, m_svg = true;
};

}  // namespace fccvis::backend
