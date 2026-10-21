// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T5.1: fluent builders and Script.
#include <catch2/catch_test_macros.hpp>

#include "TestSupport.h"
#include "fccvis/api/Builders.h"
#include "fccvis/stdlib/Filters.h"
#include "fccvis/stdlib/Styles.h"

namespace api = fccvis::api;
namespace dt = fccvis::data;
namespace st = fccvis::style;
using fccvis::session::Session;
using namespace fccvis::test;

namespace {

void AddData(Session& s) {
  REQUIRE(s.Data().Add<dt::PointElement>("calo-hits", "fixture",
      std::make_unique<dt::InMemorySeries<dt::PointElement>>(std::vector<std::vector<dt::PointElement>>{MakePoints(5), MakePoints(2)})));
  REQUIRE(s.Data().Add<dt::LineElement>("tracks", "fixture",
      std::make_unique<dt::InMemorySeries<dt::LineElement>>(std::vector<std::vector<dt::LineElement>>{MakeLines(3)})));
  s.DataChanged();
}

}  // namespace

TEST_CASE("Builders produce scripts of catalogue changes", "[api]") {
  st::PointStyle red;
  red.common.color = st::Color::FromHex(0xFF0000);
  api::Script script = api::Scene("xy").Create()
                           .Direction({0, 0, -1})
                           .Zoom(2)
                           .Layer<dt::Point>("hits", "calo-hits")
                           .Where(fccvis::stdlib::EnergyAbove(1.0, "e"))
                           .Style(red)
                           .Layer<dt::Line>("tracks", "tracks")
                           .Sheet(fccvis::stdlib::TracksByCharge());
  const auto d = script.Describe();
  REQUIRE(d.size() >= 7);
  CHECK(d[0] == "CreateScene(xy)");
  CHECK(d[1] == "SetCamera(xy)");
  CHECK(d[2].rfind("ChangeCameraDirection(xy", 0) == 0);
  CHECK(d[4] == "AddLayer(xy, hits, series=calo-hits)");
  CHECK(script.Changes()[5]->Name() == "SetLayerFilter");
  CHECK(script.Changes()[6]->Name() == "AddStyleRule");
  CHECK(script.Changes()[7]->Name() == "AddLayer");

  Session s("api");
  auto log = CaptureLog(s);
  AddData(s);
  const auto r = api::Run(s, script);
  INFO((r.messages.empty() ? "" : r.messages[0]));
  REQUIRE(r.ok);
  CHECK(r.applied == script.Size());
  auto scene = s.FindScene("xy");
  REQUIRE(scene);
  CHECK(scene->camera->magnification == 2.0);
  CHECK(scene->layers.Types().size() == 2);
  const auto* hits = scene->layers.Find<dt::PointElement>("hits");
  REQUIRE(hits);
  REQUIRE(hits->filter);
  CHECK_FALSE(hits->Passes(MakePoints(1, 0.5)[0], 0));
  CHECK(hits->Passes(MakePoints(1, 1.5)[0], 0));
}

TEST_CASE("A script is one batch: it stops at the first failing change", "[api]") {
  Session s("batch");
  auto log = CaptureLog(s);
  AddData(s);
  api::Script bad = api::Scene("a").Create().Layer<dt::Point>("hits", "calo-hits").Entry(1);
  bad.Append(api::Scene("a").Zoom(-1));  // invalid magnification
  bad.Append(api::Scene("b").Create());
  const auto r = bad.Run(s);
  CHECK_FALSE(r.ok);
  CHECK(r.applied == 4);  // CreateScene, SetCamera, AddLayer, SetEntry
  CHECK(s.HasScene("a"));
  CHECK_FALSE(s.HasScene("b"));
  CHECK(s.FindScene("a")->layers.Find<dt::PointElement>("hits")->entry == 1u);
}

TEST_CASE("Scripts are values, layouts and scene removal", "[api]") {
  Session s("values");
  auto log = CaptureLog(s);
  AddData(s);
  const api::Script both = api::Script().Append(api::Scene("a").Create()).Append(api::Scene("b").Create());
  api::Script copy = both;
  copy.Append(api::Layout(api::Split({{api::Pane("a"), "A", 2}, api::Tabs({api::Pane("b")})})));
  CHECK(both.Size() == 4);
  CHECK(copy.Size() == 5);
  REQUIRE(copy.Run(s).ok);
  CHECK(s.ValidateLayout().empty());
  CHECK(Describe(s.Options().sceneLayout) == "S(a,T(b))");

  REQUIRE(api::Run(s, api::Scene("b").Remove()).ok);
  CHECK_FALSE(s.HasScene("b"));
  REQUIRE(api::Run(s, api::Scene("a").Layer<dt::Point>("hits", "calo-hits").All()).ok);
  REQUIRE(api::Run(s, api::Scene("a").RemoveLayer("hits")).ok);
  CHECK(s.FindScene("a")->layers.Types().empty());
}
