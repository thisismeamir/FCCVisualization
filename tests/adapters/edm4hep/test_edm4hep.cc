// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T3.5: EDM4hep adapter (A2). Fixture: tests/data/edm4hep/events.edm4hep.root
// written by tests/data/edm4hep/make_fixture.cc (3 events).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "AdapterRegistry.h"
#include "Data.h"
#include "GoldenText.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

namespace ad = fccvis::adapters;
namespace dt = fccvis::data;
namespace fs = std::filesystem;
using Catch::Approx;
using fccvis::test::CompareGoldenText;

namespace {

std::string Fixture() {
  return (fs::path(FCCVIS_TEST_SOURCE_DIR) / "data" / "edm4hep" / "events.edm4hep.root").string();
}

std::unique_ptr<ad::Adapter> Edm() {
  auto a = ad::Registry::Global().Create("edm4hep");
  REQUIRE(a);
  return a;
}

std::map<std::string, std::string> Skipped(const ad::LoadReport& r) {
  std::map<std::string, std::string> out;
  for (const auto& s : r.skipped) out[s.name] = s.reason;
  return out;
}

const ad::RegisteredSeries* Find(const ad::LoadReport& r, const std::string& name) {
  for (const auto& s : r.registered)
    if (s.ref.name == name) return &s;
  return nullptr;
}

template <typename X>
std::shared_ptr<const std::vector<X>> Entry(const dt::SessionData& d, const std::string& name, std::size_t e) {
  auto p = d.Entry<X>({name, ""}, e);
  REQUIRE(p);
  return p;
}

void CheckVec(const std::array<double, 3>& v, double x, double y, double z) {
  CHECK(v[0] == Approx(x).margin(1e-4));
  CHECK(v[1] == Approx(y).margin(1e-4));
  CHECK(v[2] == Approx(z).margin(1e-4));
}

}  // namespace

TEST_CASE("EDM4hep adapter: series per collection, skipped types with reasons", "[adapter][edm4hep]") {
  dt::SessionData data;
  const auto r = Edm()->Open(data, Fixture(), {});
  INFO(r.ToString());
  REQUIRE(r.Ok());
  CHECK(r.source.rfind("edm4hep:/", 0) == 0);
  std::vector<std::string> names;
  for (const auto& s : r.registered) names.push_back(s.ref.name);
  CHECK(names == std::vector<std::string>{"Clusters", "ECalBarrelHits", "ECalBarrelSimHits", "MCParticles", "PandoraPFOs",
                                          "TrackerHits", "Tracks", "VertexBarrelHits"});
  for (const auto& s : r.registered) CHECK(s.entries == 3);
  CHECK(Find(r, "Tracks")->kind == dt::ElementKind::Line);
  CHECK(Find(r, "MCParticles")->kind == dt::ElementKind::Line);
  CHECK(Find(r, "PandoraPFOs")->kind == dt::ElementKind::Line);
  CHECK(Find(r, "Clusters")->kind == dt::ElementKind::Point);
  CHECK(Find(r, "VertexBarrelHits")->metadataKeys.count("unit:EDep") == 1);

  const auto skipped = Skipped(r);
  CHECK(skipped.at("ECalRawHits") == "raw calorimeter hits have no position");
  CHECK(skipped.at("EventHeader") == "event header: no positions");
  CHECK(skipped.at("Weights").find("not mapped") != std::string::npos);
  CHECK(data.Size<dt::PointElement>({"VertexBarrelHits", ""}) == 3);
}

