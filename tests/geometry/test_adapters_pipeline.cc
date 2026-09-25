#include "Adapters.h"
#include "Detector.h"
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <DD4hep/Detector.h>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

namespace {

std::filesystem::path DetectorPath() {
  return "/home/kid-a/scientific-software/installs/k4geo/detectors-data/FCCee/"
         "CLD/compact/CLD_o2_v05/CLD_o2_v05.xml";
}

void PrintTree(
    const std::shared_ptr<fccvis::geometry::detector::DetectorNode> &node,
    std::size_t depth = 0) {
  using namespace fccvis::geometry;

  const auto &transform = node->GetShape().GetTransform();
  const auto &translation = transform.Translation();

  std::cout << std::string(depth * 2, ' ') << node->Name() << "  "
            << node->Path() << "  "
            << "t=(" << std::fixed << std::setprecision(3) << translation[0]
            << ", " << translation[1] << ", " << translation[2] << ")"
            << "  children=" << node->Children().size() << '\n';

  for (const auto &child : node->Children())
    PrintTree(child, depth + 1);
}

void ValidateTree(
    const std::shared_ptr<fccvis::geometry::detector::DetectorNode> &node) {
  REQUIRE(node != nullptr);
  REQUIRE_FALSE(node->Name().empty());
  REQUIRE_FALSE(node->Path().empty());

  // Every DetectorNode currently represents a geometric object.
  REQUIRE_NOTHROW(node->GetShape().Geometry());

  for (const auto &child : node->Children())
    ValidateTree(child);
}

} // namespace

TEST_CASE("CLD detector can be converted into a DetectorNode tree",
          "[adapter][pipeline][dd4hep]") {
  REQUIRE(std::filesystem::exists(DetectorPath()));

  auto &detector = dd4hep::Detector::getInstance();

  REQUIRE_NOTHROW(detector.fromCompact(DetectorPath().string()));

  const auto world = detector.world();

  REQUIRE(world.isValid());
  REQUIRE(world.children().size() > 0);

  auto root = fccvis::geometry::detector::BuildNode(world);

  REQUIRE(root != nullptr);
}

TEST_CASE("Converted CLD detector tree preserves basic node information",
          "[adapter][pipeline][detector]") {
  auto &detector = dd4hep::Detector::getInstance();

  if (!detector.world().isValid())
    detector.fromCompact(DetectorPath().string());

  auto root = fccvis::geometry::detector::BuildNode(detector.world());

  REQUIRE(root != nullptr);

  REQUIRE_FALSE(root->Name().empty());
  REQUIRE_FALSE(root->Path().empty());

  REQUIRE_FALSE(root->Children().empty());
}

TEST_CASE("Converted CLD detector tree contains valid geometry",
          "[adapter][pipeline][geometry]") {
  auto &detector = dd4hep::Detector::getInstance();

  if (!detector.world().isValid())
    detector.fromCompact(DetectorPath().string());

  auto root = fccvis::geometry::detector::BuildNode(detector.world());

  REQUIRE(root != nullptr);

  ValidateTree(root);
}

TEST_CASE("Converted CLD detector tree preserves hierarchy",
          "[adapter][pipeline][tree]") {
  auto &detector = dd4hep::Detector::getInstance();

  if (!detector.world().isValid())
    detector.fromCompact(DetectorPath().string());

  auto root = fccvis::geometry::detector::BuildNode(detector.world());

  REQUIRE(root != nullptr);
  REQUIRE_FALSE(root->Children().empty());

  for (const auto &child : root->Children()) {
    REQUIRE(child != nullptr);
    REQUIRE_FALSE(child->Name().empty());
    REQUIRE_FALSE(child->Path().empty());
  }
}

TEST_CASE("Converted CLD detector tree can be recursively traversed",
          "[adapter][pipeline][tree]") {
  auto &detector = dd4hep::Detector::getInstance();

  if (!detector.world().isValid())
    detector.fromCompact(DetectorPath().string());

  auto root = fccvis::geometry::detector::BuildNode(detector.world());

  REQUIRE(root != nullptr);

  std::size_t nodeCount = 0;

  std::function<void(
      const std::shared_ptr<fccvis::geometry::detector::DetectorNode> &)>
      visit;

  visit = [&](const auto &node) {
    REQUIRE(node != nullptr);

    ++nodeCount;

    for (const auto &child : node->Children())
      visit(child);
  };

  visit(root);

  REQUIRE(nodeCount > 1);

  std::cout << "\nConverted detector tree contains " << nodeCount
            << " nodes.\n";
}

TEST_CASE("Converted CLD detector tree can be printed",
          "[adapter][pipeline][tree][output]") {
  auto &detector = dd4hep::Detector::getInstance();

  if (!detector.world().isValid())
    detector.fromCompact(DetectorPath().string());

  auto root = fccvis::geometry::detector::BuildNode(detector.world());

  REQUIRE(root != nullptr);

  std::cout << "\n"
            << "FCCVisualization DetectorNode tree\n"
            << "===================================\n";

  PrintTree(root);
}

