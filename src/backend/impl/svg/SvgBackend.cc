// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "SvgBackend.h"

#include <fstream>
#include <stdexcept>

namespace fccvis::backend {

Capabilities SvgBackend::GetCapabilities() const {
  Capabilities c;
  c.description = "SVG export: orthographic vector pictures of points, lines and surfaces (no dependencies)";
  c.kinds = {ElementKind::Point, ElementKind::Line, ElementKind::Surface};
  c.styleProperties = {"axes", "background", "color", "dash", "edgeColor", "edgeWidth", "grid",
                       "marker", "opacity", "size", "visible", "width", "wireframe"};
  c.splits = true;
  c.tabs = false;  // static picture: a Tabs node shows its first tab
  c.options = {{"width", "picture width in pixels (default 800)"},
               {"height", "picture height in pixels (default 600)"},
               {"output_dir", "directory for the output files (default .)"},
               {"tolerance", "relative surface tessellation tolerance (default 0.02)"},
               {"async", "render on a worker thread (default true)"}};
  c.options["precision"] = "decimals of coordinates, 0..6 (default 2)";
  c.options["titles"] = "draw pane titles in layout.svg (default true)";
  return c;
}

void SvgBackend::Configure(const Options& options) {
  if (auto it = options.find("precision"); it != options.end()) {
    try {
      m_svg.precision = std::stoi(it->second);
    } catch (const std::exception&) {
      m_svg.precision = -1;
    }
    if (m_svg.precision < 0 || m_svg.precision > 6)
      throw std::invalid_argument("option 'precision' must be an integer in 0..6, got '" + it->second + "'");
  }
  if (auto it = options.find("titles"); it != options.end()) m_svg.titles = it->second != "false" && it->second != "0";
}

void SvgBackend::Render(const Job& job) {
  m_svg.width = m_width;
  m_svg.height = m_height;
  auto write = [this](const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
    if (!out) Log(changes::Severity::Error, "cannot write '" + path + "'");
  };
  for (const auto& name : job.open) {
    auto it = job.scenes.find(name);
    if (it == job.scenes.end()) continue;
    write(ScenePath(name, ".svg"), svg::SceneDocument(it->second, m_svg));
  }
  if (job.layout) write(OutputPath("layout.svg"), svg::LayoutDocument(job.scenes, job.layout, job.open, m_svg));
}

static Registrar g_svgRegistrar("svg", [] { return std::make_unique<SvgBackend>(); });

}  // namespace fccvis::backend
