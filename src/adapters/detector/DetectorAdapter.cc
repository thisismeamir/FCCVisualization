// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "DetectorAdapter.h"

#include "AdapterRegistry.h"
#include "TGeoShapeAdapter.h"

#include <DD4hep/DetElement.h>
#include <DD4hep/Detector.h>
#include <DD4hep/Volumes.h>
#include <TGeoCompositeShape.h>
#include <TGeoManager.h>
#include <TGeoMaterial.h>
#include <TGeoNode.h>
#include <TGeoShapeAssembly.h>
#include <TGeoVolume.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace fccvis::adapters {

namespace {

/** Keeps a loaded geometry alive while elements reference its shapes. */
struct Holder {
  std::string ddName;       // DD4hep instance name, if any
  TGeoManager* manager = nullptr;
  bool ownsManager = false;
  ~Holder() {
    if (!ddName.empty()) dd4hep::Detector::destroyInstance(ddName);
    else if (ownsManager) delete manager;
  }
};

std::mutex& GeometryLock() {
  static std::mutex m;  // TGeo loading and gGeoManager switching are process-global
  return m;
}

struct Config {
  int depth = 3;
  std::vector<std::string> include, exclude;
  std::size_t maxVolumes = 200000;
};

std::vector<std::string> List(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s + ",") {
    if (c == ',') {
      if (!cur.empty()) out.push_back(cur);
      cur.clear();
    } else if (c != ' ') {
      cur += c;
    }
  }
  return out;
}

bool Glob(const std::string& p, const std::string& s) {
  std::size_t i = 0, j = 0, star = std::string::npos, mark = 0;
  while (j < s.size()) {
    if (i < p.size() && (p[i] == '?' || p[i] == s[j])) {
      ++i;
      ++j;
    } else if (i < p.size() && p[i] == '*') {
      star = i++;
      mark = j;
    } else if (star != std::string::npos) {
      i = star + 1;
      j = ++mark;
    } else {
      return false;
    }
  }
  while (i < p.size() && p[i] == '*') ++i;
  return i == p.size();
}

Config Parse(const Options& o) {
  Config c;
  for (const auto& [k, v] : o)
    if (k != "depth" && k != "include" && k != "exclude" && k != "max_volumes")
      throw std::invalid_argument("unknown detector option '" + k + "'");
  auto integer = [&](const char* key, long lo, long hi) -> std::optional<long> {
    auto it = o.find(key);
    if (it == o.end()) return std::nullopt;
    try {
      std::size_t used = 0;
      const long v = std::stol(it->second, &used);
      if (used == it->second.size() && v >= lo && v <= hi) return v;
    } catch (const std::exception&) {
    }
    throw std::invalid_argument(std::string("option '") + key + "' must be an integer in " + std::to_string(lo) +
                                ".." + std::to_string(hi));
  };
  if (auto v = integer("depth", 0, 100)) c.depth = static_cast<int>(*v);
  if (auto v = integer("max_volumes", 1, 100000000)) c.maxVolumes = static_cast<std::size_t>(*v);
  if (auto it = o.find("include"); it != o.end()) c.include = List(it->second);
  if (auto it = o.find("exclude"); it != o.end()) c.exclude = List(it->second);
  return c;
}

struct Walker {
  const Config& cfg;
  std::shared_ptr<Holder> holder;
  std::string subdetector;
  std::vector<data::SurfaceElement> out;
  std::map<std::string, std::size_t> unsupported;  // solid type -> count
  std::set<std::string> keys;
  std::size_t* budget;
  bool truncated = false;  ///< The budget ran out during this walk.

  using IDs = std::vector<std::pair<std::string, int>>;

