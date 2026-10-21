// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
/**
 * @file test_root.cc
 * @brief ROOT classic TEve backend (B1). Needs an X display (run under xvfb-run).
 *
 * Interactions are driven through the public TEve API on the ROOT thread
 * (RootRuntime::Post), the same calls a mouse click or drag ends in:
 * TEveSelection::UserPickedElement and TEvePointSet::PointSelected for picks,
 * TGLCamera::RotateRad for a camera drag, TEveBrowser::CloseWindow for
 * closing the window. Tags: [root] (shared TEve), [root-close] (closes the
 * Eve window; run in its own process), [root-nodisplay] (DISPLAY unset).
 */
#include <catch2/catch_test_macros.hpp>

#include "CameraChanges.h"
#include "RootRuntime.h"
#include "SceneChanges.h"
#include "Session.h"
#include "TestSupport.h"

#include <TEveBrowser.h>
#include <TEveManager.h>
#include <TEvePointSet.h>
#include <TEveScene.h>
#include <TEveSelection.h>
#include <TEveViewer.h>
#include <TGLViewer.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ch = fccvis::changes;
namespace dt = fccvis::data;
namespace fs = std::filesystem;
using fccvis::backend::root::RootRuntime;
using fccvis::session::Session;
using namespace fccvis::test;

