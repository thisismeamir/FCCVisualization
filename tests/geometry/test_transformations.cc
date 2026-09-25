#include <catch2/catch_test_macros.hpp>
#include "catch2/catch_approx.hpp"
#include "Geometry.h"

using namespace fccvis::geometry;

namespace {

constexpr double eps = 1e-10;

void RequireVector(
    const Transform::Vector& actual,
    const Transform::Vector& expected)
{
    for (std::size_t i = 0; i < 3; ++i) {
        REQUIRE(actual[i] == Catch::Approx(expected[i]).margin(eps));
    }
}

void RequireMatrix(
    const Transform::Matrix& actual,
    const Transform::Matrix& expected)
{
    for (std::size_t i = 0; i < 9; ++i) {
        REQUIRE(actual[i] == Catch::Approx(expected[i]).margin(eps));
    }
}

} // namespace

TEST_CASE("Default Transform is identity", "[geometry][transform]")
{
    Transform transform;

    REQUIRE(transform.Translation() ==
            Transform::Vector{0.0, 0.0, 0.0});

    REQUIRE(transform.Rotation() ==
            Transform::Matrix{
                1.0, 0.0, 0.0,
                0.0, 1.0, 0.0,
                0.0, 0.0, 1.0
            });
}

TEST_CASE("Transform stores translation and rotation",
          "[geometry][transform]")
{
    Transform::Vector translation{1.0, -2.0, 3.0};

    Transform::Matrix rotation{
        0.0, -1.0, 0.0,
        1.0,  0.0, 0.0,
        0.0,  0.0, 1.0
    };

    Transform transform(translation, rotation);

    REQUIRE(transform.Translation() == translation);
    REQUIRE(transform.Rotation() == rotation);
}

TEST_CASE("Identity Transform leaves points unchanged",
          "[geometry][transform]")
{
    Transform transform;

    const Transform::Vector point{1.5, -2.0, 7.25};

    RequireVector(transform.Apply(point), point);
    RequireVector(transform.Inverse(point), point);
}

TEST_CASE("Transform applies translation", "[geometry][transform]")
{
    Transform transform(
        {10.0, -2.0, 5.0},
        {1.0, 0.0, 0.0,
         0.0, 1.0, 0.0,
         0.0, 0.0, 1.0});

    RequireVector(
        transform.Apply({1.0, 2.0, 3.0}),
        {11.0, 0.0, 8.0});
}

TEST_CASE("Transform applies rotation", "[geometry][transform]")
{
    SECTION("90 degrees around Z")
    {
        Transform transform(
            {0.0, 0.0, 0.0},
            {0.0, -1.0, 0.0,
             1.0,  0.0, 0.0,
             0.0,  0.0, 1.0});

        RequireVector(
            transform.Apply({1.0, 0.0, 0.0}),
            {0.0, 1.0, 0.0});

        RequireVector(
            transform.Apply({0.0, 1.0, 0.0}),
            {-1.0, 0.0, 0.0});
    }

    SECTION("180 degrees around Z")
    {
        Transform transform(
            {0.0, 0.0, 0.0},
            {-1.0,  0.0, 0.0,
              0.0, -1.0, 0.0,
              0.0,  0.0, 1.0});

        RequireVector(
            transform.Apply({1.0, 2.0, 3.0}),
            {-1.0, -2.0, 3.0});
    }
}

TEST_CASE("Transform combines rotation and translation",
          "[geometry][transform]")
{
    Transform transform(
        {10.0, 20.0, 30.0},
        {0.0, -1.0, 0.0,
         1.0,  0.0, 0.0,
         0.0,  0.0, 1.0});

    INFO("Rotation is applied before translation");

    RequireVector(
        transform.Apply({1.0, 0.0, 0.0}),
        {10.0, 21.0, 30.0});
}

TEST_CASE("Inverse point transformation recovers the original point",
          "[geometry][transform]")
{
    Transform transform(
        {10.0, -5.0, 3.0},
        {0.0, -1.0, 0.0,
         1.0,  0.0, 0.0,
         0.0,  0.0, 1.0});

    const Transform::Vector point{2.0, 4.0, -1.0};

    const auto parent = transform.Apply(point);
    const auto local = transform.Inverse(parent);

    INFO("Apply followed by Inverse must be the identity");

    RequireVector(local, point);
}

TEST_CASE("Inverse transformation recovers the original point",
          "[geometry][transform]")
{
    Transform transform(
        {4.0, 5.0, 6.0},
        {0.0, 1.0, 0.0,
         -1.0, 0.0, 0.0,
         0.0, 0.0, 1.0});

    const Transform::Vector point{3.0, -2.0, 8.0};

    const auto parent = transform.Apply(point);
    const auto recovered = transform.Inverse(parent);

    RequireVector(recovered, point);
}

TEST_CASE("Transform inverse is the mathematical inverse",
          "[geometry][transform]")
{
    Transform transform(
        {4.0, 5.0, 6.0},
        {0.0, -1.0, 0.0,
         1.0,  0.0, 0.0,
         0.0,  0.0, 1.0});

    const auto inverse = transform.Inverse();

    const Transform::Vector point{2.0, 3.0, 4.0};

    RequireVector(
        inverse.Apply(transform.Apply(point)),
        point);

    RequireVector(
        transform.Apply(inverse.Apply(point)),
        point);
}

TEST_CASE("Matrix multiplication is associative for rotations",
          "[geometry][transform]")
{
    const Transform::Matrix rx{
        1.0, 0.0, 0.0,
        0.0, 0.0, -1.0,
        0.0, 1.0, 0.0
    };

    const Transform::Matrix ry{
         0.0, 0.0, 1.0,
         0.0, 1.0, 0.0,
        -1.0, 0.0, 0.0
    };

    const Transform::Matrix rz{
        0.0, -1.0, 0.0,
        1.0,  0.0, 0.0,
        0.0,  0.0, 1.0
    };

    const auto lhs =
        Transform::MatMul(Transform::MatMul(rx, ry), rz);

    const auto rhs =
        Transform::MatMul(rx, Transform::MatMul(ry, rz));

    RequireMatrix(lhs, rhs);
}

TEST_CASE("Matrix multiplication has identity",
          "[geometry][transform]")
{
    const Transform::Matrix identity{
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0
    };

    const Transform::Matrix rotation{
        0.0, -1.0, 0.0,
        1.0,  0.0, 0.0,
        0.0,  0.0, 1.0
    };

    RequireMatrix(
        Transform::MatMul(identity, rotation),
        rotation);

    RequireMatrix(
        Transform::MatMul(rotation, identity),
        rotation);
}

TEST_CASE("Transform composition matches sequential application",
          "[geometry][transform]")
{
    Transform first(
        {10.0, 0.0, 0.0},
        {0.0, -1.0, 0.0,
         1.0,  0.0, 0.0,
         0.0,  0.0, 1.0});

    Transform second(
        {0.0, 5.0, 0.0},
        {-1.0, 0.0, 0.0,
          0.0, 1.0, 0.0,
          0.0, 0.0, -1.0});

    const Transform composed = first * second;

    const Transform::Vector point{2.0, 3.0, 4.0};

    const auto sequential =
        first.Apply(second.Apply(point));

    const auto composed_result =
        composed.Apply(point);

    RequireVector(composed_result, sequential);
}
