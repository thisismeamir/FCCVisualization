// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "RootBackend.h"

#include "CameraChanges.h"
#include "RootRuntime.h"

#include <TColor.h>
#include <TEveBrowser.h>
#include <TEveElement.h>
#include <TEveLine.h>
#include <TEveManager.h>
#include <TEvePointSet.h>
#include <TEveScene.h>
#include <TEveSelection.h>
#include <TEveStraightLineSet.h>
#include <TEveTriangleSet.h>
#include <TEveViewer.h>
#include <TEveWindow.h>
#include <TGLCamera.h>
#include <TGLUtil.h>
#include <TGLViewer.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <functional>
#include <optional>
#include <tuple>
#include <cmath>
#include <filesystem>
#include <map>
#include <set>
#include <variant>

namespace fccvis::backend {

namespace {

using root::RootRuntime;

Color_t RootColor(const style::Color& c) { return static_cast<Color_t>(TColor::GetColor(c.r, c.g, c.b)); }

Style_t MarkerStyle(style::MarkerShape m) {
  switch (m) {
    case style::MarkerShape::Square: return 21;
    case style::MarkerShape::Diamond: return 33;
    case style::MarkerShape::Triangle: return 22;
    case style::MarkerShape::Cross: return 2;
    default: return 20;
  }
}

Style_t LineStyle(style::DashPattern d) {
  switch (d) {
    case style::DashPattern::Dashed: return 2;
    case style::DashPattern::Dotted: return 3;
    case style::DashPattern::DashDot: return 4;
    default: return 1;
  }
}

/** Viewing direction of a GL camera (what its Apply() passes to gluLookAt). */
std::array<double, 3> ViewDirection(const TGLCamera& cam) {
  TGLMatrix mx = cam.GetCamBase() * cam.GetCamTrans();
  const TGLVector3 fwd = mx.GetBaseVec(1);
  const double n = fwd.Mag();
  if (n <= 0) return {0, 0, 1};
  return {-fwd.X() / n, -fwd.Y() / n, -fwd.Z() / n};
}

/** Point set that remembers which point Eve's secondary selection picked. */
class FccPointSet final : public TEvePointSet {
 public:
  explicit FccPointSet(const char* name) : TEvePointSet(name) {}
  void PointSelected(Int_t id) override {
    lastPicked = id;
    Emit("PointSelected(Int_t)", id);  // TPointSet3D::PointSelected only prints the point's reference object
  }
  std::vector<std::size_t> indices;  // element index of each point
  std::atomic<int> lastPicked{-1};
};

struct PickTarget {
  std::string scene, layer;
  std::vector<std::size_t> indices;  // points: per point; lines/surfaces: one
  FccPointSet* points = nullptr;
};

}  // namespace

struct RootBackend::State {
  std::string id;
  Port* port = nullptr;
  Executor* events = nullptr;
  int width = 800, height = 600;
  std::string pictureDir;
  bool stopped = false;

  struct SceneObj {
    TEveScene* scene = nullptr;
    std::map<std::string, std::pair<std::shared_ptr<const draw::LayerDrawing>, TEveElementList*>> layers;
  };
  std::map<std::string, SceneObj> scenes;
  std::map<std::string, std::map<std::string, std::size_t>> entries;  // scene -> layer -> entry
  TEveWindow* top = nullptr;
  std::map<std::string, std::vector<TEveViewer*>> viewers;  // scene -> viewers
  std::optional<scene::layout::LayoutNode> layout;
  std::set<std::string> open;
  bool laidOut = false;
  std::map<std::string, draw::CameraState> cameras;  // applied per scene
  std::map<TEveElement*, PickTarget> pickable;
  std::set<TEveElement*> lastSelection;
  struct CamWatch {
    std::string scene;
    TGLMatrix baseline;
    bool pending = true;
  };
  std::map<TGLViewer*, CamWatch> watch;

