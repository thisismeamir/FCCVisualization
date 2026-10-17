// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "Drawing.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <utility>
#include <variant>

namespace fccvis::backend::draw {

namespace {

using PE = data::PointElement;
using LE = data::LineElement;
using SE = data::SurfaceElement;

double Dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 Cross(const Vec3& a, const Vec3& b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 Normalized(const Vec3& a, const Vec3& fallback) {
  const double n = std::sqrt(Dot(a, a));
  if (!(n > 0) || !std::isfinite(n)) return fallback;
  return {a[0] / n, a[1] / n, a[2] / n};
}

CameraState CameraOf(const scene::Scene& s) {
  CameraState c;
  if (!s.camera) return c;
  const auto& pos = s.camera->positionVector;
  const auto& dir = s.camera->directionVector;
  for (std::size_t i = 0; i < 3; ++i) {
    if (i < pos.size()) c.position[i] = pos[i];
    if (i < dir.size()) c.direction[i] = dir[i];
  }
  c.magnification = s.camera->magnification > 0 ? s.camera->magnification : 1.0;
  return c;
}

void Visit(const std::optional<scene::layout::LayoutNode>& node, Rect r, std::vector<PaneRect>& out,
           std::vector<std::string>* hidden, const std::optional<std::string>& label);

void VisitNode(const scene::layout::LayoutNode& n, Rect r, std::vector<PaneRect>& out,
               std::vector<std::string>* hidden, const std::optional<std::string>& label) {
  std::visit(
      [&](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, scene::layout::Pane>) {
          out.push_back({v.sceneName, r, label.value_or(v.sceneName)});
        } else if constexpr (std::is_same_v<T, scene::layout::Split>) {
          double total = 0;
          for (const auto& c : v.children) total += std::max(c.weight, 1);
          double offset = 0;
          for (const auto& c : v.children) {
            const double f = std::max(c.weight, 1) / total;
            Rect sub = r;
            if (v.orientation == scene::layout::Orientation::Horizontal) {
              sub.x = r.x + offset * r.w;
              sub.w = f * r.w;
            } else {
              sub.y = r.y + offset * r.h;
              sub.h = f * r.h;
            }
            offset += f;
            VisitNode(c.node, sub, out, hidden, c.label);
          }
        } else {
          for (std::size_t i = 0; i < v.children.size(); ++i) {
            if (i == 0) {
              VisitNode(v.children[i].node, r, out, hidden, v.children[i].label);
            } else if (hidden) {
              std::vector<PaneRect> skipped;
              VisitNode(v.children[i].node, r, skipped, nullptr, v.children[i].label);
              for (const auto& p : skipped) hidden->push_back(p.scene);
            }
          }
        }
      },
      n.value);
}

void Visit(const std::optional<scene::layout::LayoutNode>& node, Rect r, std::vector<PaneRect>& out,
           std::vector<std::string>* hidden, const std::optional<std::string>& label) {
  if (node) VisitNode(*node, r, out, hidden, label);
}

}  // namespace

Defaults ThemeDefaults() {
  Defaults d;
  d.point.common.color = style::Color::FromHex(0x7017FF);
  d.point.common.opacity = 1.0f;
  d.point.common.visible = true;
  d.point.size = 3.0f;
  d.point.marker = style::MarkerShape::Circle;
  d.line.common.color = style::Color::FromHex(0x393939);
  d.line.common.opacity = 1.0f;
  d.line.common.visible = true;
  d.line.width = 1.0f;
  d.line.dash = style::DashPattern::Solid;
  d.surface.common.color = style::Color::FromHex(0xC6C6C6);
  d.surface.common.opacity = 0.3f;
  d.surface.common.visible = true;
  d.surface.edgeColor = style::Color::FromHex(0x6F6F6F);
  d.surface.edgeWidth = 1.0f;
  d.surface.wireframe = false;
  d.environment.background = style::Color::FromHex(0xFFFFFF);
  d.environment.axes = false;
  d.environment.grid = false;
  return d;
}

std::shared_ptr<const LayerDrawing> BuildLayer(const scene::Scene& scene, const data::SessionData& data,
                                               const std::string& layer, const Defaults& defaults,
                                               double tolerance, const std::set<ElementKind>& kinds) {
  auto out = std::make_shared<LayerDrawing>();
  out->name = layer;
  const auto types = scene.layers.Types();
  auto it = types.find(layer);
  if (it == types.end()) return out;
  const auto kind = KindOf(it->second);
  if (!kind || !kinds.count(*kind)) return out;

  switch (*kind) {
    case ElementKind::Point: {
      const auto* l = scene.layers.Find<PE>(layer);
      if (!l || !l->series) break;
      auto entry = data.Entry<PE>(*l->series, l->entry.value_or(0));
      if (!entry) break;
      for (std::size_t i = 0; i < entry->size(); ++i) {
        const auto& x = (*entry)[i];
        if (!l->Passes(x, i)) continue;
        out->points.push_back({x.geometry.position, l->styles.Resolve(x, i).CompleteBy(defaults.point), i});
      }
      break;
    }
    case ElementKind::Line: {
      const auto* l = scene.layers.Find<LE>(layer);
      if (!l || !l->series) break;
      auto entry = data.Entry<LE>(*l->series, l->entry.value_or(0));
      if (!entry) break;
      for (std::size_t i = 0; i < entry->size(); ++i) {
        const auto& x = (*entry)[i];
        if (!l->Passes(x, i)) continue;
        out->lines.push_back({x.geometry.vertices, l->styles.Resolve(x, i).CompleteBy(defaults.line), i});
      }
      break;
    }
    case ElementKind::Surface: {
      const auto* l = scene.layers.Find<SE>(layer);
      if (!l || !l->series) break;
      auto entry = data.Entry<SE>(*l->series, l->entry.value_or(0));
      if (!entry) break;
      for (std::size_t i = 0; i < entry->size(); ++i) {
        const auto& x = (*entry)[i];
        if (!l->Passes(x, i)) continue;
        const auto g = x.geometry.shape.GeometryPtr();
        if (!g) continue;
        MeshPrim m;
        m.s = l->styles.Resolve(x, i).CompleteBy(defaults.surface);
        m.index = i;
        const auto mesh = g->Tessellate(tolerance);
        const auto& t = x.geometry.shape.GetTransform();
        m.vertices.reserve(mesh.vertices.size());
        for (const auto& v : mesh.vertices) m.vertices.push_back(t.Apply({v[0], v[1], v[2]}));
        m.triangles = mesh.triangles;
        m.edges = FeatureEdges(m);
        out->meshes.push_back(std::move(m));
      }
      break;
    }
  }
  return out;
}

void RetainedScenes::Apply(const SyncPlan& plan, const SessionView& view) {
  for (const auto& s : plan.closed) m_scenes.erase(s);
  for (const auto& [name, u] : plan.scenes) {
    const auto scene = view.FindScene(name);
    if (!scene) {
      m_scenes.erase(name);
      continue;
    }
    auto& d = m_scenes[name];
    d.name = name;
    d.camera = CameraOf(*scene);
    d.environment = scene->environment.CompleteBy(m_defaults.environment);
    for (const auto& gone : u.removed) d.layers.erase(gone);
    for (const auto& layer : u.layers)
      d.layers[layer] = BuildLayer(*scene, view.Data(), layer, m_defaults, m_tolerance, m_kinds);
    // drop layers that no longer exist (e.g. after a full redraw)
    const auto names = scene->layers.Names();
    for (auto it = d.layers.begin(); it != d.layers.end();)
      it = std::find(names.begin(), names.end(), it->first) == names.end() ? d.layers.erase(it) : std::next(it);
  }
}

View::View(const SceneDrawing& d, double x0, double y0, double w, double h, double margin) {
  const Vec3 f = Normalized(d.camera.direction, {0, 0, 1});
  const Vec3 up0 = std::abs(f[1]) > 0.99 ? Vec3{0, 0, 1} : Vec3{0, 1, 0};
  const Vec3 right = Normalized(Cross(f, up0), {1, 0, 0});
  const Vec3 up = Cross(right, f);
  m_basis = {right, up, f};

  double lo[2] = {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
  double hi[2] = {-lo[0], -lo[1]};
  auto add = [&](const Vec3& p) {
    const double u = Dot(p, right), v = Dot(p, up);
    lo[0] = std::min(lo[0], u);
    hi[0] = std::max(hi[0], u);
    lo[1] = std::min(lo[1], v);
    hi[1] = std::max(hi[1], v);
  };
  for (const auto& [n, l] : d.layers) {
    if (!l) continue;
    for (const auto& p : l->points) add(p.p);
    for (const auto& line : l->lines)
      for (const auto& v : line.v) add(v);
    for (const auto& m : l->meshes)
      for (const auto& v : m.vertices) add(v);
  }
  if (!(lo[0] <= hi[0])) lo[0] = hi[0] = lo[1] = hi[1] = 0;
  double extentU = hi[0] - lo[0], extentV = hi[1] - lo[1];
  if (extentU <= 0 && extentV <= 0) extentU = extentV = 1;  // single point: 1 mm across
  const double m = margin * std::min(w, h);
  const double sx = extentU > 0 ? (w - 2 * m) / extentU : std::numeric_limits<double>::infinity();
  const double sy = extentV > 0 ? (h - 2 * m) / extentV : std::numeric_limits<double>::infinity();
  m_scale = std::max(std::min(sx, sy), 1e-9) * d.camera.magnification;
  m_cx = (lo[0] + hi[0]) / 2 + Dot(d.camera.position, right);
  m_cy = (lo[1] + hi[1]) / 2 + Dot(d.camera.position, up);
  m_ox = x0 + w / 2;
  m_oy = y0 + h / 2;
}

Projected View::Project(const Vec3& p) const {
  return {m_ox + (Dot(p, m_basis[0]) - m_cx) * m_scale, m_oy - (Dot(p, m_basis[1]) - m_cy) * m_scale,
          -Dot(p, m_basis[2])};
}

std::vector<PaneRect> Arrange(const std::optional<scene::layout::LayoutNode>& layout,
                              const std::set<std::string>& open, Rect area,
                              std::vector<std::string>* hiddenTabs) {
  std::vector<PaneRect> out;
  if (layout) {
    Visit(layout, area, out, hiddenTabs, std::nullopt);
    // scenes open but not in the layout are not shown in the layout picture
    return out;
  }
  const std::vector<std::string> scenes(open.begin(), open.end());
  if (scenes.empty()) return out;
  const auto n = scenes.size();
  const auto cols = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(n))));
  const auto rows = (n + cols - 1) / cols;
  for (std::size_t i = 0; i < n; ++i) {
    const double cw = area.w / static_cast<double>(cols), ch = area.h / static_cast<double>(rows);
    out.push_back({scenes[i], {area.x + static_cast<double>(i % cols) * cw, area.y + static_cast<double>(i / cols) * ch, cw, ch}, scenes[i]});
  }
  return out;
}

