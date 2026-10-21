// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "RasterBackend.h"

#include "Canvas.h"

#include <fstream>

namespace fccvis::backend {

namespace {

bool EndsWith(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

Capabilities RasterBackend::GetCapabilities() const {
  Capabilities c;
  c.description = "FV raster backend: orthographic PNG images of points, lines and surfaces (no dependencies)";
  c.kinds = {ElementKind::Point, ElementKind::Line, ElementKind::Surface};
  c.styleProperties = {"axes", "background", "color", "dash", "edgeColor", "edgeWidth", "grid",
                       "marker", "opacity", "size", "visible", "width", "wireframe"};
  c.splits = true;
  c.tabs = false;  // static image: a Tabs node shows its first tab
  c.options = {{"width", "picture width in pixels (default 800)"},
               {"height", "picture height in pixels (default 600)"},
               {"output_dir", "directory for the output files (default .)"},
               {"tolerance", "relative surface tessellation tolerance (default 0.02)"},
               {"async", "render on a worker thread (default true)"}};
  c.options["output"] = "legacy single picture file; .ppm writes ASCII PPM";
  return c;
}

void RasterBackend::Configure(const Options& options) {
  if (auto it = options.find("output"); it != options.end()) m_output = it->second;
}

void RasterBackend::Render(const Job& job) {
  const style::Color white{255, 255, 255, 255};
  const draw::Rect full{0, 0, static_cast<double>(m_width), static_cast<double>(m_height)};
  for (const auto& name : job.open) {
    auto it = job.scenes.find(name);
    if (it == job.scenes.end()) continue;
    raster::Canvas canvas(m_width, m_height, white);
    raster::RenderScene(canvas, it->second, full);
    if (!raster::WritePng(ScenePath(name), canvas.Pixels())) Log(changes::Severity::Error, "cannot write '" + ScenePath(name) + "'");
  }

  std::optional<raster::Image> picture;
  if (job.layout) {
    raster::Canvas canvas(m_width, m_height, style::Color::FromHex(0xE0E0E0));
    for (const auto& pane : draw::Arrange(job.layout, job.open, full)) {
      auto it = job.scenes.find(pane.scene);
      if (it == job.scenes.end()) continue;
      raster::RenderScene(canvas, it->second, {pane.rect.x + 1, pane.rect.y + 1, pane.rect.w - 2, pane.rect.h - 2});
    }
    picture = canvas.Pixels();
    if (!raster::WritePng(LayoutPath(), *picture)) Log(changes::Severity::Error, "cannot write '" + LayoutPath() + "'");
  }

  if (!m_output.empty()) {
    if (!picture) {
      raster::Canvas canvas(m_width, m_height, white);
      if (!job.open.empty())
        if (auto it = job.scenes.find(*job.open.begin()); it != job.scenes.end())
          raster::RenderScene(canvas, it->second, full);
      picture = canvas.Pixels();
    }
    if (EndsWith(m_output, ".ppm")) ExportPpm(m_output, *picture);
    else if (!raster::WritePng(m_output, *picture)) Log(changes::Severity::Error, "cannot write '" + m_output + "'");
  }
}

void RasterBackend::ExportPpm(const std::string& path, const raster::Image& img) const {
  std::ofstream out(path);
  if (!out) {
    Log(changes::Severity::Error, "cannot write '" + path + "'");
    return;
  }
  out << "P3\n" << img.width << " " << img.height << "\n255\n";
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      const auto* p = img.rgba.data() + (static_cast<std::size_t>(y) * img.width + x) * 4;
      out << int(p[0]) << " " << int(p[1]) << " " << int(p[2]) << " ";
    }
    out << "\n";
  }
}

static Registrar g_rasterRegistrar("raster", [] { return std::make_unique<RasterBackend>(); });
// "fv" is the former name of this backend.
static const bool g_fvAlias = Registry::Global().RegisterAlias("fv", "raster");

}  // namespace fccvis::backend
