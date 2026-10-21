// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_message.hpp>

#include <DD4hep/Detector.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace {

// Compact file from the environment (FCCVIS_TEST_COMPACT).
std::filesystem::path ResourcePath()
{
  const char* env = std::getenv("FCCVIS_TEST_COMPACT");
  return env ? std::filesystem::path(env) : std::filesystem::path{};
}

} // namespace

TEST_CASE("CLD detector resource exists",
          "[detector][dd4hep][resource]")
{
  if (ResourcePath().empty()) SKIP("FCCVIS_TEST_COMPACT is not set");
  const auto path = ResourcePath();

  REQUIRE(std::filesystem::exists(path));

  INFO("Detector resource: " << path.string());
}

TEST_CASE("DD4hep loads CLD detector",
          "[detector][dd4hep][resource]")
{
  if (ResourcePath().empty()) SKIP("FCCVIS_TEST_COMPACT is not set");
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
  if (ResourcePath().empty()) SKIP("FCCVIS_TEST_COMPACT is not set");
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
