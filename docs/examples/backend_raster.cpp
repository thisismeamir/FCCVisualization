// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Documentation example: draw a scene with the raster backend
// (docs/source/backends/raster.md).
#include "CameraChanges.h"
#include "SceneChanges.h"
#include "Session.h"

#include <cstdio>
#include <filesystem>

using namespace fccvis;

int main() {
  // [raster]
  session::Session s("example");

  // Elements computed by the program: one entry with three hits.
  std::vector<data::PointElement> hits(3);
  for (std::size_t i = 0; i < hits.size(); ++i) hits[i].geometry.position = {10.0 * i, 5.0 * i, 0.0};
  s.Data().Add<data::PointElement>("hits", "example",
      std::make_unique<data::InMemorySeries<data::PointElement>>(
          std::vector<std::vector<data::PointElement>>{hits}));
  s.DataChanged();

  s.CreateScene("front");
  s.Resolve(changes::SetCamera{"front", std::make_shared<scene::camera::Camera>("front")});
  s.Resolve(changes::AddLayer<data::PointElement>{"front", "hits", {"hits", ""}});

  // Writes raster-out/front.png (and layout.png when a layout is set).
  s.InitBackend("raster", "png", {{"output_dir", "raster-out"}, {"width", "400"}, {"height", "300"}});
  s.Open("png", "front");
  s.Flush("png");  // wait for the render thread
  // [raster]

  const bool ok = std::filesystem::exists("raster-out/front.png");
  std::printf("front.png written: %s\n", ok ? "yes" : "no");
  return ok ? 0 : 1;
}
