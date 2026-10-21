// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T3.4: detector adapter (A1) on the toy compact detector (tests/data/detector/toy.xml).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "AdapterRegistry.h"
#include "Data.h"
#include "GoldenText.h"

#include <filesystem>

namespace ad = fccvis::adapters;
namespace dt = fccvis::data;
namespace fs = std::filesystem;
using Catch::Approx;
using namespace fccvis::test;

namespace {

std::string Toy() { return (fs::path(FCCVIS_TEST_SOURCE_DIR) / "data" / "detector" / "toy.xml").string(); }

std::unique_ptr<ad::Adapter> Detector() {
  auto a = ad::Registry::Global().Create("detector");
  REQUIRE(a);
  return a;
}

}  // namespace

TEST_CASE("Detector adapter: one series per sub-detector with metadata", "[adapter][detector]") {
  dt::SessionData data;
  const auto report = Detector()->Open(data, Toy(), {});
  INFO(report.ToString());
  REQUIRE(report.Ok());
  REQUIRE(report.registered.size() == 3);
  CHECK(report.registered[0].ref.name == "geometry/BeamPipe");
  CHECK(report.registered[1].ref.name == "geometry/Shield");
  CHECK(report.registered[2].ref.name == "geometry/Tracker");
  CHECK(report.skipped.empty());

  const auto tracker = data.Entry<dt::SurfaceElement>({"geometry/Tracker", ""}, 0);
  REQUIRE(tracker);
  CHECK(tracker->size() == 6);  // 3 layers x 2 slices at depth 3 (assembly, layer, slice)
  const auto& slice = (*tracker)[0];
  CHECK(slice.meta.Text("subdetector") == "Tracker");
  CHECK(slice.meta.Text("material") == "Silicon");
  CHECK(slice.meta.Text("solid") == "TGeoTubeSeg");
  CHECK(slice.meta.Number("system_id") == 2);
  CHECK(slice.meta.Number("layer") == 0);
  CHECK(slice.meta.Number("slice") == 0);
  CHECK(slice.meta.Has("path"));

  // Geometry in millimetres, world frame: the first layer's silicon spans r = 100 .. 100.3 mm
  CHECK(slice.geometry.shape.Contains({100.15, 0, 0}));
  CHECK_FALSE(slice.geometry.shape.Contains({99.0, 0, 0}));
  const auto mesh = slice.geometry.shape.Geometry().Tessellate(0.02);
  CHECK_FALSE(mesh.triangles.empty());

  // The shield box is placed at z = 1500 mm
  const auto shield = data.Entry<dt::SurfaceElement>({"geometry/Shield", ""}, 0);
  REQUIRE(shield->size() == 1);
  CHECK((*shield)[0].geometry.shape.Contains({0, 0, 1500}));
  CHECK_FALSE((*shield)[0].geometry.shape.Contains({0, 0, 0}));
  CHECK((*shield)[0].meta.Text("material") == "Iron");

  const auto golden = CompareGoldenText(Detector()->Help(), "detector_help.txt");
  INFO(golden.message);
  CHECK(golden.ok);
}

TEST_CASE("Detector adapter options and bad input", "[adapter][detector]") {
  dt::SessionData data;
  const auto shallow = Detector()->Open(data, Toy(), {{"depth", "1"}, {"include", "Track*"}});
  REQUIRE(shallow.registered.size() == 1);
  CHECK(data.Entry<dt::SurfaceElement>({"geometry/Tracker", ""}, 0)->size() == 3);  // the layers
  const auto again = Detector()->Open(data, Toy(), {{"depth", "1"}, {"include", "Track*"}});
  CHECK(again.registered[0].existing);

  dt::SessionData other;
  const auto excluded = Detector()->Open(other, Toy(), {{"exclude", "Tracker,Beam*"}});
  REQUIRE(excluded.registered.size() == 1);
  CHECK(excluded.registered[0].ref.name == "geometry/Shield");

  const auto missing = Detector()->Open(other, "/no/such/detector.xml", {});
  CHECK_FALSE(missing.Ok());
  CHECK(missing.skipped[0].reason == "cannot open file");
  CHECK_THROWS_AS(Detector()->Open(other, Toy(), {{"depth", "-1"}}), std::invalid_argument);
  CHECK_THROWS_AS(Detector()->Open(other, Toy(), {{"bogus", "1"}}), std::invalid_argument);
}

TEST_CASE("Detector adapter reports sub-detectors cut by max_volumes", "[adapter][detector]") {
  // 8 volumes in all (BeamPipe 1, Shield 1, Tracker 6); a budget of 4 ends inside one sub-detector.
  dt::SessionData data;
  const auto report = Detector()->Open(data, Toy(), {{"max_volumes", "4"}});
  INFO(report.ToString());
  std::size_t kept = 0;
  for (const auto& s : report.registered) kept += data.Entry<dt::SurfaceElement>(s.ref, 0)->size();
  CHECK(kept == 4);
  std::size_t incomplete = 0;
  for (const auto& s : report.skipped)
    if (s.reason.find("max_volumes reached: incomplete") != std::string::npos) ++incomplete;
  CHECK(incomplete == 1);  // the cut sub-detector is not silently registered as complete

  dt::SessionData exact;
  const auto fits = Detector()->Open(exact, Toy(), {{"max_volumes", "8"}});
  CHECK(fits.skipped.empty());
}
