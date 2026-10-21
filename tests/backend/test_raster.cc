// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
/**
 * @file test_raster.cc
 * @brief Raster backend (B0): PNG codec, rendering, layout, async coalescing,
 *        and visual regression against goldens (tests/data/golden).
 */
#include <catch2/catch_test_macros.hpp>

#include "CameraChanges.h"
#include "Drawing.h"
#include "GoldenImage.h"
#include "Png.h"
#include "SceneChanges.h"
#include "Session.h"
#include "TestSupport.h"

#include <filesystem>
#include <string>

namespace bk = fccvis::backend;
namespace rs = fccvis::backend::raster;
namespace ch = fccvis::changes;
namespace dt = fccvis::data;
namespace st = fccvis::style;
namespace fs = std::filesystem;
using fccvis::session::Session;
using namespace fccvis::test;

namespace {

fs::path OutDir(const std::string& tag) {
  auto dir = fs::path(FCCVIS_TEST_BINARY_DIR) / "raster-out" / tag;
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

/** Session with points (red), tracks (blue, dashed) and two filled squares. */
void Populate(Session& s) {
  REQUIRE(s.Data().Add<dt::PointElement>("pts", "fixture",
      std::make_unique<VectorSeries<dt::PointElement>>(
          std::vector<std::vector<dt::PointElement>>{MakePoints(5)})));
  REQUIRE(s.Data().Add<dt::LineElement>("trk", "fixture",
      std::make_unique<VectorSeries<dt::LineElement>>(
          std::vector<std::vector<dt::LineElement>>{MakeLines(3)})));
  REQUIRE(s.Data().Add<dt::SurfaceElement>("geo", "fixture",
      std::make_unique<VectorSeries<dt::SurfaceElement>>(
          std::vector<std::vector<dt::SurfaceElement>>{MakeSurfaces(2)})));
  s.DataChanged();
  for (const char* n : {"a", "b"}) {
    REQUIRE(s.CreateScene(n));
    REQUIRE(s.Resolve(ch::SetCamera{n, std::make_shared<fccvis::scene::camera::Camera>(n)}).ok);
  }
  REQUIRE(s.Resolve(ch::AddLayer<dt::SurfaceElement>{"a", "detector", {"geo", ""}}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<dt::LineElement>{"a", "tracks", {"trk", ""}}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<dt::PointElement>{"a", "hits", {"pts", ""}}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<dt::PointElement>{"b", "hits", {"pts", ""}}).ok);

  st::PointStyle red;
  red.common.color = st::Color::FromHex(0xFF0000);
  red.size = 9;
  REQUIRE(s.Resolve(ch::AddStyleRule<dt::PointElement>{"a", "hits", {}, red}).ok);
  st::LineStyle blue;
  blue.common.color = st::Color::FromHex(0x0000FF);
  blue.width = 3;
  REQUIRE(s.Resolve(ch::AddStyleRule<dt::LineElement>{"a", "tracks", {}, blue}).ok);
  st::SurfaceStyle green;
  green.common.color = st::Color::FromHex(0x00C000);
  green.common.opacity = 1.0f;
  REQUIRE(s.Resolve(ch::AddStyleRule<dt::SurfaceElement>{"a", "detector", {}, green}).ok);
}

int CountNear(const rs::Image& img, std::uint32_t rgb, int tol = 40) {
  int n = 0;
  for (int y = 0; y < img.height; ++y)
    for (int x = 0; x < img.width; ++x) {
      const auto p = img.At(x, y);
      const int dr = std::abs(int((p >> 24) & 0xFF) - int((rgb >> 16) & 0xFF));
      const int dg = std::abs(int((p >> 16) & 0xFF) - int((rgb >> 8) & 0xFF));
      const int db = std::abs(int((p >> 8) & 0xFF) - int(rgb & 0xFF));
      if (dr <= tol && dg <= tol && db <= tol) ++n;
    }
  return n;
}

}  // namespace

TEST_CASE("PNG writer and reader round-trip", "[backend][raster]") {
  rs::Image img{7, 5, std::vector<std::uint8_t>(7 * 5 * 4)};
  for (std::size_t i = 0; i < img.rgba.size(); ++i) img.rgba[i] = static_cast<std::uint8_t>(i * 37);
  const auto path = OutDir("png") / "rt.png";
  REQUIRE(rs::WritePng(path.string(), img));
  std::string error;
  const auto back = rs::ReadPng(path.string(), &error);
  REQUIRE(back);
  CHECK(back->width == 7);
  CHECK(back->height == 5);
  CHECK(back->rgba == img.rgba);
  CHECK(rs::DifferentPixelFraction(img, *back, 0) == 0.0);

  auto changed = img;
  changed.rgba[0] ^= 0xFF;
  CHECK(rs::DifferentPixelFraction(img, changed, 8) == 1.0 / 35.0);
  CHECK(rs::DifferentPixelFraction(img, rs::Image{1, 1, {0, 0, 0, 0}}, 0) == 1.0);

  // PNG signature
  const auto bytes = rs::EncodePng(img);
  REQUIRE(bytes.size() > 8);
  CHECK(bytes[1] == 'P');
  CHECK(bytes[2] == 'N');
  CHECK(bytes[3] == 'G');
  CHECK_FALSE(rs::ReadPng((OutDir("png") / "missing.png").string(), &error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("Raster backend writes one PNG per open scene and the layout", "[backend][raster]") {
  const auto dir = OutDir("files");
  Session s("raster");
  auto log = CaptureLog(s);
  Populate(s);
  REQUIRE(s.InitBackend("raster", "r",
                        {{"output_dir", dir.string()}, {"width", "200"}, {"height", "150"}}).ok);
  REQUIRE(s.SetLayout(S({P("a"), P("b")})).ok);
  REQUIRE(s.Open("r", "a").ok);
  REQUIRE(s.Open("r", "b").ok);
  REQUIRE(s.Flush("r").ok);

  for (const char* f : {"a.png", "b.png", "layout.png"}) {
    INFO(f);
    const auto img = rs::ReadPng((dir / f).string());
    REQUIRE(img);
    CHECK(img->width == 200);
    CHECK(img->height == 150);
  }
  const auto a = *rs::ReadPng((dir / "a.png").string());
  CHECK(a.At(0, 0) == 0xFFFFFFFFu);                 // theme background
  CHECK(CountNear(a, 0xFF0000) > 5 * 20);           // five red markers
  CHECK(CountNear(a, 0x0000FF, 60) > 20);           // blue tracks
  CHECK(CountNear(a, 0x00C000) > 200);              // filled squares
  const auto b = *rs::ReadPng((dir / "b.png").string());
  CHECK(CountNear(b, 0xFF0000) == 0);               // style rules are per scene
  CHECK(CountNear(b, 0x00C000) == 0);

  const auto golden = CompareGolden(*rs::ReadPng((dir / "layout.png").string()), "raster_split");
  INFO(golden.message);
  CHECK(golden.ok);
  const auto goldenA = CompareGolden(a, "raster_scene_a");
  INFO(goldenA.message);
  CHECK(goldenA.ok);
  CHECK(s.DetachBackend("r").ok);
}

TEST_CASE("Raster backend degrades tabs to their first tab", "[backend][raster]") {
  namespace dr = fccvis::backend::draw;
  std::vector<std::string> hidden;
  const auto panes = dr::Arrange(T({P("a"), P("b")}), {"a", "b"}, {0, 0, 100, 50}, &hidden);
  REQUIRE(panes.size() == 1);
  CHECK(panes[0].scene == "a");
  CHECK(panes[0].rect.w == 100);
  CHECK(hidden == std::vector<std::string>{"b"});

  const auto split = dr::Arrange(S({P("a"), T({P("b"), P("c")})}), {"a", "b", "c"}, {0, 0, 100, 50});
  REQUIRE(split.size() == 2);
  CHECK(split[0].rect.w + split[1].rect.w == 100);

  const auto grid = dr::Arrange(std::nullopt, {"x", "y", "z"}, {0, 0, 100, 100});
  CHECK(grid.size() == 3);

  Session s("tabs");
  auto log = CaptureLog(s);
  Populate(s);
  const auto dir = OutDir("tabs");
  REQUIRE(s.InitBackend("raster", "r", {{"output_dir", dir.string()}, {"width", "80"}, {"height", "60"}}).ok);
  REQUIRE(s.SetLayout(T({P("a"), P("b")})).ok);
  REQUIRE(s.Open("r", "a").ok);
  REQUIRE(s.Open("r", "b").ok);
  REQUIRE(s.Flush("r").ok);
  const auto layout = rs::ReadPng((dir / "layout.png").string());
  REQUIRE(layout);
  CHECK(CountNear(*layout, 0x00C000) > 0);  // scene a (first tab) is drawn
}

TEST_CASE("Raster backend renders asynchronously and coalesces bursts", "[backend][raster]") {
  const auto syncDir = OutDir("sync");
  const auto asyncDir = OutDir("async");
  Session s("async");
  auto log = CaptureLog(s);
  Populate(s);
  REQUIRE(s.InitBackend("raster", "sync",
                        {{"output_dir", syncDir.string()}, {"width", "120"}, {"height", "90"}, {"async", "false"}}).ok);
  REQUIRE(s.InitBackend("raster", "async",
                        {{"output_dir", asyncDir.string()}, {"width", "120"}, {"height", "90"}}).ok);
  REQUIRE(s.Open("sync", "a").ok);
  REQUIRE(s.Open("async", "a").ok);
  for (int i = 1; i <= 20; ++i)
    REQUIRE(s.Resolve(ch::ChangeCameraMagnification{"a", 1.0 + 0.05 * i}).ok);
  REQUIRE(s.Flush().ok);

  const auto x = rs::ReadPng((syncDir / "a.png").string());
  const auto y = rs::ReadPng((asyncDir / "a.png").string());
  REQUIRE(x);
  REQUIRE(y);
  CHECK(rs::DifferentPixelFraction(*x, *y, 0) == 0.0);  // the last state wins
  const auto stats = s.BackendStats("async");
  REQUIRE(stats);
  CHECK(stats->Camera("a") >= 20);
}

TEST_CASE("Raster backend rejects bad options", "[backend][raster]") {
  Session s("opts");
  auto log = CaptureLog(s);
  CHECK_FALSE(s.InitBackend("raster", "r", {{"width", "-3"}}).ok);
  CHECK_FALSE(s.InitBackend("raster", "r2", {{"height", "abc"}}).ok);
}

TEST_CASE("Golden harness reports mismatches with a diff image", "[backend][raster]") {
  const auto golden = rs::ReadPng((fs::path(FCCVIS_TEST_SOURCE_DIR) / "data/golden/raster_scene_a.png").string());
  REQUIRE(golden);
  auto changed = *golden;
  for (int y = 0; y < 20; ++y)
    for (int x = 0; x < 20; ++x) changed.rgba[(static_cast<std::size_t>(y) * changed.width + x) * 4] = 0;
  if (const char* u = std::getenv("FCCVIS_UPDATE_GOLDEN"); u && *u && std::string(u) != "0") SKIP("update mode");
  const auto r = CompareGolden(changed, "raster_scene_a");
  CHECK_FALSE(r.ok);
  CHECK(r.fraction > 0.01);
  CHECK(fs::exists(fs::path(FCCVIS_TEST_BINARY_DIR) / "golden-out" / "raster_scene_a.diff.png"));
  CHECK(CompareGolden(*golden, "raster_scene_a").ok);
}

TEST_CASE("PNG reader decodes zlib-compressed, filtered PNGs from other encoders", "[backend][raster]") {
  // tests/data/png/pillow_rgba.png: written by Pillow (dynamic Huffman, adaptive filters).
  std::string error;
  const auto img = rs::ReadPng((fs::path(FCCVIS_TEST_SOURCE_DIR) / "data/png/pillow_rgba.png").string(), &error);
  INFO(error);
  REQUIRE(img);
  REQUIRE(img->width == 37);
  REQUIRE(img->height == 23);
  int bad = 0;
  for (int y = 0; y < 23; ++y)
    for (int x = 0; x < 37; ++x) {
      const std::uint32_t want = (std::uint32_t((x * 7) % 256) << 24) | (std::uint32_t((y * 11) % 256) << 16) |
                                 (std::uint32_t((x * y) % 256) << 8) | std::uint32_t(255 - (x + y) % 128);
      if (img->At(x, y) != want) ++bad;
    }
  CHECK(bad == 0);
}

TEST_CASE("PNG writer compresses and round-trips large images", "[backend][raster]") {
  rs::Image img{300, 200, std::vector<std::uint8_t>(300 * 200 * 4, 255)};
  for (int y = 50; y < 150; ++y)
    for (int x = 0; x < 300; ++x) {
      auto* p = img.rgba.data() + (static_cast<std::size_t>(y) * 300 + x) * 4;
      p[0] = static_cast<std::uint8_t>(x);
      p[1] = static_cast<std::uint8_t>(y * 3);
      p[2] = static_cast<std::uint8_t>((x * y) & 0xFF);
    }
  const auto bytes = rs::EncodePng(img);
  CHECK(bytes.size() < img.rgba.size() / 2);  // half the picture is noise-like
  const auto path = OutDir("png-big") / "big.png";
  REQUIRE(rs::WritePng(path.string(), img));
  const auto back = rs::ReadPng(path.string());
  REQUIRE(back);
  CHECK(back->rgba == img.rgba);
}
