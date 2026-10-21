// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
/**
 * @file test_ced.cc
 * @brief CED backend (B4, T4.7): the wire protocol against a mock server, and
 *        the real glced server when one is available.
 *
 * [ced] runs a small TCP server in the test that decodes what the backend
 * sends (CedProtocol.h), answers with a picked object id and closes the
 * connection. [ced-glced] starts glced (FCCVIS_GLCED, needs an X display),
 * sends a scene, double-clicks on a hit with xdotool and expects the pick.
 */
#include <catch2/catch_test_macros.hpp>

#include "CedProtocol.h"
#include "SceneChanges.h"
#include "Session.h"
#include "TestSupport.h"

#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

namespace ch = fccvis::changes;
namespace dt = fccvis::data;
namespace ced = fccvis::backend::ced;
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

/** What the mock server decoded from one event. */
struct Received {
  std::vector<ced::Hit> hits;
  std::vector<ced::Line> lines;
  std::vector<ced::LayerText> texts;
  int draws = 0;
};

/** Accepts one client, decodes messages until the first DRAW_EVENT, then sends @p pick and closes. */
class MockCed {
 public:
  MockCed() {
    m_listen = ::socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    ::setsockopt(m_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = 0;
    REQUIRE(::bind(m_listen, reinterpret_cast<sockaddr*>(&a), sizeof a) == 0);
    REQUIRE(::listen(m_listen, 1) == 0);
    socklen_t len = sizeof a;
    ::getsockname(m_listen, reinterpret_cast<sockaddr*>(&a), &len);
    port = ntohs(a.sin_port);
  }
  ~MockCed() {
    if (m_thread.joinable()) m_thread.join();
    ::close(m_listen);
  }
  void Serve(std::int32_t pick) {
    m_thread = std::thread([this, pick] {
      const int fd = ::accept(m_listen, nullptr, nullptr);
      if (fd < 0) return;
      auto readAll = [&](void* buf, std::size_t n) {
        std::size_t got = 0;
        while (got < n) {
          const ssize_t r = ::recv(fd, static_cast<char*>(buf) + got, n - got, 0);
          if (r <= 0) return false;
          got += static_cast<std::size_t>(r);
        }
        return true;
      };
      while (true) {
        std::int32_t size = 0;
        std::uint32_t type = 0;
        if (!readAll(&size, 4) || !readAll(&type, 4)) break;
        std::vector<char> body(static_cast<std::size_t>(size - 8));
        if (!body.empty() && !readAll(body.data(), body.size())) break;
        std::lock_guard<std::mutex> lock(mutex);
        if (type == ced::kDrawEvent) {
          ++got.draws;
          break;
        }
        auto take = [&](auto& v) {
          using T = typename std::decay_t<decltype(v)>::value_type;
          v.resize(body.size() / sizeof(T));
          std::memcpy(v.data(), body.data(), v.size() * sizeof(T));
        };
        if (type == ced::kHit) take(got.hits);
        else if (type == ced::kLine) take(got.lines);
        else if (type == ced::kLayerText) take(got.texts);
      }
      ::send(fd, &pick, sizeof pick, 0);
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      ::close(fd);
    });
  }
  int port = 0;
  std::mutex mutex;
  Received got;

 private:
  int m_listen = -1;
  std::thread m_thread;
};

void Populate(Session& s) {
  REQUIRE(s.Data().Add<dt::PointElement>("pts", "fixture",
      std::make_unique<dt::InMemorySeries<dt::PointElement>>(std::vector<std::vector<dt::PointElement>>{MakePoints(3), MakePoints(4)})));
  REQUIRE(s.Data().Add<dt::LineElement>("trk", "fixture",
      std::make_unique<dt::InMemorySeries<dt::LineElement>>(std::vector<std::vector<dt::LineElement>>{MakeLines(2)})));
  s.DataChanged();
  REQUIRE(s.CreateScene("front"));
  REQUIRE(s.Resolve(ch::SetCamera{"front", std::make_shared<fccvis::scene::camera::Camera>("front")}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<dt::PointElement>{"front", "hits", {"pts", ""}, 1}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<dt::LineElement>{"front", "tracks", {"trk", ""}}).ok);
}

}  // namespace

TEST_CASE("CED backend speaks the CED protocol, picks and closing come back", "[ced]") {
  MockCed server;
  // ids are given in drawing order: layer "hits" first (points 1..4), then "tracks" (5, 6)
  server.Serve(2);
  Session s("ced");
  auto log = CaptureLog(s);
  Populate(s);
  std::mutex mutex;
  std::vector<fccvis::backend::PickEvent> picks;
  s.SetPickHandler([&](const std::string&, const fccvis::backend::PickEvent& e) {
    std::lock_guard<std::mutex> lock(mutex);
    picks.push_back(e);
  });
  REQUIRE(s.InitBackend("ced", "c", {{"port", std::to_string(server.port)}, {"host", "127.0.0.1"}}).ok);
  REQUIRE(s.Open("c", "front").ok);
  REQUIRE(s.Flush("c").ok);

  REQUIRE(Eventually([&] {
    std::lock_guard<std::mutex> lock(server.mutex);
    return server.got.draws == 1;
  }));
  {
    std::lock_guard<std::mutex> lock(server.mutex);
    REQUIRE(server.got.hits.size() == 4);
    CHECK(server.got.hits[0].layer == 0);
    CHECK(server.got.hits[0].id == 1);
    CHECK(server.got.hits[1].x == static_cast<float>(MakePoints(4)[1].geometry.position[0]));
    CHECK(server.got.lines.size() >= 2);  // two polylines, split into segments
    CHECK(server.got.lines[0].layer == 1);
    REQUIRE(server.got.texts.size() == 2);
    CHECK(std::string(server.got.texts[0].text) == "hits");
    CHECK(std::string(server.got.texts[1].text) == "tracks");
    CHECK(server.got.texts[1].layer == 1);
  }
  REQUIRE(Eventually([&] {
    std::lock_guard<std::mutex> lock(mutex);
    return !picks.empty();
  }));
  {
    std::lock_guard<std::mutex> lock(mutex);
    CHECK(picks[0].scene == "front");
    CHECK(picks[0].layer == "hits");
    CHECK(picks[0].entry == 1);
    CHECK(picks[0].index == 1);
  }
  CHECK(Eventually([&] { return !s.IsRunning("c"); }));
  CHECK(CountLog(*log, ch::Severity::Warning, "CED closed the connection") == 1);
}

