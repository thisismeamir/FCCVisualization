#include <catch2/catch_test_macros.hpp>
#include "Geometry.h"

using namespace fccvis::geometry;

TEST_CASE("Placement defaults to identity",
          "[geometry][placement]")
{
    Placement placement;

    REQUIRE(placement.GetTransform().Translation() ==
            Transform::Vector{0.0, 0.0, 0.0});

    REQUIRE(placement.GetTransform().Rotation() ==
            Transform::Matrix{
                1.0, 0.0, 0.0,
                0.0, 1.0, 0.0,
                0.0, 0.0, 1.0
            });
}

TEST_CASE("Placement stores its transform",
          "[geometry][placement]")
{
    Transform transform(
        {1.0, 2.0, 3.0},
        {0.0, -1.0, 0.0,
         1.0,  0.0, 0.0,
         0.0,  0.0, 1.0});

    Placement placement(transform);

    REQUIRE(placement.GetTransform().Translation() ==
            transform.Translation());

    REQUIRE(placement.GetTransform().Rotation() ==
            transform.Rotation());
}

TEST_CASE("Placement transform can be modified",
          "[geometry][placement]")
{
    Placement placement;

    placement.GetTransform().Translation() =
        Transform::Vector{10.0, 20.0, 30.0};

    REQUIRE(placement.GetTransform().Translation() ==
            Transform::Vector{10.0, 20.0, 30.0});
}

TEST_CASE("Placement transform can be completely replaced",
          "[geometry][placement]")
{
    Placement placement;

    Transform transform(
        {4.0, 5.0, 6.0},
        {1.0, 0.0, 0.0,
         0.0, 0.0, -1.0,
         0.0, 1.0, 0.0});

    placement.GetTransform() = transform;

    REQUIRE(placement.GetTransform().Translation() ==
            transform.Translation());

    REQUIRE(placement.GetTransform().Rotation() ==
            transform.Rotation());
}
