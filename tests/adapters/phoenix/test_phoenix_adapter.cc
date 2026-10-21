// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T3.6: Phoenix event-data JSON adapter (A4). Fixtures: tests/data/phoenix.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "AdapterRegistry.h"
#include "BackendRegistry.h"
#include "Data.h"
#include "GoldenText.h"
#include "Session.h"
#include "TestSupport.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>

namespace ad = fccvis::adapters;
namespace dt = fccvis::data;
namespace fs = std::filesystem;
using Catch::Approx;
using fccvis::test::CompareGoldenText;

namespace {

std::string Fixture(const std::string& name) {
  return (fs::path(FCCVIS_TEST_SOURCE_DIR) / "data" / "phoenix" / name).string();
}

std::unique_ptr<ad::Adapter> Phoenix() {
  auto a = ad::Registry::Global().Create("phoenix");
  REQUIRE(a);
  return a;
}

std::map<std::string, std::string> Skipped(const ad::LoadReport& r) {
  std::map<std::string, std::string> out;
  for (const auto& s : r.skipped) out[s.name] = s.reason;
  return out;
}

std::map<std::string, dt::ElementKind> Registered(const ad::LoadReport& r) {
  std::map<std::string, dt::ElementKind> out;
  for (const auto& s : r.registered) out[s.ref.name] = s.kind;
  return out;
}

}  // namespace

TEST_CASE("Phoenix adapter: series per collection, kinds from the objects", "[adapter][phoenix-json]") {
  dt::SessionData data;
  const auto r = Phoenix()->Open(data, Fixture("events.json"), {});
  INFO(r.ToString());
  REQUIRE(r.Ok());
  CHECK(Registered(r) == std::map<std::string, dt::ElementKind>{{"Hits/shared", dt::ElementKind::Point},
                                                                {"Tracks/shared", dt::ElementKind::Line},
                                                                {"pixels", dt::ElementKind::Point},
                                                                {"primary", dt::ElementKind::Point},
                                                                {"tracks", dt::ElementKind::Line}});
  for (const auto& s : r.registered) CHECK(s.entries == 2);
  const auto skipped = Skipped(r);
  CHECK(skipped.at("Jets/jets").rfind("no positions", 0) == 0);
  CHECK(skipped.at("metadata").rfind("not an event", 0) == 0);
}

TEST_CASE("Phoenix adapter: geometry and metadata", "[adapter][phoenix-json]") {
  dt::SessionData data;
  REQUIRE(Phoenix()->Open(data, Fixture("events.json"), {}).Ok());

  const auto pix = data.Entry<dt::PointElement>({"pixels", ""}, 0);
  REQUIRE(pix);
  REQUIRE(pix->size() == 2);  // the object without a position is left out
  CHECK((*pix)[0].geometry.position == fccvis::geometry::Transform::Vector{1.5, 2.5, 3.5});
  CHECK((*pix)[0].meta.Find("id"));
  CHECK(std::get<std::int64_t>(*(*pix)[0].meta.Find("id")) == 6942916751126429696LL);  // exact
  CHECK(*(*pix)[0].meta.Number("energy") == Approx(0.25));
  CHECK((*pix)[0].meta.Text("type") == "Point");
  CHECK((*pix)[0].meta.Flag("good") == true);
  CHECK((*pix)[1].geometry.position == fccvis::geometry::Transform::Vector{10, 20, 30});  // plain array

  const auto second = data.Entry<dt::PointElement>({"pixels", ""}, 1);
  REQUIRE(second);
  REQUIRE(second->size() == 1);
  CHECK(*(*second)[0].meta.Number("energy") == Approx(1000));

  const auto vtx = data.Entry<dt::PointElement>({"primary", ""}, 0);
  REQUIRE(vtx);
  REQUIRE(vtx->size() == 1);
  CHECK((*vtx)[0].geometry.position == fccvis::geometry::Transform::Vector{0.1, -0.2, 5});
  CHECK((*vtx)[0].meta.Number("ntracks") == 12);
  CHECK_FALSE((*vtx)[0].meta.Has("x"));
  CHECK(data.Entry<dt::PointElement>({"primary", ""}, 1)->empty());  // absent from the second event

  const auto trk = data.Entry<dt::LineElement>({"tracks", ""}, 0);
  REQUIRE(trk);
  REQUIRE(trk->size() == 2);
  CHECK((*trk)[0].geometry.vertices.size() == 3);
  CHECK((*trk)[0].geometry.vertices[2] == fccvis::geometry::Transform::Vector{200, 40, 10});
  CHECK(*(*trk)[0].meta.Number("d0") == Approx(0.1));
  CHECK(*(*trk)[0].meta.Number("qOverP") == Approx(0.0003));
  CHECK((*trk)[0].meta.Text("color") == "#ff0000");
  CHECK((*trk)[1].geometry.vertices.size() == 1);  // not extrapolated
}

