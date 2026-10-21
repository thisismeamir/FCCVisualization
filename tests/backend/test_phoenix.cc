// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
/**
 * @file test_phoenix.cc
 * @brief Phoenix backend (B5): event-data JSON (golden, validated by
 *        phoenix_check.py against the documented format), OBJ geometry, HTTP host.
 */
#include <catch2/catch_test_macros.hpp>

#include "GoldenText.h"
#include "SceneChanges.h"
#include "Session.h"
#include "TestSupport.h"

#include <arpa/inet.h>
#include <filesystem>
#include <fstream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace ch = fccvis::changes;
namespace dt = fccvis::data;
namespace st = fccvis::style;
namespace fs = std::filesystem;
using fccvis::session::Session;
using namespace fccvis::test;

namespace {

std::string Read(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::size_t Count(const std::string& s, const std::string& what) {
  std::size_t n = 0;
  for (auto p = s.find(what); p != std::string::npos; p = s.find(what, p + 1)) ++n;
  return n;
}

void Populate(Session& s) {
  REQUIRE(s.Data().Add<dt::PointElement>("pts", "fixture",
      std::make_unique<dt::InMemorySeries<dt::PointElement>>(std::vector<std::vector<dt::PointElement>>{MakePoints(4), MakePoints(2)})));
  REQUIRE(s.Data().Add<dt::LineElement>("trk", "fixture",
      std::make_unique<dt::InMemorySeries<dt::LineElement>>(std::vector<std::vector<dt::LineElement>>{MakeLines(2)})));
  REQUIRE(s.Data().Add<dt::SurfaceElement>("geo", "fixture",
      std::make_unique<dt::InMemorySeries<dt::SurfaceElement>>(std::vector<std::vector<dt::SurfaceElement>>{MakeSurfaces(2)})));
  s.DataChanged();
  for (const char* n : {"evt", "other"}) {
    REQUIRE(s.CreateScene(n));
    REQUIRE(s.Resolve(ch::SetCamera{n, std::make_shared<fccvis::scene::camera::Camera>(n)}).ok);
  }
  REQUIRE(s.Resolve(ch::AddLayer<dt::PointElement>{"evt", "hits", {"pts", ""}}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<dt::LineElement>{"evt", "tracks", {"trk", ""}}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<dt::SurfaceElement>{"evt", "detector", {"geo", ""}}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<dt::PointElement>{"other", "hits", {"pts", ""}, 1}).ok);
  // two colours in one layer: e > 2 red
  st::PointStyle red;
  red.common.color = st::Color::FromHex(0xFF0000);
  REQUIRE(s.Resolve(ch::AddStyleRule<dt::PointElement>{"evt", "hits",
      [](const dt::PointElement& e) { return e.meta.Number("e").value_or(0) > 2.0; }, red}).ok);
  st::LineStyle wide;
  wide.width = 3;
  REQUIRE(s.Resolve(ch::AddStyleRule<dt::LineElement>{"evt", "tracks", {}, wide}).ok);
}

std::string Get(int port, const std::string& path, const std::string& method = "GET") {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<std::uint16_t>(port));
  ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
    ::close(fd);
    return {};
  }
  const std::string req = method + " " + path + " HTTP/1.0\r\nHost: localhost\r\n\r\n";
  (void)::send(fd, req.data(), req.size(), 0);
  std::string out;
  char buf[4096];
  for (ssize_t n; (n = ::recv(fd, buf, sizeof buf, 0)) > 0;) out.append(buf, static_cast<std::size_t>(n));
  ::close(fd);
  return out;
}

}  // namespace

