// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Documentation example: load a CSV file with the csv adapter
// (docs/source/adapters/csv.md).
#include "AdapterRegistry.h"
#include "Data.h"

#include <cstdio>
#include <fstream>

using namespace fccvis;

int main() {
  {
    std::ofstream f("example_hits.csv");
    f << "event,x,y,z,energy [GeV],detector\n"
         "0,1000,0,0,1.5,ECal\n"
         "0,0,1000,0,2.5,HCal\n"
         "1,-1000,0,0,0.7,ECal\n";
  }

  // [open]
  data::SessionData data;
  auto csv = adapters::Registry::Global().Create("csv");
  adapters::LoadReport report = csv->Open(data, "example_hits.csv", {{"entry", "event"}, {"name", "hits"}});
  std::printf("%s", report.ToString().c_str());

  // Entry 0 is read from the file now, not at Open.
  auto event0 = data.Entry<data::PointElement>({"hits", ""}, 0);
  double energy = event0->at(1).meta.Number("energy").value_or(0);  // 2.5; "unit:energy" is "GeV"
  // [open]

  std::printf("entries=%zu hits in event 0=%zu energy=%.1f\n", data.Size<data::PointElement>({"hits", ""}),
              event0->size(), energy);
  return report.Ok() && event0->size() == 2 && energy == 2.5 ? 0 : 1;
}
