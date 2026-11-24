// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "PhoenixBackend.h"

#include "fccvis/stdlib/MeshExport.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <variant>

namespace fccvis::backend {

namespace {

using draw::FormatNumber;

std::string Str(const std::string& s) {
  std::string out = "\"";
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\t': out += "\\t"; break;
      case '\r': out += "\\r"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out + "\"";
}

std::string Hex(style::Color c) {
  char buf[8];
  std::snprintf(buf, sizeof buf, "#%02x%02x%02x", c.r, c.g, c.b);
  return buf;
}

std::string Num(double v) { return FormatNumber(v, 4); }

std::string Vec(const draw::Vec3& v) { return "[" + Num(v[0]) + ", " + Num(v[1]) + ", " + Num(v[2]) + "]"; }

const std::set<std::string>& Reserved() {
  static const std::set<std::string> r = {"pos", "color", "type", "size", "linewidth", "dparams"};
  return r;
}

std::string Meta(const data::Metadata* m) {
  if (!m) return {};
  std::string s;
  for (const auto& [k, v] : m->All()) {
    if (k.rfind("unit:", 0) == 0 || Reserved().count(k)) continue;
    s += ", " + Str(k) + ": ";
    std::visit(
        [&](const auto& x) {
          using T = std::decay_t<decltype(x)>;
          if constexpr (std::is_same_v<T, bool>) s += x ? "true" : "false";
          else if constexpr (std::is_same_v<T, std::int64_t>) s += std::to_string(x);
          else if constexpr (std::is_same_v<T, double>) s += Num(x);
          else s += Str(x);
        },
        v);
  }
  return s;
}

}  // namespace

PhoenixBackend::~PhoenixBackend() {
  if (m_server) m_server->Stop();
}

Capabilities PhoenixBackend::GetCapabilities() const {
  Capabilities c;
  c.description = "Phoenix web event display: event-data JSON (Hits, Tracks) and OBJ geometry, optional HTTP host";
  c.kinds = {ElementKind::Point, ElementKind::Line, ElementKind::Surface};
  c.styleProperties = {"color", "opacity", "visible", "width"};
  c.splits = false;  // Phoenix shows one event at a time; each open scene is one event
  c.tabs = false;
  c.options = {{"output_dir", "directory for events.json and the OBJ files (default .)"},
               {"port", "serve the output directory over HTTP on this port (0: off, default)"},
               {"bind", "address the HTTP server binds to (default 127.0.0.1)"},
               {"tolerance", "relative surface tessellation tolerance (default 0.02)"},
               {"async", "write on a worker thread (default true)"}};
  return c;
}

void PhoenixBackend::Configure(const Options& options) {
  if (auto it = options.find("port"); it != options.end()) {
    try {
      std::size_t used = 0;
      m_port = std::stoi(it->second, &used);
      if (used != it->second.size()) throw std::invalid_argument("port");
    } catch (const std::exception&) {
      m_port = -1;
    }
    if (m_port < 0 || m_port > 65535) throw std::invalid_argument("option 'port' must be 0..65535, got '" + it->second + "'");
  }
  if (auto it = options.find("bind"); it != options.end()) m_bind = it->second;
  if (m_port > 0) {
    std::filesystem::create_directories(m_outputDir);
    m_server = std::make_unique<phoenix::HttpServer>();
    const std::string dir = m_outputDir;
    m_server->Start(dir, m_port, m_bind, [dir] {
      std::string page = "<!doctype html><meta charset=\"utf-8\"><title>fccvis Phoenix files</title><ul>";
      std::error_code ec;
      std::vector<std::string> names;
      for (const auto& e : std::filesystem::directory_iterator(dir, ec)) names.push_back(e.path().filename().string());
      std::sort(names.begin(), names.end());
      for (const auto& n : names) page += "<li><a href=\"" + n + "\">" + n + "</a></li>";
      return page + "</ul><p>Load <code>events.json</code> in Phoenix by URL (<code>?file=&lt;url&gt;&amp;type=json</code>).</p>\n";
    });
    const std::string host = m_bind.empty() || m_bind == "0.0.0.0" ? std::string("127.0.0.1") : m_bind;
    Log(changes::Severity::Info, "serving '" + dir + "' at http://" + host + ":" + std::to_string(m_port) + "/");
  }
}

void PhoenixBackend::Stop() {
  if (m_server) m_server->Stop();
  FileBackend::Stop();
}

std::shared_ptr<const void> PhoenixBackend::Collect(const SyncPlan&, const SyncRequest& request) {
  auto out = std::make_shared<Extracted>();
  for (const auto& name : request.open) {
    const auto scene = request.view.FindScene(name);
    if (!scene) continue;
    bool first = true;
    for (const auto& [layer, type] : scene->layers.Types()) {
      const auto kind = KindOf(type);
      if (!kind || *kind == ElementKind::Surface) continue;
      auto take = [&]<typename X>() {
        const auto* l = scene->layers.Find<X>(layer);
        if (!l || !l->series) return;
        if (first) {
          out->eventNumber[name] = l->entry.value_or(0);
          first = false;
        }
        auto entry = request.view.Data().template Entry<X>(*l->series, l->entry.value_or(0));
        if (!entry) return;
        auto& meta = out->meta[name][layer];
        for (std::size_t i = 0; i < entry->size(); ++i)
          if (l->Passes((*entry)[i], i)) meta[i] = (*entry)[i].meta;
      };
      if (*kind == ElementKind::Point) take.template operator()<data::PointElement>();
      else take.template operator()<data::LineElement>();
    }
  }
  return out;
}

std::string PhoenixBackend::EventsJson(const std::map<std::string, draw::SceneDrawing>& scenes,
                                       const std::set<std::string>& open, const Extracted& extracted) {
  std::string s = "{";
  bool firstScene = true;
  for (const auto& name : open) {
    auto it = scenes.find(name);
    if (it == scenes.end()) continue;
    const auto& d = it->second;
    auto en = extracted.eventNumber.find(name);
    s += std::string(firstScene ? "\n" : ",\n") + "  " + Str(name) + ": {\n    \"event number\": " +
         std::to_string(en == extracted.eventNumber.end() ? 0 : en->second) + ",\n    \"run number\": 0";
    firstScene = false;
    auto metaOf = [&](const std::string& layer, std::size_t index) -> const data::Metadata* {
      auto sc = extracted.meta.find(name);
      if (sc == extracted.meta.end()) return nullptr;
      auto ly = sc->second.find(layer);
      if (ly == sc->second.end()) return nullptr;
      auto el = ly->second.find(index);
      return el == ly->second.end() ? nullptr : &el->second;
    };
    // Hits: one collection per layer and colour (Phoenix colours a collection by its first hit)
    std::string hits;
    for (const auto& [layer, drawing] : d.layers) {
      if (!drawing || drawing->points.empty()) continue;
      std::map<std::string, std::vector<const draw::PointPrim*>> byColour;
      std::vector<std::string> order;
      for (const auto& p : drawing->points) {
        if (!draw::Visible(p.s.common)) continue;
        const std::string c = Hex(draw::ColorOf(p.s.common));
        if (!byColour.count(c)) order.push_back(c);
        byColour[c].push_back(&p);
      }
      for (const auto& c : order) {
        const std::string collection = order.size() == 1 ? layer : layer + " " + c;
        hits += std::string(hits.empty() ? "" : ",\n") + "      " + Str(collection) + ": [";
        bool first = true;
        for (const auto* p : byColour[c]) {
          hits += std::string(first ? "\n" : ",\n") + "        {\"type\": \"Point\", \"pos\": " + Vec(p->p) +
                  ", \"color\": " + Str(c) + Meta(metaOf(layer, p->index)) + "}";
          first = false;
        }
        hits += "\n      ]";
      }
    }
    if (!hits.empty()) s += ",\n    \"Hits\": {\n" + hits + "\n    }";
    // Tracks
    std::string tracks;
    for (const auto& [layer, drawing] : d.layers) {
      if (!drawing || drawing->lines.empty()) continue;
      tracks += std::string(tracks.empty() ? "" : ",\n") + "      " + Str(layer) + ": [";
      bool first = true;
      for (const auto& l : drawing->lines) {
        if (!draw::Visible(l.s.common)) continue;
        std::string pos = "[";
        for (std::size_t i = 0; i < l.v.size(); ++i) pos += (i ? ", " : "") + Vec(l.v[i]);
        tracks += std::string(first ? "\n" : ",\n") + "        {\"pos\": " + pos + "], \"color\": " +
                  Str(Hex(draw::ColorOf(l.s.common))) + ", \"linewidth\": " + Num(l.s.width.value_or(2.f)) +
                  Meta(metaOf(layer, l.index)) + "}";
        first = false;
      }
      tracks += "\n      ]";
    }
    if (!tracks.empty()) s += ",\n    \"Tracks\": {\n" + tracks + "\n    }";
    s += "\n  }";
  }
  return s + "\n}\n";
}

void PhoenixBackend::Render(const Job& job) {
  const auto* extracted = static_cast<const Extracted*>(job.extra.get());
  const Extracted empty;
  const std::string json = EventsJson(job.scenes, job.open, extracted ? *extracted : empty);
  const auto path = OutputPath("events.json");
  {
    // write then rename, so the HTTP server never serves a half-written file
    std::ofstream out(path + ".tmp", std::ios::binary);
    out << json;
    if (!out) {
      Log(changes::Severity::Error, "cannot write '" + path + "'");
      return;
    }
  }
  std::error_code ec;
  std::filesystem::rename(path + ".tmp", path, ec);
  if (ec) Log(changes::Severity::Error, "cannot write '" + path + "': " + ec.message());

  for (const auto& name : job.open) {
    auto it = job.scenes.find(name);
    if (it == job.scenes.end()) continue;
    stdlib::ObjWriter obj;
    for (const auto& [layer, drawing] : it->second.layers) {
      if (!drawing) continue;
      std::map<std::string, std::pair<std::vector<std::array<double, 3>>, std::vector<std::array<std::uint32_t, 3>>>> merged;
      std::map<std::string, style::Color> colours;
      for (const auto& m : drawing->meshes) {
        if (!draw::Visible(m.s.common)) continue;
        auto c = draw::ColorOf(m.s.common);
        c.a = static_cast<std::uint8_t>(255 * draw::Alpha(m.s.common));
        const std::string key = Hex(c);
        colours[key] = c;
        auto& [v, t] = merged[key];
        const auto base = static_cast<std::uint32_t>(v.size());
        v.insert(v.end(), m.vertices.begin(), m.vertices.end());
        for (const auto& tri : m.triangles) t.push_back({tri[0] + base, tri[1] + base, tri[2] + base});
      }
      for (const auto& [key, vt] : merged)
        obj.Add(merged.size() == 1 ? layer : layer + "_" + key.substr(1), vt.first, vt.second, colours[key]);
    }
    if (obj.Objects() == 0) continue;
    const std::string base = std::filesystem::path(ScenePath(name, "")).filename().string();
    std::ofstream(ScenePath(name, ".obj"), std::ios::binary) << obj.Obj(base + ".mtl");
    std::ofstream(ScenePath(name, ".mtl"), std::ios::binary) << obj.Mtl();
  }
}

static Registrar g_phoenixRegistrar("phoenix", [] { return std::make_unique<PhoenixBackend>(); });

}  // namespace fccvis::backend
