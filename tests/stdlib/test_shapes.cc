// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T2.1: standard shapes.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "fccvis/stdlib/Shapes.h"

#include <cmath>
#include <functional>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

using namespace fccvis::stdlib;
using fccvis::geometry::Transform;
using Catch::Approx;

namespace {

constexpr double kPi = std::numbers::pi;

struct Case {
  std::string name;
  std::function<std::shared_ptr<StandardShape>()> make;
  double volume;               // analytic volume (mm^3)
  std::vector<Vec3> inside;    // strictly inside
  std::vector<Vec3> outside;   // strictly outside
  std::vector<Vec3> boundary;  // on the surface
};

std::vector<Case> Cases() {
  return {
      {"box", [] { return std::make_shared<Box>(1, 2, 3); }, 48,
       {{0, 0, 0}, {0.9, 1.9, 2.9}}, {{1.1, 0, 0}, {0, 0, -3.1}}, {{1, 0, 0}, {0, -2, 0}}},
      {"sphere", [] { return std::make_shared<Sphere>(2); }, 4.0 / 3 * kPi * 8,
       {{0, 0, 0}, {1.9, 0, 0}}, {{2.1, 0, 0}, {1.5, 1.5, 0}}, {{0, 0, 2}}},
      {"cylinder", [] { return std::make_shared<Cylinder>(0, 2, 3); }, kPi * 4 * 6,
       {{0, 0, 0}, {1.9, 0, 2.9}}, {{2.1, 0, 0}, {0, 0, 3.1}}, {{2, 0, 0}, {0, 0, 3}}},
      {"hollow cylinder", [] { return std::make_shared<Cylinder>(1, 2, 3); }, kPi * 3 * 6,
       {{1.5, 0, 0}}, {{0, 0, 0}, {0.9, 0, 0}, {2.1, 0, 0}}, {{0, 1, 0}, {-2, 0, 0}}},
      {"tube segment", [] { return std::make_shared<TubeSegment>(1, 2, 3, 0, kPi / 2); }, kPi * 3 * 6 / 4,
       {{1.0, 1.0, 0}}, {{-1.0, 1.0, 0}, {1, -1, 0}, {0.5, 0.5, 0}}, {{1.5, 0, 0}}},
      {"cone frustum", [] { return std::make_shared<Cone>(0, 1, 0, 3, 2); }, kPi * 4 / 3 * (1 + 3 + 9),
       {{0, 0, 0}, {2.5, 0, 1.5}}, {{2.5, 0, -1.5}, {0, 0, 2.1}}, {{2, 0, 0}}},
      {"polycone with step", [] { return std::make_shared<Polycone>(std::vector<double>{-2, 0, 0, 2},
                                                                      std::vector<double>{0, 0, 0, 0},
                                                                      std::vector<double>{1, 1, 2, 2}); },
       kPi * 1 * 2 + kPi * 4 * 2, {{0.9, 0, -1}, {1.9, 0, 1}}, {{1.5, 0, -1}, {2.1, 0, 1}}, {{1, 0, -1}}},
      {"wedge", [] { return std::make_shared<Wedge>(0, kPi, 0, kPi / 2, 2); }, 4.0 / 3 * kPi * 8 / 4,
       {{0.1, 0.5, 0.5}}, {{0.5, -0.5, 0.5}, {0, 0.5, -0.5}, {0, 2.1, 0.1}}, {{0, 0, 2}}},
      {"union", [] { return std::make_shared<Union>(std::make_shared<Box>(1, 1, 1),
                                                    std::make_shared<Transformed>(std::make_shared<Box>(1, 1, 1),
                                                                                  Transform({1, 0, 0}, {1, 0, 0, 0, 1, 0, 0, 0, 1}))); },
       12, {{1.5, 0, 0}, {-0.5, 0, 0}}, {{2.1, 0, 0}, {0, 1.1, 0}}, {}},
      {"intersection", [] { return std::make_shared<Intersection>(std::make_shared<Box>(1, 1, 1),
                                                                  std::make_shared<Sphere>(1.2)); },
       0, {{0, 0, 0}, {0.9, 0, 0}}, {{0.9, 0.9, 0.9}, {1.1, 0, 0}}, {}},
      {"difference", [] { return std::make_shared<Difference>(std::make_shared<Box>(2, 2, 2),
                                                              std::make_shared<Sphere>(1)); },
       64 - 4.0 / 3 * kPi, {{1.5, 1.5, 1.5}}, {{0, 0, 0}, {2.1, 0, 0}}, {}},
      {"transformed", [] { return std::make_shared<Transformed>(std::make_shared<Box>(1, 2, 3),
                                                                Transform({10, 0, 0}, {0, -1, 0, 1, 0, 0, 0, 0, 1})); },
       48, {{10, 0, 0}, {11.9, 0.9, 0}}, {{0, 0, 0}, {10, 1.1, 0}}, {{12, 0, 0}}},
  };
}

}  // namespace

