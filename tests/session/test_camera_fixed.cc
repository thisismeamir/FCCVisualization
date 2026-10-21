// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T1.3: CameraMode::Fixed enforcement; the origin reaches Change::Apply.
#include <catch2/catch_test_macros.hpp>

#include "CameraChanges.h"
#include "SceneChanges.h"
#include "Session.h"
#include "TestSupport.h"

namespace ch = fccvis::changes;
namespace cam = fccvis::scene::camera;
using fccvis::session::Session;
using namespace fccvis::test;

namespace {
void Fix(Session& s, const std::string& scene) {
  cam::CameraOptions o;
  o.mode = cam::CameraMode::Fixed;
  REQUIRE(s.Resolve(ch::SetCameraOptions{scene, o}).ok);
}
}  // namespace

TEST_CASE("A backend may not move the camera of a fixed scene", "[session][camera][fixed]") {
  Session s("t");
  auto log = CaptureLog(s);
  auto rec = AttachRecording(s, "gl");
  auto c = std::make_shared<cam::Camera>("c");
  REQUIRE(s.CreateScene("a"));
  REQUIRE(s.Resolve(ch::SetCamera{"a", c}).ok);
  REQUIRE(s.Open("gl", "a").ok);
  Fix(s, "a");

  SECTION("every camera change from the backend is rejected, nothing moves") {
    CHECK_FALSE(rec->port->Submit(ch::ChangeCameraMagnification{"a", 2.0}).ok);
    CHECK_FALSE(rec->port->Submit(ch::ChangeCameraDirection{"a", {1, 0, 0}}).ok);
    CHECK_FALSE(rec->port->Submit(ch::ChangeCameraPosition{"a", {1, 2, 3}}).ok);
    CHECK_FALSE(rec->port->Submit(ch::ChangeCamera{"a", std::nullopt, std::nullopt, 5.0}).ok);
    CHECK_FALSE(rec->port->Submit(ch::ResetCameraMagnification{"a"}).ok);
    CHECK_FALSE(rec->port->Submit(ch::SetCamera{"a", std::make_shared<cam::Camera>()}).ok);
    CHECK(c->magnification == 1.0);
    CHECK(c->directionVector == std::vector<double>{0, 0, 1});
    CHECK(s.FindScene("a")->camera == c);
    CHECK(CountLog(*log, ch::Severity::Error, "camera of scene 'a' is fixed") == 6);
  }

  SECTION("the rejecting backend is told to redraw the session's camera") {
    const auto before = rec->SyncCount();
    CHECK_FALSE(rec->port->Submit(ch::ChangeCameraMagnification{"a", 2.0}).ok);
    REQUIRE(rec->SyncCount() == before + 1);
    const auto last = rec->LastSync();
    CHECK(last.origin.empty());
    CHECK(ch::Has(last.dirty.at("a").parts, ch::Part::Camera));
  }

  SECTION("user and API changes still apply") {
    CHECK(s.Resolve(ch::ChangeCameraMagnification{"a", 2.0}).ok);
    CHECK(c->magnification == 2.0);
  }

  SECTION("switching back to Free re-enables interaction") {
    cam::CameraOptions o;
    o.mode = cam::CameraMode::Free;
    REQUIRE(s.Resolve(ch::SetCameraOptions{"a", o}).ok);
    CHECK(rec->port->Submit(ch::ChangeCameraMagnification{"a", 3.0}).ok);
    CHECK(c->magnification == 3.0);
  }
}

TEST_CASE("A camera shared with a fixed scene is locked for backends", "[session][camera][fixed]") {
  Session s("t");
  auto log = CaptureLog(s);
  auto rec = AttachRecording(s, "gl");
  auto c = std::make_shared<cam::Camera>("shared");
  for (const char* n : {"fixed", "free"}) {
    REQUIRE(s.CreateScene(n));
    REQUIRE(s.Resolve(ch::SetCamera{n, c}).ok);
  }
  Fix(s, "fixed");
  REQUIRE(s.Open("gl", "free").ok);
  const auto r = rec->port->Submit(ch::ChangeCameraMagnification{"free", 2.0});
  CHECK_FALSE(r.ok);
  CHECK(r.messages.at(0).find("(shared camera)") != std::string::npos);
  CHECK(c->magnification == 1.0);
}
