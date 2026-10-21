/**
 * @file RasterBackend.h
 * @brief B0: software raster backend writing PNG images (no external dependency).
 *
 * Registered as "raster" (and "fv", its former name). Draws an orthographic
 * projection of points, polylines and surfaces (filled triangles plus outline
 * edges) with the resolved styles, on its own render thread.
 *
 * Outputs, after each sync that changed something (call Session::Flush to
 * wait for them):
 * - `<output_dir>/<scene>.png` for every open scene (width x height);
 * - `<output_dir>/layout.png` with the pruned layout tiled (splits by
 *   weight; tabs show their first tab only), when a layout is set;
 * - legacy: option `output=<file>` writes the layout picture (or the single
 *   open scene) to that file; a `.ppm` extension writes ASCII PPM.
 *
 * Options: `width` (800), `height` (600), `output_dir` (`.`), `output`,
 * `tolerance` (0.02, surface tessellation), `async` (`true`).
 *
 * Degradations (declared in Capabilities): no interaction, picking or text
 * (labels are not drawn); tabs render their first tab; perspective cameras are
 * drawn orthographically.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "BackendRegistry.h"
#include "FileBackend.h"
#include "Png.h"

#include <string>

namespace fccvis::backend {

/** @brief Software raster backend (B0). */
class RasterBackend final : public FileBackend {
 public:
  [[nodiscard]] Capabilities GetCapabilities() const override;

  /** @brief File the image of @p scene is written to. */
  [[nodiscard]] std::string ScenePath(const std::string& scene) const { return FileBackend::ScenePath(scene, ".png"); }
  /** @brief File the layout picture is written to. */
  [[nodiscard]] std::string LayoutPath() const { return OutputPath("layout.png"); }

 protected:
  [[nodiscard]] std::string Kind() const override { return "raster"; }
  void Configure(const Options& options) override;
  void Render(const Job& job) override;

 private:
  void ExportPpm(const std::string& path, const raster::Image& img) const;
  std::string m_output;  // legacy single-file output
};

}  // namespace fccvis::backend
