// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Documentation example: filters by energy, sub-detector and shape
// (docs/source/guides/filters.md). Every C++ selector is checked against the
// shell expression of the same recipe on the example event: both must select
// the same hits.
#include "AdapterRegistry.h"
#include "Data.h"
#include "fccvis/api/Expression.h"
#include "fccvis/stdlib/DetectorFilters.h"
#include "fccvis/stdlib/Filters.h"
#include "fccvis/stdlib/ShapeFilters.h"
#include "fccvis/stdlib/Shapes.h"

#include <cstdio>
#include <span>
#include <string>
#include <vector>

using namespace fccvis;

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  data::SessionData store;
  auto csv = adapters::Registry::Global().Create("csv");
  if (!csv->Open(store, dir + "/hits.csv", {{"entry", "event"}}).Ok()) return 1;
  const auto hits = store.Entry<data::PointElement>({"hits", ""}, 0);
  if (!hits) return 1;

  using namespace fccvis::stdlib;
  // [recipes]
  // Energy: a threshold, and a band (inclusive bounds).
  filter::Filter<data::PointElement> energetic = EnergyAbove(2.0);
  filter::Filter<data::PointElement> band = Between("energy", 0.5, 1.0);

  // Sub-detector and layer, by the metadata the adapter provides.
  filter::Filter<data::PointElement> ecal =
      (Attr<std::string>("subdetector") == "ECalBarrel") & OneOf("layer", std::vector<double>{1, 2});
  filter::Filter<data::PointElement> hcal = Matches("subdetector", "HCal.*");

  // Shapes, in mm: hits in a barrel shell, and hits beyond |z| = 1 m.
  auto shell = std::make_shared<Cylinder>(1500, 1800, 2200);
  filter::Filter<data::PointElement> barrel = Inside(shell);
  auto central = std::make_shared<Cylinder>(0, 2500, 1000);
  filter::Filter<data::PointElement> forward = Outside(central) & EnergyAbove(0.2);

  // A placed shape: a box (half-lengths 0.6 x 0.4 x 2 m) centred at y = 1.75 m.
  geometry::Shape cube(std::make_shared<Box>(600, 400, 2000),
                       geometry::Transform({0, 1750, 0}, {1, 0, 0, 0, 1, 0, 0, 0, 1}));
  filter::Filter<data::PointElement> box = Inside(cube);
  // [recipes]

  struct Recipe {
    const char* name;
    filter::Filter<data::PointElement> cpp;
    const char* shell;
  };
  const std::vector<Recipe> recipes = {
      {"energetic", energetic, "energy > 2"},
      {"band", band, "between(energy, 0.5, 1.0)"},
      {"ecal", ecal, "subdetector == \"ECalBarrel\" and layer in [1, 2]"},
      {"hcal", hcal, "subdetector ~ \"HCal.*\""},
      {"barrel", barrel, "inside(cylinder(1500, 1800, 2200))"},
      {"forward", forward, "outside(cylinder(0, 2500, 1000)) and energy > 0.2"},
      {"box", box, "inside(translate(box(600, 400, 2000), 0, 1750, 0))"},
  };
  const std::span<const data::PointElement> event(*hits);
  int failures = 0;
  for (const auto& r : recipes) {
    const auto viaCpp = r.cpp.Apply(event).size();
    const filter::Filter<data::PointElement> parsed = api::ParseFilter(r.shell);
    const auto viaShell = parsed.Apply(event).size();
    std::printf("%-10s %4zu of %zu hits  (%s)\n", r.name, viaCpp, event.size(), r.shell);
    if (viaCpp != viaShell || viaCpp == 0) ++failures;
  }
  return failures == 0 ? 0 : 1;
}