TEST_CASE("EDM4hep adapter: hits, clusters, geometry and metadata", "[adapter][edm4hep]") {
  dt::SessionData data;
  REQUIRE(Edm()->Open(data, Fixture(), {}).Ok());

  // Random access: event 2 first (4 + 2 hits), then event 0.
  CHECK(Entry<dt::PointElement>(data, "VertexBarrelHits", 2)->size() == 6);
  const auto hits = Entry<dt::PointElement>(data, "VertexBarrelHits", 0);
  REQUIRE(hits->size() == 4);
  const auto& h = (*hits)[2];
  CheckVec(h.geometry.position, 300, 20, 0);
  CHECK(h.meta.Number("cellID") == 1002);
  CHECK(*h.meta.Number("EDep") == Approx(3e-5));
  CHECK(*h.meta.Number("time") == Approx(1.0));
  CHECK(h.meta.Text("unit:EDep") == "GeV");
  CHECK(*h.meta.Number("p") == Approx(1.0));

  // Relation stored as the related object's id.
  const auto mc = Entry<dt::LineElement>(data, "MCParticles", 0);
  REQUIRE(mc->size() == 3);
  CHECK(h.meta.Number("particle") == 0);
  CHECK(h.meta.Number("particle_collection") == (*mc)[0].meta.Number("index_collection"));
  CHECK((*mc)[1].meta.Number("index") == 1);
  CHECK(h.meta.Number("index_collection") != (*mc)[0].meta.Number("index_collection"));

  const auto calo = Entry<dt::PointElement>(data, "ECalBarrelHits", 1);
  REQUIRE(calo->size() == 5);
  CheckVec((*calo)[3].geometry.position, 1800, 60, 20);
  CHECK(*(*calo)[3].meta.Number("energy") == Approx(3.0));  // 0.5 * 4 + event 1
  CHECK((*calo)[3].meta.Number("type") == 1);

  const auto clusters = Entry<dt::PointElement>(data, "Clusters", 2);
  REQUIRE(clusters->size() == 1);
  CHECK(*(*clusters)[0].meta.Number("energy") == Approx(5.0));
  CHECK((*clusters)[0].meta.Number("hits") == 2);

  const auto sim = Entry<dt::PointElement>(data, "ECalBarrelSimHits", 0);
  REQUIRE(sim->size() == 2);
  CHECK((*sim)[1].meta.Number("contributions") == 0);
}

TEST_CASE("EDM4hep adapter: tracks, MC particles and reconstructed particles as lines", "[adapter][edm4hep]") {
  dt::SessionData data;
  REQUIRE(Edm()->Open(data, Fixture(), {}).Ok());

  const auto tracks = Entry<dt::LineElement>(data, "Tracks", 0);
  REQUIRE(tracks->size() == 1);
  const auto& tv = (*tracks)[0].geometry.vertices;
  REQUIRE(tv.size() == 3);  // states ordered IP, first hit, last hit
  CheckVec(tv[0], 0, 1, 2);  // reference point + (-D0 sin phi, D0 cos phi, Z0)
  CheckVec(tv[1], 100, 0, 0);
  CheckVec(tv[2], 300, 0, 20);
  CHECK((*tracks)[0].meta.Number("hits") == 3);
  CHECK((*tracks)[0].meta.Number("states") == 3);
  CHECK(*(*tracks)[0].meta.Number("chi2") == Approx(1.5));

  const auto mc = Entry<dt::LineElement>(data, "MCParticles", 0);
  const auto& e = (*mc)[0];
  REQUIRE(e.geometry.vertices.size() == 2);
  CheckVec(e.geometry.vertices[0], 0, 0, 0);
  CheckVec(e.geometry.vertices[1], 1500, 0, 500);
  CHECK(e.meta.Number("PDG") == 11);
  CHECK(e.meta.Number("charge") == -1);
  CHECK(e.meta.Number("generatorStatus") == 1);
  CHECK(e.meta.Flag("endpoint_set") == true);
  CHECK(e.meta.Number("daughters") == 1);
  CHECK((*mc)[1].meta.Number("parents") == 1);
  const auto& photon = (*mc)[2];  // endpoint not set: 1000 mm along the momentum
  CHECK(photon.meta.Flag("endpoint_set") == false);
  const double phi = 1.0, pn = std::sqrt(9.0 + 1.0);
  CheckVec(photon.geometry.vertices[1], 1000 * 3 * std::cos(phi) / pn, 1000 * 3 * std::sin(phi) / pn, 1000 / pn);
  CHECK(*photon.meta.Number("pt") == Approx(3.0));

  const auto reco = Entry<dt::LineElement>(data, "PandoraPFOs", 1);
  REQUIRE(reco->size() == 2);
  CheckVec((*reco)[0].geometry.vertices[1], 0, 800, 600);
  CHECK(*(*reco)[0].meta.Number("p") == Approx(5.0));
  CHECK(*(*reco)[0].meta.Number("pt") == Approx(4.0));
  CHECK((*reco)[0].meta.Number("tracks") == 1);
  CHECK((*reco)[1].meta.Number("tracks") == 0);
  CHECK((*reco)[1].meta.Number("PDG") == 22);
}

