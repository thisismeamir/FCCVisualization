// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Documentation example: fluent builders and scripts (docs/source/api/builders.md).
#include "fccvis/api/Builders.h"
#include "fccvis/stdlib/Filters.h"
#include "fccvis/stdlib/Styles.h"

#include <cstdio>

using namespace fccvis;

int main() {
  session::Session session("example");
  std::vector<data::PointElement> hits(4);
  for (std::size_t i = 0; i < hits.size(); ++i) {
    hits[i].geometry.position = {100.0 * i, 0, 0};
    hits[i].meta.Set("energy", 0.4 * static_cast<double>(i));
  }
  session.Data().Add<data::PointElement>("calo-hits", "example",
      std::make_unique<data::InMemorySeries<data::PointElement>>(std::vector<std::vector<data::PointElement>>{hits}));
  session.DataChanged();

  // [builders]
  style::PointStyle accent;
  accent.size = 5.0f;

  api::Script script = api::Scene("xy").Create()
                           .Direction({0, 0, -1})
                           .Layer<data::Point>("hits", "calo-hits")
                           .Where(stdlib::EnergyAbove(0.5))
                           .Style(accent)
                           .Rules(stdlib::ColorByAttribute<data::PointElement>("energy", stdlib::Monochrome(), {0, 2}));
  script.Append(api::Scene("rz").Create().Direction({1, 0, 0}));
  script.Append(api::Layout(api::Split({{api::Pane("xy"), "Transverse"}, {api::Pane("rz"), "Side"}})));

  for (const auto& line : script.Describe()) std::printf("%s\n", line.c_str());
  changes::Result result = script.Run(session);  // one batch through Session::Resolve
  // [builders]

  std::printf("applied %zu of %zu changes\n", result.applied, script.Size());
  return result.ok ? 0 : 1;
}