float Alpha(const style::CommonStyle& c) {
  const float a = c.color ? c.color->a / 255.f : 1.f;
  return std::clamp(a * c.opacity.value_or(1.f), 0.f, 1.f);
}

bool Visible(const style::CommonStyle& c) { return c.visible.value_or(true); }

style::Color ColorOf(const style::CommonStyle& c) { return c.color.value_or(style::Color{0, 0, 0, 255}); }

double NiceStep(double raw) {
  const double e = std::pow(10.0, std::floor(std::log10(raw)));
  for (double m : {1.0, 2.0, 5.0, 10.0})
    if (m * e >= raw) return m * e;
  return 10 * e;
}

std::vector<std::array<std::uint32_t, 2>> FeatureEdges(const MeshPrim& m) {
  auto normal = [&](const std::array<std::uint32_t, 3>& t) {
    const Vec3& a = m.vertices[t[0]];
    const Vec3& b = m.vertices[t[1]];
    const Vec3& c = m.vertices[t[2]];
    return Normalized(Cross({b[0] - a[0], b[1] - a[1], b[2] - a[2]}, {c[0] - a[0], c[1] - a[1], c[2] - a[2]}),
                      {0, 0, 0});
  };
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::vector<std::size_t>> faces;
  for (std::size_t t = 0; t < m.triangles.size(); ++t)
    for (int e = 0; e < 3; ++e) {
      const auto a = m.triangles[t][e], b = m.triangles[t][(e + 1) % 3];
      faces[{std::min(a, b), std::max(a, b)}].push_back(t);
    }
  std::vector<std::array<std::uint32_t, 2>> out;
  const double cosLimit = std::cos(20.0 * 3.14159265358979 / 180.0);
  for (const auto& [edge, fs] : faces) {
    if (fs.size() != 2 || Dot(normal(m.triangles[fs[0]]), normal(m.triangles[fs[1]])) < cosLimit)
      out.push_back({edge.first, edge.second});
  }
  return out;
}