  void Emit(std::function<void(Port&)> f) {
    if (!port || !events) return;
    Port* p = port;
    events->Post([p, f] { f(*p); });
  }

  // -------------------------------------------------------------------------
  void ClearLayer(TEveElementList* list) {
    for (auto it = list->BeginChildren(); it != list->EndChildren(); ++it) pickable.erase(*it);
    list->DestroyElements();
  }

  void BuildLayer(const std::string& scene, const std::string& name, const draw::LayerDrawing& d, TEveElementList* list) {
    ClearLayer(list);
    // points: one set per style
    std::map<std::tuple<std::uint32_t, float, int>, FccPointSet*> sets;
    for (const auto& p : d.points) {
      if (!draw::Visible(p.s.common)) continue;
      const auto c = draw::ColorOf(p.s.common);
      const float size = p.s.size.value_or(3.f);
      const int marker = static_cast<int>(p.s.marker.value_or(style::MarkerShape::Circle));
      const auto key = std::make_tuple((std::uint32_t(c.r) << 16) | (c.g << 8) | c.b, size, marker);
      auto& set = sets[key];
      if (!set) {
        set = new FccPointSet((name + " points").c_str());
        set->SetMarkerColor(RootColor(c));
        set->SetMarkerSize(size / 3.f);
        set->SetMarkerStyle(MarkerStyle(static_cast<style::MarkerShape>(marker)));
        set->SetMainTransparency(static_cast<Char_t>(100 * (1 - draw::Alpha(p.s.common))));
        list->AddElement(set);
      }
      set->SetNextPoint(p.p[0], p.p[1], p.p[2]);
      set->indices.push_back(p.index);
    }
    for (auto& [k, set] : sets) pickable[set] = {scene, name, set->indices, set};
    for (const auto& l : d.lines) {
      if (!draw::Visible(l.s.common) || l.v.size() < 2) continue;
      auto* line = new TEveLine((name + " #" + std::to_string(l.index)).c_str());
      for (const auto& v : l.v) line->SetNextPoint(v[0], v[1], v[2]);
      line->SetLineColor(RootColor(draw::ColorOf(l.s.common)));
      line->SetLineWidth(static_cast<Width_t>(std::max(1.f, l.s.width.value_or(1.f))));
      line->SetLineStyle(LineStyle(l.s.dash.value_or(style::DashPattern::Solid)));
      line->SetMainTransparency(static_cast<Char_t>(100 * (1 - draw::Alpha(l.s.common))));
      list->AddElement(line);
      pickable[line] = {scene, name, {l.index}, nullptr};
    }
    for (const auto& m : d.meshes) {
      if (!draw::Visible(m.s.common)) continue;
      const std::string label = name + " #" + std::to_string(m.index);
      if (!m.s.wireframe.value_or(false) && !m.triangles.empty()) {
        auto* t = new TEveTriangleSet(static_cast<Int_t>(m.vertices.size()), static_cast<Int_t>(m.triangles.size()));
        t->SetElementName(label.c_str());
        for (std::size_t i = 0; i < m.vertices.size(); ++i)
          t->SetVertex(static_cast<Int_t>(i), static_cast<Float_t>(m.vertices[i][0]),
                       static_cast<Float_t>(m.vertices[i][1]), static_cast<Float_t>(m.vertices[i][2]));
        for (std::size_t i = 0; i < m.triangles.size(); ++i)
          t->SetTriangle(static_cast<Int_t>(i), static_cast<Int_t>(m.triangles[i][0]),
                         static_cast<Int_t>(m.triangles[i][1]), static_cast<Int_t>(m.triangles[i][2]));
        t->GenerateTriangleNormals();
        t->SetMainColor(RootColor(draw::ColorOf(m.s.common)));
        t->SetMainTransparency(static_cast<Char_t>(100 * (1 - draw::Alpha(m.s.common))));
        list->AddElement(t);
        pickable[t] = {scene, name, {m.index}, nullptr};
      }
      // Outline (boundary edges and creases), as in the file backends.
      const float w = m.s.edgeWidth.value_or(1.f);
      if (w > 0 && !m.edges.empty()) {
        const auto col = m.s.edgeColor.value_or(draw::ColorOf(m.s.common));
        auto* e = new TEveStraightLineSet((label + " edges").c_str());
        for (const auto& [a, b] : m.edges) {
          const auto& p = m.vertices[a];
          const auto& q = m.vertices[b];
          e->AddLine(static_cast<Float_t>(p[0]), static_cast<Float_t>(p[1]), static_cast<Float_t>(p[2]),
                     static_cast<Float_t>(q[0]), static_cast<Float_t>(q[1]), static_cast<Float_t>(q[2]));
        }
        e->SetLineColor(RootColor(col));
        e->SetLineWidth(static_cast<Width_t>(std::max(1.f, w)));
        e->SetMainTransparency(static_cast<Char_t>(100 * (1 - col.a / 255.f)));
        list->AddElement(e);
        pickable[e] = {scene, name, {m.index}, nullptr};
      }
    }
  }

