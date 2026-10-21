// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T1.1: capability cross-check at Session::Open.
#include <catch2/catch_test_macros.hpp>

#include "CameraChanges.h"
#include "SceneChanges.h"
#include "Session.h"
#include "TestSupport.h"

namespace ch = fccvis::changes;
namespace dt = fccvis::data;
using fccvis::backend::ElementKind;
using fccvis::session::Session;
using namespace fccvis::test;

namespace {
struct Custom {};  // an element type no backend knows
}
// A new element type needs a style record (ARCHITECTURE decision 2).
template <> struct fccvis::style::StyleTraits<Custom> { using type = fccvis::style::PointStyle; };

TEST_CASE("LayerSet exposes the element type of each layer", "[scene][capabilities]") {
  fccvis::scene::Scene sc("a");
  sc.AddLayer<dt::PointElement>("hits", {"pts", ""});
  sc.AddLayer<dt::SurfaceElement>("det", {"geo", ""});
  const auto types = sc.layers.Types();
  REQUIRE(types.size() == 2);
  CHECK(fccvis::backend::KindOf(types.at("hits")) == ElementKind::Point);
  CHECK(fccvis::backend::KindOf(types.at("det")) == ElementKind::Surface);
  CHECK(fccvis::backend::KindOf<dt::LineElement>() == ElementKind::Line);
  CHECK_FALSE(fccvis::backend::KindOf<Custom>().has_value());
}

TEST_CASE("Unsupported element kinds are logged, not fatal, when a scene is opened",
          "[session][capabilities]") {
  Session s("t");
  auto log = CaptureLog(s);
  // A recording backend type whose instances declare points only. The session
  // queries capabilities at InitBackend, so they are set before attaching.
  auto limited = std::make_shared<fccvis::backend::Recording>();
  limited->caps.kinds = {ElementKind::Point};
  fccvis::backend::Registry::Global().Register(
      "test-limited", [limited] { return std::make_unique<fccvis::backend::RecordingBackend>(limited); });
  REQUIRE(s.InitBackend("test-limited", "lim").ok);

  REQUIRE(s.CreateScene("a"));
  REQUIRE(s.Resolve(ch::AddLayer<dt::PointElement>{"a", "hits", {"pts", ""}}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<dt::SurfaceElement>{"a", "det", {"geo", ""}}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<Custom>{"a", "odd", {"odd", ""}}).ok);

  const auto r = s.Open("lim", "a");
  CHECK(r.ok);  // not a failure
  CHECK(s.OpenScenes("lim") == std::set<std::string>{"a"});
  CHECK(CountLog(*log, ch::Severity::Warning, "layer 'det': element kind 'surface' is not supported") == 1);
  CHECK(CountLog(*log, ch::Severity::Warning, "layer 'odd': element type is not a built-in kind") == 1);
  CHECK(CountLog(*log, ch::Severity::Warning, "layer 'hits'") == 0);

  SECTION("each problem is reported once, and again for layers added later") {
    REQUIRE(s.Resolve(ch::SetEntry<dt::PointElement>{"a", "hits", 0}).ok);
    CHECK(CountLog(*log, ch::Severity::Warning, "layer 'det'") == 1);
    REQUIRE(s.Resolve(ch::AddLayer<dt::LineElement>{"a", "tracks", {"trk", ""}}).ok);
    CHECK(CountLog(*log, ch::Severity::Warning, "layer 'tracks': element kind 'line'") == 1);
  }
}

TEST_CASE("Open and Close fail cleanly for an unknown backend id", "[session][backend]") {
  Session s("t");
  auto log = CaptureLog(s);
  REQUIRE(s.CreateScene("a"));
  CHECK_FALSE(s.Open("nobody", "a").ok);
  CHECK_FALSE(s.Close("nobody", "a").ok);
  CHECK(CountLog(*log, ch::Severity::Error, "unknown backend 'nobody'") == 2);
}
