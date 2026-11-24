// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "PhoenixAdapter.h"

#include "AdapterRegistry.h"
#include "Json.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace fccvis::adapters {

namespace {

using json::Value;
using Vec = geometry::Transform::Vector;

// Collection types of the Phoenix event-data format, and the event's own scalars.
const std::set<std::string>& KnownTypes() {
  static const std::set<std::string> t = {"Hits", "Tracks", "Jets", "CaloClusters", "CaloCells", "Vertices",
                                          "MissingEnergy", "Muons", "Electrons", "Photons", "PlanarCaloCells",
                                          "IrregularCaloCells", "Compound"};
  return t;
}

bool IsEventKey(const std::string& k) {
  return KnownTypes().count(k) || k == "event number" || k == "run number" || k == "eventNumber" || k == "runNumber";
}

struct Config {
  std::vector<std::string> include, exclude;
  std::size_t cache = 4;
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

bool Glob(const char* p, const char* s) {
  if (!*p) return !*s;
  if (*p == '*') return Glob(p + 1, s) || (*s && Glob(p, s + 1));
  return *s && (*p == '?' || *p == *s) && Glob(p + 1, s + 1);
}

bool AnyGlob(const std::vector<std::string>& ps, const std::string& s) {
  return std::any_of(ps.begin(), ps.end(), [&](const std::string& p) { return Glob(p.c_str(), s.c_str()); });
}

Config Parse(const Options& options) {
  for (const auto& [k, v] : options)
    if (k != "collections" && k != "exclude" && k != "cache")
      throw std::invalid_argument("unknown option '" + k + "' (collections, exclude, cache)");
  Config c;
  if (auto it = options.find("collections"); it != options.end()) c.include = SplitList(it->second);
  if (auto it = options.find("exclude"); it != options.end()) c.exclude = SplitList(it->second);
  if (auto it = options.find("cache"); it != options.end()) {
    try {
      std::size_t used = 0;
      const long v = std::stol(it->second, &used);
      if (used != it->second.size() || v < 1) throw std::out_of_range("cache");
      c.cache = static_cast<std::size_t>(v);
    } catch (const std::exception&) {
      throw std::invalid_argument("option 'cache' must be a positive integer, got '" + it->second + "'");
    }
  }
  return c;
}

// ---- reading positions and metadata ------------------------------------------

/** [x, y, z] from an array of three numbers. */
bool Triple(const Value& v, Vec& out) {
  if (!v.IsArray()) return false;
  const auto& a = v.AsArray();
  if (a.size() < 3 || !a[0].IsNumber() || !a[1].IsNumber() || !a[2].IsNumber()) return false;
  out = {a[0].AsDouble(), a[1].AsDouble(), a[2].AsDouble()};
  return true;
}

/** Position of a point object: pos: [x, y, z], a plain [x, y, z], or x/y/z members. */
bool PointOf(const Value& item, Vec& out) {
  if (Triple(item, out)) return true;
  if (const Value* pos = item.Find("pos"); pos && Triple(*pos, out)) return true;
  const Value *x = item.Find("x"), *y = item.Find("y"), *z = item.Find("z");
  if (x && y && z && x->IsNumber() && y->IsNumber() && z->IsNumber()) {
    out = {x->AsDouble(), y->AsDouble(), z->AsDouble()};
    return true;
  }
  return false;
}

/** Vertices of a line object: pos: [[x, y, z], ...]. */
bool LineOf(const Value& item, std::vector<Vec>& out) {
  const Value* pos = item.Find("pos");
  if (!pos || !pos->IsArray() || pos->AsArray().empty() || !pos->AsArray().front().IsArray()) return false;
  out.clear();
  for (const auto& p : pos->AsArray()) {
    Vec v;
    if (Triple(p, v)) out.push_back(v);
  }
  return true;
}

enum class Kind { Point, Line, None };

Kind KindOf(const Value& item) {
  Vec p;
  std::vector<Vec> l;
  if (LineOf(item, l)) return Kind::Line;
  if (PointOf(item, p)) return Kind::Point;
  return Kind::None;
}

data::Metadata MetaOf(const Value& item, Kind kind) {
  data::Metadata m;
  if (!item.IsObject()) return m;
  for (const auto& [key, v] : item.AsObject()) {
    if (key == "pos" || (kind == Kind::Point && (key == "x" || key == "y" || key == "z"))) continue;
    if (const auto* b = std::get_if<bool>(&v.v)) m.Set(key, *b);
    else if (const auto* i = std::get_if<std::int64_t>(&v.v)) m.Set(key, *i);
    else if (const auto* d = std::get_if<double>(&v.v)) m.Set(key, *d);
    else if (const auto* s = std::get_if<std::string>(&v.v)) m.Set(key, *s);
    else if (v.IsArray()) {
      const auto& a = v.AsArray();
      if (a.size() > 16 || !std::all_of(a.begin(), a.end(), [](const Value& x) { return x.IsNumber(); })) continue;
      static const char* dparams[] = {"d0", "z0", "phi", "theta", "qOverP"};
      for (std::size_t k = 0; k < a.size(); ++k) {
        const std::string name = key == "dparams" && k < 5 ? dparams[k] : key + "_" + std::to_string(k);
        m.Set(name, a[k].AsDouble());  // array elements are parameters: always numbers
      }
    }
  }
  return m;
}

// ---- the file ----------------------------------------------------------------

struct Collection {
  std::string type, name, series;
  Kind kind = Kind::None;
  std::set<std::string> keys;  // metadata keys seen in the first objects
};

/** The text, where each event is, and the last parsed event. */
struct File {
  std::string text;
  std::vector<std::pair<std::size_t, std::size_t>> events;  // [begin, end) of each event object
  std::mutex mutex;
  std::optional<std::size_t> cachedEntry;
  std::shared_ptr<const Value> cachedEvent;

