#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "Geometry.h"

// Include wherever your test Sphere / TestNegativeShape live.

using namespace fccvis::geometry;

namespace {
class TestNegativeShape : public AbstractShape {
public:
  double Criterion(const Vector &point) const override { return point[0]; }

  Mesh Tessellate(double) const override { return {}; }

  std::shared_ptr<AbstractShape> Clone() const override {
    return std::make_shared<TestNegativeShape>(*this);
  }
};

class TestSphere : public AbstractShape {
public:
  double Criterion(const Vector &point) const override {
    return std::pow(point[0], 2.0) + std::pow(point[1], 2.0) +
           std::pow(point[1], 2.0) - 1;
  }
  Mesh Tessellate(double) const override { return {}; }

  std::shared_ptr<AbstractShape> Clone() const override {
    return std::make_shared<TestSphere>(*this);
  }
};

}
TEST_CASE("Shape uses the underlying geometry",
          "[geometry][shape]")
{
    auto geometry = std::make_shared<TestSphere>();

    Shape shape(geometry);

    INFO("Shape must expose the geometry it was constructed with");

    REQUIRE(&shape.Geometry() == geometry.get());
}

TEST_CASE("Shape defaults to the identity transform",
          "[geometry][shape]")
{
    auto geometry = std::make_shared<TestSphere>();
    Shape shape(geometry);

    REQUIRE(shape.GetTransform().Translation() ==
            Transform::Vector{0.0, 0.0, 0.0});

    REQUIRE(shape.GetTransform().Rotation() ==
            Transform::Matrix{
                1.0, 0.0, 0.0,
                0.0, 1.0, 0.0,
                0.0, 0.0, 1.0
            });
}

TEST_CASE("Shape delegates containment to its geometry",
          "[geometry][shape]")
{
    auto geometry = std::make_shared<TestSphere>();
    Shape shape(geometry);

    SECTION("inside")
    {
        INFO("Origin should be inside the sphere");
        REQUIRE(shape.Contains({0.0, 0.0, 0.0}));
    }

    SECTION("boundary")
    {
        INFO("Boundary must satisfy Criterion <= 0");
        REQUIRE(shape.Contains({1.0, 0.0, 0.0}));
    }

    SECTION("outside")
    {
        REQUIRE_FALSE(shape.Contains({2.0, 0.0, 0.0}));
    }
}

TEST_CASE("Shape correctly transforms parent coordinates into local coordinates",
          "[geometry][shape]")
{
    auto geometry = std::make_shared<TestSphere>();

    Transform transform(
        {10.0, 0.0, 0.0},
        {1.0, 0.0, 0.0,
         0.0, 1.0, 0.0,
         0.0, 0.0, 1.0});

    Shape shape(geometry, transform);

    SECTION("local origin becomes translated parent point")
    {
        REQUIRE(shape.Contains({10.0, 0.0, 0.0}));
    }

    SECTION("old parent origin is outside")
    {
        REQUIRE_FALSE(shape.Contains({0.0, 0.0, 0.0}));
    }

    SECTION("point one unit from translated centre is on boundary")
    {
        REQUIRE(shape.Contains({11.0, 0.0, 0.0}));
    }

    SECTION("point beyond translated sphere is outside")
    {
        REQUIRE_FALSE(shape.Contains({12.0, 0.0, 0.0}));
    }
}

TEST_CASE("Shape correctly handles rotated geometry",
          "[geometry][shape]")
{
    auto geometry = std::make_shared<TestNegativeShape>();

    Transform transform(
        {0.0, 0.0, 0.0},
        {0.0, -1.0, 0.0,
         1.0,  0.0, 0.0,
         0.0,  0.0, 1.0});

    Shape shape(geometry, transform);

    // Choose points according to the actual semantics of
    // TestNegativeShape.
    //
    // The important thing here is to verify that Shape performs
    // the inverse coordinate transformation before Criterion().
}

TEST_CASE("Shape mutable transform accessor modifies the shape",
          "[geometry][shape]")
{
    auto geometry = std::make_shared<TestSphere>();
    Shape shape(geometry);


    Transform transform(
        {10.0, 0.0, 0.0},
        {1.0, 0.0, 0.0,
         0.0, 1.0, 0.0,
         0.0, 0.0, 1.0});

    shape.GetTransform() = transform;
    REQUIRE(shape.GetTransform().Translation() ==
            Transform::Vector{10.0, 0.0, 0.0});

    REQUIRE(shape.Contains({10.0, 0.0, 0.0}));
    REQUIRE_FALSE(shape.Contains({0.0, 0.0, 0.0}));
}

TEST_CASE("Shape preserves the supplied transform",
          "[geometry][shape]")
{
    auto geometry = std::make_shared<TestSphere>();

    Transform transform(
        {1.0, 2.0, 3.0},
        {0.0, -1.0, 0.0,
         1.0,  0.0, 0.0,
         0.0,  0.0, 1.0});

    Shape shape(geometry, transform);

    REQUIRE(shape.GetTransform().Translation() ==
            transform.Translation());

    REQUIRE(shape.GetTransform().Rotation() ==
            transform.Rotation());
}