  // -------------------------------------------------------------------------
  void ApplyCamera(TEveViewer* v, const draw::CameraState& c) {
    TGLViewer* gl = v->GetGLViewer();
    double d[3] = {c.direction[0], c.direction[1], c.direction[2]};
    const double n = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (n > 0)
      for (double& x : d) x /= n;
    // Fit the scene (bounding box of the repainted scenes), then pan and zoom like draw::View.
    gl->UpdateScene(kFALSE);
    const double mag = c.magnification > 0 ? c.magnification : 1.0;
    // Axis-aligned directions use the orthographic cameras (directions measured with ViewDirection).
    TGLViewer::ECameraType type = TGLViewer::kCameraPerspXOY;
    bool ortho = true;
    if (std::abs(d[2]) > 0.999) type = d[2] > 0 ? TGLViewer::kCameraOrthoXnOY : TGLViewer::kCameraOrthoXOY;
    else if (std::abs(d[0]) > 0.999) type = d[0] > 0 ? TGLViewer::kCameraOrthoZOY : TGLViewer::kCameraOrthoZnOY;
    else if (std::abs(d[1]) > 0.999) type = d[1] > 0 ? TGLViewer::kCameraOrthoXOZ : TGLViewer::kCameraOrthoXnOZ;
    else ortho = false;
    gl->SetCurrentCamera(type);
    gl->ResetCurrentCamera();
    const TGLVertex3 mid = gl->RefOverallBoundingBox().Center();
    double center[3] = {mid.X() + c.position[0], mid.Y() + c.position[1], mid.Z() + c.position[2]};
    if (ortho) {
      // SetOrthoCamera only accepts the four non-negated types; Configure works for all.
      gl->RefCamera(type).Configure(0.78 * mag, 0, center, 0, 0);  // 0.78: ROOT's default ortho zoom (fits the box with a margin)
    } else {
      // Perspective camera with the XOY floor (z up): after a reset it looks along +x, and
      // Configure(h, v) gives the direction (cos h cos v, -cos h sin v, sin h).
      const double h = std::asin(std::clamp(d[2], -1.0, 1.0)), vr = std::atan2(-d[1], d[0]);
      gl->RefCamera(type).Configure(30.0 / mag, 0, center, h, vr);
    }
    gl->RequestDraw();
    watch[gl].pending = true;
  }

  void ApplyEnvironment(TEveViewer* v, const style::EnvironmentStyle& e) {
    TGLViewer* gl = v->GetGLViewer();
    gl->SetClearColor(RootColor(e.background.value_or(style::Color{255, 255, 255, 255})));
    double ref[3] = {0, 0, 0};
    gl->SetGuideState(e.axes.value_or(false) ? TGLUtil::kAxesOrigin : TGLUtil::kAxesNone, kFALSE, kFALSE, ref);
  }

