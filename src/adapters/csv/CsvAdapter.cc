// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "CsvAdapter.h"

#include "AdapterRegistry.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fccvis::adapters {

namespace {

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------
enum class Kind { Point, Line };
enum class Type { Int, Double, Bool, String };

struct Config {
  char delimiter = ',';
  std::string comment = "#";
  std::string x = "x", y = "y", z = "z";
  Kind kind = Kind::Point;
  std::string group, order, entry;
  std::vector<std::string> columns;  // empty: all others
  bool columnsGiven = false;
  std::string name;
  bool strict = false;
  std::size_t cache = 4;
};

std::string Lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

std::string Trim(const std::string& s) {
  std::size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}

std::vector<std::string> SplitList(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == ',') {
      if (!Trim(cur).empty()) out.push_back(Trim(cur));
      cur.clear();
    } else {
      cur += c;
    }
  }
  if (!Trim(cur).empty()) out.push_back(Trim(cur));
  return out;
}

bool ParseBoolOption(const std::string& key, const std::string& v) {
  const auto l = Lower(v);
  if (l == "true" || l == "1" || l == "yes" || l == "on") return true;
  if (l == "false" || l == "0" || l == "no" || l == "off") return false;
  throw std::invalid_argument("option '" + key + "' must be true or false, got '" + v + "'");
}

const std::map<std::string, std::string>& KnownOptions() {
  static const std::map<std::string, std::string> k = {
      {"cache", ""}, {"columns", ""}, {"comment", ""}, {"delimiter", ""}, {"entry", ""}, {"group", ""},
      {"kind", ""},  {"name", ""},    {"order", ""},   {"strict", ""},    {"x", ""},     {"y", ""},
      {"z", ""}};
  return k;
}

Config ParseOptions(const Options& o, const std::string& path) {
  Config c;
  for (const auto& [k, v] : o)
    if (!KnownOptions().count(k)) throw std::invalid_argument("unknown csv option '" + k + "'");
  auto get = [&](const char* k) -> std::optional<std::string> {
    auto it = o.find(k);
    if (it == o.end()) return std::nullopt;
    return it->second;
  };
  if (auto d = get("delimiter")) {
    if (*d == "tab" || *d == "\\t") c.delimiter = '\t';
    else if (*d == "space") c.delimiter = ' ';
    else if (d->size() == 1) c.delimiter = (*d)[0];
    else throw std::invalid_argument("option 'delimiter' must be one character, 'tab' or 'space'");
  }
  if (auto v = get("comment")) c.comment = *v;
  if (auto v = get("x")) c.x = *v;
  if (auto v = get("y")) c.y = *v;
  if (auto v = get("z")) c.z = *v;
  if (auto v = get("kind")) {
    const auto l = Lower(*v);
    if (l == "point") c.kind = Kind::Point;
    else if (l == "line") c.kind = Kind::Line;
    else throw std::invalid_argument("option 'kind' must be point or line, got '" + *v + "'");
  }
  if (auto v = get("group")) c.group = *v;
  if (auto v = get("order")) c.order = *v;
  if (auto v = get("entry")) c.entry = *v;
  if (auto v = get("columns")) {
    c.columns = SplitList(*v);
    c.columnsGiven = true;
  }
  if (auto v = get("strict")) c.strict = ParseBoolOption("strict", *v);
  if (auto v = get("cache")) {
    std::size_t n = 0;
    const auto* b = v->data();
    auto [p, ec] = std::from_chars(b, b + v->size(), n);
    if (ec != std::errc() || p != b + v->size() || n == 0)
      throw std::invalid_argument("option 'cache' must be a positive integer");
    c.cache = n;
  }
  if (c.kind == Kind::Line && c.group.empty())
    throw std::invalid_argument("kind=line needs option 'group' (the column naming the line of each row)");
  if (c.kind == Kind::Point && (!c.group.empty() || !c.order.empty()))
    throw std::invalid_argument("options 'group' and 'order' apply to kind=line only");
  c.name = get("name").value_or(std::filesystem::path(path).stem().string());
  if (c.name.empty()) c.name = "csv";
  return c;
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

/** Splits one record; double quotes protect delimiters, "" is a quote. */
std::vector<std::string> SplitRecord(const std::string& line, char delim) {
  std::vector<std::string> out;
  std::string cur;
  bool quoted = false, wasQuoted = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char ch = line[i];
    if (quoted) {
      if (ch == '"') {
        if (i + 1 < line.size() && line[i + 1] == '"') {
          cur += '"';
          ++i;
        } else {
          quoted = false;
        }
      } else {
        cur += ch;
      }
    } else if (ch == '"' && Trim(cur).empty()) {
      quoted = true;
      wasQuoted = true;
      cur.clear();
    } else if (ch == delim) {
      out.push_back(wasQuoted ? cur : Trim(cur));
      cur.clear();
      wasQuoted = false;
    } else {
      cur += ch;
    }
  }
  out.push_back(wasQuoted ? cur : Trim(cur));
  if (delim == ' ') {  // runs of spaces separate one field
    std::vector<std::string> packed;
    for (auto& f : out)
      if (!f.empty()) packed.push_back(std::move(f));
    return packed;
  }
  return out;
}

