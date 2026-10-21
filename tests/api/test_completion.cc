// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T5.4: completion candidates from live state (sorted, unique).
#include <catch2/catch_test_macros.hpp>

#include "fccvis/api/Interpreter.h"

#include <filesystem>

namespace api = fccvis::api;
namespace fs = std::filesystem;
using fccvis::session::Session;
using V = std::vector<std::string>;

namespace {

struct Fixture {
  Session session{"completion"};
  api::Interpreter in{session};
  Fixture() {
    in.SetBaseDirectory((fs::path(FCCVIS_TEST_SOURCE_DIR) / "data" / "csv").string());
    for (const char* c : {"load csv hits.csv entry=event",
                          "load csv tracks.csv delimiter=; kind=line group=track order=step entry=event",
                          "scene new xy rz", "layer add xy hits", "layer add xy tracks", "layer add rz hits as h",
                          "backend init raster png output_dir=completion-out"}) {
      auto r = in.Execute(c);
      INFO(c << "\n" << r.Text());
      REQUIRE(r.ok);
    }
  }
  [[nodiscard]] V C(const std::string& line) const { return in.Complete(line); }
};

bool Contains(const V& v, const V& subset) {
  for (const auto& s : subset)
    if (std::find(v.begin(), v.end(), s) == v.end()) return false;
  return true;
}

}  // namespace

TEST_CASE("Completion of commands and subcommands", "[api][completion]") {
  Fixture f;
  const auto all = f.C("");
  CHECK(std::is_sorted(all.begin(), all.end()));
  CHECK(Contains(all, {"backend", "camera", "close", "describe", "echo", "entry", "env", "exit", "help", "layer", "layout",
                       "load", "log", "open", "quit", "redo", "save", "scene", "series", "source", "style", "undo", "where"}));
  CHECK(f.C("sc") == V{"scene"});
  CHECK(f.C("s") == V{"save", "scene", "series", "source", "style"});
  CHECK(f.C("scene ") == V{"ls", "new", "rm", "show"});
  CHECK(f.C("scene s") == V{"show"});
  CHECK(f.C("backend ") == V{"flush", "init", "ls", "rm"});
  CHECK(f.C("nonsense ").empty());
}

TEST_CASE("Completion of names from the session", "[api][completion]") {
  Fixture f;
  CHECK(f.C("scene rm ") == V{"rz", "xy"});
  CHECK(f.C("scene rm xy ") == V{"rz", "xy"});
  CHECK(f.C("layer add ") == V{"rz", "xy"});
  CHECK(f.C("layer add xy ") == V{"hits", "tracks"});
  CHECK(f.C("layer add xy hits ") == V{"as", "entry"});
  CHECK(f.C("layer add xy hits as ").empty());
  CHECK(f.C("describe t") == V{"tracks"});
  CHECK(f.C("where ") == V{"rz/h", "xy/hits", "xy/tracks"});
  CHECK(f.C("entry xy/") == V{"xy/hits", "xy/tracks"});
  CHECK(f.C("entry xy/hits ") == V{"first", "last", "next", "prev"});
  CHECK(f.C("open ") == V{"png"});
  CHECK(f.C("open png ") == V{"rz", "xy"});
  CHECK(f.C("camera ") == V{"rz", "xy"});
  CHECK(f.C("camera xy ") == V{"dir", "mode", "pos", "reset", "share", "zoom"});
  CHECK(f.C("camera xy mode ") == V{"animated", "fixed", "free"});
  CHECK(f.C("camera xy share ") == V{"rz", "xy"});
}

TEST_CASE("Completion of attributes, filters and styles", "[api][completion]") {
  Fixture f;
  CHECK(f.C("where xy/hits en") == V{"energy"});
  const auto filter = f.C("where xy/hits ");
  CHECK(Contains(filter, {"detector", "energy", "layer", "saturated", "exists(", "inside(", "all()"}));
  CHECK_FALSE(Contains(filter, {"unit:energy"}));
  CHECK(Contains(f.C("where xy/hits energy > 1 "), {"and", "or", "not"}));
  CHECK(f.C("where xy/tracks ch") == V{"charge"});
  CHECK(f.C("style xy/hits ") == V{"color=", "marker=", "opacity=", "preset", "size=", "visible=", "when"});
  CHECK(f.C("style xy/tracks w") == V{"when", "width="});
  CHECK(f.C("style xy/hits preset ") == V{"color-by", "ghosted", "hits-by-energy", "size-by"});
  CHECK(f.C("style xy/tracks preset t") == V{"tracks-by-charge"});
  CHECK(Contains(f.C("style xy/hits when "), {"energy", "then"}));
  CHECK(f.C("style xy/hits when energy > 1 then c") == V{"color="});
  CHECK(f.C("env xy ") == V{"axes=", "background=", "grid=", "preset"});
  CHECK(f.C("env xy preset ") == V{"default", "publication"});
  CHECK(Contains(f.C("layout "), {"rz", "show", "split(", "tabs(", "vsplit(", "xy"}));
}

TEST_CASE("Completion of adapters, backends, options, files and help topics", "[api][completion]") {
  Fixture f;
  CHECK(Contains(f.C("load "), {"csv"}));
  CHECK(Contains(f.C("load csv h"), {"hits.csv", "help.txt"}));
  CHECK(f.C("load csv hits.csv d") == V{"delimiter="});
  CHECK(Contains(f.C("load csv hits.csv "), {"cache=", "columns=", "entry=", "x=", "z="}));
  CHECK(Contains(f.C("backend init "), {"histogram", "raster", "svg"}));
  CHECK(f.C("backend init raster png w") == V{"width="});
  CHECK(f.C("backend init svg p") == V{"precision="});
  CHECK(f.C("backend rm ") == V{"png"});
  CHECK(Contains(f.C("help "), {"adapter", "backend", "filters", "layouts", "scene", "styles"}));
  CHECK(Contains(f.C("help adapter "), {"csv"}));
  CHECK(Contains(f.C("help backend "), {"raster", "svg"}));
  CHECK(f.C("source ../cs") == V{"../csv/"});
  CHECK(f.C("echo \"unterminated").empty());
}
