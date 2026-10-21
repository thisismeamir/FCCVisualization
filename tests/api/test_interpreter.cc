// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T5.3: interpreter commands on a session (CSV adapter, raster backend).
#include <catch2/catch_test_macros.hpp>

#include "Png.h"
#include "TestSupport.h"
#include "fccvis/api/Interpreter.h"

#include <filesystem>
#include <fstream>
#include <mutex>

namespace api = fccvis::api;
namespace dt = fccvis::data;
namespace fs = std::filesystem;
using fccvis::session::Session;
using namespace fccvis::test;

namespace {

std::string CsvDir() { return (fs::path(FCCVIS_TEST_SOURCE_DIR) / "data" / "csv").string(); }

api::Reply Ok(api::Interpreter& in, const std::string& line) {
  auto r = in.Execute(line);
  INFO(line << "\n" << r.Text());
  REQUIRE(r.ok);
  return r;
}

std::string Err(api::Interpreter& in, const std::string& line) {
  auto r = in.Execute(line);
  INFO(line << "\n" << r.Text());
  REQUIRE_FALSE(r.ok);
  REQUIRE(!r.errors.empty());
  return r.errors[0];
}

}  // namespace

TEST_CASE("Interpreter builds a session from commands", "[api][shell]") {
  Session s("shell");
  api::Interpreter in(s);
  in.SetBaseDirectory(CsvDir());
  Ok(in, "load csv hits.csv entry=event");
  Ok(in, "load csv tracks.csv delimiter=; kind=line group=track order=step entry=event");
  CHECK(Ok(in, "series").lines.size() == 2);
  CHECK(Ok(in, "series hi*").lines.size() == 1);
  const auto d = Ok(in, "describe hits").lines;
  CHECK(d[0] == "hits (point)");
  CHECK(std::find(d.begin(), d.end(), "    energy  number  (4 of 4)") != d.end());

  Ok(in, "scene new xy rz");
  CHECK(Ok(in, "scene ls").lines == std::vector<std::string>{"rz", "xy"});
  Ok(in, "layer add xy hits");
  Ok(in, "layer add xy tracks as trk entry 1");
  Ok(in, "layer add rz hits as h");
  CHECK(Ok(in, "layer ls xy").lines == std::vector<std::string>{"hits  point", "trk  line"});
  Ok(in, "where xy/hits energy > 1 and detector == \"ECalBarrel\"");
  Ok(in, "style xy/hits color=red size=5");
  Ok(in, "style xy/hits when layer >= 3 then marker=square");
  Ok(in, "style xy/trk preset tracks-by-charge");
  Ok(in, "style rz/h preset hits-by-energy 0 4");
  Ok(in, "env rz axes=true grid=true");
  Ok(in, "camera rz dir 1 0 0");
  Ok(in, "camera rz zoom 1.5");
  Ok(in, "camera rz mode fixed");
  Ok(in, "layout split(xy as \"Front\" *2, rz)");
  CHECK(Ok(in, "layout show").lines[0] == "split(xy as \"Front\" *2, rz)");
  CHECK(Ok(in, "entry xy/hits next").lines[0] == "xy/hits: entry 1 of 3");
  Ok(in, "entry xy/hits first");

  const auto show = Ok(in, "scene show xy").lines;
  CHECK(show[1] == "  camera: dir 0 0 1, pos 0 0 0, zoom 1, mode free");
  CHECK(std::find(show.begin(), show.end(), "    where energy > 1 and detector == \"ECalBarrel\"") != show.end());
  CHECK(std::find(show.begin(), show.end(), "    style when layer >= 3 then marker=square") != show.end());
  CHECK(s.FindScene("rz")->camera->magnification == 1.5);
  const auto* hits = s.FindScene("xy")->layers.Find<dt::PointElement>("hits");
  REQUIRE(hits);
  REQUIRE(hits->filter);

  // Backends
  const auto out = fs::path(FCCVIS_TEST_BINARY_DIR) / "interpreter-out";
  fs::remove_all(out);
  Ok(in, "backend init raster png output_dir=" + out.string() + " width=200 height=100");
  Ok(in, "open png xy rz");
  Ok(in, "backend flush png");
  CHECK(Ok(in, "backend ls").lines[0] == "png: rz, xy");
  CHECK(fs::exists(out / "layout.png"));
  Ok(in, "close png rz");
  Ok(in, "backend rm png");
  CHECK(Ok(in, "backend ls").lines[0] == "(no backends)");

  // Journal: only state commands, canonical load lines
  const auto& j = in.Journal();
  CHECK(j[0].rfind("load csv ", 0) == 0);
  CHECK(j[0].find("hits.csv entry=event") != std::string::npos);
  CHECK(std::find(j.begin(), j.end(), "entry xy/hits 0") != j.end());
  CHECK(std::none_of(j.begin(), j.end(), [](const std::string& c) { return c.rfind("backend", 0) == 0 || c.rfind("open", 0) == 0; }));
  CHECK(Ok(in, "log 3").lines.size() == 3);
}

