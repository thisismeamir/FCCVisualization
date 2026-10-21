// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T3.6: Phoenix adapter on a large real file named by FCCVIS_TEST_PHOENIX_FILE
// (for example Phoenix's atlas.json); skips itself when unset.
#include <catch2/catch_test_macros.hpp>

#include "AdapterRegistry.h"
#include "Data.h"

#include <cstdlib>

namespace ad = fccvis::adapters;
namespace dt = fccvis::data;

TEST_CASE("Phoenix adapter reads a large real file", "[adapter][phoenix-json][data]") {
  const char* path = std::getenv("FCCVIS_TEST_PHOENIX_FILE");
  if (!path || !*path) SKIP("FCCVIS_TEST_PHOENIX_FILE is not set");
  dt::SessionData data;
  const auto r = ad::Registry::Global().Create("phoenix")->Open(data, path, {});
  INFO(r.ToString());
  REQUIRE(r.Ok());
  std::size_t elements = 0;
  for (const auto& s : r.registered) {
    for (std::size_t e = 0; e < s.entries; ++e) {
      if (s.kind == dt::ElementKind::Point) elements += data.Entry<dt::PointElement>(s.ref, e)->size();
      else elements += data.Entry<dt::LineElement>(s.ref, e)->size();
    }
  }
  CHECK(elements > 0);
}
