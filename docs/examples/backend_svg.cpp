// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Documentation example: export a layout as SVG (docs/source/backends/svg.md).
#include "CameraChanges.h"
#include "SceneChanges.h"
#include "Session.h"

#include <cstdio>
#include <filesystem>

using namespace fccvis;

int main() {
  session::Session s("example");
  std::vector<data::LineElement> tracks(2);
  tracks[0].geometry.vertices = {{0, 0, 0}, {100, 50, 20}, {200, 80, 40}};
  tracks[1].geometry.vertices = {{0, 0, 0}, {-120, 60, -30}};
  s.Data().Add<data::LineElement>("tracks", "example",
      std::make_unique<data::InMemorySeries<data::LineElement>>(std::vector<std::vector<data::LineElement>>{tracks}));
  s.DataChanged();

  // [svg]
  for (const char* view : {"xy", "zy"}) {
    s.CreateScene(view);
    s.Resolve(changes::SetCamera{view, std::make_shared<scene::camera::Camera>(view)});
    s.Resolve(changes::AddLayer<data::LineElement>{view, "tracks", {"tracks", ""}});
  }
  s.Resolve(changes::ChangeCameraDirection{"zy", {1.0, 0.0, 0.0}});

  // Two panes side by side; the labels become the pane titles in the SVG.
  scene::layout::Split split;
  split.children.push_back({scene::layout::LayoutNode{scene::layout::Pane{"xy"}}, std::string("Transverse"), 1});
  split.children.push_back({scene::layout::LayoutNode{scene::layout::Pane{"zy"}}, std::string("Side"), 1});
  s.SetLayout(scene::layout::LayoutNode{split});

  s.InitBackend("svg", "paper", {{"output_dir", "svg-out"}, {"width", "800"}, {"height", "400"}});
  s.Open("paper", "xy");
  s.Open("paper", "zy");
  s.Flush("paper");  // svg-out/xy.svg, svg-out/zy.svg, svg-out/layout.svg
  // [svg]

  const bool ok = std::filesystem::exists("svg-out/layout.svg");
  std::printf("layout.svg written: %s\n", ok ? "yes" : "no");
  return ok ? 0 : 1;
}