TEST_CASE("EDM4hep adapter options", "[adapter][edm4hep]") {
  SECTION("collections and exclude select by name pattern") {
    dt::SessionData data;
    const auto r = Edm()->Open(data, Fixture(), {{"collections", "ECal*, Tracks"}, {"exclude", "*Sim*"}});
    std::vector<std::string> names;
    for (const auto& s : r.registered) names.push_back(s.ref.name);
    CHECK(names == std::vector<std::string>{"ECalBarrelHits", "Tracks"});
    CHECK(Skipped(r).count("ECalRawHits") == 1);
    CHECK(Skipped(r).count("EventHeader") == 0);
  }
  SECTION("track=hits joins the track's hits ordered by distance") {
    dt::SessionData data;
    REQUIRE(Edm()->Open(data, Fixture(), {{"collections", "Tracks"}, {"track", "hits"}}).Ok());
    const auto t = Entry<dt::LineElement>(data, "Tracks", 0);
    const auto& v = (*t)[0].geometry.vertices;
    REQUIRE(v.size() == 3);
    CheckVec(v[0], 100, 0, 0);
    CheckVec(v[2], 300, 0, 20);
  }
  SECTION("lengths") {
    dt::SessionData data;
    REQUIRE(Edm()->Open(data, Fixture(), {{"collections", "PandoraPFOs"}, {"reco_length", "10"}}).Ok());
    CheckVec((*Entry<dt::LineElement>(data, "PandoraPFOs", 0))[0].geometry.vertices[1], 0, 8, 6);
  }
  SECTION("invalid options throw") {
    dt::SessionData data;
    CHECK_THROWS_AS(Edm()->Open(data, Fixture(), {{"track", "helix"}}), std::invalid_argument);
    CHECK_THROWS_AS(Edm()->Open(data, Fixture(), {{"mc_length", "-1"}}), std::invalid_argument);
    CHECK_THROWS_AS(Edm()->Open(data, Fixture(), {{"colour", "red"}}), std::invalid_argument);
  }
  SECTION("nothing selected") {
    dt::SessionData data;
    const auto r = Edm()->Open(data, Fixture(), {{"collections", "Nope*"}});
    CHECK_FALSE(r.Ok());
    REQUIRE(r.skipped.size() == 1);
    CHECK(r.skipped[0].reason.find("no collection matches") != std::string::npos);
  }
}

TEST_CASE("EDM4hep adapter: bad input and re-open", "[adapter][edm4hep]") {
  dt::SessionData data;
  auto missing = Edm()->Open(data, "/nonexistent/events.root", {});
  CHECK_FALSE(missing.Ok());
  REQUIRE(missing.skipped.size() == 1);
  CHECK(missing.skipped[0].reason == "cannot open file");

  const auto bad = fs::path(FCCVIS_TEST_BINARY_DIR) / "not-podio.root";
  std::ofstream(bad) << "this is not a ROOT file\n";
  auto garbage = Edm()->Open(data, bad.string(), {});
  CHECK_FALSE(garbage.Ok());
  REQUIRE(garbage.skipped.size() == 1);
  CHECK(garbage.skipped[0].reason.rfind("not readable by podio", 0) == 0);

  REQUIRE(Edm()->Open(data, Fixture(), {}).Ok());
  const auto again = Edm()->Open(data, Fixture(), {});
  REQUIRE(again.Ok());
  CHECK(std::all_of(again.registered.begin(), again.registered.end(), [](const auto& s) { return s.existing; }));
  CHECK(data.List().size() == 8);
}

TEST_CASE("EDM4hep adapter help is stable", "[adapter][edm4hep]") {
  const auto golden = CompareGoldenText(Edm()->Help(), "edm4hep_help.txt");
  INFO(golden.message);
  CHECK(golden.ok);
}