  void BuildWindows(const std::optional<scene::layout::LayoutNode>& lay, const std::set<std::string>& openScenes) {
    if (top) {
      top->DestroyWindowAndSlot();
      top = nullptr;
    }
    viewers.clear();
    watch.clear();
    if (openScenes.empty()) return;
    TEveWindowSlot* slot = TEveWindow::CreateWindowInTab(gEve->GetBrowser()->GetTabRight());
    auto leaf = [&](TEveWindowSlot* s, const std::string& sceneName, const std::string& label) {
      s->MakeCurrent();
      TEveViewer* v = gEve->SpawnNewViewer(label.c_str(), "");
      auto it = scenes.find(sceneName);
      if (it != scenes.end() && it->second.scene) v->AddScene(it->second.scene);
      v->GetGLViewer()->SetResetCamerasOnUpdate(kFALSE);
      viewers[sceneName].push_back(v);
      watch[v->GetGLViewer()] = {sceneName, TGLMatrix(), true};
      return static_cast<TEveWindow*>(v);
    };
    std::function<TEveWindow*(TEveWindowSlot*, const scene::layout::LayoutNode&, const std::string&)> place =
        [&](TEveWindowSlot* s, const scene::layout::LayoutNode& node, const std::string& label) -> TEveWindow* {
      return std::visit(
          [&](const auto& n) -> TEveWindow* {
            using N = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<N, scene::layout::Pane>) {
              return leaf(s, n.sceneName, label.empty() ? n.sceneName : label);
            } else if constexpr (std::is_same_v<N, scene::layout::Split>) {
              TEveWindowPack* pack = s->MakePack();
              pack->SetShowTitleBar(kFALSE);
              pack->SetVertical(n.orientation == scene::layout::Orientation::Vertical);
              for (const auto& c : n.children) place(pack->NewSlot(), c.node, c.label.value_or(""));
              return pack;
            } else {
              TEveWindowTab* tab = s->MakeTab();
              tab->SetShowTitleBar(kFALSE);
              for (const auto& c : n.children) place(tab->NewSlot(), c.node, c.label.value_or(""));
              return tab;
            }
          },
          node.value);
    };
    if (lay) {
      top = place(slot, *lay, "");
    } else {  // no layout: open scenes side by side
      TEveWindowPack* pack = slot->MakePack();
      pack->SetShowTitleBar(kFALSE);
      pack->SetHorizontal();
      for (const auto& sc : openScenes) leaf(pack->NewSlot(), sc, sc);
      top = pack;
    }
    if (top) top->SetElementName(("fccvis " + id).c_str());
  }

