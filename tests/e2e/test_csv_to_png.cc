// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T3.3: first end-to-end test. CSV files -> adapter -> session (scenes,
// stdlib styles and filters, layout) -> raster backend -> PNG == golden.
#include <catch2/catch_test_macros.hpp>

#include "AdapterRegistry.h"
#include "CameraChanges.h"
#include "GoldenImage.h"
#include "Png.h"
#include "SceneChanges.h"
#include "Session.h"
#include "TestSupport.h"
#include "fccvis/stdlib/Filters.h"
#include "fccvis/stdlib/Styles.h"

#include <filesystem>

namespace ad = fccvis::adapters;
namespace ch = fccvis::changes;
namespace dt = fccvis::data;
namespace rs = fccvis::backend::raster;
namespace sl = fccvis::stdlib;
namespace fs = std::filesystem;
using fccvis::session::Session;
using namespace fccvis::test;

namespace {

std::string Fixture(const std::string& name) {
  return (fs::path(FCCVIS_TEST_SOURCE_DIR) / "data" / "csv" / name).string();
}

template <typename X>
void ApplySheet(Session& s, const std::string& scene, const std::string& layer, const fccvis::style::StyleSheet<X>& sheet) {
  REQUIRE(s.Resolve(ch::AddStyleRule<X>{scene, layer, {}, sheet.base}).ok);
  for (const auto& r : sheet.rules) REQUIRE(s.Resolve(ch::AddStyleRule<X>{scene, layer, r.when, r.style}).ok);
}

}  // namespace

TEST_CASE("CSV to session to raster PNG matches the golden", "[e2e][csv][raster]") {
  Session s("e2e");
  auto log = CaptureLog(s);

  // Adapters fill the session's data; the session is told once.
  auto csv = ad::Registry::Global().Create("csv");
  REQUIRE(csv);
  const auto hits = csv->Open(s.Data(), Fixture("hits.csv"), {{"entry", "event"}});
  const auto tracks = csv->Open(s.Data(), Fixture("tracks.csv"),
      {{"delimiter", ";"}, {"kind", "line"}, {"group", "track"}, {"order", "step"}, {"entry", "event"}});
  INFO(hits.ToString() << tracks.ToString());
  REQUIRE(hits.Ok());
  REQUIRE(tracks.Ok());
  s.DataChanged();

  // Two views of event 0: everything along z, and only energetic ECal hits along x.
  for (const char* n : {"front", "side"}) {
    REQUIRE(s.CreateScene(n));
    REQUIRE(s.Resolve(ch::SetCamera{n, std::make_shared<fccvis::scene::camera::Camera>(n)}).ok);
    REQUIRE(s.Resolve(ch::AddLayer<dt::PointElement>{n, "hits", {"hits", ""}}).ok);
    REQUIRE(s.Resolve(ch::AddLayer<dt::LineElement>{n, "tracks", {"tracks", ""}}).ok);
    ApplySheet(s, n, "hits", sl::HitsByEnergy({0, 4}));
    ApplySheet(s, n, "tracks", sl::TracksByCharge());
  }
  REQUIRE(s.Resolve(ch::ChangeCameraDirection{"side", {1.0, 0.0, 0.0}}).ok);
  REQUIRE(s.Resolve(ch::SetLayerFilter<dt::PointElement>{
      "side", "hits",
      std::make_shared<fccvis::filter::Filter<dt::PointElement>>(
          (sl::EnergyAbove(1.0) & (sl::Attr<std::string>("detector") == "ECalBarrel")).For<dt::PointElement>())}).ok);
  REQUIRE(s.SetLayout(S({P("front"), P("side")})).ok);

  const auto dir = fs::path(FCCVIS_TEST_BINARY_DIR) / "e2e-out";
  fs::remove_all(dir);
  REQUIRE(s.InitBackend("raster", "png", {{"output_dir", dir.string()}, {"width", "320"}, {"height", "160"}}).ok);
  REQUIRE(s.Open("png", "front").ok);
  REQUIRE(s.Open("png", "side").ok);
  REQUIRE(s.Flush("png").ok);

  auto image = rs::ReadPng((dir / "layout.png").string());
  REQUIRE(image);
  auto golden = CompareGolden(*image, "e2e_csv_event0");
  INFO(golden.message);
  CHECK(golden.ok);

  // Next event: move both layers' cursors; the picture changes and matches its golden.
  for (const char* n : {"front", "side"}) {
    REQUIRE(s.Resolve(ch::SetEntry<dt::PointElement>{n, "hits", 1}).ok);
    REQUIRE(s.Resolve(ch::SetEntry<dt::LineElement>{n, "tracks", 1}).ok);
  }
  REQUIRE(s.Flush("png").ok);
  auto next = rs::ReadPng((dir / "layout.png").string());
  REQUIRE(next);
  CHECK(rs::DifferentPixelFraction(*image, *next, 8) > 0.001);
  golden = CompareGolden(*next, "e2e_csv_event1");
  INFO(golden.message);
  CHECK(golden.ok);

  CHECK(CountLog(*log, fccvis::changes::Severity::Error, "") == 0);
  CHECK(s.DetachBackend("png").ok);
}