namespace {

bool HaveDisplay() {
  const char* d = std::getenv("DISPLAY");
  return d && *d;
}

void Populate(Session& s) {
  REQUIRE(s.Data().Add<dt::PointElement>("pts", "fixture",
      std::make_unique<dt::InMemorySeries<dt::PointElement>>(
          std::vector<std::vector<dt::PointElement>>{MakePoints(5), MakePoints(3, 10.0)})));
  REQUIRE(s.Data().Add<dt::LineElement>("trk", "fixture",
      std::make_unique<dt::InMemorySeries<dt::LineElement>>(std::vector<std::vector<dt::LineElement>>{MakeLines(2)})));
  REQUIRE(s.Data().Add<dt::SurfaceElement>("geo", "fixture",
      std::make_unique<dt::InMemorySeries<dt::SurfaceElement>>(std::vector<std::vector<dt::SurfaceElement>>{MakeSurfaces(1)})));
  s.DataChanged();
  for (const char* n : {"front", "side"}) {
    REQUIRE(s.CreateScene(n));
    REQUIRE(s.Resolve(ch::SetCamera{n, std::make_shared<fccvis::scene::camera::Camera>(n)}).ok);
    REQUIRE(s.Resolve(ch::AddLayer<dt::SurfaceElement>{n, "detector", {"geo", ""}}).ok);
    REQUIRE(s.Resolve(ch::AddLayer<dt::LineElement>{n, "tracks", {"trk", ""}}).ok);
    REQUIRE(s.Resolve(ch::AddLayer<dt::PointElement>{n, "hits", {"pts", ""}}).ok);
  }
  REQUIRE(s.Resolve(ch::ChangeCameraDirection{"side", {1.0, 0.0, 0.0}}).ok);
}

/** Runs @p f on the ROOT thread and waits for it. */
template <typename F>
void OnRoot(F f) {
  RootRuntime::Instance().Post(f);
  RootRuntime::Instance().Wait();
}

/** Polls @p cond for up to five seconds. */
template <typename F>
bool Eventually(F cond) {
  for (int i = 0; i < 500; ++i) {
    if (cond()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return cond();
}

/** First element named @p name in the Eve scene called "<id>: <scene>". */
TEveElement* Find(const std::string& sceneTitle, const std::string& name) {
  TEveElement* found = nullptr;
  for (auto s = gEve->GetScenes()->BeginChildren(); s != gEve->GetScenes()->EndChildren(); ++s) {
    if (std::string((*s)->GetElementName()) != sceneTitle) continue;
    for (auto l = (*s)->BeginChildren(); l != (*s)->EndChildren() && !found; ++l)
      for (auto e = (*l)->BeginChildren(); e != (*l)->EndChildren() && !found; ++e)
        if (std::string((*e)->GetElementName()) == name) found = *e;
  }
  return found;
}

std::size_t PngWidth(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  unsigned char h[24] = {};
  in.read(reinterpret_cast<char*>(h), sizeof h);
  return (std::size_t(h[16]) << 24) | (std::size_t(h[17]) << 16) | (std::size_t(h[18]) << 8) | h[19];
}

}  // namespace

TEST_CASE("ROOT backend refuses to start without a display", "[root-nodisplay]") {
  ::unsetenv("DISPLAY");
  Session s("nodisplay");
  auto log = CaptureLog(s);
  const auto r = s.InitBackend("root", "eve", {});
  CHECK_FALSE(r.ok);
  std::string all;
  for (const auto& m : r.messages) all += m + "\n";
  for (const auto& [sev, m] : *log) all += m + "\n";
  INFO(all);
  CHECK(all.find("DISPLAY") != std::string::npos);
}

TEST_CASE("ROOT backend draws scenes in a layout and writes pictures", "[root]") {
  if (!HaveDisplay()) SKIP("no X display");
  const auto dir = fs::path(FCCVIS_TEST_BINARY_DIR) / "root-out";
  fs::remove_all(dir);
  Session s("root");
  auto log = CaptureLog(s);
  Populate(s);
  REQUIRE(s.SetLayout(S({P("front"), T({P("side")})})).ok);
  REQUIRE(s.InitBackend("root", "eve", {{"picture_dir", dir.string()}, {"width", "320"}, {"height", "200"}}).ok);
  REQUIRE(s.Open("eve", "front").ok);
  REQUIRE(s.Open("eve", "side").ok);
  REQUIRE(s.Flush("eve").ok);

  REQUIRE(fs::exists(dir / "front.png"));
  REQUIRE(fs::exists(dir / "side.png"));
  CHECK(PngWidth(dir / "front.png") == 320);

  std::size_t points = 0, lines = 0, viewers = 0;
  OnRoot([&] {
    for (auto e = gEve->GetViewers()->BeginChildren(); e != gEve->GetViewers()->EndChildren(); ++e) ++viewers;
    if (auto* ps = dynamic_cast<TEvePointSet*>(Find("eve: front", "hits points"))) points = ps->Size();
    if (Find("eve: front", "tracks #0") && Find("eve: front", "tracks #1")) lines = 2;
  });
  CHECK(viewers >= 2);
  CHECK(points == 5);
  CHECK(lines == 2);

  // A filter rebuilds the layer; switching entry too.
  REQUIRE(s.Resolve(ch::SetEntry<dt::PointElement>{"front", "hits", 1}).ok);
  REQUIRE(s.Flush("eve").ok);
  OnRoot([&] {
    if (auto* ps = dynamic_cast<TEvePointSet*>(Find("eve: front", "hits points"))) points = ps->Size();
  });
  CHECK(points == 3);

  REQUIRE(s.Close("eve", "side").ok);
  REQUIRE(s.Flush("eve").ok);
  bool sideGone = false;
  OnRoot([&] { sideGone = Find("eve: side", "hits points") == nullptr; });
  CHECK(sideGone);
  CHECK(s.DetachBackend("eve").ok);
  CHECK(CountLog(*log, ch::Severity::Error, "") == 0);
}

TEST_CASE("ROOT backend reports picks and camera moves", "[root]") {
  if (!HaveDisplay()) SKIP("no X display");
  Session s("events");
  auto log = CaptureLog(s);
  Populate(s);
  std::mutex mutex;
  std::vector<fccvis::backend::PickEvent> picks;
  s.SetPickHandler([&](const std::string& id, const fccvis::backend::PickEvent& e) {
    std::lock_guard<std::mutex> lock(mutex);
    if (id == "ev") picks.push_back(e);
  });
  REQUIRE(s.InitBackend("root", "ev", {}).ok);
  REQUIRE(s.Open("ev", "front").ok);
  REQUIRE(s.Resolve(ch::SetEntry<dt::PointElement>{"front", "hits", 1}).ok);
  REQUIRE(s.Flush("ev").ok);

  SECTION("a picked point reports its element index and entry") {
    OnRoot([] {
      auto* ps = dynamic_cast<TEvePointSet*>(Find("ev: front", "hits points"));
      REQUIRE(ps);
      ps->PointSelected(2);
      gEve->GetSelection()->UserPickedElement(ps, kFALSE);
    });
    REQUIRE(Eventually([&] {
      std::lock_guard<std::mutex> lock(mutex);
      return !picks.empty();
    }));
    std::lock_guard<std::mutex> lock(mutex);
    CHECK(picks.front().scene == "front");
    CHECK(picks.front().layer == "hits");
    CHECK(picks.front().entry == 1);
    CHECK(picks.front().index == 2);
  }

  SECTION("a picked track reports its index") {
    OnRoot([] {
      gEve->GetSelection()->RemoveElements();
      auto* t = Find("ev: front", "tracks #1");
      REQUIRE(t);
      gEve->GetSelection()->UserPickedElement(t, kFALSE);
    });
    REQUIRE(Eventually([&] {
      std::lock_guard<std::mutex> lock(mutex);
      return !picks.empty();
    }));
    std::lock_guard<std::mutex> lock(mutex);
    CHECK(picks.front().layer == "tracks");
    CHECK(picks.front().index == 1);
  }

  SECTION("rotating the viewer's camera changes the scene camera") {
    const auto before = s.FindScene("front")->camera->directionVector;
    OnRoot([] {
      for (auto e = gEve->GetViewers()->BeginChildren(); e != gEve->GetViewers()->EndChildren(); ++e) {
        auto* v = dynamic_cast<TEveViewer*>(*e);
        if (!v || std::string(v->GetElementName()).find("front") == std::string::npos) continue;
        v->GetGLViewer()->CurrentCamera().RotateRad(0.4, 0.3);
      }
    });
    REQUIRE(Eventually([&] { return s.FindScene("front")->camera->directionVector != before; }));
    const auto after = s.FindScene("front")->camera->directionVector;
    REQUIRE(after.size() == 3);
    CHECK(std::abs(after[2]) < 0.999);  // no longer along z
  }
  CHECK(s.DetachBackend("ev").ok);
}

TEST_CASE("Closing the Eve window stops the backend", "[root-close]") {
  if (!HaveDisplay()) SKIP("no X display");
  Session s("close");
  auto log = CaptureLog(s);
  Populate(s);
  REQUIRE(s.InitBackend("root", "eve", {}).ok);
  REQUIRE(s.Open("eve", "front").ok);
  REQUIRE(s.Flush("eve").ok);
  OnRoot([] { gEve->GetBrowser()->CloseWindow(); });
  REQUIRE(Eventually([&] { return !s.IsRunning("eve"); }));
  CHECK(CountLog(*log, ch::Severity::Warning, "the ROOT Eve window was closed") == 1);
  // A new instance cannot start in this process any more, and says why.
  const auto r = s.InitBackend("root", "again", {});
  CHECK_FALSE(r.ok);
  CHECK(s.DetachBackend("eve").ok);
}