  // -------------------------------------------------------------------------
  void Apply(const Job& job, const std::map<std::string, std::map<std::string, std::size_t>>& newEntries) {
    if (!gEve) return;  // the Eve window was closed
    entries = newEntries;
    // scenes no longer shown
    for (auto it = scenes.begin(); it != scenes.end();) {
      if (job.open.count(it->first) && job.scenes.count(it->first)) {
        ++it;
        continue;
      }
      for (auto& [ln, lo] : it->second.layers) ClearLayer(lo.second);
      if (it->second.scene) it->second.scene->Destroy();
      cameras.erase(it->first);
      it = scenes.erase(it);
    }
    bool newScene = false;
    std::set<std::string> dirty;  // scenes whose content changed
    for (const auto& name : job.open) {
      auto d = job.scenes.find(name);
      if (d == job.scenes.end()) continue;
      auto& so = scenes[name];
      if (!so.scene) {
        so.scene = gEve->SpawnNewScene((id + ": " + name).c_str());
        newScene = true;
        dirty.insert(name);
      }
      for (auto it = so.layers.begin(); it != so.layers.end();) {
        if (d->second.layers.count(it->first)) {
          ++it;
          continue;
        }
        ClearLayer(it->second.second);
        it->second.second->Destroy();
        it = so.layers.erase(it);
        dirty.insert(name);
      }
      for (const auto& [lname, drawing] : d->second.layers) {
        auto& [current, list] = so.layers[lname];
        if (!list) {
          list = new TEveElementList(lname.c_str());
          so.scene->AddElement(list);
        }
        if (current != drawing && drawing) {
          BuildLayer(name, lname, *drawing, list);
          current = drawing;
          dirty.insert(name);
        }
      }
    }
    // Rebuild the windows when the set of open scenes or the layout changed (cheap compared to geometry).
    const bool sameLayout = laidOut && !newScene && open == job.open &&
                            layout.has_value() == job.layout.has_value() &&
                            (!layout || FormatKey(*layout) == FormatKey(*job.layout));
    if (!sameLayout) {
      BuildWindows(job.layout, job.open);
      laidOut = true;
      layout = job.layout;
      open = job.open;
      cameras.clear();
    }
    for (const auto& name : dirty) scenes[name].scene->Repaint();
    for (const auto& name : job.open) {
      auto d = job.scenes.find(name);
      if (d == job.scenes.end()) continue;
      const auto& cam = d->second.camera;
      auto c = cameras.find(name);
      const bool camChanged = c == cameras.end() || c->second.direction != cam.direction ||
                              c->second.position != cam.position || c->second.magnification != cam.magnification;
      for (auto* v : viewers[name]) {
        if (camChanged) ApplyCamera(v, cam);
        ApplyEnvironment(v, d->second.environment);
      }
      cameras[name] = cam;
    }
    gEve->Redraw3D(kFALSE);
    if (!pictureDir.empty()) {
      std::filesystem::create_directories(pictureDir);
      for (auto& [name, vs] : viewers)
        if (!vs.empty()) {
          vs.front()->GetGLViewer()->DoDraw(kFALSE);
          vs.front()->GetGLViewer()->SavePictureUsingFBO((pictureDir + "/" + name + ".png").c_str(), width, height);
        }
    }
  }

  static std::string FormatKey(const scene::layout::LayoutNode& n) {
    return std::visit(
        [](const auto& v) -> std::string {
          using V = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<V, scene::layout::Pane>) {
            return "P(" + v.sceneName + ")";
          } else {
            std::string s = std::is_same_v<V, scene::layout::Split> ? "S(" : "T(";
            if constexpr (std::is_same_v<V, scene::layout::Split>)
              s += v.orientation == scene::layout::Orientation::Vertical ? "v" : "h";
            for (const auto& c : v.children) s += FormatKey(c.node) + "|" + c.label.value_or("") + ",";
            return s + ")";
          }
        },
        n.value);
  }

  // -------------------------------------------------------------------------
  void Poll() {
    if (stopped) return;
    if (!gEve) {
      stopped = true;
      Emit([](Port& p) { p.Stopped("the ROOT Eve window was closed"); });
      return;
    }
    // picks
    std::set<TEveElement*> now;
    TEveSelection* sel = gEve->GetSelection();
    for (auto it = sel->BeginChildren(); it != sel->EndChildren(); ++it) now.insert(*it);
    for (TEveElement* e : now) {
      if (lastSelection.count(e)) continue;
      auto t = pickable.find(e);
      if (t == pickable.end()) continue;
      PickEvent ev;
      ev.scene = t->second.scene;
      ev.layer = t->second.layer;
      ev.entry = entries[ev.scene][ev.layer];
      std::size_t index = t->second.indices.empty() ? 0 : t->second.indices.front();
      if (t->second.points) {
        const int picked = t->second.points->lastPicked.exchange(-1);
        if (picked >= 0 && static_cast<std::size_t>(picked) < t->second.indices.size()) index = t->second.indices[static_cast<std::size_t>(picked)];
      }
      ev.index = index;
      Emit([ev](Port& p) { p.Picked(ev); });
    }
    lastSelection = now;
    // camera moves made in a viewer
    for (auto& [gl, w] : watch) {
      const TGLMatrix m = gl->CurrentCamera().GetCamTrans();
      if (w.pending) {
        w.baseline = m;
        w.pending = false;
        continue;
      }
      bool moved = false;
      for (int i = 0; i < 16; ++i) moved |= std::abs(m.CArr()[i] - w.baseline.CArr()[i]) > 1e-9;
      if (!moved) continue;
      w.baseline = m;
      const auto dir = ViewDirection(gl->CurrentCamera());
      std::vector<double> d = {dir[0], dir[1], dir[2]};
      cameras[w.scene].direction = {d[0], d[1], d[2]};
      const std::string scene = w.scene;
      Emit([scene, d](Port& p) { p.Submit(changes::ChangeCameraDirection{scene, d}); });
    }
  }
};

