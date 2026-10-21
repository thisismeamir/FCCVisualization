// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T1.2: RemoveScene change, layout pruning, backend close.
#include <catch2/catch_test_macros.hpp>

#include "SceneChanges.h"
#include "Session.h"
#include "TestSupport.h"

#include <algorithm>

namespace ch = fccvis::changes;
using fccvis::session::Session;
using namespace fccvis::test;

TEST_CASE("RemoveScene removes the scene, prunes the layout and closes it in backends",
          "[session][scene][remove]") {
  Session s("t");
  auto log = CaptureLog(s);
  for (const char* n : {"a", "b", "c"}) REQUIRE(s.CreateScene(n));
  REQUIRE(s.SetLayout(S({P("a"), T({P("b"), P("c")}), P("b")})).ok);
  auto x = AttachRecording(s, "x");
  auto y = AttachRecording(s, "y");
  REQUIRE(s.Open("x", "a").ok);
  REQUIRE(s.Open("x", "b").ok);
  REQUIRE(s.Open("y", "b").ok);

  const auto r = s.Resolve(ch::RemoveScene{"b"});
  REQUIRE(r.ok);
  CHECK_FALSE(s.HasScene("b"));
  CHECK(s.SceneNames() == std::vector<std::string>{"a", "c"});
  REQUIRE(s.Options().sceneLayout);
  CHECK(Describe(*s.Options().sceneLayout) == "S(a,c)");
  CHECK(s.Validate().empty());

  const auto calls = x->Calls();
  CHECK(std::count(calls.begin(), calls.end(), "CloseScene(b)") == 1);
  CHECK(s.OpenScenes("x") == std::set<std::string>{"a"});
  CHECK(s.OpenScenes("y").empty());
  CHECK(Describe(x->LastSync().layout) == "a");
  CHECK(Describe(y->LastSync().layout) == "-");
  CHECK(x->LastSync().dirty.count("b") == 0);
}

TEST_CASE("RemoveScene of the last pane clears the layout", "[session][scene][remove]") {
  Session s("t");
  CaptureLog(s);
  REQUIRE(s.CreateScene("only"));
  REQUIRE(s.SetLayout(P("only")).ok);
  REQUIRE(s.Resolve(ch::RemoveScene{"only"}).ok);
  CHECK_FALSE(s.Options().sceneLayout.has_value());
  CHECK(s.SceneNames().empty());
}

TEST_CASE("RemoveScene validates before mutating", "[session][scene][remove]") {
  Session s("t");
  auto log = CaptureLog(s);
  REQUIRE(s.CreateScene("a"));
  REQUIRE(s.SetLayout(P("a")).ok);
  const auto r = s.Resolve(ch::RemoveScene{"ghost"});
  CHECK_FALSE(r.ok);
  CHECK(r.messages.at(0).find("unknown scene 'ghost'") != std::string::npos);
  CHECK(s.HasScene("a"));
  CHECK(Describe(*s.Options().sceneLayout) == "a");
}

TEST_CASE("A removed scene can be created again and reopened", "[session][scene][remove]") {
  Session s("t");
  CaptureLog(s);
  auto x = AttachRecording(s, "x");
  REQUIRE(s.CreateScene("a"));
  REQUIRE(s.Open("x", "a").ok);
  REQUIRE(s.Resolve(ch::RemoveScene{"a"}).ok);
  REQUIRE(s.CreateScene("a"));
  REQUIRE(s.Open("x", "a").ok);
  CHECK(s.OpenScenes("x") == std::set<std::string>{"a"});
  CHECK(ch::Has(x->LastSync().dirty.at("a").parts, ch::Part::Environment));
}
