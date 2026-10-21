// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T5.2: configuration save/load (journal of commands replayed through Resolve).
#include <catch2/catch_test_macros.hpp>

#include "CameraChanges.h"
#include "Png.h"
#include "TestSupport.h"
#include "fccvis/api/Config.h"
#include "fccvis/api/Interpreter.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace api = fccvis::api;
namespace fs = std::filesystem;
namespace rs = fccvis::backend::raster;
using fccvis::session::Session;

namespace {

api::Reply Ok(api::Interpreter& in, const std::string& line) {
  auto r = in.Execute(line);
  INFO(line << "\n" << r.Text());
  REQUIRE(r.ok);
  return r;
}

std::string Read(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

}  // namespace

TEST_CASE("Configuration JSON round-trips commands and checks its header", "[api][config]") {
  const std::vector<std::string> cmds = {"scene new a", "where a/h name == \"x\\\\y\"", "echo tab\there", "echo üñí"};
  const auto json = api::WriteConfig(cmds);
  CHECK(api::IsConfig(json));
  CHECK_FALSE(api::IsConfig("scene new a\n"));
  CHECK(api::ReadConfig(json) == cmds);
  CHECK(api::ReadConfig("{\"format\":\"fccvis-config\",\"version\":1,\"commands\":[\"\\u00e9\"]}") ==
        std::vector<std::string>{"\xC3\xA9"});
  CHECK(api::ReadConfig(api::WriteConfig({})).empty());

  auto error = [](const std::string& text) {
    try {
      (void)api::ReadConfig(text);
    } catch (const api::ConfigError& e) {
      return std::string(e.what());
    }
    return std::string("no error");
  };
  CHECK(error("{\"format\":\"x\",\"version\":1,\"commands\":[]}").find("\"format\" must be") != std::string::npos);
  CHECK(error("{\"format\":\"fccvis-config\",\"version\":2,\"commands\":[]}").find("newer fccvis") != std::string::npos);
  CHECK(error("{\"format\":\"fccvis-config\",\"version\":1}").find("\"commands\"") != std::string::npos);
  CHECK(error("{\"format\":\"fccvis-config\",\"version\":1,\"commands\":[1]}").find("array of strings") != std::string::npos);
  CHECK(error("{\n  \"format\": \"fccvis-config\",\n  \"version\": 1,\n  \"commands\": [\"a\" \"b\"]\n}").find("line 4, column 20") !=
        std::string::npos);
}

TEST_CASE("A saved session reloads to the same state and picture", "[api][config]") {
  const auto dir = fs::path(FCCVIS_TEST_BINARY_DIR) / "config-out";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const std::string csv = (fs::path(FCCVIS_TEST_SOURCE_DIR) / "data" / "csv").string();

  Session a("a");
  api::Interpreter one(a);
  one.SetBaseDirectory(csv);
  Ok(one, "load csv hits.csv entry=event");
  Ok(one, "load csv tracks.csv delimiter=; kind=line group=track order=step entry=event");
  Ok(one, "scene new front side tmp");
  Ok(one, "layer add front hits");
  Ok(one, "layer add front tracks as trk");
  Ok(one, "layer add side hits");
  Ok(one, "where side/hits energy > 1 and detector == \"ECalBarrel\"");
  Ok(one, "style front/hits when layer >= 2 then color=#ff0000 size=6");
  Ok(one, "style front/trk preset tracks-by-charge");
  Ok(one, "env side axes=true");
  Ok(one, "camera side dir 1 0 0");
  Ok(one, "scene rm tmp");
  Ok(one, "entry front/hits 2");
  Ok(one, "layout split(front as \"Front\" *2, side)");
  // A backend moves a camera (interaction): not in the journal, but in the saved state.
  REQUIRE(a.Resolve(fccvis::changes::ChangeCameraMagnification{"front", 1.25}, "viewer").ok);
  Ok(one, "save " + (dir / "session.json").string());
  const auto json = Read(dir / "session.json");
  CHECK(json.find("\"version\": 1") != std::string::npos);
  CHECK(json.find("camera front zoom 1.25") != std::string::npos);
  CHECK(std::count(json.begin(), json.end(), '\n') == 25);  // header lines + commands

  Session b("b");
  api::Interpreter two(b);
  Ok(two, "source " + (dir / "session.json").string());

  CHECK(b.SceneNames() == a.SceneNames());
  for (const auto& sc : a.SceneNames()) CHECK(Ok(two, "scene show " + sc).lines == Ok(one, "scene show " + sc).lines);
  CHECK(Ok(two, "layout show").lines == Ok(one, "layout show").lines);
  CHECK(Ok(two, "series").lines == Ok(one, "series").lines);

  // Same picture
  for (auto* in : {&one, &two}) {
    const std::string id = in == &one ? "one" : "two";
    Ok(*in, "backend init raster png output_dir=" + (dir / id).string() + " width=240 height=120 async=false");
    Ok(*in, "open png front side");
  }
  const auto pa = rs::ReadPng((dir / "one" / "layout.png").string());
  const auto pb = rs::ReadPng((dir / "two" / "layout.png").string());
  REQUIRE(pa);
  REQUIRE(pb);
  CHECK(rs::DifferentPixelFraction(*pa, *pb, 0) == 0.0);

  // Saving the reloaded session gives the same commands again.
  Ok(two, "save " + (dir / "again.json").string());
  CHECK(api::ReadConfig(Read(dir / "again.json")) == api::ReadConfig(json));
}

TEST_CASE("Loading a bad configuration fails without partial surprises", "[api][config]") {
  const auto dir = fs::path(FCCVIS_TEST_BINARY_DIR) / "config-bad";
  fs::create_directories(dir);
  std::ofstream(dir / "newer.json") << "{\"format\":\"fccvis-config\",\"version\":9,\"commands\":[\"scene new a\"]}";
  std::ofstream(dir / "broken.json") << "{\"format\":\"fccvis-config\",\"version\":1,\"commands\":[\"scene new a\", \"scene new a\", \"scene new b\"]}";
  Session s("bad");
  api::Interpreter in(s);
  auto r = in.Execute("source " + (dir / "newer.json").string());
  CHECK_FALSE(r.ok);
  CHECK(r.errors[0].find("newer fccvis") != std::string::npos);
  CHECK(s.SceneNames().empty());
  r = in.Execute("source " + (dir / "broken.json").string());
  CHECK_FALSE(r.ok);
  CHECK(r.errors[0].rfind("line 2: ", 0) == 0);  // the command that failed
  CHECK(s.SceneNames() == std::vector<std::string>{"a"});
}