TEST_CASE("CED backend waits for an absent server and checks its options", "[ced]") {
  Session s("ced");
  auto log = CaptureLog(s);
  Populate(s);
  CHECK_FALSE(s.InitBackend("ced", "bad", {{"port", "x"}}).ok);
  REQUIRE(s.InitBackend("ced", "c", {{"port", "1"}, {"host", "127.0.0.1"}}).ok);  // nothing listens there
  REQUIRE(s.Open("c", "front").ok);
  REQUIRE(s.Flush("c").ok);
  REQUIRE(s.Resolve(ch::SetEntry<dt::PointElement>{"front", "hits", 0}).ok);
  REQUIRE(s.Flush("c").ok);
  CHECK(CountLog(*log, ch::Severity::Warning, "cannot reach CED at 127.0.0.1:1") == 1);  // warned once
  CHECK(s.IsRunning("c"));
}

TEST_CASE("CED backend with the real glced server", "[ced-glced]") {
  const char* glced = std::getenv("FCCVIS_GLCED");
  const char* display = std::getenv("DISPLAY");
  if (!glced || !*glced || !display || !*display) SKIP("FCCVIS_GLCED or DISPLAY is not set");
  namespace fs = std::filesystem;
  const auto dir = fs::path(FCCVIS_TEST_BINARY_DIR) / "ced-glced";
  fs::create_directories(dir);
  const int port = 20000 + static_cast<int>(::getpid() % 20000);
  const std::string start = "cd " + dir.string() + "; HOME=" + dir.string() + " CED_PORT=" + std::to_string(port) + " " +
                            glced + " > glced.log 2>&1 & echo $! > glced.pid";
  REQUIRE(std::system(start.c_str()) == 0);
  struct Kill {
    fs::path pid;
    ~Kill() { std::system(("kill $(cat " + pid.string() + ") 2>/dev/null").c_str()); }
  } kill{dir / "glced.pid"};
  std::this_thread::sleep_for(std::chrono::seconds(2));

  Session s("glced");
  auto log = CaptureLog(s);
  std::vector<dt::PointElement> one(1);  // one big hit at the origin, the centre of glced's view
  one[0].geometry.position = {0, 0, 0};
  REQUIRE(s.Data().Add<dt::PointElement>("pt", "fixture",
      std::make_unique<dt::InMemorySeries<dt::PointElement>>(std::vector<std::vector<dt::PointElement>>{one})));
  s.DataChanged();
  REQUIRE(s.CreateScene("e"));
  REQUIRE(s.Resolve(ch::AddLayer<dt::PointElement>{"e", "hit", {"pt", ""}}).ok);
  fccvis::style::PointStyle big;
  big.size = 30;
  big.marker = fccvis::style::MarkerShape::Circle;  // CED point: size in pixels (box sizes are in mm)
  REQUIRE(s.Resolve(ch::AddStyleRule<dt::PointElement>{"e", "hit", {}, big}).ok);
  std::mutex mutex;
  std::vector<fccvis::backend::PickEvent> picks;
  s.SetPickHandler([&](const std::string&, const fccvis::backend::PickEvent& e) {
    std::lock_guard<std::mutex> lock(mutex);
    picks.push_back(e);
  });
  REQUIRE(s.InitBackend("ced", "c", {{"port", std::to_string(port)}}).ok);
  REQUIRE(s.Open("c", "e").ok);
  REQUIRE(s.Flush("c").ok);
  CHECK(CountLog(*log, ch::Severity::Info, "connected to CED") == 1);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::system(("import -window root " + (dir / "glced.png").string() + " 2>/dev/null").c_str());
  // Double click on the hit: glced projects the origin to (249, 249) of its 500x500 window
  // and picks only within a few pixels of a hit.
  // The first click may only focus the window, so try a few times.
  bool picked = false;
  for (int attempt = 0; attempt < 4 && !picked; ++attempt) {
    const int rc = std::system(
        "W=$(xdotool search --name 'C Event Display' | head -1) && xdotool mousemove --window $W 249 249 "
        "click --repeat 2 --delay 80 1");
    REQUIRE(rc == 0);
    picked = Eventually([&] {
      std::lock_guard<std::mutex> lock(mutex);
      return !picks.empty();
    }, 1500);
  }
  REQUIRE(picked);
  {
    std::lock_guard<std::mutex> lock(mutex);
    CHECK(picks[0].scene == "e");
    CHECK(picks[0].layer == "hit");
    CHECK(picks[0].index == 0);
  }
  // Closing glced (here: ending it) stops the backend.
  std::system(("kill $(cat " + (dir / "glced.pid").string() + ")").c_str());
  CHECK(Eventually([&] { return !s.IsRunning("c"); }));
}