TEST_CASE("Interpreter errors explain and point at the problem", "[api][shell]") {
  Session s("errors");
  api::Interpreter in(s);
  in.SetBaseDirectory(CsvDir());
  Ok(in, "load csv hits.csv");
  Ok(in, "scene new xy");
  Ok(in, "layer add xy hits");
  CHECK(Err(in, "frobnicate").find("unknown command 'frobnicate'") == 0);
  CHECK(Err(in, "load nope x.csv").find("unknown adapter 'nope'; available: csv") == 0);
  CHECK(Err(in, "load csv missing.csv").find("nothing registered") == 0);
  CHECK(Err(in, "load csv hits.csv bogus=1").find("unknown csv option 'bogus'") == 0);
  CHECK(Err(in, "scene new xy").find("CreateScene: scene 'xy' already exists") == 0);
  CHECK(Err(in, "layer add zz hits").find("unknown scene 'zz'; scenes: xy") == 0);
  CHECK(Err(in, "layer add xy nothing").find("unknown series 'nothing'") == 0);
  CHECK(Err(in, "where xy/nope energy > 1").find("scene 'xy' has no layer 'nope'; layers: hits") == 0);
  const auto e = Err(in, "where xy/hits energy >> 1");
  CHECK(e.find("expected a number") == 0);
  CHECK(e.find("at column 23") != std::string::npos);
  CHECK(e.find("\n  where xy/hits energy >> 1\n                        ^") != std::string::npos);
  CHECK(Err(in, "style xy/hits width=3").find("unknown point style key 'width'") == 0);
  CHECK(Err(in, "style xy/hits preset tracks-by-charge").find("applies to line layers") != std::string::npos);
  CHECK(Err(in, "style xy/hits when energy > 1 color=red").find("needs 'then'") != std::string::npos);
  CHECK(Err(in, "camera xy zoom -1").find("magnification must be positive") != std::string::npos);
  CHECK(Err(in, "entry xy/hits 5") == "entry 5 out of range (1 entries)");
  CHECK(Err(in, "layout split(xy,").find("at column") != std::string::npos);
  CHECK(Err(in, "open nope xy") == "unknown backend 'nope'");
  CHECK(Err(in, "redo") == "nothing to redo");
  CHECK(Err(in, "echo \"open").find("unterminated string") == 0);
  CHECK(in.Execute("# comment").ok);
  CHECK(in.Execute("   ").ok);
  CHECK(in.Execute("quit").quit);
}

