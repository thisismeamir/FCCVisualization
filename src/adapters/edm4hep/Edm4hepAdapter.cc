// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "Edm4hepAdapter.h"

#include "AdapterRegistry.h"

#include <edm4hep/CalorimeterHitCollection.h>
#include <edm4hep/ClusterCollection.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/ReconstructedParticleCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/TrackCollection.h>
#include <edm4hep/TrackerHit3DCollection.h>
#include <edm4hep/TrackerHitPlaneCollection.h>
#include <podio/Frame.h>
#include <podio/Reader.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>

namespace fccvis::adapters {

namespace {

using Vec = geometry::Transform::Vector;

struct Config {
  std::vector<std::string> include, exclude;
  bool trackHits = false;       // track=hits
  double mcLength = 1000;       // mm, MCParticle without endpoint
  double recoLength = 1000;     // mm, ReconstructedParticle segment
  std::size_t cache = 4;        // cached entries per series
};

std::vector<std::string> SplitList(const std::string& s) {
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

/** Glob match with * and ?. */
bool Glob(const char* p, const char* s) {
  if (!*p) return !*s;
  if (*p == '*') return Glob(p + 1, s) || (*s && Glob(p, s + 1));
  return *s && (*p == '?' || *p == *s) && Glob(p + 1, s + 1);
}

bool AnyGlob(const std::vector<std::string>& patterns, const std::string& s) {
  return std::any_of(patterns.begin(), patterns.end(), [&](const std::string& p) { return Glob(p.c_str(), s.c_str()); });
}

double Positive(const Options& o, const char* key, double fallback) {
  auto it = o.find(key);
  if (it == o.end()) return fallback;
  try {
    std::size_t used = 0;
    const double v = std::stod(it->second, &used);
    if (used == it->second.size() && v > 0) return v;
  } catch (const std::exception&) {
  }
  throw std::invalid_argument(std::string("option '") + key + "' must be a positive number, got '" + it->second + "'");
}

Config Parse(const Options& options) {
  static const std::vector<std::string> known = {"collections", "exclude", "track", "mc_length", "reco_length", "cache"};
  for (const auto& [k, v] : options)
    if (std::find(known.begin(), known.end(), k) == known.end())
      throw std::invalid_argument("unknown option '" + k + "' (collections, exclude, track, mc_length, reco_length, cache)");
  Config c;
  if (auto it = options.find("collections"); it != options.end()) c.include = SplitList(it->second);
  if (auto it = options.find("exclude"); it != options.end()) c.exclude = SplitList(it->second);
  if (auto it = options.find("track"); it != options.end()) {
    if (it->second != "states" && it->second != "hits")
      throw std::invalid_argument("option 'track' must be 'states' or 'hits', got '" + it->second + "'");
    c.trackHits = it->second == "hits";
  }
  c.mcLength = Positive(options, "mc_length", c.mcLength);
  c.recoLength = Positive(options, "reco_length", c.recoLength);
  c.cache = static_cast<std::size_t>(Positive(options, "cache", static_cast<double>(c.cache)));
  return c;
}

/** One open file; podio readers are not thread-safe, so every read goes through the lock. */
struct File {
  std::mutex mutex;
  std::unique_ptr<podio::Reader> reader;
  std::size_t entries = 0;
  std::optional<std::size_t> cachedEntry;
  std::shared_ptr<const podio::Frame> cachedFrame;  // last event read (all collections, so relations resolve)

  std::shared_ptr<const podio::Frame> Event(std::size_t entry) {
    std::lock_guard<std::mutex> lock(mutex);
    if (cachedEntry != entry) {
      cachedFrame = std::make_shared<const podio::Frame>(reader->readEvent(entry));
      cachedEntry = entry;
    }
    return cachedFrame;
  }
};

/** Stores an object reference as two integers that stay exact as doubles (filters compare numbers as double). */
void SetRef(data::Metadata& m, const std::string& key, const podio::ObjectID& id) {
  m.Set(key, static_cast<std::int64_t>(id.index));
  m.Set(key + "_collection", static_cast<std::int64_t>(id.collectionID));
}

template <typename V>
Vec ToVec(const V& v) {
  return {static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z)};
}

template <typename V>
double Norm(const V& v) {
  return std::sqrt(static_cast<double>(v.x) * v.x + static_cast<double>(v.y) * v.y + static_cast<double>(v.z) * v.z);
}

template <typename V>
double Transverse(const V& v) {
  return std::sqrt(static_cast<double>(v.x) * v.x + static_cast<double>(v.y) * v.y);
}

/** Segment from @p start, @p length mm along @p dir (a single point when dir is zero). */
template <typename V>
std::vector<Vec> Ray(const Vec& start, const V& dir, double length) {
  const double n = Norm(dir);
  if (!(n > 0)) return {start, start};
  return {start, {start[0] + length * dir.x / n, start[1] + length * dir.y / n, start[2] + length * dir.z / n}};
}

void Common(data::Metadata& m, const podio::ObjectID& id) { SetRef(m, "index", id); }

/** What a mapped collection type produces. */
struct Mapping {
  data::ElementKind kind;
  std::string what;                         // description
  std::vector<std::string> keys;            // metadata keys
  std::map<std::string, std::string> units; // key -> unit
};

const std::map<std::string, Mapping>& Mappings() {
  static const std::map<std::string, Mapping> m = {
      {"edm4hep::SimTrackerHit",
       {data::ElementKind::Point, "simulated tracker hits at their position",
        {"cellID", "EDep", "time", "pathLength", "quality", "p", "particle", "particle_collection", "index", "index_collection"},
        {{"EDep", "GeV"}, {"time", "ns"}, {"pathLength", "mm"}, {"p", "GeV"}}}},
      {"edm4hep::TrackerHit3D",
       {data::ElementKind::Point, "tracker hits at their position",
        {"cellID", "EDep", "time", "type", "quality", "index", "index_collection"}, {{"EDep", "GeV"}, {"time", "ns"}}}},
      {"edm4hep::TrackerHitPlane",
       {data::ElementKind::Point, "planar tracker hits at their position",
        {"cellID", "EDep", "time", "type", "quality", "index", "index_collection"}, {{"EDep", "GeV"}, {"time", "ns"}}}},
      {"edm4hep::CalorimeterHit",
       {data::ElementKind::Point, "calorimeter hits at their position",
        {"cellID", "energy", "time", "type", "index", "index_collection"}, {{"energy", "GeV"}, {"time", "ns"}}}},
      {"edm4hep::SimCalorimeterHit",
       {data::ElementKind::Point, "simulated calorimeter hits at their position",
        {"cellID", "energy", "contributions", "index", "index_collection"}, {{"energy", "GeV"}}}},
      {"edm4hep::Cluster",
       {data::ElementKind::Point, "clusters at their position",
        {"energy", "type", "hits", "clusters", "index", "index_collection"}, {{"energy", "GeV"}}}},
      {"edm4hep::Track",
       {data::ElementKind::Line, "tracks as polylines",
        {"type", "chi2", "ndf", "hits", "states", "index", "index_collection"}, {}}},
      {"edm4hep::MCParticle",
       {data::ElementKind::Line, "MC particles as segments from vertex to endpoint",
        {"PDG", "charge", "energy", "mass", "p", "pt", "generatorStatus", "simulatorStatus", "endpoint_set", "parents",
         "daughters", "index", "index_collection"},
        {{"energy", "GeV"}, {"mass", "GeV"}, {"p", "GeV"}, {"pt", "GeV"}}}},
      {"edm4hep::ReconstructedParticle",
       {data::ElementKind::Line, "reconstructed particles as segments along their momentum",
        {"PDG", "charge", "energy", "mass", "p", "pt", "goodnessOfPID", "tracks", "clusters", "particles", "index", "index_collection"},
        {{"energy", "GeV"}, {"mass", "GeV"}, {"p", "GeV"}, {"pt", "GeV"}}}},
  };
  return m;
}

/** Why an unmapped type is skipped. */
std::string SkipReason(const std::string& type) {
  if (type == "edm4hep::RawCalorimeterHit") return "raw calorimeter hits have no position";
  if (type == "edm4hep::EventHeader") return "event header: no positions";
  if (type.find("Link") != std::string::npos) return "link collection: no positions (relations are not joined)";
  return "type " + type + " is not mapped";
}

void Units(data::Metadata& m, const std::string& type) {
  for (const auto& [k, u] : Mappings().at(type).units) m.Set("unit:" + k, u);
}

// ---- conversions -----------------------------------------------------------

std::vector<data::PointElement> Points(const podio::CollectionBase& coll, const std::string& type) {
  std::vector<data::PointElement> out;
  auto add = [&](const Vec& p) -> data::Metadata& {
    out.push_back({{p}, {}});
    Units(out.back().meta, type);
    return out.back().meta;
  };
  if (const auto* c = dynamic_cast<const edm4hep::SimTrackerHitCollection*>(&coll)) {
    for (const auto& h : *c) {
      auto& m = add(ToVec(h.getPosition()));
      m.Set("cellID", static_cast<std::int64_t>(h.getCellID()));
      m.Set("EDep", static_cast<double>(h.getEDep()));
      m.Set("time", static_cast<double>(h.getTime()));
      m.Set("pathLength", static_cast<double>(h.getPathLength()));
      m.Set("quality", static_cast<std::int64_t>(h.getQuality()));
      m.Set("p", Norm(h.getMomentum()));
      if (const auto p = h.getParticle(); p.isAvailable()) SetRef(m, "particle", p.getObjectID());
      Common(m, h.getObjectID());
    }
  } else if (const auto* c3 = dynamic_cast<const edm4hep::TrackerHit3DCollection*>(&coll)) {
    for (const auto& h : *c3) {
      auto& m = add(ToVec(h.getPosition()));
      m.Set("cellID", static_cast<std::int64_t>(h.getCellID()));
      m.Set("EDep", static_cast<double>(h.getEDep()));
      m.Set("time", static_cast<double>(h.getTime()));
      m.Set("type", static_cast<std::int64_t>(h.getType()));
      m.Set("quality", static_cast<std::int64_t>(h.getQuality()));
      Common(m, h.getObjectID());
    }
  } else if (const auto* cp = dynamic_cast<const edm4hep::TrackerHitPlaneCollection*>(&coll)) {
    for (const auto& h : *cp) {
      auto& m = add(ToVec(h.getPosition()));
      m.Set("cellID", static_cast<std::int64_t>(h.getCellID()));
      m.Set("EDep", static_cast<double>(h.getEDep()));
      m.Set("time", static_cast<double>(h.getTime()));
      m.Set("type", static_cast<std::int64_t>(h.getType()));
      m.Set("quality", static_cast<std::int64_t>(h.getQuality()));
      Common(m, h.getObjectID());
    }
  } else if (const auto* cc = dynamic_cast<const edm4hep::CalorimeterHitCollection*>(&coll)) {
    for (const auto& h : *cc) {
      auto& m = add(ToVec(h.getPosition()));
      m.Set("cellID", static_cast<std::int64_t>(h.getCellID()));
      m.Set("energy", static_cast<double>(h.getEnergy()));
      m.Set("time", static_cast<double>(h.getTime()));
      m.Set("type", static_cast<std::int64_t>(h.getType()));
      Common(m, h.getObjectID());
    }
  } else if (const auto* cs = dynamic_cast<const edm4hep::SimCalorimeterHitCollection*>(&coll)) {
    for (const auto& h : *cs) {
      auto& m = add(ToVec(h.getPosition()));
      m.Set("cellID", static_cast<std::int64_t>(h.getCellID()));
      m.Set("energy", static_cast<double>(h.getEnergy()));
      m.Set("contributions", static_cast<std::int64_t>(h.getContributions().size()));
      Common(m, h.getObjectID());
    }
  } else if (const auto* cl = dynamic_cast<const edm4hep::ClusterCollection*>(&coll)) {
    for (const auto& h : *cl) {
      auto& m = add(ToVec(h.getPosition()));
      m.Set("energy", static_cast<double>(h.getEnergy()));
      m.Set("type", static_cast<std::int64_t>(h.getType()));
      m.Set("hits", static_cast<std::int64_t>(h.getHits().size()));
      m.Set("clusters", static_cast<std::int64_t>(h.getClusters().size()));
      Common(m, h.getObjectID());
    }
  }
  return out;
}

int LocationRank(std::int32_t location) {
  // AtIP, AtFirstHit, AtLastHit, AtCalorimeter, AtVertex, then AtOther (0) and unknown values.
  return location >= 1 ? location : 1000;
}

std::vector<data::LineElement> Lines(const podio::CollectionBase& coll, const std::string& type, const Config& cfg) {
  std::vector<data::LineElement> out;
  auto add = [&](std::vector<Vec> v) -> data::Metadata& {
    out.push_back({{std::move(v)}, {}});
    Units(out.back().meta, type);
    return out.back().meta;
  };
  if (const auto* c = dynamic_cast<const edm4hep::TrackCollection*>(&coll)) {
    for (const auto& t : *c) {
      std::vector<Vec> v;
      if (cfg.trackHits) {
        for (const auto& h : t.getTrackerHits()) v.push_back(ToVec(h.getPosition()));
        std::stable_sort(v.begin(), v.end(), [](const Vec& a, const Vec& b) {
          return a[0] * a[0] + a[1] * a[1] + a[2] * a[2] < b[0] * b[0] + b[1] * b[1] + b[2] * b[2];
        });
      } else {
        std::vector<edm4hep::TrackState> states(t.getTrackStates().begin(), t.getTrackStates().end());
        std::stable_sort(states.begin(), states.end(), [](const edm4hep::TrackState& a, const edm4hep::TrackState& b) {
          return LocationRank(a.location) < LocationRank(b.location);
        });
        for (const auto& s : states)
          v.push_back({s.referencePoint.x - s.D0 * std::sin(s.phi), s.referencePoint.y + s.D0 * std::cos(s.phi),
                       static_cast<double>(s.referencePoint.z) + s.Z0});
      }
      auto& m = add(std::move(v));
      m.Set("type", static_cast<std::int64_t>(t.getType()));
      m.Set("chi2", static_cast<double>(t.getChi2()));
      m.Set("ndf", static_cast<std::int64_t>(t.getNdf()));
      m.Set("hits", static_cast<std::int64_t>(t.getTrackerHits().size()));
      m.Set("states", static_cast<std::int64_t>(t.getTrackStates().size()));
      Common(m, t.getObjectID());
    }
  } else if (const auto* cm = dynamic_cast<const edm4hep::MCParticleCollection*>(&coll)) {
    for (const auto& p : *cm) {
      const Vec a = ToVec(p.getVertex()), b = ToVec(p.getEndpoint());
      const bool endpoint = a != b;
      auto& m = add(endpoint ? std::vector<Vec>{a, b} : Ray(a, p.getMomentum(), cfg.mcLength));
      m.Set("PDG", static_cast<std::int64_t>(p.getPDG()));
      m.Set("charge", static_cast<double>(p.getCharge()));
      m.Set("energy", p.getEnergy());
      m.Set("mass", p.getMass());
      m.Set("p", Norm(p.getMomentum()));
      m.Set("pt", Transverse(p.getMomentum()));
      m.Set("generatorStatus", static_cast<std::int64_t>(p.getGeneratorStatus()));
      m.Set("simulatorStatus", static_cast<std::int64_t>(p.getSimulatorStatus()));
      m.Set("endpoint_set", endpoint);
      m.Set("parents", static_cast<std::int64_t>(p.getParents().size()));
      m.Set("daughters", static_cast<std::int64_t>(p.getDaughters().size()));
      Common(m, p.getObjectID());
    }
  } else if (const auto* cr = dynamic_cast<const edm4hep::ReconstructedParticleCollection*>(&coll)) {
    for (const auto& r : *cr) {
      auto& m = add(Ray(ToVec(r.getReferencePoint()), r.getMomentum(), cfg.recoLength));
      m.Set("PDG", static_cast<std::int64_t>(r.getPDG()));
      m.Set("charge", static_cast<double>(r.getCharge()));
      m.Set("energy", static_cast<double>(r.getEnergy()));
      m.Set("mass", static_cast<double>(r.getMass()));
      m.Set("p", Norm(r.getMomentum()));
      m.Set("pt", Transverse(r.getMomentum()));
      m.Set("goodnessOfPID", static_cast<double>(r.getGoodnessOfPID()));
      m.Set("tracks", static_cast<std::int64_t>(r.getTracks().size()));
      m.Set("clusters", static_cast<std::int64_t>(r.getClusters().size()));
      m.Set("particles", static_cast<std::int64_t>(r.getParticles().size()));
      Common(m, r.getObjectID());
    }
  }
  return out;
}

/** Series of one collection; reads event frames lazily through the shared File. */
template <typename X>
class CollectionSeries final : public data::Series<X> {
 public:
  CollectionSeries(std::shared_ptr<File> file, std::string name, std::string type, Config cfg)
      : m_file(std::move(file)), m_name(std::move(name)), m_type(std::move(type)), m_cfg(std::move(cfg)) {}
  [[nodiscard]] std::size_t Size() const override { return m_file->entries; }
  [[nodiscard]] std::vector<X> Load(std::size_t entry) const override {
    const auto frame = m_file->Event(entry);
    const podio::CollectionBase* coll = frame->get(m_name);
    if (!coll) return {};  // collection absent from this event
    if constexpr (std::is_same_v<X, data::PointElement>) return Points(*coll, m_type);
    else return Lines(*coll, m_type, m_cfg);
  }

