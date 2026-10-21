// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T2.3: shape-context filters.
#include <catch2/catch_test_macros.hpp>

#include "TestSupport.h"
#include "fccvis/stdlib/ShapeFilters.h"

#include <memory>
#include <vector>

using namespace fccvis;
using namespace fccvis::stdlib;
using PE = data::PointElement;
using LE = data::LineElement;
using SE = data::SurfaceElement;

namespace {

PE At(double x, double y, double z, double energy = 1.0) {
  PE e;
  e.geometry.position = {x, y, z};
  e.meta.Set("energy", energy);
  return e;
}

LE Segment(std::vector<Vec3> v) {
  LE l;
  l.geometry.vertices = std::move(v);
  return l;
}

// The box [-1,1]^3 placed at x = +10.
geometry::Shape PlacedBox() {
  return geometry::Shape(std::make_shared<Box>(1, 1, 1),
                         geometry::Transform({10, 0, 0}, {1, 0, 0, 0, 1, 0, 0, 0, 1}));
}

}  // namespace

TEST_CASE("Inside / Outside / Within for points", "[stdlib][filters][shape]") {
  const auto box = PlacedBox();
  filter::Filter<PE> in = Inside(box), out = Outside(box), near = Within(0.5, box);
  CHECK(in.DoesPass(At(10, 0, 0)));
  CHECK(in.DoesPass(At(11, 1, 1)));      // boundary is inside
  CHECK_FALSE(in.DoesPass(At(0, 0, 0)));
  CHECK(out.DoesPass(At(0, 0, 0)));
  CHECK_FALSE(out.DoesPass(At(10, 0, 0)));
  CHECK(near.DoesPass(At(11.4, 0, 0)));
  CHECK_FALSE(near.DoesPass(At(11.6, 0, 0)));
  CHECK(near.DoesPass(At(10, 0, 0)));    // inside counts as within

  // shape given in world coordinates
  filter::Filter<PE> ball = Inside(std::make_shared<Sphere>(2.0));
  CHECK(ball.DoesPass(At(1.9, 0, 0)));
  CHECK_FALSE(ball.DoesPass(At(2.1, 0, 0)));
  CHECK_THROWS(Inside(ShapePtr{}));
}

TEST_CASE("Line tests: any vertex, all vertices, centroid", "[stdlib][filters][shape]") {
  auto ball = std::make_shared<Sphere>(1.0);
  const LE crossing = Segment({{0, 0, 0}, {3, 0, 0}});
  const LE inside = Segment({{0.1, 0, 0}, {-0.1, 0, 0}});
  const LE around = Segment({{-1.5, 0, 0}, {1.5, 0, 0}});  // vertices outside, centroid inside
  const LE empty;

  filter::Filter<LE> any = Inside(ball, LineTest::AnyVertex);
  filter::Filter<LE> all = Inside(ball, LineTest::AllVertices);
  filter::Filter<LE> centroid = Inside(ball, LineTest::Centroid);
  CHECK(any.DoesPass(crossing));
  CHECK_FALSE(all.DoesPass(crossing));
  CHECK(all.DoesPass(inside));
  CHECK_FALSE(any.DoesPass(around));
  CHECK(centroid.DoesPass(around));
  CHECK_FALSE(any.DoesPass(empty));
  CHECK_FALSE(centroid.DoesPass(empty));

  filter::Filter<LE> outAll = Outside(ball, LineTest::AllVertices);
  CHECK(outAll.DoesPass(around));
  CHECK_FALSE(outAll.DoesPass(crossing));
}

TEST_CASE("Surfaces are tested at their bounding-box centre", "[stdlib][filters][shape]") {
  const auto surfaces = test::MakeSurfaces(3);  // unit squares at x = 0, 2, 4
  filter::Filter<SE> nearOrigin = Inside(std::make_shared<Box>(1.5, 1, 1));
  CHECK(nearOrigin.DoesPass(surfaces[0]));
  CHECK_FALSE(nearOrigin.DoesPass(surfaces[1]));
  CHECK(SurfaceCentre(surfaces[2].geometry) == Vec3{4, 0, 0});

  // non-standard shape: centre from its tessellation, cached per shape
  data::Surface custom{geometry::Shape(std::make_shared<test::SquareShape>(),
                                       geometry::Transform({0, 5, 0}, {1, 0, 0, 0, 1, 0, 0, 0, 1}))};
  SE e{custom, {}};
  filter::Filter<SE> up = Inside(std::make_shared<Box>(1, 6, 1));
  CHECK(up.DoesPass(e));
  CHECK(up.DoesPass(e));
}

TEST_CASE("Shape filters compose with attribute filters", "[stdlib][filters][shape]") {
  auto ball = std::make_shared<Sphere>(5.0);
  filter::Filter<PE> f = Inside(ball) & EnergyAbove(1);
  CHECK(f.DoesPass(At(1, 1, 1, 2.0)));
  CHECK_FALSE(f.DoesPass(At(1, 1, 1, 0.5)));
  CHECK_FALSE(f.DoesPass(At(9, 1, 1, 2.0)));
  filter::Filter<PE> g = Outside(ball) | (Inside(ball) & EnergyBelow(1));
  CHECK(g.DoesPass(At(9, 0, 0, 2.0)));
  CHECK(g.DoesPass(At(0, 0, 0, 0.5)));
  CHECK_FALSE(g.DoesPass(At(0, 0, 0, 2.0)));
}

TEST_CASE("A default-constructed point sits at the origin", "[stdlib][filters][shape]") {
  // Regression: Point::position was left uninitialised, so shape filters on a
  // default element read indeterminate values (seen in Release builds).
  const PE e{};
  CHECK(e.geometry.position == Vec3{0, 0, 0});
  PE fresh;
  CHECK(fresh.geometry.position == Vec3{0, 0, 0});
  auto region = std::make_shared<Cylinder>(0, 2000, 3000);
  filter::Filter<PE> near = Within(5.0, region);
  CHECK(near.DoesPass(fresh));
}
