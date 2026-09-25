#include "Detector.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace fccvis::geometry;
using namespace fccvis::geometry::detector;

namespace {

class TestShape final : public AbstractShape {
public:
  std::shared_ptr<AbstractShape> Clone() const override {
    return std::make_shared<TestShape>(*this);
  }

  double Criterion(const Vector& p) const override {
    return p[0] * p[0] +
           p[1] * p[1] +
           p[2] * p[2] - 1.0;
  }

  Mesh Tessellate(double) const override {
    return {};
  }
};

Shape MakeShape(Transform transform = {})
{
  return Shape(
      std::make_shared<TestShape>(),
      std::move(transform));
}

} // namespace

TEST_CASE("DetectorNode stores name and path",
          "[detector][node]")
{
  DetectorNode node(
      "Tracker",
      "/world/Tracker",
      MakeShape());

  REQUIRE(node.Name() == "Tracker");
  REQUIRE(node.Path() == "/world/Tracker");
}

TEST_CASE("DetectorNode exposes shape",
          "[detector][node]")
{
  Shape shape = MakeShape(
      Transform(
          {1.0, 2.0, 3.0},
          {
              1.0, 0.0, 0.0,
              0.0, 1.0, 0.0,
              0.0, 0.0, 1.0
          }));

  DetectorNode node(
      "Tracker",
      "/world/Tracker",
      shape);

  REQUIRE(node.GetShape().GetTransform().Translation() ==
          Transform::Vector{1.0, 2.0, 3.0});
}

TEST_CASE("DetectorNode stores children",
          "[detector][node][tree]")
{
  auto child1 = std::make_shared<DetectorNode>(
      "Layer1",
      "/world/Tracker/Layer1",
      MakeShape());

  auto child2 = std::make_shared<DetectorNode>(
      "Layer2",
      "/world/Tracker/Layer2",
      MakeShape());

  DetectorNode node(
      "Tracker",
      "/world/Tracker",
      MakeShape(),
      {child1, child2});

  REQUIRE(node.Children().size() == 2);
  REQUIRE(node.Children()[0] == child1);
  REQUIRE(node.Children()[1] == child2);
}

TEST_CASE("DetectorNode has no children by default",
          "[detector][node][tree]")
{
  DetectorNode node(
      "World",
      "/world",
      MakeShape());

  REQUIRE(node.Children().empty());
}

TEST_CASE("DetectorNode world transform for root node",
          "[detector][node][transform]")
{
  DetectorNode node(
      "Tracker",
      "/world/Tracker",
      MakeShape(
          Transform(
              {10.0, 20.0, 30.0},
              {
                  1.0, 0.0, 0.0,
                  0.0, 1.0, 0.0,
                  0.0, 0.0, 1.0
              })));

  const auto world = node.WorldTransform();

  REQUIRE(world.Translation() ==
          Transform::Vector{10.0, 20.0, 30.0});
}

TEST_CASE("DetectorNode world transform includes parent",
          "[detector][node][transform]")
{
  DetectorNode node(
      "Child",
      "/world/Parent/Child",
      MakeShape(
          Transform(
              {0.0, 5.0, 0.0},
              {
                  1.0, 0.0, 0.0,
                  0.0, 1.0, 0.0,
                  0.0, 0.0, 1.0
              })));

  Transform parentWorld(
      {10.0, 20.0, 30.0},
      {
          1.0, 0.0, 0.0,
          0.0, 1.0, 0.0,
          0.0, 0.0, 1.0
      });

  const auto world = node.WorldTransform(parentWorld);

  REQUIRE(world.Translation() ==
          Transform::Vector{10.0, 25.0, 30.0});
}

TEST_CASE("DetectorNode world transform handles nested translations",
          "[detector][node][transform][tree]")
{
  auto child = std::make_shared<DetectorNode>(
      "Child",
      "/world/Parent/Child",
      MakeShape(
          Transform(
              {0.0, 5.0, 0.0},
              {
                  1.0, 0.0, 0.0,
                  0.0, 1.0, 0.0,
                  0.0, 0.0, 1.0
              })));

  DetectorNode parent(
      "Parent",
      "/world/Parent",
      MakeShape(
          Transform(
              {10.0, 0.0, 0.0},
              {
                  1.0, 0.0, 0.0,
                  0.0, 1.0, 0.0,
                  0.0, 0.0, 1.0
              })),
      {child});

  const auto parentWorld = parent.WorldTransform();
  const auto childWorld = child->WorldTransform(parentWorld);

  REQUIRE(parentWorld.Translation() ==
          Transform::Vector{10.0, 0.0, 0.0});

  REQUIRE(childWorld.Translation() ==
          Transform::Vector{10.0, 5.0, 0.0});
}

TEST_CASE("DetectorNode world transform respects parent rotation",
          "[detector][node][transform]")
{
  DetectorNode node(
      "Child",
      "/world/Parent/Child",
      MakeShape(
          Transform(
              {10.0, 0.0, 0.0},
              {
                  1.0, 0.0, 0.0,
                  0.0, 1.0, 0.0,
                  0.0, 0.0, 1.0
              })));

  Transform parentWorld(
      {0.0, 0.0, 0.0},
      {
          0.0, -1.0, 0.0,
          1.0,  0.0, 0.0,
          0.0,  0.0, 1.0
      });

  const auto world = node.WorldTransform(parentWorld);

  REQUIRE(world.Translation() ==
          Transform::Vector{0.0, 10.0, 0.0});
}