  void Visit(const TGeoNode* node, const geometry::Transform& world, int depth, IDs ids, const std::string& path) {
    if (*budget == 0) {
      truncated = true;
      return;
    }
    const dd4hep::PlacedVolume pv(node);
    if (pv.data())
      for (const auto& id : pv.volIDs()) ids.push_back(id);
    const TGeoVolume* vol = node->GetVolume();
    const bool assembly = vol->IsAssembly();
    const int n = node->GetNdaughters();
    const bool deeper = n > 0 && (depth < cfg.depth || assembly);
    if (deeper) {
      for (int i = 0; i < n; ++i) {
        const TGeoNode* d = node->GetDaughter(i);
        Visit(d, world * geometry::ToTransform(*d->GetMatrix()), depth + 1, ids, path + "/" + d->GetName());
      }
      return;
    }
    if (assembly) return;  // empty assembly
    const TGeoShape* shape = vol->GetShape();
    if (!shape || dynamic_cast<const TGeoCompositeShape*>(shape)) {
      ++unsupported[shape ? shape->ClassName() : "(no shape)"];
      return;
    }
    // The shape is owned by the geometry; the deleter keeps the geometry alive.
    auto keep = holder;
    std::shared_ptr<const TGeoShape> handle(shape, [keep](const TGeoShape*) {});
    data::SurfaceElement e{data::Surface{geometry::Shape(std::make_shared<geometry::TGeoShapeAdapter>(handle), world)}, {}};
    auto set = [&](const std::string& k, data::Metadata::Value v) {
      keys.insert(k);
      e.meta.Set(k, std::move(v));
    };
    set("subdetector", subdetector);
    set("volume", std::string(vol->GetName()));
    set("path", path);
    set("solid", std::string(shape->ClassName()));
    if (vol->GetMaterial()) set("material", std::string(vol->GetMaterial()->GetName()));
    for (const auto& [k, v] : ids) set(k == "system" ? "system_id" : k, static_cast<std::int64_t>(v));
    out.push_back(std::move(e));
    --*budget;
  }
};

class GeometrySeries final : public data::Series<data::SurfaceElement> {
 public:
  explicit GeometrySeries(std::vector<data::SurfaceElement> elements) : m_elements(std::move(elements)) {}
  [[nodiscard]] std::size_t Size() const override { return 1; }
  [[nodiscard]] std::vector<data::SurfaceElement> Load(std::size_t) const override { return m_elements; }

 private:
  std::vector<data::SurfaceElement> m_elements;
};

}  // namespace

Capabilities DetectorAdapter::GetCapabilities() const {
  Capabilities c;
  c.description = "detector geometry from DD4hep compact XML or a TGeo ROOT/GDML file: one surface series per sub-detector";
  c.kinds = {data::ElementKind::Surface};
  c.extensions = {".gdml", ".root", ".xml"};
  c.options = {
      {"depth", "3", "volume levels below each sub-detector; deeper volumes are merged into their ancestor at this level"},
      {"include", "", "comma-separated sub-detector name patterns (* and ?) to load (all if empty)"},
      {"exclude", "", "comma-separated sub-detector name patterns to skip"},
      {"max_volumes", "200000", "stop after this many volumes (the rest is reported as skipped)"},
  };
  c.notes = {
      "series geometry/<sub-detector>, one entry, one surface per volume in world coordinates (mm)",
      "metadata: subdetector, volume, path, material, solid, system_id and the other DD4hep volume IDs",
      "assemblies are walked through; leaf volumes above the depth limit are included",
  };
  c.limitations = {
      "boolean (composite) solids are not tessellated; they are reported as skipped",
      "one geometry per process at a time for TGeo files (ROOT's global gGeoManager)",
  };
  return c;
}

