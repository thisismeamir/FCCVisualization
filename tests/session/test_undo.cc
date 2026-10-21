// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T5.7: undo/redo (Change::Inverse, Session::Undo/Redo, groups).
#include <catch2/catch_test_macros.hpp>

#include "CameraChanges.h"
#include "SceneChanges.h"
#include "Session.h"
#include "TestSupport.h"

#include <sstream>

namespace ch = fccvis::changes;
namespace cam = fccvis::scene::camera;
namespace dt = fccvis::data;
namespace st = fccvis::style;
using fccvis::session::Session;
using namespace fccvis::test;

namespace {

/** Everything undo must restore, as text. */
std::string Dump(const Session& s) {
  std::ostringstream out;
  for (const auto& name : s.SceneNames()) {
    const auto sc = s.FindScene(name);
    out << "scene " << name << " camera=" << sc->camera.get();
    if (sc->camera)
      out << " dir=" << sc->camera->directionVector[0] << "," << sc->camera->directionVector[1] << ","
          << sc->camera->directionVector[2] << " pos=" << sc->camera->positionVector[0] << " mag="
          << sc->camera->magnification;
    out << " mode=" << (sc->cameraOptions.mode ? static_cast<int>(*sc->cameraOptions.mode) : -1)
        << " axes=" << (sc->environment.axes ? int(*sc->environment.axes) : -1) << "\n";
    for (const auto& layer : sc->layers.Names()) {
      const auto* l = sc->layers.Find<dt::PointElement>(layer);
      out << "  layer " << layer;
      if (l)
        out << " series=" << l->series->name << " entry=" << l->entry.value_or(0) << " filter=" << l->filter.get()
            << " rules=" << l->styles.rules.size();
      out << "\n";
    }
  }
  out << "layout " << Describe(s.Options().sceneLayout) << "\n";
  return out.str();
}

void Populate(Session& s) {
  REQUIRE(s.Data().Add<dt::PointElement>("pts", "fixture",
      std::make_unique<dt::InMemorySeries<dt::PointElement>>(
          std::vector<std::vector<dt::PointElement>>{MakePoints(3), MakePoints(2)})));
  s.DataChanged();
  REQUIRE(s.CreateScene("a"));
  REQUIRE(s.Resolve(ch::SetCamera{"a", std::make_shared<cam::Camera>("a")}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<dt::PointElement>{"a", "hits", {"pts", ""}}).ok);
}

/** Applies @p change, undoes it, redoes it, undoes it again; checks the states. */
void RoundTrip(Session& s, const ch::Change& change) {
  INFO(change.Describe());
  const std::string before = Dump(s);
  const auto depth = s.UndoDepth();
  REQUIRE(s.Resolve(change).ok);
  const std::string after = Dump(s);
  CHECK(after != before);
  CHECK(s.UndoDepth() == depth + 1);
  CHECK(s.UndoLabel() == change.Describe());
  REQUIRE(s.Undo().ok);
  CHECK(Dump(s) == before);
  CHECK(s.RedoDepth() == 1);
  REQUIRE(s.Redo().ok);
  CHECK(Dump(s) == after);
  REQUIRE(s.Undo().ok);
  CHECK(Dump(s) == before);
}

}  // namespace

TEST_CASE("Every change of the catalogue can be undone and redone", "[session][undo]") {
  Session s("undo");
  auto log = CaptureLog(s);
  Populate(s);
  auto f = std::make_shared<fccvis::filter::Filter<dt::PointElement>>([](const dt::PointElement&) { return false; });
  st::PointStyle red;
  red.common.color = st::Color::FromHex(0xFF0000);
  cam::CameraOptions fixed;
  fixed.mode = cam::CameraMode::Fixed;
  st::EnvironmentStyle axes;
  axes.axes = true;

  RoundTrip(s, ch::CreateScene{"b"});
  RoundTrip(s, ch::RemoveScene{"a"});
  RoundTrip(s, ch::AddLayer<dt::PointElement>{"a", "more", {"pts", ""}, 1});
  RoundTrip(s, ch::RemoveLayer{"a", "hits"});
  RoundTrip(s, ch::SetEntry<dt::PointElement>{"a", "hits", 1});
  RoundTrip(s, ch::SetLayerFilter<dt::PointElement>{"a", "hits", f});
  RoundTrip(s, ch::AddStyleRule<dt::PointElement>{"a", "hits", {}, red});
  RoundTrip(s, ch::SetCamera{"a", std::make_shared<cam::Camera>("other")});
  RoundTrip(s, ch::SetCameraOptions{"a", fixed});
  RoundTrip(s, ch::SetEnvironment{"a", axes});
  RoundTrip(s, ch::ChangeCameraDirection{"a", {1, 0, 0}});
  RoundTrip(s, ch::ChangeCameraPosition{"a", {5, 0, 0}});
  RoundTrip(s, ch::ChangeCameraMagnification{"a", 3});
  RoundTrip(s, ch::ChangeCamera{"a", std::vector<double>{1, 1, 1}, std::vector<double>{0, 1, 0}, 2.0});
  RoundTrip(s, ch::SetLayout{P("a")});
  CHECK(CountLog(*log, ch::Severity::Error, "") == 0);
}

