// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Documentation example: histograms of the selected elements
// (docs/source/backends/histogram.md).
#include "SceneChanges.h"
#include "Session.h"

#include <cstdio>
#include <filesystem>

using namespace fccvis;

int main() {
  session::Session s("example");
  std::vector<data::PointElement> hits(50);
  for (std::size_t i = 0; i < hits.size(); ++i) {
    hits[i].geometry.position = {static_cast<double>(i), 0, 0};
    hits[i].meta.Set("energy", 0.1 * static_cast<double>(i));
    hits[i].meta.Set("detector", std::string(i % 3 ? "ECal" : "HCal"));
  }
  s.Data().Add<data::PointElement>("hits", "example",
      std::make_unique<data::InMemorySeries<data::PointElement>>(std::vector<std::vector<data::PointElement>>{hits}));
  s.DataChanged();
  s.CreateScene("event");
  s.Resolve(changes::AddLayer<data::PointElement>{"event", "hits", {"hits", ""}});

  // [histogram]
  // The layer's filter selects what is histogrammed, exactly as it selects what is drawn.
  auto ecal = std::make_shared<filter::Filter<data::PointElement>>(
      [](const data::PointElement& e) { return e.meta.Text("detector") == "ECal"; });
  s.Resolve(changes::SetLayerFilter<data::PointElement>{"event", "hits", ecal});

  // energy in 10 bins over [0, 5); detector as categories.
  s.InitBackend("histogram", "h", {{"output_dir", "hist-out"},
                                   {"histograms", "hits:energy:10:0:5; hits:detector"}});
  s.Open("h", "event");
  s.Flush("h");  // hist-out/event.hist.csv and hist-out/event.hist.svg
  // [histogram]

  const bool ok = std::filesystem::exists("hist-out/event.hist.csv");
  std::printf("event.hist.csv written: %s\n", ok ? "yes" : "no");
  return ok ? 0 : 1;
}