RootBackend::RootBackend() : m_state(std::make_shared<State>()) {}

RootBackend::~RootBackend() {
  RootRuntime::Instance().RemovePoller(this);
  if (m_events) m_events->Wait();
}

Capabilities RootBackend::GetCapabilities() const {
  Capabilities c;
  c.description = "ROOT classic TEve viewer: interactive 3D views in the Eve window, picks and camera moves";
  c.kinds = {ElementKind::Point, ElementKind::Line, ElementKind::Surface};
  c.styleProperties = {"axes",   "background", "color", "dash",  "edgeColor", "edgeWidth",
                       "marker", "opacity",    "size",  "visible", "width",    "wireframe"};
  c.splits = true;
  c.tabs = true;
  c.options = {{"picture_dir", "write <dir>/<scene>.png after every update (offscreen FBO picture)"},
               {"width", "picture width in pixels (default 800)"},
               {"height", "picture height in pixels (default 600)"},
               {"tolerance", "relative surface tessellation tolerance (default 0.02)"}};
  return c;
}

void RootBackend::Configure(const Options& options) {
  if (auto it = options.find("picture_dir"); it != options.end()) m_pictureDir = it->second;
  RootRuntime::Instance().Ensure();  // throws without a display
  m_events = std::make_unique<Executor>([this](const std::string& e) { Log(changes::Severity::Error, "event delivery failed: " + e); });
  auto state = m_state;
  state->id = Id();
  state->port = GetPort();
  state->events = m_events.get();
  state->width = m_width;
  state->height = m_height;
  state->pictureDir = m_pictureDir;
  RootRuntime::Instance().AddPoller(this, [state] { state->Poll(); });
}

std::shared_ptr<const void> RootBackend::Collect(const SyncPlan&, const SyncRequest& request) {
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

void RootBackend::Render(const Job& job) {
  auto copy = std::make_shared<Job>(job);
  auto entries = std::static_pointer_cast<const Entries>(job.extra);
  auto state = m_state;
  RootRuntime::Instance().Post([state, copy, entries] { state->Apply(*copy, entries ? *entries : Entries{}); });
}

void RootBackend::Flush() {
  FileBackend::Flush();
  RootRuntime::Instance().Wait();
}

void RootBackend::Stop() {
  auto& rt = RootRuntime::Instance();
  rt.RemovePoller(this);
  auto state = m_state;
  if (rt.Alive()) {
    rt.Post([state] {
      if (!gEve) return;
      if (state->top) state->top->DestroyWindowAndSlot();
      state->top = nullptr;
      for (auto& [n, so] : state->scenes)
        if (so.scene) so.scene->Destroy();
      state->scenes.clear();
      state->viewers.clear();
      state->watch.clear();
      state->pickable.clear();
    });
    rt.Wait();
  }
  if (m_events) m_events->Wait();
  state->port = nullptr;
  FileBackend::Stop();
}

static Registrar g_rootRegistrar("root", [] { return std::make_unique<RootBackend>(); });

}  // namespace fccvis::backend