TEST_CASE("Scripts stop at the first failing line and report its number", "[api][shell]") {
  Session s("script");
  api::Interpreter in(s);
  const auto r = in.Run("scene new a\n# comment\nscene \\\n  new b\nscene new a\nscene new c\n");
  CHECK_FALSE(r.ok);
  REQUIRE(r.errors.size() == 1);
  CHECK(r.errors[0].rfind("line 5: ", 0) == 0);
  CHECK(s.SceneNames() == std::vector<std::string>{"a", "b"});
  const auto all = in.Run("scene new x\nscene new x\nscene new y\n", false);
  CHECK_FALSE(all.ok);
  CHECK(s.HasScene("y"));
}

TEST_CASE("Messages from backend threads reach the sink, the command's own go into its reply", "[api][shell]") {
  Session s("log");
  api::Interpreter in(s);
  std::mutex mutex;
  std::vector<std::string> seen;
  in.SetLogSink([&](fccvis::changes::Severity, const std::string& m) {
    std::lock_guard<std::mutex> lock(mutex);
    seen.push_back(m);
  });
  Ok(in, "scene new a");
  (void)in.Execute("scene new a");  // error reported in the reply only
  CHECK(seen.empty());

  // A render failure happens on the backend's thread: it reaches the sink.
  const auto out = fs::path(FCCVIS_TEST_BINARY_DIR) / "interpreter-log";
  fs::remove_all(out);
  fs::create_directories(out / "a.png");  // a directory where the image should go
  Ok(in, "backend init raster r output_dir=" + out.string());
  Ok(in, "open r a");
  Ok(in, "backend flush r");
  std::lock_guard<std::mutex> lock(mutex);
  REQUIRE(seen.size() == 1);
  CHECK(seen[0].find("cannot write") != std::string::npos);
}

TEST_CASE("Undo and redo in the shell: one command is one step, the saved state follows", "[api][shell][undo]") {
  Session s("undo");
  api::Interpreter in(s);
  in.SetBaseDirectory(CsvDir());
  Ok(in, "load csv hits.csv entry=event");
  CHECK(Err(in, "undo") == "nothing to undo");  // loading is not a change
  Ok(in, "scene new xy");
  Ok(in, "layer add xy hits");
  const auto plain = Ok(in, "scene show xy").lines;
  const auto plainState = in.StateCommands();
  Ok(in, "where xy/hits energy > 1");
  Ok(in, "style xy/hits color=red size=5");
  Ok(in, "camera xy zoom 2");
  const auto styled = Ok(in, "scene show xy").lines;
  const auto styledState = in.StateCommands();

  CHECK(Ok(in, "undo").lines == std::vector<std::string>{"undone: camera xy zoom 2"});
  CHECK(Ok(in, "undo").lines == std::vector<std::string>{"undone: style xy/hits color=red size=5"});
  Ok(in, "undo");
  CHECK(Ok(in, "scene show xy").lines == plain);
  CHECK(in.StateCommands() == plainState);  // `save` writes the undone state

  Ok(in, "redo");
  Ok(in, "redo");
  CHECK(Ok(in, "redo").lines == std::vector<std::string>{"redone: camera xy zoom 2"});
  CHECK(Ok(in, "scene show xy").lines == styled);
  CHECK(in.StateCommands() == styledState);
  CHECK(Err(in, "redo") == "nothing to redo");

  // `entry next` and multi-change commands undo as one step.
  Ok(in, "scene new rz side");
  Ok(in, "undo");
  CHECK(Ok(in, "scene ls").lines == std::vector<std::string>{"xy"});
  Ok(in, "entry xy/hits next");
  Ok(in, "undo");
  CHECK(s.FindScene("xy")->layers.Find<dt::PointElement>("hits")->entry.value_or(0) == 0);

  // A sourced script is one step.
  const auto script = fs::path(FCCVIS_TEST_BINARY_DIR) / "undo-script.fcv";
  std::ofstream(script) << "scene new a\nscene new b\nlayout split(xy, a, b)\n";
  Ok(in, "source " + script.string());
  CHECK(Ok(in, "scene ls").lines.size() == 3);
  Ok(in, "undo");
  CHECK(Ok(in, "scene ls").lines == std::vector<std::string>{"xy"});
}