std::optional<std::int64_t> AsInt(const std::string& s) {
  std::int64_t v = 0;
  const char* b = s.data();
  const char* e = b + s.size();
  if (b != e && *b == '+') ++b;
  auto [p, ec] = std::from_chars(b, e, v);
  if (ec != std::errc() || p != e || b == e) return std::nullopt;
  return v;
}

std::optional<double> AsDouble(const std::string& s) {
  double v = 0;
  const char* b = s.data();
  const char* e = b + s.size();
  if (b != e && *b == '+') ++b;
  auto [p, ec] = std::from_chars(b, e, v);
  if (ec != std::errc() || p != e || b == e) return std::nullopt;
  return v;
}

std::optional<bool> AsBool(const std::string& s) {
  const auto l = Lower(s);
  if (l == "true") return true;
  if (l == "false") return false;
  return std::nullopt;
}

/** "energy [GeV]" -> {"energy", "GeV"}. */
std::pair<std::string, std::string> SplitUnit(const std::string& header) {
  const std::string h = Trim(header);
  if (!h.empty() && h.back() == ']') {
    const auto open = h.rfind('[');
    if (open != std::string::npos && open > 0)
      return {Trim(h.substr(0, open)), Trim(h.substr(open + 1, h.size() - open - 2))};
  }
  return {h, ""};
}

std::optional<double> LengthToMm(const std::string& unit) {
  if (unit.empty() || unit == "mm") return 1.0;
  if (unit == "um" || unit == "µm") return 1e-3;
  if (unit == "cm") return 10.0;
  if (unit == "m") return 1000.0;
  return std::nullopt;
}

bool IsSkippable(const std::string& line, const std::string& comment) {
  const auto t = Trim(line);
  return t.empty() || (!comment.empty() && t.rfind(comment, 0) == 0);
}

// ---------------------------------------------------------------------------
// Index built by the scan; shared by the series
// ---------------------------------------------------------------------------
struct Column {
  std::string name, unit;
  std::size_t index = 0;
  Type type = Type::String;
};

struct Index {
  std::string path;
  Config config;
  std::size_t fields = 0;
  std::size_t ix = 0, iy = 0, iz = 0;
  std::optional<std::size_t> igroup, iorder;
  double scale = 1.0;  // coordinates -> mm
  std::vector<Column> meta;
  std::vector<std::vector<std::streamoff>> entries;  // row offsets per entry
};

data::Metadata::Value Convert(const std::string& s, Type t) {
  switch (t) {
    case Type::Int: return *AsInt(s);
    case Type::Double: return *AsDouble(s);
    case Type::Bool: return *AsBool(s);
    case Type::String: break;
  }
  return s;
}

/** Reads the rows of one entry; rows that became unreadable are dropped. */
std::vector<std::vector<std::string>> ReadRows(const Index& idx, std::size_t entry) {
  std::vector<std::vector<std::string>> rows;
  std::ifstream in(idx.path, std::ios::binary);
  if (!in) return rows;
  std::string line;
  for (auto off : idx.entries.at(entry)) {
    in.clear();
    in.seekg(off);
    if (!std::getline(in, line)) continue;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    auto f = SplitRecord(line, idx.config.delimiter);
    if (f.size() != idx.fields) continue;
    if (!AsDouble(f[idx.ix]) || !AsDouble(f[idx.iy]) || !AsDouble(f[idx.iz])) continue;
    rows.push_back(std::move(f));
  }
  return rows;
}

void FillMeta(const Index& idx, const std::vector<std::string>& row, data::Metadata& m) {
  for (const auto& c : idx.meta) {
    const auto& v = row[c.index];
    if (v.empty()) continue;
    m.Set(c.name, Convert(v, c.type));
    if (!c.unit.empty()) m.Set("unit:" + c.name, c.unit);
  }
}

geometry::Transform::Vector Position(const Index& idx, const std::vector<std::string>& row) {
  return {*AsDouble(row[idx.ix]) * idx.scale, *AsDouble(row[idx.iy]) * idx.scale,
          *AsDouble(row[idx.iz]) * idx.scale};
}

