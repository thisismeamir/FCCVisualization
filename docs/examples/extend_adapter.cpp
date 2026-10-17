// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Documentation example: writing an adapter (docs/source/development/index.md).
// Reads "x y z energy" lines; a blank line ends an event.
#include "AdapterRegistry.h"
#include "fccvis/api/Interpreter.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace fccvis;

// [adapter]
namespace {

/** The series: events are read when first asked for (the session caches them). */
class XyzSeries final : public data::Series<data::PointElement> {
 public:
  XyzSeries(std::string path, std::vector<std::streamoff> starts) : m_path(std::move(path)), m_starts(std::move(starts)) {}
  [[nodiscard]] std::size_t Size() const override { return m_starts.size(); }
  [[nodiscard]] std::vector<data::PointElement> Load(std::size_t entry) const override {
    std::ifstream in(m_path);
    in.seekg(m_starts.at(entry));
    std::vector<data::PointElement> out;
    for (std::string line; std::getline(in, line) && !line.empty();) {
      std::istringstream fields(line);
      data::PointElement e;
      double energy = 0;
      fields >> e.geometry.position[0] >> e.geometry.position[1] >> e.geometry.position[2] >> energy;
      e.meta.Set("energy", energy);
      e.meta.Set("unit:energy", std::string("GeV"));
      out.push_back(std::move(e));
    }
    return out;
  }

 private:
  std::string m_path;
  std::vector<std::streamoff> m_starts;  // file offset of the first line of each event
};

class XyzAdapter final : public adapters::Adapter {
 public:
  [[nodiscard]] std::string Name() const override { return "xyz"; }

  [[nodiscard]] adapters::Capabilities GetCapabilities() const override {
    adapters::Capabilities c;
    c.description = "whitespace-separated x y z energy, one event per block";
    c.kinds = {data::ElementKind::Point};
    c.extensions = {".xyz"};
    c.options = {{"name", "hits", "series name"}};
    return c;
  }

  adapters::LoadReport Open(data::SessionData& data, const std::string& path,
                            const adapters::Options& options) override {
    adapters::LoadReport report;
    report.source = "xyz:" + std::filesystem::absolute(path).string();
    std::ifstream in(path);
    if (!in) {
      report.skipped.push_back({path, "cannot open file"});  // problems go into the report
      return report;
    }
    std::vector<std::streamoff> starts;  // scan once: where does each event start?
    bool inEvent = false;
    for (std::string line;;) {
      const std::streamoff at = in.tellg();
      if (!std::getline(in, line)) break;
      if (line.rfind('#', 0) == 0) continue;
      if (!line.empty() && !inEvent) starts.push_back(at);
      inEvent = !line.empty();
    }
    const auto name = options.count("name") ? options.at("name") : std::string("hits");
    adapters::RegisteredSeries rs;
    rs.ref = {name, report.source};
    rs.entries = starts.size();
    rs.metadataKeys = {"energy", "unit:energy"};
    rs.description = "points from " + path;
    rs.existing = data.Status<data::PointElement>(rs.ref) == data::Lookup::Found;
    if (rs.existing || data.Add<data::PointElement>(name, report.source,
                                                    std::make_unique<XyzSeries>(path, starts), rs.description))
      report.registered.push_back(rs);
    return report;
  }
};

// Registration at static initialisation: `load xyz <file>` works from here on.
const adapters::Registrar g_xyz("xyz", [] { return std::make_unique<XyzAdapter>(); });

}  // namespace
// [adapter]

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  // [use]
  session::Session session("extend");
  api::Interpreter shell(session);
  const auto reply = shell.Run("load xyz " + dir + "/recipes/points.xyz\n"
                               "scene new s\n"
                               "layer add s hits entry 1\n"
                               "where s/hits energy > 1\n");
  std::printf("%s", reply.Text().c_str());
  // [use]
  const auto entry = session.Data().Entry<data::PointElement>({"hits", ""}, 1);
  return reply.ok && entry && entry->size() == 2 && (*entry)[0].meta.Number("energy") == 3.0 ? 0 : 1;
}
