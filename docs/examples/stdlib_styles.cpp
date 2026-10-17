// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Documentation example: palettes, attribute-driven styles, presets
// (docs/source/stdlib/styles.md).
#include "fccvis/stdlib/Styles.h"

#include <cstdio>

using namespace fccvis;

int main() {
  using namespace fccvis::stdlib;

  // [rules]
  // Hits coloured by energy over 0-50 GeV (16 bins of the monochrome palette)
  // and sized by energy (8 bins, 1.5-6 px), then accent the very energetic ones.
  style::StyleSheet<data::PointElement> hits;
  hits.base.size = 2.0f;
  AddRules(hits, ColorByAttribute<data::PointElement>("energy", Monochrome(), {0, 50}));
  AddRules(hits, SizeByAttribute<data::PointElement>("energy", 1.5f, 6.0f, {0, 50}));
  hits.rules.push_back(Highlight<data::PointElement>(EnergyAbove(45)));

  // A backend completes the cascaded style with its own defaults.
  style::PointStyle backendDefaults;
  backendDefaults.marker = style::MarkerShape::Circle;
  data::PointElement hit;
  hit.meta.Set("energy", 12.0);
  style::PointStyle resolved = hits.Resolve(hit).CompleteBy(backendDefaults);
  // [rules]

  // [presets]
  auto geometry = DetectorOutline();       // StyleSheet<SurfaceElement>
  auto tracks = TracksByCharge();          // StyleSheet<LineElement>, attribute "charge"
  auto context = Ghosted<data::LineElement>();
  auto paper = Publication();              // EnvironmentStyle
  // [presets]

  std::printf("size=%.2f marker=%d color=%d background=%d\n", resolved.size.value_or(0),
              static_cast<int>(*resolved.marker), resolved.common.color.has_value(),
              paper.background.has_value());
  (void)geometry;
  (void)tracks;
  (void)context;
  return resolved.common.color && resolved.size && paper.background ? 0 : 1;
}