TEST_CASE("Phoenix backend writes event JSON and OBJ geometry", "[backend][phoenix]") {
  const auto dir = fs::path(FCCVIS_TEST_BINARY_DIR) / "phoenix-out";
  fs::remove_all(dir);
  Session s("phoenix");
  auto log = CaptureLog(s);
  Populate(s);
  REQUIRE(s.InitBackend("phoenix", "web", {{"output_dir", dir.string()}}).ok);
  REQUIRE(s.Open("web", "evt").ok);
  REQUIRE(s.Open("web", "other").ok);
  REQUIRE(s.Flush("web").ok);

  const auto json = Read(dir / "events.json");
  CHECK(json.find("\"evt\": {\n    \"event number\": 0") != std::string::npos);
  CHECK(json.find("\"other\": {\n    \"event number\": 1") != std::string::npos);
  CHECK(json.find("\"hits #ff0000\": [") != std::string::npos);   // split by colour
  CHECK(json.find("\"hits #7017ff\": [") != std::string::npos);
  CHECK(Count(json, "\"type\": \"Point\"") == 4 + 2);
  CHECK(json.find("\"linewidth\": 3") != std::string::npos);
  CHECK(json.find("\"charge\": -1") != std::string::npos);        // metadata as decoration
  const auto golden = CompareGoldenText(json, "phoenix_events.json");
  INFO(golden.message);
  CHECK(golden.ok);

  const auto obj = Read(dir / "evt.obj");
  CHECK(obj.find("mtllib evt.mtl") != std::string::npos);
  CHECK(Count(obj, "\no ") == 1);
  CHECK(Count(obj, "\nf ") == 4);  // two squares, two triangles each
  CHECK(Count(Read(dir / "evt.mtl"), "newmtl") == 1);
  CHECK_FALSE(fs::exists(dir / "other.obj"));  // no surfaces there

  // Changing a filter rewrites the JSON.
  REQUIRE(s.Resolve(ch::SetLayerFilter<dt::PointElement>{"evt", "hits",
      std::make_shared<fccvis::filter::Filter<dt::PointElement>>(
          [](const dt::PointElement& e) { return e.meta.Number("e").value_or(0) > 2.0; })}).ok);
  REQUIRE(s.Flush("web").ok);
  CHECK(Count(Read(dir / "events.json"), "\"type\": \"Point\"") == 2 + 2);
  CHECK(CountLog(*log, fccvis::changes::Severity::Error, "") == 0);
}

TEST_CASE("Phoenix backend serves its files over HTTP with CORS", "[backend][phoenix]") {
  const auto dir = fs::path(FCCVIS_TEST_BINARY_DIR) / "phoenix-http";
  fs::remove_all(dir);
  Session s("http");
  auto log = CaptureLog(s);
  Populate(s);
  const int port = 20000 + static_cast<int>(::getpid() % 20000);
  REQUIRE(s.InitBackend("phoenix", "web", {{"output_dir", dir.string()}, {"port", std::to_string(port)}}).ok);
  REQUIRE(s.Open("web", "evt").ok);
  REQUIRE(s.Flush("web").ok);

  const auto r = Get(port, "/events.json");
  REQUIRE(r.rfind("HTTP/1.0 200 OK\r\n", 0) == 0);
  CHECK(r.find("Access-Control-Allow-Origin: *\r\n") != std::string::npos);
  CHECK(r.find("Content-Type: application/json\r\n") != std::string::npos);
  CHECK(r.substr(r.find("\r\n\r\n") + 4) == Read(dir / "events.json"));
  const auto head = Get(port, "/events.json", "HEAD");  // same headers, no body
  CHECK(head == r.substr(0, r.find("\r\n\r\n") + 4));
  CHECK(Get(port, "/").find("events.json") != std::string::npos);
  CHECK(Get(port, "/../etc/passwd").rfind("HTTP/1.0 404", 0) == 0);
  CHECK(Get(port, "/missing.json").rfind("HTTP/1.0 404", 0) == 0);

  REQUIRE(s.DetachBackend("web").ok);
  CHECK(Get(port, "/events.json").empty());  // server stopped
  CHECK_FALSE(s.InitBackend("phoenix", "bad", {{"port", "70000"}}).ok);
}
