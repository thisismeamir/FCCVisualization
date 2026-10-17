// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Documentation example: the quickstart in C++ (docs/source/getting-started/index.md).
#include "AdapterRegistry.h"
#include "fccvis/api/Builders.h"
#include "fccvis/stdlib/Styles.h"

#include <cstdio>
#include <filesystem>

using namespace fccvis;

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";  // directory of the example data

  // [cpp]
  session::Session session("quickstart");

  // 1. Load data with the CSV adapter.
  auto csv = adapters::Registry::Global().Create("csv");
  auto hits = csv->Open(session.Data(), dir + "/hits.csv", {{"entry", "event"}});
  auto tracks = csv->Open(session.Data(), dir + "/tracks.csv",
                          {{"kind", "line"}, {"group", "track"}, {"order", "step"}, {"entry", "event"}});
  std::printf("%s%s", hits.ToString().c_str(), tracks.ToString().c_str());
  session.DataChanged();

  // 2. Describe a scene.
  api::Script scene = api::Scene("event").Create()
                          .Layer<data::Point>("hits", "hits").Sheet(stdlib::HitsByEnergy({0, 3}))
                          .Layer<data::Line>("tracks", "tracks").Sheet(stdlib::TracksByCharge());
  scene.Run(session);

  // 3. Draw it with the raster backend.
  session.InitBackend("raster", "png", {{"output_dir", "quickstart-cpp"}, {"width", "480"}, {"height", "480"}});
  session.Open("png", "event");
  session.Flush("png");  // quickstart-cpp/event.png
  // [cpp]

  return std::filesystem::exists("quickstart-cpp/event.png") ? 0 : 1;
}