LoadReport DetectorAdapter::Open(data::SessionData& data, const std::string& path, const Options& options) {
  const Config cfg = Parse(options);
  LoadReport report;
  std::error_code ec;
  const auto abs = std::filesystem::weakly_canonical(std::filesystem::absolute(path, ec), ec);
  const std::string full = ec ? path : abs.string();
  report.source = "detector:" + full;
  const std::string label = std::filesystem::path(path).filename().string();
  if (!std::filesystem::exists(full)) {
    report.skipped.push_back({label, "cannot open file"});
    return report;
  }

  // Re-opening: report the series of this source without loading the geometry again.
  {
    bool any = false;
    for (const auto& info : data.List()) {
      if (info.source != report.source) continue;
      any = true;
      RegisteredSeries rs;
      rs.ref = {info.name, info.source};
      rs.kind = data::ElementKind::Surface;
      rs.entries = info.entries;
      rs.description = info.description;
      rs.existing = true;
      report.registered.push_back(rs);
    }
    if (any) {
      report.Sort();
      return report;
    }
  }

  std::lock_guard<std::mutex> lock(GeometryLock());
  auto holder = std::make_shared<Holder>();
  std::vector<std::pair<std::string, const TGeoNode*>> subdetectors;
  std::vector<geometry::Transform> worlds;
  const auto ext = std::filesystem::path(full).extension().string();
  try {
    if (ext == ".xml") {
      static int instances = 0;  // one DD4hep instance per Open (guarded by GeometryLock)
      holder->ddName = "fccvis:" + full + "#" + std::to_string(++instances);
      auto& det = dd4hep::Detector::getInstance(holder->ddName);
      det.fromCompact(full);
      holder->manager = &det.manager();
      for (const auto& [name, handle] : det.detectors()) {
        dd4hep::DetElement de(handle);
        if (!de.isValid() || name == "world" || !de.placement().isValid()) continue;
        subdetectors.emplace_back(name, de.placement().ptr());
        worlds.push_back(geometry::ToTransform(de.nominal().worldTransformation()));
      }
    } else {
      TGeoManager* previous = gGeoManager;
      holder->manager = TGeoManager::Import(full.c_str());
      if (!holder->manager) {
        gGeoManager = previous;
        report.skipped.push_back({label, "not a readable ROOT/GDML geometry"});
        return report;
      }
      holder->ownsManager = true;
      const TGeoNode* top = holder->manager->GetTopNode();
      for (int i = 0; top && i < top->GetNdaughters(); ++i) {
        const TGeoNode* d = top->GetDaughter(i);
        subdetectors.emplace_back(d->GetVolume()->GetName(), d);
        worlds.push_back(geometry::ToTransform(*d->GetMatrix()));
      }
    }
  } catch (const std::exception& e) {
    report.skipped.push_back({label, std::string("geometry could not be loaded: ") + e.what()});
    return report;
  }

  std::size_t budget = cfg.maxVolumes;
  for (std::size_t s = 0; s < subdetectors.size(); ++s) {
    const auto& [name, node] = subdetectors[s];
    const bool included = cfg.include.empty() ||
                          std::any_of(cfg.include.begin(), cfg.include.end(), [&](const auto& p) { return Glob(p, name); });
    const bool excluded = std::any_of(cfg.exclude.begin(), cfg.exclude.end(), [&](const auto& p) { return Glob(p, name); });
    if (!included || excluded) continue;
    const std::string series = "geometry/" + name;
    RegisteredSeries rs;
    rs.ref = {series, report.source};
    rs.kind = data::ElementKind::Surface;
    rs.entries = 1;
    rs.description = "volumes of sub-detector " + name + " from " + label;
    if (data.Status<data::SurfaceElement>(rs.ref) == data::Lookup::Found) {
      rs.existing = true;
      report.registered.push_back(rs);
      continue;
    }
    if (budget == 0) {
      report.skipped.push_back({name, "max_volumes reached"});
      continue;
    }
    Walker w{cfg, holder, name, {}, {}, {}, &budget};
    w.Visit(node, worlds[s], 0, {}, std::string("/") + node->GetName());
    for (const auto& [solid, count] : w.unsupported)
      report.skipped.push_back({name, std::to_string(count) + " volume(s) with unsupported solid " + solid});
    if (w.truncated)
      report.skipped.push_back({name, "max_volumes reached: incomplete, " + std::to_string(w.out.size()) + " volume(s) kept"});
    if (w.out.empty()) {
      report.skipped.push_back({name, "no drawable volumes"});
      continue;
    }
    rs.metadataKeys = w.keys;
    if (data.Add<data::SurfaceElement>(series, report.source, std::make_unique<GeometrySeries>(std::move(w.out)),
                                       rs.description))
      report.registered.push_back(rs);
  }
  report.Sort();
  return report;
}

static Registrar g_detectorRegistrar("detector", [] { return std::make_unique<DetectorAdapter>(); });

}  // namespace fccvis::adapters
