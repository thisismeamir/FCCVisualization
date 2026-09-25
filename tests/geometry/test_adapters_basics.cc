#include "Geometry.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace fccvis::geometry;

namespace {

class TestShape final : public AbstractShape {
public:
  std::shared_ptr<AbstractShape> Clone() const override {
    return std::make_shared<TestShape>(*this);
  }

  double Criterion(const Vector& p) const override {
    return p[0] * p[0] + p[1] * p[1] + p[2] * p[2] - 1.0;
  }

  Mesh Tessellate(double) const override {
    return {};
  }
};

} // namespace

TEST_CASE("Transform default construction", "[geometry][transform]")
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

TEST_CASE("Transform translation accessors", "[geometry][transform]")
{
  Transform transform;

  transform.Translation() = {1.0, 2.0, 3.0};

  REQUIRE(transform.Translation() ==
          Transform::Vector{1.0, 2.0, 3.0});

  transform.SetTranslation({4.0, 5.0, 6.0});

  REQUIRE(transform.Translation() ==
          Transform::Vector{4.0, 5.0, 6.0});
}

TEST_CASE("Transform rotation accessors", "[geometry][transform]")
{
  Transform transform;

  Transform::Matrix rotation{
      0.0, -1.0, 0.0,
      1.0,  0.0, 0.0,
      0.0,  0.0, 1.0
  };

  transform.Rotation() = rotation;
  REQUIRE(transform.Rotation() == rotation);

  Transform::Matrix other{
      1.0, 0.0, 0.0,
      0.0, 0.0, -1.0,
      0.0, 1.0, 0.0
  };

  transform.SetRotation(other);

  REQUIRE(transform.Rotation() == other);
}

TEST_CASE("Transform applies translation adapters basics.", "[geometry][transform]")
{
  Transform transform(
      {10.0, -2.0, 5.0},
      {
          1.0, 0.0, 0.0,
          0.0, 1.0, 0.0,
          0.0, 0.0, 1.0
      });

  const auto result = transform.Apply({1.0, 2.0, 3.0});

  REQUIRE(result == Transform::Vector{11.0, 0.0, 8.0});
}

TEST_CASE("Transform applies rotation adapters basics", "[geometry][transform]")
{
  Transform transform(
      {0.0, 0.0, 0.0},
      {
          0.0, -1.0, 0.0,
          1.0,  0.0, 0.0,
          0.0,  0.0, 1.0
      });

  const auto result = transform.Apply({1.0, 0.0, 0.0});

  REQUIRE(result == Transform::Vector{0.0, 1.0, 0.0});
}

TEST_CASE("Transform inverse recovers original point",
          "[geometry][transform]")
{
  Transform transform(
      {10.0, -2.0, 5.0},
      {
          0.0, -1.0, 0.0,
          1.0,  0.0, 0.0,
          0.0,  0.0, 1.0
      });

  const Transform::Vector point{3.0, 4.0, 7.0};

  const auto world = transform.Apply(point);
  const auto recovered = transform.Inverse(world);

  REQUIRE(recovered == point);
}

TEST_CASE("Transform inverse transformation composes to identity",
          "[geometry][transform]")
{
  Transform transform(
      {3.0, 4.0, 5.0},
      {
          0.0, -1.0, 0.0,
          1.0,  0.0, 0.0,
          0.0,  0.0, 1.0
      });

  const auto inverse = transform.Inverse();

  const auto point = Transform::Vector{2.0, 7.0, -1.0};

  REQUIRE(inverse.Apply(transform.Apply(point)) == point);
  REQUIRE(transform.Apply(inverse.Apply(point)) == point);
}

TEST_CASE("Transform composition", "[geometry][transform]")
{
  Transform parent(
      {10.0, 0.0, 0.0},
      {
          1.0, 0.0, 0.0,
          0.0, 1.0, 0.0,
          0.0, 0.0, 1.0
      });

  Transform child(
      {0.0, 5.0, 0.0},
      {
          1.0, 0.0, 0.0,
          0.0, 1.0, 0.0,
          0.0, 0.0, 1.0
      });

  const auto world = parent * child;

  REQUIRE(world.Apply({0.0, 0.0, 0.0}) ==
          Transform::Vector{10.0, 5.0, 0.0});
}

TEST_CASE("Shape exposes its geometry and transform",
          "[geometry][shape]")
{
  auto geometry = std::make_shared<TestShape>();

  Shape shape(
      geometry,
      Transform({1.0, 2.0, 3.0},
                {
                    1.0, 0.0, 0.0,
                    0.0, 1.0, 0.0,
                    0.0, 0.0, 1.0
                }));

  REQUIRE(&shape.Geometry() == geometry.get());

  REQUIRE(shape.GetTransform().Translation() ==
          Transform::Vector{1.0, 2.0, 3.0});
}

TEST_CASE("Shape mutable transform accessor modifies the shape adapters basics",
          "[geometry][shape]")
{
  auto geometry = std::make_shared<TestShape>();
  Shape shape(geometry);

  shape.GetTransform().Translation() = {10.0, 0.0, 0.0};

  REQUIRE(shape.GetTransform().Translation() ==
          Transform::Vector{10.0, 0.0, 0.0});
}

TEST_CASE("Shape Contains accounts for transform",
          "[geometry][shape]")
{
  auto geometry = std::make_shared<TestShape>();

  Shape shape(
      geometry,
      Transform({10.0, 0.0, 0.0},
                {
                    1.0, 0.0, 0.0,
                    0.0, 1.0, 0.0,
                    0.0, 0.0, 1.0
                }));

  REQUIRE(shape.Contains({10.0, 0.0, 0.0}));
  REQUIRE_FALSE(shape.Contains({12.0, 0.0, 0.0}));
}

TEST_CASE("Placement exposes mutable transform",
          "[geometry][placement]")
{
  Placement placement;

  placement.GetTransform().Translation() = {1.0, 2.0, 3.0};

  REQUIRE(placement.GetTransform().Translation() ==
          Transform::Vector{1.0, 2.0, 3.0});
}

TEST_CASE("Placement can be constructed from transform",
          "[geometry][placement]")
{
  Transform transform(
      {4.0, 5.0, 6.0},
      {
          1.0, 0.0, 0.0,
          0.0, 1.0, 0.0,
          0.0, 0.0, 1.0
      });

  Placement placement(transform);

  REQUIRE(placement.GetTransform().Translation() ==
          Transform::Vector{4.0, 5.0, 6.0});
}
