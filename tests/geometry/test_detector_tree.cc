#include <catch2/catch_test_macros.hpp>

#include <DD4hep/Detector.h>

#include <filesystem>
#include <iostream>

namespace {

std::filesystem::path ResourcePath()
{
  return std::filesystem::path("/home/kid-a/scientific-software/installs/k4geo/detectors-data/FCCee/CLD/compact/CLD_o2_v05/CLD_o2_v05.xml");
}

} // namespace

TEST_CASE("CLD detector resource exists",
          "[detector][dd4hep][resource]")
{
  const auto path = ResourcePath();

  REQUIRE(std::filesystem::exists(path));

  INFO("Detector resource: " << path.string());
}

TEST_CASE("DD4hep loads CLD detector",
          "[detector][dd4hep][resource]")
{
  const auto path = ResourcePath();
  std::cout << path.string() << std::endl;
  auto& detector = dd4hep::Detector::getInstance();

  REQUIRE_NOTHROW(
      detector.fromCompact(path.string()));

  const auto world = detector.world();

  REQUIRE(world.isValid());

  INFO("World name: " << world.name());
}

TEST_CASE("DD4hep CLD world has detector hierarchy",
          "[detector][dd4hep][tree]")
{
  const auto path = ResourcePath();

  auto& detector = dd4hep::Detector::getInstance();

  REQUIRE_NOTHROW(
      detector.fromCompact(path.string()));

  const auto world = detector.world();

  REQUIRE(world.isValid());
  REQUIRE(world.children().size() > 0);

  std::cout
      << "\nDD4hep detector hierarchy\n"
      << "=========================\n"
      << "World: " << world.name() << '\n'
      << "Children: " << world.children().size() << '\n';

  for (const auto& [name, child] : world.children()) {
    std::cout
        << "  - " << name
        << " [" << child.type() << "]\n";
  }
}
