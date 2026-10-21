// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// B1 (second viewer, T4.5): ROOT REve, the web-based event display.
//
// Registered as "reve". Serves the REve page with ROOT's THttpServer (no X
// display needed); open the logged URL in a browser (over SSH: forward the
// port). Every open scene becomes an REve scene shown in its own viewer
// (the browser arranges the viewers; split weights and tabs are not used),
// layers become element groups: one REvePointSet per point style
// (per-point picking), REveLine per line, REveStraightLineSet outlines for
// surfaces (REve has no triangle-mesh element without TGeo shapes).
// Camera: a direction along an axis selects the matching orthographic
// camera, any other a perspective one; zoom and pan stay in the browser.
// Events: picks (scene, layer, entry, element index) through an
// REveSelection deviator; when the last browser connection closes the
// backend reports Port::Stopped (option stop_on_close=false keeps it running).
// Camera moves in the browser are not sent to the server by REve, so they
// are not reported. Experiments: docs/design/root-experiments.md.
#include "BackendRegistry.h"
#include "CameraChanges.h"
#include "FileBackend.h"
#include "RootRuntime.h"

#include <ROOT/REveElement.hxx>
#include <ROOT/REveLine.hxx>
#include <ROOT/REveManager.hxx>
#include <ROOT/REvePointSet.hxx>
#include <ROOT/REveScene.hxx>
#include <ROOT/REveSelection.hxx>
#include <ROOT/REveStraightLineSet.hxx>
#include <ROOT/REveViewer.hxx>
#include <ROOT/RWebDisplayArgs.hxx>
#include <ROOT/RWebWindow.hxx>
#include <TColor.h>
#include <TEnv.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <tuple>

namespace fccvis::backend {

namespace {

namespace REX = ROOT::Experimental;
using root::RootRuntime;

Color_t RootColor(const style::Color& c) { return static_cast<Color_t>(TColor::GetColor(c.r, c.g, c.b)); }

Char_t Transparency(float alpha) { return static_cast<Char_t>(std::lround(100 * (1 - std::clamp(alpha, 0.f, 1.f)))); }

Style_t MarkerStyle(style::MarkerShape m) {
  switch (m) {
    case style::MarkerShape::Square: return 21;
    case style::MarkerShape::Diamond: return 33;
    case style::MarkerShape::Triangle: return 22;
    case style::MarkerShape::Cross: return 2;
    default: return 20;
  }
}

struct PickTarget {
  std::string scene, layer;
  std::vector<std::size_t> indices;  // point sets: element index per point; others: one
};

using Entries = std::map<std::string, std::map<std::string, std::size_t>>;

/** Process-wide routing of REve picks to the backend instance that owns the element. */
class PickRouter : public REX::REveSelection::Deviator {
 public:
  using Sink = std::function<void(REX::REveElement*, bool secondary, const std::set<int>&)>;
  static std::shared_ptr<PickRouter> Instance() {
    static auto router = std::make_shared<PickRouter>();
    return router;
  }
  void Add(const void* owner, Sink sink) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sinks[owner] = std::move(sink);
  }
  void Remove(const void* owner) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sinks.erase(owner);
  }
  bool DeviateSelection(REX::REveSelection* s, REX::REveElement* el, bool multi, bool secondary,
                        const std::set<int>& idx) override {
    std::map<const void*, Sink> sinks;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      sinks = m_sinks;
    }
    for (auto& [owner, sink] : sinks) sink(el, secondary, idx);
    ExecuteNewElementPicked(s, el, multi, secondary, idx);  // REve's own highlighting
    return true;
  }

 private:
  std::mutex m_mutex;
  std::map<const void*, Sink> m_sinks;
};

/** Starts REve once per process (on the ROOT thread); returns the page URL. */
std::string StartReve(int port, const std::string& bind) {
  static std::string url;
  RootRuntime::Instance().Call([&] {
    if (REX::gEve) return;
    if (port > 0) gEnv->SetValue("WebGui.HttpPort", port);
    gEnv->SetValue("WebGui.HttpLoopback", bind == "127.0.0.1" || bind == "localhost" ? "yes" : "no");
    if (!(bind == "127.0.0.1" || bind == "localhost")) gEnv->SetValue("WebGui.HttpBind", bind.c_str());
    auto* eve = REX::REveManager::Create();
    if (!eve) throw std::runtime_error("REveManager::Create failed");
    eve->GetSelection()->SetDeviator(PickRouter::Instance());
    // fccvis shows each open scene in its own viewer; REve's empty default viewer only takes space.
    if (auto* v = eve->GetDefaultViewer()) eve->GetViewers()->RemoveElement(v);
    eve->Show(ROOT::RWebDisplayArgs("server"));
    auto win = eve->GetWebWindow();
    if (!win) throw std::runtime_error("REve did not create its web window");
    url = win->GetUrl(true);
  });
  return url;
}

}  // namespace

