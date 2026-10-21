// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
/**
 * @file backend_contract_tests.cc
 * @brief Contract every backend must satisfy (T1.5), run against each backend.
 *
 * Which backends: FCCVIS_CONTRACT_BACKEND=<type> selects one (CTest registers
 * one test per enabled backend); without it, every registered type except
 * test-only ones ("test-*") is checked. The recording backend is registered
 * here as the reference.
 *
 * Checks (SPEC_BACKENDS "Common acceptance"):
 *  1. InitBackend -> Open draws; Close removes; DetachBackend releases.
 *  2. Revision diffing: a filter, style or camera change redraws only what
 *     changed (checked when the backend exposes DrawStats).
 *  3. Pruned layouts with splits and tabs are accepted.
 *  4. Unsupported content is logged, never crashes.
 *  5. Two instances of the same type coexist.
 * Backend-originated events (pick, camera move, close) are target-specific and
 * tested per backend.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "BackendRegistry.h"
#include "CameraChanges.h"
#include "RecordingBackend.h"
#include "SceneChanges.h"
#include "Session.h"
#include "TestSupport.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace bk = fccvis::backend;
namespace ch = fccvis::changes;
namespace dt = fccvis::data;
using fccvis::session::Session;
using namespace fccvis::test;

namespace {

struct Custom {};

const auto g_reference = bk::RegisterRecordingBackend("recording");

std::vector<std::string> BackendsUnderTest() {
  if (const char* e = std::getenv("FCCVIS_CONTRACT_BACKEND"); e && *e) return {e};
  std::vector<std::string> out;
  for (const auto& n : bk::Registry::Global().Names())
    if (n.rfind("test-", 0) != 0) out.push_back(n);
  return out;
}

bk::Options ContractOptions(const std::string& type, const std::string& id) {
  const auto dir = std::filesystem::temp_directory_path() / ("fccvis-contract-" + type + "-" + id);
  std::filesystem::create_directories(dir);
  return {{"output_dir", dir.string()}, {"width", "160"}, {"height", "120"}};
}

bool Supports(const bk::Capabilities& caps, bk::ElementKind k) { return caps.kinds.count(k) > 0; }

}  // namespace

template <> struct fccvis::style::StyleTraits<Custom> { using type = fccvis::style::PointStyle; };

TEST_CASE("Backend contract", "[contract]") {
  const std::string type = GENERATE(from_range(BackendsUnderTest()));
  INFO("backend type: " << type);
  REQUIRE(bk::Registry::Global().Create(type) != nullptr);

  Session s("contract");
  auto log = CaptureLog(s);
  REQUIRE(s.Data().Add<dt::PointElement>("pts", "fixture",
      std::make_unique<VectorSeries<dt::PointElement>>(
          std::vector<std::vector<dt::PointElement>>{MakePoints(5), MakePoints(3, 10.0)})));
  REQUIRE(s.Data().Add<dt::LineElement>("trk", "fixture",
      std::make_unique<VectorSeries<dt::LineElement>>(
          std::vector<std::vector<dt::LineElement>>{MakeLines(3)})));
  REQUIRE(s.Data().Add<dt::SurfaceElement>("geo", "fixture",
      std::make_unique<VectorSeries<dt::SurfaceElement>>(
          std::vector<std::vector<dt::SurfaceElement>>{MakeSurfaces(2)})));
  s.DataChanged();

  REQUIRE(s.InitBackend(type, "c", ContractOptions(type, "c")).ok);
  const auto caps = *s.BackendCapabilities("c");
  CHECK_FALSE(bk::Registry::Global().Help(type).empty());

  for (const char* n : {"a", "b"}) {
    REQUIRE(s.CreateScene(n));
    REQUIRE(s.Resolve(ch::SetCamera{n, std::make_shared<fccvis::scene::camera::Camera>(n)}).ok);
  }
  REQUIRE(s.Resolve(ch::AddLayer<dt::PointElement>{"a", "hits", {"pts", ""}}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<dt::LineElement>{"a", "tracks", {"trk", ""}}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<dt::SurfaceElement>{"a", "detector", {"geo", ""}}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<dt::PointElement>{"b", "hits", {"pts", ""}, 1}).ok);
  REQUIRE(s.SetLayout(S({P("a"), T({P("b"), P("a")})})).ok);

  // 1. Open draws
  REQUIRE(s.Open("c", "a").ok);
  REQUIRE(s.Flush("c").ok);
  const auto stats0 = s.BackendStats("c");
  if (stats0) {
    CHECK(stats0->Layer("a", "hits") == (Supports(caps, bk::ElementKind::Point) ? 1u : stats0->Layer("a", "hits")));
    CHECK(stats0->Layer("a", "tracks") >= 1);
    CHECK(stats0->Camera("a") >= 1);
  }

  // 2. Revision diffing
  auto energetic = std::make_shared<fccvis::filter::Filter<dt::PointElement>>(
      [](const dt::PointElement& e) { return e.meta.Number("e").value_or(0) > 2.0; });
  REQUIRE(s.Resolve(ch::SetLayerFilter<dt::PointElement>{"a", "hits", energetic}).ok);
  REQUIRE(s.Flush("c").ok);
  if (stats0) {
    const auto st = *s.BackendStats("c");
    CHECK(st.Layer("a", "hits") == stats0->Layer("a", "hits") + 1);
    CHECK(st.Layer("a", "tracks") == stats0->Layer("a", "tracks"));
    CHECK(st.Layer("a", "detector") == stats0->Layer("a", "detector"));
    CHECK(st.Camera("a") == stats0->Camera("a"));
  }

  fccvis::style::LineStyle red;
  red.common.color = fccvis::style::Color::FromHex(0xFF0000);
  const auto before = s.BackendStats("c");
  REQUIRE(s.Resolve(ch::AddStyleRule<dt::LineElement>{"a", "tracks", {}, red}).ok);
  REQUIRE(s.Resolve(ch::ChangeCameraDirection{"a", {1.0, 0.0, 0.0}}).ok);
  REQUIRE(s.Flush("c").ok);
  if (before) {
    const auto st = *s.BackendStats("c");
    CHECK(st.Layer("a", "tracks") == before->Layer("a", "tracks") + 1);
    CHECK(st.Layer("a", "hits") == before->Layer("a", "hits"));
    CHECK(st.Camera("a") == before->Camera("a") + 1);
  }

  // 3. Layout with splits and tabs; second scene
  REQUIRE(s.Open("c", "b").ok);
  REQUIRE(s.Flush("c").ok);
  REQUIRE(s.Close("c", "b").ok);
  CHECK(s.OpenScenes("c") == std::set<std::string>{"a"});

  // 4. Unsupported content: logged, never fatal
  REQUIRE(s.Resolve(ch::AddLayer<Custom>{"a", "odd", {"odd", ""}}).ok);
  REQUIRE(s.Flush("c").ok);
  CHECK(CountLog(*log, ch::Severity::Warning, "layer 'odd'") == 1);
  CHECK(s.IsRunning("c"));

  // 5. A second instance coexists
  REQUIRE(s.InitBackend(type, "c2", ContractOptions(type, "c2")).ok);
  REQUIRE(s.Open("c2", "a").ok);
  REQUIRE(s.Resolve(ch::ChangeCameraMagnification{"a", 2.0}).ok);
  REQUIRE(s.Flush().ok);
  CHECK(s.OpenScenes("c") == std::set<std::string>{"a"});
  CHECK(s.OpenScenes("c2") == std::set<std::string>{"a"});

  // 1. Detach releases
  CHECK(s.DetachBackend("c2").ok);
  CHECK(s.DetachBackend("c").ok);
  CHECK(s.BackendIds().empty());
  CHECK(CountLog(*log, ch::Severity::Error, "") == 0);
}