std::vector<PaintOp> Paint(const SceneDrawing& scene, const Rect& rect) {
  std::vector<PaintOp> ops;
  const auto& env = scene.environment;
  ops.push_back({PaintRole::Background, {}, 0, PaintFill{rect, env.background.value_or(style::Color{255, 255, 255, 255})}});
  const View view(scene, rect.x, rect.y, rect.w, rect.h);

  if (env.grid.value_or(false)) {
    const double step = NiceStep(60.0 / view.Scale());  // about every 60 px
    const auto o = view.Project({0, 0, 0});
    const double spx = step * view.Scale();
    const style::Color g = style::Color::FromHex(0xE0E0E0);
    for (double x = o.x + std::ceil((rect.x - o.x) / spx) * spx; x <= rect.x + rect.w; x += spx)
      ops.push_back({PaintRole::Grid, {}, 0, PaintPolyline{{{x, rect.y, 0}, {x, rect.y + rect.h, 0}}, 1, g, 1.f}});
    for (double y = o.y + std::ceil((rect.y - o.y) / spx) * spx; y <= rect.y + rect.h; y += spx)
      ops.push_back({PaintRole::Grid, {}, 0, PaintPolyline{{{rect.x, y, 0}, {rect.x + rect.w, y, 0}}, 1, g, 1.f}});
  }

  // Surfaces: triangles far to near, then outlines.
  struct Tri {
    double depth;
    PaintOp op;
  };
  std::vector<Tri> tris;
  for (const auto& [name, layer] : scene.layers) {
    if (!layer) continue;
    for (const auto& m : layer->meshes) {
      if (!Visible(m.s.common) || m.s.wireframe.value_or(false)) continue;
      const auto col = ColorOf(m.s.common);
      const float a = Alpha(m.s.common);
      for (const auto& t : m.triangles) {
        const auto pa = view.Project(m.vertices[t[0]]), pb = view.Project(m.vertices[t[1]]),
                   pc = view.Project(m.vertices[t[2]]);
        if ((pb.x - pa.x) * (pc.y - pa.y) - (pc.x - pa.x) * (pb.y - pa.y) == 0) continue;  // edge-on
        tris.push_back({(pa.depth + pb.depth + pc.depth) / 3,
                        {PaintRole::Surface, name, m.index, PaintTriangle{{pa, pb, pc}, col, a}}});
      }
    }
  }
  std::stable_sort(tris.begin(), tris.end(), [](const Tri& x, const Tri& y) { return x.depth < y.depth; });
  for (auto& t : tris) ops.push_back(std::move(t.op));
  for (const auto& [name, layer] : scene.layers) {
    if (!layer) continue;
    for (const auto& m : layer->meshes) {
      if (!Visible(m.s.common)) continue;
      const auto col = m.s.edgeColor.value_or(ColorOf(m.s.common));
      const double w = m.s.edgeWidth.value_or(1.f);
      if (w <= 0) continue;
      for (const auto& e : m.edges)
        ops.push_back({PaintRole::Edge, name, m.index,
                       PaintPolyline{{view.Project(m.vertices[e[0]]), view.Project(m.vertices[e[1]])}, w, col,
                                     col.a / 255.f}});
    }
  }

  // Lines, far to near (mean depth).
  struct LineRef {
    double depth;
    std::string_view layer;
    const LinePrim* line;
  };
  std::vector<LineRef> lines;
  for (const auto& [name, layer] : scene.layers) {
    if (!layer) continue;
    for (const auto& l : layer->lines) {
      if (!Visible(l.s.common) || l.v.size() < 2) continue;
      double d = 0;
      for (const auto& v : l.v) d += view.Project(v).depth;
      lines.push_back({d / static_cast<double>(l.v.size()), name, &l});
    }
  }
  std::stable_sort(lines.begin(), lines.end(), [](const LineRef& a, const LineRef& b) { return a.depth < b.depth; });
  for (const auto& ref : lines) {
    const auto& l = *ref.line;
    PaintPolyline pl{{}, l.s.width.value_or(1.f), ColorOf(l.s.common), Alpha(l.s.common),
                     l.s.dash.value_or(style::DashPattern::Solid)};
    for (const auto& v : l.v) pl.p.push_back(view.Project(v));
    ops.push_back({PaintRole::Line, ref.layer, l.index, std::move(pl)});
  }

  // Points, far to near.
  struct PointRef {
    Projected p;
    std::string_view layer;
    const PointPrim* point;
  };
  std::vector<PointRef> points;
  for (const auto& [name, layer] : scene.layers) {
    if (!layer) continue;
    for (const auto& p : layer->points)
      if (Visible(p.s.common)) points.push_back({view.Project(p.p), name, &p});
  }
  std::stable_sort(points.begin(), points.end(),
                   [](const PointRef& a, const PointRef& b) { return a.p.depth < b.p.depth; });
  for (const auto& ref : points) {
    const auto& s = ref.point->s;
    ops.push_back({PaintRole::Point, ref.layer, ref.point->index,
                   PaintMarker{ref.p, s.size.value_or(3.f), s.marker.value_or(style::MarkerShape::Circle),
                               ColorOf(s.common), Alpha(s.common)}});
  }

  if (env.axes.value_or(false)) {
    const auto o = view.Project({0, 0, 0});
    const double len = 0.15 * std::min(rect.w, rect.h) / view.Scale();
    const std::pair<Vec3, std::uint32_t> axes[] = {
        {{len, 0, 0}, 0xDA1E28}, {{0, len, 0}, 0x198038}, {{0, 0, len}, 0x0F62FE}};
    for (const auto& [tip, hex] : axes)
      ops.push_back({PaintRole::Axis, {}, 0, PaintPolyline{{o, view.Project(tip)}, 2, style::Color::FromHex(hex), 1.f}});
  }
  return ops;
}

std::string FormatNumber(double v, int precision) {
  if (!std::isfinite(v)) return "0";
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.*f", std::clamp(precision, 0, 9), v);
  std::string s = buf;
  if (s.find('.') != std::string::npos) {
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
  }
  if (s == "-0") s = "0";
  return s;
}

std::string XmlEscape(const std::string& in) {
  std::string out;
  for (char c : in) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&apos;"; break;
      default: out += c;
    }
  }
  return out;
}

}  // namespace fccvis::backend::draw
