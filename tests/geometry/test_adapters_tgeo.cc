#include "Adapters.h"


#include <catch2/catch_test_macros.hpp>

#include <TGeoManager.h>
#include <TGeoBBox.h>
#include <TGeoTube.h>

#include <cmath>
#include <memory>

using namespace fccvis::geometry;

namespace {

std::shared_ptr<const TGeoShape> MakeBox(double dx, double dy, double dz) {
  static TGeoManager* mgr = new TGeoManager("test", "test geometry");
  auto* box = new TGeoBBox(dx, dy, dz);
  auto* vol = new TGeoVolume("testVol", box);
  mgr->SetTopVolume(vol);
  mgr->CloseGeometry();
  return std::shared_ptr<const TGeoShape>(box, [](const TGeoShape*) {});
}
std::shared_ptr<const TGeoShape> MakeTube(
    double rmin,
    double rmax,
    double dz)
{
  return std::shared_ptr<const TGeoShape>(
      new TGeoTube(rmin, rmax, dz));
}

} // namespace

TEST_CASE("TGeoShapeAdapter can be constructed",
          "[adapter][tgeo]")
{
  auto rootShape = MakeBox(10.0, 20.0, 30.0);

  fccvis::geometry::TGeoShapeAdapter adapter(rootShape);

  REQUIRE(&adapter != nullptr);
}

TEST_CASE("TGeoShapeAdapter criterion identifies inside points",
          "[adapter][tgeo][criterion]")
{
  auto rootShape = MakeBox(10.0, 10.0, 10.0);
  TGeoShapeAdapter adapter(rootShape);

  const double criterion =
      adapter.Criterion({0.0, 0.0, 0.0});

  REQUIRE(criterion <= 0.0);
}

TEST_CASE("TGeoShapeAdapter criterion identifies outside points",
          "[adapter][tgeo][criterion]")
{
  auto rootShape = MakeBox(10.0, 10.0, 10.0);
  TGeoShapeAdapter adapter(rootShape);

  const double criterion =
      adapter.Criterion({20.0, 0.0, 0.0});

  REQUIRE(criterion > 0.0);
}

TEST_CASE("TGeoShapeAdapter criterion is signed",
          "[adapter][tgeo][criterion]")
{
  auto rootShape = MakeBox(10.0, 10.0, 10.0);
  TGeoShapeAdapter adapter(rootShape);

  REQUIRE(adapter.Criterion({0.0, 0.0, 0.0}) <= 0.0);
  REQUIRE(adapter.Criterion({20.0, 0.0, 0.0}) > 0.0);
}

TEST_CASE("TGeoShapeAdapter clone preserves geometry",
          "[adapter][tgeo][clone]")
{
  auto rootShape = MakeBox(10.0, 20.0, 30.0);

  TGeoShapeAdapter original(rootShape);

  auto clone = original.Clone();

  REQUIRE(clone != nullptr);

  REQUIRE(clone->Criterion({0.0, 0.0, 0.0}) ==
          original.Criterion({0.0, 0.0, 0.0}));

  REQUIRE(clone->Criterion({100.0, 0.0, 0.0}) ==
          original.Criterion({100.0, 0.0, 0.0}));
}

TEST_CASE("TGeoShapeAdapter clone creates independent adapter",
          "[adapter][tgeo][clone]")
{
  auto rootShape = MakeBox(10.0, 10.0, 10.0);

  TGeoShapeAdapter original(rootShape);
  auto clone = original.Clone();

  REQUIRE(clone.get() != &original);
}

TEST_CASE("TGeoShapeAdapter supports different TGeo shapes",
          "[adapter][tgeo]")
{
  auto box = MakeBox(10.0, 10.0, 10.0);
  auto tube = MakeTube(0.0, 10.0, 20.0);

  TGeoShapeAdapter boxAdapter(box);
  TGeoShapeAdapter tubeAdapter(tube);

  REQUIRE(boxAdapter.Criterion({0.0, 0.0, 0.0}) <= 0.0);
  REQUIRE(tubeAdapter.Criterion({0.0, 0.0, 0.0}) <= 0.0);

  REQUIRE(boxAdapter.Criterion({100.0, 0.0, 0.0}) > 0.0);
  REQUIRE(tubeAdapter.Criterion({100.0, 0.0, 0.0}) > 0.0);
}

TEST_CASE("TGeoShapeAdapter tessellation produces mesh",
          "[adapter][tgeo][tessellation]")
{
  auto rootShape = MakeBox(10.0, 20.0, 30.0);

  TGeoShapeAdapter adapter(rootShape);

  const auto mesh = adapter.Tessellate(1e-3);

  REQUIRE_FALSE(mesh.vertices.empty());
  REQUIRE_FALSE(mesh.triangles.empty());

  for (const auto& triangle : mesh.triangles) {
    REQUIRE(triangle[0] < mesh.vertices.size());
    REQUIRE(triangle[1] < mesh.vertices.size());
    REQUIRE(triangle[2] < mesh.vertices.size());
  }
}
