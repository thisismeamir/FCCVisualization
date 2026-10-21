// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T5.3: filter, style and layout expressions.
#include <catch2/catch_test_macros.hpp>

#include "TestSupport.h"
#include "fccvis/api/Expression.h"

namespace api = fccvis::api;
namespace dt = fccvis::data;
namespace st = fccvis::style;
using namespace fccvis::test;

namespace {

dt::PointElement Hit(double x, double energy, const std::string& det, std::int64_t layer) {
  dt::PointElement p;
  p.geometry.position = {x, 0, 0};
  p.meta.Set("energy", energy);
  p.meta.Set("subdetector", det);
  p.meta.Set("layer", layer);
  p.meta.Set("saturated", layer > 3);
  return p;
}

bool Pass(const std::string& expr, const dt::PointElement& p, std::size_t index = 0) {
  return api::ParseFilter(expr).For<dt::PointElement>().DoesPass(p, index);
}

std::size_t ErrorColumn(const std::string& expr) {
  try {
    (void)api::ParseFilter(expr);
  } catch (const api::ParseError& e) {
    return e.Column();
  }
  return 0;
}

}  // namespace

TEST_CASE("Filter expressions select like the stdlib filters", "[api][expression]") {
  const auto ecal = Hit(10, 2.0, "ECal", 2);
  const auto hcal = Hit(500, 0.3, "HCal", 5);
  CHECK(Pass("energy > 0.5", ecal));
  CHECK_FALSE(Pass("energy > 0.5", hcal));
  CHECK(Pass("energy > 0.5 and subdetector == \"ECal\" or layer >= 5", hcal));
  CHECK_FALSE(Pass("energy > 0.5 and (subdetector == \"ECal\" or layer >= 5)", hcal));
  CHECK(Pass("not subdetector == \"ECal\"", hcal));
  CHECK(Pass("layer in [1, 2, 3]", ecal));
  CHECK(Pass("subdetector in [\"HCal\", \"Muon\"]", hcal));
  CHECK(Pass("subdetector ~ \"[EH]Cal\"", hcal));
  CHECK_FALSE(Pass("subdetector ~ \"Cal\"", hcal));  // full match
  CHECK(Pass("saturated == true", hcal));
  CHECK(Pass("saturated != true", ecal));
  CHECK(Pass("exists(energy) and not exists(charge)", ecal));
  CHECK_FALSE(Pass("charge != 1", ecal));  // missing attribute fails every comparison
  CHECK(Pass("between(energy, 1, 3)", ecal));
  CHECK(Pass("inside(box(50, 50, 50))", ecal));
  CHECK_FALSE(Pass("inside(box(50, 50, 50))", hcal));
  CHECK(Pass("outside(sphere(100))", hcal));
  CHECK(Pass("within(5, translate(box(1, 1, 1), 14, 0, 0))", ecal));
  CHECK(Pass("inside(cylinder(0, 600, 10)) and inside(cone(0, 600, 0, 600, 10))", hcal));
  CHECK(Pass("first(2)", ecal, 1));
  CHECK_FALSE(Pass("first(2)", ecal, 2));
  CHECK(Pass("index(3)", ecal, 3));
  CHECK(Pass("all()", ecal));
  CHECK_FALSE(Pass("none()", ecal));
  CHECK(Pass("energy>=-1e3", ecal));
  CHECK(api::FilterAttributes("energy > 1 and layer in [1] or exists(x)") ==
        std::vector<std::string>{"energy", "layer", "x"});
}

TEST_CASE("Filter expression errors point at the column", "[api][expression]") {
  CHECK(ErrorColumn("energy >> 1") == 9);
  CHECK(ErrorColumn("energy = 1") == 8);
  CHECK(ErrorColumn("energy > 1 and") == 15);
  CHECK(ErrorColumn("(energy > 1") == 12);
  CHECK(ErrorColumn("energy") == 7);
  CHECK(ErrorColumn("foo(1)") == 1);
  CHECK(ErrorColumn("inside(blob(1))") == 8);
  CHECK(ErrorColumn("inside(box(-1, 1, 1))") == 8);
  CHECK(ErrorColumn("x ~ \"(\"") == 5);
  CHECK(ErrorColumn("name == \"open") == 9);
  CHECK(ErrorColumn("") == 1);
  CHECK(ErrorColumn("saturated < true") == 11);
  CHECK(ErrorColumn("layer in [1, \"a\"]") == 1);
}

TEST_CASE("Style assignments and colours", "[api][expression]") {
  const auto p = api::ParsePointStyle("color=#ff0000 size=4 marker=square opacity=0.5 visible=true");
  REQUIRE(p.common.color);
  CHECK(p.common.color->r == 255);
  CHECK(p.size == 4.0f);
  CHECK(p.marker == st::MarkerShape::Square);
  CHECK(p.common.opacity == 0.5f);
  const auto l = api::ParseLineStyle("width=2 dash=dashdot color=accent");
  CHECK(l.dash == st::DashPattern::DashDot);
  CHECK(l.common.color->b == 0xFF);
  const auto s = api::ParseSurfaceStyle("edgecolor=#123 wireframe=on edgewidth=0");
  CHECK(s.edgeColor->r == 0x11);
  CHECK(s.wireframe == true);
  const auto e = api::ParseEnvironment("background=white axes=true grid=false");
  CHECK(e.axes == true);
  CHECK(api::ParseColor("#11223344").a == 0x44);
  CHECK_THROWS_AS(api::ParsePointStyle("width=2"), api::ParseError);
  CHECK_THROWS_AS(api::ParsePointStyle("size=-1"), api::ParseError);
  CHECK_THROWS_AS(api::ParsePointStyle("marker=star"), api::ParseError);
  CHECK_THROWS_AS(api::ParseLineStyle("color=#12"), api::ParseError);
  CHECK_THROWS_AS(api::ParseLineStyle("color"), api::ParseError);
  try {
    (void)api::ParsePointStyle("size=1 bogus=2");
    FAIL("no error");
  } catch (const api::ParseError& err) {
    CHECK(err.Column() == 8);
    CHECK(std::string(err.what()).find("color, marker, opacity, size, visible") != std::string::npos);
  }
  CHECK(api::ParseAssignments("a=1 b=\"x y\"") == std::map<std::string, std::string>{{"a", "1"}, {"b", "x y"}});
}

TEST_CASE("Layout expressions round-trip", "[api][expression]") {
  const auto n = api::ParseLayout("split(xy as \"Front view\" *2, vsplit(rz, tabs(a, b as B)))");
  CHECK(Describe(n) == "S(xy,S(rz,T(a,b)))");
  CHECK(api::FormatLayout(n) == "split(xy as \"Front view\" *2, vsplit(rz, tabs(a, b as \"B\")))");
  CHECK(api::FormatLayout(api::ParseLayout(api::FormatLayout(n))) == api::FormatLayout(n));
  CHECK(Describe(api::ParseLayout("xy")) == "xy");
  CHECK(api::FormatLayout(api::ParseLayout("\"my scene\"")) == "\"my scene\"");
  CHECK_THROWS_AS(api::ParseLayout("split(a,"), api::ParseError);
  CHECK_THROWS_AS(api::ParseLayout("tabs(a *2)"), api::ParseError);
  CHECK_THROWS_AS(api::ParseLayout(""), api::ParseError);
  CHECK(api::SplitWords("a \"b \\\"c\\\"\" d") == std::vector<std::string>{"a", "b \"c\"", "d"});
  CHECK(api::QuoteWord("x y") == "\"x y\"");
}
