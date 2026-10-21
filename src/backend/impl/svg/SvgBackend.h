/**
 * @file SvgBackend.h
 * @brief B2: SVG export backend (no external dependency).
 *
 * Registered as "svg". Writes, after each sync that changed something (call
 * Session::Flush to wait):
 * - `<output_dir>/<scene>.svg` for every open scene;
 * - `<output_dir>/layout.svg` when a layout is set: panes tiled as groups,
 *   titled with their layout labels; tabs show their first tab.
 *
 * Same projection and painter's ordering as the raster backend (draw::Paint);
 * elements carry `data-layer` and `data-index` attributes. Default style is the
 * figure theme (monochrome, accent `#7017FF`, white background).
 *
 * Options: `width` (800), `height` (600), `output_dir` (`.`), `tolerance`
 * (0.02), `async` (`true`), `precision` (2 decimals), `titles` (`true`).
 *
 * Degradations (declared in Capabilities): no interaction or picking; no
 * perspective camera (orthographic); no per-vertex shading; tabs show their
 * first tab.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "BackendRegistry.h"
#include "FileBackend.h"
#include "Svg.h"

#include <string>

namespace fccvis::backend {

/** @brief SVG export backend (B2). */
class SvgBackend final : public FileBackend {
 public:
  [[nodiscard]] Capabilities GetCapabilities() const override;

 protected:
  [[nodiscard]] std::string Kind() const override { return "svg"; }
  void Configure(const Options& options) override;
  void Render(const Job& job) override;

 private:
  svg::SvgOptions m_svg;
};

}  // namespace fccvis::backend
