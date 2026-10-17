// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Documentation example: one session, several backends
// (docs/source/guides/multi-backend.md).
#include "AdapterRegistry.h"
#include "SceneChanges.h"
#include "fccvis/api/Builders.h"
#include "fccvis/stdlib/Styles.h"

#include <cstdio>
#include <filesystem>
#include <string>

using namespace fccvis;

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  session::Session session("multi");
  auto csv = adapters::Registry::Global().Create("csv");
  csv->Open(session.Data(), dir + "/hits.csv", {{"entry", "event"}});
  csv->Open(session.Data(), dir + "/tracks.csv", {{"kind", "line"}, {"group", "track"}, {"order", "step"}, {"entry", "event"}});
  session.DataChanged();

  api::Script setup = api::Scene("front").Create()
                          .Layer<data::Point>("hits", "hits").Sheet(stdlib::HitsByEnergy({0, 3}))
                          .Layer<data::Line>("tracks", "tracks").Sheet(stdlib::TracksByCharge());
  setup.Append(api::Scene("side").Create().Direction({1, 0, 0}).Layer<data::Point>("hits", "hits"));
  setup.Append(api::Layout(api::Split({{api::Pane("front"), "Front"}, {api::Tabs({api::Pane("side")}), "Side"}})));
  if (!setup.Run(session).ok) return 1;

  // [backends]
  // Every backend draws the same scenes; each declares what it supports.
  session.InitBackend("raster", "png", {{"output_dir", "multi/png"}, {"width", "480"}, {"height", "240"}});
  session.InitBackend("svg", "vector", {{"output_dir", "multi/svg"}, {"width", "480"}, {"height", "240"}});
  session.InitBackend("histogram", "stats", {{"output_dir", "multi/hist"}, {"histograms", "hits:energy:10:0:5"}});
  for (const char* id : {"png", "vector", "stats"}) {
    session.Open(id, "front");
    session.Open(id, "side");
    const auto caps = session.BackendCapabilities(id);
    std::printf("%-6s tabs: %s, style properties: %zu\n", id, caps->tabs ? "yes" : "no (first tab)",
                caps->styleProperties.size());
  }
  session.Flush();  // waits for every backend

  // One change, every backend follows: only what changed is redrawn.
  session.Resolve(changes::SetEntry<data::PointElement>{"front", "hits", 1});
  session.Flush();
  for (const char* id : {"png", "vector"})
    std::printf("%-6s redrew hits %zu times, tracks %zu times\n", id, session.BackendStats(id)->Layer("front", "hits"),
                session.BackendStats(id)->Layer("front", "tracks"));
  // [backends]

  // [detach]
  session.Close("png", "side");    // the raster layout now shows only "front"
  session.DetachBackend("stats");  // the others keep running
  session.Flush();
  // [detach]

  namespace fs = std::filesystem;
  bool counts = true;  // the numbers quoted in the guide
  for (const char* id : {"png", "vector"})
    counts = counts && session.BackendStats(id)->Layer("front", "hits") == 2 &&
             session.BackendStats(id)->Layer("front", "tracks") == 1;
  const bool files = fs::exists("multi/png/layout.png") && fs::exists("multi/svg/layout.svg") &&
                     fs::exists("multi/hist/front.hist.csv");
  return files && counts ? 0 : 1;
}