  std::shared_ptr<const Value> Event(std::size_t entry) {
    std::lock_guard<std::mutex> lock(mutex);
    if (cachedEntry != entry) {
      const auto [b, e] = events.at(entry);
      json::Reader r(std::string_view(text).substr(0, e), b);
      cachedEvent = std::make_shared<const Value>(r.Parse());
      cachedEntry = entry;
    }
    return cachedEvent;
  }
};

const json::Array* Items(const Value& event, const Collection& c) {
  const Value* type = event.Find(c.type);
  if (!type) return nullptr;
  const Value* coll = type->Find(c.name);
  return coll && coll->IsArray() ? &coll->AsArray() : nullptr;
}

template <typename X>
class CollectionSeries final : public data::Series<X> {
 public:
  CollectionSeries(std::shared_ptr<File> f, Collection c) : m_file(std::move(f)), m_c(std::move(c)) {}
  [[nodiscard]] std::size_t Size() const override { return m_file->events.size(); }
  [[nodiscard]] std::vector<X> Load(std::size_t entry) const override {
    const auto event = m_file->Event(entry);
    std::vector<X> out;
    const json::Array* items = Items(*event, m_c);
    if (!items) return out;  // collection absent from this event
    for (const auto& item : *items) {
      if constexpr (std::is_same_v<X, data::PointElement>) {
        Vec p;
        if (!PointOf(item, p)) continue;
        out.push_back({{p}, MetaOf(item, Kind::Point)});
      } else {
        std::vector<Vec> v;
        if (!LineOf(item, v)) continue;
        out.push_back({{std::move(v)}, MetaOf(item, Kind::Line)});
      }
    }
    return out;
  }