TEST_CASE("Undo history: batches, groups, limits and what is not recorded", "[session][undo]") {
  Session s("undo");
  auto log = CaptureLog(s);
  Populate(s);
  const std::string start = Dump(s);
  const auto startDepth = s.UndoDepth();

  SECTION("a batch is one step; a failing batch records what it applied") {
    const ch::ChangeCameraMagnification zoom{"a", 2};
    const ch::CreateScene dup{"a"};  // fails: exists
    const ch::Change* batch[] = {&zoom, &dup};
    CHECK_FALSE(s.Resolve(std::span<const ch::Change* const>(batch)).ok);
    CHECK(s.UndoDepth() == startDepth + 1);
    REQUIRE(s.Undo().ok);
    CHECK(Dump(s) == start);
  }

  SECTION("a group is one step, undone in reverse order") {
    s.BeginUndoGroup("two scenes and a layout");
    REQUIRE(s.CreateScene("b"));
    REQUIRE(s.CreateScene("c"));
    REQUIRE(s.SetLayout(S({P("a"), P("b"), P("c")})).ok);
    s.EndUndoGroup();
    CHECK(s.UndoDepth() == startDepth + 1);
    CHECK(s.UndoLabel() == "two scenes and a layout");
    REQUIRE(s.Undo().ok);
    CHECK(Dump(s) == start);
    REQUIRE(s.Redo().ok);
    CHECK(s.SceneNames().size() == 3);
  }

  SECTION("a new change after undo drops the redo history") {
    REQUIRE(s.Resolve(ch::ChangeCameraMagnification{"a", 2}).ok);
    REQUIRE(s.Undo().ok);
    CHECK(s.RedoDepth() == 1);
    REQUIRE(s.Resolve(ch::ChangeCameraMagnification{"a", 4}).ok);
    CHECK(s.RedoDepth() == 0);
    CHECK_FALSE(s.Redo().ok);
  }

  SECTION("changes from backends are not recorded and do not clear the history") {
    auto rec = AttachRecording(s, "gl");
    REQUIRE(s.Open("gl", "a").ok);
    const auto depth = s.UndoDepth();
    REQUIRE(rec->port->Submit(ch::ChangeCameraDirection{"a", {0, 1, 0}}).ok);
    CHECK(s.UndoDepth() == depth);
    // Undo of an earlier user change does not touch the camera moved by the backend.
    REQUIRE(s.Resolve(ch::SetEntry<dt::PointElement>{"a", "hits", 1}).ok);
    REQUIRE(s.Undo().ok);
    CHECK(s.FindScene("a")->camera->directionVector == std::vector<double>{0, 1, 0});
    // The backend is told to redraw what undo changed.
    const auto last = rec->LastSync();
    CHECK(last.dirty.at("a").layers.count("hits") == 1);
  }

  SECTION("a change without inverse clears the history") {
    struct Opaque final : ch::Change {
      std::string Name() const override { return "Opaque"; }
      std::string Describe() const override { return "Opaque"; }
      ch::Footprint Touches() const override { return ch::Footprint::Of({}, ch::Part::None); }
      void Apply(ch::Workspace&) const override {}
      std::shared_ptr<const ch::Change> Inverse(const ch::Workspace&) const override { return nullptr; }
    };
    REQUIRE(s.Resolve(ch::ChangeCameraMagnification{"a", 2}).ok);
    REQUIRE(s.Resolve(Opaque{}).ok);
    CHECK(s.UndoDepth() == 0);
    CHECK_FALSE(s.Undo().ok);
  }

  SECTION("the history is bounded") {
    s.SetUndoLimit(3);
    for (int i = 0; i < 5; ++i) REQUIRE(s.Resolve(ch::ChangeCameraMagnification{"a", 2.0 + i}).ok);
    CHECK(s.UndoDepth() == 3);
    while (s.UndoDepth() > 0) REQUIRE(s.Undo().ok);
    CHECK(s.FindScene("a")->camera->magnification == 3.0);  // the two oldest steps were dropped
  }

  SECTION("a shared camera stays shared") {
    REQUIRE(s.CreateScene("b"));
    REQUIRE(s.Resolve(ch::SetCamera{"b", s.FindScene("a")->camera}).ok);
    REQUIRE(s.Resolve(ch::ChangeCameraMagnification{"a", 5}).ok);
    REQUIRE(s.Undo().ok);
    CHECK(s.FindScene("b")->camera == s.FindScene("a")->camera);
    CHECK(s.FindScene("b")->camera->magnification == 1.0);
  }

  SECTION("removing a scene restores its panes on undo") {
    REQUIRE(s.CreateScene("b"));
    REQUIRE(s.SetLayout(S({P("a"), P("b")})).ok);
    REQUIRE(s.Resolve(ch::RemoveScene{"b"}).ok);
    CHECK(Describe(s.Options().sceneLayout) == Describe(std::optional{P("a")}));
    REQUIRE(s.Undo().ok);
    CHECK(Describe(s.Options().sceneLayout) == Describe(std::optional{S({P("a"), P("b")})}));
  }
  CHECK(CountLog(*log, ch::Severity::Error, "Restore") == 0);  // undo itself never fails here
}
