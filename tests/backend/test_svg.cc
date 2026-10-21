// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
/**
 * @file test_svg.cc
 * @brief SVG backend (B2): number formatting, files, golden SVG text, options.
 */
#include <catch2/catch_test_macros.hpp>

#include "CameraChanges.h"
#include "GoldenText.h"
#include "SceneChanges.h"
#include "Session.h"
#include "Svg.h"
#include "TestSupport.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace ch = fccvis::changes;
namespace dt = fccvis::data;
namespace st = fccvis::style;
namespace sv = fccvis::backend::svg;
namespace fs = std::filesystem;
using fccvis::session::Session;
using namespace fccvis::test;

namespace {

std::string Read(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::size_t Count(const std::string& s, const std::string& what) {
  std::size_t n = 0;
  for (auto p = s.find(what); p != std::string::npos; p = s.find(what, p + 1)) ++n;
  return n;
}

void Populate(Session& s) {
  REQUIRE(s.Data().Add<dt::PointElement>("pts", "fixture",
      std::make_unique<dt::InMemorySeries<dt::PointElement>>(std::vector<std::vector<dt::PointElement>>{MakePoints(3)})));
  REQUIRE(s.Data().Add<dt::LineElement>("trk", "fixture",
      std::make_unique<dt::InMemorySeries<dt::LineElement>>(std::vector<std::vector<dt::LineElement>>{MakeLines(2)})));
  REQUIRE(s.Data().Add<dt::SurfaceElement>("geo", "fixture",
      std::make_unique<dt::InMemorySeries<dt::SurfaceElement>>(std::vector<std::vector<dt::SurfaceElement>>{MakeSurfaces(1)})));
  s.DataChanged();
  for (const char* n : {"front", "side"}) {
    REQUIRE(s.CreateScene(n));
    REQUIRE(s.Resolve(ch::SetCamera{n, std::make_shared<fccvis::scene::camera::Camera>(n)}).ok);
    REQUIRE(s.Resolve(ch::AddLayer<dt::SurfaceElement>{n, "detector", {"geo", ""}}).ok);
    REQUIRE(s.Resolve(ch::AddLayer<dt::LineElement>{n, "tracks", {"trk", ""}}).ok);
    REQUIRE(s.Resolve(ch::AddLayer<dt::PointElement>{n, "hits", {"pts", ""}}).ok);
  }
  REQUIRE(s.Resolve(ch::ChangeCameraDirection{"side", {1.0, 0.0, 0.0}}).ok);
  st::PointStyle red;
  red.common.color = st::Color::FromHex(0xFF0000);
  red.size = 6;
  red.marker = st::MarkerShape::Square;
  REQUIRE(s.Resolve(ch::AddStyleRule<dt::PointElement>{"front", "hits", {}, red}).ok);
  st::LineStyle dashed;
  dashed.dash = st::DashPattern::Dashed;
  dashed.width = 2;
  REQUIRE(s.Resolve(ch::AddStyleRule<dt::LineElement>{"front", "tracks", {}, dashed}).ok);
  st::EnvironmentStyle env;
  env.axes = true;
  REQUIRE(s.Resolve(ch::SetEnvironment{"side", env}).ok);
}

}  // namespace

TEST_CASE("SVG numbers and escaping are normalised", "[backend][svg]") {
  CHECK(sv::Num(1.0, 2) == "1");
  CHECK(sv::Num(1.234567, 2) == "1.23");
  CHECK(sv::Num(-0.001, 2) == "0");
  CHECK(sv::Num(2.50, 3) == "2.5");
  CHECK(sv::Num(12, 0) == "12");
  CHECK(sv::Escape("a<b & \"c\"") == "a&lt;b &amp; &quot;c&quot;");
}

TEST_CASE("SVG backend writes scenes and a titled layout matching the golden", "[backend][svg]") {
  const auto dir = fs::path(FCCVIS_TEST_BINARY_DIR) / "svg-out";
  fs::remove_all(dir);
  Session s("svg");
  auto log = CaptureLog(s);
  Populate(s);
  fccvis::scene::layout::Split split;
  split.children.push_back({P("front"), std::string("Front <xy>"), 2});
  split.children.push_back({P("side"), std::string("Side"), 1});
  REQUIRE(s.SetLayout(fccvis::scene::layout::LayoutNode{split}).ok);
  REQUIRE(s.InitBackend("svg", "v", {{"output_dir", dir.string()}, {"width", "300"}, {"height", "120"}}).ok);
  REQUIRE(s.Open("v", "front").ok);
  REQUIRE(s.Open("v", "side").ok);
  REQUIRE(s.Flush("v").ok);

  const auto front = Read(dir / "front.svg");
  const auto layout = Read(dir / "layout.svg");
  REQUIRE(fs::exists(dir / "side.svg"));
  CHECK(front.rfind("<?xml", 0) == 0);
  CHECK(Count(layout, "<g ") == Count(layout, "</g>"));
  CHECK(Count(layout, "<g ") == 2);
  CHECK(layout.find("Front &lt;xy&gt;") != std::string::npos);  // label, escaped
  CHECK(front.find("fill=\"#ff0000\"") != std::string::npos);    // style rule
  CHECK(front.find("stroke-dasharray") != std::string::npos);    // dashed tracks
  CHECK(Count(front, "data-layer=\"hits\"") == 3);
  CHECK(layout.find("#da1e28") != std::string::npos);            // axes in side scene

  const auto golden = CompareGoldenText(layout, "svg_layout.svg");
  INFO(golden.message);
  CHECK(golden.ok);
}

TEST_CASE("SVG backend options", "[backend][svg]") {
  Session s("opts");
  auto log = CaptureLog(s);
  CHECK_FALSE(s.InitBackend("svg", "a", {{"precision", "9"}}).ok);
  CHECK_FALSE(s.InitBackend("svg", "b", {{"precision", "x"}}).ok);
  CHECK_FALSE(s.InitBackend("svg", "c", {{"tolerance", "-1"}}).ok);

  Populate(s);
  const auto dir = fs::path(FCCVIS_TEST_BINARY_DIR) / "svg-opts";
  fs::remove_all(dir);
  REQUIRE(s.InitBackend("svg", "d", {{"output_dir", dir.string()}, {"precision", "0"}, {"async", "false"}}).ok);
  REQUIRE(s.Open("d", "front").ok);
  const auto text = Read(dir / "front.svg");
  const auto body = text.substr(text.find("<rect"));  // after the XML declaration and <svg> tag
  CHECK(body.find("points=") != std::string::npos);
  CHECK_FALSE(std::regex_search(body, std::regex("points=\"[^\"]*\\.")));  // integer coordinates
}
