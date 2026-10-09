/**
 * @file session_test.cpp
 * @brief End-to-end checks of Session, Resolve, revisions, scoped backends and Prune.
 *
 * Copyright 2026 Amir H. Ebrahimnezhad
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "BackendRegistry.h"
#include "CameraChanges.h"
#include "Elements.h"
#include "Filter.h"
#include "RecordingBackend.h"
#include "Session.h"
#include "StyleTraits.h"

namespace ch = fccvis::changes;
namespace dt = fccvis::data;
namespace lay = fccvis::scene::layout;

using fccvis::session::Session;
using PE = dt::PointElement;

namespace {

// ---- helpers -------------------------------------------------------------

void Quiet(Session &s)
{
  s.SetLogSink([](ch::Severity, const std::string &) {});
}

std::shared_ptr<fccvis::backend::Recording> Attach(
    Session &s,
    const std::string &id)
{
  static int n = 0;

  const std::string type = "recording" + std::to_string(++n);

  auto rec = fccvis::backend::RegisterRecordingBackend(type);

  CHECK(s.InitBackend(type, id).ok);

  return rec;
}

// In-memory series; counts how often entries are loaded.
class VectorSeries final : public dt::Series<PE>
{
 public:
  VectorSeries(
      std::vector<std::vector<PE>> entries,
      std::shared_ptr<int> loads)
      : m_entries(std::move(entries)),
        m_loads(std::move(loads))
  {
  }

  std::size_t Size() const override
  {
    return m_entries.size();
  }

  std::vector<PE> Load(std::size_t entry) const override
  {
    ++*m_loads;
    return m_entries.at(entry);
  }

 private:
  std::vector<std::vector<PE>> m_entries;
  std::shared_ptr<int> m_loads;
};

// Each entry holds two elements with metadata "e" = 0.5 and 2.0.
std::shared_ptr<int> AddSeries(
    Session &s,
    const std::string &name,
    std::size_t entries)
{
  auto loads = std::make_shared<int>(0);

  std::vector<std::vector<PE>> data(entries);

  for (auto &entry : data) {
    PE a, b;

    a.meta.Set("e", 0.5);
    b.meta.Set("e", 2.0);

    entry = {a, b};
  }

  CHECK(s.Data().Add<PE>(
      name,
      "file",
      std::make_unique<VectorSeries>(std::move(data), loads)));

  s.DataChanged();

  return loads;
}

lay::LayoutNode P(std::string n)
{
  return lay::LayoutNode{lay::Pane{std::move(n)}};
}

lay::LayoutNode S(std::vector<lay::LayoutNode> kids)
{
  lay::Split s;

  for (auto &k : kids)
    s.children.push_back({std::move(k), std::nullopt, 1});

  return lay::LayoutNode{std::move(s)};
}

lay::LayoutNode T(std::vector<lay::LayoutNode> kids)
{
  lay::Tabs t;

  for (auto &k : kids)
    t.children.push_back({std::move(k), std::nullopt});

  return lay::LayoutNode{std::move(t)};
}

std::string Describe(const lay::LayoutNode &n)
{
  return std::visit(
      [](const auto &v) -> std::string {
        using V = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<V, lay::Pane>) {
          return v.sceneName;
        }
        else {
          std::string out =
              std::is_same_v<V, lay::Split> ? "S(" : "T(";

          for (std::size_t i = 0; i < v.children.size(); ++i)
            out += (i ? "," : "") + Describe(v.children[i].node);

          return out + ")";
        }
      },
      n.value);
}

std::string Describe(const std::optional<lay::LayoutNode> &n)
{
  return n ? Describe(*n) : "-";
}

} // namespace

// ---- tests ---------------------------------------------------------------

TEST_CASE(
    "Prune removes closed scenes and collapses empty containers",
    "[session][layout][prune]")
{
  const auto layout =
      S({P("a"), T({P("b"), P("c")}), P("a")});

  auto pruned = [&](std::set<std::string> open) {
    return Describe(
        lay::Prune(
            layout,
            [&](const std::string &n) {
              return open.count(n) > 0;
            }));
  };

  CHECK(pruned({"a", "b", "c"}) == "S(a,T(b,c),a)");
  CHECK(pruned({"a", "c"}) == "S(a,c,a)");
  CHECK(pruned({"a"}) == "S(a,a)");
  CHECK(pruned({"b"}) == "b");
  CHECK(pruned({}) == "-");
}

TEST_CASE(
    "Session revisions and backend synchronization",
    "[session][sync][revision]")
{
  Session s("t");
  Quiet(s);

  auto rec = Attach(s, "main");
  auto cam =
      std::make_shared<fccvis::scene::camera::Camera>("cam");

  CHECK(s.CreateScene("a") != nullptr);
  CHECK(s.CreateScene("a") == nullptr);

  CHECK(s.Resolve(ch::SetCamera{"a", cam}).ok);
  CHECK(s.Open("main", "a").ok);

  auto d = rec->LastSync().dirty.at("a");

  CHECK(ch::Has(d.parts, ch::Part::Camera));
  CHECK(ch::Has(d.parts, ch::Part::Environment));

  fccvis::style::EnvironmentStyle env;
  env.grid = true;

  CHECK(s.Resolve(ch::SetEnvironment{"a", env}).ok);

  d = rec->LastSync().dirty.at("a");

  CHECK(d.parts == ch::Part::Environment);
  CHECK(d.layers.empty());

  AddSeries(s, "pts", 3);

  CHECK(
      s.Resolve(
          ch::AddLayer<PE>{
              "a",
              "hits",
              dt::SeriesRef{"pts", ""}})
          .ok);

  d = rec->LastSync().dirty.at("a");

  CHECK(ch::Has(d.parts, ch::Part::LayerSet));
  CHECK(d.layers.count("hits") == 1);

  CHECK(
      s.Resolve(
          ch::SetEntry<PE>{"a", "hits", 2})
          .ok);

  d = rec->LastSync().dirty.at("a");

  CHECK(d.parts == ch::Part::None);
  CHECK(d.layers == std::set<std::string>{"hits"});

  const auto syncs = rec->SyncCount();

  CHECK(
      !s.Resolve(
          ch::SetEntry<PE>{"a", "missing", 0})
          .ok);

  CHECK(rec->SyncCount() == syncs);

  // A camera shared by two scenes dirties both.
  CHECK(s.CreateScene("b") != nullptr);
  CHECK(s.Resolve(ch::SetCamera{"b", cam}).ok);
  CHECK(s.Open("main", "b").ok);

  CHECK(
      s.Resolve(
          ch::ChangeCameraMagnification{"a", 3.0})
          .ok);

  auto last = rec->LastSync();

  CHECK(ch::Has(last.dirty.at("a").parts, ch::Part::Camera));
  CHECK(ch::Has(last.dirty.at("b").parts, ch::Part::Camera));

  CHECK(
      !s.Resolve(
          ch::ChangeCameraMagnification{"a", -1.0})
          .ok);

  CHECK(cam->magnification == 3.0);
}

TEST_CASE(
    "Scoped backends maintain independent scene sets",
    "[session][backend][scoped]")
{
  Session s("t");
  Quiet(s);

  for (const char *n : {"a", "b", "c"})
    CHECK(s.CreateScene(n) != nullptr);

  CHECK(s.SetLayout(S({P("a"), P("b"), P("c")})).ok);
  CHECK(s.Validate().empty());

  auto x = Attach(s, "x");
  auto y = Attach(s, "y");

  CHECK(s.Open("x", "a").ok);
  CHECK(s.Open("x", "b").ok);
  CHECK(s.Open("y", "c").ok);

  CHECK(Describe(x->LastSync().layout) == "S(a,b)");
  CHECK(Describe(y->LastSync().layout) == "c");

  CHECK(s.Close("x", "b").ok);
  CHECK(Describe(x->LastSync().layout) == "a");

  CHECK(
      s.OpenScenes("x") ==
      std::set<std::string>{"a"});

  x->rejectScenes.insert("c");

  CHECK(!s.Open("x", "c").ok);

  CHECK(
      s.OpenScenes("x") ==
      std::set<std::string>{"a"});

  CHECK(!s.Open("x", "ghost").ok);
  CHECK(!s.InitBackend("nonexistent").ok);

  CHECK(s.DetachBackend("x").ok);

  CHECK(
      s.BackendIds() ==
      std::vector<std::string>{"y"});
}

TEST_CASE(
    "Backend failure does not prevent healthy backends from syncing",
    "[session][backend][failure]")
{
  Session s("t");
  Quiet(s);

  auto bad = Attach(s, "bad");
  auto good = Attach(s, "good");

  bad->throwOnSync = true;

  const auto before = good->SyncCount();

  auto r = s.Resolve(ch::CreateScene{"n"});

  CHECK(r.ok);
  CHECK(!r.messages.empty());
  CHECK(good->SyncCount() > before);
}

TEST_CASE(
    "Batch resolution stops at the first failed change",
    "[session][resolve][batch]")
{
  Session s("t");
  Quiet(s);

  ch::CreateScene c1{"n"};
  ch::AddLayer<PE> bad{
      "ghost",
      "l",
      dt::SeriesRef{"pts", ""}};
  ch::CreateScene c2{"m"};

  const ch::Change *batch[] = {
      &c1,
      &bad,
      &c2,
  };

  auto r =
      s.Resolve(
          std::span<const ch::Change *const>(batch));

  CHECK(!r.ok);
  CHECK(r.applied == 1);
  CHECK(s.HasScene("n"));
  CHECK(!s.HasScene("m"));
}

TEST_CASE(
    "Scene cache and filters avoid unnecessary source loads",
    "[session][cache][filter]")
{
  Session s("t");
  Quiet(s);

  auto loads = AddSeries(s, "pts", 2);

  auto scene = s.CreateScene("a");

  CHECK(scene != nullptr);

  CHECK(
      s.Resolve(
          ch::AddLayer<PE>{
              "a",
              "hits",
              dt::SeriesRef{"pts", ""}})
          .ok);

  CHECK(scene->Prepare<PE>(s.Data(), "hits").size() == 2);
  CHECK(*loads == 1);

  auto f =
      std::make_shared<fccvis::filter::Filter<PE>>(
          [](const PE &e) {
            return e.meta.Number("e").value_or(0) > 1.0;
          });

  CHECK(
      s.Resolve(
          ch::SetLayerFilter<PE>{"a", "hits", f})
          .ok);

  CHECK(scene->Prepare<PE>(s.Data(), "hits").size() == 1);
  CHECK(*loads == 1);

  CHECK(
      s.Resolve(
          ch::SetEntry<PE>{"a", "hits", 1})
          .ok);

  CHECK(scene->Prepare<PE>(s.Data(), "hits").size() == 1);
  CHECK(*loads == 2);

  fccvis::style::PointStyle red;
  red.common.color =
      fccvis::style::Color::FromHex(0xFF0000);

  CHECK(
      s.Resolve(
          ch::AddStyleRule<PE>{
              "a",
              "hits",
              fccvis::filter::Filter<PE>{},
              red})
          .ok);

  auto styled =
      scene->Prepare<PE>(s.Data(), "hits");

  CHECK(styled.size() == 1);
  CHECK(styled[0].style.common.color.has_value());

  CHECK(s.Validate().empty());
}

TEST_CASE(
    "Backend port events propagate through the session",
    "[session][backend][port]")
{
  Session s("t");
  Quiet(s);

  auto rec = Attach(s, "main");
  auto cam =
      std::make_shared<fccvis::scene::camera::Camera>("cam");

  CHECK(s.CreateScene("a") != nullptr);
  CHECK(s.Resolve(ch::SetCamera{"a", cam}).ok);
  CHECK(s.Open("main", "a").ok);

  // A change submitted by the backend carries its id as origin.
  CHECK(rec->port != nullptr);

  auto r =
      rec->port->Submit(
          ch::ChangeCameraMagnification{"a", 4.0});

  CHECK(r.ok);
  CHECK(rec->LastSync().origin == "main");
  CHECK(cam->magnification == 4.0);

  // The user closes the scene's window.
  rec->port->SceneClosed("a");

  CHECK(s.OpenScenes("main").empty());

  // The backend stops by itself: it stays attached but is no longer synced.
  const auto syncs = rec->SyncCount();

  rec->port->Stopped("window closed");

  CHECK(!s.IsRunning("main"));

  CHECK(
      s.Resolve(
          ch::ChangeCameraMagnification{"a", 1.0})
          .ok);

  CHECK(rec->SyncCount() == syncs);
  CHECK(!s.Open("main", "a").ok);

  // TODO: replace this with an explicit assertion for the intended
  // stopped-backend id-reuse semantics.
  CHECK((
      s.InitBackend("recording1", "main").ok == false ||
      true));
}
