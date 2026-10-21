// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T1.4: live series append notification.
#include <catch2/catch_test_macros.hpp>

#include "SceneChanges.h"
#include "Session.h"
#include "TestSupport.h"

#include <thread>

namespace ch = fccvis::changes;
namespace dt = fccvis::data;
using fccvis::session::Session;
using namespace fccvis::test;
using PE = dt::PointElement;

TEST_CASE("Appending to a live series redraws exactly the layers showing it",
          "[session][data][live]") {
  Session s("t");
  CaptureLog(s);
  auto loads = std::make_shared<int>(0);
  auto live = std::make_unique<VectorSeries<PE>>(std::vector<std::vector<PE>>{MakePoints(2)}, loads);
  auto append = live->MakeAppender();
  REQUIRE(s.Data().Add<PE>("stream", "daq", std::move(live)));
  REQUIRE(s.Data().Add<PE>("static", "file",
                           std::make_unique<VectorSeries<PE>>(std::vector<std::vector<PE>>{MakePoints(1)})));
  s.DataChanged();

  for (const char* n : {"a", "b"}) REQUIRE(s.CreateScene(n));
  REQUIRE(s.Resolve(ch::AddLayer<PE>{"a", "hits", {"stream", ""}}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<PE>{"a", "other", {"static", ""}}).ok);
  REQUIRE(s.Resolve(ch::AddLayer<PE>{"b", "mirror", {"stream", "daq"}}).ok);
  auto rec = AttachRecording(s, "x");
  REQUIRE(s.Open("x", "a").ok);
  REQUIRE(s.Open("x", "b").ok);

  // The scene draws entry 0 and caches it.
  REQUIRE(s.FindScene("a")->Prepare<PE>(s.Data(), "hits").size() == 2);
  REQUIRE(*loads == 1);
  const auto dataRevBefore = rec->LastSync().dataRevision;
  CHECK(s.SeriesRevision({"stream", "daq"}) == 0);

  append.append(MakePoints(5));
  s.SeriesAppended({"stream", "daq"});

  CHECK(s.Data().Size<PE>({"stream", ""}) == 2);
  const auto last = rec->LastSync();
  CHECK(last.dataRevision == dataRevBefore);  // set of series unchanged
  CHECK(last.dirty.at("a").layers == std::set<std::string>{"hits"});
  CHECK(last.dirty.at("b").layers == std::set<std::string>{"mirror"});
  CHECK(last.dirty.at("a").parts == ch::Part::None);
  CHECK(s.SeriesRevision({"stream", "daq"}) > 0);
  CHECK(*loads == 1);  // entry 0 stays cached

  // Moving the cursor to the new entry loads only that entry.
  REQUIRE(s.Resolve(ch::SetEntry<PE>{"a", "hits", 1}).ok);
  CHECK(s.FindScene("a")->Prepare<PE>(s.Data(), "hits").size() == 5);
  CHECK(*loads == 2);
}

TEST_CASE("SeriesAppended may be called from another thread", "[session][data][live]") {
  Session s("t");
  CaptureLog(s);
  auto live = std::make_unique<VectorSeries<PE>>(std::vector<std::vector<PE>>{MakePoints(1)});
  auto append = live->MakeAppender();
  REQUIRE(s.Data().Add<PE>("stream", "daq", std::move(live)));
  REQUIRE(s.CreateScene("a"));
  REQUIRE(s.Resolve(ch::AddLayer<PE>{"a", "hits", {"stream", ""}}).ok);
  auto rec = AttachRecording(s, "x");
  REQUIRE(s.Open("x", "a").ok);
  const auto before = rec->SyncCount();

  std::thread producer([&] {
    for (int i = 0; i < 20; ++i) {
      append.append(MakePoints(1));
      s.SeriesAppended({"stream", "daq"});
    }
  });
  for (int i = 0; i < 20; ++i) REQUIRE(s.Resolve(ch::SetEntry<PE>{"a", "hits", 0}).ok);
  producer.join();

  CHECK(s.Data().Size<PE>({"stream", ""}) == 21);
  CHECK(rec->SyncCount() == before + 40);
}

TEST_CASE("InMemorySeries serves its entries", "[data]") {
  dt::SessionData d;
  REQUIRE(d.Add<PE>("mem", "here",
                    std::make_unique<dt::InMemorySeries<PE>>(std::vector<std::vector<PE>>{MakePoints(2), MakePoints(4)})));
  const auto e1 = d.Entry<PE>({"mem", ""}, 1);
  REQUIRE(e1);
  CHECK(e1->size() == 4);
  CHECK_FALSE(d.Entry<PE>({"mem", ""}, 2));
}
