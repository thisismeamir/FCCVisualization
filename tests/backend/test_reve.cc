// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
/**
 * @file test_reve.cc
 * @brief ROOT REve web backend (B1, T4.5), driven by a headless browser.
 *
 * The page is loaded in headless Chromium (Playwright, tests/backend/reve_drive.mjs):
 * the viewer renders, a click in its centre reports a pick of the point
 * there, and closing the page stops the backend.
 */
#include <catch2/catch_test_macros.hpp>

#include "SceneChanges.h"
#include "Session.h"
#include "TestSupport.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

namespace ch = fccvis::changes;
namespace dt = fccvis::data;
namespace fs = std::filesystem;
using fccvis::session::Session;
using namespace fccvis::test;

namespace {

template <typename F>
bool Eventually(F cond, int ms = 10000) {
  for (int i = 0; i < ms / 10; ++i) {
    if (cond()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return cond();
}

}  // namespace

TEST_CASE("REve backend serves the scenes, picks and closing come back", "[reve]") {
  const char* node = std::getenv("FCCVIS_NODE");
  if (!node || !*node) SKIP("FCCVIS_NODE is not set");
  Session s("reve");
  auto log = CaptureLog(s);
  std::vector<dt::PointElement> grid;  // a dense grid in the xy plane, so the centre of the view hits a point
  for (int i = -20; i <= 20; ++i)
    for (int j = -20; j <= 20; ++j) {
      dt::PointElement p;
      p.geometry.position = {10.0 * i, 10.0 * j, 0.0};
      grid.push_back(p);
    }
  REQUIRE(s.Data().Add<dt::PointElement>("grid", "fixture",
      std::make_unique<dt::InMemorySeries<dt::PointElement>>(std::vector<std::vector<dt::PointElement>>{grid, grid})));
  REQUIRE(s.Data().Add<dt::LineElement>("trk", "fixture",
      std::make_unique<dt::InMemorySeries<dt::LineElement>>(std::vector<std::vector<dt::LineElement>>{MakeLines(2)})));
  s.DataChanged();
  REQUIRE(s.CreateScene("front"));
  REQUIRE(s.Resolve(ch::SetCamera{"front", std::make_shared<fccvis::scene::camera::Camera>("front")}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<dt::PointElement>{"front", "hits", {"grid", ""}, 1}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<dt::LineElement>{"front", "tracks", {"trk", ""}}).ok);
  std::mutex mutex;
  std::vector<fccvis::backend::PickEvent> picks;
  s.SetPickHandler([&](const std::string&, const fccvis::backend::PickEvent& e) {
    std::lock_guard<std::mutex> lock(mutex);
    picks.push_back(e);
  });
  REQUIRE(s.InitBackend("reve", "web", {}).ok);
  REQUIRE(s.Open("web", "front").ok);
  REQUIRE(s.Flush("web").ok);
  std::string url;
  for (const auto& [sev, m] : *log)
    if (auto p = m.find("REve page for 'web': "); p != std::string::npos) url = m.substr(p + 21);
  REQUIRE(url.rfind("http://", 0) == 0);

  const auto dir = fs::path(FCCVIS_TEST_BINARY_DIR) / "reve-out";
  fs::create_directories(dir);
  const std::string cmd = std::string(node) + " " + FCCVIS_TEST_SOURCE_DIR + "/backend/reve_drive.mjs '" + url + "' " +
                          (dir / "reve.png").string() + " > " + (dir / "drive.json").string() + " 2>&1";
  REQUIRE(std::system(cmd.c_str()) == 0);
  std::ifstream in(dir / "drive.json");
  std::stringstream out;
  out << in.rdbuf();
  INFO(out.str());
  CHECK(out.str().find("\"errors\":[]") != std::string::npos);

  REQUIRE(Eventually([&] {
    std::lock_guard<std::mutex> lock(mutex);
    return !picks.empty();
  }));
  {
    std::lock_guard<std::mutex> lock(mutex);
    CHECK(picks.front().scene == "front");
    CHECK(picks.front().layer == "hits");
    CHECK(picks.front().entry == 1);
    CHECK(picks.front().index < grid.size());
    const auto& p = grid[picks.front().index].geometry.position;
    CHECK(std::abs(p[0]) <= 40);  // near the centre of the view
    CHECK(std::abs(p[1]) <= 40);
  }
  CHECK(Eventually([&] { return !s.IsRunning("web"); }));  // the page was closed
  CHECK(CountLog(*log, ch::Severity::Warning, "the browser closed the REve page") == 1);
  CHECK(CountLog(*log, ch::Severity::Error, "") == 0);
}
