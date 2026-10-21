// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
/**
 * @file backend_matrix_tests.cc
 * @brief Backend test matrix (T4.8, T-BT): one scripted scenario run against
 *        every enabled backend through the shell interpreter.
 *
 * Scenario: create scenes, 3 layers, filter, style rule, camera change,
 * split layout, close a pane. Contract-level outcomes are asserted for every
 * backend: each command succeeds, nothing is logged as an error, the backend
 * keeps running, and (when the backend reports DrawStats) each step redraws
 * exactly what it touched. Target-specific outputs are checked by each
 * backend's own tests. Which backend: FCCVIS_MATRIX_BACKEND=<type> (CTest
 * registers matrix::<type> per enabled backend); otherwise every registered
 * non-test backend.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "BackendRegistry.h"
#include "RecordingBackend.h"
#include "TestSupport.h"
#include "fccvis/api/Interpreter.h"

#include <cstdlib>
#include <filesystem>

namespace api = fccvis::api;
namespace bk = fccvis::backend;
namespace ch = fccvis::changes;
namespace dt = fccvis::data;
using fccvis::session::Session;
using namespace fccvis::test;

namespace {

const auto g_reference = bk::RegisterRecordingBackend("recording");

std::vector<std::string> Backends() {
  if (const char* e = std::getenv("FCCVIS_MATRIX_BACKEND"); e && *e) return {e};
  std::vector<std::string> out;
  for (const auto& n : bk::Registry::Global().Names())
    if (n.rfind("test-", 0) != 0) out.push_back(n);
  return out;
}

}  // namespace

TEST_CASE("Backend matrix scenario", "[matrix]") {
  const std::string type = GENERATE(from_range(Backends()));
  INFO("backend type: " << type);

  Session s("matrix");
  api::Interpreter in(s);
  std::vector<std::string> errors;
  in.SetLogSink([&](ch::Severity sev, const std::string& m) {
    if (sev == ch::Severity::Error) errors.push_back(m);
  });
  REQUIRE(s.Data().Add<dt::PointElement>("pts", "matrix",
      std::make_unique<dt::InMemorySeries<dt::PointElement>>(std::vector<std::vector<dt::PointElement>>{MakePoints(6)})));
  REQUIRE(s.Data().Add<dt::LineElement>("trk", "matrix",
      std::make_unique<dt::InMemorySeries<dt::LineElement>>(std::vector<std::vector<dt::LineElement>>{MakeLines(3)})));
  REQUIRE(s.Data().Add<dt::SurfaceElement>("geo", "matrix",
      std::make_unique<dt::InMemorySeries<dt::SurfaceElement>>(std::vector<std::vector<dt::SurfaceElement>>{MakeSurfaces(2)})));
  s.DataChanged();

  const auto dir = std::filesystem::temp_directory_path() / ("fccvis-matrix-" + type);
  std::filesystem::remove_all(dir);
  auto run = [&](const std::string& line) {
    const auto r = in.Execute(line);
    INFO(line << "\n" << r.Text());
    REQUIRE(r.ok);
  };
  auto stats = [&] { return s.BackendStats("m"); };

  run("scene new a b");
  run("layer add a pts");
  run("layer add a trk");
  run("layer add a geo");
  run("layer add b pts");
  std::string init = "backend init " + type + " m";
  const auto caps = bk::Registry::Global().Create(type)->GetCapabilities();
  if (caps.options.count("output_dir")) init += " output_dir=" + dir.string();
  if (caps.options.count("width")) init += " width=120 height=90";
  run(init);
  run("open m a b");
  run("backend flush m");
  const auto s0 = stats();

  run("where a/pts e > 2");
  run("backend flush m");
  const auto s1 = stats();
  run("style a/trk color=red width=2");
  run("backend flush m");
  const auto s2 = stats();
  run("camera a dir 1 0 0");
  run("backend flush m");
  const auto s3 = stats();
  run("layout split(a as \"A\" *2, b)");
  run("backend flush m");
  const auto s4 = stats();
  run("close m b");
  run("backend flush m");

  if (s0) {
    INFO("revision diffing");
    CHECK(s1->Layer("a", "pts") == s0->Layer("a", "pts") + 1);
    CHECK(s1->Layer("a", "trk") == s0->Layer("a", "trk"));
    CHECK(s1->Layer("b", "pts") == s0->Layer("b", "pts"));
    CHECK(s2->Layer("a", "trk") == s1->Layer("a", "trk") + 1);
    CHECK(s2->Layer("a", "pts") == s1->Layer("a", "pts"));
    CHECK(s3->Camera("a") == s2->Camera("a") + 1);
    CHECK(s3->Camera("b") == s2->Camera("b"));
    CHECK(s3->Layer("a", "geo") == s2->Layer("a", "geo"));
    CHECK(s4->layouts == s3->layouts + 1);
  }
  CHECK(s.OpenScenes("m") == std::set<std::string>{"a"});
  CHECK(s.IsRunning("m"));
  if (caps.options.count("output_dir")) CHECK(!std::filesystem::is_empty(dir));
  run("backend rm m");
  CHECK(errors.empty());
}
