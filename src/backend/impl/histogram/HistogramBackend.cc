// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "HistogramBackend.h"

#include <fstream>
#include <map>
#include <stdexcept>

namespace fccvis::backend {

namespace {

using Result = std::map<std::string, std::vector<hist::Histogram>>;  // scene -> histograms

template <typename X>
std::vector<const data::Metadata*> Selected(const scene::Scene& s, const data::SessionData& d, const std::string& layer,
                                            std::shared_ptr<const std::vector<X>>& keep) {
  std::vector<const data::Metadata*> out;
  const auto* l = s.layers.Find<X>(layer);
  if (!l || !l->series) return out;
  keep = d.Entry<X>(*l->series, l->entry.value_or(0));
  if (!keep) return out;
  for (std::size_t i = 0; i < keep->size(); ++i)
    if (l->Passes((*keep)[i], i)) out.push_back(&(*keep)[i].meta);
  return out;
}

}  // namespace

Capabilities HistogramBackend::GetCapabilities() const {
  Capabilities c;
  c.description = "histogram maker: distributions of metadata attributes of the selected elements (CSV, SVG)";
  c.kinds = {ElementKind::Point, ElementKind::Line, ElementKind::Surface};
  c.styleProperties = {};
  c.splits = false;
  c.tabs = false;
  c.options = {{"width", "picture width in pixels (default 800)"},
               {"height", "picture height in pixels (default 600)"},
               {"output_dir", "directory for the output files (default .)"},
               {"tolerance", "relative surface tessellation tolerance (default 0.02)"},
               {"async", "render on a worker thread (default true)"}};
  c.options["histograms"] = "[scene/]layer:attribute[:bins[:lo:hi]] items separated by ; (default: every attribute)";
  c.options["formats"] = "csv and/or svg (default csv,svg)";
  return c;
}

void HistogramBackend::Configure(const Options& options) {
  if (auto it = options.find("histograms"); it != options.end()) m_specs = hist::ParseSpecs(it->second);
  if (auto it = options.find("formats"); it != options.end()) {
    m_csv = it->second.find("csv") != std::string::npos;
    m_svg = it->second.find("svg") != std::string::npos;
    if (!m_csv && !m_svg) throw std::invalid_argument("option 'formats' must name csv and/or svg");
  }
}

std::vector<hist::Histogram> HistogramBackend::Make(const SessionView& view, const std::string& sceneName,
                                                    const std::vector<hist::Spec>& specs) {
  std::vector<hist::Histogram> out;
  const auto scene = view.FindScene(sceneName);
  if (!scene) return out;
  for (const auto& [layer, type] : scene->layers.Types()) {
    const auto kind = KindOf(type);
    if (!kind) continue;
    std::shared_ptr<const std::vector<data::PointElement>> pk;
    std::shared_ptr<const std::vector<data::LineElement>> lk;
    std::shared_ptr<const std::vector<data::SurfaceElement>> sk;
    std::vector<const data::Metadata*> selected;
    switch (*kind) {
      case ElementKind::Point: selected = Selected(*scene, view.Data(), layer, pk); break;
      case ElementKind::Line: selected = Selected(*scene, view.Data(), layer, lk); break;
      case ElementKind::Surface: selected = Selected(*scene, view.Data(), layer, sk); break;
    }
    std::vector<hist::Spec> todo;
    if (specs.empty()) {
      std::set<std::string> keys;
      for (const auto* m : selected)
        for (const auto& [k, v] : m->All())
          if (k.rfind("unit:", 0) != 0) keys.insert(k);
      for (const auto& k : keys) todo.push_back({sceneName, layer, k, 20, std::nullopt});
    } else {
      for (const auto& s : specs)
        if ((s.scene.empty() || s.scene == sceneName) && s.layer == layer) todo.push_back(s);
    }
    for (const auto& s : todo) out.push_back(hist::Fill(s, sceneName, selected));
  }
  return out;
}

std::shared_ptr<const void> HistogramBackend::Collect(const SyncPlan&, const SyncRequest& request) {
  auto result = std::make_shared<Result>();
  for (const auto& scene : request.open) (*result)[scene] = Make(request.view, scene, m_specs);
  return result;
}

void HistogramBackend::Render(const Job& job) {
  const auto* result = static_cast<const Result*>(job.extra.get());
  if (!result) return;
  for (const auto& [scene, hs] : *result) {
    if (m_csv) {
      const auto path = ScenePath(scene, ".hist.csv");
      std::ofstream out(path, std::ios::binary);
      out << hist::ToCsv(hs);
      if (!out) Log(changes::Severity::Error, "cannot write '" + path + "'");
    }
    if (m_svg) {
      const auto path = ScenePath(scene, ".hist.svg");
      std::ofstream out(path, std::ios::binary);
      out << hist::ToSvg(hs, m_width, m_height);
      if (!out) Log(changes::Severity::Error, "cannot write '" + path + "'");
    }
  }
}

static Registrar g_histogramRegistrar("histogram", [] { return std::make_unique<HistogramBackend>(); });

}  // namespace fccvis::backend