class PointSeries final : public data::Series<data::PointElement> {
 public:
  explicit PointSeries(std::shared_ptr<const Index> idx) : m_idx(std::move(idx)) {}
  [[nodiscard]] std::size_t Size() const override { return m_idx->entries.size(); }
  [[nodiscard]] std::vector<data::PointElement> Load(std::size_t entry) const override {
    std::vector<data::PointElement> out;
    for (const auto& row : ReadRows(*m_idx, entry)) {
      data::PointElement e;
      e.geometry.position = Position(*m_idx, row);
      FillMeta(*m_idx, row, e.meta);
      out.push_back(std::move(e));
    }
    return out;
  }

 private:
  std::shared_ptr<const Index> m_idx;
};

class LineSeries final : public data::Series<data::LineElement> {
 public:
  explicit LineSeries(std::shared_ptr<const Index> idx) : m_idx(std::move(idx)) {}
  [[nodiscard]] std::size_t Size() const override { return m_idx->entries.size(); }
  [[nodiscard]] std::vector<data::LineElement> Load(std::size_t entry) const override {
    const auto rows = ReadRows(*m_idx, entry);
    std::vector<std::string> order;  // groups by first appearance
    std::unordered_map<std::string, std::vector<const std::vector<std::string>*>> groups;
    for (const auto& r : rows) {
      const auto& g = r[*m_idx->igroup];
      auto [it, inserted] = groups.try_emplace(g);
      if (inserted) order.push_back(g);
      it->second.push_back(&r);
    }
    std::vector<data::LineElement> out;
    for (const auto& g : order) {
      auto& members = groups[g];
      if (m_idx->iorder) {
        const auto io = *m_idx->iorder;
        std::stable_sort(members.begin(), members.end(), [io](const auto* a, const auto* b) {
          return AsDouble((*a)[io]).value_or(0) < AsDouble((*b)[io]).value_or(0);
        });
      }
      data::LineElement e;
      for (const auto* r : members) e.geometry.vertices.push_back(Position(*m_idx, *r));
      FillMeta(*m_idx, *members.front(), e.meta);
      out.push_back(std::move(e));
    }
    return out;
  }

 private:
  std::shared_ptr<const Index> m_idx;
};

struct TypeState {
  bool any = false, isInt = true, isDouble = true, isBool = true;
  void See(const std::string& v) {
    if (v.empty()) return;
    any = true;
    if (isInt && !AsInt(v)) isInt = false;
    if (isDouble && !AsDouble(v)) isDouble = false;
    if (isBool && !AsBool(v)) isBool = false;
  }
  [[nodiscard]] Type Result() const {
    if (!any) return Type::String;
    if (isInt) return Type::Int;
    if (isDouble) return Type::Double;
    if (isBool) return Type::Bool;
    return Type::String;
  }
};

}  // namespace

// ---------------------------------------------------------------------------
// Adapter
// ---------------------------------------------------------------------------
Capabilities CsvAdapter::GetCapabilities() const {
  Capabilities c;
  c.description = "delimiter-separated text with a header row: one point per row, or polylines from grouped rows";
  c.kinds = {data::ElementKind::Point, data::ElementKind::Line};
  c.extensions = {".csv", ".tsv", ".txt"};
  c.options = {
      {"delimiter", ",", "field separator: one character, 'tab' or 'space'"},
      {"comment", "#", "lines starting with this prefix are ignored"},
      {"x", "x", "column of the x coordinate"},
      {"y", "y", "column of the y coordinate"},
      {"z", "z", "column of the z coordinate"},
      {"kind", "point", "point (one per row) or line (rows grouped by 'group')"},
      {"group", "", "kind=line: column whose value names the line of each row"},
      {"order", "", "kind=line: numeric column ordering the vertices of a line (file order if empty)"},
      {"entry", "", "column whose value names the entry of each row (single entry if empty)"},
      {"columns", "", "comma-separated metadata columns (all other columns if empty)"},
      {"name", "<file stem>", "series name"},
      {"strict", "false", "true: any malformed row rejects the whole file"},
      {"cache", "4", "number of loaded entries kept in memory"},
  };
  c.notes = {
      "metadata types are inferred per column over the whole file: integer, else number, else true/false, else text",
      "empty cells give no metadata key; header 'name [unit]' adds metadata 'unit:name'",
      "coordinates in um, mm, cm or m are converted to mm (stated in the series description)",
      "kind=line: metadata of a line comes from its first row",
      "integer entry values are ordered ascending; other entry values by first appearance",
  };
  c.limitations = {
      "quoted fields may not span several lines",
      "the entry, order and coordinate columns are not copied into metadata",
  };
  return c;
}

