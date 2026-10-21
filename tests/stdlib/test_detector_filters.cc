// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T2.4: detector sub-part filters.
#include <catch2/catch_test_macros.hpp>

#include "fccvis/stdlib/DetectorFilters.h"
#include "fccvis/stdlib/ShapeFilters.h"

#include <regex>

using namespace fccvis;
using namespace fccvis::stdlib;
using SE = data::SurfaceElement;
using PE = data::PointElement;

namespace {
PE Cell(const char* sub, std::int64_t layer, const char* volume, const char* material) {
  PE e;
  e.meta.Set(keys::kSubdetector, std::string(sub));
  e.meta.Set(keys::kLayer, layer);
  e.meta.Set(keys::kVolume, std::string(volume));
  e.meta.Set(keys::kMaterial, std::string(material));
  e.meta.Set("energy", 2.0);
  return e;
}
}  // namespace

TEST_CASE("Detector sub-part filters read the documented keys", "[stdlib][filters][detector]") {
  const PE ecal3 = Cell("ECalBarrel", 3, "ECalBarrel/stave_1/layer_3", "PbWO4");
  const PE hcal7 = Cell("HCalBarrel", 7, "HCalBarrel/module_2/layer_7", "Steel235");
  PE bare;

  CHECK(Subdetector("ECalBarrel").DoesPass(ecal3));
  CHECK_FALSE(Subdetector("ECal").DoesPass(ecal3));  // exact match
  CHECK_FALSE(Subdetector("ECalBarrel").DoesPass(bare));

  CHECK(Layer(3).DoesPass(ecal3));
  CHECK_FALSE(Layer(3).DoesPass(hcal7));
  CHECK(LayerRange(2, 7).DoesPass(ecal3));
  CHECK(LayerRange(2, 7).DoesPass(hcal7));
  CHECK_FALSE(LayerRange(4, 6).DoesPass(ecal3));
  CHECK_FALSE(LayerRange(0, 100).DoesPass(bare));

  CHECK(Volume("ECalBarrel/.*").DoesPass(ecal3));
  CHECK(Volume(".*layer_7").DoesPass(hcal7));
  CHECK_FALSE(Volume("layer_7").DoesPass(hcal7));
  CHECK_THROWS_AS(Volume("[unclosed"), std::regex_error);

  CHECK(Material("PbWO4").DoesPass(ecal3));
  CHECK_FALSE(Material("PbWO4").DoesPass(hcal7));

  // custom attribute names
  PE custom;
  custom.meta.Set("det", std::string("Muon"));
  CHECK(Subdetector("Muon", "det").DoesPass(custom));
}

TEST_CASE("Detector filters compose with energy and shape filters", "[stdlib][filters][detector]") {
  PE a = Cell("ECalBarrel", 3, "v", "m");
  a.geometry.position = {0, 0, 0};
  PE b = Cell("ECalBarrel", 9, "v", "m");
  b.geometry.position = {0, 0, 0};
  filter::Filter<PE> f = Subdetector("ECalBarrel") & LayerRange(1, 5) & EnergyAbove(1) &
                         Inside(std::make_shared<Sphere>(1.0));
  CHECK(f.DoesPass(a));
  CHECK_FALSE(f.DoesPass(b));
}
