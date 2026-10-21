// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T3.2: CSV adapter (A3). Fixtures: tests/data/csv (make_fixtures.py).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "AdapterRegistry.h"
#include "Data.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace ad = fccvis::adapters;
namespace dt = fccvis::data;
namespace fs = std::filesystem;
using Catch::Approx;

namespace {

std::string Fixture(const std::string& name) {
  return (fs::path(FCCVIS_TEST_SOURCE_DIR) / "data" / "csv" / name).string();
}

std::unique_ptr<ad::Adapter> Csv() {
  auto a = ad::Registry::Global().Create("csv");
  REQUIRE(a);
  return a;
}

std::string ReadFile(const std::string& p) {
  std::ifstream in(p);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

}  // namespace

TEST_CASE("CSV adapter is registered with stable help", "[adapter][csv]") {
  const auto names = ad::Registry::Global().Names();
  CHECK(std::find(names.begin(), names.end(), "csv") != names.end());
  auto csv = Csv();
  CHECK(csv->Name() == "csv");
  const auto help = csv->Help();
  CHECK(help == ad::Registry::Global().Help("csv"));
  CHECK(ad::Registry::Global().ForPath("events/hits.CSV") != nullptr);
  CHECK(ad::Registry::Global().ForPath("hits.unknown") == nullptr);

  const auto golden = Fixture("help.txt");
  if (const char* u = std::getenv("FCCVIS_UPDATE_GOLDEN"); u && *u && std::string(u) != "0") {
    std::ofstream(golden) << help;
  }
  CHECK(help == ReadFile(golden));
}

TEST_CASE("CSV points: series, entries, geometry and typed metadata", "[adapter][csv]") {
  dt::SessionData data;
  const auto report = Csv()->Open(data, Fixture("hits.csv"), {{"entry", "event"}});
  INFO(report.ToString());
  REQUIRE(report.Ok());
  CHECK(report.skipped.empty());
  REQUIRE(report.registered.size() == 1);
  const auto& s = report.registered[0];
  CHECK(s.ref.name == "hits");
  CHECK(s.ref.source.rfind("csv:", 0) == 0);
  CHECK(s.kind == dt::ElementKind::Point);
  CHECK(s.entries == 3);
  CHECK(s.metadataKeys == std::set<std::string>{"detector", "energy", "layer", "saturated", "unit:energy"});

  // Entries ordered by integer event number: 0, 1, 2 with 4, 3, 2 hits.
  CHECK(data.Size<dt::PointElement>({"hits", ""}) == 3);
  const auto e0 = data.Entry<dt::PointElement>({"hits", ""}, 0);
  const auto e1 = data.Entry<dt::PointElement>({"hits", ""}, 1);
  const auto e2 = data.Entry<dt::PointElement>({"hits", ""}, 2);
  REQUIRE(e0);
  REQUIRE(e1);
  REQUIRE(e2);
  CHECK(e0->size() == 4);
  CHECK(e1->size() == 3);
  CHECK(e2->size() == 2);

  const auto& h = (*e0)[0];  // 0,1000.000,0.000,-50.0,0.50,1,ECalBarrel,false
  CHECK(h.geometry.position[0] == Approx(1000.0));
  CHECK(h.geometry.position[1] == Approx(0.0).margin(1e-9));
  CHECK(h.geometry.position[2] == Approx(-50.0));
  REQUIRE(h.meta.Find("layer"));
  CHECK(std::holds_alternative<std::int64_t>(*h.meta.Find("layer")));
  CHECK(std::holds_alternative<double>(*h.meta.Find("energy")));
  CHECK(h.meta.Number("energy") == Approx(0.5));
  CHECK(h.meta.Text("unit:energy") == "GeV");
  CHECK(h.meta.Text("detector") == "ECalBarrel");
  CHECK(h.meta.Flag("saturated") == false);
  CHECK((*e0)[3].meta.Flag("saturated") == true);
  CHECK_FALSE(h.meta.Has("event"));  // entry column is not metadata
  CHECK_FALSE(h.meta.Has("x"));

  // Empty cell: no key
  CHECK_FALSE((*e1)[2].meta.Has("energy"));
  CHECK((*e1)[2].meta.Has("layer"));
}

TEST_CASE("CSV without entry column is a single entry", "[adapter][csv]") {
  dt::SessionData data;
  const auto report = Csv()->Open(data, Fixture("hits.csv"), {{"name", "all"}});
  REQUIRE(report.Ok());
  CHECK(report.registered[0].entries == 1);
  CHECK(data.Entry<dt::PointElement>({"all", ""}, 0)->size() == 9);
  CHECK(report.registered[0].metadataKeys.count("event") == 1);  // an ordinary column now
}

TEST_CASE("CSV lines: grouped and ordered vertices", "[adapter][csv]") {
  dt::SessionData data;
  const auto report = Csv()->Open(data, Fixture("tracks.csv"),
      {{"delimiter", ";"}, {"kind", "line"}, {"group", "track"}, {"order", "step"}, {"entry", "event"}});
  INFO(report.ToString());
  REQUIRE(report.Ok());
  const auto& s = report.registered[0];
  CHECK(s.kind == dt::ElementKind::Line);
  CHECK(s.entries == 2);
  CHECK(s.metadataKeys == std::set<std::string>{"charge", "track"});
  const auto e1 = data.Entry<dt::LineElement>({"tracks", ""}, 1);
  REQUIRE(e1);
  REQUIRE(e1->size() == 3);
  const auto& t = (*e1)[1];
  REQUIRE(t.geometry.vertices.size() == 4);
  CHECK(t.geometry.vertices[0][2] == Approx(0.0));   // step 0 first
  CHECK(t.geometry.vertices[3][2] == Approx(30.0));  // step 3 last
  CHECK(t.meta.Number("charge") == -1);
  CHECK(t.meta.Number("track") == 1);

  CHECK_THROWS_AS(Csv()->Open(data, Fixture("tracks.csv"), {{"kind", "line"}}), std::invalid_argument);
}

TEST_CASE("CSV malformed rows are reported with line numbers", "[adapter][csv]") {
  dt::SessionData data;
  const auto report = Csv()->Open(data, Fixture("bad.csv"), {});
  REQUIRE(report.Ok());
  CHECK(report.registered[0].entries == 1);
  CHECK(data.Entry<dt::PointElement>({"bad", ""}, 0)->size() == 4);
  REQUIRE(report.skipped.size() == 2);
  CHECK(report.skipped[0].name == "line 4");
  CHECK(report.skipped[0].reason.find("3 fields") != std::string::npos);
  CHECK(report.skipped[1].name == "line 6");
  CHECK(report.skipped[1].reason.find("'four'") != std::string::npos);
  const auto text = report.ToString();
  CHECK(text.find("- line 4:") < text.find("- line 6:"));

  dt::SessionData strictData;
  const auto strict = Csv()->Open(strictData, Fixture("bad.csv"), {{"strict", "true"}});
  CHECK_FALSE(strict.Ok());
  CHECK(strictData.List().empty());
  CHECK(strict.skipped.size() == 3);
}

TEST_CASE("CSV units, tabs and quoted fields", "[adapter][csv]") {
  dt::SessionData data;
  const auto report = Csv()->Open(data, Fixture("units.csv"), {{"delimiter", "tab"}});
  INFO(report.ToString());
  REQUIRE(report.Ok());
  CHECK(report.registered[0].description.find("converted from cm to mm") != std::string::npos);
  const auto e = data.Entry<dt::PointElement>({"units", ""}, 0);
  REQUIRE(e->size() == 2);
  CHECK((*e)[0].geometry.position[0] == Approx(10.0));
  CHECK((*e)[0].geometry.position[2] == Approx(30.0));
  CHECK((*e)[1].geometry.position[0] == Approx(-15.0));
  CHECK((*e)[0].meta.Text("label") == "a\tb");
}

TEST_CASE("CSV bad sources and options never abort", "[adapter][csv]") {
  dt::SessionData data;
  const auto missing = Csv()->Open(data, Fixture("does-not-exist.csv"), {});
  CHECK_FALSE(missing.Ok());
  REQUIRE(missing.skipped.size() == 1);
  CHECK(missing.skipped[0].reason == "cannot open file");

  const auto noColumn = Csv()->Open(data, Fixture("hits.csv"), {{"x", "px"}});
  CHECK_FALSE(noColumn.Ok());
  CHECK(noColumn.skipped[0].reason.find("'px'") != std::string::npos);

  const auto someColumns = Csv()->Open(data, Fixture("hits.csv"), {{"columns", "energy, nope"}, {"name", "sel"}});
  REQUIRE(someColumns.Ok());
  CHECK(someColumns.registered[0].metadataKeys == std::set<std::string>{"energy", "unit:energy"});
  REQUIRE(someColumns.skipped.size() == 1);
  CHECK(someColumns.skipped[0].name == "column nope");

  CHECK_THROWS_AS(Csv()->Open(data, Fixture("hits.csv"), {{"bogus", "1"}}), std::invalid_argument);
  CHECK_THROWS_AS(Csv()->Open(data, Fixture("hits.csv"), {{"delimiter", "ab"}}), std::invalid_argument);
}

TEST_CASE("CSV re-open returns the same series, entries load lazily", "[adapter][csv]") {
  const auto dir = fs::path(FCCVIS_TEST_BINARY_DIR) / "csv-lazy";
  fs::create_directories(dir);
  const auto copy = dir / "hits.csv";
  fs::copy_file(Fixture("hits.csv"), copy, fs::copy_options::overwrite_existing);

  dt::SessionData data;
  const auto first = Csv()->Open(data, copy.string(), {{"entry", "event"}});
  REQUIRE(first.Ok());
  const auto again = Csv()->Open(data, copy.string(), {{"entry", "event"}});
  REQUIRE(again.Ok());
  CHECK(again.registered[0].existing);
  CHECK(again.registered[0].ref.name == first.registered[0].ref.name);
  CHECK(again.registered[0].ref.source == first.registered[0].ref.source);
  CHECK(data.List().size() == 1);

  // Rewrite the rows of event 2 in place (same byte length) after Open:
  // entry 2 sees the new values, entry 0 is unaffected, i.e. loading an entry
  // reads only that entry's rows, at load time.
  std::string text = ReadFile(copy.string());
  const auto pos = text.find("2,809.017");
  REQUIRE(pos != std::string::npos);
  text.replace(pos, 9, "2,111.111");
  std::ofstream(copy, std::ios::binary) << text;
  const auto e2 = data.Entry<dt::PointElement>({"hits", ""}, 2);
  REQUIRE(e2);
  CHECK((*e2)[0].geometry.position[0] == Approx(111.111));
  CHECK((*data.Entry<dt::PointElement>({"hits", ""}, 0))[0].geometry.position[0] == Approx(1000.0));
}
