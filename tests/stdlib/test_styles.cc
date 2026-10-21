// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T2.5: palettes, attribute-driven styles, presets.
#include <catch2/catch_test_macros.hpp>

#include "fccvis/stdlib/Styles.h"

#include <stdexcept>

using namespace fccvis;
using namespace fccvis::stdlib;
using PE = data::PointElement;
using LE = data::LineElement;
using SE = data::SurfaceElement;
using style::Color;

namespace {
bool Same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }
PE Energy(double e) {
  PE x;
  x.meta.Set("energy", e);
  return x;
}
LE Charge(std::int64_t q) {
  LE l;
  l.meta.Set("charge", q);
  return l;
}
}  // namespace

TEST_CASE("Palettes", "[stdlib][styles]") {
  const auto accent = Color::FromHex(kAccent);
  const Palette mono = Monochrome();
  CHECK(Same(mono.At(1.0), accent));
  CHECK(Same(mono.At(5.0), accent));  // clamped
  CHECK(mono.At(0.0).r > accent.r);   // lighter tint at the low end

  const Palette seq = Sequential(Color::FromHex(0x000000), Color::FromHex(0xFFFFFF));
  CHECK(Same(seq.At(0.5), Color{128, 128, 128, 255}));
  const Palette div = Diverging();
  CHECK(Same(div.At(0.5), Color::FromHex(0xF4F4F4)));
  CHECK(Same(div.At(0.0), Color::FromHex(0x0F62FE)));

  const Palette cat = Categorical(3);
  CHECK(cat.Size() == 3);
  CHECK(Same(cat.Index(0), accent));
  CHECK(Same(cat.Index(3), cat.Index(0)));  // cycles
  CHECK(Categorical(20).Size() == 20);
  CHECK_THROWS_AS(Palette({}), std::invalid_argument);
}

TEST_CASE("ColorByAttribute bins every element with the attribute exactly once", "[stdlib][styles]") {
  const Palette pal = Sequential(Color::FromHex(0x000000), Color::FromHex(0xFF0000));
  style::StyleSheet<PE> sheet;
  AddRules(sheet, ColorByAttribute<PE>("energy", pal, {0, 10}, 5));
  REQUIRE(sheet.rules.size() == 5);
  // in-range values: bin centre colour
  CHECK(Same(*sheet.Resolve(Energy(1.0)).common.color, pal.At(0.1)));
  CHECK(Same(*sheet.Resolve(Energy(9.0)).common.color, pal.At(0.9)));
  // out of range: clamped to the first/last bin
  CHECK(Same(*sheet.Resolve(Energy(-3)).common.color, pal.At(0.1)));
  CHECK(Same(*sheet.Resolve(Energy(42)).common.color, pal.At(0.9)));
  // exactly one rule per value
  for (double e : {-1.0, 0.0, 1.99, 2.0, 5.5, 10.0, 11.0}) {
    int n = 0;
    for (const auto& r : sheet.rules) n += r.when.DoesPass(Energy(e));
    CHECK(n == 1);
  }
  // missing attribute: no rule applies, style unchanged
  CHECK_FALSE(sheet.Resolve(PE{}).common.color.has_value());
  CHECK_THROWS_AS(ColorByAttribute<PE>("energy", pal, {1, 1}), std::invalid_argument);
  CHECK_THROWS_AS(ColorByAttribute<PE>("energy", pal, {0, 1}, 0), std::invalid_argument);
}

TEST_CASE("SizeByAttribute maps to the size field of each kind", "[stdlib][styles]") {
  style::StyleSheet<PE> points;
  AddRules(points, SizeByAttribute<PE>("energy", 1.f, 5.f, {0, 4}, 5));
  CHECK(points.Resolve(Energy(0.1)).size == 1.f);
  CHECK(points.Resolve(Energy(3.9)).size == 5.f);
  style::StyleSheet<LE> lines;
  AddRules(lines, SizeByAttribute<LE>("charge", 1.f, 3.f, {-1, 1}, 2));
  CHECK(lines.Resolve(Charge(1)).width == 3.f);
  style::StyleSheet<SE> surfaces;
  SE s{data::Surface{geometry::Shape(nullptr)}, {}};
  s.meta.Set("layer", 3.0);
  AddRules(surfaces, SizeByAttribute<SE>("layer", 0.5f, 2.f, {0, 4}, 4));
  CHECK(surfaces.Resolve(s).edgeWidth == 2.f);
}

TEST_CASE("Cascade order, CompleteBy and rule matching", "[stdlib][styles]") {
  style::StyleSheet<PE> sheet;
  sheet.base.size = 2.f;
  AddRules(sheet, ColorByAttribute<PE>("energy", Monochrome(), {0, 10}, 2));
  sheet.rules.push_back(Highlight<PE>(EnergyAbove(8)));
  style::PointStyle red;
  red.common.color = Color::FromHex(0xFF0000);
  sheet.Add(EnergyAbove(9.5), red);  // later wins

  const auto accent = Color::FromHex(kAccent);
  CHECK(Same(*sheet.Resolve(Energy(9)).common.color, accent));
  CHECK(Same(*sheet.Resolve(Energy(9.9)).common.color, Color::FromHex(0xFF0000)));
  CHECK(Same(*sheet.Resolve(Energy(1)).common.color, Monochrome().At(0.25)));

  style::PointStyle defaults;
  defaults.size = 7.f;
  defaults.marker = style::MarkerShape::Square;
  defaults.common.color = Color::FromHex(0x00FF00);
  defaults.common.opacity = 0.5f;
  const auto resolved = sheet.Resolve(Energy(9)).CompleteBy(defaults);
  CHECK(resolved.size == 2.f);                          // sheet base wins over defaults
  CHECK(resolved.marker == style::MarkerShape::Square);  // filled from defaults
  CHECK(resolved.common.opacity == 1.0f);                // set by Highlight
  const auto plain = sheet.Resolve(PE{}).CompleteBy(defaults);
  CHECK(Same(*plain.common.color, Color::FromHex(0x00FF00)));  // no rule matched
}

TEST_CASE("Presets", "[stdlib][styles]") {
  const auto outline = DetectorOutline();
  CHECK(outline.base.common.opacity < 0.5f);
  CHECK(outline.base.edgeWidth.has_value());

  const auto hits = HitsByEnergy({0, 10});
  CHECK(hits.Resolve(Energy(9.9)).size > hits.Resolve(Energy(0.1)).size);

  const auto tracks = TracksByCharge();
  CHECK(Same(*tracks.Resolve(Charge(1)).common.color, Color::FromHex(kAccent)));
  CHECK(Same(*tracks.Resolve(Charge(-1)).common.color, Color::FromHex(0x1192E8)));
  CHECK(tracks.Resolve(Charge(0)).dash == style::DashPattern::Dashed);

  const auto ghost = Ghosted<LE>();
  CHECK(*ghost.Resolve(Charge(1)).common.opacity < 0.5f);

  const auto pub = Publication();
  CHECK(Same(*pub.background, Color::FromHex(0xFFFFFF)));
  CHECK(*pub.axes);
  CHECK(DefaultEnvironment().grid == true);
}
