// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
/**
 * @file test_histogram.cc
 * @brief Histogram maker (B3): spec parsing, filling, outputs, and identical
 *        selections to a display backend.
 */
#include <catch2/catch_test_macros.hpp>

#include "GoldenText.h"
#include "Histogram.h"
#include "SceneChanges.h"
#include "Session.h"
#include "TestSupport.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace ch = fccvis::changes;
namespace dt = fccvis::data;
namespace hi = fccvis::backend::hist;
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

}  // namespace

TEST_CASE("Histogram specs parse and reject bad input", "[backend][histogram]") {
  const auto specs = hi::ParseSpecs("hits:energy; front/hits:layer:4 ; tracks:charge:2:-1.5:1.5;");
  REQUIRE(specs.size() == 3);
  CHECK(specs[0].scene.empty());
  CHECK(specs[0].layer == "hits");
  CHECK(specs[0].bins == 20);
  CHECK(specs[1].scene == "front");
  CHECK(specs[1].bins == 4);
  REQUIRE(specs[2].range);
  CHECK(specs[2].range->first == -1.5);
  for (const char* bad : {"hits", "hits:e:0", "hits:e:2:1", "hits:e:2:3:1", ":e", "hits:e:x"}) {
    INFO(bad);
    CHECK_THROWS_AS(hi::ParseSpecs(bad), std::invalid_argument);
  }
}

TEST_CASE("Histograms fill numeric bins and text categories", "[backend][histogram]") {
  std::vector<dt::Metadata> m(12);
  for (int i = 0; i < 10; ++i) m[i].Set("x", static_cast<double>(i));
  m[10].Set("x", -1.0);
  // m[11] has no "x"
  for (int i = 0; i < 12; ++i) m[i].Set("det", i % 3 ? std::string("ECal") : std::string("HCal"));
  m[0].Set("unit:x", std::string("GeV"));
  std::vector<const dt::Metadata*> sel;
  for (const auto& x : m) sel.push_back(&x);

  const auto h = hi::Fill({"", "l", "x", 5, std::pair{0.0, 10.0}}, "s", sel);
  CHECK_FALSE(h.categorical);
  CHECK(h.counts == std::vector<std::uint64_t>{2, 2, 2, 2, 2});
  CHECK(h.underflow == 1);
  CHECK(h.overflow == 0);
  CHECK(h.missing == 1);
  CHECK(h.filled == 10);
  CHECK(h.unit == "GeV");

  const auto autoRange = hi::Fill({"", "l", "x", 11, std::nullopt}, "s", sel);
  CHECK(autoRange.lo == -1.0);
  CHECK(autoRange.filled == 11);  // the maximum is included
  CHECK(autoRange.overflow == 0);

  const auto c = hi::Fill({"", "l", "det", 20, std::nullopt}, "s", sel);
  CHECK(c.categorical);
  CHECK(c.categories.at("ECal") == 8);
  CHECK(c.categories.at("HCal") == 4);

  const auto csv = hi::ToCsv({h, c});
  CHECK(csv.rfind("scene,layer,attribute,kind,bin,low,high,count\n", 0) == 0);
  CHECK(csv.find("s,l,x,bin,0,0,2,2\n") != std::string::npos);
  CHECK(csv.find("s,l,x,underflow,,,0,1\n") != std::string::npos);
  CHECK(csv.find("s,l,det,category,ECal,,,8\n") != std::string::npos);
  const auto svg = hi::ToSvg({h, c}, 400, 200);
  CHECK(Count(svg, "<g ") == 2);
  CHECK(svg.find("x [GeV]") != std::string::npos);
}

TEST_CASE("Histogram backend uses the layer selection of the display", "[backend][histogram]") {
  Session s("h");
  auto log = CaptureLog(s);
  REQUIRE(s.Data().Add<dt::PointElement>("pts", "fixture",
      std::make_unique<dt::InMemorySeries<dt::PointElement>>(std::vector<std::vector<dt::PointElement>>{MakePoints(8)})));
  s.DataChanged();
  REQUIRE(s.CreateScene("a"));
  REQUIRE(s.Resolve(ch::AddLayer<dt::PointElement>{"a", "hits", {"pts", ""}}).ok);
  auto energetic = std::make_shared<fccvis::filter::Filter<dt::PointElement>>(
      [](const dt::PointElement& e) { return e.meta.Number("e").value_or(0) > 2.0; });
  REQUIRE(s.Resolve(ch::SetLayerFilter<dt::PointElement>{"a", "hits", energetic}).ok);

  const auto dir = fs::path(FCCVIS_TEST_BINARY_DIR) / "histogram-out";
  fs::remove_all(dir);
  REQUIRE(s.InitBackend("histogram", "h", {{"output_dir", dir.string()}, {"histograms", "hits:e:4:0:8"},
                                           {"width", "300"}, {"height", "200"}}).ok);
  REQUIRE(s.InitBackend("svg", "v", {{"output_dir", dir.string()}}).ok);
  REQUIRE(s.Open("h", "a").ok);
  REQUIRE(s.Open("v", "a").ok);
  REQUIRE(s.Flush().ok);

  const auto csv = Read(dir / "a.hist.csv");
  // e = 0.5 .. 7.5; selected (e > 2): 2.5 .. 7.5 -> bins [0,2) 0, [2,4) 2, [4,6) 2, [6,8) 2
  CHECK(csv.find("a,hits,e,bin,1,2,4,2\n") != std::string::npos);
  const auto golden = CompareGoldenText(csv, "histogram_a.hist.csv");
  INFO(golden.message);
  CHECK(golden.ok);
  CHECK(fs::exists(dir / "a.hist.svg"));

  // Same selection as the display backend: 6 points drawn.
  CHECK(Count(Read(dir / "a.svg"), "data-layer=\"hits\"") == 6);

  // Changing the filter rewrites the histogram.
  REQUIRE(s.Resolve(ch::SetLayerFilter<dt::PointElement>{"a", "hits", nullptr}).ok);
  REQUIRE(s.Flush().ok);
  CHECK(Read(dir / "a.hist.csv").find("a,hits,e,bin,0,0,2,2\n") != std::string::npos);

  // Default: every attribute of every layer.
  REQUIRE(s.InitBackend("histogram", "all", {{"output_dir", (dir / "all").string()}, {"formats", "csv"}}).ok);
  REQUIRE(s.Open("all", "a").ok);
  REQUIRE(s.Flush().ok);
  CHECK(Read(dir / "all" / "a.hist.csv").find("a,hits,e,bin,") != std::string::npos);
  CHECK_FALSE(fs::exists(dir / "all" / "a.hist.svg"));

  CHECK_FALSE(s.InitBackend("histogram", "bad", {{"histograms", "hits"}}).ok);
  CHECK_FALSE(s.InitBackend("histogram", "bad2", {{"formats", "png"}}).ok);
}