TEST_CASE("Converted node preserves DD4hep placement transform",
          "[adapter][pipeline][transform]") {
  auto &detector = dd4hep::Detector::getInstance();

  if (!detector.world().isValid())
    detector.fromCompact(DetectorPath().string());

  const auto world = detector.world();
  REQUIRE(world.isValid());
  REQUIRE_FALSE(world.children().empty());

  const auto &[name, de] = *world.children().begin();

  auto node = fccvis::geometry::detector::BuildNode(de);

  REQUIRE(node != nullptr);
  REQUIRE(node->Name() == de.name());
  REQUIRE(node->Path() == de.path());

  const auto &ddMatrix = de.placement().matrix();
  const auto &transform = node->GetShape().GetTransform();

  const auto *translation = ddMatrix.GetTranslation();
  const auto &nodeTranslation = transform.Translation();

  REQUIRE(nodeTranslation[0] == Catch::Approx(translation[0]));
  REQUIRE(nodeTranslation[1] == Catch::Approx(translation[1]));
  REQUIRE(nodeTranslation[2] == Catch::Approx(translation[2]));

  const auto *rotation = ddMatrix.GetRotationMatrix();
  const auto &nodeRotation = transform.Rotation();

  for (int i = 0; i < 9; ++i)
    REQUIRE(nodeRotation[i] == Catch::Approx(rotation[i]));
}

TEST_CASE("Converted node contains a functional geometry adapter",
          "[adapter][pipeline][geometry]") {
  auto &detector = dd4hep::Detector::getInstance();

  if (!detector.world().isValid())
    detector.fromCompact(DetectorPath().string());

  const auto world = detector.world();
  REQUIRE(world.isValid());
  REQUIRE_FALSE(world.children().empty());

  const auto &[name, de] = *world.children().begin();
  std::cerr << "Solid type: " << de.volume().solid()->GetName() << " / "
    << de.volume().solid().ptr()->ClassName() << "\n";

  auto node = fccvis::geometry::detector::BuildNode(de);

  REQUIRE(node != nullptr);

  const auto &geometry = node->GetShape().Geometry();

  REQUIRE_NOTHROW(geometry.Criterion({0.0, 0.0, 0.0}));

  const auto mesh = geometry.Tessellate(1.0);

  REQUIRE_FALSE(mesh.vertices.empty());
  REQUIRE_FALSE(mesh.triangles.empty());
}

TEST_CASE("DetectorNode world transform equals its local transform at root",
          "[detector][transform]") {
  auto &detector = dd4hep::Detector::getInstance();

  if (!detector.world().isValid())
    detector.fromCompact(DetectorPath().string());

  const auto world = detector.world();
  REQUIRE(world.isValid());
  REQUIRE_FALSE(world.children().empty());

  const auto &[name, de] = *world.children().begin();

  auto node = fccvis::geometry::detector::BuildNode(de);

  REQUIRE(node != nullptr);

  const auto worldTransform = node->WorldTransform();
  const auto &localTransform = node->GetShape().GetTransform();

  REQUIRE(worldTransform.Translation() == localTransform.Translation());

  REQUIRE(worldTransform.Rotation() == localTransform.Rotation());
}

TEST_CASE("DetectorNode world transform composes parent and child transforms",
          "[detector][transform][tree]") {
  auto &detector = dd4hep::Detector::getInstance();

  if (!detector.world().isValid())
    detector.fromCompact(DetectorPath().string());

  const auto world = detector.world();
  REQUIRE(world.isValid());

  std::shared_ptr<fccvis::geometry::detector::DetectorNode> parent;

  for (const auto &[name, de] : world.children()) {
    auto candidate = fccvis::geometry::detector::BuildNode(de);

    if (candidate && !candidate->Children().empty()) {
      parent = std::move(candidate);
      break;
    }
  }

  REQUIRE(parent != nullptr);
  REQUIRE_FALSE(parent->Children().empty());

  const auto child = parent->Children().front();

  REQUIRE(child != nullptr);

  const auto expected =
      parent->GetShape().GetTransform() * child->GetShape().GetTransform();

  const auto actual = child->WorldTransform(parent->WorldTransform());

  REQUIRE(actual.Translation() == expected.Translation());

  REQUIRE(actual.Rotation() == expected.Rotation());
}

TEST_CASE("Converted detector hierarchy preserves child names and paths",
          "[adapter][pipeline][tree]") {
  auto &detector = dd4hep::Detector::getInstance();

  if (!detector.world().isValid())
    detector.fromCompact(DetectorPath().string());

  const auto world = detector.world();
  REQUIRE(world.isValid());

  for (const auto &[name, de] : world.children()) {
    auto node = fccvis::geometry::detector::BuildNode(de);

    REQUIRE(node != nullptr);
    REQUIRE(node->Name() == de.name());
    REQUIRE(node->Path() == de.path());

    for (const auto &[childName, childDe] : de.children()) {
      const auto it = std::find_if(
          node->Children().begin(), node->Children().end(),
          [&](const auto &child) { return child->Name() == childDe.name(); });

      REQUIRE(it != node->Children().end());
      REQUIRE((*it)->Path() == childDe.path());
    }
  }
}
