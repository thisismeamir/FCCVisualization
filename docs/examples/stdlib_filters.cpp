// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Documentation example: standard filters (docs/source/stdlib/filters.md).
#include "Elements.h"
#include "Filter.h"
#include "fccvis/stdlib/DetectorFilters.h"
#include "fccvis/stdlib/Filters.h"
#include "fccvis/stdlib/ShapeFilters.h"

#include <cstdio>
#include <span>
#include <string>
#include <vector>

using namespace fccvis;

namespace {
data::PointElement Hit(double energy, const char* subdetector, std::int64_t layer) {
  data::PointElement e;
  e.meta.Set("energy", energy);
  e.meta.Set("subdetector", std::string(subdetector));
  e.meta.Set("layer", layer);
  return e;
}
}  // namespace

int main() {
  using namespace fccvis::stdlib;
  const std::vector<data::PointElement> hits{Hit(0.3, "ECal", 1), Hit(2.0, "ECal", 5),
                                             Hit(7.5, "HCal", 2), data::PointElement{}};

  // [attributes]
  // Attribute comparisons; a missing attribute never passes.
  auto energetic = EnergyAbove(1.0);                            // energy > 1
  auto ecal = Attr<std::string>("subdetector") == "ECal";
  auto deep = Between("layer", 4, 10);                          // inclusive
  // Compose with &, |, ! and use the result as a Filter<X> of any element kind.
  filter::Filter<data::PointElement> f = energetic & (ecal | !deep);
  auto selected = f.Apply(std::span<const data::PointElement>(hits));
  // [attributes]

  // [selection]
  filter::Filter<data::PointElement> firstTwo = FirstN(2);      // positional
  auto head = firstTwo.Apply(std::span<const data::PointElement>(hits));
  // [selection]

  // [shapes]
  // Hits inside a 2 m radius barrel region, or within 5 mm of it.
  auto region = std::make_shared<Cylinder>(0, 2000, 3000);
  filter::Filter<data::PointElement> central = Within(5.0, region) & EnergyAbove(0.1);
  // Tracks with every vertex in a box placed at z = 1 m.
  fccvis::geometry::Shape box(std::make_shared<Box>(500, 500, 500),
                              fccvis::geometry::Transform({0, 0, 1000}, {1, 0, 0, 0, 1, 0, 0, 0, 1}));
  filter::Filter<data::LineElement> contained = Inside(box, LineTest::AllVertices);
  // [shapes]

  // [detector]
  // Inner layers of the ECal barrel volumes, by the metadata adapters set.
  auto innerEcal = Subdetector("ECalBarrel") & LayerRange(1, 5) & Volume("ECalBarrel/.*");
  // [detector]
  data::PointElement cell;
  cell.meta.Set(keys::kSubdetector, std::string("ECalBarrel"));
  cell.meta.Set(keys::kLayer, std::int64_t{2});
  cell.meta.Set(keys::kVolume, std::string("ECalBarrel/stave_3"));
  if (!innerEcal.DoesPass(cell)) return 1;

  data::LineElement track;
  track.geometry.vertices = {{0, 0, 900}, {100, 0, 1200}};
  std::printf("selected=%zu head=%zu central=%d contained=%d\n", selected.size(), head.size(),
              central.DoesPass(hits[1]), contained.DoesPass(track));
  return selected.size() == 2 && head.size() == 2 && central.DoesPass(hits[1]) &&
                 contained.DoesPass(track)
             ? 0
             : 1;
}