 private:
  std::shared_ptr<File> m_file;
  std::string m_name, m_type;
  Config m_cfg;
};

}  // namespace

Capabilities Edm4hepAdapter::GetCapabilities() const {
  Capabilities c;
  c.description = "EDM4hep event data (podio files): one point or line series per collection, one entry per event";
  c.kinds = {data::ElementKind::Point, data::ElementKind::Line};
  c.extensions = {".root"};
  c.options = {
      {"collections", "", "comma-separated collection name patterns (* and ?) to register (all if empty)"},
      {"exclude", "", "comma-separated collection name patterns to skip"},
      {"track", "states", "track polyline through the track states ('states') or the track's hits ('hits')"},
      {"mc_length", "1000", "segment length (mm) along the momentum for MC particles whose endpoint is not set"},
      {"reco_length", "1000", "segment length (mm) along the momentum for reconstructed particles"},
      {"cache", "4", "converted events kept in memory per series"},
  };
  c.notes = {
      "points: SimTrackerHit, TrackerHit3D, TrackerHitPlane, CalorimeterHit, SimCalorimeterHit, Cluster (stored position)",
      "lines: Track (track-state positions ordered IP, first hit, last hit, calorimeter; or hits), MCParticle (vertex to "
      "endpoint), ReconstructedParticle (reference point along the momentum)",
      "metadata: EDM4hep member names (cellID, EDep, energy, time, PDG, charge, generatorStatus, ...), p, pt, relation "
      "counts; index and index_collection identify the object (podio index and collection ID); units mm, GeV, ns",
  };
  c.limitations = {
      "collections are discovered in the first event; types other than the ones above are skipped with the reason",
      "relations are not joined: a single relation is stored as the related object's index and collection ID "
      "(particle, particle_collection), multiple ones as counts",
      "tracks are not extrapolated (no helix): the polyline joins the stored states",
      "cellID is stored as a signed 64-bit integer",
  };
  return c;
}

LoadReport Edm4hepAdapter::Open(data::SessionData& data, const std::string& path, const Options& options) {
  const Config cfg = Parse(options);
  LoadReport report;
  std::error_code ec;
  const auto abs = std::filesystem::weakly_canonical(std::filesystem::absolute(path, ec), ec);
  const std::string full = ec ? path : abs.string();
  report.source = "edm4hep:" + full;
  const std::string label = std::filesystem::path(path).filename().string();
  if (!std::filesystem::is_regular_file(full)) {
    report.skipped.push_back({label, "cannot open file"});
    return report;
  }

  auto file = std::make_shared<File>();
  std::vector<std::pair<std::string, std::string>> collections;  // name, value type
  try {
    file->reader = std::make_unique<podio::Reader>(podio::makeReader(full));
    file->entries = file->reader->getEvents();
    if (file->entries == 0) {
      report.skipped.push_back({label, "no events (podio category 'events' is empty or missing)"});
      return report;
    }
    const auto first = file->Event(0);
    for (const auto& name : first->getAvailableCollections())
      if (const auto* coll = first->get(name)) collections.emplace_back(name, std::string(coll->getValueTypeName()));
  } catch (const std::exception& e) {
    report.skipped.push_back({label, std::string("not readable by podio: ") + e.what()});
    return report;
  }

  for (const auto& [name, type] : collections) {
    if (!cfg.include.empty() && !AnyGlob(cfg.include, name)) continue;
    if (AnyGlob(cfg.exclude, name)) continue;
    auto it = Mappings().find(type);
    if (it == Mappings().end()) {
      report.skipped.push_back({name, SkipReason(type)});
      continue;
    }
    const Mapping& map = it->second;
    RegisteredSeries rs;
    rs.ref = {name, report.source};
    rs.kind = map.kind;
    rs.entries = file->entries;
    rs.description = map.what + " (" + type + ") from " + label;
    for (const auto& k : map.keys) rs.metadataKeys.insert(k);
    for (const auto& [k, u] : map.units) rs.metadataKeys.insert("unit:" + k);
    bool added = false;
    if (map.kind == data::ElementKind::Point) {
      rs.existing = data.Status<data::PointElement>(rs.ref) == data::Lookup::Found;
      added = rs.existing || data.Add<data::PointElement>(
                                 name, report.source,
                                 std::make_unique<CollectionSeries<data::PointElement>>(file, name, type, cfg),
                                 rs.description, cfg.cache);
    } else {
      rs.existing = data.Status<data::LineElement>(rs.ref) == data::Lookup::Found;
      added = rs.existing || data.Add<data::LineElement>(
                                 name, report.source,
                                 std::make_unique<CollectionSeries<data::LineElement>>(file, name, type, cfg),
                                 rs.description, cfg.cache);
    }
    if (added) report.registered.push_back(std::move(rs));
    else report.skipped.push_back({name, "could not register the series"});
  }
  if (report.registered.empty() && report.skipped.empty())
    report.skipped.push_back({label, "no collection matches the 'collections'/'exclude' options"});
  report.Sort();
  return report;
}

static Registrar g_edm4hepRegistrar("edm4hep", [] { return std::make_unique<Edm4hepAdapter>(); });

}  // namespace fccvis::adapters