TEST_CASE("Standard shapes: criterion, closed tessellation, bounds and volume", "[stdlib][shapes]") {
  for (const auto& c : Cases()) {
    DYNAMIC_SECTION(c.name) {
      const auto shape = c.make();
      for (const auto& p : c.inside) CHECK(shape->Criterion(p) < 0);
      for (const auto& p : c.outside) CHECK(shape->Criterion(p) > 0);
      for (const auto& p : c.boundary) CHECK(std::abs(shape->Criterion(p)) < 1e-9);

      const auto mesh = shape->Tessellate(0.01);
      REQUIRE_FALSE(mesh.triangles.empty());
      CHECK(IsClosed(mesh));
      for (const auto& t : mesh.triangles)
        for (auto i : t) REQUIRE(i < mesh.vertices.size());

      const Bounds want = shape->GetBounds();
      const Bounds got = BoundsOf(mesh);
      const double slack = 0.03 * want.Diagonal();
      for (int a = 0; a < 3; ++a) {
        CHECK(got.lo[a] >= want.lo[a] - slack);
        CHECK(got.hi[a] <= want.hi[a] + slack);
        CHECK(got.lo[a] <= want.lo[a] + slack);
        CHECK(got.hi[a] >= want.hi[a] - slack);
      }
      const double vol = SignedVolume(mesh);
      CHECK(vol > 0);  // outward orientation
      if (c.volume > 0) CHECK(vol == Approx(c.volume).epsilon(0.06));

      auto copy = shape->Clone();
      for (const auto& p : c.inside) CHECK(copy->Criterion(p) == shape->Criterion(p));
    }
  }
}

TEST_CASE("Boolean shapes agree with their operands", "[stdlib][shapes][boolean]") {
  auto a = std::make_shared<Box>(1, 1, 1);
  auto b = std::make_shared<Sphere>(1.3);
  Union u(a, b);
  Intersection i(a, b);
  Difference d(a, b);
  std::mt19937 rng(42);
  std::uniform_real_distribution<double> x(-2, 2);
  for (int n = 0; n < 2000; ++n) {
    const Vec3 p{x(rng), x(rng), x(rng)};
    const bool ia = a->Criterion(p) <= 0, ib = b->Criterion(p) <= 0;
    REQUIRE((u.Criterion(p) <= 0) == (ia || ib));
    REQUIRE((i.Criterion(p) <= 0) == (ia && ib));
    REQUIRE((d.Criterion(p) <= 0) == (ia && !ib));
  }
  // Inclusion-exclusion on the tessellated volumes (approximate).
  const double va = SignedVolume(a->Tessellate(0.02)), vb = SignedVolume(b->Tessellate(0.02));
  const double vu = SignedVolume(u.Tessellate(0.02)), vi = SignedVolume(i.Tessellate(0.02));
  CHECK(vu + vi == Approx(va + vb).epsilon(0.05));
}

TEST_CASE("Half-space", "[stdlib][shapes]") {
  HalfSpace h({0, 0, 2}, 1.0, 50);
  CHECK(h.Criterion({0, 0, 0}) < 0);
  CHECK(h.Criterion({7, -3, 1.5}) > 0);
  CHECK(h.Criterion({7, -3, 1.0}) == Approx(0).margin(1e-12));
  const auto m = h.Tessellate(0.1);
  REQUIRE(m.triangles.size() == 2);
  for (const auto& v : m.vertices) CHECK(v[2] == Approx(1.0));
  // cutting a box with it gives a closed half box
  Intersection cut(std::make_shared<Box>(2, 2, 2), std::make_shared<HalfSpace>(h));
  const auto mesh = cut.Tessellate(0.02);
  CHECK(IsClosed(mesh));
  CHECK(SignedVolume(mesh) == Approx(16 * 3).epsilon(0.05));
}

TEST_CASE("Invalid shape parameters are rejected", "[stdlib][shapes]") {
  CHECK_THROWS_AS(Box(0, 1, 1), std::invalid_argument);
  CHECK_THROWS_AS(Sphere(-1), std::invalid_argument);
  CHECK_THROWS_AS(Cylinder(2, 1, 1), std::invalid_argument);
  CHECK_THROWS_AS(Cylinder(0, 1, 1, 0, 0), std::invalid_argument);
  CHECK_THROWS_AS(Polycone({0}, {0}, {1}), std::invalid_argument);
  CHECK_THROWS_AS(Polycone({1, 0}, {0, 0}, {1, 1}), std::invalid_argument);
  CHECK_THROWS_AS(Wedge(0, 1, 1, 0.5, 1), std::invalid_argument);
  CHECK_THROWS_AS(HalfSpace({0, 0, 0}, 0), std::invalid_argument);
  CHECK_THROWS_AS(Union(nullptr, std::make_shared<Box>(1, 1, 1)), std::invalid_argument);
}

TEST_CASE("Tessellation tolerance controls the segment count", "[stdlib][shapes]") {
  CHECK(SegmentsForTolerance(0.2) == 8);
  CHECK(SegmentsForTolerance(1e-4) == 223);
  Sphere s(1);
  CHECK(s.Tessellate(0.001).triangles.size() > s.Tessellate(0.05).triangles.size());
}