 private:
  std::shared_ptr<File> m_file;
  Collection m_c;
};

/** Locates the events: a map of events, or the document itself as one event. */
void FindEvents(File& f, std::vector<std::string>& ignored) {
  json::Reader r(f.text);
  r.Blank();
  const std::size_t docBegin = r.Position();
  r.Expect('{');
  std::vector<std::pair<std::string, std::pair<std::size_t, std::size_t>>> members;
  bool topIsEvent = false;
  if (!r.Accept('}')) {
    do {
      r.Blank();
      std::string key = r.String();
      r.Expect(':');
      if (IsEventKey(key)) topIsEvent = true;
      members.emplace_back(std::move(key), r.SkipValue());
    } while (r.Accept(','));
    r.Expect('}');
  }
  if (!r.AtEnd()) r.Fail("unexpected text after the JSON document");
  if (topIsEvent) {
    f.events.push_back({docBegin, r.Position()});
    return;
  }
  for (const auto& [key, range] : members) {
    json::Reader m(std::string_view(f.text).substr(0, range.second), range.first);
    bool event = false;
    if (m.Accept('{') && !m.Accept('}')) {
      do {
        m.Blank();
        if (IsEventKey(m.String())) event = true;
        m.Expect(':');
        m.SkipValue();
      } while (!event && m.Accept(','));
    }
    if (event) f.events.push_back(range);
    else ignored.push_back(key);
  }
}

/** Collections of all events (first-seen order) with the kind of their objects. */
std::vector<Collection> FindCollections(File& f) {
  std::vector<Collection> out;
  std::map<std::pair<std::string, std::string>, std::size_t> index;
  for (const auto& [begin, end] : f.events) {
    json::Reader r(std::string_view(f.text).substr(0, end), begin);
    r.Expect('{');
    if (r.Accept('}')) continue;
    do {
      r.Blank();
      const std::string type = r.String();
      r.Expect(':');
      r.Blank();
      if (!KnownTypes().count(type) || !r.Accept('{')) {
        r.SkipValue();
        continue;
      }
      if (!r.Accept('}')) {
        do {
          r.Blank();
          const std::string name = r.String();
          r.Expect(':');
          auto [it, fresh] = index.try_emplace({type, name}, out.size());
          if (fresh) out.push_back({type, name, name, Kind::None, {}});
          Collection& c = out[it->second];
          if (c.kind == Kind::None && r.Accept('[')) {  // classify by the first objects
            if (!r.Accept(']')) {
              int seen = 0;
              do {
                const Value item = r.Parse();
                if (c.kind == Kind::None) c.kind = KindOf(item);
                if (c.kind != Kind::None) {
                  const data::Metadata meta = MetaOf(item, c.kind);
                  for (const auto& [k, v] : meta.All()) c.keys.insert(k);
                }
                ++seen;
              } while (seen < 8 && r.Accept(','));
              // skip the rest of the array
              while (r.Accept(',')) r.SkipValue();
              r.Expect(']');
            }
          } else {
            r.SkipValue();
          }
        } while (r.Accept(','));
        r.Expect('}');
      }
    } while (r.Accept(','));
  }
  // A name used by two types gets the type as prefix.
  std::map<std::string, int> uses;
  for (const auto& c : out) ++uses[c.name];
  for (auto& c : out)
    if (uses[c.name] > 1) c.series = c.type + "/" + c.name;
  return out;
}

}  // namespace

Capabilities PhoenixAdapter::GetCapabilities() const {
  Capabilities c;
  c.description = "Phoenix event-data JSON: one point or line series per collection, one entry per event";
  c.kinds = {data::ElementKind::Point, data::ElementKind::Line};
  c.extensions = {".json"};
  c.options = {
      {"collections", "", "comma-separated collection name patterns (* and ?) to register (all if empty)"},
      {"exclude", "", "comma-separated collection name patterns to skip"},
      {"cache", "4", "parsed events kept in memory per series"},
  };
  c.notes = {
      "points: objects with pos [x, y, z], a plain [x, y, z] array, or x, y, z members (Hits, Vertices, ...)",
      "lines: objects with pos [[x, y, z], ...] (Tracks): the given positions, not extrapolated",
      "metadata: the other members; dparams becomes d0, z0, phi, theta, qOverP; positions in mm",
      "a file with event-number or collection-type keys at the top is read as one event",
  };
  c.limitations = {
      "collections given by eta/phi only (jets, calorimeter clusters and cells, missing energy) are skipped",
      "the whole file is read into memory; events are parsed when first needed",
  };
  return c;
}

LoadReport PhoenixAdapter::Open(data::SessionData& data, const std::string& path, const Options& options) {
  const Config cfg = Parse(options);
  LoadReport report;
  std::error_code ec;
  const auto abs = std::filesystem::weakly_canonical(std::filesystem::absolute(path, ec), ec);
  const std::string full = ec ? path : abs.string();
  report.source = "phoenix:" + full;
  const std::string label = std::filesystem::path(path).filename().string();
  std::ifstream in(full, std::ios::binary);
  if (!in || !std::filesystem::is_regular_file(full)) {
    report.skipped.push_back({label, "cannot open file"});
    return report;
  }
  auto file = std::make_shared<File>();
  std::ostringstream ss;
  ss << in.rdbuf();
  file->text = ss.str();

  std::vector<std::string> ignored;
  std::vector<Collection> collections;
  try {
    FindEvents(*file, ignored);
    collections = FindCollections(*file);
  } catch (const json::Error& e) {
    report.skipped.push_back({label, std::string("not valid JSON: ") + e.what()});
    return report;
  }
  for (const auto& k : ignored) report.skipped.push_back({k, "not an event (no collection types or event number)"});
  if (file->events.empty()) {
    report.skipped.push_back({label, "no events"});
    return report;
  }

  for (const auto& c : collections) {
    if (!cfg.include.empty() && !AnyGlob(cfg.include, c.name) && !AnyGlob(cfg.include, c.series)) continue;
    if (AnyGlob(cfg.exclude, c.name) || AnyGlob(cfg.exclude, c.series)) continue;
    if (c.kind == Kind::None) {
      report.skipped.push_back({c.type + "/" + c.name, "no positions (Phoenix draws these from eta/phi or parameters)"});
      continue;
    }
    RegisteredSeries rs;
    rs.ref = {c.series, report.source};
    rs.kind = c.kind == Kind::Point ? data::ElementKind::Point : data::ElementKind::Line;
    rs.entries = file->events.size();
    rs.metadataKeys = c.keys;
    rs.description = c.type + " collection '" + c.name + "' from " + label;
    bool added = false;
    if (c.kind == Kind::Point) {
      rs.existing = data.Status<data::PointElement>(rs.ref) == data::Lookup::Found;
      added = rs.existing || data.Add<data::PointElement>(c.series, report.source,
                                                          std::make_unique<CollectionSeries<data::PointElement>>(file, c),
                                                          rs.description, cfg.cache);
    } else {
      rs.existing = data.Status<data::LineElement>(rs.ref) == data::Lookup::Found;
      added = rs.existing || data.Add<data::LineElement>(c.series, report.source,
                                                         std::make_unique<CollectionSeries<data::LineElement>>(file, c),
                                                         rs.description, cfg.cache);
    }
    if (added) report.registered.push_back(std::move(rs));
    else report.skipped.push_back({c.series, "could not register the series"});
  }
  if (report.registered.empty() && report.skipped.empty()) {
    if (collections.empty())
      report.skipped.push_back({label, "no Phoenix collections (Hits, Tracks, Vertices, ...): not in the Phoenix event-data format"});
    else
      report.skipped.push_back({label, "no collection matches the 'collections'/'exclude' options"});
  }
  report.Sort();
  return report;
}

static Registrar g_phoenixRegistrar("phoenix", [] { return std::make_unique<PhoenixAdapter>(); });

}  // namespace fccvis::adapters
