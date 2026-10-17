// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "Adapter.h"
#include "AdapterRegistry.h"

#include <algorithm>
#include <cctype>
#include <tuple>

namespace fccvis::adapters {

std::string Capabilities::Help(const std::string& name) const {
  std::string s = name + " adapter\n";
  if (!description.empty()) s += "  " + description + "\n";
  s += "  element kinds:";
  for (auto k : kinds) s += std::string(" ") + data::ToString(k);
  s += "\n  file types:";
  for (const auto& e : extensions) s += " " + e;
  s += "\n";
  if (!options.empty()) {
    auto sorted = options;
    std::sort(sorted.begin(), sorted.end(), [](const OptionInfo& a, const OptionInfo& b) { return a.name < b.name; });
    s += "  options:\n";
    for (const auto& o : sorted) {
      s += "    " + o.name;
      if (!o.defaultValue.empty()) s += " (default " + o.defaultValue + ")";
      s += ": " + o.description + "\n";
    }
  }
  if (!notes.empty()) {
    s += "  notes:\n";
    for (const auto& n : notes) s += "    " + n + "\n";
  }
  if (!limitations.empty()) {
    s += "  limitations:\n";
    for (const auto& l : limitations) s += "    " + l + "\n";
  }
  return s;
}

namespace {
// Natural order for names ending in a number ("line 2" before "line 10").
auto NaturalKey(const std::string& n) {
  std::size_t i = n.size();
  while (i > 0 && std::isdigit(static_cast<unsigned char>(n[i - 1]))) --i;
  return std::make_tuple(n.substr(0, i), n.size() - i, n.substr(i));
}
}  // namespace

void LoadReport::Sort() {
  std::sort(registered.begin(), registered.end(), [](const RegisteredSeries& a, const RegisteredSeries& b) {
    return std::tie(a.ref.name, a.ref.source) < std::tie(b.ref.name, b.ref.source);
  });
  std::stable_sort(skipped.begin(), skipped.end(), [](const Skipped& a, const Skipped& b) {
    return std::make_tuple(NaturalKey(a.name), a.reason) < std::make_tuple(NaturalKey(b.name), b.reason);
  });
}

std::string LoadReport::ToString(std::size_t maxSkipped) const {
  LoadReport r = *this;
  r.Sort();
  std::string s = "source " + source + ": " + std::to_string(r.registered.size()) + " series, " +
                  std::to_string(r.skipped.size()) + " skipped\n";
  for (const auto& x : r.registered) {
    s += "  + " + x.ref.name + " (" + data::ToString(x.kind) + ", " + std::to_string(x.entries) +
         (x.entries == 1 ? " entry" : " entries");
    if (x.existing) s += ", already registered";
    s += ")";
    if (!x.metadataKeys.empty()) {
      s += " [";
      bool first = true;
      for (const auto& k : x.metadataKeys) {
        s += (first ? "" : ", ") + k;
        first = false;
      }
      s += "]";
    }
    s += "\n";
  }
  for (std::size_t i = 0; i < r.skipped.size() && i < maxSkipped; ++i)
    s += "  - " + r.skipped[i].name + ": " + r.skipped[i].reason + "\n";
  if (r.skipped.size() > maxSkipped)
    s += "  ... and " + std::to_string(r.skipped.size() - maxSkipped) + " more\n";
  return s;
}

std::unique_ptr<Adapter> Registry::ForPath(const std::string& path) const {
  const auto dot = path.find_last_of('.');
  if (dot == std::string::npos) return nullptr;
  std::string ext = path.substr(dot);
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
  for (const auto& [name, f] : m_factories) {
    auto a = f();
    if (a && a->GetCapabilities().extensions.count(ext)) return a;
  }
  return nullptr;
}

}  // namespace fccvis::adapters