LoadReport CsvAdapter::Open(data::SessionData& data, const std::string& path, const Options& options) {
  const Config cfg = ParseOptions(options, path);
  LoadReport report;
  std::error_code ec;
  const auto abs = std::filesystem::weakly_canonical(std::filesystem::absolute(path, ec), ec);
  const std::string fullPath = ec ? path : abs.string();
  report.source = "csv:" + fullPath;
  const std::string fileLabel = std::filesystem::path(path).filename().string();

  std::ifstream in(fullPath, std::ios::binary);
  if (!in) {
    report.skipped.push_back({fileLabel, "cannot open file"});
    return report;
  }

  // Header
  std::string line;
  std::size_t lineNo = 0;
  std::vector<std::string> header;
  while (std::getline(in, line)) {
    ++lineNo;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (lineNo == 1 && line.rfind("\xEF\xBB\xBF", 0) == 0) line.erase(0, 3);  // UTF-8 BOM
    if (IsSkippable(line, cfg.comment)) continue;
    header = SplitRecord(line, cfg.delimiter);
    break;
  }
  if (header.empty()) {
    report.skipped.push_back({fileLabel, "no header row"});
    return report;
  }

  auto idx = std::make_shared<Index>();
  idx->path = fullPath;
  idx->config = cfg;
  idx->fields = header.size();
  std::vector<std::pair<std::string, std::string>> cols;
  for (const auto& h : header) cols.push_back(SplitUnit(h));
  auto find = [&](const std::string& n) -> std::optional<std::size_t> {
    for (std::size_t i = 0; i < cols.size(); ++i)
      if (cols[i].first == n) return i;
    return std::nullopt;
  };

  std::vector<std::string> missing;
  const auto ix = find(cfg.x), iy = find(cfg.y), iz = find(cfg.z);
  for (const auto& [n, i] : {std::pair{cfg.x, ix}, std::pair{cfg.y, iy}, std::pair{cfg.z, iz}})
    if (!i) missing.push_back(n);
  std::optional<std::size_t> ientry;
  if (!cfg.entry.empty() && !(ientry = find(cfg.entry))) missing.push_back(cfg.entry);
  if (!cfg.group.empty() && !(idx->igroup = find(cfg.group))) missing.push_back(cfg.group);
  if (!cfg.order.empty() && !(idx->iorder = find(cfg.order))) missing.push_back(cfg.order);
  if (!missing.empty()) {
    std::string m;
    for (const auto& n : missing) m += (m.empty() ? "'" : ", '") + n + "'";
    report.skipped.push_back({fileLabel, "header has no column " + m});
    return report;
  }
  idx->ix = *ix;
  idx->iy = *iy;
  idx->iz = *iz;

  // Coordinate unit (one unit for all three; converted to mm)
  std::string coordUnit;
  for (auto i : {*ix, *iy, *iz}) {
    const auto& u = cols[i].second;
    if (u.empty()) continue;
    if (!coordUnit.empty() && u != coordUnit) {
      report.skipped.push_back({fileLabel, "coordinate columns state different units ('" + coordUnit + "', '" + u + "')"});
      return report;
    }
    coordUnit = u;
  }
  const auto scale = LengthToMm(coordUnit);
  if (!scale) {
    report.skipped.push_back({fileLabel, "coordinate unit '" + coordUnit + "' is not a length unit (um, mm, cm, m)"});
    return report;
  }
  idx->scale = *scale;

  // Metadata columns
  std::vector<std::size_t> metaIdx;
  if (cfg.columnsGiven) {
    for (const auto& n : cfg.columns) {
      if (auto i = find(n)) metaIdx.push_back(*i);
      else report.skipped.push_back({"column " + n, "not in the header"});
    }
  } else {
    for (std::size_t i = 0; i < cols.size(); ++i) {
      if (i == *ix || i == *iy || i == *iz || (ientry && i == *ientry) || (idx->iorder && i == *idx->iorder)) continue;
      if (cols[i].first.empty()) continue;
      metaIdx.push_back(i);
    }
  }

  // Scan rows
  std::vector<TypeState> types(cols.size());
  TypeState entryType;
  std::vector<std::string> entryKeys;                      // first-appearance order
  std::unordered_map<std::string, std::size_t> entryOf;    // key -> position in entryKeys
  std::vector<std::vector<std::streamoff>> rowsByKey;
  std::size_t good = 0;
  while (true) {
    const std::streamoff off = in.tellg();
    if (!std::getline(in, line)) break;
    ++lineNo;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (IsSkippable(line, cfg.comment)) continue;
    const auto f = SplitRecord(line, cfg.delimiter);
    std::string problem;
    if (f.size() != header.size()) {
      problem = std::to_string(f.size()) + " fields, header has " + std::to_string(header.size());
    } else {
      for (auto i : {*ix, *iy, *iz})
        if (!AsDouble(f[i])) {
          problem = "column '" + cols[i].first + "' is not a number: '" + f[i] + "'";
          break;
        }
      if (problem.empty() && idx->iorder && !AsDouble(f[*idx->iorder]))
        problem = "column '" + cols[*idx->iorder].first + "' is not a number: '" + f[*idx->iorder] + "'";
    }
    if (!problem.empty()) {
      report.skipped.push_back({"line " + std::to_string(lineNo), problem});
      continue;
    }
    ++good;
    for (auto i : metaIdx) types[i].See(f[i]);
    const std::string key = ientry ? f[*ientry] : std::string();
    if (ientry) entryType.See(key);
    auto [it, inserted] = entryOf.try_emplace(key, entryKeys.size());
    if (inserted) {
      entryKeys.push_back(key);
      rowsByKey.emplace_back();
    }
    rowsByKey[it->second].push_back(off);
  }

  const auto badRows = static_cast<std::size_t>(std::count_if(
      report.skipped.begin(), report.skipped.end(), [](const Skipped& k) { return k.name.rfind("line ", 0) == 0; }));
  if (cfg.strict && badRows > 0) {
    const auto rows = badRows;
    report.skipped.insert(report.skipped.begin(),
                          {fileLabel, "strict: " + std::to_string(rows) + " malformed row(s); nothing registered"});
    return report;
  }
  if (good == 0) {
    report.skipped.push_back({fileLabel, "no data rows"});
    return report;
  }

  // Entry order: integer keys ascending, others by first appearance
  std::vector<std::size_t> perm(entryKeys.size());
  for (std::size_t i = 0; i < perm.size(); ++i) perm[i] = i;
  if (ientry && entryType.Result() == Type::Int)
    std::stable_sort(perm.begin(), perm.end(),
                     [&](std::size_t a, std::size_t b) { return *AsInt(entryKeys[a]) < *AsInt(entryKeys[b]); });
  for (auto p : perm) idx->entries.push_back(std::move(rowsByKey[p]));

  RegisteredSeries rs;
  for (auto i : metaIdx) {
    idx->meta.push_back({cols[i].first, cols[i].second, i, types[i].Result()});
    if (!types[i].any) continue;  // never set
    rs.metadataKeys.insert(cols[i].first);
    if (!cols[i].second.empty()) rs.metadataKeys.insert("unit:" + cols[i].first);
  }

  rs.ref = {cfg.name, report.source};
  rs.entries = idx->entries.size();
  rs.kind = cfg.kind == Kind::Point ? data::ElementKind::Point : data::ElementKind::Line;
  rs.description = std::string(cfg.kind == Kind::Point ? "points (one per row)" : "polylines (rows grouped by '" + cfg.group + "')") +
                   " from " + fileLabel + ", coordinates (" + cfg.x + ", " + cfg.y + ", " + cfg.z + ")";
  if (!coordUnit.empty() && coordUnit != "mm") rs.description += " converted from " + coordUnit + " to mm";
  if (ientry) rs.description += ", entries by '" + cfg.entry + "'";

  bool added = false;
  if (cfg.kind == Kind::Point) {
    rs.existing = data.Status<data::PointElement>(rs.ref) == data::Lookup::Found;
    if (!rs.existing)
      added = data.Add<data::PointElement>(cfg.name, report.source, std::make_unique<PointSeries>(idx),
                                           rs.description, cfg.cache);
  } else {
    rs.existing = data.Status<data::LineElement>(rs.ref) == data::Lookup::Found;
    if (!rs.existing)
      added = data.Add<data::LineElement>(cfg.name, report.source, std::make_unique<LineSeries>(idx),
                                          rs.description, cfg.cache);
  }
  if (rs.existing) {
    rs.entries = cfg.kind == Kind::Point ? data.Size<data::PointElement>(rs.ref) : data.Size<data::LineElement>(rs.ref);
    added = true;
  }
  if (added) report.registered.push_back(std::move(rs));
  else report.skipped.push_back({cfg.name, "could not register the series"});
  report.Sort();
  return report;
}

static Registrar g_csvRegistrar("csv", [] { return std::make_unique<CsvAdapter>(); });

}  // namespace fccvis::adapters
