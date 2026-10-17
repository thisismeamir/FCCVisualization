// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Documentation example: standard shapes (docs/source/stdlib/shapes.md).
#include "fccvis/stdlib/Shapes.h"

#include <cstdio>
#include <memory>
#include <numbers>

int main() {
  using namespace fccvis::stdlib;
  constexpr double pi = std::numbers::pi;

  // [shapes]
  // A barrel: hollow cylinder, inner radius 1.2 m, outer 1.8 m, length 4 m.
  auto barrel = std::make_shared<Cylinder>(1200, 1800, 2000);
  // One quarter of it, as a tube segment starting at phi = 0.
  auto quarter = std::make_shared<TubeSegment>(1200, 1800, 2000, 0.0, pi / 2);
  // An end-cap disc placed at z = +2.5 m.
  auto cap = std::make_shared<Transformed>(std::make_shared<Cylinder>(300, 1800, 100),
                                           fccvis::geometry::Transform({0, 0, 2500}, {1, 0, 0, 0, 1, 0, 0, 0, 1}));
  // Booleans: the barrel without the quarter, plus the end cap.
  auto detector = std::make_shared<Union>(std::make_shared<Difference>(barrel, quarter), cap);

  bool inside = detector->Criterion({1500, -100, 0}) <= 0;   // in the barrel
  bool removed = detector->Criterion({1000, 1000, 0}) <= 0;  // in the removed quarter
  auto mesh = detector->Tessellate(0.02);                    // closed triangle mesh
  // [shapes]

  std::printf("inside=%d removed=%d triangles=%zu closed=%d\n", inside, removed,
              mesh.triangles.size(), IsClosed(mesh));
  return inside && !removed && IsClosed(mesh) ? 0 : 1;
}