/** REve backend (B1 web viewer). */
class RootWebBackend final : public FileBackend {
 public:
  RootWebBackend() : m_state(std::make_shared<State>()) {}
  ~RootWebBackend() override {
    PickRouter::Instance()->Remove(this);
    RootRuntime::Instance().RemovePoller(this);
    if (m_events) m_events->Wait();
  }

  [[nodiscard]] Capabilities GetCapabilities() const override {
    Capabilities c;
    c.description = "ROOT REve web viewer: interactive 3D views in a browser, picks";
    c.kinds = {ElementKind::Point, ElementKind::Line, ElementKind::Surface};
    c.styleProperties = {"axes", "background", "color", "edgeColor", "edgeWidth", "marker", "opacity", "size", "visible",
                         "width"};
    c.splits = false;  // the browser arranges the viewers
    c.tabs = false;
    c.options = {{"port", "HTTP port (default: a free port in ROOT's WebGui range)"},
                 {"bind", "address to listen on (default 127.0.0.1; 0.0.0.0 for other machines)"},
                 {"stop_on_close", "true (default): closing the last browser page stops the backend"},
                 {"tolerance", "relative surface tessellation tolerance (default 0.02)"}};
    return c;
  }

  void Stop() override {
    PickRouter::Instance()->Remove(this);
    auto& rt = RootRuntime::Instance();
    rt.RemovePoller(this);
    auto state = m_state;
    rt.Post([state] { state->Clear(); });
    rt.Wait();
    if (m_events) m_events->Wait();
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      state->port = nullptr;
    }
    FileBackend::Stop();
  }

  void Flush() override {
    FileBackend::Flush();
    RootRuntime::Instance().Wait();
  }

  /** @brief URL of the REve page (also logged at start). */
  [[nodiscard]] const std::string& Url() const { return m_url; }

 protected:
  [[nodiscard]] std::string Kind() const override { return "reve"; }

  void Configure(const Options& options) override {
    int port = 0;
    std::string bind = "127.0.0.1";
    bool stopOnClose = true;
    if (auto it = options.find("port"); it != options.end()) {
      try {
        std::size_t used = 0;
        port = std::stoi(it->second, &used);
        if (used != it->second.size() || port < 1 || port > 65535) throw std::out_of_range("port");
      } catch (const std::exception&) {
        throw std::invalid_argument("option 'port' must be an integer in 1..65535, got '" + it->second + "'");
      }
    }
    if (auto it = options.find("bind"); it != options.end()) bind = it->second;
    if (auto it = options.find("stop_on_close"); it != options.end()) stopOnClose = it->second != "false" && it->second != "0";
    m_url = StartReve(port, bind);
    m_events = std::make_unique<Executor>([this](const std::string& e) { Log(changes::Severity::Error, "event delivery failed: " + e); });
    auto state = m_state;
    state->id = Id();
    state->port = GetPort();
    state->events = m_events.get();
    state->stopOnClose = stopOnClose;
    PickRouter::Instance()->Add(this, [state](REX::REveElement* el, bool secondary, const std::set<int>& idx) {
      state->Picked(el, secondary, idx);
    });
    RootRuntime::Instance().AddPoller(this, [state] { state->Poll(); });
    Log(changes::Severity::Info, "REve page for '" + Id() + "': " + m_url);
  }

  [[nodiscard]] std::shared_ptr<const void> Collect(const SyncPlan&, const SyncRequest& request) override {
    auto out = std::make_shared<Entries>();
    for (const auto& name : request.open) {
      const auto scene = request.view.FindScene(name);
      if (!scene) continue;
      for (const auto& [layer, type] : scene->layers.Types()) {
        const auto kind = KindOf(type);
        if (!kind) continue;
        auto take = [&]<typename X>() {
          if (const auto* l = scene->layers.Find<X>(layer)) (*out)[name][layer] = l->entry.value_or(0);
        };
        switch (*kind) {
          case ElementKind::Point: take.template operator()<data::PointElement>(); break;
          case ElementKind::Line: take.template operator()<data::LineElement>(); break;
          case ElementKind::Surface: take.template operator()<data::SurfaceElement>(); break;
        }
      }
    }
    return out;
  }

  void Render(const Job& job) override {
    auto copy = std::make_shared<Job>(job);
    auto entries = std::static_pointer_cast<const Entries>(job.extra);
    auto state = m_state;
    RootRuntime::Instance().Post([state, copy, entries] { state->Apply(*copy, entries ? *entries : Entries{}); });
  }

 private:
  /** ROOT-side objects, touched on the ROOT thread only (except `mutex`-guarded fields). */
  struct State {
    std::mutex mutex;  // guards port and events against Stop
    std::string id;
    Port* port = nullptr;
    Executor* events = nullptr;
    bool stopOnClose = true;
    bool stopped = false;
    int lastConnections = 0;
    struct SceneObj {
      REX::REveScene* scene = nullptr;
      std::map<std::string, std::pair<std::shared_ptr<const draw::LayerDrawing>, REX::REveElement*>> layers;
    };
    std::map<std::string, SceneObj> scenes;
    std::map<std::string, REX::REveViewer*> viewers;
    std::map<std::string, draw::CameraState> cameras;
    std::map<const REX::REveElement*, PickTarget> pickable;
    Entries entries;

    void Emit(std::function<void(Port&)> f) {
      std::lock_guard<std::mutex> lock(mutex);
      if (!port || !events) return;
      Port* p = port;
      events->Post([p, f] { f(*p); });
    }

    void Picked(REX::REveElement* el, bool secondary, const std::set<int>& idx) {
      auto t = pickable.find(el);
      if (t == pickable.end()) return;
      PickEvent ev;
      ev.scene = t->second.scene;
      ev.layer = t->second.layer;
      ev.entry = entries[ev.scene][ev.layer];
      ev.index = t->second.indices.empty() ? 0 : t->second.indices.front();
      if (secondary && !idx.empty()) {
        const auto i = static_cast<std::size_t>(*idx.begin());
        if (i < t->second.indices.size()) ev.index = t->second.indices[i];
      }
      Emit([ev](Port& p) { p.Picked(ev); });
    }

    void Poll() {
      if (stopped || !REX::gEve) return;
      auto win = REX::gEve->GetWebWindow();
      const int n = win ? win->NumConnections() : 0;
      if (lastConnections > 0 && n == 0 && stopOnClose) {
        stopped = true;
        Emit([](Port& p) { p.Stopped("the browser closed the REve page"); });
      }
      lastConnections = n;
    }

    void ClearLayer(REX::REveElement* list) {
      for (auto* c : list->RefChildren()) pickable.erase(c);
      list->DestroyElements();
    }

    void Clear() {
      if (!REX::gEve) return;
      REX::gEve->BeginChange();
      for (auto& [name, so] : scenes) {
        for (auto& [ln, lo] : so.layers) ClearLayer(lo.second);
        if (so.scene) so.scene->DestroyElements();
      }
      for (auto& [name, v] : viewers) v->RemoveElements();
      REX::gEve->EndChange();
      viewers.clear();
      scenes.clear();
      pickable.clear();
    }

    void BuildLayer(const std::string& scene, const std::string& name, const draw::LayerDrawing& d,
                    REX::REveElement* list) {
      ClearLayer(list);
      std::map<std::tuple<std::uint32_t, float, int>, std::pair<REX::REvePointSet*, std::vector<std::size_t>>> sets;
      for (const auto& p : d.points) {
        if (!draw::Visible(p.s.common)) continue;
        const auto c = draw::ColorOf(p.s.common);
        const float size = p.s.size.value_or(3.f);
        const int marker = static_cast<int>(p.s.marker.value_or(style::MarkerShape::Circle));
        auto& [set, indices] = sets[std::make_tuple((std::uint32_t(c.r) << 16) | (c.g << 8) | c.b, size, marker)];
        if (!set) {
          set = new REX::REvePointSet(name + " points");
          set->SetMarkerColor(RootColor(c));
          set->SetMarkerSize(size);
          set->SetMarkerStyle(MarkerStyle(static_cast<style::MarkerShape>(marker)));
          set->SetMainTransparency(Transparency(draw::Alpha(p.s.common)));
          set->SetAlwaysSecSelect(true);  // picks report the point
          list->AddElement(set);
        }
        set->SetNextPoint(static_cast<float>(p.p[0]), static_cast<float>(p.p[1]), static_cast<float>(p.p[2]));
        indices.push_back(p.index);
      }
      for (auto& [k, v] : sets) pickable[v.first] = {scene, name, v.second};
      for (const auto& l : d.lines) {
        if (!draw::Visible(l.s.common) || l.v.size() < 2) continue;
        auto* line = new REX::REveLine(name + " #" + std::to_string(l.index));
        for (const auto& v : l.v) line->SetNextPoint(static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2]));
        line->SetLineColor(RootColor(draw::ColorOf(l.s.common)));
        line->SetLineWidth(static_cast<Width_t>(std::max(1.f, l.s.width.value_or(1.f))));
        line->SetMainTransparency(Transparency(draw::Alpha(l.s.common)));
        list->AddElement(line);
        pickable[line] = {scene, name, {l.index}};
      }
      for (const auto& m : d.meshes) {
        if (!draw::Visible(m.s.common) || m.edges.empty()) continue;
        const auto col = m.s.edgeColor.value_or(draw::ColorOf(m.s.common));
        auto* e = new REX::REveStraightLineSet(name + " #" + std::to_string(m.index));
        for (const auto& [a, b] : m.edges) {
          const auto& p = m.vertices[a];
          const auto& q = m.vertices[b];
          e->AddLine(static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2]), static_cast<float>(q[0]),
                     static_cast<float>(q[1]), static_cast<float>(q[2]));
        }
        e->SetLineColor(RootColor(col));
        e->SetLineWidth(static_cast<Width_t>(std::max(1.f, m.s.edgeWidth.value_or(1.f))));
        list->AddElement(e);
        pickable[e] = {scene, name, {m.index}};
      }
    }

    static REX::REveViewer::ECameraType CameraType(const draw::CameraState& c) {
      const auto& d = c.direction;
      const double n = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
      if (n > 0) {
        // Same viewing directions as the classic TEve backend (measured there).
        if (std::abs(d[2]) / n > 0.999) return d[2] > 0 ? REX::REveViewer::kCameraOrthoXnOY : REX::REveViewer::kCameraOrthoXOY;
        if (std::abs(d[0]) / n > 0.999) return d[0] > 0 ? REX::REveViewer::kCameraOrthoZOY : REX::REveViewer::kCameraOrthoZnOY;
        if (std::abs(d[1]) / n > 0.999) return d[1] > 0 ? REX::REveViewer::kCameraOrthoXOZ : REX::REveViewer::kCameraOrthoXnOZ;
      }
      return REX::REveViewer::kCameraPerspXOY;
    }

    void Apply(const Job& job, const Entries& newEntries) {
      if (!REX::gEve) return;
      entries = newEntries;
      REX::gEve->BeginChange();
      for (auto it = scenes.begin(); it != scenes.end();) {  // scenes no longer shown
        if (job.open.count(it->first) && job.scenes.count(it->first)) {
          ++it;
          continue;
        }
        for (auto& [ln, lo] : it->second.layers) ClearLayer(lo.second);
        if (it->second.scene) it->second.scene->DestroyElements();
        if (auto v = viewers.find(it->first); v != viewers.end()) {
          v->second->RemoveElements();
          viewers.erase(v);
        }
        cameras.erase(it->first);
        it = scenes.erase(it);
      }
      for (const auto& name : job.open) {
        auto d = job.scenes.find(name);
        if (d == job.scenes.end()) continue;
        auto& so = scenes[name];
        if (!so.scene) {
          so.scene = REX::gEve->SpawnNewScene((id + ": " + name).c_str());
          auto* v = REX::gEve->SpawnNewViewer((id + ": " + name).c_str());
          v->AddScene(so.scene);
          viewers[name] = v;
        }
        for (auto it = so.layers.begin(); it != so.layers.end();) {
          if (d->second.layers.count(it->first)) {
            ++it;
            continue;
          }
          ClearLayer(it->second.second);
          so.scene->RemoveElement(it->second.second);
          it = so.layers.erase(it);
        }
        for (const auto& [lname, drawing] : d->second.layers) {
          auto& [current, list] = so.layers[lname];
          if (!list) {
            list = new REX::REveElement(lname);
            so.scene->AddElement(list);
          }
          if (current != drawing && drawing) {
            BuildLayer(name, lname, *drawing, list);
            current = drawing;
          }
        }
        const auto& cam = d->second.camera;
        auto c = cameras.find(name);
        if (c == cameras.end() || c->second.direction != cam.direction) viewers[name]->SetCameraType(CameraType(cam));
        cameras[name] = cam;
        const auto& env = d->second.environment;
        const auto bg = env.background.value_or(style::Color{255, 255, 255, 255});
        viewers[name]->SetBlackBackground(bg.r + bg.g + bg.b < 3 * 128);
        viewers[name]->SetAxesType(env.axes.value_or(false) ? 1 : 0);
      }
      REX::gEve->EndChange();
    }
  };

  std::shared_ptr<State> m_state;
  std::unique_ptr<Executor> m_events;
  std::string m_url;
};

static Registrar g_reveRegistrar("reve", [] { return std::make_unique<RootWebBackend>(); });

}  // namespace fccvis::backend