TEST_CASE("Phoenix adapter reads what the Phoenix backend writes", "[adapter][phoenix-json]") {
  using fccvis::session::Session;
  if (!fccvis::backend::Registry::Global().Create("phoenix")) SKIP("the Phoenix backend is not built");
  const auto dir = fs::path(FCCVIS_TEST_BINARY_DIR) / "phoenix-roundtrip";
  fs::remove_all(dir);
  Session s("roundtrip");
  auto points = fccvis::test::MakePoints(4);
  auto lines = fccvis::test::MakeLines(2);
  REQUIRE(s.Data().Add<dt::PointElement>("hits", "fixture",
      std::make_unique<dt::InMemorySeries<dt::PointElement>>(std::vector<std::vector<dt::PointElement>>{points})));
  REQUIRE(s.Data().Add<dt::LineElement>("trk", "fixture",
      std::make_unique<dt::InMemorySeries<dt::LineElement>>(std::vector<std::vector<dt::LineElement>>{lines})));
  s.DataChanged();
  REQUIRE(s.CreateScene("evt"));
  REQUIRE(s.Resolve(fccvis::changes::AddLayer<dt::PointElement>{"evt", "hits", {"hits", ""}}).ok);
  REQUIRE(s.Resolve(fccvis::changes::AddLayer<dt::LineElement>{"evt", "trk", {"trk", ""}}).ok);
  REQUIRE(s.InitBackend("phoenix", "web", {{"output_dir", dir.string()}}).ok);
  REQUIRE(s.Open("web", "evt").ok);
  REQUIRE(s.Flush("web").ok);

  dt::SessionData back;
  const auto r = Phoenix()->Open(back, (dir / "events.json").string(), {});
  INFO(r.ToString());
  REQUIRE(r.Ok());
  std::size_t nPoints = 0;
  for (const auto& info : back.List<dt::PointElement>()) {
    const auto e = back.Entry<dt::PointElement>({info.name, info.source}, 0);
    for (const auto& p : *e) {
      ++nPoints;
      bool found = false;
      for (const auto& q : points)
        found = found || (p.geometry.position == q.geometry.position &&
                          p.meta.Number("e") == q.meta.Number("e"));
      CHECK(found);
    }
  }
  CHECK(nPoints == points.size());
  const auto t = back.Entry<dt::LineElement>({"trk", ""}, 0);
  REQUIRE(t);
  REQUIRE(t->size() == lines.size());
  CHECK((*t)[0].geometry.vertices == lines[0].geometry.vertices);
}

TEST_CASE("Phoenix adapter: a real ATLAS file and single-event files", "[adapter][phoenix-json]") {
  dt::SessionData data;
  const auto r = Phoenix()->Open(data, Fixture("ATLAS_calibration.json"), {});
  INFO(r.ToString());
  REQUIRE(Registered(r) == std::map<std::string, dt::ElementKind>{{"TestTracks", dt::ElementKind::Line}});
  const auto t = data.Entry<dt::LineElement>({"TestTracks", ""}, 0);
  REQUIRE(t);
  CHECK(t->size() == 9);
  CHECK(t->front().meta.Text("label") == "0.000000/0.000000");
  CHECK(Skipped(r).count("CaloClusters/TestClusters") == 1);

  const auto single = fs::path(FCCVIS_TEST_BINARY_DIR) / "phoenix-single.json";
  std::ofstream(single) << R"({"event number": 5, "Hits": {"h": [[1, 2, 3], [4, 5, 6]]}})";
  dt::SessionData one;
  const auto r1 = Phoenix()->Open(one, single.string(), {});
  REQUIRE(r1.Ok());
  CHECK(one.Size<dt::PointElement>({"h", ""}) == 1);
  CHECK(one.Entry<dt::PointElement>({"h", ""}, 0)->size() == 2);
}

TEST_CASE("Phoenix adapter: options, bad input and re-open", "[adapter][phoenix-json]") {
  SECTION("collections and exclude") {
    dt::SessionData data;
    const auto r = Phoenix()->Open(data, Fixture("events.json"), {{"collections", "pix*,Tracks/*"}, {"exclude", "Tracks/shared"}});
    CHECK(Registered(r) == std::map<std::string, dt::ElementKind>{{"pixels", dt::ElementKind::Point}});
  }
  SECTION("bad options") {
    dt::SessionData data;
    CHECK_THROWS_AS(Phoenix()->Open(data, Fixture("events.json"), {{"cache", "0"}}), std::invalid_argument);
    CHECK_THROWS_AS(Phoenix()->Open(data, Fixture("events.json"), {{"colour", "x"}}), std::invalid_argument);
  }
  SECTION("bad files") {
    dt::SessionData data;
    CHECK(Phoenix()->Open(data, "/nonexistent.json", {}).skipped.at(0).reason == "cannot open file");
    const auto broken = fs::path(FCCVIS_TEST_BINARY_DIR) / "phoenix-broken.json";
    std::ofstream(broken) << "{\"e\": {\"Hits\": {\"h\": [[1, 2, 3]]}";
    const auto r = Phoenix()->Open(data, broken.string(), {});
    CHECK_FALSE(r.Ok());
    CHECK(r.skipped.at(0).reason.rfind("not valid JSON: JSON line 1, column", 0) == 0);
    const auto other = fs::path(FCCVIS_TEST_BINARY_DIR) / "phoenix-other.json";
    std::ofstream(other) << R"({"runNumber": 1, "PVS": [{"pv_x": 1}]})";
    CHECK(Phoenix()->Open(data, other.string(), {}).skipped.at(0).reason.find("not in the Phoenix event-data format") !=
          std::string::npos);
  }
  SECTION("re-open") {
    dt::SessionData data;
    REQUIRE(Phoenix()->Open(data, Fixture("events.json"), {}).Ok());
    const auto again = Phoenix()->Open(data, Fixture("events.json"), {});
    for (const auto& s : again.registered) CHECK(s.existing);
  }
}

TEST_CASE("Phoenix adapter help is stable", "[adapter][phoenix-json]") {
  const auto golden = CompareGoldenText(Phoenix()->Help(), "phoenix_adapter_help.txt");
  INFO(golden.message);
  CHECK(golden.ok);
}
